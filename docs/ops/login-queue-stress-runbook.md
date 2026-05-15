# 登录排队压测 — 判读清单

> 配合 `robot/main.go` 的 queue-aware 模式 + `etc/login.yaml` 的 `Queue.Enabled=true`
> 使用。robot 的 `[stats ...]` 周期日志会持续输出关键指标。

---

## 起步:一个有意义的压测场景

```yaml
# robot/etc/robot.yaml
RobotCount: 5000
GatewayAddr: "http://127.0.0.1:8080"
UseHttpLogin: true
ZoneID: 1
# 用 1000 robot 当 baseline,5000 robot 触发排队
```

```yaml
# go/login/etc/login.yaml
Queue:
  Enabled: true
  DispatchInterval: 1s
  AdmitTTL: 60s
  DefaultRetryAfterMs: 2000
  SoftCapMultiplier: 1.5
  ZoneCapacityOverride:
    "1": 1000   # 强制 1000,5000 robot 一定会触发排队
```

启动顺序:Redis → etcd → login → gate → Java Gateway → robot。

---

## 核心健康指标(看 robot 的 `[stats ...]` 日志)

| 指标 | 健康范围 | 红线含义 |
|---|---|---|
| `q_entered` | 4000±200(总数 5000 − 容量 1000) | 远低于 4000 = 容量计算错了/排队判定没触发 |
| `q_admitted` | 单调上升,最终 → q_entered | 上升停滞 = dispatcher 卡死 |
| `q_expired` | < 1% of q_entered | > 5% = AdmitTTL 太短 OR 客户端轮询太慢 |
| `q_avg_wait` | < 60s(1000/s admit 速率下) | > 5min = dispatcher 严重落后 EnterGame |
| `q_max_rank` | 单调下降到 0 | 长时间不变 = 该 robot 卡住,看 Redis 检查 |
| `login_ok` 增速 | 接近 zone capacity / login_avg | login_ok 不涨但 q_admitted 涨 = gate 链路坏 |
| `recon_fb` | 接近 0 | > 5% = 重连豁免判定有 bug,看 player_locator 日志 |

### 周期 report 示例(健康状态)

```
[stats 30s] conn=5000 login_ok=920 login_fail=12 ... q_entered=4080 q_admitted=920 q_expired=3 q_avg_wait=28.4s q_max_rank=4078
[stats 60s] conn=5000 login_ok=1980 login_fail=15 ... q_entered=4080 q_admitted=1980 q_expired=5 q_avg_wait=31.2s q_max_rank=4078
[stats 90s] conn=5000 login_ok=3050 login_fail=18 ... q_entered=4080 q_admitted=3050 q_expired=7 q_avg_wait=33.8s q_max_rank=4078
```

**关键观察**:`q_admitted` 每 30s 涨 1000±10(zone capacity);`q_avg_wait` 稳定在 30s 附近(等于排队的平均位置 × 1s 出队节奏);`q_max_rank` 在第一波入队后就固定不变(没有新的"插队")。

---

## 异常模式速查

### 模式 1:dispatcher 卡死
```
[stats 30s] ... q_entered=4080 q_admitted=20 q_avg_wait=22s q_max_rank=4078
[stats 60s] ... q_entered=4080 q_admitted=20 q_avg_wait=22s q_max_rank=4078  ← admitted 不涨
```
**排查**:
- `redis-cli ZCARD queue:zone:1` → 应该单调下降。不降 = ZPOPMIN 没在跑
- 查 login 日志 `[loginqueue] elected dispatcher leader` 是否存在 + `[loginqueue] zone=1 admitted=N` 是否持续
- 无 leader 日志 = `dispatcher:lock:login_queue` 抢锁失败,看 Redis 连接
- 有 leader 但无 admitted 日志 = `FreeSlots` 永远返回 0,查 `GateWatcher.FetchAllNodes()` 和 ZoneCapacityOverride

### 模式 2:over-issue(超发 gate token)
```
[stats 30s] ... q_admitted=2000  (期望 920)
gate 日志:VerifyGateToken 突然出现大量 token 过期 / 玩家被踢
```
**根因**:`admitted:zone:{id}` SET 在 SREM 后没及时减少,FreeSlots 算少了
**排查**:
- `redis-cli SCARD admitted:zone:1` → 应等于"已收 admit 但还没 connect 的玩家数",通常 < 50
- 持续 > 200 = `consumeAdmit` 的 SREM 路径有 bug,或者客户端拿了 admit 没去连 gate

### 模式 3:dispatcher 双 leader(双副本同时出队)
```
两台 login 都打印 [loginqueue] elected dispatcher leader
admitted 短时间内突增超过 freeSlots 上限
```
**排查**:
- `redis-cli GET dispatcher:lock:login_queue` 应该只有一个 owner UUID
- 看 heartbeat 日志:lost leadership 后是不是没有真停掉 drainLoop

### 模式 4:queue token 过期风暴
```
[stats 60s] ... q_expired=400 (期望 < 50)
```
**根因**:`AdmitTTL=60s` 但客户端轮询间隔 + 网络抖动 > 60s
**排查**:
- 客户端日志看实际轮询间隔
- 调大 `AdmitTTL` 到 120s,看 q_expired 是否回落
- 不调 AdmitTTL,看是不是网络层 RTT 异常

### 模式 5:重连玩家被排队(豁免失效)
```
[stats 30s] ... q_entered=4500 (期望 4080)
玩家手感:断网重连也要排队 → 投诉
```
**排查**:
- AssignGate 进来的请求带 `account` 字段了吗?Java Gateway 透传了吗?
- `player_locator.GetSession()` 返回 GateID 非空才豁免;手动查一个重连账号的 PlayerSession.State

### 模式 6:Redis QPS 打爆
```
Redis 监控:queries/sec > 50k,p99 命令延迟 > 50ms
```
**根因**:`DispatchInterval=1s` × 20 个 zone × (ZCARD + SCARD + ZPOPMIN + HGET + N×SET + N×SADD)
**估算**:1 个 zone 每秒 ~5 个 Redis 命令(空闲)+ N 个(N=admit 批量),20 zone 满载约 2000 QPS。**远低于** Redis 上限,如果打爆说明客户端轮询太频(QueryQueueStatus 的 GETDEL + HGET + ZRANK = 每个 robot 每 2s 3 命令 × 4000 robot = 6000 QPS,可以接受)
**排查**:redis MONITOR 抽样 100 个命令,看哪类命令占比最大,反推哪层在打

### 模式 7:gate ephemeral port 耗尽回归
```
gate 机器 sysctl net.ipv4.ip_local_port_range 内 TIME_WAIT 占满
```
**这是 CLAUDE.md §7 提的老问题,排队层不应该引入新风险**,但要确认:
- queue-aware robot 是否在 admit 后立刻 close → 重连?(我的实现:admit 后建 TCP 连接,close 在 robot 完整 session 结束才发生,所以这条不应该被排队恶化)
- 看 `ss -s` 的 TIME_WAIT 计数,对比开排队前后

---

## 验证清单(每次改 loginqueue 都跑一遍)

### 1. 单元 + 集成测试都过
```bash
cd go/login && go test ./internal/logic/pkg/loginqueue/... -count=1
# 期望:11 个 PASS,~1s
```

### 2. 单 robot 冒烟(zone 不饱和)
```bash
# RobotCount=5, ZoneCapacityOverride: "1": 100
# 期望:5 个 robot 全部 fast-path 直接进,q_entered=0
```

### 3. 多 robot 冒烟(zone 饱和)
```bash
# RobotCount=200, ZoneCapacityOverride: "1": 100
# 期望:前 100 fast-path,后 100 q_entered=100, q_admitted 在 100s 内涨到 100
```

### 4. dispatcher 杀掉再起(故障演练)
```bash
# 跑 5000 robot,排队中途 kill -9 当前 leader login pod
# 期望:30s 内备份接管,q_admitted 短暂停顿后继续上涨,q_avg_wait 略升但无饿死
```

### 5. Redis 重启(灾难演练)
```bash
# 跑 5000 robot,中途 redis-cli SHUTDOWN NOSAVE
# 期望:
#   - dispatcher 失 leader → 自动退出
#   - 客户端 QueryQueueStatus 报错 → robot fall through 到 outer retry
#   - Redis 起来后,所有数据丢失(预期),robot 触发新一轮 AssignGate 重建队列
```

---

## Grafana / Prometheus 指标(已接入)

login 进程通过 go-zero 内置 `/metrics` endpoint(`login.yaml` 第 119 行 `Prometheus` 配置块)暴露下列 6 个指标:

| 指标 | 类型 | 标签 | 用途 |
|---|---|---|---|
| `login_queue_size` | Gauge | zone | ZCARD 实时长度,dispatcher 每 tick 刷 |
| `login_queue_admitted_inflight` | Gauge | zone | SCARD admitted set,容量计算的 in-flight 项 |
| `login_queue_wait_seconds` | Histogram | zone | 入队到 admit 的端到端等待(buckets: 0.5/1/2/5/10/20/30/60/120/300/600s)|
| `login_queue_admit_total` | Counter | zone | dispatcher 累计 admit 数,`rate(...[1m])` = drain QPS |
| `login_queue_expired_total` | Counter | zone, reason | reason ∈ `bad_signature` / `ttl` / `meta_gone` / `queue_disabled` |
| `login_queue_dispatcher_is_leader` | Gauge | pod | 1=leader,0=standby;`sum(...) == 1` 是稳态不变量 |

### 推荐告警(PromQL)

```promql
# dispatcher 没有 leader (Redis flap / 全副本挂)
sum(login_queue_dispatcher_is_leader) != 1

# 队列在涨但 admit 不涨(dispatcher 卡死)
deriv(login_queue_size[5m]) > 0 and rate(login_queue_admit_total[5m]) == 0

# p99 等待超过 5 分钟
histogram_quantile(0.99, rate(login_queue_wait_seconds_bucket[5m])) > 300

# 签名失败激增(可能有人在压测/攻击)
rate(login_queue_expired_total{reason="bad_signature"}[5m]) > 10
```

### Grafana 看板布局建议

- **Top row** — `login_queue_size` 按 zone 折线 + `login_queue_dispatcher_is_leader` 按 pod stat panel
- **Mid row** — `rate(login_queue_admit_total[1m])` 按 zone + `login_queue_admitted_inflight` 按 zone
- **Bottom row** — `histogram_quantile(p50/p95/p99, login_queue_wait_seconds_bucket)` 按 zone + `rate(login_queue_expired_total[5m])` 按 reason 堆叠

> robot 的 `[stats] q_*` 日志仍然有用——它是**单 robot 视角**,Prometheus 是**服务端视角**。两者互补:robot 看"我等了多久",Prometheus 看"系统整体在做什么"。

---

## 已知不足(本期不修)

1. **排队 token 不能转移**:玩家换设备重登会拿到新 queueToken,丢掉位置。生产可接受,因为玩家通常不会排队中换设备。
2. **多 zone 公平性**:dispatcher 按 `activeZones` 顺序遍历,理论上前面的 zone 抢先用完 admit 批次。实际不是问题(每个 zone 独立的 FreeSlots),但极端边界条件待观察。
3. **客户端没有显示 ETA**:只有 `queue_rank` + `queue_total`,前端可以用 `rank / (capacity/sec)` 自己算 ETA。

---

## 故障应急

**立即关闭排队**:`login.yaml` 改 `Queue.Enabled: false`,重启 login pod(或者 hot-reload 如果 go-zero config watcher 已接好)。**所有玩家立刻走 fast path,等同于排队没存在过。**

**清空堆积**:`redis-cli`
```
DEL queue:zone:1
DEL admitted:zone:1
KEYS admit:* | xargs DEL
KEYS queue:meta:* | xargs DEL
KEYS queue:token:* | xargs DEL
```
玩家会自己重试 `/api/assign-gate` 重新入队(如果排队还开着)或直接进游戏(如果 Enabled=false)。
