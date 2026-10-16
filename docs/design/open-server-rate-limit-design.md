# 开服登录削峰设计 (Gateway 限流 + 排队)

**Date:** 2026-05-08
**Task:** #10
**Owner:** Java Gateway (`java/gateway_node/`)
**目标:** 防开服 5 万人同时点登录打爆 go-zero login + DB
**修订:** 2026-10-14 G4b — K8s 按发布档位开启(§十一);§5.3 / §5.4 补更正注(冷却键与 `queue_token` 重入的现行实现)

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

> **更正(2026-10-14,按 `java/gateway_node/.../ratelimit/AssignGateRateLimiter.java` 核对;上面的骨架保留,不删)**:骨架里的 `acc:{account}` 与
> `accountCooldown.tryAcquire(account)` 与实现不符。
> - 账号冷却不在 Redis,是**进程内**带过期清理的 `ConcurrentHashMap`,每个 gateway 副本各一份(§二 图里 Layer 3 的「Redis lock」同样不成立)。
> - G4b-J2 之后冷却键 = `<scope>:<account>@<客户端 IP>`,`scope` 按端点分:`/api/assign-gate` 为 `assign`,`/api/login` 为 `login`。`account` 在这一步未经认证,
>   只按账号计时的话,任何人都能替别人的账号触发冷却;键里带 IP 后,这样刷只能挡住同一 IP 上的这个账号。代价:同一账号换网络后不共享冷却窗口。
> - 桶键实为 `rl:zone:{zoneId}` / `rl:ip:{ip}`,`/api/login` 与 `/api/assign-gate` 共用这两个桶。`zoneId = 0` 时跳过 zone 桶。
> - 判定顺序仍是「波次 → zone 桶(先扣)→ IP 桶 → 冷却」:G4b-J1(先 IP 后 zone)未做,见 §11.7 R13。

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

> **更正(2026-10-14,按 `java/gateway_node/.../controller/AssignGateController.java` 核对;上面的骨架保留,不删)**:
> - 拒绝不抛异常:IP 桶耗尽或冷却命中回 HTTP 200、`code=429`(`error` = `IP_RATE_LIMIT` / `ACCOUNT_COOLDOWN`);zone 桶空回 `code=100`、`queue_source="ratelimit"`。
> - G4b-J0 之后,带 `queue_token` 的 assign-gate **照样过 zone 桶与 IP 桶,只跳过账号冷却**(传给 `check` 的 `account` 为 `null`)。此前 `queue_token` 非空就整个跳过限流,
>   而 gateway 验不了 token(密钥在 login),login 排队关着时这类请求又直接走快速通道签 gate 票,等于伪造一个字段就绕过全部限流。冷却仍跳过:网络抖动后的重试常落在 5s 窗口内。

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
8. ⬜ 灰度配置上线,默认关闭 —— **2026-10-14 状态**:jar 内仍缺省关(`application.yaml` 的 `gate.rate-limit.enabled: false`),本地、compose 与 K8s dev 档保持关闭;
   K8s staging / prod 由 G4b 按档位直接打开(§十一),不再「默认关闭」
9. ⬜ 单 zone 灰度开启,观察 24 小时 —— **2026-10-14 状态**:不做,G4b 有意偏离(§11.3 #2、#3):staging / prod 缺省整体开启,
   以 `-GatewayRateLimit off` 秒级止血加指纹自动滚动代替灰度
10. ⬜ 全量开启 + 配置开服波次 —— **2026-10-14 状态**:按档位全量开启已落码(§十一),**未编译、未测试**,待 Codex 验证,阈值待压测确认;
    开服波次不打开(需要开服时刻,属运营决策,§11.3 #8)

---

## 十一、K8s 按发布档位开启(G4b)

> **状态**:已落码,**未编译、未测试**,待 Codex 验证。本节所有数值都是开服档起步值,**待压测确认**(L-14),不能据此下容量结论(AGENTS §6)。
> 规格 `docs/handoff/launch-readiness-20261013/spec_G4b.md`;部署参数、止血命令与运维要点见 `deploy/k8s/README.md` Optional Flags「削峰(G4b)」。
> 负责人 2026-10-13 拍板:G4b-J0 + J2 做、J1 不做(R13 作为已知风险);zone 起步值 2000/s、突发 5000;登录排队的两条基线缺陷(Q1 / Q2)另立 G4b-G 处理,不在本节;
> 其余阈值照规格常量表。

### 11.1 档位矩阵

`tools/scripts/k8s_deploy.ps1` 新增 `-GatewayRateLimit` / `-LoginQueue`,取值 `auto|on|off`,缺省 `auto` = 跟随 `-ReleaseProfile`。

| | dev(缺省 auto) | staging / prod(缺省 auto) | 显式 `on` | 显式 `off` |
|---|---|---|---|---|
| gateway `gate.rate-limit.enabled` 与阈值 | 不写(jar 内缺省关,ConfigMap 与 G4b 之前逐字相同) | 写 `enabled: true` 与开服档阈值 | 写(任何档位) | 不写;非 dev 档打 `[PeakShaving]GatewayRateLimit=off` 警告 |
| gateway `spring.data.redis.password` | `MMORPG_REDIS_PASSWORD` 非空才写(dev 档没设时回落空串,不写) | 写(staging / prod 必须设该变量) | 与开关无关,同前两列 | 与开关无关,同前两列 |
| login `Queue` 段 | 不写 | 写:`Enabled: true`,锁键带本 zone,`ZoneCapacityOverride` 只含本 zone,其余镜像 `go/login/etc/login.yaml` | 写 | 不写;非 dev 档打 `[PeakShaving]LoginQueue=off` 警告 |
| login `Secrets.QueueToken` | 不写 | 写 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`(必填,≥32,非占位,与 GATE / INTERNAL_AUTH 两把不同) | dev 档:环境变量为空就不写(login 宽松档回落 GateToken 并 WARN) | 不写,也不要求该变量 |
| Pod 模板注解 `mmorpg.io/peak-shaving-hash` | 无 | gateway 与 login 各一条 | 有 | 无(开 → 关时模板变化,自动滚动) |
| 带 `queue_token` 的 assign-gate(J0 之后) | 限流关 → 放行 | 过 zone 桶与 IP 桶,只跳过账号冷却 | 同左 | 同 dev |

- 写 Redis 口令的原因:Bucket4j 的 Lettuce 客户端只从 `spring.data.redis.password` 取口令,而 release 档 Redis 带 `--requirepass`。不写时限流过不了认证,
  要么每次调用都落进 catch 一律放行(fail-open,§七),要么 gateway 建连接时就起不来(取决于 Lettuce 的握手行为,未实测)。
- 参数不粘滞(与 `-GateRouterMode` 同一口径):以 `off` 止血后,下一次部署(含 `k8s_zone_rollback.ps1` 的 Step 6)不传就按档位恢复为开。
  `dev_tools.ps1` / `k8s_image.ps1` / `k8s_zone_rollback.ps1` 不透传这两个参数,止血要直接调 `k8s_deploy.ps1`。
- 只改 ConfigMap 不会让进程重读配置,所以开关、阈值、可信代理网段、Redis 口令(gateway)、排队密钥(login)都进 Pod 模板的指纹注解,任一变化即自动滚动。
  不要用 `kubectl rollout undo` 关限流或排队:它只退 Pod 模板,不退 ConfigMap。
- **(2026-10-15 更正,上面的矩阵与各条原文保留)** login 排队受 `k8s_deploy.ps1` 的脚本常量 `$script:LoginQueueMergeGateCleared`(当前 `$false`)机械拦截:
  门槛未清时 `-LoginQueue auto` 在 dev / staging / prod 都解析为关(非 dev 档部署 login 时打 `[PeakShaving]LoginQueueGate=closed` 警告),
  `-LoginQueue on` 在参数解析后、任何 kubectl 之前拒绝;login ConfigMap 不写 `Queue` 段与 `Secrets.QueueToken`,login 不带 `mmorpg.io/peak-shaving-hash`,
  也不要求 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`。矩阵里 login `Queue` 段、`Secrets.QueueToken` 两行的 staging / prod 列与显式 `on` 列,以及上面「参数不粘滞」一条的
  「按档位恢复为开」(就 login 排队而言),都只在门槛清掉之后成立;`-GatewayRateLimit` 不受影响。出处 `docs/handoff/launch-readiness-20261013/spec_G4b.md` §7「合入门槛」,
  放开步骤见 `deploy/k8s/README.md` Optional Flags「削峰(G4b)」的「登录排队的合入门槛」。

### 11.2 起步值与出处

单一来源是 `k8s_deploy.ps1` 的常量表 `$script:PeakShavingLaunchTier`(ConfigMap 生成、Pod 指纹、部署打印都只读它;不设覆盖参数)。全部待压测确认。

| 键 | 起步值 | 出处 |
|---|---|---|
| `zone-default-rps` / `zone-default-burst` | 2000 / 5000 | 本文 §二「Layer 1 配额参考」的「开服日 / 大版本回归」档;负责人 2026-10-13 拍板按该档起步 |
| `ip-rps` / `ip-burst` | 5 / 20 | jar 内 `application.yaml` 的同名键(§5.2 示例为 2/10,偏离见 §11.3 #1) |
| `account-cooldown-ms` | 5000 | §5.2 与 jar 内 `application.yaml` 一致 |
| login `ZoneCapacityOverride.<本 zone>` | 5000 | `login-queue-2026-05.md` §10 与 login.yaml 注释里的示例值。它是**本 zone 同时在线上限,不是速率** |

- 不写 `wave.*`(开服波次要一个开服时刻,属运营决策)、`zone-overrides`,也不写没有消费方的 `queue-timeout-ms`。
- 部署侧自检:rps ≥ 1、burst ≥ rps、冷却非负、排队容量在 1..uint32,不满足即 throw `内部错误:…`。`release_preflight.ps1` 只查 jar 内的值,
  K8s 上真正生效的是 ConfigMap 里这一份,所以同一条规则在部署脚本里再守一次。

### 11.3 与本文前文的偏离(规格 §8)

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| 1 | IP 阈值取 5/20,不取 §5.2 的 2/10 | 本文没有 IP 的开服档;jar 内已是 5/20,`release_preflight` 查的也是它;一次登录要扣 2 个 IP 令牌(`/api/login` 与 `/api/assign-gate` 共用 `rl:ip:*`) | 防刷力度比 §5.2 弱 2.5 倍 |
| 2 | staging / prod 缺省就开,不走 §七「灰度」与 §十 第 8~9 步的「默认关、单 zone 灰度 24h 再开」 | 开服首日 P0 | 没有 canary;以 `off` 秒级止血加指纹自动滚动弥补 |
| 3 | login 排队整个 Deployment 一起开,不按 login.yaml 注释「先开一个 canary Pod」 | K8s 生成器按 Deployment 粒度生成配置 | 同上 |
| 4 | `ZoneCapacityOverride` 所有 zone 统一 5000,不按「观测峰值 × 0.9」逐 zone 设 | 没有任何 zone 的观测峰值 | 可能把一个 zone 的在线卡在 5000 |
| 5 | `Secrets.QueueToken` 写明文 `Value`,不按 `secrets.go` 的建议改用 `Env` | 兄弟项 `InternalAuth` 也是明文 `Value` | 与 InternalAuth 暴露面相同(ConfigMap 可读即可见) |
| 6 | gateway 的 Redis 口令明文进 ConfigMap,不走管理面口令那套 Secret | 同 namespace 的 go-svc ConfigMap 已带同一个明文值,改走 Secret 不缩小暴露面 | 与数据源口令同级 |
| 7 | 新增 Pod 指纹注解:开关、阈值、代理网段、口令、排队密钥一变就滚动 | ConfigMap 只在启动时读,不这样做 `off` 止血在镜像不变时不生效 | 每次调阈值都滚动 gateway / login;排队密钥轮换时在排玩家的 token 失效、重新入队 |
| 8 | 开服波次(§四)不打开 | 需要开服时刻,属运营决策 | 开服 0 秒的整体洪峰只靠令牌桶挡 |
| 9 | J0:带 `queue_token` 的 assign-gate 在排队**开着**时也过 zone / IP 桶(Java 版只在其排队关闭时限流这类请求) | 本仓库的 gateway 与排队分属两个进程,gateway 不知道 login 排队开没开、也验不了 token,只能二选一,选 fail-closed | 网络抖动后带 token 重调 assign-gate 偶尔多扣一个令牌,可能先拿到一次 `ratelimit` 排队;正常轮询走 `/api/queue-status`,不受影响 |

> **#9 的 2026-10-15 更正(代价漏算 429 与 AdmitTTL;上表原文保留)**:上表「代价」只算了多扣一个令牌、先拿到一次 `ratelimit` 排队。
> 按当时的客户端口径(§11.5 原第 2、4 条:带 token 重调 assign-gate,吃 `code=429` 时按 30/60/120s 退避),CGNAT / 网吧后的玩家连吃两次 429 就是 90s,
> 超过 login 的 `Queue.AdmitTTL` 60s。已被放行的玩家放行槽随之过期,此前 login 对这枚 token 一直回「排队中、名次 0」,最长约 1h(`QueueEntryTTL`)。
> **负责人 2026-10-15 按「最标准」定**:J0 的 fail-closed 取舍与服务端回包格式都不变,改客户端契约。持有 `queue_token` 一律轮询 `/api/queue-status`(不过限流器);
> 带 token 重调 assign-gate 只作为 queue-status 本身失败后的恢复手段;重入被限流就回到 queue-status 轮询。逐条见 §11.5 的 2026-10-15 更正。
> login 侧同批改为放行窗口过后对该 token 回 EXPIRED(gateway 回 410;另一项改动,落在 `go/login/internal/logic/pkg/loginqueue/queue.go`,本注未核对其实现)。
> 所以仍按旧口径退避重调的客户端,窗口过后也会拿到确定的 410、重新入队,不会再卡在名次 0 一小时。链条与依据见规格 `spec_G4b.md` §4.10 J0「客户端契约」与 §8 #10 的 2026-10-15 注。

### 11.4 gateway 代码改动(G4b-J)

三条都是限流关着时不可达、一打开就生效的问题。

- **J0(已落码)**:`AssignGateController` 不再因 `queue_token` 非空而整个跳过限流。带 token 的重入照样过 zone 桶与 IP 桶,只跳过账号冷却(传给 `check` 的 `account` 为 `null`)。
  此前 login 排队关着时,带任意非空 `queue_token` 的请求绕过整个 Bucket4j,并直接走快速通道拿到 gate 票。回包码集合不变。测试 `AssignGateControllerTest`(纯 Mockito)。
- **J2(已落码)**:账号冷却键 = `<scope>:<account>@<客户端 IP>`,仍是进程内 Map、每个副本各一份。`account` 未经认证,只按账号计时的话,任何人每隔一个冷却窗口替别人的账号调一次接口,
  对方在同一副本上就一直拿 429 `ACCOUNT_COOLDOWN`。测试 `AssignGateRateLimiterTest.accountCooldown_isPerAccountAndIp`。
- **J1(不做)**:判定顺序仍是「波次 → zone 桶(先扣)→ IP 桶 → 冷却」,见 §11.7 R13。

### 11.5 客户端退避要求(另仓 `../mmorpg-client`,本仓库只登记)

- `code=100`:按 `retry_after_ms` 加随机抖动重试(值为 0 时取 2s);
- `429`:按 30/60/120s 指数退避并加抖动;
- `410`:丢掉 `queue_token`,重新调 assign-gate;
- (J0 之后)带 `queue_token` 重调 assign-gate 拿到 `code=100, queue_source="ratelimit"`(不带新 token)时,保留原 token,按 `retry_after_ms` 重试,不要当成排位丢失。

客户端契约(回包码集合、帧、proto、tip 码)不变。独立客户端是否已按上述口径退避:未核实(另仓,本批未读取)。

> **(2026-10-15 更正:持有 `queue_token` 时的口径。负责人 2026-10-15 按「最标准」定;上面四条原文保留)**
> 原则:状态查询走专用的状态端点,入口端点只管入场;限流不该让已被放行的玩家错过 login 的放行窗口(`AdmitTTL` 60s)。
> 按上面第 2、4 条,持 token 的玩家连吃两次 429 就退避 90s,窗口必然错过(§11.3 #9 更正)。
> 上面第 2 条(429 按 30/60/120s 退避)从此只适用于**不持有** `queue_token` 的请求;第 4 条(带 token 拿到 `ratelimit` 排队时重调 assign-gate)作废。
> 本节第 1 条与 §三「客户端行为」对持 token 的客户端同样按下面执行。现行口径:
>
> - 持有 `queue_token` 时,一律用 `/api/queue-status` 轮询排位与取票,间隔取 `retry_after_ms`,为 0 时取 2s。该端点不过限流器(`QueueStatusController`)。
>   排队开着时它与 assign-gate 重入走同一个 `Lookup`,能取走放行票;排队关着时回 EXPIRED → `410`。
> - 带 token 重调 `/api/assign-gate` 只作为 `/api/queue-status` 本身失败(网络错误 / 5xx)后的恢复手段。重入回 `code=100, queue_source="ratelimit"` 或 `code=429` 时,
>   保留 token、回到 `/api/queue-status` 轮询(间隔同上),**不**按 30/60/120s 退避重调 assign-gate。
> - 任一端点回 `410`:丢掉 `queue_token`,不带 token 重调 assign-gate,此后按不持 token 的口径处理。
> - 服务端回包格式与回包码集合不变。这里的 `429` 指回包 `code=429`(HTTP 200 承载,见 §5.4 更正)。伪造的 token 过不了 login 的 HMAC 校验,J0 不因此削弱。
> - 旧客户端兼容:login 侧同批改为放行窗口过后对该 token 回 EXPIRED(见 §11.3 #9 更正),仍按旧口径退避的客户端最坏在窗口过后拿到确定的 410,不会卡在名次 0。
>   但它仍会错过这一次放行、重新排队,所以客户端要按上面的口径改。
> - 落地状态:gateway 只改注释(`AssignGateRequest.java` 的 `queueToken`、`AssignGateController.java`),代码不变。
>   本仓库 robot 已按此实现(`robot/http_assign_gate.go` 的 `AssignGateWithQueue`,用例 `robot/http_assign_gate_test.go`),**未编译、未运行**,待 Codex 验证。
>   独立客户端 `../mmorpg-client` 待客户端任务跟进(本批未读取);Java 版的对齐由主会话登记 `PARITY.md`(AGENTS §12)。

### 11.6 运行期改阈值要清桶

Bucket4j 只在桶不存在时按新配置建桶;持续有流量时桶的 TTL 一直被刷新、不会过期,所以只改 rps / burst 并滚动 gateway,旧桶仍按旧配置跑
(推断:`AssignGateRateLimiter` 用 `build(key, supplier)`,没有配置替换;依 Bucket4j 8.x 语义,未实测)。改阈值后在维护窗口删掉 `rl:zone:*`(必要时连同 `rl:ip:*`),
命令见 `deploy/k8s/README.md`。代价:清空后每个桶会重新拿到一次满额 burst。后续可考虑给 Java 加配置替换(规格 §10 #8)。

### 11.7 已知风险

| # | 风险 | 缓解 |
|---|---|---|
| R1 | 2000/5000 保护不了 EnterGame:zone 桶每秒放约 1000–2000 人(看客户端调 `/api/login` 时带不带 zone_id),而 EnterGame 预加载链(新号冷读库、go/db worker 与分区数)的承载远低于此(规格 R1 引的压测记录约 50–100/s 稳态,本批未复核) | L-14 定终值;压测有结论前可考虑先用 §二「维护后开服」档 200/500(规格 §10 #1) |
| R2 | IP 桶误伤网吧 / CGNAT;集群内 robot 共用 Pod IP,staging 开着限流压测会被 IP 桶扭曲(robot 把 429 当失败) | L-14 先用 `-GatewayRateLimit off` 跑基线;开着限流压测需要 robot 伪造 XFF,同时 trusted-proxies 覆盖 robot 的 Pod 网段(后续项) |
| R13 | J1 未做:先扣 zone 令牌再判 IP 桶,一个 IP 每秒发 2000 个请求,每个请求都先扣掉 1 个 zone 令牌再被 IP 桶拒绝,单个 IP 就能把一个 zone 的入口压成排队 | 负责人 2026-10-13 作为已知风险接受;压测或线上出现时做 J1(先 IP 后 zone,zone 桶空时退还 IP 令牌) |
| R14 | Redis 内存:`rl:ip:*` 每个 IP 一个键;`rl:zone:<id>` 的 id 由请求决定,限流又先于 zone 合法性校验,J1 未做时这一步不受 IP 限速,可被造出任意多个 zone 桶键;IPv6 客户端在自己的 /64 内换源地址即得到新 IP 桶;排队打开后 `queue:*` 键按在排人数线性占内存。`deploy/k8s/manifests/infra/redis.yaml` 刻意不设 `--maxmemory` 与 memory limits(上限待压测数据定),这类键的增长没有 Redis 侧的上限 | L-14 测内存曲线;L-06 把限流与排队拆到独立 Redis |
| R16 | 排队轮询压在共享单主 Redis:`/api/queue-status` 不限流,每次查询要多次 Redis 往返(规格估约 2.5·Q ops/s,Q = 全服在排人数,推断,未实测),与限流桶、会话、锁争用同一个 Redis 主 | L-14 带排队压测时测 Redis 主的 ops / CPU;开服档轮询间隔另议(规格 §10 #12);L-06 |

其余风险(热键 CAS 争用、gateway 启动依赖 Redis 可达、可观测性缺口、参数不粘滞、部署侧与 login 的占位判定宽严不一、排队密钥轮换丢排位、
口令含特殊字符、已在运行的 zone 首次开排队时在线已超容量等)见规格 §9 与 `deploy/k8s/README.md`。

### 11.8 验证(交 Codex,未运行)

- `pwsh -NoProfile -File tools/scripts/tests/k8s_client_entry_contract.tests.ps1` 与 `tools/scripts/tests/k8s_deploy_contract.tests.ps1` 中的削峰用例;
- `java/gateway_node` 下 `./mvnw.cmd -q test "-Dtest=AssignGateRateLimiterTest,AssignGateControllerTest"`(Linux 用 `./mvnw`)。

命令顺序、前置清理与通过标准以规格 §7 为准。Java 版(另一仓库)的对应状态见规格 §11:功能已对齐、行为有意不同,「按档位打开」待其部署工具落地。

---

## 关联
- [docs/design/ARCH.md](./ARCH.md) §8 削峰
- [docs/design/architecture-current-state-vs-gaps-2026-05.md](./architecture-current-state-vs-gaps-2026-05.md) 缺口 #10
- [docs/design/login-simplification-2026-04.md](./login-simplification-2026-04.md) RedisLocker(后端兜底)
- [docs/design/gate-load-balancing-design.md](./gate-load-balancing-design.md) AssignGate 上下文
- 代码: `java/gateway_node/src/main/java/com/game/gateway/controller/AssignGateController.java`
- 代码: `java/gateway_node/src/main/java/com/game/gateway/service/AssignGateService.java`
