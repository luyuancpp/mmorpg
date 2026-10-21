//go:build integration

package inbox

import (
	"context"
	"database/sql"
	"errors"
	"sync"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	base "proto/common/base"
	dspb "proto/data_service"
	inboxpb "proto/player_inbox"
)

// 只给真实SQL调用加测试屏障，不替换任何身份判定或P锁。
type observedSQLRoles struct {
	inner   RoleAuthority
	entered chan struct{}
	once    sync.Once
}

func (r *observedSQLRoles) LockCurrent(ctx context.Context, tx *sql.Tx, player uint64) (*dspb.AccountRoleOwnership, error) {
	r.once.Do(func() { close(r.entered) })
	return r.inner.LockCurrent(ctx, tx, player)
}
func (r *observedSQLRoles) VerifyAdmission(ctx context.Context, tx *sql.Tx, original *dspb.AccountRoleOwnership, admission *base.RoleAdmissionIdentity) error {
	return r.inner.VerifyAdmission(ctx, tx, original, admission)
}

func TestIntegrationInboxEnqueueSharesOriginalPWithNativeFreeze(t *testing.T) {
	e := openIntegration(t)
	const player = uint64(8101)
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if _, err := (itPlayers{missing: map[uint64]bool{}, db: e.db}).PlayerExists(ctx, player); err != nil {
		t.Fatal(err)
	}
	tx, err := e.db.BeginTx(ctx, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer tx.Rollback()
	var revision uint64
	if err = tx.QueryRowContext(ctx, "SELECT owner_revision FROM account_role WHERE player_id=? FOR UPDATE", player).Scan(&revision); err != nil {
		t.Fatal(err)
	}
	observed := &observedSQLRoles{inner: e.store.roles, entered: make(chan struct{})}
	e.store.roles = observed
	result := make(chan error, 1)
	payload := gainPayload(t, 1)
	go func() {
		_, err := e.store.Enqueue(ctx, EnqueueParams{PlayerID: player, Type: gmBlock, Producer: inboxpb.PlayerInboxProducerKind_PLAYER_INBOX_PRODUCER_GM, RequestID: 1, IssuedAtMs: testNowMs, Payload: payload, Operator: "fixture"}, nil)
		result <- err
	}()
	select {
	case <-observed.entered:
	case <-ctx.Done():
		t.Fatal("未到真实角色P首锁")
	}
	select {
	case err := <-result:
		t.Fatalf("持P期间入箱不得提前完成: %v", err)
	case <-time.After(30 * time.Millisecond):
	}
	if _, err = tx.ExecContext(ctx, "UPDATE account_role SET trade_listing_id=901 WHERE player_id=?", player); err != nil {
		t.Fatal(err)
	}
	if err = tx.Commit(); err != nil {
		t.Fatal(err)
	}
	select {
	case err = <-result:
		if !errors.Is(err, ErrOriginalRole) {
			t.Fatalf("封存先提交后必须拒绝新入箱: %v", err)
		}
	case <-ctx.Done():
		t.Fatal("角色锁释放后入箱未结束")
	}
	var count int
	if err = e.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM player_inbox_item WHERE shard=? AND player_id=?", RowShard(player), player).Scan(&count); err != nil || count != 0 {
		t.Fatalf("封存后留下新条目: %d %v", count, err)
	}
}

func TestIntegrationInboxPermanentRequestCannotRebindAfterTransfer(t *testing.T) {
	e := openIntegration(t)
	const player = uint64(8102)
	first := e.mustEnqueue(t, player, 1)
	oldAdmission := e.admission(t, player, 1)
	_, err := e.store.Void(itCtx(t), VoidParams{PlayerID: player, Domain: gmDomain, Seq: first.Seq, Reason: inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_CANCELLED, Operator: "fixture"}, nil)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = e.db.Exec("UPDATE account_role SET account=?,owner_revision=2 WHERE player_id=?", []byte("new-inbox-owner"), player); err != nil {
		t.Fatal(err)
	}
	e.clock.advance(60000)
	if _, err = e.enqueue(t, player, 1, uint64(e.clock.ms.Load()), gainPayload(t, 1)); !errors.Is(err, ErrOriginalRole) {
		t.Fatalf("旧request即使刷新签发时刻也不能换主: %v", err)
	}
	if _, err = e.store.SweepTerminal(itCtx(t), ^uint64(0), 500); err != nil {
		t.Fatal(err)
	}
	old := e.item(t, player, first.Seq)
	if old == nil || old.OwnerRevision != 1 || old.OwnerAccount == "new-inbox-owner" {
		t.Fatalf("永久原唯一行被删除/补权: %+v", old)
	}
	if _, err = e.store.Fetch(itCtx(t), &inboxpb.FetchPlayerInboxRequest{PlayerId: player, Domain: gmDomain, OwnerEpoch: 1, Admission: oldAdmission}); !errors.Is(err, ErrOriginalRole) {
		t.Fatalf("旧scene原件不得取得新主消息: %v", err)
	}
	second := e.mustEnqueue(t, player, 2)
	current := e.item(t, player, second.Seq)
	if current.OwnerAccount != "new-inbox-owner" || current.OwnerRevision != 2 {
		t.Fatalf("新意图未捕获新原件: %+v", current)
	}
	if _, err = e.enqueue(t, player, 1, uint64(e.clock.ms.Load()), gainPayload(t, 1)); !errors.Is(err, ErrOriginalRole) {
		t.Fatalf("清理入口后旧请求仍必须拒绝: %v", err)
	}
}

func TestIntegrationInboxRequiresExactRegisteredOpenAdmission(t *testing.T) {
	e := openIntegration(t)
	const player = uint64(8103)
	e.mustEnqueue(t, player, 1)
	registered := e.admission(t, player, 11)
	request := &inboxpb.FetchPlayerInboxRequest{PlayerId: player, Domain: gmDomain, OwnerEpoch: 11, Admission: registered}
	changed := proto.Clone(registered).(*base.RoleAdmissionIdentity)
	changed.SceneId++
	request.Admission = changed
	if _, err := e.store.Fetch(itCtx(t), request); !errors.Is(err, ErrOriginalRole) {
		t.Fatalf("同E不同完整原件不能消费: %v", err)
	}
	request.Admission = registered
	if _, err := e.db.Exec("UPDATE account_role_admission SET closed_through=highest_generation WHERE player_id=? AND scene_instance_id=?", player, []byte(registered.SceneInstanceId)); err != nil {
		t.Fatal(err)
	}
	if _, err := e.store.Fetch(itCtx(t), request); !errors.Is(err, ErrOriginalRole) {
		t.Fatalf("已封原世代不能复活: %v", err)
	}
	if _, err := e.db.Exec("UPDATE player_inbox_item SET owner_account='',owner_revision=0 WHERE shard=? AND player_id=?", RowShard(player), player); err != nil {
		t.Fatal(err)
	}
	request.OwnerEpoch = 12
	request.Admission = e.admission(t, player, 12)
	if _, err := e.store.Fetch(itCtx(t), request); !errors.Is(err, ErrOriginalRole) {
		t.Fatalf("历史缺owner不得从当前目录补权: %v", err)
	}
}
