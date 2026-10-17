# Login Queue (AssignGate Real Queue) — 2026-05

> **决策**: ARCH.md §11 #14
> **代码**: `go/login/internal/logic/pkg/loginqueue/`, `java/gateway_node/.../service/AssignGateService.java`
> **运维**: [docs/ops/login-queue-stress-runbook.md](../ops/login-queue-stress-runbook.md)
> **关联**: 2026-05-08 [open-server-rate-limit-design.md](./open-server-rate-limit-design.md)(Bucket4j 限流);本文是其**互补层**,不是替代
> **修订**: 2026-10-12 G1b — 出队 leader 锁改为每 zone 一把(§7.1)
> **修订**: 2026-10-13 G4b-G — 在途占位改为逐个到期的 ZSET,交票后保留 15s(§5「在途占位的生命周期」)
> **修订**: 2026-10-15 — 负责人按「最标准」拍板:交票后宽限 15s → 20s;在途占位键的整键 TTL 改为只延长不缩短;真实 Redis 首跑定为打开排队前的硬门槛(§5「在途占位的生命周期」、§14 第 8 条)。出队每个非空拍只取一次 gate 候选、批内按本拍已分人数轮转(§5 Dispatcher Loop)。均已落码,**未编译、未测试**,待 Codex 验证

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
                   ─ admitted:zset:zone:{zoneId}  ZSET member=queueId | fast:<uuid>, score=占位到期毫秒(逐个到期;整键 TTL 只做垃圾回收)
                   ─ queue:meta:{queueId}     HASH {account,device,zoneId,createdAt}, TTL=1h
                   ─ queue:token:{token}      STRING queueId, TTL=1h(token→queueId 反查)
                   ─ dispatcher:lock:login_queue:zone:{zoneId}  STRING uuid, TTL=30s(本 zone leader 锁)
```

> **(2026-10-15 更正,上图原文保留)** 上图键表里 `admit:{queueId}` 一行的值格式早已过时。R17 R1 修复(2026-06-04)起,放行槽只存 `AdmitSlot`:JSON `{"node_id","ip","port","zone_id"}`,即 dispatcher 挑中的 gate 端点及其 zone,**不含** `token_payload`、签名与 deadline(`go/login/internal/logic/pkg/loginqueue/queue.go:77-98`)。放行时写入:`SET admit:{queueId} <AdmitSlot JSON>`,TTL 仍为 `AdmitTTL`(默认 60s),与在途占位的 `EVAL admittedHoldScript` 排进同一个 MULTI(`queue.go:465-473`)。gate 票在客户端取走时才签:`Lookup` → `consumeAdmitAndSign` 先 `GETDEL admit:{queueId}`,再用调用方传入的 `SignAdmitFn` 现签(`queue.go:331-361`;签票实现是 `gatetoken.go` 的 `SignGateToken`),所以 gate 票的有效期从取走那一刻起算。同图 Dispatcher 框里「签 gate token → SET admit:{queueId}」也按此读:出队只挑 gate、不签票(§5 Dispatcher Loop 已是现行写法)。另:`queue:meta:{queueId}` 的字段名是 `zone_id`,不是 `zoneId`(`queue.go:207-212`)。

> **(2026-10-15 更正,上图 AssignGate 第 4 步;原文保留)** 第 4 步「FreeSlots>0 且队列空 → 直接签 gate token」漏了占位:签票之前先调 `TryReserveFastPathSlot`(`go/login/internal/logic/loginpregate/assigngatelogic.go:110-129`;脚本与函数见 `go/login/internal/logic/pkg/loginqueue/queue.go:563-593`),一段 Lua 先清掉 `admitted:zset:zone:{z}` 里已到期的成员,未到期成员数 < `budget`(= `capacity − online`,注意不是 `free`)才写入成员 `fast:<uuid>`、分数 = 现在 + 交票宽限 20s(`defaultHandoffGrace`),并按「只增不减」续整键 TTL;返回 1 才签票。占不到(名额被并发请求占满)或脚本出错(Redis 抖动,fail-closed)都改走第 5 步入队,不签票。这个占位不撤,20s 后按分数自然到期,期间与排队路径的在途占位一起计入 `FreeSlots` 的 `admitted_inflight`,所以快速通道同样受容量约束,不是旁路。第 3 步(`Queue.Enabled=false`)与第 2 步(重连豁免)才是真正「直接签票」,不读不写任何排队键。占位的生命周期见 §5「在途占位的生命周期」表的「快速通道占位」一行。

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

> **(2026-10-15 更正,上表「客户端动作」列原文保留)** 负责人 2026-10-15 定了持有 `queue_token` 时的客户端契约,以 `docs/design/open-server-rate-limit-design.md` §11.5 的 2026-10-15 更正与补注为准。上表按此改读:
> - `100 "login"`:拿到 token 后一律轮询 `/api/queue-status`,不再是「sleep `retryAfterMs`」。每次等待 = max(`retryAfterMs`, 2s) + [0, 20%] 均匀抖动;queue-status 连续失败时下限从 2s 起翻倍,封顶 30s,成功一次即复位。
> - `100 "ratelimit"` 与 `429`:上表动作只适用于**不持** `queue_token` 的请求;429 的「退避」指 30/60/120s。
>   持 token 的客户端只在 queue-status 本身失败(网络错误 / HTTP 非 200 / `code=500`)时带 token 重调一次 assign-gate 作恢复。
>   重入回这两种时保留 token、回到 queue-status 轮询,**不**退避重调 assign-gate,否则已被放行的玩家会错过 `AdmitTTL`(60s)放行窗口。
> - 表里未列的 `503`(`zone_maintenance` / `zone_closed` / `zone_not_open`)与 `404 zone_not_found`,由 gateway 区服准入在 assign-gate 与 queue-status 上都可能返回。持 token 时是终态:不重入,按错误结束。
> - `410` 一行不变。

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

> **(2026-10-15 更正,上图第 1 步「未命中 → status=QUEUEING + 当前 rank」;原文保留;已落码,未编译、未测试,待 Codex 验证)** 负责人 2026-10-15 按「最标准」定:放行之后客户端拿到确定的终态,不再停在名次 0。此前「未命中」一律回 QUEUEING,放行槽没人来取、按 `AdmitTTL` 过期之后再查,或已经取走过一次之后再查(回包丢失后重试、go-redis 超时重发 `GETDEL`),ZRANK 都落空,回「排队中、名次 0」,客户端一直轮询到 meta 按 `QueueEntryTTL`(1h)过期(§14 第 6 条)。现行判定(`go/login/internal/logic/pkg/loginqueue/queue.go` 的 `Lookup`,`:282` 起):
>
> - dispatcher 放行时在写放行槽的同一个 MULTI 里给 meta 记 `admitted_at`(放行时刻 unix 毫秒,`markAdmittedScript`,`queue.go:603`;只在 meta 还在时写,不凭空建出没有 TTL 的 meta;`HSET` 不改 meta 的 TTL)。MULTI 里各命令对其它客户端同时可见,所以「看得到 `admitted_at`」⇒「放行槽也已写过」。
> - `Lookup` 先 `GETDEL` 放行槽,取到即签票回 ADMITTED(不变)。取不到时一次 `HMGET meta zone_id admitted_at`(替换原来的 `HGET zone_id`,往返数不变)。
> - 有 `admitted_at`:再 `GETDEL` 一次(复用 `consumeAdmitAndSign`,覆盖「第一次 `GETDEL` 早于放行的 MULTI」的竞态),取到照常签票回 ADMITTED;仍取不到 = 放行槽已被取走或已过期,回 **EXPIRED**,`login_queue_expired_total` 记新原因 `admit_window_passed`(带 zone 标签)。gateway 对 assign-gate 重入与 queue-status 的 EXPIRED 都回 410,客户端丢掉 token 重走 `/api/assign-gate`(§4 表)。
> - 没有 `admitted_at`:维持原逻辑。ZRANK 命中回名次;落空是在途窗口(已 `ZPOPMIN`、写放行槽的 MULTI 还没落地,亚秒级),仍回 QUEUEING、名次 0,下一次轮询就能取到。
> - token 索引或 meta 不在、meta 损坏:EXPIRED(`meta_gone`),不变。
>
> 仍会停在名次 0 的只剩 `PopAdmit` 在写放行槽之前丢掉的条目(`pickFn` 返回 `(nil, nil)`、Marshal 失败、写放行槽的 MULTI 失败,§14 第 7 条),这类条目没有 `admitted_at`,与在途窗口无法区分。回包丢失时那张 gate 票仍找不回来,客户端收到 410 后从队尾重新排;Java 版对取走后再查同样回 410,另给取走加了请求号,让 go-redis 超时重发能拿回同一张票(`spec_G4b-G.md` §5.8 第 4 条),本仓库没有做。`queue:meta:{queueId}` 的字段因此多一个 `admitted_at`(§3 键表未列)。
>
> **(2026-10-15 补注,上面第 1 条与末段;原文保留;已落码,未编译、未测试,待 Codex 验证)** 同日复查后,`PopAdmit` 写放行的方式又改了一次:
>
> - 放行槽、`admitted_at`、在途占位改由一段脚本 `admitEntryScript` 写(`Script.Run`,即 `EVALSHA`,遇到 `NOSCRIPT` 回退 `EVAL`),不再是 MULTI{`SET`, `markAdmittedScript`, `admittedHoldScript`};`markAdmittedScript` 已删除。三者对其它客户端仍同时可见,「看得到 `admitted_at`」⇒「放行槽也已写过」照旧成立,上面 `Lookup` 的判定顺序不变。
> - 脚本先做两道只读判定:**meta 不在**(条目在队里待满 `QueueEntryTTL`,token 索引与排队令牌同时失效,再没人取得到)→ 整条丢弃,不写放行槽、不占名额、不计入放行数,每批一行 INFO(`reason=meta_gone`);**`admitted_at` 已在** → 什么都不写、计入放行数。第二道是改成单条命令的必要配套:go-redis 对单条命令在读超时 / 连接断开后会自动重发,原来的 MULTI 读失败不重发;不加这道,重发会写出第二个放行槽、签出第二张 gate 票。
> - `ZPOPMIN` 之后的写入不再随调用方 ctx(dispatcher 的 drainCtx)取消,改用 `context.WithoutCancel`:没有取消时不设截止,开区大批次不会被截断;看到停服或丢租约的取消之后,剩下的写入再给 3s 预算(`popAdmitWriteBudget`,自看到取消时起算)。停服、丢租约因此不再整批丢掉已弹出的条目。写入出错不回填队列,每批汇总一行 ERROR(`reason=write_failed`)。
> - `QueueEntryTTL` 必须为正、`AdmitTTL` 不小于 1ms(`loginqueue.ValidateTTLs`;放行脚本的 `SET PX` 取整到毫秒,亚毫秒会成为 `PX 0` 被拒):起服时 `initLoginQueue` 校验,`PopAdmit` 弹出之前也校验。
>
> 末段「仍会停在名次 0」的路径因此变为:取消之后写入超出 3s 预算、Redis 出错(含 noeviction 写满)、进程被强杀、`pickFn` 返回 `(nil, nil)`、Marshal 失败;「写放行槽的 MULTI 失败」一项改读「放行脚本执行失败」。详见 §14 第 7、8 条补注。

### FreeSlots 公式

```
online       = Σ(gate.PlayerCount for gate in GateWatcher.FetchAllNodes() if gate.zoneId == z)
capacity     = Queue.ZoneCapacityOverride[z] || ceil(max(online, 1) * SoftCapMultiplier)
admitted_inflight = 清掉过期成员后的 ZCARD admitted:zset:zone:{z}
free         = max(0, capacity - online - admitted_inflight)
```

**关键安全属性 — admitted_inflight 必须减**:dispatcher 写出 admit 后,该玩家从 ZSET 中 popped(不在 ZCARD)、从 gate.PlayerCount 中也未出现(他还没真连上),如果 FreeSlots 不算这部分,下一拍会再放出一批,**一秒内超发数倍 admit token**,gate 那边玩家 token 验证全失败。取走后占位再留 20s(2026-10-13 起为 15s,10-15 改 20s),覆盖「票已交出、gate 还没把人数报上来」这一段(G4b-G;此前取走即撤占位,这一段漏计,见下一小节)。

### 在途占位的生命周期(G4b-G,2026-10-13)

2026-10-13 之前在途占位是 `admitted:zone:{z}`(SET,整键滑动 TTL),排队一打开就有两条缺陷:

- **Q1**:快速通道的 `fast:<uuid>` 成员从不单独删除,每次写入又把整键 TTL 续上。已连上 gate 的快速通道玩家同时算进 `online` 和在途,持续有人登录时余量被重复扣到 0,「放一批、停最长 60s」。
- **Q2**:取走即 SREM,而 gate 的人数要等下一次上报才涨。开区时每拍都按「在线 0、在途 0」再放一满批,能放进几倍容量。

G4b-G 换成新键 `admitted:zset:zone:{z}`(ZSET,分数 = 占位到期的 Unix 毫秒),每个成员按自己的分数到期。完整推导、测试与评审记录见 [spec_G4b-G.md](../handoff/launch-readiness-20261013/spec_G4b-G.md)。

一个占位成员的一生(`G` = 交票后宽限 20s,2026-10-15 起;10-13 至 10-15 为 15s。`now` = login 进程时钟的 Unix 毫秒):

| 路径 | 成员 | 写入时的分数(到期毫秒) | 之后 | 何时消失 |
|---|---|---|---|---|
| dispatcher 放行 | `queueId` | `now + AdmitTTL` | 客户端轮询取走时改成 `now + G` | 没人来取:放行后 `AdmitTTL` 到点;取走:取走后 `G` 到点 |
| 快速通道占位 | `fast:<uuid>` | `now + G` | 不再改 | 占位后 `G` 到点 |

- 到点的成员在**下一次计数或占位**时被 `ZREMRANGEBYSCORE key -inf now` 清掉(分数 ≤ now 即过期),没有后台清理任务。计数 = 清掉过期成员之后的 `ZCARD`,清理与计数在同一段 Lua 里,所以计数是一条写命令。
- 取走只发生一次(`GETDEL` 只成功一次)。重复轮询在改分数之前就返回,不会反复续期。`signFn` 失败、meta 读不到 zone 时不改分数,占位保持放行时的分数,最迟 `AdmitTTL` 到点。
- 整键 TTL 只做垃圾回收,成员是否有效只看分数。zone 下线或被合走后没人再来读,整键 TTL 负责把残留收走。**只延长不缩短**(2026-10-15 起):三条写路径(放行、取走、快速通道)都是先 `ZADD`,再看剩余 `PTTL`,小于 `W = max(AdmitTTL, G) + 1min`(默认 2 分钟)才 `PEXPIRE W`。
  - 为什么:`W` 由各副本按自己的 `AdmitTTL` 算,排队开着时滚动调整 `AdmitTTL` 就有新旧两种 `W` 并存。10-13 版每次写入无条件 `PEXPIRE W`,小 `W` 副本的写入会把整键 TTL 压到大 `W` 副本写的占位之前,该 zone 随后一段时间没有写入(例如满员停放)时,整键连同未到期的占位一起消失,下一拍最多多放一批。只延长不缩短之后整键不会先于任何成员到期,`AdmitTTL` 可以在排队开着时滚动调整(此前「调整幅度不超过 1min」的约束作废)。代价:调小之后,已续上的较大整键 TTL 要自然走完才收敛,只影响 zone 下线后键多留多久。做法与 Java 版的「不足才续」一致。
  - 为什么不用 `PEXPIRE … GT`:`GT` 把没有 TTL 的键当作 TTL 无限大,刚由 `ZADD` 建出的键就永远设不上过期(miniredis v2.35.0 已对源码核实;真实 Redis 7 凭记忆,未核实)。也必须先 `ZADD` 再读 `PTTL`:键不存在时 `PTTL` 回 −2,`PEXPIRE` 打空,随后 `ZADD` 建出的键没有 TTL。
  - 放行与取走共用一段脚本(`queue.go` 的 `admittedHoldScript`):放行在原来的 MULTI 里 `SET` 放行槽后 `EVAL` 它,取走直接 `EVAL`;快速通道脚本里是同样的两行。只有 10-15 之前的 G4b-G 镜像(无条件续期)与本版在排队开着、`AdmitTTL` 不同时混跑,旧镜像的副本才仍会缩短整键 TTL;按下文「排队关着滚」的部署顺序不会出现。
- 旧键 `admitted:zone:{z}` 新版本不读、不写、不删。最后一个旧版本停写后,它在 60s 内整键过期。

**时钟:login 进程时钟,经 ARGV 传进脚本**。分数和「现在」都由 Go 用 login 的墙钟算好,不用 Redis `TIME`。理由:测试直接拨假时钟,脚本里没有时间运算;与 Java 版、本包既有做法(排队令牌、gate 票的过期都用 login 墙钟)一致。代价是多副本时钟偏差 δ 直接进占位寿命(± δ)。同一 zone 的副本间偏差按毫秒到几十毫秒估(假设,未测),达到宽限量级(≥ 20s)才有可见后果。2026-10-15 负责人确认维持 login 时钟:系统本来就依赖各节点时钟同步(gate 票与排队令牌的过期同样用 login 墙钟),**login 节点做 NTP 时钟同步是部署要求**;也不在脚本里加上界清理(时钟故障按运维故障处置,见下面的止血):

- **故障副本还在跑时**:它读的时候按自己的钟清理,别人写的、其实还有效的占位被早清,多放。
- **故障副本恢复(NTP 校回)或被摘掉之后,不会立刻自愈**:它最后 20s / 60s 内写的占位,分数停在「故障钟 + 20s / 60s」,还要再过 δ 才到期。这段时间该 zone 的有效容量少掉这些占位(≤ 当时的 `capacity − online`)。清理只有 `-inf … now` 一个方向,而每次写占位都保证整键 TTL 至少剩 `W`,所以只要 zone 还有写入,整键 TTL 不兜底;整个 zone 完全停放时,最长约 `AdmitTTL` + 整键 TTL(默认 3 分钟)后自愈。旧实现没有「占位按某个副本的钟到期」这回事,这是新增的失败模式。
- **判断与止血**:`redis-cli ZRANGE admitted:zset:zone:<z> 0 -1 WITHSCORES` 里有分数比当前毫秒时间戳大出 `max(AdmitTTL, 20s)` 以上的成员,就是这种残留。止血 `redis-cli DEL admitted:zset:zone:<z>`,代价是在途清零,最多多放一批 `capacity − online`(写在 runbook 模式 2)。
- **退路**:偏差不可忽略时改用 Redis `TIME`。键、成员、分数含义都不变,新旧两种写法可以混跑。

**20s 从哪来**:票交出去之后,这个人要「连上 gate(耗时 `c`)→ gate 下一次上报人数(C++ gate 每 10s 一次,`cpp/nodes/gate/main.cpp` 的 `playerCountReportTimer`)→ login 读 etcd(不缓存)」才出现在 `online` 里,最坏滞后 `c + 10s`。20s = 两个上报周期 = 一个周期的上报滞后 + 10s 建连与传播余量。Java 版是 15s,它的 gate 每 5s 发布一次,余量同为 10s。2026-10-13 本仓库照搬 15s,余量只有 5s;10-15 负责人按「最标准」改为 20s,两版余量对齐,宽限值本身不再相同。

- 20s 是代码常量 `loginqueue.defaultHandoffGrace`,不是配置。它与 gate 的上报周期没有机械关联,改上报周期时要同步核对它。数值待 L-14 测「交票 → gate 会话建立」的分布后确认。
- 快速通道当场交票,等同排队路径的「已取走」,所以也取同一个宽限(20s,代码里是同一个字段),不取 `AdmitTTL`(Java 版取 60s,这一点两版有意不同)。

**算式**:记 `C` 容量,`O` gate 上报的在线数,`A` 未到期的占位数,`free = max(0, C − O − A)`;`v = c + φ` 是交票后这个人出现在 `O` 里的时刻,`φ ∈ [0, 10s)` 是距下一次上报的相位。以下按 `G` = 20s 算(10-13 版按 15s 算的数字见 spec_G4b-G.md §5.2 的括注)。

- `v < G` 时,`[v, G)` 这段被算两遍(占位 + `O`),少放。`c` 取 1s 时双计 9~19s、平均约 14s;瞬时少放的名额 ≈ 交票速率 × 14s,最多 × 20s。
- `v > G` 时,`[G, v)` 这段一遍都没算,多放。只在 `c + φ > 20s` 时出现,建连在 10s 以内时为 0。gate 迟迟不上报或建连普遍超过 10s 时,多放的节奏从修复前的约每 2~3s 一批降到约每 22~23s 一批,每批 ≤ `C − O`:有界改善,不是消除。
- gate 票有效期 10 分钟,远大于宽限。拿了票超过 20s 才连上的人不在任何占位里,连上就是多 1 人(既有性质;gate 侧另有连接上限)。

**放行节奏的上界**(判读压测与 runbook 用):每次放行都以 `O + A ≤ C` 为界。一个名额被排队放行的人拿走后,最早要到「取走 + 20s」才可能放回,而且前提是这个人没有进 `O`。

- 放进去的人留在线上(正常玩家):名额从占位转给在线数,不放回。排队的长期放行速率 = 在线玩家的离开速率;`O + A` 顶到 `C` 之后 `q_admitted` 不涨是**正常回压**,不是 dispatcher 卡死。
- 拿了票不连 gate,或连上很快就走:名额每约 22~23s(取走延迟 ≤ 轮询 2s + 一拍 1s,再加 20s)周转一次,任意这样一个窗口内经排队放行的人数 ≤ 窗口起点的 `C − O`。修复前这个周期是 2~3s(Q2)。
- §11 与 runbook 里写于 2026-05 的节奏数字没有按这个上界标定过,待 L-14 重新标定。

**新旧版本混跑与部署顺序**:旧 Pod 读写 `admitted:zone:{z}`(SET),新 Pod 读写 `admitted:zset:zone:{z}`(ZSET)。键不同,不会 `WRONGTYPE`;但两边互相看不见对方的占位。

- **要求的做法:排队关着滚**。`Queue.Enabled=false` 时不构造 `Queue`,新旧两种键都没人碰,滚动对占位记账是空操作(当前所有环境都是这个状态)。先在排队关闭下把所有 zone 的 login 滚到含 G4b-G 的版本,再打开排队。这条要求落在三处:`login.yaml` 上线清单第 0 步(手工打开排队的人照它做);K8s 上的合入顺序 G1b → G4b-G → G4b-2(只有 G4b-2 才写 `Queue` 段,它的契约用例 15 断言 `queue.go` 已是新键);本节与 runbook「故障应急」。
- **排队开着滚**(只在本机或裸机有人手工打开过时可能),按谁经手逐格记账:
  - 旧 leader 放行、旧 Pod 取走:与 G4b-G 之前完全相同,取走即 SREM,Q2 照旧;
  - 旧 Pod 的快速通道:照旧写旧键的 `fast:<uuid>`,Q1 照旧;
  - 旧 leader 放行、新 Pod 取走:旧 SET 里的成员没人删,按旧版 Q1 的方式残留(旧 leader 少放);新键里补一个 20s 的占位;
  - 新 leader 放行、旧 Pod 取走:新键里的占位不被改分数,留到放行后 60s(少放,有界);
  - 新 leader 放行、新 Pod 取走,以及新 Pod 的快速通道:G4b-G 的行为;
  - 两种键互不可见带来的额外量:新旧两边的快速通道各自按自己的键判余量,任一时刻在途占位合计最多 2 ×(`C − O`);
  - 不丢人、不重复放行:`ZPOPMIN` 与 `GETDEL` 的原子性与占位键无关。
- **结论**:相对各自版本单跑,混跑额外多放 ≤ 一个 `C − O`、少放有界,窗口等于滚动时长;**leader 或经手的副本是旧版期间 Q1 / Q2 仍在,不能把混跑窗口当作 G4b-G 已生效**。G4b-G 的两条修复只在放行方与取走方都是新版时才成立。可以接受,但不推荐。
- **回滚(新 → 旧)**:窗口与上面对称,新键在最后一次写入后 2 分钟内整键过期(有副本配过更大的 `AdmitTTL` 时,按其中最大的 `W`)。排队开着时不要用 `kubectl rollout undo` 退镜像:先用 `-LoginQueue off` 关排队,再退(§15)。

### Dispatcher Loop

```
zoneId = Node.ZoneId  // 只出本 zone;队列空直接跳过
if ZCARD queue:zone:{zoneId} == 0: return   // 先判空,空闲 zone 不碰 gate 目录
candidates = CandidatesForZone(zoneId)      // 每个非空拍只取一次 gate 候选(2026-10-15 起)
free = FreeSlots(candidates)                // 再算容量,与挑 gate 共用这一份候选
if free <= 0: return
batch = min(free, ZCARD)
members = ZPOPMIN queue:zone:{zoneId} batch
picker = 本拍的挑 gate 器(candidates)        // 负载 = PlayerCount + 本拍已分给它的人数
for each queueId in members:
    gate = picker.pick()                    // 取负载最少、同负载 NodeID 最小,选中后本拍计数 +1
    MULTI
      SET admit:{queueId} <AdmitSlot JSON:选好的 gate> EX AdmitTTL   // gate 票在取走时才签
      EVAL admittedHoldScript:              // 一段脚本(2026-10-15 起)
        ZADD admitted:zset:zone:{zoneId} <now_ms + AdmitTTL> queueId   // 占位按自己的分数到期
        if PTTL < W: PEXPIRE W              // W = max(AdmitTTL, 20s) + 1min,整键 TTL 只延长不缩短
    EXEC
```

**一拍的 gate 分配(2026-10-15 起;未编译、未测试)**:每个非空拍只调一次 `CandidatesForZone`(生产实现是一次 etcd 前缀读加一次排空标记 MGET,不缓存),算容量和批内挑 gate 共用这一份。批内按「gate 上报的 `PlayerCount` + 本拍已分给它的人数」挑最少的,同数取 NodeID 最小,选中后本拍计数 +1:一拍的放行先填最闲的 gate,填平之后在各台之间轮转。账只记在本拍,下一拍重新取候选、从 gate 上报的人数重新算起。

- 改之前:每放一个人重取一次候选(一拍放 N 人就是 N 次串行目录读),而 `PlayerCount` 每 10s 才上报一次、一拍之内不变,只按它挑会让整批都落到同一台 gate。这批条目已被 `ZPOPMIN` 弹出、既不在队列也不在占位里,拍越长,进程中途退出时丢的越多。
- 快速通道(assign-gate 当场签票)仍是一次 `FreeSlots` 加一次签票,各读一次目录,单次挑选规则不变(`PickGate`,与批内挑选共用同一份规则)。
- 本拍没有候选时 `FreeSlots` 先报 `no gates for zone`,整拍不出队、条目原样留在队列(日志口径不变,见 runbook 模式 1)。
- 与 Java 版「一轮按段挑 gate、把这一轮已分给它的人数算进负载」对齐(Java 仓库 `PARITY.md`「登录排队」行 ③)。

2026-10-12(G1b)以前是 `for each zoneId in activeZones()`,且先 FreeSlots 后 ZCARD。`activeZones` 实际只含本 zone(见 §7.1),所以改成单 zone 不丢功能。改成先判空的代价:队列非空、本 zone 却一台 gate 都没有时,每拍一条 `[loginqueue] freeSlots zone=… queueLen=… err=…` ERROR(以前被静默跳过)。

**signFn 失败回滚**:如果选 gate 或签 token 失败(罕见,gate 全部下线),把 queueId 以 score=0 重新 ZADD,保证下一拍最优先重试,不丢人。见 `queue.go:PopAdmit`。

---

## 6. Queue Token 安全模型

`queue_token` 是服务端签名的 opaque blob:

```
queue_token = base64url( JSON{kind:"queue", q:queueId, z:zoneId, e:expireTs} | "." | hex(HMAC-SHA256) )
```

- **HMAC 用什么 secret**:复用 `GateTokenSecret`。理由:queue token 和 gate token 的 wire-format 完全不同(JSON+HMAC vs proto-marshalled GateTokenPayload),不可能互相 confuse;一个 secret 旋转就同时换两套,运维更简。
- **(2026-10-14 事实更正,上一条已过时、原文保留)**:代码早已按用途拆出独立的 `Secrets.QueueToken`,**不再复用 `GateTokenSecret`**(`go/login/internal/config/secrets.go` 包注释与 `SecretsConf.QueueToken`;拆开的理由是「任何一侧泄露,另一侧同时失守」)。规则以 `ResolveSecrets` 为准:
  - 生产模式(`Mode` 不是 dev / test,空串也按生产)下 `Queue.Enabled=true` 就必须配 QueueToken,主密钥与附加密钥都要 ≥32 字节、不是占位串,且主密钥不得与 GateToken、InternalAuth 相同,否则 login 拒绝启动;
  - 只有宽松档(dev / test)在没配 QueueToken 时回落 GateToken 的密钥,并打一条 WARN;
  - 每把密钥是「主密钥 + 只验不签的 `Additional`」,支持三段式不停服轮换(新密钥先进 Additional → 提升为主密钥 → 摘掉旧密钥)。
  - K8s 上由环境变量 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET` 注入,见 §10「K8s 上的取值」。
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
- 新 leader 在 ≤ TTL + TTL/3(约 40s)内接管:leader 被 kill -9 时锁剩余 TTL 在 20~30s 之间(心跳每 10s 续到 30s),键过期后备机最多再等一个 10s 重试周期。期间 admit 暂停,客户端继续轮询不报错(只是 rank 不动)
- 任期内 panic(出队路径的代码缺陷,2026-10-14 起):一个任期(`leadOneTerm`)的收尾——停心跳、leader 指标归 0、放锁——全走 defer,panic 展开时照样执行;safego 按任期接住(点位 `login.queue_dispatcher_term`)后本副本退避 10s 再参选,同 zone 备机可以接手。此前收尾是直线代码,panic 后心跳 goroutine 还在续期,副本成了「持锁不出队」的僵尸 leader,该 zone 队列只进不出
- leader 指标按电平重发:leader 每拍重发 1,备机每轮抢锁失败重发 0。login 起服时 dispatcher 早于 go-zero 指标开关打开就已当选,那时写的样本被丢弃,只在边沿写一次的话冷启动后整个任期没有这条序列

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
2. **超发有界**:每个 leader 都按自己读到的在途占位数算余量,自己上一拍写入的占位在下一拍读数之前已完成,所以每个 leader 的在途数 ≤ cap − online,两个合计 ≤ 2×(cap − online)。与上文「双 leader 最坏是 admit 多发一批」的判断一致。
3. **消除窗口的办法(推荐)**:当前所有环境的排队都是关的。**在 `Enabled=false` 下把本修复滚完,再打开排队**,根本不会出现并存窗口(此时也不构造 dispatcher,`login_queue_dispatcher_is_leader` 一条序列都没有,新旧标签集不会并存)。偏要在 `Enabled=true` 下滚动时,旧 pod 的序列在 `sum by (zone)` 里归到 `zone=""` 一组,这组触发告警属于预期。老版本不再续期后,全服旧键 30s 内自然过期,不需要人工清理。

由 `TestIntegration_DualLeadersSameZone_NoLossBoundedOverAdmit` 钉住 1、2 两条。

**前提:锁与它保护的队列在同一个 Redis 实例上**。以后按用途拆 Redis 时,leader 锁必须跟着排队键走;拆开后 Redis 抖动会出现「锁还在、队列读不到」或反过来的组合,上面的有界分析就不成立。按 zone 分锁本身与「每 zone 一个 Redis」「全服共享一个 Redis」两种拓扑都兼容。

**zone-down 与合服后旧 zone 的队列**:本修复不产生新的残留。源 zone S 的 login 下线后不再有人 ZADD,`queue:zone:S` 在最后一次入队后 `QueueEntryTTL`(1h)内过期,`admit:*` 在 `AdmitTTL`(60s)内过期,`admitted:zset:zone:S` 在最后一次写入后 `AdmitTTL + 1min`(默认 2 分钟)内整键过期,锁键 30s 内过期。目标 zone T 的 leader 只出 `queue:zone:T`;S 的排队玩家重新走 AssignGate 时按角色列表给出的归属 zone 进 T 的队,排队位次不迁移(§14 第 4 条)。合服回退把 S 拉起来后,S 的新 leader 自然接手。要立即清理见 §15。

**停机空窗(未核实,既有行为)**:SIGTERM 路径能否走到 `Dispatcher.Stop()` 主动释放锁尚未核实(login 与 scene_manager 的注释说法冲突)。最坏情况是锁按 TTL 过期,滚动升级时该 zone 最长约 40s(TTL + 备机重试周期 TTL/3)没有 leader,期间不放行、不丢人。修复前(全服锁)也是这样,不是 G1b 引入的回归。

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

> **(2026-10-15 更正,上面协议树原文保留)** 上树是排队初版的口径,与负责人 2026-10-15 定的客户端契约有三处出入,以 `docs/design/open-server-rate-limit-design.md` §11.5 的 2026-10-15 更正与补注为准:
> - 「每 retryAfterMs 调 QueryQueueStatus」改为:每次等待 max(retryAfterMs, 2s) + [0, 20%] 抖动;QueryQueueStatus 连续失败时下限从 2s 起翻倍,封顶 30s,成功一次即复位。
> - 持 queueToken 时,只有 QueryQueueStatus 本身失败(网络错误 / HTTP 非 200 / gateway `code=500`)才带同一 token 重调一次 AssignGate 作恢复。重入回 `queueSource="ratelimit"` 或 429 时保留 token、回到轮询,不退避重调。
>   上树 `queueSource="ratelimit"` 一支(重调 AssignGate、无 queueToken)只适用于还没拿到 token 的请求。
> - gateway 区服准入回 503 zone_* 或 404 zone_not_found(上树未列)是终态:不重入,按错误结束。status=3(gateway 回 410)仍是丢 queueToken、不带 token 重调 AssignGate,同上树。
>
> 上一行 robot 的实现由另一名代理同批改为此口径;本注写下时未核对,未编译、未运行。
> (2026-10-16 核对补注)主会话已静态核对 robot 实现与上述口径一致(见设计稿 §11.5 补注的「落地状态」),仍未编译、未运行。

---

## 10. 配置(login.yaml `Queue` 段)

```yaml
Queue:
  Enabled: false                    # kill switch,默认关
  DispatchInterval: 1s              # dispatcher 出队节奏
  AdmitTTL: 60s                     # admit token 客户端要在此 TTL 内 GETDEL,也是没人来取时在途占位的寿命;
                                    # 取走后占位只再留 20s(代码常量 loginqueue.defaultHandoffGrace,不是配置;10-15 前为 15s);
                                    # 排队开着时也可以滚动调整(整键 TTL 只延长不缩短,§5)
  QueueEntryTTL: 1h                 # ZSET / meta / token-index 的 TTL 上限
  SoftCapMultiplier: 1.5            # 没配 ZoneCapacityOverride 时,用 max(online,1) * mult
  DefaultRetryAfterMs: 2000         # 客户端轮询间隔建议
  DispatcherLockTTL: 30s            # leader 锁 TTL
  DispatcherLockKey: "dispatcher:lock:login_queue"  # 锁键前缀;实际 = <前缀>:zone:<Node.ZoneId>,每 zone 一个 leader
  ZoneCapacityOverride:             # 推荐生产配死,不靠 SoftCapMultiplier 估算
    "1": 5000
    "2": 5000
```

**K8s 上的取值(G4b,2026-10-14;已落码,未编译、未测试,待 Codex 验证)**:`tools/scripts/k8s_deploy.ps1` 按 `-LoginQueue auto|on|off` 决定 login ConfigMap 写不写本段。
缺省 `auto` 跟随 `-ReleaseProfile`:dev 不写(go-zero 零值 `Enabled=false`,产物与 G4b 之前逐字相同),staging / prod 写。写的时候:

- `Enabled: true`;`DispatchInterval` / `AdmitTTL` / `QueueEntryTTL` / `SoftCapMultiplier` / `DefaultRetryAfterMs` / `DispatcherLockTTL` 从 `go/login/etc/login.yaml` 原样镜像。
- `DispatcherLockKey: "dispatcher:lock:login_queue:zone:<本 zone>"`:写进 ConfigMap 的前缀已经带本 zone,login 再按 §7.1 拼一次 `:zone:<Node.ZoneId>`,
  所以 **K8s 上本 zone 的实际锁键是 `dispatcher:lock:login_queue:zone:<id>:zone:<id>`**,选主日志 `[loginqueue] elected dispatcher leader zone=<id> key=…` 里打的也是它。
  每个 zone 仍恰好一把锁;§3、§15 与 `docs/ops/login-queue-stress-runbook.md` 写的 `dispatcher:lock:login_queue:zone:<id>` 是本地 / compose 默认配置下的键名,在 K8s 上排查时按带两段 zone 的键查。
  显式带 zone 是纵深防御:`kubectl rollout undo` 把 login 退回 §7.1 之前的镜像、而 ConfigMap 仍是开排队的这一份时,旧镜像原样使用配置值,照样按 zone 隔离,
  不会退回「全集群一个 leader、其它 zone 只进不出」。login 若改成「一个 leader 覆盖全部 zone」,这一行必须删掉,否则 N 个 leader 同时出队。
- `ZoneCapacityOverride` 只写本 zone 一项,开服档起步值 `5000`(**待压测确认**;单一来源是 `k8s_deploy.ps1` 的 `$script:PeakShavingLaunchTier.LoginQueueZoneCapacity`)。
  每个 zone 的 login 只出队本 zone,用不到别的 zone 的值。必须配:不配会落到 `max(online, 1) × SoftCapMultiplier` 的回落公式,代码里是向下取整
  (`loginqueue/capacity.go`),在线 0 / 1 人时容量只有 1,队列非空后可能长期放不出人。它是**同时在线上限**,不是速率:对已在运行的 zone 首次打开排队前,
  先核对该 zone 当前在线(各 gate 自报的 `player_count` 之和)低于这个值,否则新登录全部入队,直到在线降下来。
- `Secrets.QueueToken.Value` = 环境变量 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`(staging / prod 必填:≥32、非占位、与 `MMORPG_GATE_TOKEN_SECRET` / `MMORPG_INTERNAL_AUTH_SECRET` 不同,
  不满足在部署的任何写操作之前拒绝;dev 可不设,此时不写、login 回落 GateToken,见 §6 更正)。写明文 `Value` 而不是 `secrets.go` 建议的 `Env`,与兄弟项 `InternalAuth` 一致。
- 开关、`Queue` 段与排队密钥都进 login Pod 模板的 `mmorpg.io/peak-shaving-hash` 注解,任一变化 login 自动滚动。参数与运维要点见 `deploy/k8s/README.md` Optional Flags「削峰(G4b)」。

> **(2026-10-15 更正,本块原文保留)** 上面「staging / prod 写」只在登录排队的合入门槛清掉之后成立。`k8s_deploy.ps1` 用脚本常量 `$script:LoginQueueMergeGateCleared`(当前 `$false`)机械拦截:门槛未清时 `-LoginQueue auto` 在 dev / staging / prod 都解析为关,不写本段与 `Secrets.QueueToken`,也不要求 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`(非 dev 档部署 login 时打 `[PeakShaving]LoginQueueGate=closed` 警告);`-LoginQueue on` 在参数解析后、任何 kubectl 之前拒绝。三项门槛(G1b 多 zone 用例跑绿、G4b-G 跑绿、`spec_G4b-G.md` §8.4 真实 Redis 冒烟记进 `docs/PROGRESS.md`)见 `docs/handoff/launch-readiness-20261013/spec_G4b.md` §7「合入门槛」与 `deploy/k8s/README.md`「登录排队的合入门槛」;§14 第 8 条的「任何环境都不打开排队」就是由这道门禁执行的。

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
| q_admitted 增速 | 上界是在线玩家的离开速率,加上拿了票没留在线上的那部分名额每约 22~23s 周转一次(§5「放行节奏的上界」;交票后宽限 20s,10-15 前按 15s 是约 17s;数字待 L-14 标定) | `在线 + login_queue_admitted_inflight ≥ capacity` 时不涨是正常回压;停滞且同时满足 `在线 + 在途 < capacity` 才判 dispatcher 卡死 |
| q_expired / q_entered | < 1% | > 5% → AdmitTTL 太短或客户端轮询慢 |
| q_avg_wait | < 60s(待 L-14 标定:放行速率受上一行的上界约束,不是固定的 admit 速率) | > 5min → dispatcher 落后 EnterGame |
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
6. **取走后再轮询回「排队中、名次 0」**(2026-10-13 登记,G4b-G 不含):同一令牌取走之后再轮询,meta 与 token 索引还在、ZRANK 落空,回 QUEUEING、rank 0,直到 1h 过期;回包丢失时这张 gate 票找不回来。Java 版回 410。这是 `PARITY.md`「登录排队」行 ③ 的另一句(`spec_G4b-G.md` §5.8 第 1 条)。
   - **(2026-10-15 更正:已修;已落码,未编译、未测试,待 Codex 验证)** 取走之后再查,以及放行槽没人来取、按 `AdmitTTL` 过期之后再查,都回 EXPIRED(gateway 410,原因 `admit_window_passed`),判据是 meta 里的 `admitted_at`,见 §5 决策树后的更正。回包丢失时 gate 票仍找不回来,客户端从队尾重新排。PopAdmit 写放行槽之前丢掉的条目仍停在名次 0 直到 meta 过期(第 7 条)。
7. **`PopAdmit` 不是单段 Lua**(2026-10-13 登记,既有):「FreeSlots 读占位数 → ZPOPMIN → 逐条写占位」是多次往返,一批把队列弹空之后、占位逐条写完之前,快速通道看到「队列空、占位还没写全」可以再占进来,任一时刻在途 ≤ 2 ×(capacity − online)。`pickFn` 返回 `(nil, nil)`、Marshal 失败、MULTI 失败时条目丢失(G1b 规格 §4.5 第 1 条)。Java 版把弹出、算空位、写占位放在一段 Lua 里(`spec_G4b-G.md` §5.4、§5.8 第 2 条)。
8. **在途占位脚本只有 miniredis 上的证据**(2026-10-15 登记):G4b-G 的计数、快速通道与 `admittedHoldScript` 三段脚本(含 10-15 的条件续期、MULTI 里的 `EVAL`)只有 miniredis 单测覆盖(单测本身也还待 Codex 执行),没有任何真实 Redis 上的运行证据。负责人 2026-10-15 定为**硬门槛**:G4b-2 打开排队之前,`spec_G4b-G.md` §8.4 的最小冒烟必须在真实 Redis 7.x 上跑通并记进 `docs/PROGRESS.md`,否则任何环境都不打开排队(`spec_G4b.md` §7 合入门槛)。
   - **(2026-10-15 补注,第 7、8 条;已落码,未编译、未测试)** 第 7 条:写放行改为单段脚本 `admitEntryScript`(见 §5 决策树后的补注),弹出之后的写入用 `WithoutCancel`,看到取消之后再给 3s 预算(没有取消时不截断),停服或丢租约不再丢整批;写入出错每批一行 ERROR。弹出本身仍不在这段脚本里,本条「不是单段 Lua」的其余结论(快速通道可再占进来、在途 ≤ 2×)不变。仍丢条目的路径:取消之后超出预算、Redis 出错、强杀、`pickFn` 回 `(nil, nil)`、Marshal 失败。其中 **noeviction 写满**会让每拍弹出的整批全部写失败、几拍之内把队列弹空、所有人停在名次 0 约 1h:`ZPOPMIN` 不带 DENYOOM 照常成功,写入脚本被拒。根治要把弹出与写入合成一段 Lua、用 `#!lua` 声明 flags 且不带 `allow-oom`(Redis ≥ 7,miniredis 不支持,只能在真实 Redis 上验证),**待负责人定**;上线前至少给 redis-queue 配内存水位告警。另:锁续期单次失败不会触发丢租约(`onLost` 只在超过 fence 窗口或属主已换人时调用)。第 8 条:真实 Redis 冒烟改为覆盖 `admitEntryScript`(直接 `EVALSHA` / `EVAL`,不再是 MULTI 里的 `EVAL`)。
9. **排队超过 `QueueEntryTTL` 会被弹回队尾**(2026-10-15 登记,既有,**待负责人定**):meta、token 索引、排队令牌只在 `Enqueue` 时设一次 `QueueEntryTTL`(1h),等待中不续期。等待超过 1h 的玩家下一次查询回 EXPIRED(handler 在 `Lookup` 之前先校验排队令牌的 HMAC 过期时刻,令牌与 meta 同时到期,所以原因通常是 `ttl`,只有令牌按秒取整多出的不到 1s 内才是 `meta_gone`;监控看 `login_queue_expired_total{reason="ttl"}`),丢 token 从队尾重排;他原来的队列成员留在 ZSET 里,轮到时被 `admitEntryScript` 按 meta 不在丢弃(不再白占名额)。开服首日若等待可能超过 1h,要么调大 `QueueEntryTTL`,要么在查询时续期 meta / token 索引 / 令牌(需同步改令牌过期语义,Java 版对齐),由负责人决定。

---

## 15. 故障应急

**关闭排队(秒级回滚)**:`login.yaml` 改 `Queue.Enabled: false`,所有玩家立刻走 fast path。

**K8s 上关闭排队(G4b 止血,2026-10-14;已落码,未编译、未测试,待 Codex 验证)**:不要手改 ConfigMap(进程只在启动时读,Pod 模板不变就不会滚动),也不要 `kubectl rollout undo`
(它只退 Pod 模板、不退 ConfigMap,排队照旧开着)。直接调 `k8s_deploy.ps1` 重跑该 zone 的部署,其余参数与上一次部署一致,加上 `-LoginQueue off`:

```powershell
# 先加 -DryRun 核对 "PeakShaving:" 那一行是 login_queue=off,再去掉 -DryRun 执行;完整参数说明见 deploy/k8s/README.md Optional Flags「削峰(G4b)」
pwsh -File tools/scripts/k8s_deploy.ps1 -Command zone-up -ZoneName <zone> -ZoneId <id> -ReleaseProfile prod `
  <该 zone 上一次部署的其余参数照传(镜像 tag 显式给线上那一版)> -LoginQueue off
```

- `dev_tools.ps1`、`k8s_image.ps1`、`k8s_zone_rollback.ps1` **不透传** `-LoginQueue`,包装入口上找不到这个参数。
- 关闭后 login Pod 模板上的 `mmorpg.io/peak-shaving-hash` 注解消失,login 自动滚动;所以 K8s 上的「秒级」是一次 login 滚动的时间。新 Pod 起来后,在排玩家轮询
  `/api/queue-status` 拿到 EXPIRED(gateway 回 410),丢掉 token 重走 `/api/assign-gate` 快速通道。关排队时部署不要求 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`。
- 参数不粘滞:下一次部署(含回档 Step 6、合服后的 zone-up)不传 `-LoginQueue off`,staging / prod 就按档位恢复为开。原因排除前每次部署都要照传。
  - **(2026-10-15 更正,上一条原文保留)** 「按档位恢复为开」只在登录排队的合入门槛清掉之后成立:门槛未清时(`k8s_deploy.ps1` 的 `$script:LoginQueueMergeGateCleared` 为 `$false`),不传 `-LoginQueue` 在任何档位都是关,`-LoginQueue on` 被拒,本节的止血场景也就不会出现。见 §10 末尾的更正与 `docs/handoff/launch-readiness-20261013/spec_G4b.md` §7「合入门槛」。
- 遗留的 `queue:*` / `admit*` 键按 TTL 自然过期,要立即清理见下面「清空堆积」。K8s 上锁键是 `dispatcher:lock:login_queue:zone:<id>:zone:<id>`(§10),
  下面命令里的锁键在 K8s 上要换成它。

**清空堆积**:

> 多 zone 共用 Redis 时,下面后三行(`admit:*` / `queue:meta:*` / `queue:token:*`)不分 zone,会清掉**全部 zone** 的放行槽和排队条目;只想清一个 zone 时只执行第一行,其余键按 TTL(60s / 1h)自然过期。

```
redis-cli DEL queue:zone:1 admitted:zset:zone:1
redis-cli --scan --pattern 'admit:*'        | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:meta:*'   | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:token:*'  | xargs -r redis-cli DEL
# leader 锁一般不用删(30s 自然过期);确需强制换主:
redis-cli DEL dispatcher:lock:login_queue:zone:1
# 合服后源 zone S 的队列残留会在 QueueEntryTTL(1h)内自行过期;要立即清:
redis-cli DEL queue:zone:<S> admitted:zset:zone:<S> dispatcher:lock:login_queue:zone:<S>
# 旧键 admitted:zone:* 是 2026-10-13(G4b-G)之前版本的残留,最后一个旧版本停写后 60s 内自行过期,不用删
```

玩家自动重试 `/api/assign-gate` → 重新入队(若 Enabled=true)或直接进游戏(若 Enabled=false)。
