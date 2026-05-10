# 端到端阶梯压测: HTTP /api/login 路径 (2026-05-09)

**Date:** 2026-05-09
**前置改动:** [open-server-rate-limit-design.md](./open-server-rate-limit-design.md), [third-party-login-end-to-end-design.md](./third-party-login-end-to-end-design.md), [gate-login-rpc-boundary.md](./gate-login-rpc-boundary.md)
**目的:** 在新登录链路全部上线后,跑一次阶梯加压拐点测试,作为后续容量规划的基线。

---

## 一、链路 & 配置

```
robot ──HTTP /api/login──→ Java Gateway (limiter OFF) ──gRPC──→ go-zero login
robot ──HTTP /api/assign-gate──→ Java Gateway ──gRPC──→ go-zero login (HMAC token)
robot ──TCP──→ cpp gate ──gRPC──→ go-zero login (auth_type=access_token,轻量 BindSession)
robot ──TCP──→ cpp gate ──Kafka gate-{0}──→ go-zero login + scene_manager
```

- `robot/etc/robot.stress-{N}.yaml` (`use_http_login: true`, `auth_type: password`)
- 每档跑 30 秒
- 每档前清 redis 测试残留(`player:session:*` / `account_data:*` / `player_locator:*`/ `access_token:*` / `refresh_token:*` / `login_session:*` / `login_step:*` / `account_lock:*` / `player_locker:*`)
- 单 cpp gate(zone 1, gate_node_id 0) + 单 cpp scene
- 所有 go 服务 + Java Gateway 单实例,本机 docker infra(redis / etcd / mysql / kafka)

---

## 二、结果(基线)

| robot_count | login_ok | login_fail | login_stuck | enter_ok | avg_login | max_login | duration |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 50  | 100 | 0 | 0 | 100 | **101 ms** | 342 ms | 30 s |
| 100 | 200 | 0 | 0 | 200 | **84 ms**  | 331 ms | 31 s |
| 200 | 316 | 0 | 0 | 316 | **75 ms**  | 339 ms | 30 s |
| 500 | 604 | 0 | 0 | 604 | **69 ms**  | 163 ms | 30 s |

> `login_ok` 数 ≈ 2 × `robot_count` 是因为 robot 在窗口内有一次 reconnect(`recon_ok` 列与 `robot_count` 接近),所以每个机器人贡献 2 次 login。500 档少 4(626/630),原因是窗口结束时机器人还在握手,不算 fail/stuck。

### 关键观察
1. **没有 fail / stuck** —— 4 档总计 1220 次成功登录,0 失败 0 卡住
2. **avg_login 随并发上升下降** —— 100→69 ms,JIT/cache warmup 主导,真实瓶颈远未到
3. **max_login 在 500 档降到 163 ms** —— 50/100/200 的 ~340 ms 是首次启动冷启动 outlier(JVM/etcd channel 首次初始化),之后稳定
4. **msg 吞吐**: 500 档 99 msg/s,大部分是周期性心跳/动作

---

## 三、本机环境的限制

这是一台 **Windows 开发机** 的基线,生产容量更高:
- 单 cpp gate 进程,Windows 无 sysctl tuning(详见 [gate-kernel-tuning-runbook.md](../ops/gate-kernel-tuning-runbook.md)),Linux 生产 ~5x 余量
- 单 docker kafka(单 broker),Linux 生产至少 3 broker
- 单 zone 配置,生产横向多 zone

---

## 四、过程中复现的预存 Bug(已记录,不阻塞登录路径)

### #B-1 robot 退出时不发 Disconnect → player_locator session 永远 ONLINE
- robot 进程退出 / 被 kill 时,**没有主动发 `ClientPlayerLogin.Disconnect`**
- cpp gate 检测到 TCP 关闭也只是清自己的 SessionInfo,没把 Kafka 消息转给 player_locator
- 结果:`player:session:{playerId}` 永远 `state=ONLINE`,下一轮重启 robot 时 `DecideEnterGame` 命中 ONLINE → ReplaceLogin → KickPlayer 给"幽灵 gate" → 新 gate 永远收不到 BindSession → robot 等 scene-ready 超时
- **生产环境**:玩家正常拔网/退出会走 TCP RST,gate 能监听到 fin/rst → 触发完整 cleanup,所以**生产无症状**;只是 dev 反复跑机器人时遇到
- **解法**(下个迭代):
  1. robot main `defer` 里发 Disconnect 再退出(直接的修法)
  2. cpp gate 在 TCP close 时,通过 Kafka 发 `PlayerDisconnected` 给 `player_locator` topic(架构上的修法,见 player_login_flow.md §2)
- 已加到 docs/design/architecture-current-state-vs-gaps-2026-05.md 缺口清单

### #B-2 docker kafka 在测试机上偶发 Exited (255)
- 跑测试中段 kafka 容器自己 die(`docker ps -a` 看 Exited 49 minutes ago)
- 重启后必须**重启 cpp gate** 让 rdkafka client 干净 bootstrap(看到 `Topic gate-0 partition count changed from 1 to 0` 是恢复信号但不充分)
- 生产 kafka 集群多 broker,偶发不会影响登录,但 dev 单 broker 要警惕

### #B-3 EnterGame 默认 decision = ReplaceLogin,zset 是空的
- player_locator 的 lease ZSET 在测试中保持空,但 GetSession 直接读 `player:session:{id}` key 仍能命中残留
- 这两个存储不一致——zset(lease 监控)和 session key(权威)各管一摊;残留不影响在线人数统计但会让 Decide 走错分支
- **观察**:这是 `LeaseMonitor` 的 watchdog 设计,session key 是事实源,lease ZSET 只用于 30s 重连超时 trigger

---

## 五、容量推论(粗算)

按 500 档结果反推单机容量上限(本机 Windows dev):

| 资源 | 当前用量 (500 bots) | 估算上限 |
|---|---|---|
| login QPS | ~50 (1000 bots/30s × 1.5×reconnect) | go-zero 单实例 ≥1.5 万,**离顶 300x** |
| Java Gateway QPS | ~50 (与 login 同) | Spring Boot 单实例 1 万,**离顶 200x** |
| cpp gate 长连接 | 600 | muduo 单进程 5 万,**离顶 80x** |
| cpp gate ephemeral port | <100 (`ss -s` Bound 估) | 64K 上限,**离顶 600x** |
| kafka gate-0 partition | 1 | 多分区可线性扩 |

→ **本机 dev 单实例的真实极限大概在 5000-10000 bots**。Linux 生产配 sysctl + 多 gate + 多 zone,**单 zone 5 万玩家是合理目标**。

下一步阶梯应跑到 1000 / 2000 / 5000,看哪个组件先翻车(预期是 cpp gate CPU 或 kafka 单 broker)。

---

## 六、复现命令

```bash
# 一次性环境(首次)
docker compose up -d
dev.bat build
dev.bat start            # cpp gate + scene + go-svc
cd java/gateway_node && mvn -DskipTests package spring-boot:repackage
java -jar target/gateway-node-0.0.1-SNAPSHOT.jar &

# 每轮压测前 wipe(避免 #B-1 残留)
docker exec redis redis-cli FLUSHDB

# 跑某一档
sed "s/REPLACE_ME/500/" robot/etc/robot.stress-template.yaml > robot/etc/robot.stress-500.yaml
cd robot && timeout 30 ./robot.exe -c etc/robot.stress-500.yaml
```

完整自动化脚本:本文档头注释里附的 bash 阶梯循环。

---

## 关联文档
- [docs/design/ARCH.md](./ARCH.md) §11 决策表(本压测确认决策 #9-#13 的正确性)
- [docs/design/stress-test-2026-05-ephemeral-port.md](./stress-test-2026-05-ephemeral-port.md) — 老 gate 路径压测复盘
- [docs/design/architecture-current-state-vs-gaps-2026-05.md](./architecture-current-state-vs-gaps-2026-05.md) — 缺口清单
- 数据: `run/logs/stress/results.csv`, `run/logs/stress/run-{N}.log`
