# 上线前 Checklist (HTTP /api/login 路径)

**Date:** 2026-05-09
**目的:** 把"新登录链路"从灰度到全量上线的所有动作 / 阈值 / 回滚 SOP 沉淀成一张清单。
**前置阅读:** [ARCH.md §11-12](../design/ARCH.md), [open-server-rate-limit-design.md](../design/open-server-rate-limit-design.md), [stress-test-2026-05-http-login.md](../design/stress-test-2026-05-http-login.md)

---

## A. 部署前(代码已合,准备发布镜像)

### A.1 代码 / 测试

- [ ] `mvn test` Java Gateway 全绿(当前 34/34)
- [ ] `go build ./...` go-zero login + player_locator + scene_manager + data_service + db 全 EXIT=0
- [ ] cpp gate / scene MSBuild Debug + Release 全过
- [ ] robot 单二进制可产出(`go build -o robot.exe .` ~26 MB)
- [ ] `git log --oneline` 包含本轮 11 个 commit(自 `4d6c70482 docs: onboarding...` 起)

### A.2 配置审计

- [ ] `java/gateway_node/src/main/resources/application.yaml`
  - `gate.rate-limit.enabled: false` (上线**默认关**,T+0 灰度才开)
  - `login.grpc.endpoints` 指向真实 login.rpc(staging 51000;prod 50000)
  - `gate.token-secret` 与 `bin/etc/base_deploy_config.yaml.gate_token_secret` 一致(不一致时 cpp gate 验签全 fail)
- [ ] `go/login/etc/login.yaml`
  - `GateTokenSecret` 与上同步
  - `AuthProviders.WeChat` / `QQ` 真实 AppId/AppSecret 通过环境变量注入
  - `TokenConfig.AccessTokenTTL=2h` / `RefreshTokenTTL=720h`
- [ ] `go/player_locator/etc/player_locator.yaml`
  - `LeaseTTL` ≥ 30s(否则 30s 重连窗口失效)
- [ ] `bin/etc/base_deploy_config.yaml`
  - `Kafka.Brokers` 至少 3 个 broker(单 broker 见 §F.B-2)
  - `etcd_hosts` 与服务一致

### A.3 Linux 内核(gate 机器)

固化到 `/etc/sysctl.d/99-gate.conf`,然后 `sysctl --system`:

```ini
net.ipv4.ip_local_port_range = 1024 65535
net.ipv4.tcp_tw_reuse        = 1
net.ipv4.tcp_max_tw_buckets  = 1048576
net.ipv4.tcp_fin_timeout     = 15
net.core.somaxconn           = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.netfilter.nf_conntrack_max = 1048576
net.ipv4.tcp_keepalive_time   = 300
net.ipv4.tcp_keepalive_intvl  = 30
net.ipv4.tcp_keepalive_probes = 3
```

⚠️ **不开** `tcp_tw_recycle`(NAT 环境会丢)。
完整说明: [docs/ops/gate-kernel-tuning-runbook.md](./gate-kernel-tuning-runbook.md)

### A.4 服务 ulimit(gate / scene)

- [ ] `LimitNOFILE=1048576` 写入 systemd unit
- [ ] `LimitNPROC=65535` 写入 systemd unit
- [ ] `/etc/security/limits.d/gate.conf` 同步配置

---

## B. 监控基线(发布前布点)

### B.1 必须 dashboard

| 面板 | 数据源 | 关键阈值 |
|---|---|---|
| 登录成功率 (HTTP) | gateway log `code:0 / 100 / 401 / 429 / 500` | 95% 持续 5 分钟 → 告警 |
| 登录 latency p99 | gateway access log | > 500 ms 持续 1 分钟 → 告警 |
| Login.Login QPS / decision 分布 | go-zero login stat + decision counter | decision=2 (REPLACE) 占比 > 5% → 告警 |
| Refresh-token rotation QPS | gateway `/api/refresh-token` | 突涨 10x → 怀疑攻击 |
| Bucket4j 限流命中率 | Java JMX | rate limited count |
| **legacy_login_count vs new_login_count** | go-zero login deprecation 计数器 | T+0 灰度期 legacy 单调下降 |
| gate ephemeral port | `node_exporter` `node_sockstat_TCP_*` | TIME_WAIT > 50% port range → 告警 |
| gate accept queue 溢出 | `nstat TcpExtListenOverflows` | > 0 → 严重告警 |
| Kafka `gate-{gateId}` lag | kafka-ui / cmak | > 1000 → 告警 |
| player_locator session ONLINE 数 | redis ZCARD player_locator:lease_zset + SCAN player:session: | 偏差 > 20% → 怀疑 #B-1 类 leak |

### B.2 日志关键词

错误关键字(grep + 告警):
- `[DEPRECATION]` → 知道老路径还在用,不告警
- `kLoginAccountNotFound` `kLoginInProgress` → 用户体感
- `Failed to parse NodeInfo` → 已修(allocated 过滤),如再出现说明 etcd schema 变更
- `Connect to ipv4#.*:9092 failed` → kafka 挂了
- `bind: An attempt was made to access a socket in a way forbidden` → 端口冲突(Windows)/ 占用
- `[disconnect] get account failed` → session 残留(参考 #B-1)
- `panic:` `fatal:` `OOM` `Killed` → P0

---

## C. 灰度切换步骤(T+0 → T+3,详见 ARCH §12)

### T+0(2026-05):内测灰度

- [ ] robot 全量用 `use_http_login: true`(已有 `robot/etc/robot.stress-*.yaml`)
- [ ] 内测客户端(version >= X)启用 `/api/login` 路径
- [ ] Gateway `rate-limit.enabled: false`(生产观察日志再开)
- [ ] 退出条件:**新路径成功率 ≥ 老路径** 且 **`[DEPRECATION]` 计数稳定**

### T+1(2026-06):全量切换

- [ ] 线上客户端默认走 `/api/login`
- [ ] Gateway 按 zone 灰度开 limiter:
  ```yaml
  gate.rate-limit:
    enabled: true
    zone-default-rps: 500
    zone-default-burst: 1000
    zone-overrides:
      1: { rps: 2000, burst: 5000 }   # 主力 zone
  ```
- [ ] 老路径保留兜底,但日志 `[DEPRECATION]` 监控降到 < 1%
- [ ] 退出条件:全量 24 小时稳定,refresh-token 走 HTTP 比例 > 90%

### T+2(2026-07):老路径下线

- [ ] go-zero login 加 feature flag `legacy-gate-login-enabled: false`
- [ ] 老路径计数 0 持续 2 周
- [ ] 退出条件:零误伤客户端

### T+3(2026-08):代码移除

- [ ] cpp gate `HandleGrpcNodeMessage` 删除对 `ClientPlayerLoginLoginMessageId` 的路由(只删 Login,**保留** EnterGame/CreatePlayer/LeaveGame/Disconnect/RefreshToken)
- [ ] go-zero `deprecation.go` 删除
- [ ] `proto/message_id.txt` 标记 #48 deprecated

---

## D. 开服日 SOP

### D.1 开服前 1 小时

- [ ] redis FLUSHDB(测试库)+ 真实库做 `BGSAVE` 备份
- [ ] kafka topic 提前创建:`gate-0` ~ `gate-{N}` partition=8,retention 5min
- [ ] etcd `Compact + Defrag`
- [ ] Gateway 配置波次 schedule:
  ```yaml
  gate.rate-limit.wave:
    enabled: true
    start-epoch-sec: <开服时刻 unix>
    schedule:
      - { offset-sec: 0,   allow-zones: [1, 2] }
      - { offset-sec: 30,  allow-zones: [3, 4] }
      - { offset-sec: 120, allow-zones: [-1] }   # 全开
  ```
- [ ] 客户端公告热更:开服时间 / 排队提示 UI / 重试策略

### D.2 开服瞬间(T0 ~ T+5min)

监控值班(必须 2 人):
1. p99 登录延迟突涨? → 看 Gateway/login QPS 是否打满
2. 排队队列深度? → `gate_assign_queue_depth` 跌 0 = 限流没生效
3. cpp gate ephemeral port? → 接近 50% → 准备扩 IP / 横向加 gate
4. kafka lag 突涨? → 加分区或加 broker

### D.3 开服后(T+5min ~ T+1h)

- [ ] 全 zone wave 已开
- [ ] Gateway QPS 平稳 < 单实例 1万
- [ ] login QPS 平稳 < 单实例 1.5万
- [ ] gate 长连数 < 单进程 5 万

---

## E. 回滚 SOP

### E.1 触发条件(任一即立即回滚)

- 新路径登录成功率 < 90%(老路径 99%+)
- p99 > 2s 持续 5 分钟
- Gateway/login OOM 或 panic
- 数据完整性问题(玩家进游戏看到错乱数据)

### E.2 三级回滚

**1 级 - 关限流**(30 秒生效):
```bash
# Gateway 配置中心改 gate.rate-limit.enabled=false
# 或环境变量:GATE_RATE_LIMIT_ENABLED=false
kubectl rollout restart deploy/gateway-node -n mmorpg
```

**2 级 - 切回老路径**(2 分钟生效):
```yaml
# 客户端配置中心(或灰度服务):
useHttpLogin: false     # 客户端走老的 gate Login RPC 路径
```
go-zero login 不动,因为它对两条路径都兼容。

**3 级 - 完全回滚版本**(5-10 分钟):
```bash
# k8s rollback 到上一个 tag
kubectl rollout undo deploy/gateway-node -n mmorpg
kubectl rollout undo deploy/login -n mmorpg
# cpp gate 是 statefulset,谨慎滚动
```

### E.3 回滚后必做

- [ ] 抓 5 分钟 Gateway / login / gate 日志归档(给事后复盘)
- [ ] dump redis `player:session:*` 样本 100 个
- [ ] dump etcd `--prefix LoginNodeService.rpc/`
- [ ] 写一份 RCA(根因分析),最迟下个工作日

---

## F. 已知风险 & 应对

### F.B-1: dev 重启 robot 后 player_locator session 残留 ONLINE

- **状态:** 治标 (E) + 治本 (G) 都已合
- **治标:** robot main `defer sendDisconnectBestEffort(gc)` 主动通知 login.Disconnect (commit `0f4712996` 之后)
- **治本:** cpp gate `HandleConnectionDisconnection` 把 `SessionInfo.playerId` 填进 `SessionDetails`,login `markPlayerSessionDisconnecting` 真的能跑(本轮 commit)
- **生产影响:** 玩家正常 TCP 断会触发 OS-level RST → gate 同样路径,本来就走治本逻辑,所以**生产无症状**;dev 重跑机器人时遇到

### F.B-2: docker kafka 单 broker 偶发 Exited (255)

- **状态:** 仅 dev,生产 3+ broker 不会触发
- **dev 应对:** `docker start kafka && dev.bat stop && dev.bat start`(必须重启 cpp gate 让 rdkafka 干净 bootstrap)
- **生产应对:** 依赖 kafka 集群高可用 + ISR 监控

### F.B-3: 1k+ 阶梯压测在 Windows dev 跑不动

- **状态:** Windows Hyper-V 端口保留 + 单 docker broker + cpp gate 无 sysctl tuning
- **应对:** **1k/2k/5k 必须在 Linux staging 跑**,把结果追加到 [stress-test-2026-05-http-login.md](../design/stress-test-2026-05-http-login.md)
- **当前基线:** Windows dev 单实例 50/100/200/500 全 0 失败,延迟 69-101ms

---

## G. 上线后 1 周观察项

- [ ] `legacy_login_count / new_login_count` 比例曲线(T+1 退出条件)
- [ ] refresh token rotation 成功率(应 > 99.9%)
- [ ] Bucket4j 限流误伤率(用户上报 + log 误差 < 0.01%)
- [ ] cpp gate→login gRPC channel 复用(`netstat -tan | grep 51000` 应只有少数 ESTAB,不上千)
- [ ] WeChat / QQ provider 端到端登录漏斗(若灰度中开启了第三方登录)

---

## 关联文档
- [ARCH.md](../design/ARCH.md) §11-12 决策表 + deprecation 计划
- [onboarding.md](../design/onboarding.md) 5 分钟上手
- [open-server-rate-limit-design.md](../design/open-server-rate-limit-design.md) 限流配置参考
- [stress-test-2026-05-http-login.md](../design/stress-test-2026-05-http-login.md) 压测基线
- [gate-kernel-tuning-runbook.md](./gate-kernel-tuning-runbook.md) sysctl 完整清单
