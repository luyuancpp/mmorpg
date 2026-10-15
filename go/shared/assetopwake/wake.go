// Package assetopwake 把 scene 的「玩家落地」信号接到资产重投循环上
// (docs/design/landing-recovery-sync-digest.md §5.3)。
//
// scene 在玩家进场完成时向核心 Redis 发布 `PUBLISH assetop:wake <player_id 十进制>`;
// guild / trade 各自订阅,抢到跨副本去重键的那个副本立即对该玩家的 PENDING 行走一遍
// Claim + ProcessOne(assetop.Loop.WakePlayer),不再等最长 60 秒的退避或 30 分钟的停放档。
//
// 唤醒**只是提示**:Pub/Sub 断线、去重键写失败、并发已满时直接丢弃,行照常按原退避重投,
// 只慢不错(设计 L-4)。所以本包任何失败都不重试、不阻塞订阅循环。
//
// 为什么单独成包:assetop 按约定不引 Redis 客户端(它的定位交给 shared/scenenode),
// 订阅与去重是 Redis 的事,放在这里。
//
// 全部代码未编译,待 Codex 验证(AGENTS §10.1)。
package assetopwake

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"

	"shared/assetop"
	"shared/safego"
)

// Channel 是唤醒频道名。scene(C++,player_landing.cpp)发布用的是同一串字面量,改名两边同改。
const Channel = "assetop:wake"

// dedupKeyFormat 是跨副本去重键:assetop:wake:{服务名}:{player_id}。
// 每个服务一把:guild 与 trade 各自都要处理这次落地,不能互相去重。
const dedupKeyFormat = "assetop:wake:%s:%d"

// DedupKey 返回某服务、某玩家这次唤醒的去重键。
func DedupKey(service string, playerID uint64) string {
	return fmt.Sprintf(dedupKeyFormat, service, playerID)
}

// 默认值。去重窗口 5 秒对齐 scene 侧「同一实体 5 秒内只发一次」的合并窗口(设计 §5.2):
// 同一次落地的重复发布(重连、gate 补发)落在一个窗口里,每个服务只有一个副本去查库。
const (
	DefaultConcurrency = 4
	DefaultDedupTTL    = 5 * time.Second
	DefaultWakeTimeout = 3 * time.Second
)

const (
	// dedupTimeout 是写去重键那一次 Redis 往返的上限,与 WakeTimeout 分开计:
	// 去重卡住不该吃掉唤醒本身的预算。
	dedupTimeout = time.Second
	// resubscribeDelay 是订阅断开(建立失败或消息通道被关闭)之后重新订阅前的等待,防止热循环。
	// go-redis 自己会在连接断开时重订阅,走到这里通常是 Redis 整个不可用。
	resubscribeDelay = time.Second
	// maxLoggedPayload 是非法消息写进日志时保留的最大字节数:负载来自网络,不能原样整段打进日志。
	maxLoggedPayload = 32
)

// Waker 是唤醒的落点,生产实现是 *assetop.Loop。
type Waker interface {
	WakePlayer(ctx context.Context, playerID uint64) (int, error)
}

// Counter 记 assetop_wake_total。生产传 *assetop.Metrics,而且必须是 Loop 用的**同一个**实例:
// 同名指标在一个注册表里只能注册一次,订阅端与 Loop 各管一段 result(见 assetop.WakeResult)。
type Counter interface {
	IncWake(assetop.WakeResult)
}

// Config 是订阅端的装配参数。零值字段取默认值。
type Config struct {
	// Service 进去重键,低基数常量(guild / trade),必填。
	Service string
	// Concurrency 是本副本同时在跑的唤醒数上限;满了直接丢(计 dropped)。默认 4。
	// 它只限「同时在查库、在等名额」的唤醒个数;真正压向 scene 的并发仍由 Loop 的 Workers 预算封顶。
	Concurrency int
	// DedupTTL 是去重键的存活时间(SET NX EX)。默认 5 秒。
	DedupTTL time.Duration
	// WakeTimeout 是每次 WakePlayer 的独立超时。默认 3 秒。
	WakeTimeout time.Duration
}

func (c Config) withDefaults() (Config, error) {
	if c.Service == "" {
		return c, errors.New("assetopwake: Service 必填(进去重键)")
	}
	if c.Concurrency < 0 || c.DedupTTL < 0 || c.WakeTimeout < 0 {
		return c, fmt.Errorf("assetopwake: 参数不得为负(Concurrency=%d DedupTTL=%v WakeTimeout=%v)",
			c.Concurrency, c.DedupTTL, c.WakeTimeout)
	}
	if c.Concurrency == 0 {
		c.Concurrency = DefaultConcurrency
	}
	if c.DedupTTL == 0 {
		c.DedupTTL = DefaultDedupTTL
	}
	if c.WakeTimeout == 0 {
		c.WakeTimeout = DefaultWakeTimeout
	}
	return c, nil
}

// bus 是本包用到的全部 Redis 能力。生产实现是 goRedisBus;单测注入替身,
// 不为测试给 shared 模块加 miniredis 依赖(同 shared/leader 的做法)。
type bus interface {
	// subscribe 订阅 channel,确认订阅生效后返回消息通道与关闭函数。
	// 通道只在关闭函数被调用、或底层订阅被永久关闭时关闭;go-redis 断线会自动重订阅,期间的消息丢失。
	subscribe(ctx context.Context, channel string) (<-chan *redis.Message, func() error, error)
	// claimDedup 即 SET key 1 NX EX ttl:true = 本副本拿到这次唤醒。
	claimDedup(ctx context.Context, key string, ttl time.Duration) (bool, error)
}

// Subscriber 订阅唤醒频道并把唤醒交给 Waker。
//
// 生命周期:New → Start(ctx) → Stop()。Start / Stop 都可重复调用;Stop 之后 Start 是空操作。
// 所有后台 goroutine(订阅循环与在途唤醒)都记在 wg 里,Stop 返回时它们都已退出。
type Subscriber struct {
	bus     bus
	cfg     Config
	waker   Waker
	counter Counter

	// slots 是在途唤醒的名额,容量 = Concurrency;拿不到就丢(不排队:唤醒越排越旧越没用)。
	slots chan struct{}

	// mu 只保护 started / stopped / cancel,让 Start 与 Stop 无论怎么交错都不会在 wg.Wait 之后再 wg.Add。
	mu      sync.Mutex
	started bool
	stopped bool
	cancel  context.CancelFunc
	wg      sync.WaitGroup
}

// New 组装订阅端。client 是核心 Redis(scene 发布用的那个实例,本地与 K8s 都是 `redis` DB 0);
// 指错实例的表现是永远收不到唤醒、只剩原退避,不会报错 —— 所以装配处要按设计 §2 核对。
// counter 可为 nil(不计数)。
func New(client redis.UniversalClient, cfg Config, waker Waker, counter Counter) (*Subscriber, error) {
	if client == nil {
		return nil, errors.New("assetopwake: 缺少 Redis 客户端")
	}
	return newSubscriber(goRedisBus{c: client}, cfg, waker, counter)
}

func newSubscriber(b bus, cfg Config, waker Waker, counter Counter) (*Subscriber, error) {
	if waker == nil {
		return nil, errors.New("assetopwake: 缺少 Waker(assetop.Loop)")
	}
	cfg, err := cfg.withDefaults()
	if err != nil {
		return nil, err
	}
	if counter == nil {
		counter = noopCounter{}
	}
	return &Subscriber{
		bus:     b,
		cfg:     cfg,
		waker:   waker,
		counter: counter,
		slots:   make(chan struct{}, cfg.Concurrency),
	}, nil
}

// Start 起订阅循环。ctx 取消或 Stop 都会让它退出;重复调用、Stop 之后再调都是空操作。nil 接收者安全。
func (s *Subscriber) Start(ctx context.Context) {
	if s == nil {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.started || s.stopped {
		return
	}
	s.started = true
	runCtx, cancel := context.WithCancel(ctx)
	s.cancel = cancel
	s.wg.Add(1)
	safego.Go("assetopwake.subscribe", func() {
		defer s.wg.Done()
		s.run(runCtx)
	})
	logx.Infof("[AssetOpWake] Subscribed to wake channel=%s service=%s concurrency=%d", Channel, s.cfg.Service, s.cfg.Concurrency)
}

// Stop 取消订阅并等所有在途唤醒结束(每个最多 WakeTimeout,外加 assetop 落库预留的一小段)。
// 可重复调用;nil 接收者安全。
func (s *Subscriber) Stop() {
	if s == nil {
		return
	}
	s.mu.Lock()
	s.stopped = true
	cancel := s.cancel
	s.mu.Unlock()
	if cancel != nil {
		cancel()
	}
	s.wg.Wait()
}

// run 保持订阅,断了就等 resubscribeDelay 再订,直到 ctx 结束。
func (s *Subscriber) run(ctx context.Context) {
	for {
		err := s.consume(ctx)
		if ctx.Err() != nil {
			return
		}
		logx.Errorf("[AssetOpWake] Wake subscription interrupted; resubscribing in %v (wakes in between fall back to backoff): %v",
			resubscribeDelay, err)
		timer := time.NewTimer(resubscribeDelay)
		select {
		case <-ctx.Done():
			timer.Stop()
			return
		case <-timer.C:
		}
	}
}

// consume 订阅一次并逐条分发,直到 ctx 结束(返回 nil)或订阅断开(返回原因)。
func (s *Subscriber) consume(ctx context.Context) error {
	msgs, closeFn, err := s.bus.subscribe(ctx, Channel)
	if err != nil {
		return err
	}
	defer func() { _ = closeFn() }()

	for {
		select {
		case <-ctx.Done():
			return nil
		case msg, ok := <-msgs:
			if !ok {
				return errors.New("assetopwake: 订阅通道已关闭")
			}
			if msg != nil {
				s.handle(ctx, msg.Payload)
			}
		}
	}
}

// handle 处理一条消息。它跑在订阅循环里,必须不阻塞:解析与拿名额都是本地操作,
// 去重与唤醒放进独立 goroutine。
func (s *Subscriber) handle(ctx context.Context, payload string) {
	s.counter.IncWake(assetop.WakeReceived)

	playerID, err := ParsePlayerID(payload)
	if err != nil {
		s.counter.IncWake(assetop.WakeError)
		logx.Errorf("[AssetOpWake] Dropped malformed wake message payload=%q: %v", truncate(payload, maxLoggedPayload), err)
		return
	}

	select {
	case s.slots <- struct{}{}:
	default:
		// 名额满:丢。去重键还没写,别的副本仍可以接下这次唤醒。
		s.counter.IncWake(assetop.WakeDropped)
		return
	}
	// 订阅循环自己记在 wg 里且还没退出,这里的 Add 不会与 Stop 的 Wait 撞上「计数为 0 时 Add」。
	s.wg.Add(1)
	safego.Go("assetopwake.wake", func() {
		defer s.wg.Done()
		defer func() { <-s.slots }()
		s.wake(ctx, playerID)
	})
}

// wake 抢去重键,抢到才唤醒。去重键写失败时不唤醒:那多半是 Redis 本身出了问题,
// 而唤醒丢了只是退回原退避;反过来不去重地唤醒,每次落地每个副本都要查一次库。
func (s *Subscriber) wake(ctx context.Context, playerID uint64) {
	dedupCtx, cancelDedup := context.WithTimeout(ctx, dedupTimeout)
	claimed, err := s.bus.claimDedup(dedupCtx, DedupKey(s.cfg.Service, playerID), s.cfg.DedupTTL)
	cancelDedup()
	if err != nil {
		s.counter.IncWake(assetop.WakeError)
		logx.Errorf("[AssetOpWake] Failed to write wake dedup key; wake skipped player_id=%d: %v", playerID, err)
		return
	}
	if !claimed {
		s.counter.IncWake(assetop.WakeDeduped)
		return
	}

	wakeCtx, cancel := context.WithTimeout(ctx, s.cfg.WakeTimeout)
	defer cancel()
	// woke / none / error 由 Loop.WakePlayer 自己计,这里只留日志。
	if n, err := s.waker.WakePlayer(wakeCtx, playerID); err != nil {
		logx.Errorf("[AssetOpWake] Wake failed; rows fall back to backoff player_id=%d processed=%d: %v", playerID, n, err)
	}
}

// ParsePlayerID 解析唤醒负载:十进制、非零的 uint64,不容忍前后空白与正负号。
// scene 发布的就是 `%llu`,任何别的形状都说明发布方有 bug,丢掉比猜一个 id 安全。
func ParsePlayerID(payload string) (uint64, error) {
	id, err := strconv.ParseUint(payload, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("assetopwake: player_id 不是十进制无符号整数: %w", err)
	}
	if id == 0 {
		return 0, errors.New("assetopwake: player_id 为 0")
	}
	return id, nil
}

func truncate(s string, n int) string {
	if len(s) <= n {
		return s
	}
	return s[:n]
}

type noopCounter struct{}

func (noopCounter) IncWake(assetop.WakeResult) {}
