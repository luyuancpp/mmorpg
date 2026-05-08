# 架构现状盘点 vs 缺口清单 (2026-05)

**Date:** 2026-05-08
**Status:** Snapshot — 用于决策"还需要做什么"

> 起因: 压测出现 `Bound=15470 / SynSent=1540 / login 25s 长尾`,扩 ephemeral port 后 avg 414ms→78ms。
> 复盘后发现项目其实已实现 90% 的标准 MMO 登录架构,本文对照盘点并列出真正待补缺口。

---

## 一、已实现(无需重做)

| # | 能力 | 实现 | 文档 |
|---|---|---|---|
| 1 | Java Gateway (HTTP 门户) | `java/gateway_node/` Spring Boot | [java-gateway-portal-decision.md](./java-gateway-portal-decision.md) |
| 2 | Sa-Token 接入 | `java/springboot_satoken_auth_starter/` + `SaTokenProvider` | [auth-provider-framework.md](./auth-provider-framework.md) |
| 3 | go-zero login(gRPC) | `go/login/` | [player_login_flow.md](./player_login_flow.md) |
| 4 | AssignGate + HMAC gate token | `go/login/internal/logic/pregate/` | [gate-load-balancing-design.md](./gate-load-balancing-design.md) |
| 5 | 双 token (Access 2h + Refresh 30d) | `go/login/internal/logic/pkg/token/` | [dual-token-authentication.md](./dual-token-authentication.md) |
| 6 | Auth Provider 框架 | `go/login/internal/logic/pkg/auth/` | [auth-provider-framework.md](./auth-provider-framework.md) |
| 7 | WeChat / QQ Provider 实现 | `providers.go` | [auth-provider-framework.md](./auth-provider-framework.md) |
| 8 | RedisLocker (UUID + Lua CAS) | `pkg/locker/` | [login-simplification-2026-04.md](./login-simplification-2026-04.md) |
| 9 | loginstep 状态机 | `pkg/loginstep/` | [login-simplification-2026-04.md](./login-simplification-2026-04.md) |
| 10 | KeyOrderedKafkaProducer | `pkg/consistent/` | [login-simplification-2026-04.md](./login-simplification-2026-04.md) |
| 11 | player_locator 会话权威源 | `go/player_locator/` | [player_login_flow.md](./player_login_flow.md) |
| 12 | scene_manager 路由 | `go/scene_manager/` | [enter-scene-zone-routing.md](./enter-scene-zone-routing.md) |
| 13 | Gate Kafka 命令通道 | `gate-{gateId}` topic | [player_login_flow.md](./player_login_flow.md) |
| 14 | 重连 / 顶号 / 超时清理 | LOGIN_FIRST/RECONNECT/REPLACE | [player_login_flow.md](./player_login_flow.md) |
| 15 | Centre 节点已退役 | Login + player_locator 接管 | [centre_decommission_*.md](./centre_decommission_migration_plan.md) |

---

## 二、最近压测发现的实测结论(已生效)

| 指标 | 优化前 | 优化后 | 改善 |
|---|---|---|---|
| login_fail | 3% | 0 | 完全消除 |
| avg_login | 414-453ms | 77-79ms | 5.5x |
| max_login | 18-25s | 0.9-1.2s | ~20x(长尾消失) |
| TCP SynSent | 1540 | 0 | 清零 |
| Bound | 15470 | 2704 | 5.7x 缓解 |

**根因:** 客户端侧 ephemeral port 耗尽 + TIME_WAIT 不可复用。
**已做:** 扩 `ip_local_port_range = 1024 65535` + `tcp_tw_reuse=1`。
**待做:** 见 §三。

---

## 三、真正的缺口清单

### 缺口 #10: 开服削峰(Java Gateway 限流 + 排队)
**风险:** 开服 5 万人同时点登录,login `player_locker` 排队 + DB 加载压力雪崩。
**做法:**
- `java/gateway_node/` 的 `/api/assign-gate` 加 Bucket4j+Redis 全局令牌桶(2000 token/s)
- 拿不到令牌 → 返回 `{ code: QUEUEING, queue_pos, retry_after }`
- 客户端 UI 显示排队,定时重试
- 按 zone 分批放人

**为什么放在 Gateway:** HTTP 短连接、Bucket4j 生态成熟、削峰失败不影响游戏中玩家。

### 缺口 #13: gate→login gRPC 长连接核查
**风险:** 压测 `Bound=2704` 残留说明 gate 主动 connect 仍在持续发生。
**核查点:**
- `cpp/nodes/gate/` 内 EnterGame 路径上的 gRPC client 是否单例长连
- 是否每次请求都 `new Channel`(go-zero 客户端的常见误用)
- gRPC keepalive 是否开启,是否被对端 reset

**核查命令:**
```bash
# gate 机器上
ss -tan state time-wait | awk '{print $4}' | sort | uniq -c | sort -rn | head
# 如果集中在 login 的 IP:port → 短连接;如果集中在 gate 监听端口 → 客户端短连
```

### 缺口 #11: QQ/微信 provider 端到端联调
**现状:** `WeChatProvider` / `QQProvider` 已实现 `Validate`,但全链路未跑通。
**待做:**
- Java Gateway 接收 SDK code → 转发 `LoginRequest{auth_type, auth_token}`
- 业务侧账号映射(`wx_<unionid>` / `qq_<unionid>` 与 `UserAccounts` 表绑定)
- 老账号绑定新渠道的"账号合并"流程
- 灰度配置:某 zone 仅开 password,某 zone 开微信/QQ

### 缺口 #9: 内核 sysctl 固化
**现状:** 调优用 `sysctl -w` 临时生效,重启丢失。
**待做:** 写到 `/etc/sysctl.d/99-gate.conf`,见同目录 runbook。

### 缺口 #2: 压测报告 + 拐点曲线
**现状:** 单点结果有了,但没系统压到拐点。
**待做:** 阶梯 45k → 60k → 80k → 100k → 120k,画 QPS-延迟曲线。

---

## 四、不做(明确排除)

| 方案 | 排除原因 |
|---|---|
| 在 gate 前挂 LVS / DPVS | gate 是有状态的(SessionInfo + Kafka gate-{id} 路由),L4 LB 把重连分到别的 gate 会丢消息。详见 [gate-load-balancing-design.md](./gate-load-balancing-design.md) |
| 在游戏 TCP 链路前挂 Nginx | MMO/MOBA 延迟敏感,游戏链路零中间代理是硬约束 |
| 改 muduo 框架 | 硬约束: 框架不动,只调业务层 + 配置 + 内核 |
| `tcp_tw_recycle=1` | NAT 环境会丢连接,4.12+ 内核已删除 |

---

## 五、推进顺序建议

```
第1步: 缺口 #13 核查 → 快速 ss 命令定位短连源头(15 分钟)
第2步: 缺口 #9  固化 sysctl(30 分钟)
第3步: 缺口 #10 Java Gateway 加 Bucket4j 限流(2 天)
第4步: 缺口 #11 QQ/微信端到端联调(3-5 天)
第5步: 缺口 #2  阶梯压测找拐点(1 天)
```
