package logic

// fakes_test.go —— logic 单测共用的假存储、假会话、假推送与指标读取。
//
// 假存储只**复述 data 层契约的返回值**(SendOutcome、nil 行、applied 布尔),不模拟 SQL 语义:
// 锁、白名单、条件 upsert 的正确性在 data 包的真库测试里;这里测的是 logic 对每一种返回值的映射与调用顺序。

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/prometheus/client_golang/prometheus"

	"mail/internal/config"
	"mail/internal/data"
	"mail/internal/metrics"
	"mail/internal/session"

	base "proto/common/base"
	mailpb "proto/mail"
	plpb "proto/player_locator"

	"shared/kafkautil"
)

// testNow 是单测的固定时钟;testNowMs 是它的毫秒值。
var testNow = time.UnixMilli(1_800_000_000_000)

const testNowMs = uint64(1_800_000_000_000)

var errBoom = errors.New("boom: storage down")

// fakeStore 实现 MailStore。每个方法记一次调用名;errOn[方法名] 非 nil 时返回该错误。
type fakeStore struct {
	mu    sync.Mutex
	calls []string
	errOn map[string]error

	sendPersonalResult data.SendResult
	lastPersonalSend   data.PersonalSend
	sendSystemResult   data.SendResult
	lastSystemSend     data.SystemSend

	personalList       []*mailpb.MailPersonalRecord
	lastListLimit      uint32
	lastListBefore     uint64
	unread, claimable  uint32
	systemList         []*mailpb.MailSystemRecord
	systemTruncated    bool
	lastSystemLimit    uint32
	states             map[uint64]*mailpb.MailSystemStateRecord
	systemByID         map[uint64]*mailpb.MailSystemRecord
	personalRead       *mailpb.MailPersonalRecord
	personalByIDs      map[uint64]*mailpb.MailPersonalRecord
	deletePersonalN    int64
	lastDeleteIDs      []uint64
	deleteReadN        int64
	lastDeleteReadCap  uint32
	systemDeleteResult map[uint64]bool
	systemDeleteCalls  []uint64
}

func newFakeStore() *fakeStore {
	return &fakeStore{
		errOn:              map[string]error{},
		states:             map[uint64]*mailpb.MailSystemStateRecord{},
		systemByID:         map[uint64]*mailpb.MailSystemRecord{},
		personalByIDs:      map[uint64]*mailpb.MailPersonalRecord{},
		systemDeleteResult: map[uint64]bool{},
	}
}

func (f *fakeStore) record(name string) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.calls = append(f.calls, name)
	return f.errOn[name]
}

func (f *fakeStore) callNames() []string {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]string(nil), f.calls...)
}

func (f *fakeStore) called(name string) bool {
	for _, c := range f.callNames() {
		if c == name {
			return true
		}
	}
	return false
}

func (f *fakeStore) SendPersonal(_ context.Context, in data.PersonalSend) (data.SendResult, error) {
	f.lastPersonalSend = in
	if err := f.record("SendPersonal"); err != nil {
		return data.SendResult{}, err
	}
	return f.sendPersonalResult, nil
}

func (f *fakeStore) SendSystem(_ context.Context, in data.SystemSend) (data.SendResult, error) {
	f.lastSystemSend = in
	if err := f.record("SendSystem"); err != nil {
		return data.SendResult{}, err
	}
	return f.sendSystemResult, nil
}

func (f *fakeStore) ListPersonal(_ context.Context, _, before, _ uint64, limit uint32) ([]*mailpb.MailPersonalRecord, error) {
	f.lastListLimit, f.lastListBefore = limit, before
	if err := f.record("ListPersonal"); err != nil {
		return nil, err
	}
	return f.personalList, nil
}

func (f *fakeStore) PersonalCounts(context.Context, uint64, uint64) (uint32, uint32, error) {
	if err := f.record("PersonalCounts"); err != nil {
		return 0, 0, err
	}
	return f.unread, f.claimable, nil
}

func (f *fakeStore) ListActiveSystem(_ context.Context, _ uint64, limit uint32) ([]*mailpb.MailSystemRecord, bool, error) {
	f.lastSystemLimit = limit
	if err := f.record("ListActiveSystem"); err != nil {
		return nil, false, err
	}
	return f.systemList, f.systemTruncated, nil
}

func (f *fakeStore) SystemStates(_ context.Context, _ uint64, ids []uint64) (map[uint64]*mailpb.MailSystemStateRecord, error) {
	if err := f.record("SystemStates"); err != nil {
		return nil, err
	}
	out := map[uint64]*mailpb.MailSystemStateRecord{}
	for _, id := range ids {
		if st, ok := f.states[id]; ok {
			out[id] = st
		}
	}
	return out, nil
}

func (f *fakeStore) GetSystem(_ context.Context, id uint64) (*mailpb.MailSystemRecord, error) {
	if err := f.record("GetSystem"); err != nil {
		return nil, err
	}
	return f.systemByID[id], nil
}

func (f *fakeStore) GetSystemState(_ context.Context, _, id uint64) (*mailpb.MailSystemStateRecord, error) {
	if err := f.record("GetSystemState"); err != nil {
		return nil, err
	}
	return f.states[id], nil
}

func (f *fakeStore) PersonalByIDs(_ context.Context, _ uint64, ids []uint64) (map[uint64]*mailpb.MailPersonalRecord, error) {
	if err := f.record("PersonalByIDs"); err != nil {
		return nil, err
	}
	out := map[uint64]*mailpb.MailPersonalRecord{}
	for _, id := range ids {
		if rec, ok := f.personalByIDs[id]; ok {
			out[id] = rec
		}
	}
	return out, nil
}

func (f *fakeStore) MarkPersonalRead(context.Context, uint64, uint64, uint64) (*mailpb.MailPersonalRecord, error) {
	if err := f.record("MarkPersonalRead"); err != nil {
		return nil, err
	}
	return f.personalRead, nil
}

func (f *fakeStore) MarkSystemRead(_ context.Context, _, id, now uint64) (bool, error) {
	if err := f.record("MarkSystemRead"); err != nil {
		return false, err
	}
	// 复述条件 upsert 的结果:有效且有行时把 read_ms 填上(测 logic 读回 state 的路径)。
	if st, ok := f.states[id]; ok && st.GetReadMs() == 0 && st.GetDeletedMs() == 0 {
		st.ReadMs = now
		return true, nil
	}
	return false, nil
}

func (f *fakeStore) DeletePersonal(_ context.Context, _ uint64, ids []uint64, _ uint64) (int64, error) {
	f.lastDeleteIDs = append([]uint64(nil), ids...)
	if err := f.record("DeletePersonal"); err != nil {
		return 0, err
	}
	return f.deletePersonalN, nil
}

func (f *fakeStore) DeleteReadPersonal(_ context.Context, _, _ uint64, limit uint32) (int64, error) {
	f.lastDeleteReadCap = limit
	if err := f.record("DeleteReadPersonal"); err != nil {
		return 0, err
	}
	return f.deleteReadN, nil
}

func (f *fakeStore) MarkSystemDeleted(_ context.Context, _, id, _ uint64) (bool, error) {
	f.systemDeleteCalls = append(f.systemDeleteCalls, id)
	if err := f.record("MarkSystemDeleted"); err != nil {
		return false, err
	}
	return f.systemDeleteResult[id], nil
}

// fakeSessions 实现 SessionLookup。
type fakeSessions struct {
	sess *plpb.PlayerSession
	err  error
}

func (f *fakeSessions) Lookup(context.Context, uint64) (*plpb.PlayerSession, error) {
	return f.sess, f.err
}

// fakePush 记录推送调用。
type fakePush struct {
	calls     int
	to        kafkautil.PlayerGateInfo
	messageID uint32
	body      []byte
	err       error
}

func (f *fakePush) push(_ context.Context, to kafkautil.PlayerGateInfo, messageID uint32, body []byte) error {
	f.calls++
	f.to, f.messageID, f.body = to, messageID, body
	return f.err
}

// testConfig 是单测用的合法配置(Timeout 4000 → 预算 3.5s;Mode 默认 dev 以便测附件)。
func testConfig(mode string) config.Config {
	c := config.Config{}
	c.Timeout = 4000
	c.Mode = mode
	c.Mail = config.MailConf{
		MaxPersonalMailsPerPlayer: 100,
		MaxActiveSystemMails:      100,
		MaxPersonalMailTtlDays:    30,
		MaxSystemMailTtlDays:      30,
		AllowedCurrencyTypes:      []uint32{0, 1, 2},
	}
	c.Sweep = config.SweepConf{Mode: config.SweepModeReportOnly, Interval: time.Minute, RetentionDays: 7, BatchLimit: 1000}
	return c
}

// testDeps 组装一套用假实现的依赖。
func testDeps(store *fakeStore, mode string) *Deps {
	var next uint64 = 5000
	return &Deps{
		Config:     testConfig(mode),
		Repo:       store,
		Sessions:   &fakeSessions{},
		NextID:     func(context.Context) (uint64, error) { next++; return next, nil },
		AdminToken: "",
		Now:        func() time.Time { return testNow },
	}
}

// playerCtx 是带客户端会话身份的 ctx(拦截器校验通过之后的样子)。
func playerCtx(playerID uint64) context.Context {
	return session.WithDetails(context.Background(), &base.SessionDetails{SessionId: 1, PlayerId: playerID})
}

// counterValue 读默认 registry 里某个指标在给定 label 组合下的值(counter 或 gauge);序列不存在返回 0。
// 指标是懒注册的,先调 metrics.Register(幂等)。
func counterValue(t *testing.T, name string, labels map[string]string) float64 {
	t.Helper()
	metrics.Register()
	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather 指标失败: %v", err)
	}
	for _, mf := range families {
		if mf.GetName() != name {
			continue
		}
	metricLoop:
		for _, m := range mf.GetMetric() {
			got := make(map[string]string, len(m.GetLabel()))
			for _, l := range m.GetLabel() {
				got[l.GetName()] = l.GetValue()
			}
			for k, v := range labels {
				if got[k] != v {
					continue metricLoop
				}
			}
			if m.GetCounter() != nil {
				return m.GetCounter().GetValue()
			}
			return m.GetGauge().GetValue()
		}
	}
	return 0
}
