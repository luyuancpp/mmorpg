package auth

import (
	"context"
	"errors"
	"sync/atomic"
	"testing"
	"time"
)

func TestProductionPasswordProviderCrossReplicaFailureBudget(t *testing.T) {
	encoded, err := HashPassword("correct-password")
	if err != nil {
		t.Fatal(err)
	}
	quota, _ := testPasswordQuota(t, 5)
	store := &fakePasswordHashStore{canonical: "Álice", hash: encoded, found: true}
	first, err := NewProductionPasswordProvider(store, quota)
	if err != nil {
		t.Fatal(err)
	}
	second, err := NewProductionPasswordProvider(store, quota)
	if err != nil {
		t.Fatal(err)
	}
	var computations atomic.Int64
	for _, provider := range []*ProductionPasswordProvider{first, second} {
		provider.verifyKDF = func(_, _ string) (bool, error) {
			computations.Add(1)
			return false, nil
		}
	}
	for index := 0; index < 6; index++ {
		provider := []*ProductionPasswordProvider{first, second}[index%2]
		if _, err := provider.ValidatePassword(context.Background(), "ALICE", "wrong-password"); err == nil {
			t.Fatal("错密不得通过认证")
		}
	}
	if actual := computations.Load(); actual != 5 {
		t.Fatalf("跨副本同canonical账号KDF次数=%d，期望5次失败后拒绝", actual)
	}
}

type recordingPasswordQuota struct {
	kind                PasswordAccountKind
	identity            string
	begins              int
	outcomes            []PasswordAttemptOutcome
	beginErr, finishErr error
}

func (q *recordingPasswordQuota) Begin(_ context.Context, kind PasswordAccountKind, identity string) (PasswordAttempt, error) {
	q.begins++
	q.kind = kind
	q.identity = identity
	return PasswordAttempt{}, q.beginErr
}
func (q *recordingPasswordQuota) Finish(ctx context.Context, _ PasswordAttempt, outcome PasswordAttemptOutcome) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	q.outcomes = append(q.outcomes, outcome)
	return q.finishErr
}

func TestPasswordProviderSettlesOnlyRealMismatchAndAbortsInternalFailures(t *testing.T) {
	for _, test := range []struct {
		name                string
		found               bool
		canonical, hash     string
		storeErr, verifyErr error
		match               bool
		want                PasswordAttemptOutcome
		wantAuth            bool
		wantBegin           bool
	}{
		{name: "错密", found: true, canonical: "Álice", hash: "valid", want: PasswordMismatch, wantBegin: true},
		{name: "成功", found: true, canonical: "Álice", hash: "valid", match: true, want: PasswordSuccess, wantAuth: true, wantBegin: true},
		{name: "未知账号", want: PasswordMismatch, wantBegin: true},
		{name: "空哈希", found: true, canonical: "Álice", want: PasswordAbort, wantBegin: true},
		{name: "坏PHC", found: true, canonical: "Álice", hash: "broken", verifyErr: errors.New("bad PHC"), want: PasswordAbort, wantBegin: true},
		{name: "KDF满", found: true, canonical: "Álice", hash: "valid", verifyErr: ErrPasswordKDFCapacity, want: PasswordAbort, wantBegin: true},
		{name: "DB故障", storeErr: errors.New("db unavailable")},
		{name: "空canonical", found: true, hash: "valid"},
	} {
		t.Run(test.name, func(t *testing.T) {
			q := &recordingPasswordQuota{}
			p := &ProductionPasswordProvider{store: &fakePasswordHashStore{canonical: test.canonical, hash: test.hash, found: test.found, err: test.storeErr}, quota: q, dummyHash: "dummy", kdfSlots: make(chan struct{}, 1), kdfWaitTimeout: time.Second, verifyKDF: func(_, encoded string) (bool, error) {
				if encoded == "dummy" {
					return false, nil
				}
				return test.match, test.verifyErr
			}}
			result, err := p.ValidatePassword(context.Background(), "ALICE", "password")
			if test.wantAuth {
				if err != nil || result == nil || result.Account != "Álice" {
					t.Fatalf("成功=%v,%v", result, err)
				}
			} else if err == nil || result != nil {
				t.Fatalf("拒绝=%v,%v", result, err)
			}
			if test.wantBegin {
				if q.begins != 1 || len(q.outcomes) != 1 || q.outcomes[0] != test.want {
					t.Fatalf("结算=%d,%v", q.begins, q.outcomes)
				}
				if test.found && (q.identity != "Álice" || q.kind != KnownPasswordAccount) {
					t.Fatal("必须使用权威canonical原值")
				}
				if !test.found && (q.kind != UnknownPasswordAccount || q.identity != "ALICE") {
					t.Fatal("未知域必须使用原输入")
				}
			} else if q.begins != 0 || len(q.outcomes) != 0 {
				t.Fatal("DB失败不能产生错密")
			}
		})
	}
}

func TestPasswordProviderDoesNotAbortAfterUnknownFinishOrRunKDFAfterUnknownBegin(t *testing.T) {
	for _, outcome := range []PasswordAttemptOutcome{PasswordSuccess, PasswordMismatch} {
		t.Run(string(outcome), func(t *testing.T) {
			q := &recordingPasswordQuota{finishErr: ErrPasswordQuotaUnavailable}
			p := &ProductionPasswordProvider{store: &fakePasswordHashStore{canonical: "known", hash: "valid", found: true}, quota: q, kdfSlots: make(chan struct{}, 1), kdfWaitTimeout: time.Second, verifyKDF: func(_, _ string) (bool, error) { return outcome == PasswordSuccess, nil }}
			if result, err := p.ValidatePassword(context.Background(), "alias", "password"); result != nil || !errors.Is(err, ErrPasswordQuotaUnavailable) {
				t.Fatalf("未知结算必须拒绝=%v,%v", result, err)
			}
			if len(q.outcomes) != 1 || q.outcomes[0] != outcome {
				t.Fatalf("不能换abort=%v", q.outcomes)
			}
		})
	}
	q := &recordingPasswordQuota{beginErr: ErrPasswordQuotaUnavailable}
	p := &ProductionPasswordProvider{store: &fakePasswordHashStore{canonical: "known", hash: "valid", found: true}, quota: q, verifyKDF: func(_, _ string) (bool, error) { t.Fatal("Begin未知不能跑KDF"); return false, nil }}
	if _, err := p.ValidatePassword(context.Background(), "alias", "password"); !errors.Is(err, ErrPasswordQuotaUnavailable) {
		t.Fatal(err)
	}
	if len(q.outcomes) != 0 {
		t.Fatal("无有效票不能abort")
	}
}

func TestPasswordProviderCancellationReleasesOnlyOwnLeaseWithoutCounting(t *testing.T) {
	q, _ := testPasswordQuota(t, 1)
	ctx, cancel := context.WithCancel(context.Background())
	p := &ProductionPasswordProvider{store: &fakePasswordHashStore{canonical: "known", hash: "valid", found: true}, quota: q, kdfSlots: make(chan struct{}, 1), kdfWaitTimeout: time.Second, verifyKDF: func(_, _ string) (bool, error) { cancel(); return true, nil }}
	if result, err := p.ValidatePassword(ctx, "alias", "password"); result != nil || !errors.Is(err, context.Canceled) {
		t.Fatalf("取消必须拒绝=%v,%v", result, err)
	}
	mustPasswordFinish(t, q, mustPasswordBegin(t, q, "known"), PasswordSuccess)
}

func TestPasswordProviderConstructorRequiresNonNilQuota(t *testing.T) {
	store := &fakePasswordHashStore{}
	var typedNil *RedisPasswordFailureQuota
	for _, quota := range []PasswordFailureQuota{nil, typedNil} {
		if _, err := NewProductionPasswordProvider(store, quota); err == nil {
			t.Fatal("生产构造器不能省略配额")
		}
	}
	var nilStore *fakePasswordHashStore
	if _, err := NewProductionPasswordProvider(nilStore, passPasswordQuota{}); err == nil {
		t.Fatal("typed nil store")
	}
}
