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
- 查 login 日志 `[loginqueue] elected dispatcher leader zone=<id>` 是否存在 + `[loginqueue] zone=1 admitted=N` 是否持续
- 无 leader 日志 = `dispatcher:lock:login_queue:zone:<id>` 抢锁失败,看 Redis 连接。多 zone 共用 Redis 时每个 zone 各有一把锁(2026-10-12 G1b 起),某个 zone 没有锁键 = 该 zone 没有打开 Queue 的 login 副本
- 有 leader 但无 admitted 日志 = `FreeSlots` 永远返回 0,查 `GateWatcher.FetchAllNodes()` 和 ZoneCapacityOverride
- 有 leader、日志里持续出现 `[loginqueue] freeSlots zone=<id> queueLen=<n> err=no gates for zone` = 有人在排,但本 zone 一台可用 gate 都没有(G1b 之前这种情况是静默跳过的)
- 有 leader、日志里持续出现 `[loginqueue] queueLen zone=<id> err=...` = 读不出队列键(WRONGTYPE / ACL / 单键异常),锁照常续期但每拍都不出队(2026-10-14 之前这种情况零日志)
- 日志里有 `goroutine_panic` 且 `point=login.queue_dispatcher_term`(指标 `safego_panic_total{point="login.queue_dispatcher_term"}` 在涨)= 出队路径有代码缺陷。该任期已放锁,本副本退避 TTL/3 后再参选,同 zone 备机可以接手;每个副本都 panic 就是每轮当选即 panic,队列照样不出,按栈修代码

### 模式 2:over-issue(超发 gate token)
```
[stats 30s] ... q_admitted=2000  (期望 920)
gate 日志:VerifyGateToken 突然出现大量 token 过期 / 玩家被踢
```
**根因**:`admitted:zone:{id}` SET 在 SREM 后没及时减少,FreeSlots 算少了
**排查**:
- `redis-cli SCARD admitted:zone:1` → 应等于"已收 admit 但还没 connect 的玩家数",通常 < 50
- 持续 > 200 = `consumeAdmit` 的 SREM 路径有 bug,或者客户端拿了 admit 没去连 gate

### 模式 3:dispatcher 双 leader(同一 zone 的双副本同时出队)
```
同一 zone 的两台 login 都打印 [loginqueue] elected dispatcher leader zone=<同一个 id>
admitted 短时间内突增超过 freeSlots 上限
```
G1b(2026-10-12)之后每个 zone 各有一个 leader,**不同 zone 各打印一次是正常的**,只有同一个 zone id 出现两次才是双 leader。

**排查**:
- `redis-cli GET dispatcher:lock:login_queue:zone:1` 应该只有一个 owner UUID。新旧版本混跑窗口里 zone 键和全服旧键 `dispatcher:lock:login_queue` 可能同时存在,属于预期(设计稿 §7.1)
- 同一 zone 的副本配了不同的 `DispatcherLockKey` 也会长期双 leader:`redis-cli --scan --pattern "*:zone:1"` 能看到两把锁
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
# 期望:全部 PASS,耗时数秒(主备切换类用例要等 1s 的备机重试周期)。
#       个数不写死:2026-10-14 为 59 个顶层用例,以后增删用例以 -v 实际列出的为准。
#       G1b 的验收命令与通过标准见 docs/handoff/launch-readiness-20261013/spec_G1b.md §7.2
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
# 跑 5000 robot,排队中途 kill -9 该 zone 当前的 leader login pod
# 期望:约 20~40s 内同 zone 的备份接管(锁剩余 TTL 在 20~30s 之间,心跳每 TTL/3=10s 续到 30s;
#       再加备机重试周期 TTL/3=10s),q_admitted 短暂停顿后继续上涨,q_avg_wait 略升但无饿死;
#       只有这个 zone 的放行暂停(≤ TTL + TTL/3 ≈ 40s),其它 zone 不受影响
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
| `login_queue_dispatcher_is_leader` | Gauge | pod, zone | 1=leader,0=standby;`sum by (zone) (...) == 1` 是稳态不变量(每 zone 一个 leader)。leader 每拍重发 1、备机每轮抢锁失败重发 0(2026-10-14 起),指标开关打开前写丢的样本下一拍就补上。**序列缺失不等于 0**:整 zone 没有副本上报时 `sum by (zone)` 根本不出这一组,发现不了整 zone 缺席,见下方 `absent` 告警 |

### 推荐告警(PromQL)

```promql
# 某个 zone 双 leader,或有副本在上报但没有 leader(Redis flap / 同 zone 锁键配得不一样)。
# 建规则时 for: 1m,盖过 leader 交接窗口:kill -9 时锁剩余 TTL ≤ 30s + 备机重试周期 10s ≈ 40s,
# 优雅停机最长 10s;不带 for 的话每次滚动重启 / 切主都会响。
# 「该 zone 全副本挂」这条发现不了:没有序列时 sum by (zone) 不出这一组,!= 1 永远不命中。
sum by (zone) (login_queue_dispatcher_is_leader) != 1

# 某个开了排队的 zone 一条序列都没有(全副本挂 / 没开排队 / 没抓到),只有这条能报。for: 2m。
# absent 不能按标签自动分组,必须按开了排队的 zone 清单逐个写(下面是 zone 1)。
# 也可以与上一条合成「sum by (zone) (...) > 1」加「absent(login_queue_dispatcher_is_leader{zone="<id>"} == 1)」
# (后者同时覆盖全 0 与无序列),两种写法二选一。都依赖 leader / 备机按拍重发,否则冷启动后会一直报。
absent(login_queue_dispatcher_is_leader{zone="1"})

# 队列在涨但 admit 不涨(dispatcher 卡死)
deriv(login_queue_size[5m]) > 0 and rate(login_queue_admit_total[5m]) == 0

# p99 等待超过 5 分钟
histogram_quantile(0.99, rate(login_queue_wait_seconds_bucket[5m])) > 300

# 签名失败激增(可能有人在压测/攻击)
rate(login_queue_expired_total{reason="bad_signature"}[5m]) > 10
```

### Grafana 看板布局建议

- **Top row** — `login_queue_size` 按 zone 折线 + `login_queue_dispatcher_is_leader` 按 zone、pod 的 stat panel
- **Mid row** — `rate(login_queue_admit_total[1m])` 按 zone + `login_queue_admitted_inflight` 按 zone
- **Bottom row** — `histogram_quantile(p50/p95/p99, login_queue_wait_seconds_bucket)` 按 zone + `rate(login_queue_expired_total[5m])` 按 reason 堆叠

> robot 的 `[stats] q_*` 日志仍然有用——它是**单 robot 视角**,Prometheus 是**服务端视角**。两者互补:robot 看"我等了多久",Prometheus 看"系统整体在做什么"。

---

## 已知不足(本期不修)

1. **排队 token 不能转移**:玩家换设备重登会拿到新 queueToken,丢掉位置。生产可接受,因为玩家通常不会排队中换设备。
2. ~~**多 zone 公平性**:dispatcher 按 `activeZones` 顺序遍历,理论上前面的 zone 抢先用完 admit 批次。~~ 2026-10-12 G1b 起每 zone 一个 leader,不存在 zone 之间抢批次(设计稿 §7.1、§14 第 5 条)。
3. **客户端没有显示 ETA**:只有 `queue_rank` + `queue_total`,前端可以用 `rank / (capacity/sec)` 自己算 ETA。

---

## 故障应急

**立即关闭排队**:`login.yaml` 改 `Queue.Enabled: false`,重启 login pod(或者 hot-reload 如果 go-zero config watcher 已接好)。**所有玩家立刻走 fast path,等同于排队没存在过。**

**清空堆积**(与设计稿 §15 一致):

> 不要用 `KEYS`:它会阻塞全服共用的那个 Redis 主。多 zone 共用 Redis 时,下面后三行(`admit:*` / `queue:meta:*` / `queue:token:*`)不分 zone,会清掉**全部 zone** 的放行槽和排队条目;只想清一个 zone 时只执行第一行,其余键按 TTL(60s / 1h)自然过期。

```
redis-cli DEL queue:zone:1 admitted:zone:1
redis-cli --scan --pattern 'admit:*'        | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:meta:*'   | xargs -r redis-cli DEL
redis-cli --scan --pattern 'queue:token:*'  | xargs -r redis-cli DEL
```
玩家会自己重试 `/api/assign-gate` 重新入队(如果排队还开着)或直接进游戏(如果 Enabled=false)。
