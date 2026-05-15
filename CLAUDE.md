# 项目记忆 — MMO Server (mmorpg)

> 给后续 AI / 新人:不读完 docs 也能 5 分钟入戏的精简版。
> 详细架构请看 [docs/design/ARCH.md](docs/design/ARCH.md)。

---

## 1. 三层语言栈

| 层 | 技术 | 目录 | 职责 |
|---|---|---|---|
| HTTP 门户 / 运维后台 | **Java Spring Boot + Sa-Token** | `java/gateway_node/`, `java/springboot_satoken_auth_starter/` | 服务器列表 / assign-gate / 公告 / 热更 / CDN / 运维后台 |
| 游戏内 RPC | **Go (go-zero)** | `go/login/`, `go/scene_manager/`, `go/player_locator/`, `go/data_service/`, ... | 登录 / 场景管理 / 玩家定位 / 数据服务 / 好友 / 公会 |
| 运行时长连接 / ECS | **C++ (muduo)** | `cpp/nodes/gate/`, `cpp/nodes/scene/` | gate(玩家长连接) / scene(战斗 ECS) |

通信: gRPC(同步) + Kafka(异步控制流,topic = `gate-{gateId}`)
注册中心: etcd
存储: Redis(token / 锁 / 会话定位 / 缓存)+ MySQL(账号 / 玩家档案 / 公告)

---

## 2. 硬约束(改动前必读)

1. **muduo 框架代码不改** — gate 优化只在业务层 + 配置 + 内核
2. **MMO/MOBA 延迟敏感** — 游戏 TCP 链路**零中间代理**,不挂 LVS / Nginx / HAProxy
3. **Gate 是有状态的**(`SessionInfo` + Kafka `gate-{id}` 路由)→ **不能用 L4 LB 做 gate 后向均衡**
4. **会话权威源 = Login + player_locator**(Centre 节点已退役)

---

## 3. 登录链路(8 步)

```
Client → /api/server-list  → Java Gateway
Client → /api/assign-gate  → Java Gateway → gRPC AssignGate → go-zero login
                                         ← {gate_ip, port, HMAC token}
Client → TCP Gate
Client → ClientTokenVerify → Gate(本地 HMAC 验签,不查 Redis)
Client → Login(auth_type, auth_token) → go-zero login
            ↓ AuthProvider.Validate(password / wechat / qq / satoken / netease)
            ↓ EnterGame: RedisLocker → loginstep → DecideEnterGame
            ↓ Kafka → gate-{gateId}: BindSession + RoutePlayer
            ↓ Scene 收 GateLoginNotify(enter_gs_type)
玩家进游戏循环
```

**enter_gs_type:** 1=FIRST / 2=REPLACE(顶号) / 3=RECONNECT(30s 重连窗口) / 0=NONE

**详细事件流(含同/跨 gate 重连、超时清理):** [docs/design/player_login_flow.md](docs/design/player_login_flow.md)

---

## 4. Token 三层(完全正交)

| 层 | TTL | 谁颁发 | 谁验证 | 存储 |
|---|---|---|---|---|
| 第三方鉴权 (WeChat/QQ/Sa-Token/password) | 平台决定 | 第三方 / 自有 | go-zero login `AuthProvider` | 第三方/自有 |
| Access Token | 2h | go-zero login | go-zero login (auth_type="access_token") | Redis `access_token:{T}` |
| Refresh Token | 30d | go-zero login | go-zero login (一次性,rotation) | Redis `refresh_token:{T}` |
| Gate Token (HMAC-SHA256 签 GateTokenPayload) | 300s | go-zero login `AssignGate` | C++ gate 本地验签 | 不存储 |

**Sa-Token 的角色:** 仅在 Java Gateway 这层(运维后台 / 内部 API),**不下发给 cpp gate**。

详见 [docs/design/dual-token-authentication.md](docs/design/dual-token-authentication.md) + [docs/design/auth-provider-framework.md](docs/design/auth-provider-framework.md)

---

## 5. 关键基础设施

- **锁:** `pkg/locker/RedisLocker`(UUID + Lua CAS,杜绝错删别人锁)
- **状态机:** `pkg/loginstep/`(纯 string,替代 looplab/fsm)
- **Kafka 顺序:** `KeyOrderedKafkaProducer`,playerId 一致性哈希到同 partition,`Idempotent=true` + `MaxOpenRequests=1`
- **会话定位:** `player_locator` + Redis ZSET(lease 超时 30s)

---

## 6. 内核基线(gate 机器必做)

```
ip_local_port_range = 1024 65535
tcp_tw_reuse        = 1
tcp_max_tw_buckets  = 1048576
somaxconn           = 65535
tcp_max_syn_backlog = 65535
```

⚠️ 不开 `tcp_tw_recycle`(NAT 会丢)

实测收益: avg_login 414ms → 78ms(5.5x),max 25s → 1.2s(~20x)

完整配置: [docs/ops/gate-kernel-tuning-runbook.md](docs/ops/gate-kernel-tuning-runbook.md)

---

## 7. 当前已知缺口(2026-05)

跟踪在任务清单 / [docs/design/architecture-current-state-vs-gaps-2026-05.md](docs/design/architecture-current-state-vs-gaps-2026-05.md):

- **#10** Java Gateway 加 Bucket4j 限流 + 排队(防开服风暴)
- **#11** QQ/微信 provider 端到端联调 + 账号映射
- **#13** 核查 cpp gate→go-zero login gRPC 是否长连复用(压测 Bound=2704 残留)
- **#9 / #2** sysctl 固化 + 阶梯压测找拐点

**已完成**:
- ~~**#10** Java Gateway 加 Bucket4j 限流 + 排队(防开服风暴)~~ — ✅ Bucket4j 2026-05-08 + AssignGate 真排队 2026-05-14,见 [login-queue-2026-05.md](docs/design/login-queue-2026-05.md)

---

## 8. 文档索引(最常翻的几篇)

| 想看什么 | 去哪 |
|---|---|
| 总架构 | [docs/design/ARCH.md](docs/design/ARCH.md) |
| 登录全流程 | [docs/design/player_login_flow.md](docs/design/player_login_flow.md) |
| 第三方登录怎么加 | [docs/design/auth-provider-framework.md](docs/design/auth-provider-framework.md) |
| 双 token 设计 | [docs/design/dual-token-authentication.md](docs/design/dual-token-authentication.md) |
| 为啥 gate 不能挂 LB | [docs/design/gate-load-balancing-design.md](docs/design/gate-load-balancing-design.md) |
| Java Gateway 选型 | [docs/design/java-gateway-portal-decision.md](docs/design/java-gateway-portal-decision.md) |
| login 简化历史 | [docs/design/login-simplification-2026-04.md](docs/design/login-simplification-2026-04.md) |
| 现状盘点 vs 缺口 | [docs/design/architecture-current-state-vs-gaps-2026-05.md](docs/design/architecture-current-state-vs-gaps-2026-05.md) |
| 登录排队设计 | [docs/design/login-queue-2026-05.md](docs/design/login-queue-2026-05.md) |
| 内核调优 SOP | [docs/ops/gate-kernel-tuning-runbook.md](docs/ops/gate-kernel-tuning-runbook.md) |
| 排队压测判读 | [docs/ops/login-queue-stress-runbook.md](docs/ops/login-queue-stress-runbook.md) |
| 压测复盘 | [docs/design/stress-test-2026-05-ephemeral-port.md](docs/design/stress-test-2026-05-ephemeral-port.md) |

更全索引在 ARCH.md §10。

---

## 9. 给 AI 的工作守则

1. **先读 ARCH.md,再动手**。项目已实现 90% 标准 MMO 登录架构,新方案 90% 概率能复用现有组件。
2. **改动登录链路前**,看 `player_login_flow.md` 的 enter_gs_type 表;不要新增第 5 种值。
3. **不要建议给 gate 挂 LVS / Nginx**,详见 ARCH.md §2 + gate-load-balancing-design.md。
4. **不要建议改 muduo**,只改业务层。
5. **新增第三方登录渠道** 走 AuthProvider 框架,4 步搞定,见 auth-provider-framework.md §How to Add。
6. **加锁优先用 `RedisLocker`**,不要再发明轮子。
7. **写架构变更先更新 ARCH.md §11 决策表**,再写专题文档。
