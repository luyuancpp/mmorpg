# G1b 实现规格：登录排队 leader 锁按 zone 拆分（多 zone 共用 Redis 时非 leader zone 不出队）

> 状态：只读调研产出的实现规格。**未编译、未测试，待 Codex 验证**（AGENTS.md §10.1）。
> 对应开服准备清单 L-01（`docs/design/tidb-phase1-and-launch-readiness.md:812`）、launch_verdict 第 13、19、175 行。
> 本规格引用的行号以 2026-10-12 工作区为准；`go/login` 在工作区里没有未提交的改动（`git status` 未列出 go/login），所以 HEAD 与工作区的 go/login 一致。
> 已经过一轮对抗评审（逐条回到代码核对），吸收情况见文末「评审处理记录」。

---

## 0. 结论（一句话）

锁的范围（全服一把）和出队范围（只有本 zone）由两个独立的输入决定，两者对不上。修法是让 `Dispatcher` 只认一个 `zoneID`：出队范围用它，锁键也从它派生（`<DispatcherLockKey>:zone:<zoneID>`）。这样每个 zone 各有一个 leader，两种范围不会再对不上。配置键名和默认值不变，客户端契约不变，Java 版不受影响。

---

## 1. 现状事实（缺陷成因）

| # | 事实 | 出处 |
|---|---|---|
| F1 | `DispatcherLockKey` 默认 `dispatcher:lock:login_queue`，全服只有一个键；`DispatcherLockTTL` 默认 30s | `go/login/internal/config/config.go:252-253` |
| F2 | 本地配置同值；`Queue.Enabled: false` | `go/login/etc/login.yaml:428-436`（Enabled 在 :429，锁键在 :436） |
| F3 | GateWatcher 的 etcd 前缀带本进程 `Node.ZoneId`，格式为 `<svc>.rpc/zone/<zone>/node_type/<t>/` | `go/login/internal/svc/servicecontext.go:151-162`；`go/login/internal/logic/pkg/node/node.go:36-37` |
| F4 | `activeZones` 的注释说取「有 gate 的 zone ∪ 非空队列的 zone」，实现只遍历 GateWatcher 的节点，所以结果只有本 zone（本 zone 一台 gate 都没有时为空） | `servicecontext.go:449-466` |
| F5 | 锁键按配置原样传进 `NewDispatcher` | `servicecontext.go:468-481` |
| F6 | 选主时 `TryLock(d.lockKey)`；没抢到的副本每 `max(lockTTL/3,1s)`（即 10s）重试一次，**永远不出队** | `go/login/internal/logic/pkg/loginqueue/dispatcher.go:155-202`（backoff 在 :159，TryLock 在 :166） |
| F7 | `dispatchOnce` 只遍历 `activeZonesProvider` 返回的 zone | `dispatcher.go:226-285` |
| F8 | 就算 leader 去遍历别的 zone 也出不了队：候选 gate 按 zone 过滤（`servicecontext.go:348`），本进程只看得见本 zone 的 gate，于是 `FreeSlots` 对外 zone 报 `no gates for zone` | `servicecontext.go:342-349`；`go/login/internal/logic/pkg/loginqueue/capacity.go:62-71` |
| F9 | `ZoneCapacityOverride` 写在各 login 自己的配置里，全局 leader 拿不到别的 zone 的容量。K8s 的 login ConfigMap 里根本没有 `Queue` 段 | `config.go:257`；`tools/scripts/k8s_deploy.ps1:3558-3690`（login 模板，无 `Queue:`） |
| F10 | 多 zone 确实共用一个 Redis。K8s 下所有 zone 的 login 都连 `redis.${InfraNamespace}:6379`；本地多 zone 的派生 yaml 只改写 `ZoneId` 和端口，Redis 与锁键都相同 | `k8s_deploy.ps1:3575-3584`；`tools/scripts/go_services.ps1:64-65`、`:399-410` |
| F11 | 网关按 zone 把 AssignGate 路由到该 zone 的 login，因为每个 login 只看得见本 zone 的 gate。入队用的是 `in.ZoneId`；外 zone 的请求在 `FreeSlots` 处报错，不会入队。所以 `queue:zone:Z` 只会由 zone Z 的 login 写入（Z≠0） | `java/gateway_node/.../grpc/LoginRpcClient.java:80-95`；`go/login/internal/logic/loginpregate/assigngatelogic.go:95-107`、`:132` |
| F12 | 后果是整个 zone 被堵死：快速通道要求 `queueLen == 0`（`assigngatelogic.go:110`）。非 leader zone 只要有一个人入队，之后进来的人全部入队。每次 ZADD 都会刷新 ZSET 的 TTL（`queue.go:172-173`），只要有人持续进来，队列就永远不会过期 | 同左 |
| F13 | leader 指标只有 `pod` 一个标签，runbook 的告警写的是 `sum(...) != 1`。按 zone 分锁后这条告警会误报 | `go/login/internal/logic/pkg/loginqueue/metrics.go:116-129`；`docs/ops/login-queue-stress-runbook.md:176`、`:182` |
| F14 | `pkg/locker` 用 SETNX 加锁、Lua 比对后续期 / 释放，心跳超过 20s 没续上就自我降级（TTL 30s） | `go/login/internal/logic/pkg/locker/player_locker.go:56-72`、`:126-238`（:165 有数值） |

**为什么不选「全局 leader + activeZones 覆盖所有 zone」**（launch_verdict 给的另一个方案）：要让 leader 出得了别的 zone 的队，它就得同时具备三样东西：(a) 跨 zone 的 gate 视图，即另起一个不带 zone 的 etcd watch，并且要和 `buildGateCandidates` 的过滤和去重保持一致；(b) 所有 zone 的 `ZoneCapacityOverride`，而这个配置是按 zone 分开下发的（F9）；(c) 接受「一台 login 挂了，全服排队停摆约 30s」这种跨 zone 耦合（launch_verdict:175 列为 P1）。按 zone 拆锁三样都不需要，改动也最小。

---

## 2. 文件清单

| 类型 | 文件 | 要点 |
|---|---|---|
| 修改 | `go/login/internal/logic/pkg/loginqueue/dispatcher.go` | 去掉 `activeZonesProvider`，改为单个 `zoneID` 字段；`NewDispatcher` 签名改为 `(…, lockTTL, lockKeyBase string, zoneID uint32) (*Dispatcher, error)`；新增 `ZoneDispatcherLockKey`；`dispatchOnce` 只处理本 zone，并且**先看队列长度再算容量**；日志和 leader 指标都带上 zone；重写包头注释 |
| 修改 | `go/login/internal/logic/pkg/loginqueue/metrics.go` | `dispatcherIsLeaderGauge` 的标签从 `pod` 扩为 `pod, zone`；更新头注释（:14）和变量注释（:116-120）里「sum==1」的说法 |
| 修改 | `go/login/internal/svc/servicecontext.go` | 删掉 `activeZones` 闭包（:449-466，它的注释与实现不一致）；把 `config.AppConfig.Node.ZoneId` 作为 zoneID 传给 `NewDispatcher`；构造失败时 panic（启动期 fail-closed，只在 `Queue.Enabled=true` 时触发） |
| 修改 | `go/login/internal/config/config.go` | **只改 `QueueConf` 的类型注释**（:233-244），补一条说明 `DispatcherLockKey` 现在是前缀。字段行不动，避免 gofmt 重新对齐（见 §3.5） |
| 修改 | `go/login/etc/login.yaml` | 在 `DispatcherLockKey` 上方加注释；Roll-out checklist（:415-420）补一条多 zone 升级顺序。**值不变** |
| 修改 | `go/login/internal/logic/pkg/loginqueue/dispatcher_test.go` | 现有 3 个用例改用新签名；新增 5 个用例（T1–T4、T9，§5） |
| 新增 | `go/login/internal/svc/login_queue_wiring_test.go` | 接线测试 T8a–T8c（§5.3）：zoneID 取自 `Node.ZoneId`、锁键按 zone、`ZoneId=0` 起服 panic、`Enabled=false` 不校验 |
| 修改 | `docs/design/login-queue-2026-05.md` | §3 拓扑、§5 Dispatcher Loop、§7 新增 7.1、§10、§11 指标表、§14 第 5 条、§15 应急清理（§6 逐条列出） |
| 修改 | `docs/ops/login-queue-stress-runbook.md` | 锁键名、leader 不变量和告警的 PromQL 改为按 zone；「双 leader」症状改为「同一 zone」；删掉 activeZones 公平性条目（:69、:70、:84、:88、:150、:176、:182、:196、:207） |
| 修改 | `docs/design/ARCH.md` | 只改两处措辞：:419 文档索引行、:496 决策 #14 的「单 leader dispatcher」补「2026-10-12 起每 zone 一个 leader」。该文件 :503 有别人未提交的改动，**不碰** |
| 不改 | `tools/scripts/k8s_deploy.ps1` | login ConfigMap（:3558-3690）**没有写** `Queue` 段，也没有 `DispatcherLockKey`，所以不用改。L-02 打开排队时要另外加这一段，那属于 L-02 的范围 |
| 不改 | `deploy/login-stack.linux/login.yaml` | 没有 `Queue` 段（`grep` 只找到 `QueueShardCount`） |
| 不改 | `AGENTS.md`、`docs/PROGRESS.md`、`docs/README.md` | 按任务约定，进度由 Docs 阶段统一追加；没有新建文档，不用登记索引 |
| 不改 | Java 版（`D:\luyuan\xuanming-server-mmo`） | 不受影响，见 §10 |

改动共 10 个文件（9 改 1 新增），没有 proto 改动，没有生成产物，没有 C++ 改动。

---

## 3. 代码规格（可照写）

### 3.1 `loginqueue/dispatcher.go`

**import**：在现有 import 基础上加 `"errors"` 和 `"fmt"`。`context`、`sync`、`time`、`locker`、`redis`、`logx`、`safego` 都继续使用。

**包头注释**（替换 :1-32；:4-11 那段英文的 "Why leader-elect" 和 :13-27 的 lifecycle 原样保留，只改选主范围和 drain 范围两段）：

```go
// dispatcher.go — 每个 zone 一个 leader 的出队 goroutine:把本 zone 队列里的条目
// 放行成 admit:{queueId}。
//
// Why leader-elect (instead of every login replica draining):  ← 原 :4-11 原样保留
//
// 选主范围是「一个 zone」,不是全服(G1b,2026-10-12):
//
//	login 按 zone 部署,GateWatcher 的 etcd 前缀带本进程的 Node.ZoneId,
//	CapacityProvider 只看得见本 zone 的 gate。出队要先算 FreeSlots、再挑 gate,
//	所以一个 dispatcher 只能出它看得见 gate 的那个 zone。旧实现的锁键全服一把、
//	出队范围却只有本 zone:多个 zone 共用一个 Redis 时,只有抢到锁的那个 zone
//	出队,其它 zone 一个人都放不进来。现在锁键 = ZoneDispatcherLockKey(配置前缀, zoneID),
//	锁的范围与出队范围由同一个 zoneID 决定,不会再分家。
//
// Leader election uses pkg/locker.RedisLocker:  ← 原 :13-17 原样保留
// Dispatcher lifecycle:                          ← 原 :19-27 原样保留
//
// 每拍只看本 zone:队列为空直接跳过(不碰 gate 目录);非空才算 FreeSlots 并出队。
package loginqueue
```

**结构体**（替换 :48-75；注意 gofmt 的对齐分段，插入注释行会把对齐分成新段）：

```go
// Dispatcher drains ONE zone's queue. Login replicas of the same zone compete
// for the same zone-scoped lock; replicas of other zones never contend with it.
type Dispatcher struct {
	queue        *Queue
	provider     CapacityProvider
	rdb          *redis.Client
	hmacSecret   []byte
	gateTokenTTL time.Duration

	interval          time.Duration
	softCapMultiplier float64
	lockTTL           time.Duration
	// zoneID 是本 dispatcher 唯一负责出队的 zone(本 login 的 Node.ZoneId);
	// lockKey 由它派生,锁的范围与出队范围不会分家。
	zoneID  uint32
	lockKey string

	// podID is the metric label for login_queue_dispatcher_is_leader.  ← 原 :61-63 注释保留
	podID string

	cancel  context.CancelFunc
	stopped chan struct{}
	mu      sync.Mutex
}
```

**构造与锁键**（替换 :77-103）：

```go
// NewDispatcher builds a Dispatcher that drains exactly one zone. Callers must
// invoke Start exactly once.
//
// lockKeyBase 是配置里的 Queue.DispatcherLockKey,实际抢的锁键是
// ZoneDispatcherLockKey(lockKeyBase, zoneID)。zoneID 必须是本 login 的 Node.ZoneId,
// 也就是 provider 看得见 gate 的那个 zone;否则抢到了锁也出不了队。
//
// zoneID 为 0 或 lockKeyBase 为空时返回错误,并且不起任何 goroutine:zone 0 的队列条目
// 在 Lookup 里被当成损坏数据(queue.go:265),空前缀会让锁键和别的业务键挤在一起。
func NewDispatcher(
	queue *Queue,
	provider CapacityProvider,
	rdb *redis.Client,
	hmacSecret []byte,
	gateTokenTTL time.Duration,
	interval time.Duration,
	softCapMultiplier float64,
	lockTTL time.Duration,
	lockKeyBase string,
	zoneID uint32,
) (*Dispatcher, error) {
	if zoneID == 0 {
		return nil, errors.New("loginqueue: dispatcher zone id must be non-zero")
	}
	if lockKeyBase == "" {
		return nil, errors.New("loginqueue: dispatcher lock key base must be non-empty")
	}
	return &Dispatcher{
		queue:             queue,
		provider:          provider,
		rdb:               rdb,
		hmacSecret:        hmacSecret,
		gateTokenTTL:      gateTokenTTL,
		interval:          interval,
		softCapMultiplier: softCapMultiplier,
		lockTTL:           lockTTL,
		zoneID:            zoneID,
		lockKey:           ZoneDispatcherLockKey(lockKeyBase, zoneID),
		stopped:           make(chan struct{}),
	}, nil
}

// ZoneDispatcherLockKey 返回 zone 级出队 leader 锁键 <base>:zone:<zoneID>。
// 同一 zone 的 login 副本抢同一把锁,不同 zone 互不相干。
// 排查时用:redis-cli GET dispatcher:lock:login_queue:zone:1
func ZoneDispatcherLockKey(base string, zoneID uint32) string {
	return fmt.Sprintf("%s:zone:%d", base, zoneID)
}
```

**leader 指标的小助手**（新增，放在 `podLabel` 后面）。它替换 :115、:178、:186、:193 四处 `dispatcherIsLeaderGauge.Set(…, d.podLabel())`：

```go
// setLeaderGauge 发布本副本在本 zone 的 leader 状态。zone 是低基数标签(zone 数),
// 让「每个 zone 恰好一个 leader」能直接写成 sum by (zone) (...) == 1。
func (d *Dispatcher) setLeaderGauge(v float64) {
	dispatcherIsLeaderGauge.Set(v, d.podLabel(), labelZone(d.zoneID))
}
```

**electAndRun 的日志**（:177、:185）：

```go
logx.Infof("[loginqueue] elected dispatcher leader zone=%d key=%s", d.zoneID, d.lockKey)
...
logx.Errorf("[loginqueue] dispatcher lost leadership zone=%d: %v", d.zoneID, err)
```

选主循环的其余部分（TryLock、心跳、释放）一律不动。

**dispatchOnce**（替换 :223-285）。改成单 zone，并把 `QueueLen` 提到 `FreeSlots` 之前：

```go
// dispatchOnce runs one drain pass over this dispatcher's zone. 出错只记日志,
// 条目留在队列里等下一拍,不丢人。
func (d *Dispatcher) dispatchOnce(ctx context.Context) {
	zoneID := d.zoneID
	// Publish gauge snapshot first ...  ← 原 :232-234 注释保留
	publishGaugeSnapshot(ctx, d.queue, zoneID)

	// 先看队列再算容量。以前 activeZones 来自 GateWatcher,本 zone 一台 gate 都没有时
	// 根本不会走到这里;现在本 zone 总在出队范围内,不先判空的话,空闲又没 gate 的 zone
	// 会每拍刷一条 freeSlots ERROR。队列非空却没有 gate,说明有人在排而放不进去,
	// 下面照常报 ERROR。
	queueLen, err := d.queue.QueueLen(ctx, zoneID)
	if err != nil || queueLen == 0 {
		return
	}
	free, _, _, _, err := FreeSlots(ctx, d.provider, d.queue, zoneID, d.softCapMultiplier)
	if err != nil {
		logx.Errorf("[loginqueue] freeSlots zone=%d queueLen=%d err=%v", zoneID, queueLen, err)
		return
	}
	if free == 0 {
		return
	}

	batch := min(free, queueLen)

	// pickFn ... ← 原 :252-272 原样搬过来(闭包参数 zoneID 遮蔽外层同名变量,行为与原来一致)

	admitted, err := d.queue.PopAdmit(ctx, zoneID, int(batch), pickFn)
	if err != nil {
		logx.Errorf("[loginqueue] PopAdmit zone=%d err=%v", zoneID, err)
		return
	}
	recordAdmit(zoneID, admitted)
	if admitted > 0 {
		logx.Infof("[loginqueue] zone=%d admitted=%d freeBefore=%d queueLen=%d",
			zoneID, admitted, free, queueLen)
	}
}
```

说明：`queueLen` 已经是 `uint32`（`queue.go:485-491`），原来的 `uint32(queueLen)` 转换可以去掉。`ctx.Err()` 的检查在单 zone 下没有意义，删掉。外层 `drainLoop` 每拍之前已经 select 过 `ctx.Done()`。

### 3.2 `loginqueue/metrics.go`

- :14 改为 `//   login_queue_dispatcher_is_leader  Gauge   per pod+zone  1 = this pod leads its zone, 0 = standby`。
- :116-120 的注释改为：「每个副本在自己所在 zone 发布 1 或 0。稳态下 `sum by (zone)` 恰好为 1；短暂为 0 表示没有 leader，短暂为 2 表示 Redis 抖动或滚动升级窗口里出现了双 leader（见 login-queue-2026-05.md §7.1）。」
- :121-129 中 `Labels: []string{"pod", "zone"}`；Help 改为 `"... sum by (zone) (login_queue_dispatcher_is_leader) should be 1 in steady state."`。
- §9 合规：zone 是低基数标签，没有引入 player_id。

### 3.3 `svc/servicecontext.go`

删掉 :449-466 的 `activeZones` 闭包和它的注释（注释与实现不一致，F4）。:468-481 替换为：

```go
	// 出队只能覆盖本 zone:GateWatcher 的前缀带 Node.ZoneId(见 NewServiceContext 的 gatePrefix),
	// FreeSlots 和挑 gate 都只看得见本 zone 的 gate。所以锁也只抢本 zone 那一把:同 zone 的副本
	// 互为主备,不同 zone 各有 leader。zoneID 必须与 gatePrefix 用同一个值(G1b:锁键原来全服
	// 一把,多 zone 共用 Redis 时只有 leader 所在 zone 出队)。
	dispatcher, err := loginqueue.NewDispatcher(
		s.LoginQueue,
		s.queueCapProvider,
		s.RedisClient,
		// dispatcher 这条参数是给 gate token 签名用的(它只挑 gate、
		// 真正签名在 consume 时由 handler 做),所以传 gate 密钥不是队列密钥。  ← 原注释保留
		s.GateTokenSigningSecret(),
		10*time.Minute, // gateTokenTTL — must match assigngatelogic.gateTokenTTL (Round 19, R17 R2 收尾)
		cfg.DispatchInterval,
		cfg.SoftCapMultiplier,
		cfg.DispatcherLockTTL,
		cfg.DispatcherLockKey,
		config.AppConfig.Node.ZoneId,
	)
	if err != nil {
		// 开了排队却造不出 dispatcher,本 zone 的队列就只进不出。起服即失败,不带病上线。
		panic(fmt.Errorf("login queue dispatcher: %w", err))
	}
	s.QueueDispatcher = dispatcher
```

:482-489 的 `SetPodID` 不变。`fmt`、`context`、`os` 在本文件其它地方仍在使用，import 不变。panic 的写法与同文件 :119、:160、:190 一致。

### 3.4 `config/config.go`（只改注释）

在 `QueueConf` 类型注释（:233-244）末尾追加一条（与现有 `//   - ` 列表格式相同）：

```go
//   - DispatcherLockKey 是出队 leader 锁键的**前缀**,实际锁键是
//     <DispatcherLockKey>:zone:<Node.ZoneId>(loginqueue.ZoneDispatcherLockKey),
//     每个 zone 一个 leader。dispatcher 只看得见本 zone 的 gate,锁的范围必须与之相同。
//     2026-10-12 以前它就是锁键本身、全服一把,多 zone 共用 Redis 时只有 leader 所在
//     zone 出队(G1b)。默认值没变,旧配置无需改动;Queue.Enabled=true 时它不能为空、
//     Node.ZoneId 不能为 0,否则起服失败。同一 zone 的所有 login 副本必须配同一个值,
//     配得不一样就是两把互不相干的锁,该 zone 长期双 leader。
```

### 3.5 gofmt 注意（Claude 不跑 gofmt）

- `config.go` 的字段行（:246-257）**不要插入注释**：插入后对齐会分段，`DispatcherLockKey` 一行的空格数必须改，很容易手写错。
- `dispatcher.go` 结构体按 §3.1 写：`zoneID  uint32`（两个空格）、`lockKey string`、`mu      sync.Mutex`（六个空格）。构造体字面量的 key 以 `softCapMultiplier:`（18 字符）为准对齐到第 19 列。
- Codex 跑 `gofmt -l` 时如果列出了这些文件，**允许** Codex 对这几个文件执行 `gofmt -w`（只改格式），并在报告里注明。

### 3.6 `etc/login.yaml`

在 :435 和 :436 之间插入注释（值不变）：

```yaml
  DispatcherLockTTL: 30s
  # 出队 leader 锁键的前缀:实际锁键 = <DispatcherLockKey>:zone:<Node.ZoneId>,每个 zone 一个 leader
  # (2026-10-12 G1b;此前全服一把,多 zone 共用 Redis 时只有 leader 所在 zone 出队)。各 zone 照写这个默认值即可;
  # 同一 zone 的副本必须同值,否则该 zone 会长期双 leader。
  DispatcherLockKey: "dispatcher:lock:login_queue"
```

Roll-out checklist（:415-420）在第 1 条前插入：

```yaml
#   0. 多个 zone 共用一个 Redis 时,先把所有 zone 的 login 升到 2026-10-12(G1b)之后的版本,
#      再打开 Enabled。新旧版本混跑时,同一个 zone 可能短暂出现两个 leader:只会多放行一批,
#      不丢人、不重复放行(docs/design/login-queue-2026-05.md §7.1)。
```

---

## 4. 关键语义

### 4.1 Redis 键

| 键 | 类型 | TTL | 说明 |
|---|---|---|---|
| `dispatcher:lock:login_queue:zone:{zoneId}` | STRING（owner uuid） | `DispatcherLockTTL`（30s），心跳每 10s 续一次 | **新增**。每个 zone 一把 |
| `dispatcher:lock:login_queue` | STRING | 30s | 新版本**不再写**。老版本 leader 停止续期后 30s 内自然过期，不需要人工清理 |
| `queue:zone:{z}`、`admitted:zone:{z}`、`admit:{id}`、`queue:meta:{id}`、`queue:token:{t}` | — | 不变 | 不变（`queue.go:138-142`） |

锁和队列键用的是同一个 `s.RedisClient`（`servicecontext.go:442-443`、`:470-471`），这个前提要保留：launch_verdict:173 建议以后按用途拆 Redis 实例，拆的时候 leader 锁必须跟着排队键走到同一个实例。锁和它保护的队列不在一个实例上时，Redis 抖动会出现「锁还在、队列读不到」或反过来的组合，§4.5 的有界分析就不成立了。按 zone 分锁本身与「每 zone 一个 Redis」「全服共享一个 Redis」两种拓扑都兼容。

### 4.2 配置键与默认值

| 键 | 默认 | 语义变化 |
|---|---|---|
| `Queue.DispatcherLockKey` | `dispatcher:lock:login_queue`（不变） | 由「锁键」改为「锁键前缀」。同一 zone 的副本必须同值（修复前后都一样：值不同就是两把锁、长期双 leader，超发上界按 §4.5 第 2 条翻倍）；不同 zone 可以同值，也可以不同 |
| `Queue.DispatcherLockTTL` | 30s（不变） | 无 |
| `Queue.Enabled` | false（不变） | 无。**本修复不打开排队**，打开是 L-02 的事 |
| `Node.ZoneId` | — | `Queue.Enabled=true` 时必须非 0（新增的启动校验） |

没有新增开关。不保留「全服一把锁」的兼容开关，因为那种行为就是缺陷本身；而且队列默认是关的，存量部署的行为没有变化。

### 4.3 错误语义

| 场景 | 行为 | 方向 |
|---|---|---|
| `Queue.Enabled=true` 且 `Node.ZoneId==0` 或 `DispatcherLockKey==""` | `NewDispatcher` 返回 error，`initLoginQueue` panic，login 起不来 | fail-closed：开了排队却出不了队 = 队列只进不出 |
| `Queue.Enabled=false` | 不构造 dispatcher，没有任何校验（`servicecontext.go:439-441`） | 与现状一致 |
| 本 zone 队列为空 | 每拍只做一次 gauge 快照（ZCARD + SCARD，`metrics.go:201-208`）再加一次 ZCARD，不碰 gate 目录（不取 etcd 快照、不查排空标记） | 比现状更省；由 T9 钉住 |
| 队列非空，但本 zone 没有 gate 或取不到 etcd | 每拍一条 `[loginqueue] freeSlots zone=… queueLen=… err=…` ERROR，条目留在队列里 | 以前这种情况被 activeZones 静默跳过，现在暴露出来，便于排查「有人在排、放不进去」 |
| 抢锁失败或 Redis 抖动 | 与现状一致，10s 后重试 | 不变 |
| 心跳丢锁 | 与现状一致，`drainCancel` 后重新参选 | 不变 |
| AssignGate / QueryQueueStatus 的应答和 tip | **完全不变** | 客户端契约不变 |

### 4.4 并发与生命周期

- 状态所有者：每个 `Dispatcher` 只有一个选主 goroutine（`dispatcher.go:120`），`zoneID` 和 `lockKey` 构造后只读。
- 同 zone 多副本：同一把锁，任一时刻最多一个 leader（Redis 主从切换的窗口除外，见 4.5）。
- 不同 zone：锁不同、队列键不同、`admitted` 集合不同，彼此完全独立。
- zone-down：该 zone 的 login 全部下线后锁不再续期，30s 内过期，别的 zone 不关心这把锁。`Stop()` 会主动 `Release`（`dispatcher.go:197-199`），但 SIGTERM 路径能不能走到 `ctx.Stop()`（`login.go:110`）**未核实**：login 的写法假定 `server.Start()` 返回后 defer 会执行（`login.go:294-298` 的注释），scene_manager 的注释却说 go-zero 的 SIGTERM 走 `os.Exit`、main 的 defer 不执行，所以它另挂了 `proc.AddShutdownListener(elector.Stop)`（`go/scene_manager/scene_manager_service.go:123-126`）。两边说法冲突。最坏情况是锁按 TTL 过期：滚动升级时该 zone 约 30s 没有 leader，期间不放行、不丢人。修复前（全服锁）也是这样，不是 G1b 引入的回归。要消除这段空窗，可以照 scene_manager 的做法另开小改动，不并入 G1b。

### 4.5 滚动升级（新旧锁键并存窗口）安全性

窗口内的情况：老版本 leader 持有全服旧键，出的是它自己所在的 zone Zo；Zo 里已经升级的新版本副本会去抢 `…:zone:Zo`。两把锁不互斥，所以 **Zo 会短暂出现两个 leader**。其它 zone 的新版本副本各抢各的键，从它们升级那一刻起就恢复出队。

两个 leader 同时出队同一个 zone：

1. **不丢人、不重复放行**：`ZPOPMIN` 是原子操作（`queue.go:396`），每个 queueId 只会被一个 leader 弹出。弹出后写 `admit:{id}` 和 `SADD admitted`（`queue.go:428-432`）；挑 gate 失败时以 score 0 放回队首（`queue.go:415`），放回的只是自己弹出的那些。这里的「不丢人」是指**双 leader 不比单 leader 多丢**：`PopAdmit` 本身有几条既有的丢条目路径（`pickFn` 返回 `(nil, nil)` 时 :418-422 直接丢；`json.Marshal` 或写 admit 的事务失败时 :423-434 `continue`，条目已经弹出、没写 admit），单 leader 下同样存在，不在 G1b 范围内。
2. **超发有界**：每个 leader 都按自己读到的 `SCARD admitted` 算余量（`capacity.go:89-100`），而且自己上一拍的 SADD 在下一拍读数之前已经完成。没人消费时，每个 leader 累计至多放满一个容量，两个 leader 合计 ≤ 2×cap。有人消费（SREM）之后的每个空档，同样至多被两个 leader 各放一次。结论与设计稿既有的判断一致：「双 leader 的最坏情况是 admit 多发一批……不是数据损坏」（`docs/design/login-queue-2026-05.md:217`）。快速通道的 Lua 占位（`queue.go:456-482`）同样计入 admitted，不受影响。
3. **消除窗口的办法（推荐）**：当前所有环境的排队都是关的（`login.yaml:429`；K8s 的 ConfigMap 没有 `Queue` 段）。**在 Enabled=false 的状态下把本修复滚完，再打开排队（L-02）**，这样根本不会出现并存窗口。回滚（新版本退回旧版本）时的窗口与上面对称，分析相同。Enabled=false 时不构造 dispatcher，`login_queue_dispatcher_is_leader` 一条序列都没有，所以新旧标签集（`{pod}` 与 `{pod, zone}`）也不会在同一时刻并存；如果偏要在 Enabled=true 下滚动，窗口里旧 pod 的序列在 `sum by (zone)` 里归到 `zone=""` 一组，这一组触发 §6 第 8 条的告警属于预期。

用例 `TestIntegration_DualLeadersSameZone_NoLossBoundedOverAdmit`（§5）把 1、2 两条钉住，并断言两把锁都被持有，证明测试确实跑在双 leader 的形状上。

### 4.6 zone-down 与合服后旧 zone 的队列

- 本修复**不产生新的残留**。源 zone S 的 login 下线后，不再有人 ZADD，`queue:zone:S` 在最后一次入队后 `QueueEntryTTL`（1h）内过期；`admit:*` 和 `admitted:zone:S` 在 `AdmitTTL`（60s）内过期，meta 和 token 1h 内过期（`queue.go:172-181`、`:429-431`），锁键 30s 内过期。
- 目标 zone T 的 leader 只出 `queue:zone:T`，与 S 无关。S 的排队玩家要回到 T，靠的是重新走 AssignGate 时角色列表已经给出归属 zone，这条路径不变。queue token 绑定了 zone（设计稿 §6），排队位次不迁移，设计稿 §14 第 4 条早已接受这一点。S 没有 login 时，网关的 `/api/queue-status` 按 zone 选 login 通道，选不到就抛错（`LoginRpcClient.java:328-331` 走 zone 必填的 `unaryCall`，`pickChannel` 在 :376-386 报 `login.rpc no endpoint for zone=`），客户端拿到的是失败、不是排队中，然后会重新走 AssignGate。这条与本修复无关。
- 合服回退（unmerge）把 S 拉起来后，S 的新 leader 自然接手。
- 可选的人工清理写进设计稿 §15（§6 第 7 条），不改 `merge-zone-runbook.md`。

### 4.7 既有边角（不在本修复范围，只登记）

1. **`queue:zone:0` 黑洞**。`in.ZoneId == 0` 时 `buildGateCandidates` 不按 zone 过滤（`servicecontext.go:348`），`FreeSlots` 会成功，条目被写进 `queue:zone:0`。没有任何 leader 出这个队，`Lookup` 还把 zone 0 当成损坏数据（`queue.go:265-272`），等于一个黑洞。修复前后都存在。正常路径到不了这里：mmorpg 网关的 AssignGate / QueryQueueStatus 走 zone 必填的 `unaryCall`，`zoneId <= 0` 时在调用 login 之前就抛 `IllegalArgumentException`（`java/gateway_node/.../grpc/LoginRpcClient.java:340-343`）；Java 版网关对 `zone_id=0` 回 404（Java 仓库 `PARITY.md:27`）。但集群内能直连 login gRPC 的调用方不受网关约束。是否加防御见 §9 第 3 条。
2. **快速通道占位只靠整集合 TTL 回收**（打开排队 L-02 之前要单独评估，**不并入 G1b**）。快速通道往 `admitted:zone:Z` 里 SADD 一个 `fast:<uuid>` 占位并刷新整个集合的 TTL（`queue.go:456-482`），dispatcher 每次放行也会刷新（`queue.go:430-431`）。全仓只有 `consumeAdmitAndSign` 会 SREM，而且只删 queueId（`queue.go:355`），`fast:*` 占位永远不会被单独删掉。只要 60s（AdmitTTL）内还有人走快速通道或被放行，集合就不过期，已经连上 gate 的快速通道玩家会被同时算进 online 和 admitted（`capacity.go:89-100`）。可用余量于是被重复扣减，扣到 0 之后不再有 SADD，集合要再过最多 60s 才过期，然后放下一批。持续有人登录时，zone 会在「放一批 → 停摆最长 60s」之间循环。Java 版 `PARITY.md:98` 有意差异 ② 记的就是这条基线缺陷（Java 改成按成员到期的 ZSET）。
3. **放行条目取走即撤占位**（同样留到 L-02 评估）。`consumeAdmitAndSign` 在客户端取走 admit 时就 SREM（`queue.go:355`），这时玩家还没连上 gate，gate 也还没上报新的 PlayerCount，所以这段时间他既不在 admitted 里，也不在 online 里，下一拍可能多放一批。Java 版 `PARITY.md:98` 有意差异 ③ 记了这条（取走后再留 15s）。

第 2、3 条与 G1b 正交（单 leader 和每 zone 一个 leader 下表现相同），但都会在 L-02 打开排队后的开服首日出现，所以登记在这里，处理方式见 §9 待拍板第 7 条。

---

## 5. 测试清单

T1–T7、T9 在 `go/login/internal/logic/pkg/loginqueue/dispatcher_test.go`（`package loginqueue`）；T8a–T8c 在新文件 `go/login/internal/svc/login_queue_wiring_test.go`（`package svc`）。全部只依赖 **miniredis（进程内）**，不需要任何外部依赖。

### 5.1 公共助手（新增）

```go
// mustNewDispatcher 用测试节拍(30ms 一拍、2s 锁)构造一个只负责 zoneID 的 dispatcher。
func mustNewDispatcher(t *testing.T, q *Queue, prov CapacityProvider, rdb *redis.Client,
	secret []byte, lockKeyBase string, zoneID uint32) *Dispatcher {
	t.Helper()
	d, err := NewDispatcher(q, prov, rdb, secret, 300*time.Second, 30*time.Millisecond, 1.5,
		2*time.Second, lockKeyBase, zoneID)
	if err != nil {
		t.Fatalf("NewDispatcher(zone=%d): %v", zoneID, err)
	}
	return d
}
```

### 5.2 用例

| # | 用例名 | 新增 / 改 | 外部依赖 | 断言 |
|---|---|---|---|---|
| T1 | `TestZoneDispatcherLockKey_IsPerZone` | 新增 | 无 | `ZoneDispatcherLockKey("dispatcher:lock:login_queue",1) == "dispatcher:lock:login_queue:zone:1"`；zone 1/2/10/101 的键互不相同，也都不等于前缀本身 |
| T2 | `TestNewDispatcher_RejectsZeroZoneOrEmptyLockKeyBase` | 新增 | 无 | `(…, "dispatcher:lock:login_queue", 0)` 和 `(…, "", 1)` 都返回 `nil, err != nil`（q / prov / rdb 传 nil，证明校验发生在任何使用之前） |
| T3 | `TestIntegration_TwoZonesSharedRedis_BothQueuesDrain` | 新增，**G1b 回归** | miniredis | 两个 zone 共用一个 miniredis 和一个 `Queue`，前缀相同（login.yaml 默认值），各自的 provider 只认本 zone 的 gate；各入队 5 人，3s 内两个队列都出空；同时 `EXISTS …:zone:1 …:zone:2 == 2`，且旧的全服键不存在（`mr.Exists(base) == false`） |
| T4 | `TestIntegration_DualLeadersSameZone_NoLossBoundedOverAdmit` | 新增，**并存窗口** | miniredis | 同一个 zone 1，两个 dispatcher 用不同前缀（模拟老版本全服键与新版本 zone 键并存，两把锁不互斥）。cap=4，PlayerCount=0，先入队 20 人再 Start。等到 `ZCARD+SCARD==20` 且 `SCARD>=4` 后：两把 zone 锁 `EXISTS == 2`（确认真的是双 leader，否则用例会在单 leader 下空过）；再等 300ms 复核：`SCARD admitted ≤ 2×cap`；ZSET 成员与 admitted 成员的并集正好是入队的 20 个 queueId，且每个只出现一次 |
| T5 | `TestIntegration_QueueDrainAdmits` | 改：fixture 换新签名 | miniredis | 不变 |
| T6 | `TestIntegration_DispatcherRespectsCapacity` | 改：fixture 换新签名 | miniredis | 不变 |
| T7 | `TestIntegration_LeaderElectionSerializesDispatch` | 改：用 `mustNewDispatcher(…, "dispatcher:lock:dual", 1)` ×2 | miniredis | 不变 |
| T8a | `TestInitLoginQueue_DispatcherLocksOwnZone`（svc） | 新增，**接线** | miniredis | `Node.ZoneId=7`、`Enabled=true`、前缀取默认值：`initLoginQueue` 造出 dispatcher；Start 后 3s 内 `dispatcher:lock:login_queue:zone:7` 出现；全服旧键不存在。GateWatcher 传 nil：空队列那一拍不碰 gate 目录，所以能跑。注意 T8a 不负责检查出队顺序（写反时 panic 会被 safego 接住，T8a 照样通过），顺序由 T9 检查；`-v` 输出里出现 `login.queue_dispatcher` 的 panic 记录也算失败 |
| T8b | `TestInitLoginQueue_ZeroZonePanicsWhenQueueEnabled`（svc） | 新增，**fail-closed** | miniredis | `Enabled=true`、`Node.ZoneId=0` → `initLoginQueue` panic |
| T8c | `TestInitLoginQueue_DisabledSkipsDispatcherValidation`（svc） | 新增，**向后兼容** | miniredis | `Enabled=false`、`ZoneId=0`、前缀为空 → 不 panic，`LoginQueue` 和 `QueueDispatcher` 都是 nil |
| T9 | `TestDispatchOnce_EmptyQueueSkipsGateDirectory` | 新增，**先判空再算容量** | miniredis | 不 Start，直接调 `dispatchOnce`（没有 goroutine，结果确定）。空队列调 3 次，`CandidatesForZone` 一次都没被调用；入队 1 人后再调 1 次，`CandidatesForZone` 被调用过，队列变空 |

`startIntegrationFixture`（:40-72）改为：删掉 `activeZones` 闭包，调用 `NewDispatcher(q, prov, rdb, secret, 300*time.Second, 50*time.Millisecond, 1.5, 2*time.Second, "dispatcher:lock:test", 1)`，err 非空时 `t.Fatalf`，并加注释「用它的两个用例都只有 zone 1」。

### 5.3 参考实现（T3、T4 骨架、T9、T8）

```go
// G1b 回归:两个 zone 的 login 共用一个 Redis,配置里的 DispatcherLockKey 一模一样(login.yaml 默认值),
// 各自只看得见本 zone 的 gate。每个 zone 都要有自己的 leader,队列都要出空。
// 旧实现锁键全服一把,只有抢到锁的 zone 出队,另一个 zone 一个人都放不进来。
func TestIntegration_TwoZonesSharedRedis_BothQueuesDrain(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	const base = "dispatcher:lock:login_queue"
	newZoneDispatcher := func(zone uint32) *Dispatcher {
		// 照生产形状:每个 zone 的 provider 只认得本 zone 的 gate(GateWatcher 前缀带 ZoneId)。
		prov := &integrationProvider{
			caps:  map[uint32]uint32{zone: 100},
			cands: map[uint32][]GateCandidate{zone: {{NodeID: zone, IP: "10.0.0.1", Port: 9000, ZoneID: zone}}},
		}
		return mustNewDispatcher(t, q, prov, rdb, secret, base, zone)
	}
	d1, d2 := newZoneDispatcher(1), newZoneDispatcher(2)
	d1.Start()
	defer d1.Stop()
	d2.Start()
	defer d2.Stop()

	for range 5 {
		if _, err := q.Enqueue(ctx, 1, "zone1-account", "dev"); err != nil {
			t.Fatalf("enqueue zone 1: %v", err)
		}
		if _, err := q.Enqueue(ctx, 2, "zone2-account", "dev"); err != nil {
			t.Fatalf("enqueue zone 2: %v", err)
		}
	}
	waitFor(t, 3*time.Second, "zone 1 and zone 2 queues both drained", func() bool {
		n1, err1 := q.QueueLen(ctx, 1)
		n2, err2 := q.QueueLen(ctx, 2)
		return err1 == nil && err2 == nil && n1 == 0 && n2 == 0
	})

	// 锁的形状:每个 zone 一把,全服旧键没人写。
	n, err := rdb.Exists(ctx, ZoneDispatcherLockKey(base, 1), ZoneDispatcherLockKey(base, 2)).Result()
	if err != nil || n != 2 {
		t.Fatalf("each zone must hold its own leader lock: exists=%d err=%v", n, err)
	}
	if mr.Exists(base) {
		t.Fatalf("legacy global lock key %q must not be written any more", base)
	}
}
```

T4 的骨架（同样的 miniredis、`q`、`secret` 搭法）：

```go
	const capacity, total = 4, 20
	prov := &integrationProvider{
		caps:  map[uint32]uint32{1: capacity},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}
	want := make(map[string]bool, total)
	for range total {
		res, err := q.Enqueue(ctx, 1, "u", "d")
		if err != nil {
			t.Fatalf("enqueue: %v", err)
		}
		want[res.QueueID] = true
	}
	// 两个不同前缀 = 两把互不相干的锁 = 同一 zone 两个 leader(滚动升级窗口的形状)。
	dNew := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:login_queue", 1)
	dOld := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:legacy_window", 1)
	dNew.Start()
	defer dNew.Stop()
	dOld.Start()
	defer dOld.Stop()

	snapshot := func() (queued, admitted []string) {
		queued, _ = rdb.ZRange(ctx, zoneZSetKey(1), 0, -1).Result()
		admitted, _ = rdb.SMembers(ctx, admittedSetKey(1)).Result()
		return queued, admitted
	}
	waitFor(t, 3*time.Second, "both leaders saturated the zone", func() bool {
		qd, ad := snapshot()
		return len(ad) >= capacity && len(qd)+len(ad) == total
	})
	// 两把锁都被持有 = 真的是双 leader;否则下面的断言在单 leader 下也会过,证明不了什么。
	held, err := rdb.Exists(ctx,
		ZoneDispatcherLockKey("dispatcher:lock:login_queue", 1),
		ZoneDispatcherLockKey("dispatcher:lock:legacy_window", 1)).Result()
	if err != nil || held != 2 {
		t.Fatalf("dual-leader shape not reached: exists=%d err=%v", held, err)
	}
	time.Sleep(300 * time.Millisecond) // 再给每个 leader 约十拍,确认余量为 0 后不再放行
	queued, admitted := snapshot()
	if len(admitted) > 2*capacity {
		t.Fatalf("over-admission must be bounded by 2×cap=%d, got %d", 2*capacity, len(admitted))
	}
	seen := make(map[string]int, total)
	for _, id := range append(queued, admitted...) {
		seen[id]++
	}
	if len(seen) != total {
		t.Fatalf("entries lost: tracked %d of %d", len(seen), total)
	}
	for id, c := range seen {
		if c != 1 || !want[id] {
			t.Fatalf("queueId %s appears %d times (known=%v)", id, c, want[id])
		}
	}
```

确定性：miniredis 不按墙钟让 key 过期（需要 FastForward 才会过期），心跳续期也不会出错，所以锁的归属在测试期间是稳定的。T4 的 `≤2×cap` 在任何交错下都成立（推导见 §4.5 第 2 条）。等待都有上限（3s），没有依赖执行顺序。

T9 参考实现（`dispatcher_test.go`，需要的 import 都已存在：`context`、`sync/atomic`、`testing`、`time`、miniredis、go-redis）：

```go
// countingProvider 包一层 CapacityProvider,数 CandidatesForZone 被调了几次,
// 用来证明空队列那一拍没碰 gate 目录(生产实现每次都要取 etcd 快照、查排空标记)。
type countingProvider struct {
	CapacityProvider
	candidateCalls atomic.Int32
}

func (p *countingProvider) CandidatesForZone(ctx context.Context, zoneID uint32) ([]GateCandidate, error) {
	p.candidateCalls.Add(1)
	return p.CapacityProvider.CandidatesForZone(ctx, zoneID)
}

// G1b 之后本 zone 每拍都在出队范围内(以前要本 zone 有 gate 才进 activeZones)。
// 先判空再算容量:空闲的 zone 不碰 gate 目录,非空才走 FreeSlots。
func TestDispatchOnce_EmptyQueueSkipsGateDirectory(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-empty")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	prov := &countingProvider{CapacityProvider: &integrationProvider{
		caps:  map[uint32]uint32{1: 10},
		cands: map[uint32][]GateCandidate{1: {{NodeID: 1, IP: "10.0.0.1", Port: 9000, ZoneID: 1}}},
	}}
	// 不 Start:直接调 dispatchOnce,没有 goroutine,结果确定。没 Start 过的 dispatcher 不能 Stop。
	d := mustNewDispatcher(t, q, prov, rdb, secret, "dispatcher:lock:empty", 1)

	for range 3 {
		d.dispatchOnce(ctx)
	}
	if n := prov.candidateCalls.Load(); n != 0 {
		t.Fatalf("empty queue must not touch the gate directory, CandidatesForZone called %d times", n)
	}

	if _, err := q.Enqueue(ctx, 1, "u", "d"); err != nil {
		t.Fatalf("enqueue: %v", err)
	}
	d.dispatchOnce(ctx)
	if prov.candidateCalls.Load() == 0 {
		t.Fatal("non-empty queue must compute capacity from the gate directory")
	}
	if n, err := q.QueueLen(ctx, 1); err != nil || n != 0 {
		t.Fatalf("queue should be drained after one pass: len=%d err=%v", n, err)
	}
}
```

T8 参考实现（新文件 `go/login/internal/svc/login_queue_wiring_test.go`；`newSvcTestRedis` 在同包 `candidates_client_endpoint_test.go:164`；改写 `config.AppConfig` 再在 Cleanup 里还原，是同包 `auth_init_test.go:37-40` 的既有写法）：

```go
package svc

import (
	"testing"
	"time"

	"login/internal/config"
	"login/internal/logic/pkg/loginqueue"
)

// 本文件盯住 G1b 的接线:出队 leader 锁按本 login 的 Node.ZoneId 拆,
// 锁键 = Queue.DispatcherLockKey + ":zone:" + ZoneId;开了排队却给不出合法 zone 时起服即失败。

func loginQueueTestConf(lockKeyBase string) config.QueueConf {
	return config.QueueConf{
		Enabled:           true,
		DispatchInterval:  30 * time.Millisecond,
		AdmitTTL:          60 * time.Second,
		QueueEntryTTL:     time.Hour,
		SoftCapMultiplier: 1.5,
		DispatcherLockTTL: 2 * time.Second,
		DispatcherLockKey: lockKeyBase,
	}
}

// GateWatcher 留 nil:队列为空时 dispatcher 不碰 gate 目录(见 loginqueue 的 T9),
// 这一轮选主 + 空跑不会走到 CandidatesForZone。
func TestInitLoginQueue_DispatcherLocksOwnZone(t *testing.T) {
	previous := config.AppConfig
	t.Cleanup(func() { config.AppConfig = previous })
	rdb, mr := newSvcTestRedis(t)

	const base = "dispatcher:lock:login_queue"
	config.AppConfig = config.Config{}
	config.AppConfig.Node.ZoneId = 7
	config.AppConfig.Queue = loginQueueTestConf(base)

	s := &ServiceContext{RedisClient: rdb}
	s.initLoginQueue()
	if s.QueueDispatcher == nil {
		t.Fatal("Queue.Enabled=true must build a dispatcher")
	}
	s.QueueDispatcher.Start()
	defer s.QueueDispatcher.Stop()

	want := loginqueue.ZoneDispatcherLockKey(base, 7)
	deadline := time.Now().Add(3 * time.Second)
	for !mr.Exists(want) {
		if time.Now().After(deadline) {
			t.Fatalf("zone 7 leader lock %q was never acquired", want)
		}
		time.Sleep(10 * time.Millisecond)
	}
	if mr.Exists(base) {
		t.Fatalf("legacy global lock key %q must not be written any more", base)
	}
}

func TestInitLoginQueue_ZeroZonePanicsWhenQueueEnabled(t *testing.T) {
	previous := config.AppConfig
	t.Cleanup(func() { config.AppConfig = previous })
	rdb, _ := newSvcTestRedis(t)

	config.AppConfig = config.Config{} // Node.ZoneId = 0
	config.AppConfig.Queue = loginQueueTestConf("dispatcher:lock:login_queue")

	defer func() {
		if recover() == nil {
			t.Fatal("Queue.Enabled=true with Node.ZoneId=0 must panic at startup (queue would only grow)")
		}
	}()
	(&ServiceContext{RedisClient: rdb}).initLoginQueue()
}

// 默认关:不构造队列和 dispatcher,也就不做任何新校验,与修复前一致。
func TestInitLoginQueue_DisabledSkipsDispatcherValidation(t *testing.T) {
	previous := config.AppConfig
	t.Cleanup(func() { config.AppConfig = previous })
	rdb, _ := newSvcTestRedis(t)

	config.AppConfig = config.Config{}
	config.AppConfig.Queue = loginQueueTestConf("")
	config.AppConfig.Queue.Enabled = false

	s := &ServiceContext{RedisClient: rdb}
	s.initLoginQueue()
	if s.LoginQueue != nil || s.QueueDispatcher != nil {
		t.Fatal("Queue.Enabled=false must build neither the queue nor the dispatcher")
	}
}
```

T8a 的清理顺序：函数内的 `defer Stop()` 先于 `t.Cleanup` 执行，dispatcher 先停，miniredis 后关（`miniredis.RunT` 把关闭登记在 Cleanup 里）。T8 改写的是包级 `config.AppConfig`，所以这几个用例和同包其它用例都不能加 `t.Parallel()`（现状也没有并行用例）。

### 5.4 红用例（只在 HEAD 旧 API 上跑，不入库）

新 API 下的 T3 在 HEAD 上编译不过（签名改了），所以红证据用一份按**旧签名**写的复现用例，在 HEAD 的临时 worktree 里跑（§11.4「确实无法先写时说明原因并给替代验证」）。文件名 `g1b_repro_test.go`：

```go
package loginqueue

import (
	"context"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
)

// G1b 红用例(只在 HEAD 旧 API 上跑):两个 zone 共用 Redis、配同一个默认锁键,各自的 dispatcher
// 只看得见本 zone(生产 activeZones 的实际行为)。期望两个 zone 都出队;旧代码只有抢到锁的 zone 出队 → 超时 FAIL。
func TestG1bRepro_SharedRedisTwoZones_BothQueuesDrain(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("miniredis: %v", err)
	}
	defer mr.Close()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer rdb.Close()
	secret := []byte("g1b-repro")
	q := New(rdb, time.Hour, 60*time.Second, secret)
	ctx := context.Background()

	newZone := func(zone uint32) *Dispatcher {
		prov := &integrationProvider{
			caps:  map[uint32]uint32{zone: 100},
			cands: map[uint32][]GateCandidate{zone: {{NodeID: zone, IP: "10.0.0.1", Port: 9000, ZoneID: zone}}},
		}
		return NewDispatcher(q, prov, rdb, secret, 300*time.Second, 30*time.Millisecond, 1.5, 2*time.Second,
			"dispatcher:lock:login_queue",
			func(context.Context) []uint32 { return []uint32{zone} })
	}
	d1, d2 := newZone(1), newZone(2)
	d1.Start()
	defer d1.Stop()
	d2.Start()
	defer d2.Stop()

	for range 5 {
		if _, err := q.Enqueue(ctx, 1, "a", "d"); err != nil {
			t.Fatal(err)
		}
		if _, err := q.Enqueue(ctx, 2, "b", "d"); err != nil {
			t.Fatal(err)
		}
	}
	waitFor(t, 3*time.Second, "zone 1 and zone 2 queues both drained", func() bool {
		n1, _ := q.QueueLen(ctx, 1)
		n2, _ := q.QueueLen(ctx, 2)
		return n1 == 0 && n2 == 0
	})
}
```

（`integrationProvider` 和 `waitFor` 在 HEAD 的 `dispatcher_test.go:17-35`、`:89-99` 里已有。）

---

## 6. 文档改动清单（`docs/design/login-queue-2026-05.md`、runbook）

1. 设计稿头部（:3-6）加一行：`> **修订**: 2026-10-12 G1b — 出队 leader 锁改为每 zone 一把(§7.1)`。
2. §3 拓扑（:63-67）：「Dispatcher goroutine(单 leader,1s 一拍): for zone:」改为「Dispatcher goroutine(**每 zone 一个 leader**,1s 一拍):只出本 zone(Node.ZoneId)」。:75 改为 `dispatcher:lock:login_queue:zone:{zoneId}  STRING uuid, TTL=30s(本 zone leader 锁)`。
3. §5 Dispatcher Loop（:167-180）：`for each zoneId in activeZones():` 改为 `zoneId = Node.ZoneId  // 只出本 zone;队列空直接跳过`，并注明先 ZCARD 再 FreeSlots。
4. §7（:200-217）：示例代码里的锁键改为 `ZoneDispatcherLockKey(cfg.DispatcherLockKey, zoneID)`，「非 leader 副本」改为「同 zone 的非 leader 副本」。新增 **§7.1 为什么按 zone 分锁（G1b，2026-10-12）**，内容取本规格 §1（成因和不选全局 leader 的理由）、§4.5（并存窗口与有界超发）、§4.6（zone-down / 合服残留）。
5. §10（:261-262）：注释改为 `# 锁键前缀;实际 = <前缀>:zone:<Node.ZoneId>,每 zone 一个 leader`。
6. §11 指标表（:301）：标签改为 `pod, zone`。
7. §14 第 5 条（:335）「多 zone 公平性」改为：「~~多 zone 公平性~~ 已由 §7.1 消解：每个 zone 有独立的 leader，不存在 zone 之间互相抢批次。」§15（:343-350）的「清空堆积」第一行本来就带 zone（`queue:zone:1 admitted:zone:1`），保留；但后三行 `--scan --pattern 'admit:*'` / `'queue:meta:*'` / `'queue:token:*'` 不分 zone，在多 zone 共用 Redis 时会把**所有 zone** 的放行槽和排队条目一起清掉。要在这三行上方加一句警告：「多 zone 共用 Redis 时这三行会清掉全部 zone；只想清一个 zone 时只执行第一行，其余键按 TTL（60s / 1h）自然过期。」然后追加：
   ```
   # leader 锁一般不用删(30s 自然过期);确需强制换主:
   redis-cli DEL dispatcher:lock:login_queue:zone:1
   # 合服后源 zone S 的队列残留会在 QueueEntryTTL(1h)内自行过期;要立即清:
   redis-cli DEL queue:zone:<S> admitted:zone:<S> dispatcher:lock:login_queue:zone:<S>
   ```
8. `docs/ops/login-queue-stress-runbook.md`：
   - :68-69 中，日志改为 `[loginqueue] elected dispatcher leader zone=<id>`；「无 leader 日志 = `dispatcher:lock:login_queue:zone:<id>` 抢锁失败」。再补一句：多 zone 共用 Redis 时每个 zone 各有一把锁，某个 zone 没有锁键 = 该 zone 没有打开 Queue 的 login 副本。
   - :70 后补一条：有 leader、日志里持续出现 `[loginqueue] freeSlots zone=<id> queueLen=<n> err=no gates for zone` = 有人在排，但本 zone 一台可用 gate 都没有（G1b 之前这种情况是静默跳过的）。
   - :82-84「模式 3」的症状改为「**同一 zone** 的两台 login 都打印 `elected dispatcher leader zone=<同一个 id>`」。G1b 之后不同 zone 各打印一次是正常的，照旧文案会被误判成双 leader。
   - :88 改为 `redis-cli GET dispatcher:lock:login_queue:zone:1`，应该只有一个 owner UUID。新旧版本混跑窗口里 zone 键和全服旧键可能同时存在，属于预期（设计稿 §7.1）。另外补一个排查点：同一 zone 的副本配了不同的 `DispatcherLockKey` 也会长期双 leader，`redis-cli --scan --pattern "*:zone:1"` 能看到两把锁。
   - :150 「kill -9 当前 leader login pod」改为「kill -9 **该 zone** 当前的 leader login pod」，预期只有这个 zone 的放行暂停（≤ TTL），其它 zone 不受影响。
   - :176 标签改为 `pod, zone`；不变量改为 `sum by (zone) (...) == 1`。
   - :182 告警改为 `sum by (zone) (login_queue_dispatcher_is_leader) != 1`。
   - :196 改为「`login_queue_dispatcher_is_leader` 按 zone、pod 的 stat panel」。
   - :207「已知不足」第 2 条（多 zone 公平性、`activeZones` 顺序遍历）改为划线并注明「2026-10-12 G1b 起每 zone 一个 leader，不存在 zone 之间抢批次」，与设计稿 §14 第 5 条同批改。
   - 「故障应急 / 清空堆积」（:216-222）的 `KEYS admit:* | xargs DEL` 等三行：`KEYS` 会阻塞全服共用的那个 Redis 主（launch_verdict:22），而且同样不分 zone。改成与设计稿 §15 一致的 `redis-cli --scan --pattern … | xargs -r redis-cli DEL`，并加上第 7 条那句多 zone 警告。
9. `docs/design/ARCH.md`（只改两行措辞，不碰该文件 :503 别人未提交的改动）：
   - :419 索引行「AssignGate 真排队(Redis ZSET + dispatcher leader)」改为「…(Redis ZSET + 每 zone 一个 dispatcher leader)」。
   - :496 决策 #14 的「单 leader dispatcher」后补「(2026-10-12 G1b 起改为每 zone 一个 leader,见 login-queue-2026-05.md §7.1)」。决策行的日期和其余内容不动。

`docs/PROGRESS.md`、`docs/design/tidb-phase1-and-launch-readiness.md` 中 L-01 的状态由 Docs 阶段统一更新，本阶段不改。没有新建文档，`docs/README.md` 不用登记。

---

## 7. 交给 Codex 的验证

前置：不需要清 Redis / Kafka / MySQL，也不需要起任何服务。全部是 Go 单测，Windows PowerShell。C++、Java、proto 都没改，不需要 MSBuild / mvn / build.bat。

### 7.1 红（推荐，用于证明复现）

```powershell
git -C D:\luyuan\xuanming-server-mmo-cpp worktree add --detach D:\luyuan\g1b-red HEAD
# 把本规格 §5.4 的代码写成 D:\luyuan\g1b-red\go\login\internal\logic\pkg\loginqueue\g1b_repro_test.go
cd D:\luyuan\g1b-red\go\login
go test ./internal/logic/pkg/loginqueue/ -run TestG1bRepro -count=1 -v
cd D:\luyuan\xuanming-server-mmo-cpp
git worktree remove --force D:\luyuan\g1b-red
```

**通过标准**：`TestG1bRepro_SharedRedisTwoZones_BothQueuesDrain` **FAIL**，信息为 `timeout waiting for: zone 1 and zone 2 queues both drained`。如果 PASS，说明复现前提不成立，停下来报告，不继续下一步。保留 `-v` 输出中 `elected dispatcher leader` 那几行。

### 7.2 绿（工作区，修复之后）

工作目录 `D:\luyuan\xuanming-server-mmo-cpp\go\login`：

```powershell
gofmt -l internal\logic\pkg\loginqueue internal\svc internal\config
go vet ./internal/logic/pkg/loginqueue/... ./internal/svc/... ./internal/config/...
go build ./...
go test ./internal/logic/pkg/loginqueue/... -count=1 -v -run "TestZoneDispatcherLockKey|TestNewDispatcher_|TestIntegration_|TestDispatchOnce_"
go test ./internal/svc/... -count=1 -v -run "TestInitLoginQueue_"
go test ./internal/logic/pkg/loginqueue/... -count=10 -run "TestIntegration_TwoZonesSharedRedis|TestIntegration_DualLeadersSameZone|TestIntegration_LeaderElectionSerializesDispatch"
go test ./internal/svc/... -count=10 -run "TestInitLoginQueue_"
go test ./internal/logic/pkg/loginqueue/... ./internal/svc/... ./internal/config/... -count=1
go test ./... -count=1
```

**通过标准**：
- `gofmt -l` 没有输出。若有输出，允许对列出的文件执行 `gofmt -w` 后重跑，并在报告中注明。
- vet 和 build 都没有错误。
- T1–T9（含 T8a–T8c）全部 PASS，跑 10 遍稳定，没有偶发失败。`TestInitLoginQueue_` 的 `-v` 输出里不能有 `login.queue_dispatcher` 的 panic 记录（safego 会接住 panic，用例本身不会因此失败）。
- `go test ./...`：本次改动涉及的包必须全绿。其它包如果失败，先在 HEAD 上跑同一条命令做基线对比：HEAD 上同样失败的记为既有问题、不修；只在修复后才失败的就停下来报告。
- 本机有 cgo 工具链时，可以对 loginqueue 加跑一次 `-race`；没有就省略，并在报告中写明。

失败时保留：失败用例的完整 `-v` 输出，以及 `go vet` / `go build` 的报错原文。

### 7.3 可选：本地双 zone 冒烟（由负责人决定是否跑，不属于压测）

前提是 `dev-start-zones` 起两个 zone，两个 zone 的 login 都把 `Queue.Enabled` 改成 true，并配上较小的 `ZoneCapacityOverride`（**不改 `go/login/etc/login.yaml`**），robot 两个 zone 各跑一批。

注意：派生 yaml（`run/etc/go_services/z<Zone>_login.yaml`）每次经 `go_services.ps1` 起服都会从基础 yaml 重新生成（`go_services.ps1:435` 的 `Set-Content`），所以「先改派生 yaml，再用脚本重启 login」是行不通的，改动会被覆盖。可执行的做法：
1. `dev-start-zones` 起好两个 zone；
2. `pwsh tools/scripts/dev_tools.ps1 -Command go-svc-stop -GoServices login` 停掉 login（如果只停掉了一个 zone 的实例，就按 go_services 的 pid 文件结束剩下的 `z1_login*` / `z2_login*` 进程）；
3. 把两个派生 yaml 复制到 scratchpad，只在副本里改 `Queue` 段；
4. 在 `go/login` 下给每个 zone 起一个进程：`go run login.go -loginService <副本绝对路径>`（login 的配置参数名是 `-loginService`，见 `login.go:40`）。

冒烟结束后用 `dev-stop` 收尾，副本留在 scratchpad 里，不进仓库。

验收：`redis-cli --scan --pattern "dispatcher:lock:login_queue*"` 只列出 `…:zone:1` 和 `…:zone:2`，没有不带后缀的旧键；两个 zone 的 `login_queue_admit_total{zone="1"}`、`{zone="2"}` 都在增长；每个 zone 恰好一个 pod 的 `login_queue_dispatcher_is_leader` 为 1。

---

## 8. 与现有设计的偏离与代价

| 偏离 | 原设计 | 新设计 | 代价 / 理由 |
|---|---|---|---|
| 选主范围 | 全服一个 leader（设计稿 §3、§7，`login-queue-2026-05.md:63-75`、`:200-217`） | 每 zone 一个 leader | 每个副本的成本不变，仍是一个选主 goroutine。Redis 侧多出 zone 数把锁，每把每 10s 一次续期和若干次 TryLock，可以忽略。好处是消除了跨 zone 耦合（launch_verdict:175） |
| 配置语义 | `DispatcherLockKey` 是锁键本身 | 变成前缀 | 键名和默认值不变，旧配置不用改。如果有人手工给各 zone 配了不同的键，新键是 `<手配键>:zone:<id>`，仍然按 zone 唯一，与 §4.5 的窗口分析相同 |
| 构造函数 | `NewDispatcher(…, lockKey, activeZones func)` | `NewDispatcher(…, lockKeyBase, zoneID) (*Dispatcher, error)` | 包内 API，调用方只有 servicecontext 和测试。把「锁范围 = 出队范围」做成结构上的约束，不再依赖调用方自觉保持一致（§11.1 信息隐藏） |
| 出队前判空 | 先 FreeSlots 后 QueueLen | 先 QueueLen 后 FreeSlots | 空闲 zone 少一次 etcd 快照读和一次 SCARD；本 zone 没有 gate 但有人排队时会出现 ERROR 日志（以前被静默跳过） |
| 指标标签 | `dispatcher_is_leader{pod}` | `{pod, zone}` | 序列身份变了，看板和告警要跟着改（runbook 一起改）。zone 是低基数标签 |
| 与 Java 版的差异 | 两版都是全局 leader | mmorpg 每 zone 一个，Java 仍是全局一个 | 服务端内部实现，不涉及客户端契约（AGENTS §12 允许各自按惯用方式实现）。Java 的全局 leader 能看到所有 zone 的 gate，没有这个缺陷，见 §10 |

---

## 9. 风险与待负责人拍板

**风险**

1. 新旧版本混跑窗口里，同一个 zone 出现双 leader，会超发至多一批（≤ 每个额外 leader 一个容量窗口），不丢人也不重复放行。缓解办法是在 Enabled=false 的状态下升级（现状就是关的）。
2. 有 zone 在排队、但本 zone 一台 gate 都没有时，每秒一条 ERROR。目前不加节流；如果嫌吵，后续可以复用 `skippedGateLogEvery`（`servicecontext.go:388`）那种节流写法。
3. 多集群共用一个 Redis 且 zone 号重复时，`queue:zone:*` 本来就会撞。本修复不改变这一点，也不解决它。
4. 看板和告警没跟着改的话，`sum(...) != 1` 会在多 zone 下持续误报。runbook 必须同批改（已列入 §6 第 8 条）。

**待拍板**

1. **方案**：按 zone 拆锁（推荐） vs 全局 leader + activeZones 覆盖所有 zone（不推荐，理由见 §1 末尾）。
2. **指标是否加 `zone` 标签**：推荐加。不加的话，「每个 zone 恰好一个 leader」这个不变量在 PromQL 里写不出来，只能依赖 K8s 抓取时附带的 namespace 标签，而本地和裸机部署没有这个标签。
3. **可选加固（单独的小改动，不并入 G1b）**：在 AssignGate 的排队路径上拒绝 `in.ZoneId != Node.ZoneId` 的请求，以堵住 §4.7 的 `queue:zone:0` 黑洞，把「每个被写入的队列都有 leader 来出」这条不变量钉死在入口处。正常路径上网关已经保证 zone 一致，所以对正常流量无影响。
4. **上线顺序**：本修复在 `Queue.Enabled=false` 下全量滚完之后才允许执行 L-02（打开排队）。K8s 的 login ConfigMap 届时要补 `Queue` 段（含 `ZoneCapacityOverride`），属于 L-02 的范围。
5. **Java 版是否也改成按 zone 选主**：不需要。是否为了消除「全局 leader 挂掉，全服排队停顿约 30s」而对齐，由 Java 负责人决定。
6. 合服 runbook 是否加入「清理源 zone 队列键」这一步：推荐不加，因为残留会在 1h 内自行过期；手动清理命令放在设计稿 §15。
7. **L-02 打开排队前的两条基线缺陷**（§4.7 第 2、3 条：快速通道占位只靠整集合 TTL 回收、放行条目一被取走就撤占位）：推荐登记成 L-02 的前置项，单独开任务修（方向可参考 Java 版：占位改成按成员到期的 ZSET、取走后再留一小段时间），**不并入 G1b**。原因：两条都改的是容量记账，与选主范围无关，混在一起会让 G1b 的回归面变大、回滚变难。不修的话，打开排队后首日会出现「周期性停摆最长 60s」和「取走到上线之间多放一批」，这两种现象很容易被误判成 G1b 没修好，所以至少要在 L-02 的压测验收里把它们单独列出来观测。

---

## 10. Java 版（AGENTS §12）状态判断

- **不受 G1b 影响，无需修改代码。** Java 的排队放行在 game-gateway 里，用一把全局选主锁 `<ns>:login-queue-dispatcher`（`D:\luyuan\xuanming-server-mmo\game-discovery\src\main\java\com\game\discovery\RedisKeys.java:382-385`，`game-gateway\...\queue\QueueDispatcherRunner.java:44`）。但 leader 会遍历区服目录里**所有** OPEN 的 zone（`game-gateway\...\queue\QueueDispatcher.java:94-105`），每个 zone 的 gate 从共享的 Redis gate 目录读取（`QueueDispatcher.java:113` `gates.listGates(zoneId)`）。也就是说，Java 的出队范围本来就覆盖全服，与它的锁范围一致，不存在「锁全服、出队只出本 zone」的错位。
- 客户端契约（assign-gate 100 / queue-status、queue token、tip）不变，Java 不需要同批修改。
- `PARITY.md` 第 98 行「登录排队」那一行已经是「已对齐（行为有意不同）」，Docs 阶段在这一行补一句：「mmorpg 2026-10-12（G1b）起每 zone 一个出队 leader；Java 全局一个 leader 遍历全部区，不受影响。」本仓库 PROGRESS 条目注明 Java 状态为「不适用（Java 不受影响）」。

---

## 评审处理记录（对抗评审，2026-10-12）

评审方法：逐条回到代码核对（`dispatcher.go`、`metrics.go`、`queue.go`、`capacity.go`、`player_locker.go`、`servicecontext.go`、`config.go`、`login.yaml`、`dispatcher_test.go`、svc 包既有测试、`login.go`、`go_services.ps1`、`k8s_deploy.ps1`、mmorpg 网关 `LoginRpcClient.java`、设计稿与 runbook、Java 版 `QueueDispatcher.java` / `RedisKeys.java` / `PARITY.md`）。只读，没有改仓库文件，没有运行任何编译或测试。

### 核对后成立、无需修改的部分

- F1–F14 的行号与事实逐条对上（`dispatcher.go:115/159/166/177-193/226-285`、`servicecontext.go:151-162/348/449-489`、`config.go:252-257`、`login.yaml:428-436`、`queue.go:172-173/265/396/415/428-432/456-491`、`capacity.go:62-71/89-100`、`metrics.go:116-129`、runbook :69/:88/:176/:182/:196、K8s login ConfigMap 无 `Queue` 段）。
- 编译面：`dispatcher.go` 新增 `errors`、`fmt` 都有用处，`context`、`sync` 仍在用；`servicecontext.go` 删掉 `activeZones` 后 `context`（:99 等）、`os`（:487）仍在用；`dispatcher_test.go` 删掉闭包后 `context` 仍被 `integrationProvider` 用到；`min(free, queueLen)` 两边都是 `uint32`；`free, …, err :=` 左边有新变量，合法；`Node.ZoneId` 是 `uint32`（`config.go:406`）；`dispatcherIsLeaderGauge` 全仓只有 `dispatcher.go` 的 4 处调用，统一换成 `setLeaderGauge` 后不会出现标签个数不匹配；仓库里没有引用该指标的看板 / 告警 / 脚本（tools、deploy、robot 均无匹配）。
- gofmt 判断：独占一行的注释会切断结构体字段的对齐段（与标准库 `runtime2.go` 中 `g` 结构体的实际排版一致），所以 §3.5 写的 `zoneID  uint32` / `lockKey string` / `mu      sync.Mutex` 是对的。
- 有界超发推导（§4.5 第 2 条）：每个 leader 读到的 SCARD 至少包含自己此前的 SADD，所以每个 leader 自己的在途数 ≤ cap − online，两个 leader 合计 ≤ 2×(cap − online)。有消费时也成立，T4 的 `≤ 2×cap` 不会偶发失败。
- 红用例 §5.4 在 HEAD 上必然 FAIL：两个 dispatcher 争同一把键，miniredis 不按墙钟过期，输家永远拿不到锁；`go/login` 依赖的 `../shared`、`../proto` 生成代码都在 git 里，临时 worktree 能编。
- Java 结论（§10）属实：Java 的全局 leader 遍历全部 OPEN 区，gate 来自共享 Redis 目录（`QueueDispatcher.java:94-113`），锁范围与出队范围一致。
- 不选「全局 leader + activeZones 覆盖全部 zone」的理由成立：mmorpg 的 gate 视图是按 zone 前缀的 etcd watch（`servicecontext.go:153-157`），容量覆盖值按 zone 分开下发，与 Java 的共享目录不同。

### 必须改（已吸收）

| # | 问题 | 证据 | 处理 |
|---|---|---|---|
| M1 | runbook「模式 3 双 leader」的症状写的是「两台 login 都打印 elected dispatcher leader」。G1b 之后不同 zone 各打印一次是常态，照旧文案排障会把正常状态误判成双 leader；runbook :207 的「多 zone 公平性 / activeZones 顺序遍历」也会变成错误描述。原规格 §6 第 8 条漏了这两处 | `login-queue-stress-runbook.md:82-84`、`:207` | §6 第 8 条补上 :84（改为「同一 zone」）、:207（划线注明已消解），顺带补 :70（新出现的 `freeSlots … queueLen=… no gates` ERROR 的含义）和 :150（kill 的是「该 zone 的」leader） |

### 重要（已吸收）

| # | 问题 | 证据 | 处理 |
|---|---|---|---|
| I1 | 「先判空再算容量」是新行为，规格把它列为偏离（§8），却没有测试钉住；以后有人把顺序改回去，空闲 zone 每拍取 etcd + 查排空标记、无 gate 的空闲 zone 每秒刷 ERROR，这些都不会被发现 | `dispatcher.go:237-248` 现状顺序；`watcher.go:27-33` 每次都是一次 etcd Get | 新增 T9 `TestDispatchOnce_EmptyQueueSkipsGateDirectory`：不 Start，直接调 `dispatchOnce`，结果确定；给出参考实现（§5.3） |
| I2 | 缺陷出在接线层（锁键来自配置、出队范围来自 GateWatcher），T1–T7 只测 loginqueue 包，没有测试钉住「zoneID 取自 `Node.ZoneId`」「`Enabled=true` 且 ZoneId=0 时 panic」「`Enabled=false` 时不做任何新校验」这三条新契约 | `servicecontext.go:421-490`；svc 包已有 `newSvcTestRedis`（`candidates_client_endpoint_test.go:164`）和改写 `config.AppConfig` 的先例（`auth_init_test.go:37-40`） | 新增 `go/login/internal/svc/login_queue_wiring_test.go`，T8a–T8c，给出参考实现；§2 文件清单、§7.2 命令和通过标准同步更新 |
| I3 | T4 没有证明自己真的跑在双 leader 上：如果某个 dispatcher 没抢到锁，`≤2×cap` 和「不丢不重」在单 leader 下同样成立，用例会空过 | T4 原骨架 | T4 在饱和后断言两把 zone 锁 `EXISTS == 2`（§5.2 表、§5.3 骨架） |
| I4 | `ARCH.md` 的索引行和决策 #14 写的是「单 leader dispatcher」，修复后与实现矛盾；原规格的文档清单没有它 | `docs/design/ARCH.md:419`、`:496`；该文件的未提交改动只在 :503 | §2 文件清单和 §6 第 9 条新增：只改这两行措辞，不碰 :503 |
| I5 | 打开排队（L-02）时会遇到两条与 G1b 正交的基线缺陷，规格没有登记：① 快速通道的 `fast:<uuid>` 占位从不单独 SREM，只靠整集合 TTL 回收，而每次 SADD 都会刷新 TTL，导致已上线玩家被重复扣减，zone 出现「放一批 → 停摆最长 60s」的循环；② 放行条目一被取走就撤占位，取走到上线之间会多放一批。开服首日这两种现象很容易被误判成 G1b 没修好 | `queue.go:355`（唯一的 SREM，只删 queueId）、`:430-431`、`:456-482`；`capacity.go:89-100`；Java `PARITY.md:98` 有意差异 ②③ 记的就是这两条 | §4.7 改为三条登记（原 zone 0 黑洞为第 1 条）；§9 待拍板新增第 7 条：作为 L-02 的前置项另开任务，**不并入 G1b**，至少要在 L-02 的压测验收里单独观测 |
| I6 | §7.3 本地冒烟照写不可执行：派生 yaml 每次起服都会被 `go_services.ps1` 重写，「改派生 yaml + 用脚本重启」会被覆盖 | `go_services.ps1:366`、`:435`（`Set-Content`）；login 的配置参数是 `-loginService`（`login.go:40`） | §7.3 改为四步：起 zone → 停 login → 在 scratchpad 副本里改 `Queue` → `go run login.go -loginService <副本>` |
| I7 | 应急清理有跨 zone 的副作用：设计稿 §15 的三条 `--scan admit:* / queue:meta:* / queue:token:*` 不分 zone；runbook 的「清空堆积」更是用 `KEYS`，会阻塞全服共用的那个 Redis 主。多 zone 共用 Redis 正是本修复的前提 | 设计稿 :343-348；runbook :216-222；launch_verdict:22 | §6 第 7 条加多 zone 警告；§6 第 8 条把 runbook 的 `KEYS` 改成 `--scan`，并加同一句警告 |

### 小问题（已吸收）

| # | 问题 | 处理 |
|---|---|---|
| S1 | §4.7 说「网关对 zone_id=0 直接回 404」，这是 Java 版的行为。mmorpg 网关是在 `LoginRpcClient.unaryCall` 里对 `zoneId <= 0` 抛 `IllegalArgumentException`，根本不调 login（`LoginRpcClient.java:340-343`） | 改成两版各自的准确行为，并注明集群内直连 login gRPC 的调用方不受网关约束 |
| S2 | §4.3 说空队列「一次 gauge 快照和一次 ZCARD」，实际是快照里的 ZCARD + SCARD，再加一次 ZCARD | 改正措辞，注明由 T9 钉住 |
| S3 | §4.5 第 1 条「不丢人」没有限定条件。`PopAdmit` 有几条既有的丢条目路径（`(nil, nil)`、Marshal 失败、写 admit 的事务失败），单 leader 下同样存在 | 限定为「双 leader 不比单 leader 多丢」，列出既有路径和行号 |
| S4 | 同一 zone 的副本配了不同的 `DispatcherLockKey`，就是长期双 leader（修复前后都是这样），配置注释没说 | §3.4、§3.6 的注释和 §4.2 补「同 zone 必须同值」；runbook 的 :88 补对应的排查命令 |
| S5 | 锁和队列必须在同一个 Redis 实例上。launch_verdict:173 建议按用途拆 Redis，拆的时候如果把两者拆开，§4.5 的分析就失效 | §4.1 补一段前提说明 |
| S6 | §4.6「网关在 S 没有 login 时对 queue-status 返回什么」可以核实：按 zone 选通道失败会抛错（`LoginRpcClient.java:328-331`、`:376-386`） | 改为已核实 |
| S7 | §4.4 的 SIGTERM 能否走到 `Stop()`：login 的注释（`login.go:294-298`）与 scene_manager 的注释（`scene_manager_service.go:123-126`）说法冲突 | 保留「未核实」，补上双方的证据。注明最坏情况是该 zone 约 30s 没有 leader，与修复前相同，不是回归；照 scene_manager 挂 `proc.AddShutdownListener` 可以另开小改动 |
| S8 | 新旧标签集并存窗口里，`sum by (zone)` 会出现一个 `zone=""` 组 | §4.5 第 3 条注明：按推荐在 Enabled=false 下滚动时没有任何序列，不存在这个窗口；偏要在 Enabled=true 下滚动时，该组告警属于预期 |

### 未采纳（附理由）

| # | 建议 | 不采纳的理由 |
|---|---|---|
| N1 | `ZoneDispatcherLockKey` 只在包内使用，按 YAGNI 应改为不导出 | 它是对运维公开的锁键契约（配置注释、runbook、设计稿都引用这个名字），而且新增的 T8a 在 svc 包里调用了它，已经有包外调用方 |
| N2 | 让 `publishGaugeSnapshot` 返回队列长度，省掉第二次 ZCARD | 每 zone 每秒只省一次 O(1) 的 ZCARD，却要改一个跨文件的函数签名。没有数据支撑的微优化，不扩大改动面（AGENTS §11.3） |
| N3 | 在 G1b 里顺手挂 `proc.AddShutdownListener(QueueDispatcher.Stop)`，消除滚动升级时约 30s 的无 leader 空窗 | 既有行为，与选主范围无关；而且 login 现有的停机路径本身说法冲突（S7），需要单独核实后再改。已登记在 §4.4 |
| N4 | 把 §4.7 第 2、3 条（容量记账的两条基线缺陷）一起修进 G1b | 改的是容量记账，回归面和回滚粒度都与 G1b 不同。登记为 L-02 的前置项（§9 第 7 条） |

### 仍需 Codex 验证的事实（Claude 无法静态确认）

- go-zero `metric.GaugeVec.Set(v, labels...)` 是变参签名，miniredis v2.35 有 `Exists(k) bool`，并且 TTL 不随墙钟自动递减。仓库的现有用法与之一致（`match/internal/team/store_test.go:116`、`:234`），本机没有模块缓存，没能读到源码。如果编译或测试与此不符，按 §7.2 的失败流程保留输出并报告。
- T8a 的 `GateWatcher` 传 nil，是因为空队列时不会调用 `CandidatesForZone`。但 T8a **测不出**顺序写反：写反时 nil watcher 会在 dispatcher 的 goroutine 里 panic，`safego.Go` 会把它接住（`shared/safego/safego.go:102-107`），而锁在 panic 之前就已经拿到，所以 T8a 照样通过，只在日志里留一条 panic 记录。顺序这件事由 T9 负责。Codex 跑 T8 时，如果 `-v` 输出里有 `login.queue_dispatcher` 的 panic 记录，也要按失败上报。
