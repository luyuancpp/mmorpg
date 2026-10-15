# G4b 实现规格：削峰在 staging / prod 档打开（gateway Bucket4j 限流 + login 登录排队）

> 只读核对产出（2026-10-12），未改任何仓库文件。行号按**工作区**（含他人未提交改动）引用。
> 对应清单：`docs/design/tidb-phase1-and-launch-readiness.md:812-813`（L-01、L-02），`launch_verdict.txt:19`、`:33`。

---

## 0. 结论速览

- **做什么**：`tools/scripts/k8s_deploy.ps1` 按 `-ReleaseProfile` 生成配置。dev 两项都关，ConfigMap 与今天逐字相同；staging / prod 两项都开，阈值用开服档起步值，全部标「待压测确认」。新增两个开关参数 `-GatewayRateLimit` / `-LoginQueue`，取值 `auto|on|off`，缺省 `auto` = 跟随档位。`off` 用于止血和基线压测。
- **G1b（排队 leader 锁修复）未落地**。依据见 §1.4。所以本批拆成两半：
  - **G4b-1 gateway 限流**：不依赖 G1b，但**依赖 G4b-J0**（评审补，§4.10）：今天 assign-gate 只要带任意非空 `queue_token` 就整个跳过限流，而 login 排队关着时这种请求直接走快速通道签 gate 票（`AssignGateController.java:51-57`；`assigngatelogic.go:62-66`、`:90-93`）。G4b-1 单独上线、或用 `-LoginQueue off` 止血时，限流对任何会伪造一个字段的客户端都形同虚设。合入顺序 G4b-J0 → G4b-1（可同批）。
  - **G4b-2 login 排队**：合入顺序必须是 G1b → G4b-2，且 G4b-2 之前还要处理 §1.5 的两条基线排队缺陷（G4b-G，或负责人书面接受风险，§10 #11）。G4b-2 另外在 ConfigMap 里显式写本 zone 的锁键，作纵深防御。G1b 规格（`launch_spec_G1b.md` §3.1）已定为「配置值是**前缀**，实际锁键 = `<前缀>:zone:<Node.ZoneId>`」，所以 G1b 之后的镜像实际抢的是 `dispatcher:lock:login_queue:zone:101:zone:101`（每 zone 仍唯一，正确）；这一行真正防的是 `kubectl rollout undo` 回到 G1b 之前的镜像、而 ConfigMap 仍是开排队的新版（§4.5、R8）。
- **只翻开关不够**，打开前要补三个隐性前提（第 4 条是评审补的容量注意事项），代码核对后确认：
  1. gateway ConfigMap 没写 Redis 口令，而 staging / prod 的 Redis 带 `--requirepass`。Bucket4j 每次调用都会遇到 NOAUTH，然后**静默 fail-open**，开关等于没开（§1.3）。
  2. login 在生产模式下开排队，必须配一把独立的 `Secrets.QueueToken`，否则拒绝启动（§1.2）。
  3. `ZoneCapacityOverride` 不配就会卡死。回落公式向下取整，在线 0 或 1 人时容量只有 1（§1.2）。
  4. （评审补）排队打开后，压在共享单主 Redis 上的不只是限流桶，还有不限流的 `/api/queue-status` 轮询：每次 `Lookup` 4–5 次往返，约 2.5·Q ops/s（Q = 全服在排人数，R16）。阈值与轮询间隔要在 L-14 里一起测（§10 #12）。
- 规模：约 10 个文件，PowerShell 2 个、契约测试 2 个、文档约 6 个；Java gateway 2–3 个（G4b-J0 必做，J1/J2 建议，§4.10）。本批不改 Go 和 C++；§1.5 的排队基线缺陷若选择修，另立 G4b-G（Go，§10 #11）。

---

## 1. 现状核对（事实 + 文件:行号）

### 1.1 gateway（`java/gateway_node`）Bucket4j

| 事实 | 出处 |
|---|---|
| 限流总开关在 jar 内默认关，注释是「等运维验证 staging Redis 后再开」 | `src/main/resources/application.yaml:36-38`；`RateLimitProperties.java:19-20`（`enabled = false`） |
| jar 内阈值：zone 500/1000，IP 5/20，冷却 5000ms，`queue-timeout-ms` 300000 | `application.yaml:39-44` |
| 属性类自身的默认值：IP 2/10 | `RateLimitProperties.java:32,35` |
| `queue-timeout-ms` 没有任何消费方（死键），只在属性类里有 getter / setter | `RateLimitProperties.java:41,114-115`，全仓 grep |
| 关闭或没有 ProxyManager 时直接放行 | `AssignGateRateLimiter.java:70-72` |
| 判定顺序：波次 → **zone 桶（先扣）** → IP 桶（耗尽回 429）→ 账号冷却（进程内） | `AssignGateRateLimiter.java:74-118` |
| zone_id=0 时跳过 zone 桶 | `AssignGateRateLimiter.java:83` |
| Redis 或 Bucket4j 抛异常 → 记一行 WARN 后放行（fail-open） | `AssignGateRateLimiter.java:99-102` |
| 桶键 `rl:zone:{id}`、`rl:ip:{ip}`；`/api/login` 与 `/api/assign-gate` 共用这两个桶，冷却按端点分开 | `AssignGateRateLimiter.java:149-155`、`:69`；`LoginController.java:54` |
| 带 `queue_token` 的 assign-gate 绕过限流：判据只是「非空」，gateway 不验签、也不知道 login 排队开没开 | `AssignGateController.java:51-57` |
| （评审补）login 排队关闭时，带 `queue_token` 的请求不进重入分支，直接走快速通道签 gate 票。两条合起来 = 排队关着时任意非空 `queue_token` 绕过整个 Bucket4j 并拿到票 | `assigngatelogic.go:62-66`、`:90-93` |
| （评审补）`/api/queue-status` 设计上不限流 | `QueueStatusController.java:22-26`、`:38-41` |
| （评审补）账号冷却的键是未经认证的 `account`（第三方登录时是 auth_token 的 32 位 hashCode），进程内 Map，2 副本各自一份 | `LoginController.java:50-54`、`:66-75`；`AssignGateRateLimiter.java:104-118` |
| （评审补）限流先于 zone 合法性校验：任意 `zone_id` 都会建一个 `rl:zone:<id>` 桶键；按现行顺序这一步还在 IP 桶之前，不受 IP 限速 | `AssignGateController.java:59`（`AssignGateService` 的 404 在其后）；`AssignGateRateLimiter.java:83-91` |
| 回包：zone 桶空 → code=100、`queue_source="ratelimit"`；IP 或冷却 → code=429。HTTP 状态恒为 200 | `AssignGateResponse.java:42-49`、`:71-76` |
| Lettuce 客户端从 `spring.data.redis.password` 取口令（空串不设），在 Bean 创建时就 `connect()` | `RateLimitConfig.java:46-58`、`:60-64`（评审更正行号：该文件只有 107 行） |
| ProxyManager 用 CAS 实现，桶空闲 TTL 为 1h | `RateLimitConfig.java:5`（`lettuce.cas` 包）、`:71`、`:89-92` |
| （评审补）`StringRedisTemplate` 注入到 `ZoneHealthProbeService` 但未使用；复合 `/actuator/health` 含 redis 指示器，探针分组不含 | `ZoneHealthProbeService.java:31`；`application.yaml:113-131` |
| XFF 只认可信代理网段；没配就取 socket 对端 | `ClientIpResolver.java:62-88`；`RateLimitProperties.java:51` |
| 镜像 WORKDIR 是 `/app`，ConfigMap 挂在 `/app/config`（Spring 外部配置覆盖 jar 内配置） | `deploy/k8s/Dockerfile.java-svc:65`；`manifests/java-svc/gateway.yaml:59-62,95-98` |
| gateway 每个 zone 2 副本；Pod 模板目前只有一条口令指纹注解 | `gateway.yaml:24`；`k8s_deploy.ps1:5009-5011` |
| Bucket4j 版本 8.10.1 | `pom.xml:87-94` |

### 1.2 login（`go/login`）登录排队

| 事实 | 出处 |
|---|---|
| `Queue.Enabled: false`；锁键 `dispatcher:lock:login_queue`；`ZoneCapacityOverride` 只有注释，没有实际配置 | `etc/login.yaml:428-442`（`:429`、`:436`、`:440-442`） |
| `QueueConf` 各字段默认值：Enabled=false，锁键全局 | `internal/config/config.go:245-258`（`:246`、`:253`） |
| 生产模式（Mode 不是 dev/test）下开排队，必须配 `Secrets.QueueToken`，长度 ≥32 字节、不是占位串，且不能与 GateToken、InternalAuth 复用，否则拒绝启动 | `secrets.go:37`、`:46-60`、`:343-347`（只有宽松档才回落到 GateToken）、`:361`、`:371-389` |
| K8s 的 login ConfigMap 在 staging / prod 不写 Mode（等于 pro）；`Secrets` 段里只有 InternalAuth | `k8s_deploy.ps1:3403`、`:3663-3665` |
| **容量回落公式向下取整**：`uint32(max(online,1) * 1.5)`，在线 0 或 1 人时容量是 1。所以队列一旦非空、在线停在 1 人，就再也放不出人 | `loginqueue/capacity.go:76-87`、`:95-100` |
| 快速通道用 Lua 原子占位；排队关闭时，带 `queue_token` 的请求也走快速通道 | `loginpregate/assigngatelogic.go:62-66`、`:96-135` |
| 排队关闭时 `/queue-status` 回 EXPIRED，客户端会重新调 assign-gate | `loginpregate/querystatuslogic.go:45-54` |
| 状态码 0/1/2/3 = ADMITTED / QUEUEING / ERROR / EXPIRED；gateway 映射成 0 / 100 / 500 / 410 | `loginqueue/queue.go:46-49`；`AssignGateService.java:41-63` |
| K8s 的 login ConfigMap 没有 `Prometheus` 段，所以 `login_queue_*` 指标在集群里抓不到 | `k8s_deploy.ps1:3558-3690`（对照 `login.yaml:321-324`） |

### 1.3 K8s 生成器现状

- 档位参数是 `-ReleaseProfile dev|staging|prod`（`k8s_deploy.ps1:36-38`），已有 `auto` 跟随型开关的先例：`-RequireClientEndpoint`（`:186-191`；`Resolve-RequireClientEndpoint`，`:1323-1328`）。
- gateway ConfigMap（`New-JavaSvcConfigMapYaml`，`:4877-4964`）：
  - `rate-limit:` 段只写了 `trusted-proxies`，由库函数 `New-GatewayRateLimitYaml` 生成（`lib/k8s_client_entry.ps1:1267-1284`），接在 `gate:` 下（`k8s_deploy.ps1:4889-4896`、`:4942`）；
  - `spring.data.redis` 只写了 host 和 port，**没有 password**（`:4936-4938`）。
- Redis 在 release 档带口令：`MMORPG_REDIS_PASSWORD` 在非 dev 档必须配置，长度 ≥12（`k8s_deploy.ps1:452-453`）。redis.yaml 在 Secret `redis-auth` 存在时加 `--requirepass`（`manifests/infra/redis.yaml:84-103`；`k8s_deploy.ps1:5790-5796`）。go-svc 的各个 ConfigMap 都带明文 `Password`（`:3459`、`:3585` 等）。
  - **结论：今天在 staging / prod 打开 Bucket4j，每次都会 NOAUTH，然后 fail-open，等于没开（推断：依 `AssignGateRateLimiter.java:99-102` 的 catch 与 Lettuce 语义，未实测）。** 另一种可能（评审补，同样未实测）：Redis 是 7.2（`redis.yaml` 的 `image: redis:7.2`），Lettuce 6 连接时先发 `HELLO 3`；不带口令时服务端回 NOAUTH，若 Lettuce 不把它当成可回落 RESP2 的错误，`connect()` 就在 Bean 创建时抛出，gateway 起不来（CrashLoop）。两种结局都由「写口令」一并修掉，这里只说明 kind 实测时可能看到的是哪一种。
- login ConfigMap（`:3558-3690`）没有 `Queue` 段，也没有 `Secrets.QueueToken`。
- 所有 zone 的 login 连的是同一个 `redis.${InfraNamespace}`（`:3583-3585`）。
- 只改 ConfigMap 不会滚动 Pod：只有 gateway 有口令指纹注解，靠 `Add-PodTemplateAnnotation` + `Get-InjectedSecretFingerprint` 实现（`:4641-4691`、`:4705-4720`、`:5009-5011`）。go-svc 和 gateway 的 ConfigMap 都没有内容哈希注解。
- 写路径的顺序是：镜像门禁 → 发布预检 → 入口预检 → `Initialize-InjectedSecrets` → deadline 门禁 → 写操作（`:5934-5943`）。

### 1.4 G1b 是否已落地：**没有**

- 锁键默认仍是全局的：`config.go:253`、`login.yaml:436`。
- 锁键原样传给 dispatcher：`servicecontext.go:470-481`（`:479`）→ `dispatcher.go:166` → `SetNX(key)`（`locker/player_locker.go:56-59`）。
- `activeZones` 只读本 zone 前缀的 GateWatcher：`servicecontext.go:153-162`、`:453-466`。它的注释说要取「∪ 非空队列的 zone」，代码里并没有实现这部分。
- `git status` 里 `go/login` 没有改动；`servicecontext.go` 最后一次提交是 `e7d9c9611`（2026-09-29）。
- **在 K8s 上的后果**：多个 zone 共用一个 Redis，全局只有一个 leader，只有 leader 所在的 zone 会出队。其它 zone 一旦有人入队，`队列非空` 会让后来的人全部入队（`assigngatelogic.go:110`），这个 zone 就锁死了，要等 `QueueEntryTTL`（1h）过期才能恢复。
- **G1b 规格已定的语义**（评审补，`launch_spec_G1b.md` §0、§3.1、§4.1、§4.5）：`Queue.DispatcherLockKey` 改为**前缀**，`Dispatcher` 只认本 zone，实际锁键 = `ZoneDispatcherLockKey(前缀, Node.ZoneId)` = `<前缀>:zone:<id>`；选主日志改为 `elected dispatcher leader zone=%d key=%s`；同一 zone 新旧版本混跑时短暂双 leader，最多多放行一批。G1b 明确把「快速通道占位只靠整集合 TTL 回收」留给 L-02（即本批）单独评估（G1b §4.7 第 2 条），见 §1.5。

### 1.5 打开排队后会变成线上行为的两条基线缺陷（评审补）

排队在 K8s 上从没开过，所以下面两条今天不可达；G4b-2 一打开就生效。Java 版已经修掉（Java 仓库 `PARITY.md`「登录排队」行有意差异 ②③）。

| # | 事实 | 出处 | 后果 |
|---|---|---|---|
| Q1 | 快速通道往 `admitted:zone:Z` 里 SADD 一个 `fast:<uuid>` 占位，并刷新**整个集合**的 TTL（AdmitTTL 60s）；全仓只有 consume 时 SREM，且只删 queueId，`fast:*` 永远不会被单独删除；dispatcher 每次放行也刷新整个集合的 TTL | `queue.go:456-482`（Lua `reserveFastPathSlotScript` 与 `TryReserveFastPathSlot`）、`:355`（consume 只 SREM queueId）、`:430-431`（放行时 SADD + `Expire` 整集合） | 已连上 gate 的快速通道玩家同时算进 online 和 admitted（`capacity.go:89-100`），余量被重复扣减；持续有人登录时集合不过期，扣到 0 后停摆，最长再等 60s 集合整体过期才放下一批 → 「放一批、停最长 60s」循环 |
| Q2 | consume 时立刻 SREM 占位，而 gate 的 `PlayerCount` 经 etcd 每几秒才刷新一次 | `queue.go:355`；`capacity.go:72-74` | 开区时 dispatcher 每拍（1s）都按「在线≈0、在途≈0」再放一满批，几秒内可放进容量的数倍（Java 版改为取走后占位再留 15s） |

---

## 2. 目标行为（档位矩阵）

| | dev（缺省 auto） | staging / prod（缺省 auto） | 显式 `on` | 显式 `off` |
|---|---|---|---|---|
| gateway `gate.rate-limit.enabled` 与阈值 | 不写（jar 默认关；ConfigMap 与今天逐字相同） | 写 `enabled: true` 和开服档阈值 | 写（任何档位） | 不写；非 dev 档打警告 |
| gateway `spring.data.redis.password` | 只在 `MMORPG_REDIS_PASSWORD` 非空时写（dev 回落为空串，所以不写） | 写（与开关无关） | 同左 | 同左 |
| login `Queue` 段 | 不写 | 写：`Enabled: true`，其余字段从 login.yaml 镜像，锁键带 zone，`ZoneCapacityOverride` 只含本 zone | 写 | 不写；非 dev 档打警告 |
| login `Secrets.QueueToken` | 不写 | 写 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`（必填，≥32，不是占位串，与 Gate / InternalAuth 不同） | dev 档：环境变量为空就不写（login 宽松档回落 GateToken 并打 WARN） | 不写，也不要求这个环境变量 |
| Pod 模板 `mmorpg.io/peak-shaving-hash` | 无 | gateway 与 login 各有一条（gateway 的输入含 Redis 口令，见 §4.7） | 有 | 无（开 → 关时模板变化，自动滚动） |
| gateway 带 `queue_token` 的 assign-gate（G4b-J0 之后） | 限流关 → 放行（与今天相同） | 过 zone 桶与 IP 桶，跳过账号冷却 | 同左 | 同 dev |

参数不粘滞，与 `-GateRouterMode` 口径一致：用 `off` 止血之后，下一次部署（包括 `k8s_zone_rollback.ps1` 第 6 步）不传这个参数，就会按档位恢复为开。

---

## 3. 文件清单

| 文件 | 类型 | 要点 |
|---|---|---|
| `tools/scripts/k8s_deploy.ps1` | 修改 | ① 两个新参数；② 档位解析与阈值常量表（单一来源）；③ `Show-PeakShavingPlan` 打印计划与警告；④ `Initialize-InjectedSecrets` 解析排队密钥并校验它与其它密钥不同；⑤ gateway ConfigMap 写阈值和 Redis 口令；⑥ login ConfigMap 写 `Queue` 与 `Secrets.QueueToken`；⑦ `Add-PodTemplateAnnotation` 支持附加注解，gateway 与 login 写削峰指纹；⑧ `:1375` 注释更正（行为不变） |
| `tools/scripts/lib/k8s_client_entry.ps1` | 修改 | `New-GatewayRateLimitYaml` 加可选参数 `-Settings`（阈值在前、trusted-proxies 在后，共用一个 `rate-limit:` 键）；不传时行为逐字不变；文件头 `:72` 的签名说明同步更新 |
| `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | 修改 | `$ProdEnv` 加 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`；新增「削峰（G4b）」一节，用例见 §6 |
| `tools/scripts/tests/k8s_client_entry_contract.tests.ps1` | 修改 | 新增 1 个库用例（§6 L1）；`:539-548` 的既有用例不动 |
| `deploy/k8s/README.md` | 修改 | 「Optional Flags」加两个参数、新环境变量，以及调阈值的操作要点（要删 `rl:zone:*`）。评审补三条：① `dev_tools.ps1` / `k8s_image.ps1` / 回档脚本**不透传**这两个参数，`off` 止血要直接调 `k8s_deploy.ps1`；② K8s 上 G1b 之后实际锁键是 `dispatcher:lock:login_queue:zone:<id>:zone:<id>`，排查时按它查（`docs/ops/login-queue-stress-runbook.md:88` 的键名在 K8s 上不适用）；③ 对**已在运行**的 staging / prod zone 首次打开排队前，先核对该 zone 当前在线（各 gate `PlayerCount` 之和）低于 `LoginQueueZoneCapacity`，否则新登录全部排队直到在线降下来（R20） |
| `docs/design/open-server-rate-limit-design.md` | 修改 | 新增「十一、K8s 按发布档位开启（G4b）」；`§十` 的第 8–10 步标注状态；登记客户端退避（另仓）与 §8 的偏离 |
| `docs/design/login-queue-2026-05.md` | 修改 | §6（`:194`）事实更正：已拆出独立的 `Secrets.QueueToken`，见 `secrets.go:105-107`、`:343-347`；§10 补 K8s 取值与本 zone 锁键；§15 补 `-LoginQueue off` 的止血用法 |
| `docs/design/gateway-k8s-deployment.md` | 修改 | `:53-55` 附近补一句：限流按档位开启；gateway 现在带 Redis 口令 |
| `docs/design/zone_data_rollback.md`（`:234` 那一行）、`docs/ops/merge-zone-runbook.md`（`:658` 那一行） | 修改（只追加该行） | staging / prod 第 6 步还需要环境变量 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`。这两个文件有他人的未提交改动，只在原句后追加，不动其它段落 |
| （G4b-J0，**G4b-1 的前置，必做**）`java/gateway_node/.../controller/AssignGateController.java`；新增 `src/test/java/com/game/gateway/controller/AssignGateControllerTest.java` | 修改 / 新增 | 带 `queue_token` 不再整个跳过限流，只跳过账号冷却，见 §4.10 |
| （G4b-J1 / J2，建议同批，可拆，需拍板 §10 #2）`java/gateway_node/.../ratelimit/AssignGateRateLimiter.java`、`src/test/.../AssignGateRateLimiterTest.java` | 修改 | J1：先判 IP 桶、再判 zone 桶，zone 桶空时退回 IP 令牌；J2：账号冷却键带客户端 IP。见 §4.10 |
| `docs/design/k8s-client-entry.md`（`:698` 那一行，可选） | 修改（只追加该行） | staging / prod 准备清单补 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET` |

**不改**：
- `AGENTS.md`、`docs/PROGRESS.md`（Docs 阶段统一追加）、`docs/README.md`（本批不新建文档）；
- Go / C++ 代码、`go/login/etc/login.yaml`、jar 内 `application.yaml`（本地 dev 与 compose 保持关闭）；
- `deploy/docker-compose*.yml`、`deploy/login-stack.linux`；
- `release_preflight.ps1`：它检查的是 jar 文件（`:379-410`），jar 内保持关闭仍然正确。可选：把 `:406` 的 WARN 文案改成「jar 内缺省关是预期，K8s 由 k8s_deploy.ps1 按档位打开（G4b）」。

**为什么不加 Java 的 Spring profile**（例如 `application-prod.yaml`）：K8s 已经用挂载的 `/app/config/application.yaml` 覆盖 jar 内配置，`trusted-proxies`、`token-secret` 都走这条路。再加一份 profile 文件，就有了第二个真相来源，而且调一次阈值要重打镜像。

---

## 4. 详细设计（照写）

### 4.1 参数（`k8s_deploy.ps1` 的 param 块，放在 `-LoginDevPasswordAuth`（`:230-235`）之后）

```powershell
	# 削峰(G4b,docs/design/open-server-rate-limit-design.md §十一):gateway Bucket4j 限流 / login 登录排队。
	# auto(缺省)= 跟随 -ReleaseProfile:dev 关(ConfigMap 与 G4b 之前逐字相同);staging / prod 开,阈值见 $script:PeakShavingLaunchTier。
	# off 只用于事故止血与基线压测(AGENTS §6 对比的"关削峰"那一轮);on 只用于 dev 档联调。
	# 不粘滞:以 off 运行的环境,下一次部署(含 k8s_zone_rollback.ps1 Step 6、dev_tools.ps1 包装入口)不传就按档位恢复为开。
	# 切换会改 Pod 模板上的 mmorpg.io/peak-shaving-hash 注解,gateway / login 自动滚动,不需要手工 rollout restart。
	[ValidateSet("auto", "on", "off")]
	[string]$GatewayRateLimit = "auto",
	# login 排队开时 staging / prod 必须注入 MMORPG_LOGIN_QUEUE_TOKEN_SECRET(≥32,与 GATE / INTERNAL_AUTH 两把不同)。
	# 前置:G1b(排队 leader 锁按 zone 拆)已合入;本脚本另写本 zone 的 DispatcherLockKey 作纵深防御(见 New-LoginQueueYaml)。
	[ValidateSet("auto", "on", "off")]
	[string]$LoginQueue = "auto",
```

局部变量**不要**叫 `$gatewayRateLimit` / `$loginQueue`。PowerShell 变量名不区分大小写，同名会遮住参数，`:3407` 有同类的说明。

### 4.2 档位解析与阈值常量（放在 `$script:BattleWorkload = $null`（`:313`）之后、dot-source 库（`:315`）之前）

```powershell
# 削峰(G4b)开服档起步值。单一来源:ConfigMap 生成、Pod 指纹、部署打印都只读这里。全部「待压测确认」(L-14)。
#   ZoneDefaultRps/Burst   open-server-rate-limit-design.md:48「开服日 / 大版本回归」档
#   IpRps/IpBurst          java/gateway_node application.yaml:41-42(设计稿 §5.2 示例为 2/10;取宽者:网吧 / 运营商 NAT 下
#                          一次登录要扣 2 个 IP 令牌(/api/login + /api/assign-gate 共用 rl:ip:*),见 G4b 规格 §8)
#   AccountCooldownMs      设计稿 :157 与 application.yaml:43 一致
#   LoginQueueZoneCapacity login-queue-2026-05.md:263-265 / login.yaml:440-442 的示例值。注意它是**本 zone 同时在线上限**,不是速率。
$script:PeakShavingLaunchTier = [ordered]@{
	ZoneDefaultRps         = 2000
	ZoneDefaultBurst       = 5000
	IpRps                  = 5
	IpBurst                = 20
	AccountCooldownMs      = 5000
	LoginQueueZoneCapacity = 5000
}
$script:PeakShavingHashAnnotation = 'mmorpg.io/peak-shaving-hash'
# 由 Initialize-InjectedSecrets 解析(只在本次部署 login 且排队开时)。
$script:LoginQueueTokenSecret = ""

function Resolve-PeakShavingSwitch {
	param(
		[Parameter(Mandatory = $true)][string]$Value,
		# 参数名不要用 $Profile:它会遮住 PowerShell 自动变量 $PROFILE。
		[Parameter(Mandatory = $true)][string]$ReleaseProfileName
	)
	switch ($Value) {
		'on'  { return $true }
		'off' { return $false }
		default { return ($ReleaseProfileName -ne 'dev') }
	}
}
$script:GatewayRateLimitOn = Resolve-PeakShavingSwitch -Value $GatewayRateLimit -ReleaseProfileName $ReleaseProfile
$script:LoginQueueOn = Resolve-PeakShavingSwitch -Value $LoginQueue -ReleaseProfileName $ReleaseProfile
```

### 4.3 gateway：阈值字典与 rate-limit 片段（`k8s_deploy.ps1`，放在 `New-JavaSvcConfigMapYaml` 之前）

```powershell
# gate.rate-limit 下要写的阈值。关 = $null(什么都不写,落回 jar 内 enabled:false)。键是 Spring 属性名(kebab-case),
# 必须与 jar 内 application.yaml 同名 —— 拼错 Spring 静默忽略(契约测试逐键核对)。
function Get-GatewayRateLimitSettings {
	if (-not $script:GatewayRateLimitOn) { return $null }
	$t = $script:PeakShavingLaunchTier
	# release_preflight.ps1 的「rps ≥ 1、burst ≥ rps」只查 jar 内 application.yaml(F 节);K8s 上真正生效的是这里写进
	# ConfigMap 的值,同一条规则必须在这里再守一次,否则改坏常量表会把闸门焊死(rps 0 = 全量排队)或自相矛盾。
	foreach ($pair in @(@('ZoneDefaultRps', 'ZoneDefaultBurst'), @('IpRps', 'IpBurst'))) {
		$rps = [long]$t[$pair[0]]
		$burst = [long]$t[$pair[1]]
		if ($rps -lt 1 -or $burst -lt $rps) {
			throw "内部错误:PeakShavingLaunchTier.$($pair[0])=$rps / $($pair[1])=$burst 不满足 rps ≥ 1 且 burst ≥ rps。"
		}
	}
	if ([long]$t.AccountCooldownMs -lt 0) { throw "内部错误:PeakShavingLaunchTier.AccountCooldownMs=$($t.AccountCooldownMs) 不能为负。" }
	return [ordered]@{
		'enabled'             = $true
		'zone-default-rps'    = [long]$t.ZoneDefaultRps
		'zone-default-burst'  = [long]$t.ZoneDefaultBurst
		'ip-rps'              = [long]$t.IpRps
		'ip-burst'            = [long]$t.IpBurst
		'account-cooldown-ms' = [long]$t.AccountCooldownMs
	}
}

# ConfigMap 与 Pod 指纹共用同一段文本(DRY):两处算出来不一致,指纹就失去意义。
function New-GatewayRateLimitBlockText {
	return New-GatewayRateLimitYaml -TrustedProxies $GatewayTrustedProxies -Indent '  ' -Settings (Get-GatewayRateLimitSettings)
}
```

`New-JavaSvcConfigMapYaml` 的改动（`:4889-4896`、`:4924-4946`）：

```powershell
	$gatewayRateLimitBlock = ''
	$gatewayRedisPasswordLine = ''
	if ($SvcName -eq 'gateway') {
		$gatewayRateLimitBlock = New-GatewayRateLimitBlockText
		if (-not [string]::IsNullOrEmpty($gatewayRateLimitBlock)) { $gatewayRateLimitBlock = "`n" + $gatewayRateLimitBlock }
		# Bucket4j 的 Lettuce 客户端只从 spring.data.redis.password 取口令(RateLimitConfig.java 的 bucket4jRedisClient)。
		# release 档的 Redis 带 --requirepass(infra/redis.yaml),不写口令 = 每次 NOAUTH → AssignGateRateLimiter
		# 的 catch 一律放行(fail-open),限流开了等于没开。与 go-svc ConfigMap、本 ConfigMap 的数据源口令同为明文
		# (同 namespace 的 go-svc-login-config 已经带同一个值,改走 Secret 不缩小暴露面)。空口令不写:dev 产物逐字不变。
		if (-not [string]::IsNullOrEmpty($script:RedisPassword)) {
			$gatewayRedisPasswordLine = "`n      password: `"$($script:RedisPassword)`""
		}
	}
```

gateway 模板只改一行（原 `:4938`）：

```
      port: 6379${gatewayRedisPasswordLine}
```

staging / prod 的生成结果：

```yaml
gate:
  token-secret: "..."
  rate-limit:
    enabled: true
    zone-default-rps: 2000
    zone-default-burst: 5000
    ip-rps: 5
    ip-burst: 20
    account-cooldown-ms: 5000
    trusted-proxies:        # 只在给了 -GatewayTrustedProxies 时出现
      - "10.244.0.0/16"
```

不写 `wave.*`（开服波次需要一个开服时刻，属于运营决策，§10 #7），不写 `zone-overrides`，也不写死键 `queue-timeout-ms`。

副作用（评审补，行为向好，不需改代码）：写上口令后，复合 `/actuator/health` 里的 redis 指示器从 DOWN 变为 UP。K8s 探针打的是不含外部依赖的分组（`application.yaml:113-131`；`gateway.yaml:65-87`），所以不影响就绪；只是人工排障时看到的状态变了。

### 4.4 库函数 `New-GatewayRateLimitYaml`（`lib/k8s_client_entry.ps1:1267-1284`）

```powershell
function New-GatewayRateLimitYaml {
	param(
		[Parameter(Mandatory = $true)][AllowEmptyString()][string]$TrustedProxies,
		[string]$Indent = '  ',
		# 削峰阈值(G4b):键 = gate.rate-limit 下的 Spring 属性名(kebab-case),值只收 bool / 整数;$null 或空 = 不写。
		# 与 trusted-proxies 共用同一个 rate-limit: 键(Spring 的 SnakeYAML 遇重复键直接拒启)。
		[AllowNull()][System.Collections.IDictionary]$Settings = $null
	)

	$settingLines = @()
	if ($null -ne $Settings) {
		foreach ($key in $Settings.Keys) {
			if ([string]$key -cnotmatch '^[a-z][a-z0-9-]*$') {
				throw "内部错误:gate.rate-limit 的设置键 '$key' 不是 kebab-case 属性名。"
			}
			$v = $Settings[$key]
			# $null 单列一支:落到 else 里的 $v.GetType() 会先抛"对空值调用方法",报错就不再点名是哪个键。
			$rendered = if ($null -eq $v) { throw "内部错误:gate.rate-limit.$key 的值为空。" }
				elseif ($v -is [bool]) { if ($v) { 'true' } else { 'false' } }
				elseif ($v -is [int] -or $v -is [long]) { ([long]$v).ToString([System.Globalization.CultureInfo]::InvariantCulture) }
				else { throw "内部错误:gate.rate-limit.$key 的值只收 bool / 整数(实际类型 $($v.GetType().Name))。" }
			$settingLines += "${Indent}  ${key}: $rendered"
		}
	}

	$cidrs = ConvertTo-ClientEntryCidrList -CommaSeparated $TrustedProxies
	if ($cidrs.Count -gt 0) {
		$invalid = Get-ClientEntryInvalidCidrs -CommaSeparated $TrustedProxies
		if ($invalid.Count -gt 0) { throw "-GatewayTrustedProxies 含非法 CIDR:$($invalid -join ', ')" }
	}
	if ($settingLines.Count -eq 0 -and $cidrs.Count -eq 0) { return '' }

	$lines = @("${Indent}rate-limit:") + $settingLines
	if ($cidrs.Count -gt 0) {
		$lines += "${Indent}  trusted-proxies:"
		foreach ($cidr in $cidrs) { $lines += "${Indent}    - $(ConvertTo-ClientEntryYamlString -Value $cidr)" }
	}
	return (ConvertTo-ClientEntryLf -Text ($lines -join "`n"))
}
```

- 不传 `-Settings` 时，输出与 `:539-548` 的既有库用例逐字一致。
- `k8s_deploy.ps1:1377` 的「本次会不会丢掉 trusted-proxies」判定**不传 `-Settings`**，语义保持不变。只把 `:1375` 的注释改成「只看 trusted-proxies 是否为空；阈值另由 -Settings 写，不参与本判定」。

### 4.5 login：`New-LoginQueueYaml`（`k8s_deploy.ps1`，放在 `New-GoSvcConfigMapYaml` 之前）

```powershell
<#
.SYNOPSIS
	login.yaml 顶层的 Queue 段(G4b)。关 = 空串(不写 → go-zero 零值 Enabled=false,产物与 G4b 之前逐字相同)。
.DESCRIPTION
	- 除 Enabled / DispatcherLockKey / ZoneCapacityOverride 外,全部从 go/login/etc/login.yaml 镜像(契约值单一来源)。
	- DispatcherLockKey = <login.yaml 的基名>:zone:<本 zone>。G1b 之后 login 把它当前缀、自己再拼 :zone:<Node.ZoneId>,实际锁键是
	  <基名>:zone:<id>:zone:<id>,每个 zone 仍恰有一个 leader;这里显式带 zone 防的是另一种情形:kubectl rollout undo 把 login
	  退回 G1b 之前的镜像,而 ConfigMap 不随之回退、仍是开排队的这一份。旧镜像原样使用配置值,写全局基名就会退回「全集群一个
	  leader、其它 zone 锁死」(servicecontext.go:453-466 的旧实现)。同一 zone 新旧镜像混跑的窗口里两把键不互斥,短暂双 leader,
	  最多多放行一批(G1b 规格 §4.5 已接受)。若 G1b 最终改成「一个 leader 覆盖全部 zone」,必须删掉这一行,否则 N 个 leader 同时
	  出队所有 zone、超发准入 —— 契约测试用例 15 钉住 G1b 的前缀语义。
	- ZoneCapacityOverride 只写本 zone:每个 zone 的 login 只出队本 zone。不配会落到 capacity.go:76-87 的回落公式(向下取整,
	  在线 0/1 人时容量为 1),队列非空时可能永远放不出人。
#>
function New-LoginQueueYaml {
	param([Parameter(Mandatory = $true)][int]$CurrentZoneId)

	if (-not $script:LoginQueueOn) { return '' }
	$y = 'go/login/etc/login.yaml'
	$dispatchInterval = Get-AuthoritativeScalar -RelativePath $y -KeyPath 'Queue.DispatchInterval'
	$admitTtl         = Get-AuthoritativeScalar -RelativePath $y -KeyPath 'Queue.AdmitTTL'
	$entryTtl         = Get-AuthoritativeScalar -RelativePath $y -KeyPath 'Queue.QueueEntryTTL'
	$softCap          = Get-AuthoritativeScalar -RelativePath $y -KeyPath 'Queue.SoftCapMultiplier'
	$retryAfterMs     = Get-AuthoritativeScalar -RelativePath $y -KeyPath 'Queue.DefaultRetryAfterMs'
	$lockTtl          = Get-AuthoritativeScalar -RelativePath $y -KeyPath 'Queue.DispatcherLockTTL'
	$lockKeyBase      = Get-AuthoritativeScalar -RelativePath $y -KeyPath 'Queue.DispatcherLockKey'
	$capacity = [long]$script:PeakShavingLaunchTier.LoginQueueZoneCapacity
	if ($capacity -lt 1 -or $capacity -gt [uint32]::MaxValue) {
		throw "内部错误:LoginQueueZoneCapacity=$capacity 不在 1..$([uint32]::MaxValue)(Go 侧是 uint32)。"
	}
	if ($CurrentZoneId -lt 1) { throw "生成 login Queue 段失败:ZoneId=$CurrentZoneId 必须为正。" }

	return @"
# 登录排队(G4b,按 -LoginQueue / -ReleaseProfile 生成)。除下面三项外全部镜像 go/login/etc/login.yaml。
# DispatcherLockKey 带本 zone:新镜像把它当前缀再拼一次 zone(实际锁键 ...:zone:N:zone:N),旧镜像原样用,两种都按 zone 隔离
# (见 k8s_deploy.ps1 New-LoginQueueYaml)。
# ZoneCapacityOverride 是本 zone 同时在线上限(不是速率),开服档起步值,待压测确认。
Queue:
  Enabled: true
  DispatchInterval: $dispatchInterval
  AdmitTTL: $admitTtl
  QueueEntryTTL: $entryTtl
  SoftCapMultiplier: $softCap
  DefaultRetryAfterMs: $retryAfterMs
  DispatcherLockTTL: $lockTtl
  DispatcherLockKey: "${lockKeyBase}:zone:${CurrentZoneId}"
  ZoneCapacityOverride:
    "${CurrentZoneId}": $capacity
"@
}
```

注意：here-string 里的注释**不要出现 `$`**。

`New-GoSvcConfigMapYaml` 的改动：只在 `$SvcName -eq 'login'` 时计算。`k8s_migrate_gate.tests.ps1:51-55` 用 AST 抽 `Apply-OneGoSvc` 跑 trade / friend / data-service，不加载新函数，所以非 login 路径不能调用它们。

```powershell
	$loginQueueBlock = ''
	$loginQueueTokenSecretBlock = ''
	if ($SvcName -eq 'login') {
		$queueYaml = New-LoginQueueYaml -CurrentZoneId $CurrentZoneId
		if (-not [string]::IsNullOrEmpty($queueYaml)) {
			$loginQueueBlock = "`n" + $queueYaml
			# Secrets.QueueToken 与 InternalAuth 同处一个 Secrets: 键(不能另起一个 Secrets:)。dev 档允许为空:login 宽松档回落 GateToken
			# 并打 WARN(secrets.go:343-347);staging / prod 由 Initialize-InjectedSecrets 保证非空、≥32、与另两把不同。
			if (-not [string]::IsNullOrEmpty($script:LoginQueueTokenSecret)) {
				$loginQueueTokenSecretBlock = "`n  QueueToken:`n    Value: `"$($script:LoginQueueTokenSecret)`""
			}
		}
	}
```

login 模板只改两行（原 `:3665`、`:3668`）：

```
    Value: "${internalAuthSecret}"${loginQueueTokenSecretBlock}
...
RequireClientEndpoint: ${requireClientEndpointValue}${loginDevPasswordAuthBlock}${loginQueueBlock}
```

### 4.6 密钥注入（`Initialize-InjectedSecrets`，`:431-486`，追加在管理面口令那段之前）

```powershell
	# 登录排队令牌的 HMAC 密钥(login Secrets.QueueToken)。生产模式开了排队就必需,且不得与 GateToken / InternalAuth 复用
	# (secrets.go:361、:371-389;不满足 login 拒启 → CrashLoopBackOff)。只在本次部署 login 且排队开时解析:
	# 关排队 / infra-up / -SkipGoSvc 都不因它被阻断。dev 档回落空串(不写 → login 回落 GateToken 并告警)。
	$script:LoginQueueTokenSecret = ""
	if ($script:LoginQueueOn -and (Test-ZoneLoginDeployed)) {
		$script:LoginQueueTokenSecret = Resolve-InjectedSecret -EnvName "MMORPG_LOGIN_QUEUE_TOKEN_SECRET" `
			-DevFallback "" -ReleaseProfile $ReleaseProfile -Purpose "login 登录排队令牌 HMAC 密钥(Secrets.QueueToken)" -MinLength 32
		if (-not [string]::IsNullOrEmpty($script:LoginQueueTokenSecret)) {
			if ($script:LoginQueueTokenSecret -ceq $script:GateTokenSecret) {
				throw "MMORPG_LOGIN_QUEUE_TOKEN_SECRET 必须与 MMORPG_GATE_TOKEN_SECRET 不同:login 拒绝跨用途复用主密钥(go/login/internal/config/secrets.go 的复用检查),部署出来只会 CrashLoopBackOff。"
			}
			if ($script:LoginQueueTokenSecret -ceq $script:InternalAuthSecret) {
				throw "MMORPG_LOGIN_QUEUE_TOKEN_SECRET 必须与 MMORPG_INTERNAL_AUTH_SECRET 不同:login 拒绝跨用途复用主密钥(go/login/internal/config/secrets.go 的复用检查),部署出来只会 CrashLoopBackOff。"
			}
		}
	}
```

- 报错只点环境变量名，**不回显值**。
- 用 `-ceq` 原样比较，与 Go 侧 `hmac.Equal` 一致，不做 trim。
- 为什么用明文 `Value` 而不是 `Env`：同一个 `Secrets` 段里，InternalAuth 已经是明文 Value（`:3663-3665`）。兄弟项保持同一形态，暴露面相同（§8 #5）。

### 4.7 Pod 模板指纹注解（让开关和阈值变更能自动滚动）

1. **扩展 `Add-PodTemplateAnnotation`**（`:4641-4691`），新增可选参数：

   ```powershell
   		# 附加注解(按迭代顺序写在 -Key 之后)。同一 Pod 模板只能有一个 annotations: 段,多条注解必须一次写入。
   		[AllowNull()][System.Collections.IDictionary]$AdditionalAnnotations = $null
   ```

   - 把 `(-Key,-Value)` 和附加项合成一个列表，每一项都做与原来相同的值校验（`:4649-4651`）。
   - 键名再加一条校验 `^[a-z0-9]([a-z0-9.-]*[a-z0-9])?/[a-z0-9]([a-z0-9._-]*[a-z0-9])?$`；键重复就 throw。
   - 生成的行依次是：说明注释、`annotations:`、`-Key` 那一行、附加行。**主注解必须排第一行**，`k8s_deploy_contract.tests.ps1:1476-1489` 的 `Get-GatewayAdminApiKeyHash` 正则依赖这一点。
   - 不传附加注解时输出逐字不变。
2. **gateway**（`Apply-JavaSvcManifests`，`:5003-5012`）：

   ```powershell
   			$peakAnnotations = $null
   			if ($script:GatewayRateLimitOn) {
   				# 指纹输入 = 写进 ConfigMap 的整段 rate-limit 文本(含 trusted-proxies)+ Redis 口令:阈值 / 开关 / 代理网段 / 口令任一变化
   				# → 模板变 → 滚动。口令必须在内:Lettuce 只在启动时读 spring.data.redis.password,轮换 MMORPG_REDIS_PASSWORD 而 gateway
   				# 不滚动,旧口令被 Redis 拒绝后 AssignGateRateLimiter 一律 fail-open,限流静默失效。
   				# Get-InjectedSecretFingerprint 是通用的「域分隔串 + 值」SHA-256 截断(48 位,不可反推),域分隔串写用途。
   				$peakAnnotations = [ordered]@{
   					$script:PeakShavingHashAnnotation = (Get-InjectedSecretFingerprint -SecretName 'peak-shaving/gateway' `
   						-Value ((New-GatewayRateLimitBlockText) + "`n" + $script:RedisPassword))
   				}
   			}
   			$manifestContent = Add-PodTemplateAnnotation -SvcName $svcName -ManifestContent $manifestContent `
   				-Key $script:GatewayAdminApiKeyHashAnnotation `
   				-Value (Get-InjectedSecretFingerprint -SecretName $script:GatewayAdminApiKeySecretName -Value $script:GatewayAdminApiKey) `
   				-AdditionalAnnotations $peakAnnotations
   ```

3. **login**（`Apply-OneGoSvc`，`:4779-4839`）：加在 manifest 本地渲染段里，即 `$isLoginDevPassword` 判断之前，早于任何集群写操作。

   ```powershell
   	if ($SvcName -eq 'login' -and $script:LoginQueueOn) {
   		# 指纹输入 = Queue 段 + 排队密钥:只改 ConfigMap 时 Pod 模板不变、apply 是 no-op,login 进程一直按旧配置跑。
   		$fingerprintInput = (New-LoginQueueYaml -CurrentZoneId $CurrentZoneId) + "`n" + $script:LoginQueueTokenSecret
   		$manifestContent = Add-PodTemplateAnnotation -SvcName $SvcName -ManifestContent $manifestContent `
   			-Key $script:PeakShavingHashAnnotation -Value (Get-InjectedSecretFingerprint -SecretName 'peak-shaving/login' -Value $fingerprintInput)
   	}
   ```

   login 的 manifest 目前没有 annotations（`manifests/go-svc/login.yaml:28-32`），可以直接用。

   注意（评审补）：`Apply-OneGoSvc` 被 `k8s_migrate_gate.tests.ps1` 用 AST 单独抽出来跑 trade / friend / data-service，`$script:LoginQueueOn` 在那里未定义（`$null`）。上面的条件把 `$SvcName -eq 'login'` 放在 `-and` 左边，短路后不会求值右边，也不会调用未加载的 `New-LoginQueueYaml`；**不要调换两边顺序**。
4. **只改 Redis 口令时 login 不滚动**是既有缺口（所有 go-svc 的 ConfigMap 都没有内容哈希），不在本批修；gateway 因为新依赖了这把口令且失效时静默 fail-open，所以在上面单独纳入指纹。

### 4.8 写路径打印与警告

新增 `Show-PeakShavingPlan`，在 `:5940` 的 `Assert-ClientEntryDeployPreflight` 之后、`Initialize-InjectedSecrets` 之前调用：

```powershell
function Show-PeakShavingPlan {
	$t = $script:PeakShavingLaunchTier
	$gw = if ($script:GatewayRateLimitOn) { 'on' } else { 'off' }
	$lq = if ($script:LoginQueueOn) { 'on' } else { 'off' }
	Write-Host "PeakShaving: gateway_rate_limit=$gw(-GatewayRateLimit $GatewayRateLimit) login_queue=$lq(-LoginQueue $LoginQueue) profile=$ReleaseProfile 起步值(待压测确认) zone=$($t.ZoneDefaultRps)/s burst=$($t.ZoneDefaultBurst) ip=$($t.IpRps)/s burst=$($t.IpBurst) cooldown=$($t.AccountCooldownMs)ms queue_capacity=$($t.LoginQueueZoneCapacity)/zone"
	if ($ReleaseProfile -ne 'dev' -and -not $script:GatewayRateLimitOn -and (Test-ZoneGatewayDeployed)) {
		Write-Warning "[削峰]GatewayRateLimit=off(ReleaseProfile=$ReleaseProfile):gateway 限流关闭。只用于事故止血或基线压测;参数不粘滞,下次部署不传即恢复为开。"
	}
	if ($ReleaseProfile -ne 'dev' -and -not $script:LoginQueueOn -and (Test-ZoneLoginDeployed)) {
		Write-Warning "[削峰]LoginQueue=off(ReleaseProfile=$ReleaseProfile):login 排队关闭,在排玩家的 queue_token 变为 EXPIRED 后重走快速通道。参数不粘滞。"
	}
	if ($script:GatewayRateLimitOn -and (Test-ZoneGatewayDeployed) -and (ConvertTo-ClientEntryCidrList -CommaSeparated $GatewayTrustedProxies).Count -eq 0) {
		Write-Warning "[削峰]gateway限流已开但未配置可信代理(-GatewayTrustedProxies):IP 桶按 socket 对端分,集群内直连的 robot 按 Pod IP 共用一个桶(每秒 $($t.IpRps)、突发 $($t.IpBurst));前面若有未登记的 L7 代理,全体玩家共用一个桶。"
	}
}
```

- 已有 Ingress 却不给 trusted proxies 的情况，现有 preflight 已经报错拒绝（`lib/k8s_client_entry.ps1:1453-1460`；`k8s_deploy.ps1:1386-1394`），这里不重复。
- 警告文本以不含空格的 `[削峰]Xxx` 开头。原因：Write-Warning 可能在空格处折行（参见 `k8s_deploy_contract.tests.ps1:1504-1507` 的说明），测试只匹配这个开头。

### 4.9 不改、但要知道的

- `k8s_zone_rollback.ps1`、`dev_tools.ps1`、`k8s_image.ps1` 不透传两个新参数。它们会透传 `-ReleaseProfile`，`auto` 就随档位开启。回档脚本第 0 步 b 的静态预检（同参数加 `-DryRun` 跑一遍 `k8s_deploy.ps1`，`k8s_zone_rollback.ps1:25-27`）会提前发现缺 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`，在停服之前就拒绝。**评审补**：因为包装入口不透传，`off` 止血只能直接调 `k8s_deploy.ps1 -Command zone-up … -GatewayRateLimit off`；README 要写明这条命令，免得事故时在 `dev_tools.ps1` 上找不到参数（是否补透传见 §10 #9）。
- 运行期改阈值需要清 Redis：Bucket4j 只在桶不存在时用新配置，桶在持续流量下 TTL 一直刷新，不会过期。所以改 rps/burst 之后要在维护窗口执行 `redis-cli --scan --pattern 'rl:zone:*' | xargs -r redis-cli DEL`（`rl:ip:*` 同理）。代价是桶被清空后会重新拿到一次满额 burst。（推断：`AssignGateRateLimiter.java:84-85` 用的是 `build(key, supplier)`，没有 `withImplicitConfigurationReplacement`；依 Bucket4j 8.x 语义，未实测。）写进 README 与设计稿 §十一。

### 4.10 G4b-J：gateway 限流在「打开」那一刻才暴露的三个面

三条都是限流关着时不可达、一打开就生效的问题。J0 是 G4b-1 的前置（必做）；J1、J2 建议同批，可拆，需拍板 §10 #2。三条 Java 版都已按同样口径做过（Java 仓库 `PARITY.md`「开服限流」行有意差异 ④⑧②）。

#### J0（必做，G4b-1 前置）：`queue_token` 不再整个绕过限流

- **问题**：`AssignGateController.java:51-57` 只要 `queue_token` 非空就直接调 `assignGateService.assignGate`，gateway 不验签（密钥在 login），也不知道 login 排队开没开。login 排队关闭时，带 token 的请求不进重入分支、直接走快速通道签 gate 票（`assigngatelogic.go:62-66`、`:90-93`）。所以 G4b-1 先合、或 `-LoginQueue off` 止血期间，任何客户端填一个任意 `queue_token` 就绕过 zone 桶、IP 桶和冷却，拿到 gate 票；排队开着时伪造 token 拿不到票，但每个请求都打一次 login gRPC + HMAC，同样不受限。违反 §11.3「安全路径 fail-closed」。
- **改法**（约 5 行，只改控制器，`AssignGateRateLimiter` 不动）：

  ```java
  // 带 queue_token 的重入照样过 zone 桶与 IP 桶,只跳过账号冷却。gateway 验不了 token(密钥在 login),
  // 也不知道 login 排队开没开 —— 排队关着时 login 对带 token 的请求直接走快速通道签票,无条件放行 =
  // 任意伪造一个字段就绕过整个限流。正常轮询走 /api/queue-status(不限流);带 token 重调 assign-gate
  // 只发生在网络抖动后的重试,偶尔多扣一个令牌可以接受。冷却要跳过:重试常在 5s 窗口内,会被自己挡住。
  boolean reentry = req.getQueueToken() != null && !req.getQueueToken().isBlank();
  RateLimitDecision decision = limiter.check(req.getZoneId(), ip, reentry ? null : req.getAccount());
  ```

  删掉原来的提前 return 与其注释，后面的 `isQueue` / `isDeny` / `assignGate` 分支不变。`account == null` 时 `check` 本来就跳过冷却（`AssignGateRateLimiter.java:105`）。
- **客户端契约**：不新增回包码。重入被 zone 桶挡住时回 `code=100, queue_source="ratelimit"`（不带新 token），客户端保留原 `queue_token` 按 `retry_after_ms` 重试即可（robot 已是这样：`robot/main.go:666-670` 把 token 留在 req 上）；IP 桶空回 429。
- **测试**：新增 `src/test/java/com/game/gateway/controller/AssignGateControllerTest.java`（纯 Mockito，mock `AssignGateService` / `AssignGateRateLimiter` / `ClientIpResolver`，不起 Spring 上下文）：
  - `reentryWithQueueToken_stillRateLimited`：limiter 返回 `deny("IP_RATE_LIMIT")` 时回 code=429，`verify(assignGateService, never()).assignGate(any())`；
  - `reentryWithQueueToken_skipsAccountCooldown`：`verify(limiter).check(eq(1L), anyString(), isNull())`；
  - `freshRequest_passesAccountToLimiter`：不带 token 时第三个参数等于请求里的 account。

#### J1（建议）：先判 IP 桶、再判 zone 桶

- **问题**：现在先扣 zone 令牌，再判 IP 桶（`AssignGateRateLimiter.java:83-98`）。一个 IP 每秒发 2000 个请求，每个请求都先扣掉 1 个 zone 令牌，然后才在 IP 桶处被拒。结果是这一个 IP 就能把整个 zone 的入口压成「排队」。同理，任意 `zone_id` 建桶键这一步也不受 IP 限速（R14）。限流关闭时没有这个面，**打开就会出现**，违反 §11.3「默认拒绝、纵深防御」。Java 版已经是先 IP 后 zone，并且退还 IP 令牌。
- **改法**：
  1. 顺序改为：波次 → IP 桶（耗尽回 `deny IP_RATE_LIMIT`）→ zone 桶（空则 `ipBucket.addTokens(1)` 后回 `queue`）→ 冷却。两段桶调用仍在同一个 `try` 内，fail-open 语义不变。`addTokens` 按容量封顶，不会把桶加爆。
  2. 更新类注释的 `<ol>`。
- **代价**（评审补）：zone 桶已空的请求，Redis 往返从 1 次（zone CAS）变成 3 次（IP CAS + zone CAS + IP 退还）。这恰好发生在开服尖峰、共享单主 Redis 最忙的时候（R4）。退还是为了 NAT 后多人共用一个 IP 时不被 zone 排队顺带耗光 IP 桶；如果 L-14 测出 Redis 是瓶颈，可以去掉退还（2 次），代价是 NAT 用户更早吃 429。
- **测试**（`AssignGateRateLimiterTest.java`）：
  - `stubBuckets` 改成按键前缀分发：`argThat(k -> new String(k, UTF_8).startsWith("rl:zone:"))` 返回 zone 桶，`rl:ip:` 返回 IP 桶。
  - **评审补：有三个既有用例不走 `stubBuckets`、按调用顺序内联打桩，必须一起改，否则顺序一换就红**：
    - `zoneBucketExhausted_queues`（`:151-167`）：现在对所有 `build` 都返回 zone 拒绝桶，换序后它被当成 IP 桶，`tryConsume` 未打桩回 false → 变成 IP_RATE_LIMIT。改为按前缀分发（IP 通过、zone 拒绝），类注释「IP bucket is never reached」改为「IP 令牌被退还」，并顺带断言 `verify(ip).addTokens(1)`；
    - `accountCooldownHit_secondCallDenied`（`:180-200`）：`thenReturn(z1, i1, z2, i2)` 依赖 zone 先、IP 后，换序后 z1 被当成 IP 桶，同样回 false。改为按前缀分发（两次调用各自命中同一前缀即可）；
    - `zeroZoneSkipsZoneBucket`（`:212-226`）：只建一个桶，顺序无关，保持不变；`verify(builder, times(1))` 仍成立。
  - 新增两个用例：
    - `ipExhausted_doesNotConsumeZoneToken`：IP 桶 `tryConsume` 返回 false 时，`verify(zoneBucket, never()).tryConsumeAndReturnRemaining(anyLong())`；
    - `zoneExhausted_refundsIpToken`：zone 桶未消费时，`verify(ipBucket).addTokens(1)`，结果为 QUEUE。

#### J2（建议）：账号冷却键带客户端 IP

- **问题**：冷却键是 `cooldownScope + ":" + account`（`AssignGateRateLimiter.java:106`），而 `account` 在这一步未经认证。限流一开，任何人每 4 秒替受害者账号调一次 `/api/login`，受害者在同一 gateway 副本上就一直拿 429 `ACCOUNT_COOLDOWN`（2 副本各自一份 Map，攻击者两边都打即可）。这是打开限流才出现的定点封号面。
- **改法**：键改为 `cooldownScope + ":" + account + "@" + ip`（`check` 已有 `clientIp` 参数）。攻击者换 IP 就只能挡住自己。代价：同一账号换网络后不再共享冷却窗口，防重试风暴的作用对单个客户端不变。
- **测试**：`accountCooldown_isPerAccountAndIp`：同账号、不同 IP 第二次仍 PASS；同账号同 IP 第二次 DENY。

- 由 Codex 在 `java/gateway_node` 跑：`./mvnw.cmd -q test "-Dtest=AssignGateRateLimiterTest,AssignGateControllerTest"`（Windows；Linux 用 `./mvnw`）。不需要外部依赖，纯 Mockito。

---

## 5. 错误语义

**部署期**（都在任何 kubectl 写操作之前；DryRun 同样生效）：

| 情形 | 行为 | 文本（测试锚点） |
|---|---|---|
| 参数值不在 `auto/on/off` 里 | PowerShell 参数绑定失败 | ValidateSet 原生报错 |
| staging/prod、排队开、部署 login，缺 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET` | throw | `ReleaseProfile=prod 要求从环境变量 MMORPG_LOGIN_QUEUE_TOKEN_SECRET`（`release_common.ps1:679-708` 的既有句式） |
| 同上，值是占位串或长度 <32 | throw | `环境变量 MMORPG_LOGIN_QUEUE_TOKEN_SECRET(...)的值仍是占位串` / `长度 N < 要求的 32` |
| 与 GATE 或 INTERNAL_AUTH 相同（任何档位，只要非空） | throw，不回显值 | `MMORPG_LOGIN_QUEUE_TOKEN_SECRET 必须与 MMORPG_GATE_TOKEN_SECRET 不同` / `… MMORPG_INTERNAL_AUTH_SECRET 不同` |
| 阈值常量越界（rps < 1、burst < rps、冷却为负、排队容量不在 1..uint32）、设置键或值类型非法 | throw `内部错误:…` | 只有改坏常量表时才会出现；校验在 `Get-GatewayRateLimitSettings`（§4.3，评审补）与 `New-LoginQueueYaml`（§4.5） |
| 非 dev 档 `off` / 限流开但没有可信代理 | Write-Warning，不阻断 | `[削峰]GatewayRateLimit=off` / `[削峰]LoginQueue=off` / `[削峰]gateway限流已开但未配置可信代理` |

**运行期**（行为不新增，只是从「不可达」变为「生效」）：

- **gateway**：
  - zone 桶空 → HTTP 200，`code=100`，`queue_source="ratelimit"`，带 `retry_after_ms`；
  - IP 桶耗尽或冷却 → `code=429`，`error=IP_RATE_LIMIT|ACCOUNT_COOLDOWN`；
  - Redis 异常 → 放行，并按请求逐条 WARN `rate-limit fail-open`（fail-open 是设计稿 §七 的既定取舍）；
  - `/api/queue-status` 轮询不限流；带 `queue_token` 重调 `/api/assign-gate` 在 G4b-J0 之后照样过 zone 桶与 IP 桶，只跳过冷却（**行为变化**，评审补）。
- **login**：
  - 满员 → `QUEUEING(1)` + `queue_token`；
  - 没有 gate → `ERROR(2)`，不入队；
  - 令牌验签失败（排队开着时）或 `/queue-status` 时排队已关 → `EXPIRED(3)`，gateway 映射为 410，客户端重新调 assign-gate。注意排队关着时带 token 重调 assign-gate 不会得到 EXPIRED，而是直接走快速通道（`assigngatelogic.go:62-66`、`:90-93`），这正是 J0 要堵的口子。
- **login 启动期**：生产模式下开了排队却没有合格的 QueueToken → 拒绝启动（CrashLoopBackOff）。部署侧的长度、占位、复用检查拦掉大部分情况，剩余缺口见 §9 R10。
- **gateway 启动期**：限流开时，`RateLimitConfig` 在创建 Bean 时同步 `connect()` Redis，Redis 不可达则启动失败（§9 R5）。

---

## 6. 测试清单（全部不需要外部依赖：只用 pwsh 7 起子进程跑 DryRun 或调用纯函数，不碰集群；G4b-J 用 Mockito）

`k8s_deploy_contract.tests.ps1`：

- 在 `$ProdEnv`（`:904-919`）里加一项：`MMORPG_LOGIN_QUEUE_TOKEN_SECRET = 'contract-test-queue-token-0123456789abcdef'`。要求 ≥32，与其它各值都不同，不以 Go 占位前缀开头（`secrets.go:46-56`）。**不加这一项，prod 基线 `:928` 会变红**。
- 新增一节「削峰（G4b）」，放在 `:984`（login Mode 用例）之后、`:986` 第 4 节负向用例之前，复用 `$devOut` 与 `$prodRun`，另外起 8 个子进程（用例 15、16 不起子进程）：
  - `$peakStagingRun`：`$BaseArgs + -ReleaseProfile staging -SkipPreflight -NodeImage ghcr.io/luyuancpp/mmorpg-node:0123456789ab`，env 用 `$ProdEnv`；
  - `$peakProdOffRun`：`$ProdArgs + -GatewayRateLimit off -LoginQueue off`，env 为 `$ProdEnv` 的拷贝，并把 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET=''`。拷贝方式用 `@{} + $ProdEnv` 后再按键赋值，不要用 `+` 合并同名键，那会抛异常；
  - `$peakDevOnRun`：`$BaseArgs + -GatewayRateLimit on -LoginQueue on`，env 为 `@{ MMORPG_LOGIN_QUEUE_TOKEN_SECRET=''; MMORPG_REDIS_PASSWORD='' }`；
  - `$peakAllUpRun`：`@('-Command','all-up','-DryRun','-SkipInfra','-SkipJavaSvc','-ZonesConfigPath',(Join-Path (Get-RepoRoot) 'deploy/k8s/zones.sample.json'),'-GoSvcRegistry','registry.invalid/test','-ReleaseProfile','prod','-SkipPreflight','-NodeImage','ghcr.io/luyuancpp/mmorpg-node:0123456789ab')`，env 用 `$ProdEnv`。**评审更正**：原稿漏了 `-DryRun`（不带它子进程会对本机当前 kubectl context 真执行 apply），zones 路径也必须用 `Join-Path (Get-RepoRoot)` 给绝对路径（同 `:1252` 既有写法），子进程工作目录不保证是仓库根；
  - `$peakGwRedisRotRun`（评审补，用例 17）：`$ProdArgs + -SkipGoSvc`，env 为 `$ProdEnv` 的拷贝并把 `MMORPG_REDIS_PASSWORD` 换成另一个 ≥12 位的值；
  - 3 个负向运行，见用例 10、11。

| # | 用例名 | 断言要点 |
|---|---|---|
| 1 | `削峰 dev 档缺省关:gateway 不写 gate.rate-limit.enabled,login 不写 Queue / Secrets.QueueToken,两者无 peak-shaving 注解(产物与 G4b 前相同)` | `$devOut`：四项都不存在；输出含 `gateway_rate_limit=off` 和 `login_queue=off` |
| 2 | `削峰 staging / prod 档开:gateway 阈值 == 开服档起步值,rate-limit: 只有一个` | 两档的 flat 值依次为 `enabled=true`、2000、5000、5、20、5000；在 `Select-ManifestByName 'java-svc-gateway-config'` 取出的块上（不是整段输出），`(?m)^\s+rate-limit:\s*$` 恰好匹配 1 次；另断 `zone-default-burst ≥ zone-default-rps`、`ip-burst ≥ ip-rps`（与 `release_preflight.ps1` F 节同一规则，评审补） |
| 3 | `削峰 staging / prod 档开:login Queue 段除三项外 == go/login/etc/login.yaml,锁键带本 zone,ZoneCapacityOverride 只有本 zone` | `Queue.Enabled=true`；6 个镜像键逐个等于 `Get-EtcValue`；`DispatcherLockKey == "<基名>:zone:101"`；`ZoneCapacityOverride.101 == 5000`，且这个前缀下只有一个键；在 login ConfigMap 块上 `(?m)^ {4}Queue:[ ]*\r?$` 与 `(?m)^ {4}Secrets:[ ]*\r?$` 各恰好 1 次。**评审更正**：原稿写 `^Queue:` / `^Secrets:`，但 ConfigMap 把 login.yaml 整体缩进 4 格嵌在 `login.yaml: \|` 下（`k8s_deploy.ps1:4134`），顶格锚点永远匹配 0 次，断言必红 |
| 4 | `削峰写出的键名与服务自己的配置同名(拼错会被静默忽略)` | 用例 2 写出的每个键，在 `java/gateway_node/src/main/resources/application.yaml` 的 `gate.rate-limit.<键>` 下都能用 `Get-EtcValue` 读到；`Queue.*` 的 7 个键在 login.yaml 里都能读到；`config.go` 含 `json:"ZoneCapacityOverride,optional"` |
| 5 | `「不写即关」的前提:jar 内 gate.rate-limit.enabled 与 login.yaml Queue.Enabled 仍为 false,两处 Go/Java 缺省也是 false` | `Get-EtcValue` 两处读到 `false`；`RateLimitProperties.java` 含 `private boolean enabled = false;`；`config.go` 含 `Enabled             bool          \`json:"Enabled,default=false"\``，正则按 `QueueConf` 段定位 |
| 6 | `staging / prod 的 gateway 带 spring.data.redis.password 且 == MMORPG_REDIS_PASSWORD;dev 无 Redis 口令时不写` | 两档的值相等；`$peakDevOnRun` 不含该键 |
| 7 | `login Secrets.QueueToken.Value == MMORPG_LOGIN_QUEUE_TOKEN_SECRET,InternalAuth 不受影响` | prod / staging 两档都检查 |
| 8 | `peak-shaving 指纹:gateway 在口令指纹之后多一条、login 一条;staging 与 prod 同输入同指纹;off 时消失` | gateway 在 `Select-ManifestByName 'gateway'` 取出的块上（该函数已把换行统一成 LF；整段原始输出在 Windows 上是 CRLF，`\n` 锚点会失配，评审补）用正则：`annotations:[ ]*\n[ ]+mmorpg\.io/gateway-admin-api-key-hash: "[0-9a-f]{12}"[ ]*\n[ ]+mmorpg\.io/peak-shaving-hash: "(?<h>[0-9a-f]{12})"`；login 用 `Select-ManifestByName 'login'` 后匹配同一个键；`$peakProdOffRun` 与 `$devOut` 都不含 `peak-shaving-hash`；`$peakDevOnRun` 的 login 指纹 ≠ prod（密钥不同） |
| 9 | `-GatewayRateLimit off -LoginQueue off(prod):两段不写、不要求排队密钥、打出关闭警告` | 退出码 0；不含 `gate.rate-limit.enabled` 与 `Queue.Enabled`；匹配 `\[削峰\]GatewayRateLimit=off` 与 `\[削峰\]LoginQueue=off` |
| 10 | `负向:prod 缺 MMORPG_LOGIN_QUEUE_TOKEN_SECRET 在任何写操作之前拒绝并点名变量` | 退出码 ≠0；匹配 `ReleaseProfile=prod 要求从环境变量 MMORPG_LOGIN_QUEUE_TOKEN_SECRET`；`Assert-NotMatch '\[dry-run\] kubectl [^\r\n]*\b(apply\|create\|delete\|patch\|replace)\b'` |
| 11 | `负向:排队密钥与 GATE / INTERNAL_AUTH 相同时拒绝,只点变量名不回显值` | 两次运行；分别匹配 `必须与 MMORPG_GATE_TOKEN_SECRET 不同`、`必须与 MMORPG_INTERNAL_AUTH_SECRET 不同`；`Assert-NotMatch` 被复用的那个值（用 `[regex]::Escape` 转义） |
| 12 | `dev -GatewayRateLimit on -LoginQueue on:两段写出,不写 Secrets.QueueToken,不要求注入,Mode 仍为 dev` | 退出码 0；`Queue.Enabled=true`；不含 `Secrets.QueueToken`；`Mode == login.yaml 的 Mode` |
| 13 | `all-up 多 zone(prod):每个 zone 的 login 锁键与 ZoneCapacityOverride 只指向本 zone,两把锁键互不相同` | 退出码 0；用 `Get-ManifestBlocks` 过滤出 `name: go-svc-login-config`，共 2 块；每块的锁键 == `"<基名>:zone:" + 该块 Node.ZoneId`（101 / 102）；每块的 `ZoneCapacityOverride.*` 只有本 zone 一个键 |
| 14 | `staging 与 prod 的削峰产物逐字相同(档位只决定开关,不决定阈值)` | 两次运行抽出的 `rate-limit:` 段与 `Queue:` 段文本相等 |
| 15（评审补，G4b-2） | `G4b-2 前置:G1b 与 G4b-G 已合入(DispatcherLockKey 是前缀,锁键按 zone 派生;在途占位是新键 ZSET)` | `go/login/internal/logic/pkg/loginqueue/dispatcher.go` 含 `func ZoneDispatcherLockKey(` 与 `"%s:zone:%d"`；`go/login/internal/svc/servicecontext.go` 不再含 `activeZones := func`；`go/login/internal/logic/pkg/loginqueue/queue.go` 含 `admitted:zset:zone:%d`，且不含 `"admitted:zone:%d"`。把「G1b → G4b-2」「G4b-G → G4b-2」两条合入顺序变成机械门禁：G1b 或 G4b-G 没合，这条就红（G4b-G 一半是 2026-10-15 按 `spec_G4b-G.md` §5.5 / §7.2 第 4 项补的） |
| 16（评审补，G4b-1） | `常量表自检:改坏 PeakShavingLaunchTier 时在任何写操作之前 throw` | 用 AST 从 `k8s_deploy.ps1` 抽出 `Get-GatewayRateLimitSettings`（`ParseFile` + `FunctionDefinitionAst` + `Set-Item Function:script:`，写法同 `k8s_migrate_gate.tests.ps1:15-29`），在测试脚本作用域里设 `$script:GatewayRateLimitOn = $true` 与一份 `ZoneDefaultRps=2000; ZoneDefaultBurst=1000`（其余合法）的常量表，断言 throw 文本匹配 `不满足 rps ≥ 1 且 burst ≥ rps`；对照：合法表不 throw、返回 6 个键 |
| 17（评审补，G4b-1） | `gateway 削峰指纹随 Redis 口令变化(轮换口令自动滚动 gateway)` | `$peakGwRedisRotRun` 与 `$prodRun` 的 gateway `peak-shaving-hash` 不同、`gateway-admin-api-key-hash` 相同；`$peakGwRedisRotRun` 的 `data.application.yaml.spring.data.redis.password` == 换过的新口令 |

`k8s_client_entry_contract.tests.ps1`：

| # | 用例名 | 断言要点 |
|---|---|---|
| L1 | `库:gate.rate-limit 片段 -Settings 在前、trusted-proxies 在后,共用一个 rate-limit: 键;只有阈值也写;值只收 bool / 整数,键必须 kebab-case` | ① settings + proxies 的完整期望文本；② 只有 settings；③ `$null` + `''` 返回 `''`；④ 值为 `'2000'`（字符串）时 throw，匹配 `只收 bool / 整数`；⑤ 键为 `zoneDefaultRps` 时 throw，匹配 `kebab-case` |

回归（不改，但必须仍然全绿）：`k8s_migrate_gate.tests.ps1`（AST 抽取的 `Apply-OneGoSvc`）、`k8s_gate_drain.tests.ps1`、`k8s_zone_rollback_gate_router_mode.tests.ps1`、`k8s_zone_rollback_flush_guard.tests.ps1`。

G4b-J：J0 的 `AssignGateControllerTest` 三个用例（必做）；J1 的两个新用例加上既有三个用例的改写（§4.10 列出）；J2 的一个用例。

回归里的 `k8s_gate_drain.tests.ps1:363-376` 会用正则抽出 login 模板正文、把 `${…}` 一律替换成 `1` 再解析（评审核对）：`Value: "${internalAuthSecret}"${loginQueueTokenSecretBlock}` 会变成 `Value: "1"1`，该用例只读 `Node.*` / `Registry.*` / `GateDrain.*`，不受影响；`RequireClientEndpoint` 整行被它另行替换，追加的 `${loginQueueBlock}` 一并消失，也不受影响。

---

## 7. 交给 Codex 的验证命令与通过标准

工作目录：`D:\luyuan\xuanming-server-mmo-cpp`，使用 PowerShell 7（`pwsh`）。

**前置清理**：清掉跑测机器上残留的 `MMORPG_*` 环境变量，避免 dev 基线被本机变量污染：

```powershell
Get-ChildItem Env:MMORPG_* | Remove-Item
```

**按顺序串行执行**（每条都是独立子进程，互不并发）：

```powershell
pwsh -NoProfile -File tools/scripts/tests/k8s_client_entry_contract.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/k8s_migrate_gate.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/k8s_gate_drain.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1
# G4b-J(J0 必做;J1 / J2 做了才有对应用例)。Push-Location / Pop-Location 保证失败时也回到仓库根:
Push-Location java/gateway_node
try { ./mvnw.cmd -q test "-Dtest=AssignGateRateLimiterTest,AssignGateControllerTest" } finally { Pop-Location }
```

**通过标准**：
- 每条命令退出码为 0，`Complete-TestRun` 汇总里失败数为 0；mvn 输出无 `FAIL`、`Tests run:` 汇总里 Failures / Errors 为 0；
- §6 中新用例名（G4b-1：1、2、4、5、6、8/9 的 gateway 部分、14、16、17、L1；G4b-2：3、7、8/9 的 login 部分、10–13、15）出现在 PASS 列表；
- `k8s_deploy_contract` 的 `:928` prod 基线，以及 `:1460-1555` 管理面口令一节，仍然全部通过。

**失败时保留**：
- 失败用例的完整输出；
- 对应 DryRun 子进程的整段输出，含 `--- BEGIN MANIFEST ---` 块；
- `git diff --stat`。

**合入门槛**：
- G4b-1：G4b-J0 同批或先合，`AssignGateControllerTest` 由 Codex 跑绿。
- G4b-2：同一棵树里，G1b 规格指定的多 zone 用例（L-01 验收标准：每个 zone 的队列都能出队；G1b T3 / T8a）已经由 Codex 跑绿，契约用例 15 绿；并且 §1.5 的 Q1 / Q2 要么由 G4b-G 修掉并跑绿，要么负责人在 §10 #11 书面接受。三者缺一，G4b-2 不合。
  - 2026-10-13 已选方案 A：「或负责人书面接受」这一支作废，只剩「G4b-G 由 Codex 跑绿」这一条。
  - 打开排队之前，G4b-G 的新脚本至少在真实 Redis 上跑过一次（`spec_G4b-G.md` §8.4 的最小冒烟）；是否作为硬门槛见该规格 §11 #5。
  - **（2026-10-15 负责人按「最标准」定为硬门槛，`spec_G4b-G.md` §11 #5）真实 Redis 首跑**：`spec_G4b-G.md` §8.4 的最小冒烟（含 10-15 补的两项：整键 `PTTL` 在 (0, 120000] 内、手工调大的整键 TTL 不被后续写入压回）由 Codex 或负责人在真实 Redis 7.x 上跑通，命令原文与输出摘要记进 `docs/PROGRESS.md`。没有这份记录，G4b-2 不合，任何环境都不打开排队（含 `-LoginQueue on` 与手改 `Queue.Enabled`）；miniredis 上的单测结果不能顶替。与上面三条并列，缺一不合。

**真实环境（可选，不作为合入门槛，需要外部依赖：kind 集群、gateway / login / node 三套镜像，以及上面那组 MMORPG_* 环境变量）**：

在 kind 上以 `-ReleaseProfile staging` 跑 `zone-up` 两个 zone，然后核对：
1. gateway 日志出现 `Bucket4j Redis client ->`，且在 20 次请求内没有出现 `rate-limit fail-open`；
2. 从一个 Pod 1 秒内 `curl -XPOST /api/assign-gate` 30 次，第 21 次起出现 `code:429` / `IP_RATE_LIMIT`；
3. 两个 zone 的 login 日志分别出现 `elected dispatcher leader zone=101 key=dispatcher:lock:login_queue:zone:101:zone:101` 和 `zone=102 key=…:zone:102:zone:102`（评审更正：日志格式与「前缀再拼 zone」以 G1b 规格 §3.1 为准，原稿的期望串在 G1b 之后永远对不上）；两个 zone 各入队若干人后都能出空；
4. 改 `-GatewayRateLimit off` 重跑，gateway 和 login 都自动滚动；
5. （评审补，J0）`-LoginQueue off`、限流开着时，带 `"queue_token":"x"` 的 assign-gate 在第 21 次起同样拿到 `code:429`，不再直接拿到 gate 票。

这些只记作「真实环境证据」，不能替代契约测试。

---

## 8. 与现有设计的偏离与代价

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| 1 | IP 阈值取 5/20，不取设计稿 §5.2 的 2/10（`open-server-rate-limit-design.md:155-156`） | 设计稿没有 IP 的「开服档」。jar 内 `application.yaml:41-42` 已经取 5/20，`release_preflight` 检查的也是它。一次登录要扣 2 个 IP 令牌。`gate-connection-admission-control.md:409-413` 明确说 per-IP 会误伤洪峰 | 防刷力度比设计稿弱 2.5 倍 |
| 2 | staging / prod 缺省就开，不走「T+0 关、灰度单 zone 24h 再开」（`release-checklist.md:214`、`:291`、`:297-305`；设计稿 `:234-236`） | 开服首日 P0（`launch_verdict.txt:19`）；该清单是 2026-05 的历史附录（`release-checklist.md:189-191`） | 没有 canary。用 `off` 秒级止血，配合指纹自动滚动来弥补 |
| 3 | login 整个 Deployment 一起开，不按 `login.yaml:415-420` 的「先开一个 canary Pod」 | K8s 生成器按 Deployment 粒度生成配置 | 同上 |
| 4 | `ZoneCapacityOverride` 所有 zone 统一用 5000（文档示例值），不按「观测峰值 × 0.9」逐 zone 设置 | 没有任何 zone 的观测峰值 | 可能把一个 zone 的在线卡在 5000（§10 #3） |
| 5 | `Secrets.QueueToken` 写明文 `Value`，不按 `secrets.go:86-87` 的建议改用 `Env` | 兄弟项 InternalAuth 也是明文 Value（`k8s_deploy.ps1:3663-3665`），保持一致 | 与 InternalAuth 暴露面相同（ConfigMap 可读即可见）；DryRun 输出里能看到明文，与现有 ConfigMap 一样 |
| 6 | gateway 的 Redis 口令写明文进 ConfigMap，不走管理面口令那套 Secret 模式 | 同一 namespace 的 `go-svc-login-config` 等已经带同一个明文值，改走 Secret 不缩小暴露面 | 与数据源口令同级 |
| 7 | 新增 Pod 指纹注解：开关、阈值、代理网段、排队密钥一变就滚动 | 不这样做，`off` 止血在镜像不变时不生效（ConfigMap 只在启动时读，同理见 `:4694-4696`） | 每次调阈值都会滚动 gateway / login；排队密钥轮换时，在排玩家的令牌失效、重新入队（§9 R11） |
| 8 | 开服波次 `wave` 不打开（设计稿 §四） | 需要开服时刻，属于运营决策 | 开服 0 秒的整体洪峰只靠令牌桶挡 |
| 9 | `login-queue-2026-05.md:194` 写的是「复用 GateTokenSecret」 | 代码早已拆开（`secrets.go:1-8`） | 文档事实更正，不属于行为偏离 |
| 10（评审补） | G4b-J0：带 `queue_token` 的 assign-gate 在排队**开着**时也过 zone / IP 桶；Java 版只在其排队关闭时才限流这类请求（`PARITY.md`「开服限流」行有意差异 ④） | 本仓库的 gateway 与排队分属两个进程，gateway 不知道 login 排队开没开、也验不了 token；只能二选一，选 fail-closed | 网络抖动后带 token 重调 assign-gate 的玩家偶尔多扣一个 zone / IP 令牌，可能先拿到一次 `ratelimit` 排队；正常轮询走 `/api/queue-status`，不受影响 |

---

## 9. 风险

| # | 风险 | 依据 | 缓解 |
|---|---|---|---|
| R1 | **2000/5000 保护不了 EnterGame**。zone 桶每秒放约 1000–2000 人（看客户端 login 时带不带 zone_id），而实测 EnterGame 稳态只有 50–100/s，250/s 的浪涌就触发 87k 次 preload timeout；R18 平均每秒约 64 次进场。新号首次进场必须冷读库、同步等 8s | `login.yaml:457-460`；`tidb-phase1-…md:816`、`:846` | §10 #1 拍板；L-14 定终值；在压测有结论前可考虑先用设计稿的「维护后开服」档 200/500 |
| R2 | IP 桶误伤：网吧、CGNAT；集群内 robot 共用 Pod IP，staging 压测会被 IP 桶扭曲（robot 把 429 当失败，`robot/http_assign_gate.go:146-147`） | `AssignGateRateLimiter.java:94-98` | L-14 先用 `-GatewayRateLimit off` 跑基线；要开着限流跑，robot 需要伪造 XFF，同时 trusted-proxies 要覆盖 robot 的 Pod 网段（后续项，§10 #5） |
| R3 | 调阈值不生效，除非清掉 `rl:zone:*`（推断，见 §4.9） | `AssignGateRateLimiter.java:84-85`；`RateLimitConfig.java:71`、`:89-92` | 运维手册写清；后续给 Java 加 `withImplicitConfigurationReplacement`（§10 #8） |
| R4 | 热键 `rl:zone:<id>` 在共享单主 Redis 上被 CAS 争用（推断，未实测）；J1 之后 zone 桶已空的请求每次 3 个 Redis 往返（§4.10 J1「代价」） | `RateLimitConfig.java:5`、`:89-92`（`lettuce.cas`）；`launch_verdict.txt:22` | L-14 测 Redis ops 和 gateway 的 p99；L-06 拆出限流专用 Redis；Redis 先饱和就去掉 J1 的退还 |
| R5 | 限流开启后，gateway 启动依赖 Redis 可达（Bean 创建时 `connect()`），与 `gateway.yaml:63-68`「Redis 不该挡住网关启动」的设计相悖 | `RateLimitConfig.java:60-64` | Redis 故障期间 login 本来就不可用，影响有限；改成懒连接留作后续 |
| R6 | 可观测性缺口：gateway 没有指标（全仓没有 Micrometer）；K8s 的 login 没有 Prometheus 段，抓不到 `login_queue_*`；fail-open 时每个请求一条 WARN，会刷屏 | `k8s_deploy.ps1:3558-3690`；`AssignGateRateLimiter.java:100` | 后续项（§10 #8）；开服当天靠日志和客户端回包码判断 |
| R7 | 参数不粘滞：`off` 止血后，任何一次重新部署（含回档第 6 步）都会恢复为开 | 与 `-GateRouterMode` 同一口径 | 文档写清；§10 #9 讨论是否给回档脚本透传 |
| R8 | 与 G1b 的方案耦合。G1b 规格已定「配置值是前缀」，显式的分 zone 键在新镜像上变成 `…:zone:N:zone:N`（每 zone 唯一，正确，只是排查时键名要按它查）；它的价值在 `rollout undo` 回到 G1b 之前的镜像时仍按 zone 隔离。若 G1b 最终改成「一个 leader 覆盖全部 zone」，显式键会产生 N 个 leader，超发准入 | §1.4；§4.5；`launch_spec_G1b.md` §3.1、§4.5 | 合入顺序 G1b → G4b-2，由契约用例 15 机械守住；README 写明 K8s 上的实际键名 |
| R9 | 开服首拍：容量 5000、在线 0 时，dispatcher 和快速通道最多一次放 5000 人；实际速率受上游 zone 桶限制（burst 5000，之后 2000/s）。叠加 §1.5 Q2，dispatcher 在 gate 人数刷新前的几拍里可放进容量的数倍 | `capacity.go:95-100`；`assigngatelogic.go:110-129` | 与 R1 同一处拍板；Q2 见 R15 |
| R10 | Go 侧占位判定更严：只要前缀是 `secret`、`example`、`test-secret` 等就拒绝（`secrets.go:46-56`），而部署侧只做精确匹配（`release_common.ps1:648-660`）。某个值能过部署检查，login 却会 CrashLoop | 既有缺口，InternalAuth 同样存在 | README 提示用随机串；不在本批复制一份 Go 的前缀表（DRY） |
| R11 | 排队密钥轮换会自动滚动 login，在排玩家拿到 EXPIRED 后重新入队，排位丢失 | `secrets.go:12-14`（三段式轮换需要 Additional） | 只在维护窗口轮换；三段式轮换留作后续 |
| R12 | 口令里含 `"` 或 `\` 会破坏生成的 YAML；gateway 的口令还会经 Spring 占位符解析，含 `${` 时 Spring 会把它当嵌套占位符，解析不了就拒启（评审补，推断，与既有数据源口令同一问题） | 既有问题，所有模板都一样 | 运维用只含字母数字与 `-_` 的随机串 |
| R13 | 先扣 zone 再判 IP：单个 IP 就能把一个 zone 的入口压成排队 | `AssignGateRateLimiter.java:83-98` | G4b-J1（§4.10） |
| R14 | `rl:ip:*` 在 Redis 上的内存：每个 IP 一个键，TTL 约 1h + 补满时间；百万级 IP 估约 100MB，而 Redis 没设 maxmemory（推断）。评审补：① `rl:zone:<id>` 的 id 由请求决定，限流先于 zone 校验，按现行顺序不受 IP 限速，攻击者可造任意多个 zone 桶键；② IPv6 客户端在自己的 /64 内换源地址即可得到新 IP 桶（Java 版按 /64 分桶，`PARITY.md`「开服限流」行 ⑨）；③ 排队打开后 `queue:zone:*`、`queue:meta:*`、`queue:token:*` 按在排人数线性占内存 | `RateLimitConfig.java:71`、`:89-92`；`redis.yaml:137-143`；`AssignGateController.java:59` | L-14 测内存曲线；L-06；①由 J1 改为先过 IP 桶后受限；②③登记为后续项 |
| R15（评审补） | §1.5 Q1：排队打开后快速通道占位只靠整集合 TTL 回收，持续有人登录时余量被重复扣减到 0，zone「放一批、停最长 60s」循环；Q2：consume 即撤占位而 gate 人数滞后，开区几拍内超发数倍容量 | §1.5；G1b 规格 §4.7 第 2 条（明确留给 L-02） | G4b-G（§10 #11）或负责人书面接受；L-14 观察 `admitted:zset:zone:*` 的 ZCARD（清过期后的在途占位数，即指标 `login_queue_admitted_inflight`；G4b-G 起换了新键，旧通配 `admitted:zone:*` 匹配不到它）与出队速率曲线 |
| R16（评审补） | 排队轮询压在共享单主 Redis：`/api/queue-status` 不限流，每次 `Lookup` 4–5 次往返，约 2.5·Q ops/s；`DefaultRetryAfterMs` 镜像 login.yaml 的 2000。开服若全服排 10 万人，约 25 万 ops/s 落在同一个 Redis 主上，与限流桶、会话、锁、PlayerAllData 争用（推断，未实测） | `queue.go:220-290`；`QueueStatusController.java:22-26`；`launch_verdict.txt:22` | L-14 带排队压测时必须测 Redis 主的 ops / CPU；§10 #12 决定开服档轮询间隔；L-06 把排队与限流拆到独立 Redis |
| R17（评审补） | J0 之前：限流开、排队关（G4b-1 单独上线或 `-LoginQueue off` 止血）时，带任意 `queue_token` 的 assign-gate 绕过整个限流并拿到 gate 票 | `AssignGateController.java:51-57`；`assigngatelogic.go:62-66`、`:90-93` | G4b-J0 作为 G4b-1 前置（§4.10） |
| R18（评审补） | 账号冷却按未认证的 account 计：限流一开，任何人都能每 4 秒替他人账号打一次 `/api/login`，让对方在同一副本上持续 429 | `AssignGateRateLimiter.java:104-118`；`LoginController.java:50-54` | G4b-J2（冷却键带 IP）；不做则登记为已知风险 |
| R19（评审补） | gateway 只在启动时读 Redis 口令：轮换 `MMORPG_REDIS_PASSWORD` 而 gateway 不滚动，旧口令被拒后限流一律 fail-open | `RateLimitConfig.java:46-58` | 已在 §4.7 把口令纳入 gateway 削峰指纹（用例 17）；go-svc 同类缺口不在本批 |
| R20（评审补） | 对已在运行的 staging / prod zone 首次开排队：若当前在线已 ≥ `LoginQueueZoneCapacity`（5000），新登录全部入队，直到在线降到 5000 以下；dispatcher 不会因此报错 | `capacity.go:76-100` | 首次打开前按 README 核对各 zone 在线；超过的先调大常量（§10 #3）或先只开 gateway 限流 |

---

## 10. 待负责人拍板

1. **zone 阈值**：按设计稿开服档 2000/5000 开（本规格的缺省），还是在 L-14 出结果前先用 200/500（设计稿「维护后开服」档，见 R1）？
2. **G4b-J**：J0 已列为 G4b-1 的前置（不拍板就不能合 G4b-1，见 R17）；要拍板的是 J0 的取舍（排队开着时带 token 的重入也限流，§8 #10）以及 J1（约 15 行 + 2 个新用例 + 改写 2 个既有用例；否则 R13 随开关上线）、J2（约 1 行 + 1 个用例；否则 R18 随开关上线）是否同批。
3. **`ZoneCapacityOverride = 5000`/zone**：它是在线上限。是否接受？还是先写一个更大的值（例如 `GateMaxConnections 20000`（`bin/etc/base_deploy_config.yaml:126`）× gate 副本数 × 0.9），只防满载不防速率？已在运行、在线已超过该值的 zone 首次开排队会让新登录全部入队（R20）。
4. **G1b 方案**：G1b 规格已定「前缀 + `:zone:<id>`」（§1.4）。确认 G4b-2 保留显式分 zone 键（价值在 rollout undo 到 G1b 之前的镜像，代价是新镜像上键名变成 `…:zone:N:zone:N`）；或者请 G1b 让 `ZoneDispatcherLockKey` 在前缀已以 `:zone:<id>` 结尾时不再重复拼接，键名就两边一致。
5. **L-14 的压测口径**：开着限流压测需要 robot 伪造 XFF，并且 trusted-proxies 覆盖 robot 的 Pod 网段（robot 要改代码，属于后续项）；还是先用 `-GatewayRateLimit off` 跑基线？
6. **是否给阈值加覆盖参数**（如 `-GatewayZoneRps` / `-GatewayZoneBurst` / `-LoginQueueZoneCapacity`）？本规格坚持 YAGNI，只有常量表；开服当天调值只能改常量重发，或者 `kubectl edit` + 滚动 + 清 `rl:zone:*`。
7. **开服日是否启用波次**（`wave.start-epoch-sec` 与 schedule）？
8. **后续项排期**：K8s 的 login 暴露 `/metrics`；gateway 接 Micrometer；Bucket4j 配置版本化替换；限流 Redis 独立（L-06）。
9. **回档脚本与 `dev_tools.ps1` / `k8s_image.ps1`** 是否透传 `-GatewayRateLimit` / `-LoginQueue`（「留空不透传、给了才透传」，同 `-GateRouterMode`；要补就同步 `k8s_client_entry_contract.tests.ps1` 的 `$WrapperPairs`）？目前不透传：回档后按档位恢复为开，事故止血只能直接调 `k8s_deploy.ps1`。
10. **开服日过后**，prod 是否降回「普通运营」档 500/1000？
11. **（评审补）§1.5 的两条排队基线缺陷怎么处理**：
    - 方案 A（建议）：另立 **G4b-G**（Go，`go/login/internal/logic/pkg/loginqueue`），照 Java 版有意差异 ②③ 修：在途占位改为新键 `admitted:zset:zone:{z}` 的 ZSET（分数 = 到期毫秒），`AdmittedCount` 先 `ZREMRANGEBYSCORE -inf now` 再 `ZCARD`（同一段 Lua），快速通道 Lua 与 dispatcher 放行都改 ZADD；consume 不 ZREM，而是把分数改成「现在 + 15s」（覆盖 gate 每几秒发布一次人数的滞后）。用新键名而不是原地改类型，避免新旧版本混跑时 `WRONGTYPE`；K8s 上排队从没开过，没有存量状态要迁移。单测用 miniredis 覆盖：持续快速通道下余量不归零、consume 后 15s 内不超发、旧键不再被写。合入顺序 G1b → G4b-G → G4b-2。**2026-10-13 已拍板（方案 A）**，规格见 `spec_G4b-G.md`。
    - 方案 B：负责人书面接受 R15，开服当天靠 L-14 的曲线与 `-LoginQueue off` 兜底。
12. **（评审补）开服档的排队轮询间隔**：`DefaultRetryAfterMs` 现镜像 login.yaml 的 2000（每个在排玩家每秒 0.5 次 `Lookup`，R16）。是否在 `PeakShavingLaunchTier` 里加一项、开服档写 5000 之类的值（排位更新变慢，Redis 压力降到 2/5）？本规格默认不加（YAGNI），等 L-14 的 Redis 曲线再定。

---

## 11. Java 版状态（AGENTS §12）

- **功能已对齐，行为有意不同**：
  - Java 的 game-gateway 自带限流和排队，都缺省关闭，开关为 `GAME_GATEWAY_RATE_LIMIT_ENABLED` / `GAME_GATEWAY_QUEUE_ENABLED`（Java 仓库 `game-gateway/src/main/resources/application.yaml:76-84`、`:93-103`；`PARITY.md`「登录排队」「开服限流」两行）。
  - 判定顺序已经是先 IP、后 zone，zone 桶空时退回 IP 令牌，等于已经做了 G4b-J（`PARITY.md`「开服限流」行）。
  - 排队由一个 leader 遍历区服目录里的全部 zone（`QueueDispatcher.java:19-26`、`:95`），容量取 `zone_config.capacity`。Java 版**没有 G1b 的缺陷**，也不需要 `ZoneCapacityOverride`。
- **「按档位打开」这件事 Java 版待做**：
  - Java 仓库没有 K8s 部署脚本，`deploy/` 下只有 compose 和 docker。
  - 规划文档已经写定 prod 两个开关都为 true（`docs/porting/ops-release-spec.md:1188`），关闭限流时预检报 WARN（`:1022`）。
  - 等它的部署工具落地时，按同一口径实现：staging / prod 两开关为 true，zone 2000/5000，IP 5/20，冷却 5000。
  - 原因：部署工具还没做。
- 客户端契约（帧、proto、tip 码）不变，两版都不需要同批改协议。G4b-J0 只改变「带 token 的 assign-gate 何时可能拿到 100 / 429」，回包码集合不变。
- **G4b-J / G4b-G 与 Java 已有实现的对齐**（评审补）：J0 对应 Java 有意差异 ④（但本仓库在排队开着时也限流，Java 不限，见 §8 #10，仍是有意差异）；J1 对应 ⑧（做完即对齐）；J2 对应 ②（Java 冷却键 = 身份哈希 + IP，且放在 Redis；本仓库仍是进程内，部分对齐）；G4b-G 对应 `PARITY.md`「登录排队」行的 ②③（做完即对齐）。（2026-10-15 更正：③ 是复合条目，G4b-G 只对齐其中「取走后占位再留 15s」一句；② 也只对齐「逐个到期」本身，快速通道占位寿命与整键 TTL 仍不同。见 `spec_G4b-G.md` §12，`PARITY.md` 已按那里改写。J1 按负责人 2026-10-13 拍板不做，⑧ 仍是差异。）（2026-10-15 再更正，负责人「按最标准的去做」之后：mmorpg 交票后与快速通道的占位都改为保留 20s（gate 每 10s 上报，留两个上报周期），与 Java 的 15s 不再相同，③ 这一句改为差异；整键 TTL 两版都只延长不缩短，② 里这条已对齐；「一轮按段挑 gate」mmorpg 已改为每拍取一次候选、批内按本地计数轮转，③ 里这一句已对齐。以 `spec_G4b-G.md` §11 / §12 与 `PARITY.md`「登录排队」行为准。）
- **登记**：由 Docs 阶段在 `PARITY.md` 加一行「削峰按发布档位开启（G4b）」：本仓库 commit ↔ Java「待做（部署工具未落地）」；J0 / J1 / J2 / G4b-G 落地时，同步改 `PARITY.md`「登录排队」「开服限流」两行的「有意差异」措辞（基线已修的条目不再写成「基线是……」），否则对账本会继续把已修的基线缺陷当现状。

---

## 12. 合入顺序、回滚、客户端

- **合入顺序**：
  0. G4b-J0（Java，必做）；J1 / J2 按 §10 #2 拍板，可与 0 同批；
  1. G4b-1：lib 的 `-Settings`、gateway 段（含常量自检）、Redis 口令、gateway 指纹（含 Redis 口令）、`Show-PeakShavingPlan`、用例 1/2/4/5/6/8（gateway 部分）/9（gateway 部分）/14/16/17/L1；
  2. G1b；
  3. G4b-G（Go，§10 #11 选方案 A 时）；
  4. G4b-2：login 段、排队密钥、login 指纹、用例 3/7/8（login 部分）/9–13/15。
  - 两半都在 D1 第 0、1a 批已有改动的 `k8s_deploy.ps1` 上继续改，不回退别人的段落（`tidb-phase1-…md:22` 也要改这个文件，合并时注意冲突）。
- **回滚**：
  - 止血：直接调 `k8s_deploy.ps1`（包装入口不透传，§4.9）重跑部署并加 `-GatewayRateLimit off` 和/或 `-LoginQueue off`。指纹变化会自动滚动。排队关闭后，在排玩家轮询 `/api/queue-status` 拿到 EXPIRED（410），丢掉 token 重新走 assign-gate 快速通道（`querystatuslogic.go:45-54`）。
  - 不要用 `kubectl rollout undo` 关排队（评审补）：它只退 Pod 模板、不退 ConfigMap，排队仍开着；退到 G1b 之前的镜像时靠显式分 zone 锁键保持按 zone 出队（§4.5），但 §1.5 的问题照旧。
  - 代码回滚：撤销本批提交，staging / prod 生成物回到 G4b 之前的形态（不写 rate-limit 阈值、Queue 段、QueueToken、gateway Redis 口令与削峰指纹，下一次部署时 gateway / login 因注解消失自动滚动）。
  - 遗留的 `rl:*`、`queue:*` 键靠 TTL 自然清掉，紧急情况按 `login-queue-2026-05.md:339-350` 手动清理。
- **客户端**：退避属于另一个仓库 `../mmorpg-client` 的事，本仓库只在设计稿 §十一 登记要求：
  - `code=100` 时，按 `retry_after_ms` 加随机抖动重试（值为 0 时取 2s）；
  - `429` 时，按 30/60/120s 指数退避并加抖动；
  - `410` 时，丢掉 `queue_token`，重新调 assign-gate；
  - （评审补，J0 之后）带 `queue_token` 重调 assign-gate 拿到 `code=100, queue_source="ratelimit"`（不带新 token）时，保留原 token，按 `retry_after_ms` 重试，不要当成排位丢失。

---

## 13. 评审处理记录（对抗式评审，2026-10-12）

本节是历史快照，引用的 Java 仓库 `PARITY.md` 行号不改：其中 `:98` 指「登录排队」行，`:99` 指「开服限流」行，`:98-99` 指这两行。该文件此后一直在追加，这些行号都比现状小，而且还会漂，以行名为准（正文已于 2026-10-15 改为按行名引用）。

> 只读核对：逐条回到工作区代码（含他人未提交改动）、G1b 规格与 Java 仓库 `PARITY.md`；未改任何仓库文件，未运行任何编译 / 测试 / 生成命令。以下「已吸收」均已改进上文对应小节。

### 13.1 必须改（会让实现出错、测试必红、或违反 AGENTS）

| # | 问题 | 依据 | 处理 |
|---|---|---|---|
| M1 | 带任意非空 `queue_token` 的 assign-gate 整个绕过 Bucket4j；login 排队关着时它还直接走快速通道签 gate 票。G4b-1 先合、或 `-LoginQueue off` 止血期间，限流对伪造一个字段的客户端无效，违反 §11.3 fail-closed | `AssignGateController.java:51-57`；`assigngatelogic.go:62-66`、`:90-93` | 已吸收：新增 G4b-J0（只改控制器约 5 行 + `AssignGateControllerTest` 3 例），列为 G4b-1 前置；§0、§1.1、§2、§3、§4.10、§5、§7（门槛 + 实测第 5 步）、§8 #10、R17、§10 #2、§11、§12 |
| M2 | J1（换序）只改 `stubBuckets` 不够：`zoneBucketExhausted_queues`、`accountCooldownHit_secondCallDenied` 按调用顺序内联打桩，换序后被当成 IP 桶的 mock 的 `tryConsume` 回 false，两例必红 | `AssignGateRateLimiterTest.java:151-167`、`:180-200` | 已吸收：§4.10 J1 列出要改写的用例与改法；`zeroZoneSkipsZoneBucket` 核对不受影响 |
| M3 | 用例 13 的 `$peakAllUpRun` 漏了 `-DryRun`，子进程会对本机当前 kubectl context 真执行 apply；zones 路径是相对路径 | 规格原 §6；对照 `k8s_deploy_contract.tests.ps1:1252` | 已吸收：改为完整参数数组，带 `-DryRun` 与 `Join-Path (Get-RepoRoot)` |
| M4 | 用例 3 的 `^Queue:` / `^Secrets:` 永远匹配 0 次：ConfigMap 把 login.yaml 整体缩进 4 格 | `k8s_deploy.ps1:4134` | 已吸收：改为在 login ConfigMap 块上匹配 `(?m)^ {4}Queue:[ ]*\r?$` / `Secrets` |
| M5 | 锁键语义与 G1b 规格冲突：G1b 已定配置值是前缀、实际键 `<前缀>:zone:<id>`，日志为 `elected dispatcher leader zone=%d key=%s`。原稿 §7 的期望日志串在 G1b 之后永远对不上，§4.5 注释与 R8 前提失效 | `launch_spec_G1b.md` §0、§3.1、§4.1、§4.5 | 已吸收：保留显式分 zone 键（价值改述为 `rollout undo` 到 G1b 之前镜像时的纵深防御），更正 §0、§1.4、§4.5 注释与 here-string 注释、§7 第 3 步、R8、§10 #4（另给出「请 G1b 不重复拼接」的可选项）；新增用例 15 把「G1b → G4b-2」变成机械门禁；用例 13 改为按 `Node.ZoneId` 断言 |

### 13.2 重要（规格漏项、测试缺失、迁移 / 回退缺口）

| # | 问题 | 依据 | 处理 |
|---|---|---|---|
| I1 | 打开排队即生效的两条基线缺陷：快速通道 `fast:*` 占位只靠整集合 TTL 回收（余量被重复扣减、「放一批停 60s」循环）；consume 即撤占位而 gate 人数滞后（开区几拍超发数倍）。G1b 规格明确把前者留给 L-02（本批），原稿未提 | `queue.go:355`、`:430-431`、`:456-482`；`capacity.go:72-100`；G1b §4.7 第 2 条；Java `PARITY.md:98` ②③ | 已吸收：新增 §1.5、R15、§10 #11（G4b-G 方案提纲：新键 ZSET 逐个到期 + consume 后留 15s；或书面接受），列入 G4b-2 合入门槛与 §12 顺序 |
| I2 | 排队轮询压在共享单主 Redis：`/api/queue-status` 不限流，每次 `Lookup` 4–5 次往返，约 2.5·Q ops/s；原稿只提了限流桶的 CAS 争用 | `queue.go:220-290`；`QueueStatusController.java:22-26`；`launch_verdict.txt:22` | 已吸收：§0 第 4 条、R16、§10 #12（开服档轮询间隔，默认不加常量，等 L-14） |
| I3 | 账号冷却按未认证 account 计、进程内：限流一开即出现「替他人账号刷 `/api/login` 让其持续 429」的定点封号面 | `AssignGateRateLimiter.java:104-118`；`LoginController.java:50-54` | 已吸收：G4b-J2（冷却键带 IP，1 行 + 1 例，建议）、R18 |
| I4 | gateway 削峰指纹不含 Redis 口令：轮换 `MMORPG_REDIS_PASSWORD` 时 gateway 不滚动，Lettuce 仍用旧口令，限流静默 fail-open | `RateLimitConfig.java:46-58`；原 §4.7 | 已吸收：§4.7 指纹输入加入口令、R19、新增用例 17；go-svc 同类缺口登记为既有、不在本批 |
| I5 | §5 声称「阈值常量越界 → throw」，但 §4.3 代码对 rate-limit 阈值没有任何校验；`release_preflight.ps1` F 节只查 jar 内的值，K8s 真正生效的值逃出该门禁 | `release_preflight.ps1:379-410`；原 §4.3 | 已吸收：`Get-GatewayRateLimitSettings` 加「rps ≥ 1、burst ≥ rps、冷却非负」自检；新增用例 16（AST 抽函数）；用例 2 加 burst ≥ rps 断言；§5 表更正 |
| I6 | 对已在运行、在线已超过 5000 的 staging / prod zone 首次开排队：新登录全部入队直到在线回落，原稿无预检 | `capacity.go:76-100` | 已吸收：R20、§10 #3、README 第 ③ 条 |
| I7 | J1 换序的代价未写：zone 桶已空的请求每次 3 个 Redis 往返（原 1 个），恰在开服尖峰 | `AssignGateRateLimiter.java:83-98` | 已吸收：§4.10 J1「代价」、R4 |
| I8 | R14 漏了攻击者可控的 `rl:zone:<任意 id>` 键（限流先于 zone 校验、按现行顺序不受 IP 限速）、IPv6 /64 内换地址、排队键内存 | `AssignGateController.java:59`；Java `PARITY.md:99` ⑨ | 已吸收：R14 扩写；①由 J1 缓解 |
| I9 | 包装入口（`dev_tools.ps1` / `k8s_image.ps1` / 回档脚本）不透传新参数，事故止血只能直调 `k8s_deploy.ps1`，原稿未在 README 里写明 | `deploy/k8s/AGENTS.md`「包装入口透传」；`k8s_zone_rollback.ps1:25-27` | 已吸收：§3 README 第 ① 条、§4.9、§10 #9（含 `$WrapperPairs` 同步提示）、§12 止血步骤 |
| I10 | J0 / J1 / J2 / G4b-G 落地后，`PARITY.md:98-99` 的「有意差异」会把已修的基线缺陷继续当现状 | Java `PARITY.md:98-99` | 已吸收：§11 补对齐关系与登记要求 |
| I11 | 用例 8 的 `\n` 锚点若在整段原始输出上匹配会因 CRLF 失配 | `deploy_capture.ps1` 的 `Get-ManifestBlocks`（块内统一 LF） | 已吸收：用例 8、用例 2 明确在 `Select-ManifestByName` 的块上匹配 |

### 13.3 小问题

| # | 问题 | 处理 |
|---|---|---|
| m1 | `RateLimitConfig.java` 行号全错（文件只有 107 行；原稿写 :165 / :200-224 / :231 / :249-252），§4.3 还要把错行号写进代码注释 | 已更正 §1.1、§4.3（注释改为指函数名）、R3、R4、R5 |
| m2 | `assigngatelogic.go:114` 应为 `:110`（`free > 0 && queueLen == 0`）；`:117-135` 应为 `:110-129`；`$script:BattleWorkload` 在 `:313` | 已更正 |
| m3 | §1.3 的「每次 NOAUTH 后 fail-open」只是一种可能；Redis 7.2 + Lettuce 6 的 HELLO 握手也可能让 `connect()` 在 Bean 创建时失败（CrashLoop）。两种都由写口令修掉 | 已在 §1.3 注明，供 kind 实测时对照 |
| m4 | `New-GatewayRateLimitYaml` 的值为 `$null` 时，`$v.GetType()` 先抛空引用，报错不再点名键 | 已在 §4.4 单列 `$null` 分支 |
| m5 | §4.10 写 `mvn`、§7 写 `./mvnw`，且 `cd` 失败时不回仓库根 | 统一为 `./mvnw.cmd` + `Push-Location` / `try` / `finally` |
| m6 | R12 漏了 Spring 占位符：gateway 口令含 `${` 时会被当嵌套占位符解析 | 已补进 R12（推断） |
| m7 | 写上 Redis 口令后复合 `/actuator/health` 的 redis 指示器由 DOWN 变 UP（不影响探针分组） | 已在 §4.3 注明 |
| m8 | `Apply-OneGoSvc` 被 AST 单独抽出跑非 login 服务，条件里 `$SvcName -eq 'login'` 必须留在 `-and` 左边 | 已在 §4.7 第 3 点注明 |
| m9 | `docs/design/k8s-client-entry.md:698` 的 staging / prod 准备清单也列环境变量 | 已作为可选项加入 §3 |
| m10 | 「代码回滚 → 生成物回到 dev 形态」不准确；`kubectl rollout undo` 不退 ConfigMap，不能用来关排队 | 已改写 §12 回滚 |

### 13.4 核对通过、未改的结论（抽查）

- `Resolve-InjectedSecret` 的报错原文、dev 档不校验长度 / 占位（`release_common.ps1:679-705`）；`Initialize-InjectedSecrets` 在入口预检之后、deadline 门禁之前（`k8s_deploy.ps1:5934-5943`），用例 10 / 11 的「零写操作」成立。
- Go 侧：宽松档 QueueToken 回落 GateToken 并 WARN（`secrets.go:343-347`）；`Queue.Enabled` 时必需（`:361`）；三把主密钥两两不同（`:369-389`）；`ZoneCapacityFromMap` 按十进制字符串取键（`capacity.go:106-114`），`"101": 5000` 能命中。
- `Get-AuthoritativeScalar` 去掉引号后返回 `dispatcher:lock:login_queue`（`release_common.ps1:53-63`、`:112-126`），拼出的锁键不带多余引号。
- `$ProdEnv` 补 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET` 后，`:990-1020` 与 `:1490-1500` 的负向用例照旧只因被测那一把密钥失败；`k8s_client_entry_contract.tests.ps1` 的管理面口令用例都带 `SkipGoSvc`、`:1213` 在入口预检就被拒，均不需要新变量。
- `k8s_gate_drain.tests.ps1:363-376` 对 login 模板的正则抽取不受新增占位影响（§6 末段已注明）。
- `*.ps1` 在 `.gitattributes` 里强制 CRLF，login 指纹的 here-string 换行跨机器一致，不会因检出系统不同而无故滚动。
- Java 版状态（§11）与 `PARITY.md:98-99` 一致：Java 已做 IP 先于 zone、退还 IP 令牌、token 不绕过（其排队关闭时）、占位逐个到期。

### 13.5 考虑过但未采纳

- 「ConfigMap 只镜像 login.yaml 的锁键前缀、不写分 zone 键」：与 G1b 语义更整齐，但 `rollout undo` 到 G1b 之前的镜像时会退回全局单 leader，丢掉纵深防御；保留原稿做法，改为如实写明代价（M5）。
- 「J0 只过 IP 桶、不过 zone 桶」：排队关着时僵尸网络仍可用伪造 token 绕过 zone 桶；选整套限流、只跳过冷却。
- 在本批直接写 G4b-G 的完整 Go 规格：超出本批「不改 Go」的范围，且需要 login 负责人选方案；只给提纲与合入门槛（§10 #11）。
