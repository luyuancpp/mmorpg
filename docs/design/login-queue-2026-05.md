# Login Queue (AssignGate Real Queue) — 2026-05

> **决策**: ARCH.md §11 #14
> **代码**: `go/login/internal/logic/pkg/loginqueue/`, `java/gateway_node/.../service/AssignGateService.java`
> **运维**: [docs/ops/login-queue-stress-runbook.md](../ops/login-queue-stress-runbook.md)
> **关联**: 2026-05-08 [open-server-rate-limit-design.md](./open-server-rate-limit-design.md)(Bucket4j 限流);本文是其**互补层**,不是替代

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
                   │ Dispatcher goroutine(单 leader,1s 一拍):              │
                   │   for zone:                                              │
                   │     free = capacity - online - admitted_inflight         │
                   │     ZPOPMIN free 个 → 签 gate token → SET admit:{queueId}│
                   └──────────────────────────────────────────────────────────┘

                   Redis (共享):
                   ─ queue:zone:{zoneId}      ZSET, score=入队 ms, member=queueId
                   ─ admit:{queueId}          STRING JSON{ip,port,payload,sig,deadline}, TTL=60s
                   ─ admitted:zone:{zoneId}   SET of queueId, TTL=60s(滑动)
                   ─ queue:meta:{queueId}     HASH {account,device,zoneId,createdAt}, TTL=1h
                   ─ queue:token:{token}      STRING queueId, TTL=1h(token→queueId 反查)
                   ─ dispatcher:lock:login_queue  STRING uuid, TTL=30s(leader 锁)
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
for each zoneId in activeZones():
    free = FreeSlots(zoneId)
    if free <= 0 or ZCARD == 0: continue
    batch = min(free, ZCARD)
    members = ZPOPMIN queue:zone:{zoneId} batch
    for each queueId in members:
        admit = PickAndSignGateToken(zoneId)  // 选 gate + 签 HMAC
        SET admit:{queueId} <admit JSON> EX AdmitTTL
        SADD admitted:zone:{zoneId} queueId
        EXPIRE admitted:zone:{zoneId} AdmitTTL  // 滑动 TTL
```

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
lock, _ := rl.TryLock(ctx, "dispatcher:lock:login_queue", 30*time.Second)
if lock.IsLocked() {
    stopHeartbeat := lock.StartHeartbeat(10*time.Second, 30*time.Second, onLost)
    drainLoop(ctx)  // 1s 一拍出队
    stopHeartbeat()
}
```

- 非 leader 副本每 10s `TryLock` 重试,被动备份
- Lost lease(Redis 抖动 / 手动 kill)→ heartbeat 触发 `drainCancel`,goroutine 退出
- 新 leader 在 ≤ TTL(30s)内接管。期间 admit 暂停,客户端继续轮询不报错(只是 rank 不动)

**为什么不用 etcd 选举**:RedisLocker 已在项目里跑了几个月没翻车,加 etcd lease 是重复造轮子。RedisLocker 唯一弱点是 Redis 主从切换时短暂双 leader,但本场景下双 leader 的最坏情况是 admit 多发一批(玩家更早进游戏),不是数据损坏。

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
  DispatcherLockKey: "dispatcher:lock:login_queue"
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
| `login_queue_dispatcher_is_leader` | Gauge | pod |

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
5. **多 zone 公平性**:dispatcher 按 `activeZones` 顺序遍历,极端情况下前面 zone 抢光 admit 批次。实测不是问题(每 zone 独立 FreeSlots),边界条件待观察。

---

## 15. 故障应急

**关闭排队(秒级回滚)**:`login.yaml` 改 `Queue.Enabled: false`,所有玩家立刻走 fast path。

**清空堆积**:
```
redis-cli DEL queue:zone:1 admitted:zone:1
redis-cli --scan --pattern 'admit:*'        | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:meta:*'   | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:token:*'  | xargs -r redis-cli DEL
```

玩家自动重试 `/api/assign-gate` → 重新入队(若 Enabled=true)或直接进游戏(若 Enabled=false)。
