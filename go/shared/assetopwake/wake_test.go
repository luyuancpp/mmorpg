package assetopwake

import (
	"context"
	"errors"
	"io/fs"
	"math"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"

	"shared/assetop"
)

// 本文件用 bus 接口的替身代替 Redis(同 shared/leader:shared 模块不为测试引 miniredis)。
// 订阅交出测试手里的通道,去重键按 SET NX 语义记在内存里;唤醒的落点用 fakeWaker 记录。
// 直接调 handle 的用例用 wg.Wait 等后台 goroutine,不靠 sleep。

// 编译期契约:生产装配传的就是这两个类型。
var (
	_ Waker   = (*assetop.Loop)(nil)
	_ Counter = (*assetop.Metrics)(nil)
)

// waitTimeout 是等后台 goroutine 的上限;正常几毫秒内就到,超时说明真的卡住了。
const waitTimeout = 5 * time.Second

// ── 替身 ─────────────────────────────────────────────────────

type fakeBus struct {
	mu sync.Mutex

	// 订阅侧:每次订阅新建一条通道,并发出 subscribed 信号。
	cur          chan *redis.Message
	subscribeErr error
	subscribes   int
	closes       int
	gotChannel   string
	subscribed   chan struct{}

	// 去重侧:SET NX 语义,记下 TTL 以便断言。
	keys       map[string]time.Duration
	dedupErr   error
	dedupCalls int
}

func newFakeBus() *fakeBus {
	return &fakeBus{keys: map[string]time.Duration{}, subscribed: make(chan struct{}, 8)}
}

func (b *fakeBus) subscribe(_ context.Context, channel string) (<-chan *redis.Message, func() error, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.subscribes++
	b.gotChannel = channel
	if b.subscribeErr != nil {
		return nil, nil, b.subscribeErr
	}
	ch := make(chan *redis.Message, 16)
	b.cur = ch
	b.subscribed <- struct{}{}
	return ch, func() error {
		b.mu.Lock()
		defer b.mu.Unlock()
		b.closes++
		return nil
	}, nil
}

func (b *fakeBus) claimDedup(_ context.Context, key string, ttl time.Duration) (bool, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.dedupCalls++
	if b.dedupErr != nil {
		return false, b.dedupErr
	}
	if _, ok := b.keys[key]; ok {
		return false, nil
	}
	b.keys[key] = ttl
	return true, nil
}

// publish 往当前这次订阅的通道里投一条消息(等价于 scene 的 PUBLISH)。
func (b *fakeBus) publish(payload string) {
	b.mu.Lock()
	ch := b.cur
	b.mu.Unlock()
	ch <- &redis.Message{Channel: Channel, Payload: payload}
}

// dropSubscription 模拟订阅永久断开:关掉当前通道。
func (b *fakeBus) dropSubscription() {
	b.mu.Lock()
	ch := b.cur
	b.mu.Unlock()
	close(ch)
}

func (b *fakeBus) counts() (subscribes, closes, dedupCalls int) {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.subscribes, b.closes, b.dedupCalls
}

type fakeWaker struct {
	mu      sync.Mutex
	calls   []uint64
	budgets []time.Duration

	// block 非 nil 时 WakePlayer 等它关闭或 ctx 结束才返回。
	block    chan struct{}
	called   chan uint64
	returned atomic.Int32
	err      error
}

func newFakeWaker() *fakeWaker { return &fakeWaker{called: make(chan uint64, 64)} }

func (w *fakeWaker) WakePlayer(ctx context.Context, playerID uint64) (int, error) {
	w.mu.Lock()
	w.calls = append(w.calls, playerID)
	if dl, ok := ctx.Deadline(); ok {
		w.budgets = append(w.budgets, time.Until(dl))
	} else {
		w.budgets = append(w.budgets, -1)
	}
	w.mu.Unlock()
	w.called <- playerID
	if w.block != nil {
		select {
		case <-w.block:
		case <-ctx.Done():
		}
	}
	w.returned.Add(1)
	return 1, w.err
}

func (w *fakeWaker) snapshot() ([]uint64, []time.Duration) {
	w.mu.Lock()
	defer w.mu.Unlock()
	return append([]uint64(nil), w.calls...), append([]time.Duration(nil), w.budgets...)
}

type fakeCounter struct {
	mu sync.Mutex
	n  map[assetop.WakeResult]int
}

func newFakeCounter() *fakeCounter { return &fakeCounter{n: map[assetop.WakeResult]int{}} }

func (c *fakeCounter) IncWake(r assetop.WakeResult) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.n[r]++
}

func (c *fakeCounter) get(r assetop.WakeResult) int {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.n[r]
}

func newTestSubscriber(t *testing.T, bus *fakeBus, cfg Config, waker Waker, counter Counter) *Subscriber {
	t.Helper()
	if cfg.Service == "" {
		cfg.Service = "guild"
	}
	s, err := newSubscriber(bus, cfg, waker, counter)
	if err != nil {
		t.Fatalf("newSubscriber 失败: %v", err)
	}
	return s
}

// waitSignal 在上限内等一个信号。
func waitSignal(t *testing.T, ch <-chan struct{}, what string) {
	t.Helper()
	select {
	case <-ch:
	case <-time.After(waitTimeout):
		t.Fatalf("等待%s超时", what)
	}
}

// waitWake 在上限内等下一次 WakePlayer 被调用,返回它的 player_id。
func waitWake(t *testing.T, w *fakeWaker, what string) uint64 {
	t.Helper()
	select {
	case id := <-w.called:
		return id
	case <-time.After(waitTimeout):
		t.Fatalf("等待%s超时", what)
	}
	return 0
}

// stopWithin 在上限内等 Stop 返回;超时说明有 goroutine 没退出。
func stopWithin(t *testing.T, s *Subscriber) {
	t.Helper()
	done := make(chan struct{})
	go func() {
		s.Stop()
		close(done)
	}()
	waitSignal(t, done, "Stop 返回")
}

// ── 纯函数与装配 ─────────────────────────────────────────────

func TestParsePlayerID(t *testing.T) {
	maxID := strconv.FormatUint(math.MaxUint64, 10)
	cases := []struct {
		payload string
		want    uint64
		ok      bool
	}{
		{"42", 42, true},
		{maxID, math.MaxUint64, true},
		{"0", 0, false},
		{"", 0, false},
		{"-1", 0, false},
		{"+1", 0, false},
		{" 42", 0, false},
		{"42 ", 0, false},
		{"42\n", 0, false},
		{"0x2a", 0, false},
		{"4_2", 0, false},
		{"abc", 0, false},
		{"18446744073709551616", 0, false}, // uint64 上限 + 1
	}
	for _, c := range cases {
		t.Run(strconv.Quote(c.payload), func(t *testing.T) {
			got, err := ParsePlayerID(c.payload)
			if (err == nil) != c.ok || got != c.want {
				t.Fatalf("ParsePlayerID(%q) = (%d, %v),期望 (%d, ok=%v)", c.payload, got, err, c.want, c.ok)
			}
		})
	}
}

// 频道名与去重键是跨进程契约:频道名 scene(C++)发布用同一串字面量,去重键每个服务一把。
func TestChannelAndDedupKey(t *testing.T) {
	if Channel != "assetop:wake" {
		t.Fatalf("频道名应为 assetop:wake,实际 %q", Channel)
	}
	if got := DedupKey("guild", 42); got != "assetop:wake:guild:42" {
		t.Fatalf("去重键格式错: %q", got)
	}
	if got := DedupKey("trade", math.MaxUint64); got != "assetop:wake:trade:18446744073709551615" {
		t.Fatalf("去重键应按十进制写完整 uint64,实际 %q", got)
	}
}

// cppLandingHeader 是 scene 侧声明唤醒频道的头文件,路径相对仓库根。
const cppLandingHeader = "cpp/libs/services/scene/player/system/player_landing.h"

// 频道名两边各写一份字面量。漂移的表现是 guild / trade 永远收不到唤醒、只剩原退避,一个错都不报,
// 所以逐字比对 C++ 的 kAssetWakeChannel。只拷了 go/ 的构建环境里没有头文件时跳过。
func TestChannelMatchesCpp(t *testing.T) {
	// go test 的工作目录是本包目录(go/shared/assetopwake),仓库根在上三级。
	data, err := os.ReadFile(filepath.Join("..", "..", "..", filepath.FromSlash(cppLandingHeader)))
	if errors.Is(err, fs.ErrNotExist) {
		t.Skip("找不到 C++ 源码,跳过逐字比对")
	}
	if err != nil {
		t.Fatalf("读 %s: %v", cppLandingHeader, err)
	}
	const decl = `*kAssetWakeChannel = "`
	src := string(data)
	start := strings.Index(src, decl)
	if start < 0 {
		t.Fatalf("%s 里找不到 %q(声明须保持 const char 指针 + 单个字面量的写法)", cppLandingHeader, decl)
	}
	rest := src[start+len(decl):]
	end := strings.IndexByte(rest, '"')
	if end < 0 {
		t.Fatalf("%s 的 kAssetWakeChannel 字面量没有闭合", cppLandingHeader)
	}
	if got := rest[:end]; got != Channel {
		t.Fatalf("C++ kAssetWakeChannel = %q,Go Channel = %q:两边必须逐字一致", got, Channel)
	}
}

func TestConfigDefaultsAndValidation(t *testing.T) {
	s := newTestSubscriber(t, newFakeBus(), Config{}, newFakeWaker(), nil)
	if s.cfg.Concurrency != DefaultConcurrency || cap(s.slots) != DefaultConcurrency {
		t.Fatalf("默认并发应为 %d,实际 cfg=%d slots=%d", DefaultConcurrency, s.cfg.Concurrency, cap(s.slots))
	}
	// 唤醒超时必须明显大于 Loop 的默认 OpBudget(2.5s):WakePlayer 只在剩余预算够一整行时才开始新行。
	if s.cfg.DedupTTL != 5*time.Second || s.cfg.WakeTimeout != 10*time.Second {
		t.Fatalf("默认去重窗口 5s、唤醒超时 10s,实际 %v / %v", s.cfg.DedupTTL, s.cfg.WakeTimeout)
	}
	if budget := assetop.DefaultLoopConfig().OpBudget; DefaultWakeTimeout <= budget {
		t.Fatalf("默认唤醒超时 %v 必须大于 Loop 默认 OpBudget %v,否则每次唤醒一行也开始不了", DefaultWakeTimeout, budget)
	}

	bad := []struct {
		name string
		cfg  Config
	}{
		{"缺服务名", Config{}},
		{"并发为负", Config{Service: "guild", Concurrency: -1}},
		{"去重窗口为负", Config{Service: "guild", DedupTTL: -time.Second}},
		{"唤醒超时为负", Config{Service: "guild", WakeTimeout: -time.Second}},
	}
	for _, c := range bad {
		t.Run(c.name, func(t *testing.T) {
			if _, err := newSubscriber(newFakeBus(), c.cfg, newFakeWaker(), nil); err == nil {
				t.Fatal("非法配置必须被拒")
			}
		})
	}
	if _, err := newSubscriber(newFakeBus(), Config{Service: "guild"}, nil, nil); err == nil {
		t.Fatal("缺 Waker 必须被拒")
	}
	if _, err := New(nil, Config{Service: "guild"}, newFakeWaker(), nil); err == nil {
		t.Fatal("缺 Redis 客户端必须被拒")
	}
}

// ── 单条消息的处理 ───────────────────────────────────────────

// 合法消息:写本服务的去重键(SET NX EX 5)后唤醒;同一窗口内的重复消息被去重。
// 另一个服务有自己的键,不受影响。
func TestHandleWakesOnceAndDedups(t *testing.T) {
	bus := newFakeBus()
	waker := newFakeWaker()
	counter := newFakeCounter()
	s := newTestSubscriber(t, bus, Config{Service: "guild"}, waker, counter)
	ctx := context.Background()

	s.handle(ctx, "42")
	s.wg.Wait()
	s.handle(ctx, "42")
	s.wg.Wait()

	calls, _ := waker.snapshot()
	if len(calls) != 1 || calls[0] != 42 {
		t.Fatalf("同一窗口内只应唤醒一次 player 42,实际 %v", calls)
	}
	bus.mu.Lock()
	ttl, ok := bus.keys["assetop:wake:guild:42"]
	bus.mu.Unlock()
	if !ok || ttl != 5*time.Second {
		t.Fatalf("应写去重键 assetop:wake:guild:42 且 TTL 5s,实际 ok=%v ttl=%v", ok, ttl)
	}
	if counter.get(assetop.WakeReceived) != 2 || counter.get(assetop.WakeDeduped) != 1 {
		t.Fatalf("received 应为 2、deduped 应为 1,实际 %v", counter.n)
	}

	// trade 订阅同一条消息:它有自己的去重键,照样唤醒。
	tradeWaker := newFakeWaker()
	trade := newTestSubscriber(t, bus, Config{Service: "trade"}, tradeWaker, nil)
	trade.handle(ctx, "42")
	trade.wg.Wait()
	if calls, _ := tradeWaker.snapshot(); len(calls) != 1 {
		t.Fatalf("另一个服务不得被 guild 的去重键挡住,实际 %v", calls)
	}
}

// 非法负载:计 error 后丢弃,不写去重键、不唤醒。
func TestHandleDropsMalformedPayload(t *testing.T) {
	bus := newFakeBus()
	waker := newFakeWaker()
	counter := newFakeCounter()
	s := newTestSubscriber(t, bus, Config{}, waker, counter)

	for _, payload := range []string{"abc", "0", "-7", "42 "} {
		s.handle(context.Background(), payload)
	}
	s.wg.Wait()

	if calls, _ := waker.snapshot(); len(calls) != 0 {
		t.Fatalf("非法负载不得唤醒,实际 %v", calls)
	}
	if _, _, dedup := bus.counts(); dedup != 0 {
		t.Fatalf("非法负载不得写去重键,实际写了 %d 次", dedup)
	}
	if counter.get(assetop.WakeReceived) != 4 || counter.get(assetop.WakeError) != 4 {
		t.Fatalf("received 与 error 都应为 4,实际 %v", counter.n)
	}
}

// 并发已满:直接丢(计 dropped),而且不写去重键 —— 别的副本仍能接下这次唤醒。
// 名额在唤醒结束后归还,之后的消息照常处理。
func TestHandleDropsWhenConcurrencyFull(t *testing.T) {
	bus := newFakeBus()
	waker := newFakeWaker()
	waker.block = make(chan struct{})
	counter := newFakeCounter()
	s := newTestSubscriber(t, bus, Config{Concurrency: 1}, waker, counter)
	ctx := context.Background()

	s.handle(ctx, "1") // 名额在 handle 内同步拿走
	s.handle(ctx, "2") // 名额已满:同步丢弃
	if got := counter.get(assetop.WakeDropped); got != 1 {
		t.Fatalf("并发满时应丢弃并计 dropped=1,实际 %d", got)
	}
	waitWake(t, waker, "player 1 被唤醒")
	close(waker.block)
	s.wg.Wait()

	s.handle(ctx, "3")
	s.wg.Wait()

	calls, _ := waker.snapshot()
	if len(calls) != 2 || calls[0] != 1 || calls[1] != 3 {
		t.Fatalf("应只唤醒 1 与 3,实际 %v", calls)
	}
	bus.mu.Lock()
	_, wroteDropped := bus.keys[DedupKey("guild", 2)]
	bus.mu.Unlock()
	if wroteDropped {
		t.Fatal("被丢弃的唤醒不得写去重键")
	}
}

// 去重键写失败:不唤醒(多半是 Redis 本身有问题,唤醒丢了只是退回原退避)。
func TestHandleDedupErrorSkipsWake(t *testing.T) {
	bus := newFakeBus()
	bus.dedupErr = errors.New("redis 不可用")
	waker := newFakeWaker()
	counter := newFakeCounter()
	s := newTestSubscriber(t, bus, Config{}, waker, counter)

	s.handle(context.Background(), "42")
	s.wg.Wait()

	if calls, _ := waker.snapshot(); len(calls) != 0 {
		t.Fatalf("去重键写失败时不得唤醒,实际 %v", calls)
	}
	if got := counter.get(assetop.WakeError); got != 1 {
		t.Fatalf("应计 error=1,实际 %d", got)
	}
}

// 每次唤醒有自己的超时(默认 10 秒),不继承一个没有截止时间的父 ctx 无限跑下去。
func TestWakeHasOwnTimeout(t *testing.T) {
	waker := newFakeWaker()
	s := newTestSubscriber(t, newFakeBus(), Config{}, waker, nil)

	s.handle(context.Background(), "42")
	s.wg.Wait()

	_, budgets := waker.snapshot()
	if len(budgets) != 1 || budgets[0] <= 0 || budgets[0] > DefaultWakeTimeout {
		t.Fatalf("唤醒的 ctx 应带 (0, %v] 的截止时间,实际 %v", DefaultWakeTimeout, budgets)
	}
}

// Waker 出错只记日志,不影响后续消息。
func TestWakeErrorDoesNotStopLaterWakes(t *testing.T) {
	waker := newFakeWaker()
	waker.err = errors.New("列出失败")
	s := newTestSubscriber(t, newFakeBus(), Config{}, waker, nil)

	s.handle(context.Background(), "1")
	s.wg.Wait()
	s.handle(context.Background(), "2")
	s.wg.Wait()

	if calls, _ := waker.snapshot(); len(calls) != 2 {
		t.Fatalf("前一次唤醒出错不该影响下一次,实际 %v", calls)
	}
}

// ── 生命周期 ─────────────────────────────────────────────────

// Start 订阅 assetop:wake 并分发消息;Stop 关闭订阅、等后台 goroutine 全部退出;
// Stop 之后再 Start 是空操作,重复 Stop 安全。
func TestStartStop(t *testing.T) {
	bus := newFakeBus()
	waker := newFakeWaker()
	s := newTestSubscriber(t, bus, Config{}, waker, nil)

	s.Start(context.Background())
	s.Start(context.Background()) // 重复 Start 不再起第二个订阅循环
	waitSignal(t, bus.subscribed, "订阅建立")
	bus.publish("42")
	if got := waitWake(t, waker, "唤醒"); got != 42 {
		t.Fatalf("应唤醒 player 42,实际 %d", got)
	}

	stopWithin(t, s)
	subscribes, closes, _ := bus.counts()
	if subscribes != 1 || closes != 1 {
		t.Fatalf("应订阅一次并在 Stop 时关闭,实际 subscribes=%d closes=%d", subscribes, closes)
	}
	bus.mu.Lock()
	gotChannel := bus.gotChannel
	bus.mu.Unlock()
	if gotChannel != Channel {
		t.Fatalf("应订阅 %s,实际 %q", Channel, gotChannel)
	}

	s.Start(context.Background())
	stopWithin(t, s)
	if subscribes, _, _ := bus.counts(); subscribes != 1 {
		t.Fatalf("Stop 之后 Start 应是空操作,实际又订阅了(共 %d 次)", subscribes)
	}
}

// Stop 要等在途唤醒结束:取消会传到唤醒的 ctx,唤醒返回之后 Stop 才返回。
func TestStopWaitsInFlightWake(t *testing.T) {
	bus := newFakeBus()
	waker := newFakeWaker()
	waker.block = make(chan struct{}) // 永不放行:只能靠 ctx 取消返回
	s := newTestSubscriber(t, bus, Config{}, waker, nil)

	s.Start(context.Background())
	waitSignal(t, bus.subscribed, "订阅建立")
	bus.publish("7")
	waitWake(t, waker, "唤醒开始")

	stopWithin(t, s)
	if waker.returned.Load() != 1 {
		t.Fatal("Stop 返回时在途唤醒必须已经结束")
	}
}

// 父 ctx 结束也会让订阅循环退出,之后 Stop 立即返回。
func TestStartStopsOnParentCancel(t *testing.T) {
	bus := newFakeBus()
	s := newTestSubscriber(t, bus, Config{}, newFakeWaker(), nil)

	ctx, cancel := context.WithCancel(context.Background())
	s.Start(ctx)
	waitSignal(t, bus.subscribed, "订阅建立")
	cancel()
	stopWithin(t, s)
	if _, closes, _ := bus.counts(); closes != 1 {
		t.Fatalf("退出时应关闭订阅,实际 closes=%d", closes)
	}
}

// 订阅断开(消息通道被关闭)后隔 resubscribeDelay 重新订阅,新订阅上的消息照常处理。
func TestResubscribeAfterSubscriptionDrops(t *testing.T) {
	bus := newFakeBus()
	waker := newFakeWaker()
	s := newTestSubscriber(t, bus, Config{}, waker, nil)
	defer stopWithin(t, s)

	s.Start(context.Background())
	waitSignal(t, bus.subscribed, "首次订阅")
	bus.dropSubscription()
	waitSignal(t, bus.subscribed, "重新订阅")

	bus.publish("9")
	if got := waitWake(t, waker, "重订阅后的唤醒"); got != 9 {
		t.Fatalf("应唤醒 player 9,实际 %d", got)
	}
	if subscribes, closes, _ := bus.counts(); subscribes != 2 || closes < 1 {
		t.Fatalf("应订阅两次且关闭过断开的那一次,实际 subscribes=%d closes=%d", subscribes, closes)
	}
}

// 没 Start 就 Stop、nil 接收者:都不 panic、不阻塞。
func TestStopWithoutStartAndNilReceiver(t *testing.T) {
	s := newTestSubscriber(t, newFakeBus(), Config{}, newFakeWaker(), nil)
	stopWithin(t, s)

	var nilSub *Subscriber
	nilSub.Start(context.Background())
	nilSub.Stop()
}
