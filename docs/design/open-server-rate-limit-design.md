# 开服登录削峰设计 (Gateway 限流 + 排队)

**Date:** 2026-05-08
**Task:** #10
**Owner:** Java Gateway (`java/gateway_node/`)
**目标:** 防开服 5 万人同时点登录打爆 go-zero login + DB

---

## 一、为什么放在 Gateway 这一层

| 候选位置 | 优点 | 缺点 | 结论 |
|---|---|---|---|
| 客户端 SDK | 减少入站流量 | 客户端不可信,容易绕过 | ❌ |
| **Java Gateway `/api/assign-gate`** | HTTP 短连、Bucket4j 生态成熟、削峰失败不影响游戏中玩家 | 多 Gateway 实例需分布式协调 → Redis | ✅ |
| go-zero login | 已有 RedisLocker | login 已经是被保护对象,不该再背限流职责 | ❌ |
| C++ gate 入口 | 离玩家最近 | gate 不该感知账号策略 | ❌ |

**关键洞察:** `/api/assign-gate` 是登录链路的**第一个 HTTP 入口**,削峰在这里失败,玩家看到的是排队 UI,不会冲击下游。

---

## 二、限流策略(三层叠加)

```
┌─────────────────────────────────────────────────────┐
│ Layer 1: 全局令牌桶 (zone 维度)                       │
│   Bucket4j + Redis,每 zone 独立配额                   │
│   防止单 zone 风暴扩散到整服                           │
├─────────────────────────────────────────────────────┤
│ Layer 2: IP 限流 (反爬/防刷)                         │
│   单 IP 每分钟 N 次 /api/assign-gate                  │
│   防止脚本/单机器人刷登录                              │
├─────────────────────────────────────────────────────┤
│ Layer 3: 账号串行化 (重复登录)                        │
│   account → Redis lock,同账号 5s 内只能调一次         │
│   防止客户端连点 / 重试风暴                            │
└─────────────────────────────────────────────────────┘
        ↓ 全部通过
   /api/assign-gate 正常处理
```

### Layer 1 配额参考(可调)

| zone 状态 | tokens/sec | burst |
|---|---|---|
| 普通运营 | 500 | 1000 |
| **开服日 / 大版本回归** | **2000** | **5000** |
| 维护后开服(分批) | 200(每 30s 涨一档) | 500 |

go-zero login 单实例容量 ~1.5万 QPS,2 实例 = 3万。Gateway 限到 2000/s 留 15x 缓冲。

---

## 三、客户端友好的排队语义

### 接口契约

```http
POST /api/assign-gate
{ "zone_id": 1, "account": "u_123" }
```

**正常返回:**
```json
{
  "code": 0,
  "gate_ip": "10.0.1.7",
  "gate_port": 9000,
  "token": "...",
  "deadline": 1736300000
}
```

**排队返回(限流命中):**
```json
{
  "code": 100,                  // QUEUEING
  "queue_pos": 12345,
  "retry_after_ms": 3000,
  "wait_estimate_sec": 60       // 预计等待
}
```

**硬封顶(超 5 分钟仍排队):**
```json
{
  "code": 101,                  // QUEUE_TIMEOUT
  "message": "服务器繁忙,请稍后重试"
}
```

### 客户端行为

```
Loop:
  POST /api/assign-gate
  if code == 0      → 拿 token,连 gate(原流程)
  if code == 100    → UI 显示"排队中,前面 N 人",等 retry_after_ms 重试
  if code == 101    → UI 显示"服务器繁忙",指数退避(30s/60s/120s)
  if code >= 500    → 重试 + 上报埋点
```

**重要:** `queue_pos` 不能是真实位次(开销太大),用 token 桶可用 token 数 / 平均消耗速率估算即可,玩家感觉"在前进"就行。

---

## 四、按 zone 分批放人(开服特化)

普通限流应付不了"开服 0 秒所有人同时点登录"。**额外加波次控制**:

```yaml
# application.yml
gate:
  open-server-wave:
    enabled: true
    schedule:
      - { offsetSec:    0, allowZones: [1, 2] }
      - { offsetSec:   30, allowZones: [3, 4] }
      - { offsetSec:   60, allowZones: [5, 6, 7, 8] }
      - { offsetSec:  120, allowZones: ALL }
```

`allowZones` 之外的请求直接返回 `QUEUEING` + 估算等待时间,客户端 UI 显示"X 区将在 mm:ss 后开放"。

运营可在管理后台动态调整 schedule,无需改代码。

---

## 五、Bucket4j + Redis 实施(最小代码)

### 5.1 pom 新依赖

```xml
<dependency>
    <groupId>com.bucket4j</groupId>
    <artifactId>bucket4j_jdk17-redis</artifactId>
    <version>8.10.1</version>
</dependency>
<dependency>
    <groupId>com.bucket4j</groupId>
    <artifactId>bucket4j_jdk17-lettuce</artifactId>
    <version>8.10.1</version>
</dependency>
```

### 5.2 配置

```yaml
gate:
  rate-limit:
    enabled: true
    zone-default-rps: 500
    zone-default-burst: 1000
    ip-rps: 2
    ip-burst: 10
    account-cooldown-ms: 5000
    queue-timeout-ms: 300000   # 5 分钟硬封顶
```

### 5.3 拦截器骨架

```java
@Component
public class AssignGateRateLimiter {
    // Bucket key 设计
    //   zone:{zoneId}     → Layer 1
    //   ip:{clientIp}     → Layer 2
    //   acc:{account}     → Layer 3 (cooldown)

    public Decision check(int zoneId, String clientIp, String account) {
        // 1. 波次控制
        if (!waveSchedule.isOpen(zoneId)) {
            return Decision.queue(waveSchedule.openInSec(zoneId) * 1000);
        }
        // 2. zone 限流
        if (!zoneBucket(zoneId).tryConsume(1)) {
            return Decision.queue(estimateWaitMs(zoneId));
        }
        // 3. ip 限流
        if (!ipBucket(clientIp).tryConsume(1)) {
            return Decision.deny("IP_RATE_LIMIT");
        }
        // 4. 账号 cooldown
        if (!accountCooldown.tryAcquire(account)) {
            return Decision.deny("ACCOUNT_COOLDOWN");
        }
        return Decision.pass();
    }
}
```

### 5.4 接入点

```java
@PostMapping("/assign-gate")
public AssignGateResponse assignGate(@RequestBody AssignGateRequest req,
                                     HttpServletRequest http) {
    var decision = rateLimiter.check(req.zoneId(),
                                     extractIp(http),
                                     req.account());
    if (decision.queueing()) {
        return AssignGateResponse.queueing(decision.posEstimate(),
                                           decision.retryAfterMs());
    }
    if (decision.denied()) {
        throw new RateLimitedException(decision.reason());
    }
    return assignGateService.assignGate(req);  // 原流程
}
```

---

## 六、监控指标(Prometheus)

| Metric | 类型 | 标签 | 用途 |
|---|---|---|---|
| `gate_assign_total` | Counter | zone, result | 总请求数 / 成功 / 排队 / 拒绝 |
| `gate_assign_queue_depth` | Gauge | zone | 当前排队深度 |
| `gate_assign_latency_ms` | Histogram | zone, result | 端到端延迟 |
| `gate_assign_bucket_tokens` | Gauge | zone | 桶剩余令牌(可视化) |
| `gate_assign_wave_open` | Gauge | zone | 波次是否开放 (0/1) |

Grafana 面板要画的图:
1. 各 zone 的 RPS / 排队深度叠加
2. 波次开启时间线 + 实际登录峰值对比
3. p99 延迟在限流前后对比

---

## 七、灰度与故障降级

### 灰度
- 配置 `gate.rate-limit.enabled=false` 一键关闭
- 单 zone 测:`zone-overrides: { 1: 100 }` 给 zone 1 设 100 rps 验证

### 降级(限流系统自身故障)
- Redis 不可用 → fail-open(放过)+ 告警(避免限流系统反向打爆登录)
- Bucket4j 异常 → 同上

```java
try {
    if (!zoneBucket(zoneId).tryConsume(1)) { ... }
} catch (Exception e) {
    log.warn("rate-limit fail-open", e);
    // 继续放行
}
```

---

## 八、与现有架构的衔接

- **不替换**任何现有组件,纯**前置拦截**
- `RedisLocker`(go-zero login 内)继续工作,作为最后一道防线
- `KeyOrderedKafkaProducer` 不变
- 与双 token 系统正交:Refresh token 走 `/api/refresh-token`(不在本限流范围,因为它本身就是低频)

---

## 九、容量推演

```
开服峰值 5 万玩家 / 5 分钟入完
→ 平均 167 RPS 不限流也扛得住
→ 真实风险是 0~30s 内挤进来 30000 人 (1000 RPS)

zone 级配额 2000 RPS:
→ 30000 玩家 / 2000 RPS = 15 秒消化完
→ 客户端 UI 平均显示 7.5 秒排队
→ go-zero login 无压力(2000 << 1.5万 单实例上限)
```

---

## 十、实施步骤

1. ✅ 写本文档
2. ⬜ 加 Bucket4j 依赖到 `pom.xml`
3. ⬜ 实现 `AssignGateRateLimiter` + `WaveSchedule`
4. ⬜ 改 `AssignGateController` 接入
5. ⬜ 加 Prometheus 指标
6. ⬜ 单元测试(模拟 Redis 故障 fail-open)
7. ⬜ 联调:机器人 5000 并发模拟,验证排队 → 正常 → 全通过
8. ⬜ 灰度配置上线,默认关闭
9. ⬜ 单 zone 灰度开启,观察 24 小时
10. ⬜ 全量开启 + 配置开服波次

---

## 关联
- [docs/design/ARCH.md](./ARCH.md) §8 削峰
- [docs/design/architecture-current-state-vs-gaps-2026-05.md](./architecture-current-state-vs-gaps-2026-05.md) 缺口 #10
- [docs/design/login-simplification-2026-04.md](./login-simplification-2026-04.md) RedisLocker(后端兜底)
- [docs/design/gate-load-balancing-design.md](./gate-load-balancing-design.md) AssignGate 上下文
- 代码: `java/gateway_node/src/main/java/com/game/gateway/controller/AssignGateController.java`
- 代码: `java/gateway_node/src/main/java/com/game/gateway/service/AssignGateService.java`
