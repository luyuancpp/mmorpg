# Login Queue (AssignGate Real Queue) — 2026-05

> **决策**: ARCH.md §11 #14
> **代码**: `go/login/internal/logic/pkg/loginqueue/`, `java/gateway_node/.../service/AssignGateService.java`
> **运维**: [docs/ops/login-queue-stress-runbook.md](../ops/login-queue-stress-runbook.md)
> **关联**: 2026-05-08 [open-server-rate-limit-design.md](./open-server-rate-limit-design.md)(Bucket4j 限流);本文是其**互补层**,不是替代
> **修订**: 2026-10-12 G1b — 出队 leader 锁改为每 zone 一把(§7.1)

---

## 1. 为什么再做一层"真排队"

`open-server-rate-limit-design.md`(2026-05-08)在 Java Gateway 上落地了 Bucket4j 三层限流(zone / IP / account cooldown)+ 开服波次。**它解决的是"挡住请求洪峰"**,但不能告诉玩家"前面还有多少人"。

实战中两个场景是 Bucket4j 解不了的:

1. **开服当日**:5 万玩家在 09:00:00 同时点"进入服务器",Bucket4j 把请求按 token 桶节流到 2000/s,但玩家拿到的回应是"请重试"——他们不知道何时能进、是不是被踢、要不要换服。截图(用户提供的排队 UI)需要展示 `rank=N / total=M` 这种**有序位次**。
2. **配置容量回压**:DBA 判定本服 gate 集群当下只能稳吃 1 万人,但 Bucket4j 不知道"在线玩家数"——它只看请求 RPS。需要一个组件能把 `online + admitted_inflight` 与 `zoneCapacity` 比对,**动态决定**当前请求是放行还是入队。

Bucket4j 是**无序的速率匙**(token bucket),真排队是**有序的容量阀**(FIFO + 容量计算)。两者必须互补,不是二选一。

---

## 2. 决策记录(为什么放在 go-zero login,不是 Java Gateway)

| 维度 | 放 go-zero login(选定) | 放 Java Gateway(否决) |
|---|---|---|
| 出队需要的状态 | EnterGame 进度、PlayerSession、enter_gs_type 全在本进程 | 要新增 4 个 gRPC 才能查 |
| Leader 选举 | 复用 `pkg/locker.RedisLocker` + `Lock.StartHeartbeat` | 要新引选举库 |
| 重连/顶号豁免 | 直接调 `sessionmanager.GetSession`(同进程) | 要再实现一份判定 |
| 跟现有 Bucket4j 关系 | Bucket4j 在前(过滤洪峰),Queue 在后(保位次) | 两套语义同居,容易污染 |
| 未来扩"进场景排队" | 同一套 Redis ZSET 模板复用 | 要在 Go 侧再写一遍 |

详细对比见对话记录 / [plan 文件](#)。本期**只做选服登录排队**,进场景排队等真有"攻城战 / 世界 Boss / 跨服竞技场"玩法时按同一套模板升级,不重复造轮子。

---

## 3. 总体拓扑

```
                   ┌─────────────────────── Java Gateway ──────────────────────┐
                   │                                                            │
Client ── POST ───►│ /api/assign-gate                                           │
                   │   ├─ ① Bucket4j 限流(zone/IP/account)──── 挡洪峰        │
                   │   │      └ 拿不到令牌 → code=100 queueSource="ratelimit"  │
                   │   └─ ② gRPC LoginPreGate.AssignGate ─────────────────────┐│
                   │                                                          ││
                   │ /api/queue-status                                        ││
                   │   └─ gRPC LoginPreGate.QueryQueueStatus ─────────────────┤│
                   └──────────────────────────────────────────────────────────┘│
                                                                                │
                   ┌─────────────────────── go-zero login ────────────────────┐│
                   │                                                          ││
                   │ AssignGate handler:                                      ◄┘
                   │   1. queue_token 非空 → 验签 + Lookup(Redis)            │
                   │   2. account 非空 + 有 PlayerSession → 重连豁免         │
                   │   3. Queue.Enabled=false → 直接签 gate token             │
                   │   4. FreeSlots>0 且队列空 → 直接签 gate token            │
                   │   5. 其它 → Enqueue + 返回 queue_token+rank              │
                   │                                                          │
                   │ QueryQueueStatus handler:                                │
                   │   验签 + Lookup → ADMITTED 或 QUEUEING                   │
                   │                                                          │
                   │ Dispatcher goroutine(每 zone 一个 leader,1s 一拍):       │
                   │   只出本 zone(Node.ZoneId):                              │
                   │     free = capacity - online - admitted_inflight         │
                   │     ZPOPMIN free 个 → 签 gate token → SET admit:{queueId}│
                   └──────────────────────────────────────────────────────────┘

                   Redis (共享):
                   ─ queue:zone:{zoneId}      ZSET, score=入队 ms, member=queueId
                   ─ admit:{queueId}          STRING JSON{ip,port,payload,sig,deadline}, TTL=60s
                   ─ admitted:zone:{zoneId}   SET of queueId, TTL=60s(滑动)
                   ─ queue:meta:{queueId}     HASH {account,device,zoneId,createdAt}, TTL=1h
                   ─ queue:token:{token}      STRING queueId, TTL=1h(token→queueId 反查)
                   ─ dispatcher:lock:login_queue:zone:{zoneId}  STRING uuid, TTL=30s(本 zone leader 锁)
```

---

## 4. 协议(proto/login/login.proto)

新增字段全部加在末尾(7-11 / 1-10),保持 wire-format 向前兼容:

```protobuf
message AssignGateRequest {
  uint32 zone_id     = 1;
  string queue_token = 2;   // 已在排队的客户端用此重入,跳过容量检查
  string account     = 3;   // 用于重连豁免判定
  string device_id   = 4;
}

message AssignGateResponse {
  // 既有字段 1-6 保持不变 (ip / port / token_payload / token_signature / token_deadline / error)
  uint32 status         = 7;   // 0=ADMITTED, 1=QUEUEING, 2=ERROR, 3=EXPIRED
  string queue_token    = 8;   // status=1 时返回
  uint32 queue_rank     = 9;   // 0-based,0 = "下一个就是你"
  uint32 queue_total    = 10;  // 整个 zone 队列总人数
  uint32 retry_after_ms = 11;  // status=1 时建议轮询间隔
}

message QueryQueueStatusRequest  { string queue_token = 1; }
message QueryQueueStatusResponse {
  uint32 status         = 1;   // 0=ADMITTED, 1=QUEUEING, 3=EXPIRED
  // status=0 时 (gate token bundle):
  string ip = 2; uint32 port = 3;
  bytes  token_payload = 4; bytes token_signature = 5; int64 token_deadline = 6;
  // status=1 时:
  uint32 queue_rank = 7; uint32 queue_total = 8; uint32 retry_after_ms = 9;
  string error = 10;
}

service LoginPreGate {
  rpc AssignGate       (AssignGateRequest)       returns (AssignGateResponse);
  rpc QueryQueueStatus (QueryQueueStatusRequest) returns (QueryQueueStatusResponse);
}
```

**Java Gateway HTTP 层**保留双语义 `code=100`,用 `queueSource` 区分来源:

| HTTP code | queueSource | 含义 | 客户端动作 |
|---|---|---|---|
| 0   | —          | 已放行,gate token 已附 | 立刻 TCP 连 gate |
| 100 | "ratelimit" | Bucket4j 限流(无位次) | sleep `retryAfterMs` 重调 `/api/assign-gate` |
| 100 | "login"    | 真排队,有 `queueRank/queueTotal/queueToken` | sleep `retryAfterMs` 调 `/api/queue-status` |
| 410 | —          | queue_token 过期 | 丢 token,重调 `/api/assign-gate` |
| 429 | —          | 限流硬拒(IP 风暴 / 账号冷却) | 退避后重试或显示错误 |

---

## 5. 入队/出队判定

### AssignGate 决策树(top-to-bottom,first match wins)

```
1. queue_token 非空
     ├─ ParseAndVerifyQueueToken 失败 → status=EXPIRED
     ├─ Lookup 命中 admit → status=ADMITTED + gate token(consumeAdmit 一次性消费)
     └─ 未命中 → status=QUEUEING + 当前 rank

2. account 非空
     └─ sessionmanager.GetSession() 返回非空 + GateID 非空
         → 重连/顶号:跳过排队,fast path 签 token
       否则 fall through

3. !Queue.Enabled
     → fast path 签 token(legacy 行为,kill switch)

4. FreeSlots > 0 且 ZCARD queue:zone:{id} == 0
     → fast path 签 token

5. else
     → Enqueue:ZADD queue:zone:{id} + 写 meta + token 反查索引
     → 返回 status=QUEUEING + queue_token + rank
```

### FreeSlots 公式

```
online       = Σ(gate.PlayerCount for gate in GateWatcher.FetchAllNodes() if gate.zoneId == z)
capacity     = Queue.ZoneCapacityOverride[z] || ceil(max(online, 1) * SoftCapMultiplier)
admitted_inflight = SCARD admitted:zone:{z}
free         = max(0, capacity - online - admitted_inflight)
```

**关键安全属性 — admitted_inflight 必须减**:dispatcher 写出 admit 后,该玩家从 ZSET 中 popped(不在 ZCARD)、从 gate.PlayerCount 中也未出现(他还没真连上),如果 FreeSlots 不算这部分,下一拍会再放出一批,**一秒内超发数倍 admit token**,gate 那边玩家 token 验证全失败。

### Dispatcher Loop

```
zoneId = Node.ZoneId  // 只出本 zone;队列空直接跳过
if ZCARD queue:zone:{zoneId} == 0: return   // 先判空,空闲 zone 不碰 gate 目录
free = FreeSlots(zoneId)                    // 再算容量
if free <= 0: return
batch = min(free, ZCARD)
members = ZPOPMIN queue:zone:{zoneId} batch
for each queueId in members:
    admit = PickAndSignGateToken(zoneId)  // 选 gate + 签 HMAC
    SET admit:{queueId} <admit JSON> EX AdmitTTL
    SADD admitted:zone:{zoneId} queueId
    EXPIRE admitted:zone:{zoneId} AdmitTTL  // 滑动 TTL
```

2026-10-12(G1b)以前是 `for each zoneId in activeZones()`,且先 FreeSlots 后 ZCARD。`activeZones` 实际只含本 zone(见 §7.1),所以改成单 zone 不丢功能。改成先判空的代价:队列非空、本 zone 却一台 gate 都没有时,每拍一条 `[loginqueue] freeSlots zone=… queueLen=… err=…` ERROR(以前被静默跳过)。

**signFn 失败回滚**:如果选 gate 或签 token 失败(罕见,gate 全部下线),把 queueId 以 score=0 重新 ZADD,保证下一拍最优先重试,不丢人。见 `queue.go:PopAdmit`。

---

## 6. Queue Token 安全模型

`queue_token` 是服务端签名的 opaque blob:

```
queue_token = base64url( JSON{kind:"queue", q:queueId, z:zoneId, e:expireTs} | "." | hex(HMAC-SHA256) )
```

- **HMAC 用什么 secret**:复用 `GateTokenSecret`。理由:queue token 和 gate token 的 wire-format 完全不同(JSON+HMAC vs proto-marshalled GateTokenPayload),不可能互相 confuse;一个 secret 旋转就同时换两套,运维更简。
- **过期检查**:`ParseAndVerifyQueueToken` 看 `e` 字段,过期立返 EXPIRED,不查 Redis。
- **伪造防御**:HMAC + 1h TTL。即使攻击者拿到队列容量信息,也无法伪造 queueId 跳过排队(签名匙在服务端)。

---

## 7. Dispatcher Leader 选举

复用现有 `pkg/locker.RedisLocker`:

```go
// 锁键 = <Queue.DispatcherLockKey>:zone:<Node.ZoneId>,每个 zone 一把(§7.1)
lock, _ := rl.TryLock(ctx, ZoneDispatcherLockKey(cfg.DispatcherLockKey, zoneID), 30*time.Second)
if lock.IsLocked() {
    stopHeartbeat := lock.StartHeartbeat(10*time.Second, 30*time.Second, onLost)
    drainLoop(ctx)  // 1s 一拍出队
    stopHeartbeat()
}
```

- 同 zone 的非 leader 副本每 10s `TryLock` 重试,被动备份
- Lost lease(Redis 抖动 / 手动 kill)→ heartbeat 触发 `drainCancel`,goroutine 退出
- 新 leader 在 ≤ TTL(30s)内接管。期间 admit 暂停,客户端继续轮询不报错(只是 rank 不动)

**为什么不用 etcd 选举**:RedisLocker 已在项目里跑了几个月没翻车,加 etcd lease 是重复造轮子。RedisLocker 唯一弱点是 Redis 主从切换时短暂双 leader,但本场景下双 leader 的最坏情况是 admit 多发一批(玩家更早进游戏),不是数据损坏。

### 7.1 为什么按 zone 分锁(G1b,2026-10-12)

**缺陷**:锁的范围和出队范围原来由两个独立的输入决定,两者对不上。

- 锁键取配置 `Queue.DispatcherLockKey`,默认 `dispatcher:lock:login_queue`,全服只有一把。
- 出队范围是 `activeZones`。它的注释说取「有 gate 的 zone ∪ 非空队列的 zone」,实现却只遍历 GateWatcher 的节点;而 GateWatcher 的 etcd 前缀带本进程 `Node.ZoneId`,所以结果只有本 zone。
- 就算 leader 去遍历别的 zone 也出不了队:候选 gate 按 zone 过滤,本进程只看得见本 zone 的 gate,`FreeSlots` 对外 zone 报 `no gates for zone`。
- 网关按 zone 把 AssignGate 路由到该 zone 的 login,`queue:zone:Z` 只会由 zone Z 的 login 写入。

多个 zone 共用一个 Redis 时(K8s 下所有 zone 的 login 都连同一个 Redis),只有抢到锁的那个 zone 出队,其它 zone 的 login 每 10s 重试一次、永远不出队。快速通道要求队列为空,所以非 leader zone 只要有一个人入队,之后进来的人全部入队,而每次 ZADD 都刷新 ZSET 的 TTL,队列永不过期——整个 zone 被堵死。

**修法**:`Dispatcher` 只认一个 `zoneID`(本 login 的 `Node.ZoneId`)。出队范围用它,锁键也从它派生:`<DispatcherLockKey>:zone:<zoneID>`(`loginqueue.ZoneDispatcherLockKey`)。每个 zone 一个 leader,两种范围在结构上不会再对不上。配置键名和默认值不变(`DispatcherLockKey` 的语义从「锁键」变为「锁键前缀」);`Queue.Enabled=true` 时 `Node.ZoneId` 为 0 或前缀为空,login 起服即失败(开了排队却出不了队 = 队列只进不出)。客户端契约不变。

**为什么不选「全局 leader + activeZones 覆盖所有 zone」**:要让 leader 出得了别的 zone 的队,它得同时具备 (a) 跨 zone 的 gate 视图(另起一个不带 zone 的 etcd watch,并和 `buildGateCandidates` 的过滤、去重保持一致);(b) 所有 zone 的 `ZoneCapacityOverride`,而这个配置是按 zone 分开下发的;(c) 接受「一台 login 挂了,全服排队停摆约 30s」的跨 zone 耦合。按 zone 拆锁三样都不需要,改动也最小。Java 版的全局 leader 遍历全部 OPEN 区、gate 来自共享 Redis 目录,锁范围与出队范围本来一致,不受此缺陷影响。

**同一 zone 的副本必须配同一个 `DispatcherLockKey`**:配得不一样就是两把互不相干的锁,该 zone 长期双 leader(修复前后都如此)。不同 zone 可以同值,也可以不同。

**滚动升级(新旧锁键并存窗口)**:老版本 leader 持全服旧键、出它所在的 zone Zo;Zo 里已升级的新副本抢 `…:zone:Zo`。两把锁不互斥,Zo 会短暂出现两个 leader;其它 zone 的新副本从升级那一刻起就恢复出队。两个 leader 同时出同一个 zone:

1. **不丢人、不重复放行**:`ZPOPMIN` 原子,每个 queueId 只会被一个 leader 弹出;挑 gate 失败时放回的只是自己弹出的那些。这里的「不丢人」是指双 leader 不比单 leader 多丢——`PopAdmit` 既有的几条丢条目路径(`pickFn` 返回 `(nil, nil)`、`json.Marshal` 或写 admit 的事务失败)单 leader 下同样存在。
2. **超发有界**:每个 leader 都按自己读到的 `SCARD admitted` 算余量,自己上一拍的 SADD 在下一拍读数之前已完成,所以每个 leader 的在途数 ≤ cap − online,两个合计 ≤ 2×(cap − online)。与上文「双 leader 最坏是 admit 多发一批」的判断一致。
3. **消除窗口的办法(推荐)**:当前所有环境的排队都是关的。**在 `Enabled=false` 下把本修复滚完,再打开排队**,根本不会出现并存窗口(此时也不构造 dispatcher,`login_queue_dispatcher_is_leader` 一条序列都没有,新旧标签集不会并存)。偏要在 `Enabled=true` 下滚动时,旧 pod 的序列在 `sum by (zone)` 里归到 `zone=""` 一组,这组触发告警属于预期。老版本不再续期后,全服旧键 30s 内自然过期,不需要人工清理。

由 `TestIntegration_DualLeadersSameZone_NoLossBoundedOverAdmit` 钉住 1、2 两条。

**前提:锁与它保护的队列在同一个 Redis 实例上**。以后按用途拆 Redis 时,leader 锁必须跟着排队键走;拆开后 Redis 抖动会出现「锁还在、队列读不到」或反过来的组合,上面的有界分析就不成立。按 zone 分锁本身与「每 zone 一个 Redis」「全服共享一个 Redis」两种拓扑都兼容。

**zone-down 与合服后旧 zone 的队列**:本修复不产生新的残留。源 zone S 的 login 下线后不再有人 ZADD,`queue:zone:S` 在最后一次入队后 `QueueEntryTTL`(1h)内过期,`admit:*` 与 `admitted:zone:S` 在 `AdmitTTL`(60s)内过期,锁键 30s 内过期。目标 zone T 的 leader 只出 `queue:zone:T`;S 的排队玩家重新走 AssignGate 时按角色列表给出的归属 zone 进 T 的队,排队位次不迁移(§14 第 4 条)。合服回退把 S 拉起来后,S 的新 leader 自然接手。要立即清理见 §15。

**停机空窗(未核实,既有行为)**:SIGTERM 路径能否走到 `Dispatcher.Stop()` 主动释放锁尚未核实(login 与 scene_manager 的注释说法冲突)。最坏情况是锁按 TTL 过期,滚动升级时该 zone 约 30s 没有 leader,期间不放行、不丢人。修复前(全服锁)也是这样,不是 G1b 引入的回归。

---

## 8. 重连/顶号豁免

`enter_gs_type` 表(参见 [player_login_flow.md](./player_login_flow.md))规定:`REPLACE`(顶号)/`RECONNECT`(30s 重连)的玩家 **不应排队**——他本来就在游戏里,只是断线重连一下。

实现:`AssignGateLogic.lookupSessionByAccount(account)`
1. 从 Redis `account_data:{account}` 拿到 player_id 列表
2. 调 `sessionmanager.GetSession(playerId)` 查 player_locator
3. 若 `existing.GateID != ""` → 跳过排队,fast path

**已知妥协**:目前账号→player_id 取列表第一项。多角色账号需要扩展(查 `playerId` 列表里哪个有 ONLINE/DISCONNECTING session),不影响本期发布,留 TODO。

---

## 9. 客户端协议

```
AssignGate(account, deviceId, zoneId)
   ├─ status=0:连 gate(payload, signature)
   ├─ status=1 + queueSource="login":每 retryAfterMs 调 QueryQueueStatus(queueToken)
   │     ├─ status=0:连 gate
   │     ├─ status=1:UI 更新 rank/total
   │     └─ status=3:丢 queueToken,重调 AssignGate
   ├─ status=1 + queueSource="ratelimit":sleep retryAfterMs,重调 AssignGate(无 queueToken)
   └─ status=2/3:UI 显示错误或重新走 AssignGate
```

robot 的 queue-aware 实现见 `robot/main.go::assignGateHTTPLocal` + `robot/http_assign_gate.go::AssignGateWithQueue`。

---

## 10. 配置(login.yaml `Queue` 段)

```yaml
Queue:
  Enabled: false                    # kill switch,默认关
  DispatchInterval: 1s              # dispatcher 出队节奏
  AdmitTTL: 60s                     # admit token 客户端要在此 TTL 内 GETDEL
  QueueEntryTTL: 1h                 # ZSET / meta / token-index 的 TTL 上限
  SoftCapMultiplier: 1.5            # 没配 ZoneCapacityOverride 时,用 max(online,1) * mult
  DefaultRetryAfterMs: 2000         # 客户端轮询间隔建议
  DispatcherLockTTL: 30s            # leader 锁 TTL
  DispatcherLockKey: "dispatcher:lock:login_queue"  # 锁键前缀;实际 = <前缀>:zone:<Node.ZoneId>,每 zone 一个 leader
  ZoneCapacityOverride:             # 推荐生产配死,不靠 SoftCapMultiplier 估算
    "1": 5000
    "2": 5000
```

---

## 11. 观测与压测

### robot 周期 stats(`[stats ...]` 日志)

新增 5 个字段:
```
q_entered=4080 q_admitted=920 q_expired=3 q_avg_wait=28s q_max_rank=4078
```

### 健康范围速查

| 指标 | 健康 | 红线 |
|---|---|---|
| q_admitted 增速 | 接近 zone capacity / s | 长时间停滞 → dispatcher 卡死 |
| q_expired / q_entered | < 1% | > 5% → AdmitTTL 太短或客户端轮询慢 |
| q_avg_wait | < 60s(1000/s admit 速率下) | > 5min → dispatcher 落后 EnterGame |
| q_max_rank | 早期升高,后期单调下降到 0 | 不下降 → 该玩家卡住 |

详细判读见 [docs/ops/login-queue-stress-runbook.md](../ops/login-queue-stress-runbook.md)。

### Prometheus 指标(2026-05-14 已接入)

通过 go-zero 内置 `/metrics` endpoint 暴露,详细告警/看板 PromQL 见 [login-queue-stress-runbook.md](../ops/login-queue-stress-runbook.md) §"Grafana / Prometheus 指标"。

| 指标 | 类型 | 标签 |
|---|---|---|
| `login_queue_size` | Gauge | zone |
| `login_queue_admitted_inflight` | Gauge | zone |
| `login_queue_wait_seconds` | Histogram | zone |
| `login_queue_admit_total` | Counter | zone |
| `login_queue_expired_total` | Counter | zone, reason |
| `login_queue_dispatcher_is_leader` | Gauge | pod, zone |

---

## 12. 实施回顾

按 plan 分了 4 个 PR,逐个独立可回滚:

| PR | 内容 | 测试 |
|---|---|---|
| 1 | proto 字段 + Java DTO 添加 | 仅字段添加,proto-gen.exe 后所有调用方自动可见 |
| 2 | go-zero `loginqueue` 包 + handler + dispatcher | 8 单元 + 3 集成,共 11 测试,< 1s |
| 3 | Java Gateway 改 gRPC client + 删本地 HMAC + QueueStatusController | mvn test 36/36 |
| 4 | robot queue-aware + Stats 5 个新指标 + 压测 runbook | go test PASS |

**前置工作**:`go/login/internal/logic/pregate/getgatelistlogic.go` 已删除——它是 2026-04 留下的孤儿(grep 零调用方),与 `loginpregate/assigngatelogic.go` 同时存在,功能重复。本次顺手收敛。

---

## 13. 与既有决策的关系

- **决策 #2(Java Gateway 选型)**:不变。Java 仍是 HTTP 入口,只是 AssignGate 的"决策"职责下沉到 go-zero,Java 退化为转发器
- **决策 #9(2026-05-08:Login 上移到 HTTP)**:本期延续——AssignGate 也从 Java 自签 HMAC 改成调 go-zero gRPC,**消除两份签名实现并存**(Java 之前自己实现了一份选 gate + 签 HMAC,本次删除)
- **决策 #13(Bucket4j 限流)**:**保留**,本期作为 Queue 的前置闸,不重叠
- **缺口 #10(开服削峰)**:升级状态从"Bucket4j 已落地"→"Bucket4j + 真排队都已落地"

---

## 14. 已知不足 / 后续工作

1. **多角色账号重连**:`lookupSessionByAccount` 仅取第一个 player_id,极少数多角色账号在罕见情况下可能被错排。修复成本低,等真有玩家投诉再补。
2. **Prometheus 接入**:列在 §11,本期未做,等首次生产灰度后再加。
3. **进场景排队**:本期不做,等真有"攻城战"等玩法时按同一套 ZSET 模板升级。详见对话记录中 §"为什么本期不做"。
4. **Queue token 转移**:玩家换设备重登拿到新 token,丢失原位置。目前可接受。
5. ~~**多 zone 公平性**:dispatcher 按 `activeZones` 顺序遍历,极端情况下前面 zone 抢光 admit 批次。~~ 已由 §7.1 消解:每个 zone 有独立的 leader,不存在 zone 之间互相抢批次。

---

## 15. 故障应急

**关闭排队(秒级回滚)**:`login.yaml` 改 `Queue.Enabled: false`,所有玩家立刻走 fast path。

**清空堆积**:

> 多 zone 共用 Redis 时,下面后三行(`admit:*` / `queue:meta:*` / `queue:token:*`)不分 zone,会清掉**全部 zone** 的放行槽和排队条目;只想清一个 zone 时只执行第一行,其余键按 TTL(60s / 1h)自然过期。

```
redis-cli DEL queue:zone:1 admitted:zone:1
redis-cli --scan --pattern 'admit:*'        | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:meta:*'   | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:token:*'  | xargs -r redis-cli DEL
# leader 锁一般不用删(30s 自然过期);确需强制换主:
redis-cli DEL dispatcher:lock:login_queue:zone:1
# 合服后源 zone S 的队列残留会在 QueueEntryTTL(1h)内自行过期;要立即清:
redis-cli DEL queue:zone:<S> admitted:zone:<S> dispatcher:lock:login_queue:zone:<S>
```

玩家自动重试 `/api/assign-gate` → 重新入队(若 Enabled=true)或直接进游戏(若 Enabled=false)。
