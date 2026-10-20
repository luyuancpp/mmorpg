# G2b 实现规格:MySQL 连接预算与连接池可观测性

> 状态:规格(只读核对代码与文档后写成),**未落码、未编译、未测试**。行号以 2026-10-12 工作区(含 D1 第 0、1a 批未提交改动)为准。
> 2026-10-12 已过一轮对抗式评审,成立的问题已改进正文(滚动余量口径、max_connections 1600→1800、内存 4Gi→5Gi、CI 触发路径、import 清单等),逐条见文末「评审处理记录」。
> 依据:`docs/PROGRESS.md:7171-7191`「落点多库连接拓扑核算」;`docs/design/no-single-node-horizontal-scaling-20261001.md:154-167`(更正注);
> `docs/design/tidb-phase1-and-launch-readiness.md:153-182`(§2.4)、`:819`(L-08)、`:815`(L-04);launch_verdict 第 20、21、172、174 行。
> 不做:把 1040 改为不耗重试预算(L-04,登记为后续,见 §12)。

---

## 0. 一页结论

| 项 | 现状(证据) | 本规格的做法 |
|---|---|---|
| 总账 | 只在文档里手算;`max_connections=500`(`mysql.yaml:74`)在 2 个 zone 时零余量 | 部署脚本里写一份**可执行**总账(纯函数),写路径打印一行;契约测试钉住「规划峰值 ≤ max_connections」;滚动余量按「每个 Deployment 多 2 个 Pod(1 个 surge + 1 个仍在优雅退出的旧 Pod)」计 |
| max_connections | 500;MySQL Pod 1 CPU / limits 1Gi(`mysql.yaml:154-160`) | **1800**,内存 requests 1Gi / limits 5Gi(均「待压测确认」) |
| Java gateway 池 | 未配置,Hikari 默认恒开 10 条 × 2 副本 × 每 zone | 显式 `maximum-pool-size: 5`、`minimum-idle: 2`,ConfigMap 镜像 |
| 过时注释 | `db.yaml:73-79`、`config.go:252-253` 写「MySQL 默认 151 留得出余量」 | 改为指向总账函数与 mysql.yaml |
| 池指标 | go/db、data_service 都没导出 `sql.DBStats`;go/db 在 K8s 上连 /metrics 都没开 | 新增 `shared/sqlpoolstats`,导出 `mmorpg_sql_pool_*{sql_client,pool}`;go/db K8s 打开 :9160 |
| 告警 | 无 | 新文件 `deploy/k8s/mysql-connection-alerts.yaml`:2 条 MySQL 侧(需 mysqld_exporter,待拍板部署)+ 3 条客户端侧(现在就有数据) |
| 额外落点库池 | 一个旋钮 `ExtraStoreMaxOpenConn`,无 `ConnMaxIdleTime` | 拆成 zone 家族 / 非 zone 两个旋钮,旧键作回落;加 `ExtraStoreConnMaxIdleSeconds`(默认 60) |

**改动面 30 个文件(去重后),已达到 AGENTS §10.2 的「预计改动 30+ 文件 → 停止并报告、等待授权」阈值:落码前必须拿到负责人显式授权**(§10 第 1 条)。分三批落(A=P0 13 个、B=P1 18 个、C=P2 9 个,有重叠),每批独立可回滚。

---

## 1. 事实核对:共用 K8s MySQL 的全部客户端(以生成的 ConfigMap / manifest 为准)

### 1.1 常驻连接池

| 客户端 | 部署形态 | 每实例池上限(K8s 实际生效值) | 证据 |
|---|---|---|---|
| go/db 本 zone 库主池 | 每 zone 2 副本 | `Database.MaxOpenConn` = 60(ConfigMap 镜像 db.yaml) | `k8s_deploy.ps1:3062`、`:3446`;`go/db/etc/db.yaml:68`;`proto_sql/db.go:262`;`manifests/go-svc/db.yaml:32` |
| go/db 额外落点库池 | 每个按需打开的库一个池,常驻、库数不设上限 | **ConfigMap 不写 Placement 段** → Go 默认 8(idle 2) | `k8s_deploy.ps1:3457-3463`(注释写明不写);`config.go:255-258`、`:291-297`;`store_registry.go:171`、`:299-316`;常驻见 `store_registry.go:29` |
| data_service 快照 / 流水 / 号段 | 每 zone 2 副本;三个 store **各开一个池**,共用 `SnapshotMySQL.MaxOpenConn` | 3 × 5 | `servicecontext.go:208`、`:214`、`:220`(各自 `openMySQL`);`k8s_deploy.ps1:3101`、`:3517`;`data_service.yaml:97`;`mysql.go:71-80` |
| data_service 名字注册表 | 同上,独立池 | **ConfigMap 不写 PlayerName 段** → Go 默认 20 | `servicecontext.go:236-239`;`config.go:569-570`、`:588-590`;`data_service.yaml:110` |
| data_service route store(D1 第 1a 批) | 同上,独立池,**只在 db_first 时装配** | 默认 20;K8s 固定 `Mode: redis` → 今天 0 | `servicecontext.go:252`、`:290-299`;`config.go:268`、`:323-325`;`k8s_deploy.ps1:3529-3530` |
| Java gateway | **每 zone**(zone namespace)2 副本,只在给了 `-JavaSvcRegistry` 时部署 | 未配置 Hikari → 默认 `maximumPoolSize=10`,`minimumIdle` 缺省等于它,即恒开 10 条(Hikari 文档,外部事实,未在本机核实) | `java-svc/gateway.yaml:24`;`k8s_deploy.ps1:4924-4935`、`:5406`;`application.yaml:10-14` |
| friend | 全局(infra namespace)2 副本 | `MySQL.MaxOpenConn` = 20 | `k8s_deploy.ps1:980`(Global)、`:4077-4084`;`go/friend/etc/friend.yaml:97`;`go-svc/friend.yaml:63` |
| trade | 全局 2 副本 | 20 | `k8s_deploy.ps1:3962-3969`;`go/trade/etc/trade.yaml:55`;`go-svc/trade.yaml:52` |

不连 MySQL 或 K8s 上不启用的:
- login `PasswordAuth`(池 20)K8s ConfigMap 不写、默认 `Enabled: false`(`k8s_deploy.ps1:3410-3412`;`go/login/etc/login.yaml:342-345`)。**一旦启用,每 zone 再 +2×20**。
- guild / team 不在 `$GoSvcCatalogue`,K8s 不部署(`k8s_deploy.ps1:149`)。
- C++ 节点不连 MySQL;`k8s_deploy.ps1:1872` 的 `max_connections: 100` 是 zone Redis 客户端池,别混淆。
- chat / match / scene-manager / player-locator / client-rpc-router 的服务 yaml 里没有 MySQL 段(grep 核对)。

### 1.2 临时连接(计入「运维余量」)

data-service-migrate Job(`data-service-migrate.yaml:106`,池上限沿用 SnapshotMySQL 5)、friend / trade migrate Job(`friend-migrate.yaml:130`、`trade-migrate.yaml:121`)、go/db `cmd/migrate`(`cmd/migrate/main.go:194`,4 条)、备份 CronJob 的 mysqldump(`mysql-backup-cronjob.yaml:39`、`:81`)、readiness 探针 mysqladmin(`mysql.yaml:161-172`)、merge_zone / route_tool / 人工会话。合计取 **20**(估计值,待实测)。

### 1.3 滚动余量

- db、data-service 显式 `maxSurge: 1`(`go-svc/db.yaml:35-39`;`go-svc/data-service.yaml:37-42`);gateway / friend / trade 没写 strategy,K8s 默认 25%,2 副本向上取整也是 1(K8s 默认值,外部事实)。
- **maxSurge 1 不等于「只多 1 个 Pod 的连接」**(评审补):Deployment 控制器算 surge 时不计正在退出的 Pod(K8s Deployment 文档的滚动更新一节明说「滚动期间 Pod 数与资源会超过 replicas + maxSurge,直到旧 Pod 的 terminationGracePeriodSeconds 到期」,外部事实,未在本机核实)。
  2 副本、maxSurge 1、maxUnavailable 0 的序列:N1 就绪 → O1 开始退出 → 立即起 N2 → N2 就绪 → O2 开始退出;O1 / O2 的退出窗口与 N1 / N2 重叠,同一时刻最多 4 个 Pod 持有连接 = replicas + 2。
  退出中的 Pod 不是空池:go/db 的 preStop 先 `sleep 5`(`go-svc/db.yaml:80-83`)期间仍在消费、按满池跑,随后排空最多约 15s,`terminationGracePeriodSeconds: 45`(`:97`);data-service 同样 preStop 5s(`go-svc/data-service.yaml:97-100`)、grace 30s。
  所以总账的滚动余量按**每个 Deployment 多 2 个实例**计(`Get-MysqlConnectionPlan.RolloutExtraPodsPerDeployment = 2`)。
- `-WaitReady` 用的 `kubectl rollout status` 在新 RS 就绪即返回,不等旧 Pod 退出完(`Wait-ForDeploymentReady`),所以即使逐 zone 等待,上一个 zone 仍在退出的旧 Pod 也可能与下一个 zone 的 surge 重叠最多一个 grace 窗口(≤45s)。总账不覆盖这一段,见风险 §9 第 4 条。
- **`all-up` 不带 `-WaitReady` 时各 zone 的滚动会重叠**:`Wait-ForZoneReady` 在没有 `-WaitReady` 时直接返回(`k8s_deploy.ps1:2994-2996`),`all-up` 逐 zone 调 `Apply-Zone`(`:5975-5981`)。总账只按「一个 zone 在滚」算,重叠滚动不在保证内(风险 §9 第 4 条)。

### 1.4 额外 zone 家族库的预算按 zone 数平方增长

每个 go/db 实例最坏会打开其余全部 zone 的库(冷缓存访客读、合服 pin 后的源区库),且打开后常驻(`store_registry.go:29`)。所以 zone 家族额外池的名义预算是 `Z × 2 × 8 × (Z−1)`:Z=4 时 192,Z=10 时 1440。这是「连接网状」在**同一台 MySQL 上**的真实形态,也是 P2 要拆旋钮的直接原因。合服留下的源区库(已不在 zones 配置里)另算。

### 1.5 其它现状

- go/db 在 K8s 上 **/metrics 是关的**:ConfigMap 不写 `MetricsListenAddr`(`k8s_deploy.ps1:3423-3465`),缺省空串 = 不起端口(`go/db/internal/config/config.go:15-20`;`metrics.go:260-265`),manifest 也没有 9160 端口(`go-svc/db.yaml:67-69`)。`owner-epoch-alerts.yaml:9-11` 的抓取前提在 K8s 上其实不成立。
- data_service 指标已开 :9260(`k8s_deploy.ps1:3532`;`go-svc/data-service.yaml:51-54`、`:82-84`)。
- 仓库里**没有** mysqld_exporter(全仓 grep `mysqld_exporter|mysql_global_status` 无结果),MySQL 服务端连接数今天无从观测。
- 超限后果(读代码推断,未实测):go/db 不特判 1040,耗尽 3 次重试进 `kafka:dead:queue:*`,死信无自动消费者(`PROGRESS.md:7181`)。

---

## 2. 总账公式与取值

### 2.1 公式(本规格实现成 PowerShell 纯函数,§4.1.2)

记号:Z = zone 数;E_z = 每个 go/db 实例打开的额外 zone 家族库数;E_n = 非 zone 落点库数(Phase 2 的 `player_store_*`);F = route store 是否 db_first(0/1);R = 滚动期间每个 Deployment 多出的实例数(§1.3,取 2)。

```
db_inst  = 60 + 8·E_z + 8·E_n                    # go/db 每实例(批 C 后两个 8 分别取 ZoneStore / NonZoneStore 旋钮)
ds_inst  = 3·5 + 20 + 20·F                        # data_service 每实例
gw_inst  = 5                                      # Java gateway 每实例(批 A 起显式配置,原隐式 10)
per_zone = 2·db_inst + 2·ds_inst + 2·gw_inst
global   = 2·20 (friend) + 2·20 (trade) = 80
steady   = Z·per_zone + global
surge    = R·(db_inst + ds_inst + gw_inst + 20 + 20)  # 一个 zone 的三个 Deployment + 两个全局 Deployment 各多 R 个实例
                                                      # (1 个 maxSurge 新 Pod + 1 个仍在优雅退出、连接未释放的旧 Pod)
peak     = steady + surge + ops(20)
```

自检锚点:取 gw_inst=10、E_z=E_n=F=0、不计 surge 与 ops 时,`steady = Z·210 + 80`,Z=4 → 920、Z=2 → 500,与 `PROGRESS.md:7178-7180` 逐字一致。

### 2.2 规划档与建议值

规划档(`Get-MysqlConnectionPlan`,§4.1.2):**Z=4、E_z=Z−1=3(最坏)、E_n=0(Phase 2 未开)、F=1(D1 第 2 阶段会切 db_first,提前计入)、R=2(§1.3)、ops=20**。

| 场景 | db_inst | ds_inst | per_zone | steady | surge(R=2) | **peak** | 对 1800 余量 |
|---|---|---|---|---|---|---|---|
| 规划档 Z=4 | 84 | 55 | 288 | 1232 | 368 | **1620** | 180(≈10%) |
| 仓库 zones.json Z=2(E_z=1) | 68 | 55 | 256 | 592 | 336 | **948** | 852 |
| 旧口径对照 Z=4(gw 10、E=0、F=0,无 surge / ops) | 60 | 35 | 210 | 920 | — | 920 | — |
| `zones.10zones.yaml`(Z=10,E_z=9)| 132 | 55 | 384 | 3920 | 464 | **4404** | **超 2604** |

(评审前按 R=1 算的规划峰值是 1436,对 1600 留 164;R=2 后 1620 已超 1600,所以上限改为 1800。)

结论:
- **max_connections = 1800**(待压测确认)。比规划峰值高约 10%,不追求「高到不可能触发」:上限要保留保护作用(`tidb-phase1-and-launch-readiness.md:174-177`)。
- 10 个 zone 的配置在单台 MySQL 上无解,与池怎么调无关(E_z=0、F=0 时稳态也有 2080,F=1 时 2480)。部署时会打 WARN(§4.1.3);要上 10 个 zone 必须先完成 TiDB Phase 1 或重新设计池。
- Phase 2(E_n≥1,且非 zone 池要调大)打开前必须重做规划档(launch L-16)。

### 2.3 MySQL Pod 内存(同步调整,待压测确认)

| 组成 | 估计 | 依据 |
|---|---|---|
| `innodb_buffer_pool_size` | 128Mi | my.cnf 未设,MySQL 8.0 默认值(外部事实) |
| 全局开销(performance_schema、数据字典、日志缓冲、表缓存) | ≈ 0.5Gi | 经验值,未实测 |
| 1800 条连接 × ≈1MiB 常驻(线程栈 + 会话缓冲) | ≈ 1.8Gi | 经验值,未实测 |
| 活跃会话的排序 / 连接 / 读缓冲与大包(快照行最大 8MiB,`db.yaml:104-106`) | ≈ 0.6Gi | 估算 |
| TempTable 内存临时表(`temptable_max_ram` 8.0 默认 1Gi,全局上限) | ≤ 1Gi | 外部事实,未在本机核实 |
| **合计** | ≈ 4.0Gi | → **limits 5Gi**(各项都是经验值,留约 1Gi;超 limits 是 OOMKill,比 1040 更糟) |

requests 从 512Mi 提到 **1Gi**:保持 dev kind 节点可调度。生产应 requests = limits(Guaranteed QoS),否则节点内存紧张时超出 requests 的 MySQL 会先被驱逐(待拍板 §10 第 3 条)。CPU 不在本次范围(1 核仍是首日硬瓶颈,`tidb-phase1-and-launch-readiness.md:90`)。

---

## 3. 分批与文件清单

### 批 A(P0,13 个文件,无 Go 逻辑改动)

| # | 文件 | 新/改 | 要点 |
|---|---|---|---|
| A1 | `deploy/k8s/manifests/infra/mysql.yaml` | 改 | `max_connections = 1800`(:74)+ 依据注释;resources memory requests 1Gi / limits 5Gi(:154-160)+ 依据注释 |
| A2 | `java/gateway_node/src/main/resources/application.yaml` | 改 | `spring.datasource.hikari.maximum-pool-size: 5`、`minimum-idle: 2` + 用量注释 |
| A3 | `java/gateway_node/src/test/java/com/game/gateway/repository/GatewayDataSourcePoolConfigTest.java` | 新 | H2 上校验 Hikari 两个值生效 |
| A4 | `go/db/etc/db.yaml` | 改 | 更正 :68-79 的「151」注释 |
| A5 | `go/db/internal/config/config.go` | 改 | 更正 :250-254 的「151」注释(只改注释) |
| A6 | `tools/scripts/k8s_deploy.ps1` | 改 | 总账五个函数 + 写路径打印;db ConfigMap 镜像 `Placement.ExtraStore*`;data-service ConfigMap 镜像 `PlayerName.*` 与 `RouteStore.MaxOpenConn/MaxIdleConn`;gateway ConfigMap 镜像 Hikari;相关注释 |
| A7 | `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | 改 | 新增 7 例(88 → 95) |
| A8 | `deploy/k8s/manifests/go-svc/db.yaml` | 改 | :31 预算注释改指向总账 |
| A9 | `deploy/k8s/manifests/go-svc/data-service.yaml` | 改 | :34 预算注释改指向总账 |
| A10 | `deploy/k8s/README.md` | 改 | :505、:596、:636 三处预算说明指向总账;新增短节「MySQL 连接总账」 |
| A11 | `docs/design/no-single-node-horizontal-scaling-20261001.md` | 改 | §3.4 更正注(:154-167)之后**追加**一条落码说明,不改原文 |
| A12 | `docs/design/tidb-phase1-and-launch-readiness.md` | 改 | §2.4 表(:161 gateway 行、:165 滚动余量行)与 L-08(:819)之后**追加**状态注,不改原文;P1-1(:910)的「导出 `sql.DBStats`(与 L-08 合并)」追加「由 G2b 批 B 承担,P1-1 不再另注册收集器」 |
| A13 | `.github/workflows/deploy-config-tests.yml` | 改 | push / pull_request 两处 `paths` 各加 `java/gateway_node/src/main/resources/application.yaml`、`deploy/k8s/*.yaml`、`deploy/k8s/*.json`:总账的输入(gateway Hikari、zones.json)与批 B 的告警阈值文件今天都不在触发路径里,改它们不会跑契约测试(现状 `deploy-config-tests.yml:30-71` 的两份 `paths`) |

### 批 B(P1,18 个文件)

| # | 文件 | 新/改 | 要点 |
|---|---|---|---|
| B1 | `go/shared/sqlpoolstats/collector.go` | 新 | 按池类别聚合 `sql.DBStats` 的 Prometheus Collector |
| B2 | `go/shared/sqlpoolstats/collector_test.go` | 新 | 7 例,无外部依赖 |
| B3 | `go/db/internal/metrics/metrics.go` | 改 | 池类别常量、`NewSQLPoolCollector`、`RegisterSQLPools` |
| B4 | `go/db/internal/metrics/metrics_test.go` | 新 | 1 例 |
| B5 | `go/db/internal/logic/pkg/proto_sql/store_registry.go` | 改 | `StoreRegistry.home` 字段、`PoolSamples()` |
| B6 | `go/db/internal/logic/pkg/proto_sql/store_registry_test.go` | 改 | 1 例 |
| B7 | `go/db/db.go` | 改 | `metrics.RegisterSQLPools(stores.PoolSamples)` |
| B8 | `go/data_service/internal/store/dbstats.go` | 新 | `DBStatsOf(any)`:五个 store 的类型分支 |
| B9 | `go/data_service/internal/store/dbstats_test.go` | 新 | 1 例(表驱动) |
| B10 | `go/data_service/internal/svc/servicecontext.go` | 改 | `SQLPoolSamples()` + 纯函数 `sqlPoolSamples` |
| B11 | `go/data_service/internal/svc/servicecontext_sqlpool_test.go` | 新 | 2 例 |
| B12 | `go/data_service/internal/metrics/metrics.go` | 改 | 池类别常量、`NewSQLPoolCollector`、`RegisterSQLPools` |
| B13 | `go/data_service/internal/metrics/metrics_test.go` | 改 | 1 例 |
| B14 | `go/data_service/data_service.go` | 改 | `metrics.RegisterSQLPools(svcCtx.SQLPoolSamples, c.RouteStore.DBFirst())` |
| B15 | `deploy/k8s/mysql-connection-alerts.yaml` | 新 | PrometheusRule,两组 5 条 |
| B16 | `deploy/k8s/manifests/go-svc/db.yaml` | 改 | 9160 `metrics` 端口 + prometheus 注解 |
| B17 | `tools/scripts/k8s_deploy.ps1` | 改 | db ConfigMap 写 `MetricsListenAddr: ":9160"` |
| B18 | `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | 改 | 新增 2 例(95 → 97) |

### 批 C(P2,9 个文件)

| # | 文件 | 新/改 | 要点 |
|---|---|---|---|
| C1 | `go/db/internal/config/config.go` | 改 | 新键 `ZoneStore*` / `NonZoneStore*` / `ExtraStoreConnMaxIdleSeconds`,旧键回落 |
| C2 | `go/db/internal/config/config_test.go` | 改 | 新 3 例 + 改 1 例 |
| C3 | `go/db/etc/db.yaml` | 改 | Placement 段改写新键(:122-125) |
| C4 | `go/db/internal/logic/pkg/proto_sql/store_registry.go` | 改 | `StorePoolOptions`、按家族选池、`SetConnMaxIdleTime` |
| C5 | `go/db/internal/logic/pkg/proto_sql/store_registry_test.go` | 改 | 3 例 |
| C6 | `go/db/db.go` | 改 | 启动横幅(:229-232) |
| C7 | `tools/scripts/k8s_deploy.ps1` | 改 | db ConfigMap 镜像新键;总账输入改读新键 |
| C8 | `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | 改 | 镜像用例换键(总数不变,97) |
| C9 | `docs/design/player-storage-placement.md` | 改 | §6.1(:142)键名更新 |

`docs/PROGRESS.md` 本阶段不写(由 Docs 阶段统一追加);不新增 `docs/` 下文档,`docs/README.md` 无需登记。

去重后的文件数:批 A 13 + 批 B 新增 15(`go-svc/db.yaml`、`k8s_deploy.ps1`、契约测试与批 A 重叠)+ 批 C 新增 2(`go/db/internal/config/config_test.go`、`player-storage-placement.md`)= **30**。
`deploy/k8s/manifests/go-svc/friend.yaml:31`、`trade.yaml:23` 两处「要算进 max_connections」的注释刻意不改(再加就是 32 个文件);它们的说法没错,只是没指向总账。

---

## 4. 详细设计

### 4.1 批 A(P0)

#### 4.1.1 `mysql.yaml`

`:73-74` 改为(注释讲依据,数值标待压测):

```yaml
    # ── 连接上限(2026-10-12 G2b;数值待压测确认)───────────────────────
    # 共用这台 MySQL 的是全部 zone 的 go/db、data_service、Java gateway,外加全局的 friend、trade。
    # 总账公式与规划参数在 tools/scripts/k8s_deploy.ps1(Get-MysqlConnectionPlan / Get-MysqlConnectionLedger),
    # 部署写路径打印 "MySQL connection ledger" 一行;契约测试钉住「规划峰值 ≤ 本值」。
    # 规划档:4 个 zone、每个 go/db 实例打开其余 3 个 zone 的库(最坏)、Phase 2 未开、route store 按 db_first 计入,
    # 峰值 1620(含一个 zone 滚动发布 —— 每个 Deployment 多 1 个新 Pod + 1 个仍在优雅退出的旧 Pod —— 与运维余量 20),
    # 本值留约 10% 余量。
    # 以前的 500 只够 2 个 zone 的稳态(恰好 500,零余量);4 个 zone 稳态就要 920(docs/PROGRESS.md 2026-10-12)。
    # 超限的后果不只是变慢:go/db 把 1040 当普通错误,耗尽重试后进没有消费者的死信(读代码推断,未实测)。
    # 改本值必须同步改下面 Deployment 的内存 limits(按每条连接约 1MiB 常驻估,待实测)。
    # 注意:本 ConfigMap 变了不会让 mysqld 重启 —— 只改这一行时运行中的实例仍是旧值,而总账与契约测试已按新值算。
    # 只改上限时要么在维护窗口 `kubectl -n <infra> rollout restart deploy/mysql`,要么 `SET PERSIST max_connections = <新值>`。
    max_connections = 1800
```

`:154-160` 改为:

```yaml
          # 内存随 max_connections 同步(2026-10-12 G2b;待压测确认):
          #   innodb_buffer_pool_size 默认 128Mi + 全局开销约 0.5Gi + 1800 条连接 × 约 1MiB
          #   + 活跃会话缓冲与大包约 0.6Gi + TempTable(temptable_max_ram 默认 1Gi)≈ 4.0Gi,
          #   各项都是经验值,留约 1Gi → limits 5Gi。
          #   超过 limits 是 OOMKill:单实例 MySQL 整体重启,比 1040 更糟。
          # requests 只提到 1Gi,保证 dev kind 节点能调度;生产应 requests = limits(Guaranteed),见 G2b 待拍板。
          # CPU 不在本次调整范围:1 核仍是开服硬瓶颈(docs/design/tidb-phase1-and-launch-readiness.md §1.5)。
          resources:
            requests:
              cpu: 200m
              memory: 1Gi
            limits:
              cpu: "1"
              memory: 5Gi
```

注意:Deployment 模板变了会触发 `Recreate`(`mysql.yaml:84-85`),即**全部 zone 的数据层停一次**(风险 §9 第 1 条)。

#### 4.1.2 `k8s_deploy.ps1`:总账函数(放在 `Assert-GrpcClientDeadlineBudget`(:878)之后)

全部只经 `$RepoRoot` 读文件,不依赖 `$InfraManifestsDir` 等脚本变量:契约测试按 AST 抽出前四个函数直接调用(与 `k8s_deploy_contract.tests.ps1:322-338` 的 deadline 守卫同一做法)。

```powershell
# ─────────────────────────────────────────────────────────────────
# MySQL 连接总账(G2b;docs/PROGRESS.md 2026-10-12「落点多库连接拓扑核算」)
# ─────────────────────────────────────────────────────────────────

<#
.SYNOPSIS
	MySQL 连接总账的规划档:deploy/k8s/manifests/infra/mysql.yaml 的 max_connections 按它的峰值取值。
.DESCRIPTION
	改这里 = 重算 max_connections 与 MySQL Pod 内存(mysql.yaml 里两处注释同步改)。
	契约测试钉住「规划峰值 ≤ max_connections」。
	(批 B 起再加一句:deploy/k8s/mysql-connection-alerts.yaml 的额外库数阈值与这里同源,契约测试核对 —— 批 A 时那个文件还不存在,不要提前写。)
	纯函数,契约测试按 AST 抽出直接调用。
#>
function Get-MysqlConnectionPlan {
	$zoneCount = 4
	return [ordered]@{
		ZoneCount         = $zoneCount
		# 最坏情形:每个 go/db 实例都打开了其余全部 zone 的库(冷缓存访客读、合服 pin 后的源区库),打开后常驻。
		# 合服留下、已不在 zones 配置里的源区库不在其中 —— 合服后按实际库数重算。
		ExtraZoneStores   = $zoneCount - 1
		# Phase 2 的非 zone 落点库(player_store_*)未开;打开 PinOnCreate 前必须重做这份规划(launch L-16)。
		NonZoneStores     = 0
		# D1 第 2 阶段会把 RouteStore 切到 db_first(player-route-mapping-durability-proposal.md §7 P2),提前计入它的独立池。
		RouteStoreDbFirst = $true
		# 滚动发布时每个 Deployment 多出的实例数:maxSurge 1 的新 Pod + 1 个仍在优雅退出、连接还没释放的旧 Pod。
		# Deployment 控制器算 surge 时不计正在退出的 Pod,2 副本时同一刻最多 replicas + 2 个 Pod 持有连接
		# (K8s Deployment 文档滚动更新一节,外部事实;go/db 的 preStop + 排空约 20s、grace 45s)。
		RolloutExtraPodsPerDeployment = 2
		# 迁移 Job、备份、探针、merge_zone / route_tool、人工会话(估计值,待实测)。
		OpsReserve        = 20
	}
}

function Get-MysqlMaxConnectionsFromManifest { ... }   # 见下
function Get-ManifestDeploymentReplicas { param([Parameter(Mandatory = $true)][string]$RelativePath) ... }
function Get-MysqlConnectionLedgerInputs { ... }
function Get-MysqlConnectionLedger { param(...) ... }
function Write-MysqlConnectionLedger { ... }           # 只在写路径调用,不进 AST 抽取名单
```

**`Get-MysqlMaxConnectionsFromManifest`**:读 `deploy/k8s/manifests/infra/mysql.yaml`,正则 `(?m)^\s*max_connections\s*=\s*(\d+)\s*$` 必须**恰好命中 1 次**,否则 throw「MySQL 连接总账:… max_connections 应恰好出现 1 次,实际 N 次(fail-closed)」。文件不存在同样 throw。

**`Get-ManifestDeploymentReplicas -RelativePath`**:读 manifest 文本,正则 `(?m)^  replicas:\s*(\d+)\s*$`(两格缩进 = Deployment 顶层 spec)恰好 1 次,否则 throw。五个 manifest 各只有一个 Deployment(`go-svc/db.yaml:32`、`go-svc/data-service.yaml:35`、`java-svc/gateway.yaml:24`、`go-svc/friend.yaml:63`、`go-svc/trade.yaml:52`)。

**`Get-MysqlConnectionLedgerInputs`**:返回 `[ordered]`,值全部来自权威文件(`Get-AuthoritativeScalar`,`k8s_deploy.ps1:502-514`,查不到即 throw):

| 键 | 来源 |
|---|---|
| `MaxConnections` | `Get-MysqlMaxConnectionsFromManifest` |
| `DbReplicas` | `deploy/k8s/manifests/go-svc/db.yaml` |
| `DbHomeMaxOpen` | `go/db/etc/db.yaml` `ServerConfig.Database.MaxOpenConn` |
| `DbZoneStoreMaxOpen` | 批 A:`Placement.ExtraStoreMaxOpenConn`;批 C 改 `Placement.ZoneStoreMaxOpenConn` |
| `DbNonZoneStoreMaxOpen` | 批 A:`Placement.ExtraStoreMaxOpenConn`;批 C 改 `Placement.NonZoneStoreMaxOpenConn` |
| `DsReplicas` | `deploy/k8s/manifests/go-svc/data-service.yaml` |
| `DsAuditStorePools` | 常数 3,注释写明「snapshot / transaction_log / id_segment 各开一个池,共用 SnapshotMySQL.MaxOpenConn(go/data_service/internal/svc/servicecontext.go NewServiceContext)」 |
| `DsAuditMaxOpen` | `go/data_service/etc/data_service.yaml` `SnapshotMySQL.MaxOpenConn` |
| `DsPlayerNameMaxOpen` | 同上 `PlayerName.MaxOpenConn` |
| `DsRouteMaxOpen` | 同上 `RouteStore.MaxOpenConn` |
| `GatewayReplicas` | `deploy/k8s/manifests/java-svc/gateway.yaml` |
| `GatewayMaxPool` | `java/gateway_node/src/main/resources/application.yaml` `spring.datasource.hikari.maximum-pool-size` |
| `FriendReplicas` / `FriendMaxOpen` | `go-svc/friend.yaml` / `go/friend/etc/friend.yaml` `MySQL.MaxOpenConn` |
| `TradeReplicas` / `TradeMaxOpen` | `go-svc/trade.yaml` / `go/trade/etc/trade.yaml` `MySQL.MaxOpenConn` |

gateway 无论本次是否部署(`-JavaSvcRegistry` 为空时跳过)都计入:总账按「可能同时存在」保守算。login `PasswordAuth` 不计入(K8s 不写),由契约测试钉住「不写」这一前提。

**`Get-MysqlConnectionLedger`**(纯函数):

```powershell
function Get-MysqlConnectionLedger {
	param(
		[Parameter(Mandatory = $true)][System.Collections.IDictionary]$Inputs,
		[Parameter(Mandatory = $true)][int]$ZoneCount,
		[Parameter(Mandatory = $true)][int]$ExtraZoneStores,
		[Parameter(Mandatory = $true)][int]$NonZoneStores,
		[Parameter(Mandatory = $true)][bool]$RouteStoreDbFirst,
		# 规划档与写路径传 Get-MysqlConnectionPlan 的值(2);自检复现 2026-10-12 的名义值时传 0(那份核算不含滚动余量)。
		[Parameter(Mandatory = $true)][int]$RolloutExtraPodsPerDeployment,
		[Parameter(Mandatory = $true)][int]$OpsReserve
	)
	$required = @('MaxConnections', 'DbReplicas', 'DbHomeMaxOpen', 'DbZoneStoreMaxOpen', 'DbNonZoneStoreMaxOpen',
		'DsReplicas', 'DsAuditStorePools', 'DsAuditMaxOpen', 'DsPlayerNameMaxOpen', 'DsRouteMaxOpen',
		'GatewayReplicas', 'GatewayMaxPool', 'FriendReplicas', 'FriendMaxOpen', 'TradeReplicas', 'TradeMaxOpen')
	$v = @{}
	foreach ($key in $required) {
		$n = 0
		if (-not $Inputs.Contains($key) -or -not [int]::TryParse([string]$Inputs[$key], [ref]$n) -or $n -le 0) {
			throw "MySQL 连接总账:输入 $key='$($Inputs[$key])' 必须是正整数(fail-closed:不替你猜一个连接数)"
		}
		$v[$key] = $n
	}
	if ($ZoneCount -le 0 -or $ExtraZoneStores -lt 0 -or $NonZoneStores -lt 0 -or $RolloutExtraPodsPerDeployment -lt 0 -or $OpsReserve -lt 0) {
		throw "MySQL 连接总账:ZoneCount=$ZoneCount 必须为正,ExtraZoneStores=$ExtraZoneStores / NonZoneStores=$NonZoneStores / RolloutExtraPodsPerDeployment=$RolloutExtraPodsPerDeployment / OpsReserve=$OpsReserve 不能为负"
	}
	$dbPerInstance = $v.DbHomeMaxOpen + $ExtraZoneStores * $v.DbZoneStoreMaxOpen + $NonZoneStores * $v.DbNonZoneStoreMaxOpen
	$dsPerInstance = $v.DsAuditStorePools * $v.DsAuditMaxOpen + $v.DsPlayerNameMaxOpen
	if ($RouteStoreDbFirst) { $dsPerInstance += $v.DsRouteMaxOpen }
	$gatewayPerInstance = $v.GatewayMaxPool
	$perZone = $v.DbReplicas * $dbPerInstance + $v.DsReplicas * $dsPerInstance + $v.GatewayReplicas * $gatewayPerInstance
	# 变量名不用 $global:它是作用域修饰符的名字,读的人会以为在写全局作用域。
	$globalPools = $v.FriendReplicas * $v.FriendMaxOpen + $v.TradeReplicas * $v.TradeMaxOpen
	$steady = $ZoneCount * $perZone + $globalPools
	# 一个 zone 的三个 Deployment 与两个全局 Deployment 各多 RolloutExtraPodsPerDeployment 个实例
	# (maxSurge 1 的新 Pod + 仍在优雅退出的旧 Pod;K8s 默认 25% 在 2 副本时 maxSurge 也是 1)。
	# all-up 不带 -WaitReady 时各 zone 的滚动会重叠,不在这个余量里。
	$surge = $RolloutExtraPodsPerDeployment * ($dbPerInstance + $dsPerInstance + $gatewayPerInstance + $v.FriendMaxOpen + $v.TradeMaxOpen)
	$peak = $steady + $surge + $OpsReserve
	return [ordered]@{
		DbPerInstance = $dbPerInstance; DsPerInstance = $dsPerInstance; GatewayPerInstance = $gatewayPerInstance
		PerZone = $perZone; Global = $globalPools; Steady = $steady; RolloutSurge = $surge; OpsReserve = $OpsReserve
		Peak = $peak; MaxConnections = $v.MaxConnections; Headroom = $v.MaxConnections - $peak
	}
}
```

PowerShell 细节:`Mandatory` 的 `[int]` 传 0、`[bool]` 传 `$false` 都能绑定(Mandatory 只拒 null / 空串 / 空集合);调用处一律具名传参。

#### 4.1.3 写路径打印(不阻断)

`Write-MysqlConnectionLedger`:算两份 —— 本次 zones 配置(zones 配置 ∪ `-ZoneId`,与 `Get-InfraZoneIds`(`k8s_deploy.ps1:5453-5470`)同一口径;E_z 取 `max(0, Z−1)`,E_n / F / R / ops 取规划档)与规划档,打印一行。
zones 配置文件缺席时**不要直接调 `Get-InfraZoneIds`**:它那条 WARNING 讲的是「mysql-init-sql 只预建 -ZoneId 的库」,出现在 zone-up 的输出里是误导。做法:先 `Test-Path (Resolve-ZonesConfigPath)`,存在才调 `Get-InfraZoneIds`,缺席时 Z 取 1(只有 `-ZoneId`)并在那一行末尾加 `zones_config=missing`。
`-ZoneId` 默认 101 恰在仓库 `zones.json` 里;传了不在 zones 配置里的 `-ZoneId` 时 Z 会多 1,这是保守方向,不另处理。
打印一行:

```
MySQL connection ledger: zones=<Z> steady=<S> peak=<P> max_connections=<M> headroom=<H>; plan zones=4 peak=<P'> headroom=<H'>
```

任一份 `Peak > MaxConnections` 时 `Write-Warning`,**文本以无空格的 `MySQL连接总账超限:` 开头**(ConsoleHost 可能按窗口宽度在空格处折行,契约测试只断这一段,先例见 `k8s_deploy_contract.tests.ps1:1505-1507`),后面写清哪一份、峰值、max_connections、两条出路(先完成 TiDB Phase 1,或重做 `Get-MysqlConnectionPlan` 并同步 mysql.yaml 的 max_connections 与内存)。

调用点:`k8s_deploy.ps1:5943`(`Assert-GrpcClientDeadlineBudget` 之后),只在 `zone-up / all-up / infra-up` 写路径;`*-down / *-status` 不受影响。**只打 WARN 不 throw**(是否升级为门禁见 §10 第 4 条)。输入文件缺键时 throw —— 与 ConfigMap 生成器同一纪律,本来就会在生成 ConfigMap 时 throw。

PowerShell 注意:字符串里变量后紧跟冒号要写 `${var}:`;`-f` 格式串里的中文括号不需要转义。

> **更正(2026-10-15,负责人「按最标准的去做」)**:本节第一段的「zones 配置 ∪ `-ZoneId`,与 `Get-InfraZoneIds` 同一口径」「存在才调 `Get-InfraZoneIds`」与「传了不在 zones 配置里的 `-ZoneId` 时 Z 会多 1,这是保守方向,不另处理」作废。`all-up` / `infra-up` 不为 `-ZoneId` 部署 zone 级工作负载(db / data-service / gateway):`-ZoneId` 在这两条命令里用作全局 Go 服务与 battle 的 ZoneId、mysql-init-sql 预建库的输入,本身不产生每 zone 的 MySQL 连接;把它(缺省值 101 或显式传入)并入 Z 的话,zones 配置不含它时(如 `zones.10zones.yaml` 的 1..10)Z 凭空多 1,可能打出虚假的 `[MySQLConnectionLedger]OverLimit`。现行口径按命令计 Z(同一 zoneId 只算一次):`zone-up` = zones 配置 ∪ 本次 `-ZoneId`(不在配置里也并入,这个 zone 确实要部署);`all-up` / `infra-up` = 只数 zones 配置里的 zone。zones 配置直接用 `Get-ZonesFromJson` 读,不调 `Get-InfraZoneIds`(它为 mysql-init-sql 预建库服务,在那里并入 `-ZoneId` 是对的)。缺席分支不变:Z 取 1,行尾标 `zones_config=missing`(`all-up` 走不到这一支,更早的入口预检读 zones 配置时已报错)。实现见 `tools/scripts/k8s_deploy.ps1` 的 `Write-MysqlConnectionLedger`;契约测试是 `k8s_deploy_contract.tests.ps1`「MySQL 连接总账」一节新增的 all-up / infra-up / zone-up 三例(`zones.10zones.yaml`,zones=10 / zones=10 / zones=11);§6.1 表 A 行的 Z = 11 是 zone-up,不受影响。未编译、未测试。正文不改。

#### 4.1.4 ConfigMap 镜像池参数(让总账输入 == 集群实际值)

1. **db ConfigMap**(`k8s_deploy.ps1:3423-3465`):`RedisClient` 块之后加

   ```
   # 额外落点库连接池(镜像 go/db/etc/db.yaml;计入 MySQL 连接总账 Get-MysqlConnectionLedger)。
   # 只写连接池两项:AllowStoreFamilies / Required / Redis 仍不写 = 取缺省(true / false / 复用上面的 RedisClient),
   # 语义与以前整段不写相同(go/db config.go PlacementConfig 的缺省规则)。
   Placement:
     ExtraStoreMaxOpenConn: ${dbExtraStoreMaxOpenConn}
     ExtraStoreMaxIdleConn: ${dbExtraStoreMaxIdleConn}
   ```

   两个变量在 `:3062-3063` 旁用 `Get-AuthoritativeScalar 'go/db/etc/db.yaml' 'Placement.ExtraStoreMaxOpenConn'` / `...IdleConn` 读。同步改 `:3457-3463` 的注释(「Placement 段不写」→「Placement 只写连接池,Redis 不写」)与 data-service 模板 `:3479` 的「所以 go-svc-db ConfigMap 不写 Placement 段」→「不写 Placement.Redis」。

2. **data-service ConfigMap**(`:3467-3550`):`SnapshotMySQL` 块之后加 `PlayerName:` 的 `MaxOpenConn` / `MaxIdleConn`(读 `data_service.yaml` `PlayerName.MaxOpenConn` / `PlayerName.MaxIdleConn`);`RouteStore:`(:3529-3530)在 `Mode: redis` 下加 `MaxOpenConn` / `MaxIdleConn`(读 `RouteStore.MaxOpenConn` / `RouteStore.MaxIdleConn`),注释写明「Mode 仍固定 redis 不镜像;只镜像池大小,切 db_first 时总账已含它」。`PlayerName` 只写两项是安全的:其余键零值由 `Normalize` 补默认(`go/data_service/internal/config/config.go:587-604`)。
   副作用要写进注释:以前 K8s 上 `RouteStore` 只写 `Mode`、池大小取代码默认;镜像之后,服务 yaml 里一个非法的池值(如 `MaxIdleConn > MaxOpenConn`)会让 K8s 上的 data-service **在 redis 模式下也拒启**(`data_service.go:94-100` 不分 Mode 校验 RouteStore;`PlayerName` 非法则名字 store 不装配,`Store.Required: true` 时同样拒启)。
   所以生成器在读这四个值时顺手核一次「正整数且 idle ≤ open」,不满足即 throw(在 apply 之前失败,而不是 CrashLoopBackOff);`Placement.ExtraStore*` 同样核。

3. **gateway ConfigMap**(`:4924-4948`):`datasource:` 下加

   ```
       # 连接池大小镜像 java/gateway_node/src/main/resources/application.yaml(单一真相),计入 MySQL 连接总账。
       hikari:
         maximum-pool-size: ${gatewayHikariMaxPool}
         minimum-idle: ${gatewayHikariMinIdle}
   ```

   两个变量只在 `$SvcName -eq 'gateway'` 时读(`auth` 分支不读,避免无关服务因缺键 throw)。外挂文件 `/app/config/application.yaml` 与 jar 内那份按属性合并(WORKDIR `/app`,`deploy/k8s/Dockerfile.java-svc`;Spring Boot 默认加载 `./config/`,外部事实),即使不镜像 jar 内值也生效;镜像是为了让集群值与总账输入同源、契约测试可钉。

4. friend / trade ConfigMap 已镜像(`:3238-3239`、`:3303-3304`),只把 `:3962`、`:4077` 两处注释补一句「总账见 Get-MysqlConnectionLedger」。

#### 4.1.5 Java gateway Hikari(`application.yaml:10-14`)

```yaml
  datasource:
    url: ...
    driver-class-name: com.mysql.cj.jdbc.Driver
    # 连接池显式配置(2026-10-12 G2b;数值待压测确认)。不写时是 Hikari 默认:maximumPoolSize=10,minimumIdle 缺省
    # 等于 maximumPoolSize —— 每个副本恒开 10 条,K8s 上每 zone 2 副本 = 20 条,计入所有 zone 共用的那台 MySQL 的总账。
    # 用量:服务器列表与公告每次请求查一次(ServerListService / AnnouncementService,不缓存),准入状态按 zone 缓存 1s
    # (AssignGateService),健康探测每 5s 一次 findAll(ZoneHealthProbeService),管理接口偶发;都是单表小查询。
    # 5 条按每次 1–2ms 推算每副本约 2500–5000 QPS(推算,未实测)。minimum-idle 2:空闲时不白占 5 条。
    # K8s ConfigMap(tools/scripts/k8s_deploy.ps1 New-JavaSvcConfigMapYaml)镜像这两个值,契约测试钉住两边一致。
    hikari:
      maximum-pool-size: 5
      minimum-idle: 2
```

用量证据:`ServerListService.java:25-26`(每次 `findAll`)、`AssignGateService.java:70`(TTL 1s)与 `:230`、`ZoneHealthProbeService.java:71`、`application.yaml:91-92`(5000ms)、`AnnouncementService.java:20`。

`connection-timeout` 不改(Hikari 默认 30s;池更小后排队更可能,是否改 3s 见 §10 第 5 条)。

> **2026-10-15 更正**:`connection-timeout` 已按 3000 毫秒落码(未编译、未测试,待 Codex 验证),理由与连带改动见 §9 第 7 条的更正注。jar 内 `application.yaml`、`New-JavaSvcConfigMapYaml` 的镜像(`Get-MysqlPoolMirror` 按下限 250 核对:HikariCP 5.1.0 把 0 当无限等、低于 250 抛异常)、契约测试的镜像用例与负向夹具、`GatewayDataSourcePoolConfigTest` 一起改。§10 第 5 条原为待拍板项,负责人如不接受,改回时这几处要同批改。
>
> **2026-10-15 已定**:负责人按「最标准」定为 3000 毫秒,见 §10 第 5 条的「已定」注;上面「负责人如不接受」一句保留作历史。

#### 4.1.6 过时注释更正

`go/db/etc/db.yaml:68-79`(保留英文历史,删掉两处「151」论断,追加中文更正):

```yaml
    MaxOpenConn: 60 # 2026-05-28 deep-dive §1: 10 was the real bottleneck.
                    # partition=10 + MaxOpenConn=10 meant every worker
                    # competed for exactly one MySQL connection; under
                    # ~90 task/s steady input the pool was always full
                    # and Kafka backlog grew to 91k. 30 gave 3x headroom.
                    # 2026-06-01 Round 14 §P5: 30→60 to match
                    # SubShardCount 4→8 (10 partitions × 8 sub-shards
                    # = 80 effective workers, 60 connections keeps the
                    # pool the limiter).
                    # 2026-10-12 更正:以前这里写的「MySQL 默认 max_connections=151 留得出余量」不成立。
                    # K8s 上所有 zone 的 go/db、data_service、gateway 与全局的 friend、trade 共用同一台 MySQL,
                    # 本池 × 副本数(2)× zone 数是总账里最大的一项。改这个值必须重算总账:
                    # tools/scripts/k8s_deploy.ps1 的 Get-MysqlConnectionLedger(部署时打印 "MySQL connection ledger"),
                    # 契约测试钉住「规划峰值 ≤ deploy/k8s/manifests/infra/mysql.yaml 的 max_connections」。
```

`go/db/internal/config/config.go:250-254`(批 A 只改注释,常量不动):

```go
// 额外落点库连接池默认值(player-storage-placement.md §6.1)。
//
// 每个被按需打开的库各自一个池,库数不设上限,所以单库默认给小。预算按「每个 go/db 实例的额外库数 × 池大小 × 副本数 × zone 数」
// 计入同一台 MySQL 的总账(tools/scripts/k8s_deploy.ps1 Get-MysqlConnectionLedger;mysql.yaml 的 max_connections 按其规划峰值取值)。
// 以前这里写的「MySQL 默认 max_connections=151 下仍留得出余量」不成立(2026-10-12 更正)。
// Phase 2 大部分玩家搬进全局库后,全局库这一个池会成为瓶颈,届时按压测结论显式调大。
```

(批 C 再把最后一句改成两个旋钮的说法。)

`go-svc/db.yaml:31`、`go-svc/data-service.yaml:34` 的预算注释各改为一句「按 zone 数 × 副本数计入共用 MySQL 的总账,公式与规划见 k8s_deploy.ps1 Get-MysqlConnectionLedger」,保留每实例数字。

### 4.2 批 B(P1)

#### 4.2.1 `go/shared/sqlpoolstats`(新包)

```go
// Package sqlpoolstats 把 database/sql 连接池的 DBStats 按「池类别」聚合后导出给 Prometheus。
//
// 为什么按类别聚合而不是一池一条序列:go/db 的额外落点库按需打开、库数不设上限(player-storage-placement.md §6.1),
// 按库名打 label,序列数会随库数增长;总账关心的本来就是「这一类一共能开多少、开了多少」。
// 为什么不用 client_golang 自带的 collectors.NewDBStatsCollector:它一池一个 Collector、label 是 db_name,
// 同一 db_name 注册两次直接报错,也做不了按类别求和。
package sqlpoolstats

import (
	"database/sql"
	"fmt"
	"math"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/zeromicro/go-zero/core/logx"
)
// 不 import "time":WaitDuration 是 time.Duration,取 .Seconds() 不需要导入 time 包,多导就是编译错误。
// go/shared/go.mod 已直接 require client_golang v1.21.1 与 go-zero v1.9.2,不需要改 go.mod / go.sum。

// OtherPool 收容类别不在声明集合里的样本(调用方笔误),不让它们扩张 label 基数。
const OtherPool = "other"

// Sample 是一次抓取时一个连接池的快照。
type Sample struct {
	Pool  string      // 池类别:低基数封闭集合,绝不放库名 / player_id(AGENTS §9)
	Stats sql.DBStats
}

// Source 在每次抓取时被调用一次,返回此刻全部连接池的快照。
// 契约:并发安全;不做网络 I/O、不阻塞(sql.DB.Stats 只取一把内存锁);可以返回空。
type Source func() []Sample

// NewCollector 构造收集器。service 进常量 label sql_client;pools 是声明的池类别,每次抓取都为每个类别输出一组序列
// (没有池时全 0、pools 为 0)。参数非法(service 或某个 pool 为空、pool 重复、pool 等于 OtherPool)直接 panic:
// 这是启动期的编程错误,与 prometheus.MustRegister 同语义。
func NewCollector(service string, pools []string, source Source) prometheus.Collector
```

导出的指标(namespace `mmorpg`,subsystem `sql_pool`;变量 label `pool`;常量 label `sql_client`):

| 指标 | 类型 | 取值(类别内求和) |
|---|---|---|
| `mmorpg_sql_pool_pools` | gauge | 池个数 |
| `mmorpg_sql_pool_max_open_connections` | gauge | `MaxOpenConnections` 之和;任一池为 0(不限)时为 `+Inf` |
| `mmorpg_sql_pool_open_connections` | gauge | `OpenConnections` |
| `mmorpg_sql_pool_in_use_connections` | gauge | `InUse` |
| `mmorpg_sql_pool_idle_connections` | gauge | `Idle` |
| `mmorpg_sql_pool_wait_count_total` | counter | `WaitCount` |
| `mmorpg_sql_pool_wait_duration_seconds_total` | counter | `WaitDuration` 秒 |
| `mmorpg_sql_pool_max_idle_closed_total` | counter | `MaxIdleClosed` |
| `mmorpg_sql_pool_max_idle_time_closed_total` | counter | `MaxIdleTimeClosed`(批 C 的效果靠它看) |
| `mmorpg_sql_pool_max_lifetime_closed_total` | counter | `MaxLifetimeClosed` |

常量 label 叫 `sql_client` 而不是 `service`:kube-prometheus 的 ServiceMonitor 会加 `service` 目标 label,同名时抓到的 label 被改名为 `exported_service`,告警表达式会静默落空(仓库里 `shared/assetop/metrics.go:43` 用了 `service`,同样有这个隐患,本次不动它)。

实现要点:
- `Describe` 输出 10 个 Desc;`Collect` 先为声明的每个类别建零值累加器,再把样本按类别累加(未知类别并入 `other`),最后逐类别 `prometheus.MustNewConstMetric`。
- counter 是类别内求和:只要池不被移除就单调。go/db 注册表的池常驻不删(`store_registry.go:29`、`:273-276`),data_service 的 store 只在退出时关闭,满足前提;注释写明「移除池会让 counter 回退,被当成 reset」。
- **source panic 时本次抓取本收集器不输出任何序列**(`defer recover` + `logx.Errorf`),不能输出 0:counter 先归 0 再回到真值会被 `increase()` 当成一次暴涨,误触发等待告警。go/db 是存盘主路径,观测代码不能把它带崩(promhttp 的 Gather 在 goroutine 里调 Collect,未恢复的 panic 会让进程退出 —— 依据 client_golang 实现,凭记忆,待核实)。
- `source == nil` 视为没有池。
- `Collect` 可能被并发调用(两路抓取同时到),累加器一律是 `Collect` 内的局部变量,收集器自身构造后只读,不加锁也不共享可变状态。
- `pool="other"` 只在确有未声明类别的样本时输出,不预置 0(它本来就不该出现,预置会让面板上多一组看着像在用的序列)。
- 各调用方构造 `sqlpoolstats.Sample` 一律写键名(`sqlpoolstats.Sample{Pool: p, Stats: st}`):`go vet` 的 composites 检查对**导入包**的结构体字面量不带键名直接报告,§7 的通过标准是 vet 无告警。

#### 4.2.2 go/db 接线

`go/db/internal/metrics/metrics.go`:

```go
// 连接池类别(mmorpg_sql_pool_*{sql_client="db",pool=...});封闭集合,不含库名(落点库数不设上限)。
const (
	PoolHome         = "home"           // 本 zone 库(InitDB 打开,Database.MaxOpenConn)
	PoolExtraZone    = "extra_zone"     // 额外 zone 家族库 zone_{1..999999}_db(本 zone 库除外)
	PoolExtraNonZone = "extra_non_zone" // 非 zone 落点库 player_store_{>=1000000}_db
)

// NewSQLPoolCollector 只构造不注册(单测用自己的注册表)。
func NewSQLPoolCollector(source sqlpoolstats.Source) prometheus.Collector {
	return sqlpoolstats.NewCollector(subsystem, []string{PoolHome, PoolExtraZone, PoolExtraNonZone}, source)
}

// RegisterSQLPools 把连接池快照接到默认注册表(Start 暴露的那个)。进程内调一次;注册失败只记日志:
// 指标是观测面,不能挡住落库(与 Start 同口径)。
func RegisterSQLPools(source sqlpoolstats.Source) {
	register()
	if err := prometheus.Register(NewSQLPoolCollector(source)); err != nil {
		logx.Errorf("[metrics] register sql pool collector failed (pool metrics unavailable): %v", err)
	}
}
```

`store_registry.go`:`StoreRegistry` 加字段 `home uint32`(`NewStoreRegistry` 里赋 `opts.HomeStorageID`),新增

```go
// PoolSamples 返回此刻每个已打开库的连接池快照(shared/sqlpoolstats.Source)。
// 类别只有三种(metrics.Pool*):本 zone 库、额外 zone 家族库、非 zone 落点库。读锁内只拷出 *sql.DB,
// Stats() 在锁外取(它自己加 db.mu),不挡按需打开。没有连接的库(单测的 storeNamed)跳过。
func (r *StoreRegistry) PoolSamples() []sqlpoolstats.Sample
```

分类:`id == r.home` → `PoolHome`;`placement.IsZoneStorage(id)` → `PoolExtraZone`;否则 `PoolExtraNonZone`。

import 增量(go/db 的 go.mod 已 `replace shared => ../shared`,client_golang 已在依赖里,不改 go.mod):
- `go/db/internal/metrics/metrics.go` 加 `"shared/sqlpoolstats"`(prometheus、logx 已有);
- `store_registry.go` 加 `"shared/sqlpoolstats"`(`metrics`、`placement`、`database/sql` 已有)。依赖方向 proto_sql → metrics → sqlpoolstats,不成环。
- 单测里 home 与按需打开的库用 `dbtest.Open(t, nil)` 拿句柄即可:`Stats()` 不下发语句,nil handler 不会被调用。

`go/db/db.go`:`:107-110` 建好注册表之后、`:133` `metrics.Start` 之前加 `metrics.RegisterSQLPools(stores.PoolSamples)`(方法值的类型 `func() []sqlpoolstats.Sample` 可直接赋给命名类型 `sqlpoolstats.Source`,db.go 不需要新 import)。

K8s:db ConfigMap(`k8s_deploy.ps1:3423-3465`)加顶层 `MetricsListenAddr: ":9160"`(注释:与 manifest 的 metrics 端口 / prometheus.io/port 注解一致;9160 与本地 `db.yaml:148` 同值);`go-svc/db.yaml` 照 `data-service.yaml:51-54`、`:82-84` 加 `prometheus.io/scrape|port|path` 注解与 `containerPort: 9160 name: metrics`。这同时让 `owner-epoch-alerts.yaml:9-11` 的抓取前提在 K8s 上成立。

#### 4.2.3 data_service 接线

`go/data_service/internal/store/dbstats.go`(新):

```go
// DBStatsOf 返回 v 背后那个 MySQL store 的独立连接池统计(给 /metrics,不做 I/O)。
// v 不是本包的五种 store 之一、或是 nil 指针时 ok=false。
//
// 用一个类型分支而不是给每个 store 加方法、再在 svc 的接口上扩一项:svc 的 store 接口是 logic 的依赖面,
// 单测的假实现不该为了观测去实现它;五个 store 是封闭集合,新增 store 时在这里加一行(dbstats_test 的表会提醒)。
func DBStatsOf(v any) (sql.DBStats, bool) {
	switch s := v.(type) {
	case *SnapshotStore:
		if s != nil && s.db != nil {
			return s.db.Stats(), true
		}
	case *TransactionLogStore: // 同上
	case *IdSegmentStore:      // 同上
	case *PlayerNameStore:     // 同上
	case *RouteStore:          // 同上
	}
	return sql.DBStats{}, false
}
```

(五个字段名均为 `db`:`snapshot_store.go:154`、`transaction_log_store.go:69`、`id_segment_store.go:85`、`player_name_store.go:222`、`route_store.go:269`。)

`svc/servicecontext.go`:

```go
// SQLPoolSamples 是 /metrics 的连接池快照来源(shared/sqlpoolstats.Source):只报装配成功的 store,一个 store 一个池,
// 类别即 store 名(metrics.Pool*)。字段在 NewServiceContext 之后不再写,抓取时并发读是安全的。
func (s *ServiceContext) SQLPoolSamples() []sqlpoolstats.Sample {
	if s == nil {
		return nil
	}
	return sqlPoolSamples([]namedStore{
		{metrics.PoolSnapshot, s.SnapshotStore},
		{metrics.PoolTransactionLog, s.TxLogStore},
		{metrics.PoolIdSegment, s.IdSegmentStore},
		{metrics.PoolPlayerName, s.PlayerNameStore},
		{metrics.PoolPlayerRoute, s.PlayerRouteStore},
	}, store.DBStatsOf)
}

type namedStore struct {
	pool  string
	store any
}

// sqlPoolSamples 是 SQLPoolSamples 的纯逻辑部分;statsOf 显式传入,单测用假实现(真 store 的 db 字段在包外造不出来)。
func sqlPoolSamples(stores []namedStore, statsOf func(any) (sql.DBStats, bool)) []sqlpoolstats.Sample
```

接口字段为 nil 时转成 `any` 也是 nil,`DBStatsOf` 返回 false;假实现不命中类型分支,同样跳过。

import 增量:`servicecontext.go` 加 `"database/sql"`(`sqlPoolSamples` 的签名用 `sql.DBStats`)、`"shared/sqlpoolstats"`、`"data_service/internal/metrics"`(Pool* 常量)。
`metrics` 不 import `svc` / `store`,不成环;`internal/metrics/metrics.go` 另加 `"shared/sqlpoolstats"`。data_service 的 go.mod 已 `replace shared => ../shared`。
`sqlPoolSamples` 里构造样本写键名(见 §4.2.1);`namedStore` 是本包类型,位置式字面量不触发 vet。

`internal/metrics/metrics.go`:常量 `PoolSnapshot="snapshot"`、`PoolTransactionLog="transaction_log"`、`PoolIdSegment="id_segment"`、`PoolPlayerName="player_name"`、`PoolPlayerRoute="player_route"`;`NewSQLPoolCollector(source, includeRoute bool)`(`includeRoute` 为 false 时不声明 `player_route`,理由同 `PrimeRoute` 的注释 `metrics.go:328-331`:redis 模式下不要一组恒 0、看着像在跑的序列);`RegisterSQLPools(source, includeRoute)` 同 go/db。

`data_service.go:140` 之后加 `metrics.RegisterSQLPools(svcCtx.SQLPoolSamples, c.RouteStore.DBFirst())`。**不能放进 `NewServiceContext`**:单测会多次构造 ServiceContext,默认注册表重复注册会报错。

#### 4.2.4 告警文件 `deploy/k8s/mysql-connection-alerts.yaml`(新)

格式照 `owner-epoch-alerts.yaml:1-31`:文件头写来源、安装方式(`kubectl apply -n observability -f ...`)、抓取前提、严重级约定。

```yaml
apiVersion: monitoring.coreos.com/v1
kind: PrometheusRule
metadata:
  name: mysql-connection-alerts
  labels:
    app: mysql
    prometheus: kube-prometheus
    role: alert-rules
spec:
  groups:
    # 需要 mysqld_exporter(仓库里还没有,部署方式待拍板)。没有它时这一组没有数据、不会触发,也不会误报;
    # 刻意不写 absent(mysql_up):exporter 落地前它会恒响。
    - name: mysql.connection-budget
      interval: 30s
      rules:
        - alert: MysqlConnectionsRefused            # critical
          expr: |
            sum(increase(mysql_global_status_connection_errors_total{error="max_connections"}[5m])) > 0
          for: 0m
        - alert: MysqlConnectionsNearMax            # warning
          expr: |
            max_over_time(mysql_global_status_threads_connected[5m]) / mysql_global_variables_max_connections > 0.8
          for: 5m
    # go/db(:9160)与 data-service(:9260)的 /metrics,现在就有数据。friend / trade / Java gateway 没导出池指标,不在覆盖范围内。
    - name: sql-pool.client
      interval: 30s
      rules:
        - alert: SqlPoolSlowAcquire                 # warning:平均每次排队取连接 > 100ms 且排队频繁
          expr: |
            (
              sum by (namespace, sql_client, pool) (rate(mmorpg_sql_pool_wait_duration_seconds_total[5m]))
                /
              sum by (namespace, sql_client, pool) (rate(mmorpg_sql_pool_wait_count_total[5m]))
            ) > 0.1
            and
            sum by (namespace, sql_client, pool) (rate(mmorpg_sql_pool_wait_count_total[5m])) > 1
          for: 10m
        - alert: SqlPoolSaturated                   # info:go/db home 池按设计就是限流点(db.yaml:28-35),只作趋势
          expr: |
            sum by (namespace, sql_client, pool) (mmorpg_sql_pool_in_use_connections)
              / sum by (namespace, sql_client, pool) (mmorpg_sql_pool_max_open_connections) > 0.9
          for: 15m
        - alert: DbExtraStorePoolsBeyondPlan        # warning:额外库数超过总账规划档,max_connections 可能已不够
          expr: |
            max by (namespace, instance) (mmorpg_sql_pool_pools{sql_client="db", pool="extra_zone"}) > 3
            or
            max by (namespace, instance) (mmorpg_sql_pool_pools{sql_client="db", pool="extra_non_zone"}) > 0
          for: 10m
```

每条都带 `labels.severity` 与中文 `summary` / `description`。`MysqlConnectionsRefused` 的描述写清 1040 → 重试预算 → 死信链(`PROGRESS.md:7181`,读代码推断)与处置入口;`DbExtraStorePoolsBeyondPlan` 的两个阈值(3、0)必须与 `Get-MysqlConnectionPlan` 的 `ExtraZoneStores` / `NonZoneStores` 相同,**每个阈值单独占一行**以便契约测试正则提取。所有阈值都标「待压测确认」。mysqld_exporter 指标名(`mysql_global_status_connection_errors_total{error="max_connections"}`、`mysql_global_status_threads_connected`、`mysql_global_variables_max_connections`)凭记忆,部署 exporter 时核对。

### 4.3 批 C(P2)

#### 4.3.1 配置键(`go/db/internal/config/config.go`)

| 键 | 类型 | 默认 | 语义 |
|---|---|---|---|
| `Placement.ZoneStoreMaxOpenConn` / `ZoneStoreMaxIdleConn` | int | 回落旧键,再回落 8 / 2 | **每个**额外 zone 家族库的池 |
| `Placement.NonZoneStoreMaxOpenConn` / `NonZoneStoreMaxIdleConn` | int | 同上 | **每个**非 zone 落点库的池 |
| `Placement.ExtraStoreConnMaxIdleSeconds` | int | 60 | 额外落点库池 `SetConnMaxIdleTime`;≤0 视为没写 |
| `Placement.ExtraStoreMaxOpenConn` / `ExtraStoreMaxIdleConn` | int | 8 / 2 | **保留**,注释标「旧键,只作新键缺省时的回落,新配置不要再写」 |

命名用 NonZone 而不是 Global:与 `shared/placement` 的 `FirstNonZoneStorageID`(`placement.go:52-60`)一致,也避开与 data_service 的「全局库」(SnapshotMySQL)混淆(launch_verdict 第 3 行指出过这处概念混用)。

`Normalize`(替换 `:291-297`):

```go
	p := &c.Placement
	// 旧键先补默认:它是新键缺省时的回落值,也让「只写旧键」的旧部署两类库维持原来的大小。
	if p.ExtraStoreMaxOpenConn <= 0 {
		p.ExtraStoreMaxOpenConn = DefaultPlacementExtraStoreMaxOpenConn
	}
	if p.ExtraStoreMaxIdleConn <= 0 {
		p.ExtraStoreMaxIdleConn = DefaultPlacementExtraStoreMaxIdleConn
	}
	if p.ZoneStoreMaxOpenConn <= 0 {
		p.ZoneStoreMaxOpenConn = p.ExtraStoreMaxOpenConn
	}
	// ZoneStoreMaxIdleConn / NonZoneStoreMaxOpenConn / NonZoneStoreMaxIdleConn 同理
	if p.ExtraStoreConnMaxIdleSeconds <= 0 {
		p.ExtraStoreConnMaxIdleSeconds = DefaultPlacementExtraStoreConnMaxIdleSeconds // 60
	}
```

保持幂等(`Normalize` 注释「可重复调用」,`config.go:264`)。优先级:新键 > 旧键 > 默认。不新增拒启校验(idle > open 由 database/sql 静默压到 open,与今天相同;不为此引入新的启动失败)。

默认值注释(`:250-258`)改为两个旋钮的说法,并说明 60s:冷库(偶发访客读)的池因此回到 0 条,不再按 MaxIdleConn 常驻;`ConnMaxLifetime` 仍是 5 分钟,本值大于 300 等于不生效(空闲连接本来就会在 5 分钟寿命到期时被回收,本值只是让冷池更早归零)。

#### 4.3.2 `store_registry.go`

```go
// StorePoolOptions 是一个额外落点库连接池的参数(本 zone 库的池由 InitDB 按 Database.MaxOpenConn 设,不走这里)。
type StorePoolOptions struct {
	MaxOpenConn     int
	MaxIdleConn     int
	ConnMaxIdleTime time.Duration
}

// extraStorePools 是两类额外落点库各自的池参数。
type extraStorePools struct {
	Zone    StorePoolOptions
	NonZone StorePoolOptions
}

// storePoolFor 按库名所属家族选池参数。库名由 placement.StoreDBName 派生(openOnce),只可能属于两个家族之一;
// 解析不出来是编程错误,fail-closed 报错(经 openOnce 包成 ErrStoreUnavailable,写延后、不耗重试),不猜一个池。
func (p extraStorePools) storePoolFor(dbName string) (StorePoolOptions, error)

// applyStorePool 把池参数设到 handle 上;ConnMaxLifetime 与本 zone 库同为 5 分钟(proto_sql/db.go:264)。
func applyStorePool(handle *sql.DB, pool StorePoolOptions)
```

- `OpenPlacementStore(ctx, dbName string, pool StorePoolOptions)` 取代 `(ctx, dbName, maxOpenConn, maxIdleConn int)`(`:299`,唯一调用方是 `:177-179` 的闭包,已 grep 确认);`:314-316` 改为 `applyStorePool(handle, pool)`。
- `NewStoreRegistryFromConfig`(`:164-187`)从 `config.AppConfig.Placement` 组出 `extraStorePools`(idle 时间 = `time.Duration(pc.ExtraStoreConnMaxIdleSeconds) * time.Second`),Open 闭包先 `storePoolFor` 再 `OpenPlacementStore`;启动日志 `:184-185` 改为打印 `zoneStorePool=%d/%d nonZoneStorePool=%d/%d extraStoreIdleTimeout=%v`。
- `db.go:229-232` 横幅同步(全仓 grep `extraPool` 没有其它消费者)。

#### 4.3.3 `db.yaml` 与 ConfigMap

`go/db/etc/db.yaml:122-125` 换成:

```yaml
  # 额外落点库(本 zone 库之外)的连接池,**每个库一个池**,按库所属家族分两个旋钮(本 zone 库仍用上面的 Database.MaxOpenConn):
  #   ZoneStore*    —— zone 家族库 zone_{1..999999}_db:冷缓存访客读、合服 pin 后的源区库。最坏情形每个实例会打开其余全部
  #                    zone 的库,预算随 zone 数平方增长(总账见 tools/scripts/k8s_deploy.ps1 Get-MysqlConnectionLedger)。
  #   NonZoneStore* —— 非 zone 落点库 player_store_{>=1000000}_db(Phase 2)。大部分玩家搬进来后它是瓶颈,按压测单独调大,
  #                    不连带放大访客池;调大前先重做总账规划档。
  # 旧键 ExtraStoreMaxOpenConn / ExtraStoreMaxIdleConn 仍认:新键没写时两类库都回落到它,新配置不要再写。
  ZoneStoreMaxOpenConn: 8
  ZoneStoreMaxIdleConn: 2
  NonZoneStoreMaxOpenConn: 8
  NonZoneStoreMaxIdleConn: 2
  # 额外落点库池里空闲超过这么多秒的连接即关闭(SetConnMaxIdleTime):冷库的池回到 0 条,不按 MaxIdleConn 常驻。
  # ConnMaxLifetime 仍是 5 分钟,本值大于 300 等于不生效。
  ExtraStoreConnMaxIdleSeconds: 60
```

db ConfigMap 的 `Placement` 块从批 A 的两个旧键换成这五个新键(都从 `go/db/etc/db.yaml` 镜像);总账输入改读 `Placement.ZoneStoreMaxOpenConn` / `Placement.NonZoneStoreMaxOpenConn`。go-zero 读配置对未知键静默忽略(凭记忆,待核实),所以「新 ConfigMap + 旧镜像」的滚动窗口里旧二进制取默认 8,行为不变。

---

## 5. 错误语义

| 位置 | 情形 | 行为 |
|---|---|---|
| `Get-MysqlConnectionLedgerInputs` | 任一权威文件缺键 / 不存在;`max_connections` 或 `replicas` 不是恰好一处 | throw(fail-closed),部署写路径中止,与 ConfigMap 生成器同一纪律 |
| `Get-MysqlConnectionLedger` | 输入非正整数、ZoneCount ≤ 0、其余(含 RolloutExtraPodsPerDeployment)负数 | throw,点名键与值 |
| ConfigMap 生成器(批 A 新镜像的池参数) | 非正整数或 `MaxIdleConn > MaxOpenConn` | throw,在任何 apply 之前(否则 data-service 在集群里拒启 / CrashLoop) |
| `Write-MysqlConnectionLedger` | zones 配置文件缺席 | Z 取 1,行尾标 `zones_config=missing`,不调 `Get-InfraZoneIds`(避免 mysql-init-sql 那条误导性 WARNING) |
| `Write-MysqlConnectionLedger` | 峰值 > max_connections | **只打 WARN**(`MySQL连接总账超限:` 开头),部署继续 |
| `sqlpoolstats.NewCollector` | service / pool 为空、pool 重复、pool = `other` | panic(启动期编程错误) |
| Collector `Collect` | source panic | recover,本次抓取本收集器不输出,`logx.Errorf`;其它指标照常 |
| Collector `Collect` | 未声明类别的样本 | 并入 `pool="other"` |
| `RegisterSQLPools`(两服务) | 注册失败(如重复) | 只记 ERROR,服务照常(观测面不挡落库) |
| `PoolSamples` / `SQLPoolSamples` / `DBStatsOf` | 库没有连接、store 为 nil 或假实现 | 跳过,不报错 |
| go/db `storePoolFor` | 库名不属于任何家族 | 返回错误 → `openOnce` 包成 `ErrStoreUnavailable`:写延后、不耗重试(`store_registry.go:33-36`) |
| go/db 新配置键 | ≤0 / 缺省 | 回落旧键 → 默认,不拒启 |
| Java gateway | 池耗尽 | Hikari 等 `connectionTimeout`(默认 30s)后抛 `SQLTransientConnectionException` → 5xx(既有语义,池变小后更容易出现) |
| MySQL | 连接数达上限 | 1040;go/db 链路行为**不变**(L-04 后续) |

---

## 6. 测试清单

### 6.1 PowerShell 契约测试(`tools/scripts/tests/k8s_deploy_contract.tests.ps1`,不需要集群;需要 pwsh 7)

AST 抽取名单:`Get-AuthoritativeScalar`、`Get-MysqlConnectionPlan`、`Get-MysqlMaxConnectionsFromManifest`、`Get-ManifestDeploymentReplicas`、`Get-MysqlConnectionLedgerInputs`、`Get-MysqlConnectionLedger`(缺任何一个即 throw,同 `:333-337`);各用例自己设 `$RepoRoot = Get-RepoRoot`。

| 批 | 用例名 | 断言要点 |
|---|---|---|
| A | `MySQL 连接总账:规划档峰值 ≤ mysql.yaml 的 max_connections` | 规划档 `Peak -le MaxConnections`,失败信息打印整份分项 |
| A | `MySQL 连接总账:deploy/k8s/zones.json 的 zone 数(E_z=Z−1)峰值 ≤ max_connections` | zone 数取 zones.json 的 `zones.Count`(`ConvertFrom-Json` 直接读,不需要抽 `Get-ZonesFromJson`);E_n / F / R / ops 取规划档 |
| A | `自检:总账公式复现 2026-10-12 核算(Z=4 → 920、Z=2 → 500),对旧 max_connections=500 判超限,输入非法即 throw` | 合成输入(gw 10、E=F=0、ops 0、`-RolloutExtraPodsPerDeployment 0`);规划档对 MaxConnections=500 的合成输入 `Peak -gt 500`;再钉一条滚动口径:同一合成输入下 R=2 与 R=0 的 `Peak` 之差 == 2 ×(db + ds + gw + friend + trade 每实例之和);缺键 / 0 / 非数字三种都 throw 且文本含键名,R 传 -1 也 throw |
| A | `ConfigMap 镜像 MySQL 池参数 == 服务 yaml(db Placement、data-service PlayerName / RouteStore、gateway Hikari),login 不写 PasswordAuth` | 键对表逐项 `Assert-Equal`;`data.application.yaml.spring.datasource.hikari.maximum-pool-size` / `minimum-idle`;`-not` 含 `data.login.yaml.PasswordAuth.Enabled` |
| A | `连 MySQL 的 ConfigMap 只有总账认识的那些(zone-up 三个、infra-up 两个)` | 扫 devOut 的 ConfigMap 块,非注释行匹配 `mysql\.[^\s:/]+:3306|jdbc:mysql://` 的名字集合 == `go-svc-db-config, go-svc-data-service-config, java-svc-gateway-config`;同一用例里再起一次 infra-up DryRun(沿用 `$BattleInfraArgs`,去掉 `-SkipGoSvc` 与 `-WaitReady`(免得 DryRun 里去等 migrate Job)、加 `-GoSvcRegistry registry.invalid/test`,环境 `$BattleTestEnv`;子进程先断 `ExitCode -eq 0`),集合 == `go-svc-friend-config, go-svc-trade-config`。只扫 zone-up 时,新增一个连 MySQL 的**全局**服务能绕过总账而无人察觉 |
| A | `部署写路径打印 MySQL connection ledger(zone-up DryRun)` | devOut 含 `MySQL connection ledger: zones=2 `;`plan zones=4 peak=<用 AST 函数算出的值>`(数字同源,不写死) |
| A | `负向:zones.10zones.yaml 时打 MySQL连接总账超限 警告且不阻断` | 新起一次 DryRun:基线参数 + `-ZonesConfigPath (Join-Path (Get-RepoRoot) 'deploy/k8s/zones.10zones.yaml') -SkipGoSvc -SkipJavaSvc`(绝对路径,照 `:1252` 的 all-up 用例;子进程工作目录不保证是仓库根);`ExitCode -eq 0`,输出含 `MySQL连接总账超限`;Z = 10 个 zone ∪ `-ZoneId 101` = 11 |
| B | `db 指标端口:ConfigMap MetricsListenAddr ":9160" 与 manifest 9160 metrics 端口 + prometheus 注解一致` | 照 `:172-176` 的 data-service 断言 |
| B | `mysql-connection-alerts.yaml:PrometheusRule 形状,额外库数阈值与 Get-MysqlConnectionPlan 同源` | 含 `kind: PrometheusRule`、`name: mysql-connection-alerts`;`- alert:` 恰好 5 条,每条有 `severity:`;正则提取 `pool="extra_zone"}) > (\d+)` == `ExtraZoneStores`、`pool="extra_non_zone"}) > (\d+)` == `NonZoneStores` |
| C | (改)上面「ConfigMap 镜像」一例的 db 键对换成五个新键 | 总数不变 |

计数:88 → 95(批 A)→ 97(批 B)→ 97(批 C)。评审新增的断言都并进既有用例,不改计数。

批 A 还要求 CI 真的会跑这些用例:A13 把 `java/gateway_node/src/main/resources/application.yaml`、`deploy/k8s/*.yaml`、`deploy/k8s/*.json` 加进 `deploy-config-tests.yml` 的触发路径;否则只改 gateway 池大小、zones.json 或告警阈值的 PR 不会触发契约测试,总账与现实可以静默分家。

### 6.2 Go(全部无外部依赖,不带 `-tags integration`)

| 批 | 包 | 用例 |
|---|---|---|
| B | `shared/sqlpoolstats` | `TestCollectorSumsSamplesPerPool`(两池同类:max 8+8、open、in_use、wait_count、wait 1.5s → 秒、pools=2) |
| B | 同上 | `TestCollectorPrimesDeclaredPoolsWithZeros` |
| B | 同上 | `TestCollectorFoldsUndeclaredPoolIntoOther` |
| B | 同上 | `TestCollectorReportsUnlimitedPoolAsInf` |
| B | 同上 | `TestCollectorSkipsScrapeWhenSourcePanics`(Gather 无错、无 `mmorpg_sql_pool_*`) |
| B | 同上 | `TestCollectorIsPedanticallyConsistent`(`prometheus.NewPedanticRegistry` 注册 + Gather 无错,`sql_client` 常量 label 存在) |
| B | 同上 | `TestNewCollectorRejectsInvalidPools`(表驱动:空 service、空 pool、重复、`other` → panic) |
| B | `db/internal/metrics` | `TestSQLPoolCollectorDeclaresDbPools`(nil source,三类各一组、`sql_client="db"`) |
| B | `db/internal/logic/pkg/proto_sql` | `TestStoreRegistryPoolSamplesClassifiesByFamily`(home=zone_1 用 `dbtest.Open` 句柄 `SetMaxOpenConns(60)`;按需打开 zone_2_db、player_store_1000000_db 各带句柄;断言三类与 `Stats().MaxOpenConnections`;无连接的库跳过) |
| B | `data_service/internal/store` | `TestDBStatsOfReportsEachStorePool`(表驱动五种 store:`sql.Open("mysql", "u:p@tcp(127.0.0.1:1)/x")` 不拨号,各 `SetMaxOpenConns` 不同值;nil 指针、未知类型 → false) |
| B | `data_service/internal/svc` | `TestSQLPoolSamplesSkipsUnassembledStores`、`TestSQLPoolSamplesNamesPoolsByStore`(走 `sqlPoolSamples` 纯函数 + 假 `statsOf`) |
| B | `data_service/internal/metrics` | `TestSQLPoolCollectorDeclaresRouteOnlyUnderDBFirst`(includeRoute false → 4 类,true → 5 类) |
| C | `db/internal/config` | `TestPlacementSplitPoolsDefault`(整段缺省 → 两类 8/2、idle 60)、`TestPlacementSplitPoolsFallBackToLegacyKeys`(只写旧键 16/4 → 两类 16/4)、`TestPlacementSplitPoolsNewKeysWin`(旧 16、NonZone 32 → zone 16、non-zone 32) |
| C | 同上 | 改 `TestEtcYamlPlacementBlockMatchesDefaults`:Normalize **之前**断言 yaml 写出了四个新键与 idle 秒数,之后断言等于默认 |
| C | `db/internal/logic/pkg/proto_sql` | `TestStorePoolForPicksPoolByFamily`、`TestStorePoolForRejectsNonPlacementName`、`TestApplyStorePoolSetsMaxOpen`(`dbtest.Open` 句柄,`Stats().MaxOpenConnections`) |

既有用例 `TestPlacementBlockMissingMeansDefaults`、`TestPlacementExplicitValues`(`config_test.go:17-80`)读旧键,`Normalize` 仍回填旧键,保持绿。

### 6.3 Java

| 批 | 用例 | 依赖 |
|---|---|---|
| A | `GatewayDataSourcePoolConfigTest.mainApplicationYamlPinsHikariPool`:`@DataJpaTest(properties = "spring.datasource.url=jdbc:h2:mem:pool_config;MODE=MySQL;DB_CLOSE_DELAY=-1")` + `@AutoConfigureTestDatabase(replace = NONE)` + `@ActiveProfiles("test")`(照 `ZoneConfigRepositoryWiringTest.java:36-39`);注入 `DataSource`,`assertInstanceOf(HikariDataSource.class, ...)`,`getMaximumPoolSize()==5`、`getMinimumIdle()==2` | 无(H2) |

---

## 7. 交给 Codex 的验证

通用:工作目录为仓库根 `D:\luyuan\xuanming-server-mmo-cpp`;Go 1.26.x、pwsh 7、JDK 23 + Maven;不需要集群、Redis、Kafka、MySQL(可选 kind 冒烟除外)。失败时保留:失败用例全文输出、`go test -v` 的失败段、`mvn` 的 surefire 报告。任何一步失败不要连续重试,先回报。

### 批 A

1. `pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1` → **95 通过、0 失败**。
2. `pwsh -NoProfile -File tools/scripts/tests/k8s_migrate_gate.tests.ps1` → 全部通过(同样按 AST 抽 k8s_deploy.ps1 的函数,确认新函数不破坏抽取)。
3. `cd java/gateway_node && mvn -q -Dtest=GatewayDataSourcePoolConfigTest test` → 通过;再 `mvn -q test` → 与改动前基线相比没有新增失败(依赖真 MySQL 的 `*MySqlTest` 照旧行为)。
4. `cd go/db && gofmt -l ./internal/config && go build ./... && go test -count=1 ./internal/config/...` → gofmt 无输出、构建与测试通过(本批 Go 只改注释,确认没碰坏)。
5. 可选(需要 kind,**会重启 MySQL**,见风险 §9 第 1 条):`infra-up` 后进 mysql 容器执行 `SHOW VARIABLES LIKE 'max_connections'; SHOW VARIABLES LIKE 'open_files_limit';`,期望 1800(若被 open_files_limit 压低,MySQL 启动日志会有 "Changed limits: max_connections" 警告);`kubectl describe pod` 确认 limits 5Gi。口令按 runbook 注入,不要写进日志。
6. 静态核对 A13:`.github/workflows/deploy-config-tests.yml` 的 push 与 pull_request 两份 `paths` 都含新增三条,YAML 能被解析(本机有 `actionlint` 就跑一次,没有就如实记「未校验」)。

### 批 B

1. `cd go/shared && gofmt -l ./sqlpoolstats && go vet ./sqlpoolstats/... && go test -count=1 ./sqlpoolstats/...`
2. `cd go/db && gofmt -l . && go build ./... && go vet ./... && go test -count=1 ./internal/metrics/... ./internal/logic/pkg/proto_sql/... ./internal/config/... ./internal/kafka/...`
3. `cd go/data_service && gofmt -l . && go build ./... && go vet ./... && go test -count=1 ./internal/store/ ./internal/svc/... ./internal/metrics/...`(store 包只跑默认 tag,集成测试不在本批)
4. 契约测试 → **97 通过**。
5. `promtool check rules`:先把 `deploy/k8s/mysql-connection-alerts.yaml` 的 `spec` 段取成单独文件(`groups:` 顶层)再检查 → 5 条无语法错误;本机没有 promtool 就如实记录「未校验」。
6. 可选本地冒烟:按 `go_services.ps1` 起 db 与 data_service,`curl -s 127.0.0.1:9160/metrics | Select-String mmorpg_sql_pool_` 应有 `sql_client="db"` 的 home / extra_zone / extra_non_zone 三组,`home` 的 max_open 为 60;`:9260` 应有 4 组(redis 模式无 `player_route`)。

通过标准:gofmt 无输出,vet 无告警,所有列出的测试 PASS(不是 SKIP)。

### 批 C

1. `cd go/db && gofmt -l . && go build ./... && go vet ./... && go test -count=1 ./internal/config/... ./internal/logic/pkg/proto_sql/... ./internal/kafka/...`
2. 契约测试 → 97 通过。
3. 可选:本地 db 启动日志 `[placement] store registry ready` 一行出现 `zoneStorePool=8/2 nonZoneStorePool=8/2 extraStoreIdleTimeout=1m0s`。

---

## 8. 与现有设计的偏离与代价

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| 1 | 池指标用共享名 `mmorpg_sql_pool_*` + 常量 label `sql_client`,不用各服务前缀 `db_` / `data_service_` | 一条告警表达式覆盖两个服务;`sql_client` 避开 ServiceMonitor 的 `service` 目标 label | 与两服务既有指标命名风格不同 |
| 2 | 不用 `collectors.NewDBStatsCollector` | 需要按类别聚合不定数量的池 | 多维护一个小 Collector(有 Pedantic 注册表测试兜底) |
| 3 | `mmorpg_sql_pool_pools{sql_client="db"}` 之和与既有 `db_placement_open_stores` 是同一事实 | 后者面板可能已在用,不删 | 两个指标表达同一件事 |
| 4 | 在 K8s 上打开 go/db 的 /metrics(:9160)| 不打开则 P1 在 K8s 上没有数据;顺带满足 `owner-epoch-alerts.yaml` 的前提 | 任务原文没写这一项;多暴露一个集群内端口 |
| 5 | ConfigMap 多镜像了 db Placement 池、data-service PlayerName / RouteStore 池、gateway Hikari | 总账输入必须等于集群实际值;以前这些靠「Go 默认值恰好等于 yaml」 | ConfigMap 多几行;批 A→C 之间 db 的 Placement 键换一次 |
| 6 | 总账超限只打 WARN,不拒绝部署 | 新行为默认不阻断(任务硬规则);10 个 zone 的既有配置会立刻被挡 | 超限部署仍可能发生,靠人看 WARN |
| 7 | `max_connections=1800` 按规划档推算,不是压测得出 | 压测要按 AGENTS §6 由 Codex 跑,本规格不宣称容量 | 数值可能偏高或偏低;内存估算未实测 |
| 13 | 滚动余量按每个 Deployment 多 2 个实例计(R=2),比 maxSurge 字面值多算一个 | 退出中的旧 Pod 不计入 surge 却仍持有连接(§1.3) | 规划峰值从 1436 涨到 1620,上限与内存随之上调;旧 Pod 的连接实际可能早于 grace 到期释放,这一项偏保守 |
| 14 | 改 `.github/workflows/deploy-config-tests.yml` 的触发路径 | 总账的输入与告警阈值文件不在触发路径里,契约测试拦不住漂移 | 动了 CI 配置(只加路径,不碰凭证 / secrets,AGENTS §9 允许);文件数到 30 |
| 8 | MySQL 侧两条告警写在 mysqld_exporter 的指标上,但 exporter 本批不部署 | 新增镜像 / 账号属 AGENTS §10.2「需要授权」;规则先就位 | exporter 落地前这两条无数据 |
| 9 | gateway Hikari 改为 5 / 2(minimumIdle < maximumPoolSize) | 每 zone 省 10 条;HikariCP 官方建议固定大小池以求性能 | 突发时要现建连接;池更小,排队更早出现 |
| 10 | P2 保留旧键并让 `Normalize` 继续回填它 | 旧部署零改动兼容,旧测试保持绿 | 进程内多一对「只作回落」的字段 |
| 11 | `ConnMaxIdleTime` 只加在额外落点库池 | 任务范围;本 zone 主池与 data_service 池常热 | data_service 池仍无 `ConnMaxLifetime`(TiDB 文档 P1-1 已登记,`tidb-phase1-and-launch-readiness.md:148`、`:910`) |
| 12 | `DsAuditStorePools = 3` 在 PowerShell 里是常数 | Go 侧「三个 store 各开一个池」没有可读的配置项 | 一份事实两处写;注释互指 |

---

## 9. 风险

1. **应用批 A 的 mysql.yaml 会重启唯一的 MySQL**:模板变了 → `Recreate`(`mysql.yaml:84-85`),所有 zone 的数据层停一次。停机期间 go/db 的写任务连不上库,按同一条「普通错误耗重试 → 死信」链处理(推断)。必须在维护窗口、各 zone 停服或排空写入后执行;`SET PERSIST max_connections` 可在线改,但内存 limits 必须先到位,所以不建议单独在线调。
2. 内存估算未实测:limits 5Gi 不够会 OOMKill(比 1040 更糟);requests 1Gi 时节点内存紧张会先驱逐 MySQL。
3. 容器 open files 上限过低时 MySQL 会自动压低 max_connections 并只打一条启动警告,配置看似生效、实际没有(批 A 验证第 5 步核对)。
   同一类「配置看似生效」:`mysql-config` ConfigMap 变了不会重启 mysqld(Deployment 模板没变),以后**只改** max_connections 时运行中的实例仍是旧值,总账与契约测试却已按新值通过。mysql.yaml 注释写明两条出路(维护窗口 `rollout restart`,或 `SET PERSIST`);本批不加 ConfigMap 校验和注解(那会让任何 my.cnf 改动都重启唯一的 MySQL,属于行为变更,另行拍板)。
4. `all-up` 不带 `-WaitReady` 时各 zone 滚动重叠,峰值超出总账的单 zone 余量(§1.3)。发布 runbook 应要求 `-WaitReady` 或逐 zone `zone-up`;即使带 `-WaitReady`,上一 zone 退出中的旧 Pod 仍可能与下一 zone 的 surge 重叠最多一个 grace 窗口(go/db 45s),总账不覆盖这一段。
5. 额外 zone 家族池预算随 Z² 增长(§1.4);E_z 规划取 Z−1 已是最坏,但合服留下的源区库不在内,合服后要重算。
6. Phase 2 打开(E_n ≥ 1 且非 zone 池调大)不经重做规划就上线,会直接击穿 1800;`DbExtraStorePoolsBeyondPlan` 会报警,但只是事后。
6a. 批 C 的回退口径:只 `kubectl rollout undo deploy/db` 会留下批 C 的 ConfigMap(只有新键),旧二进制不认新键、按默认 8/2 开池。若新键已被调到 8 以下(例如为抑制 Z² 把 `ZoneStoreMaxOpenConn` 降到 4),回退后实际池大于总账。回退批 C 必须用旧 commit 重跑 `k8s_deploy.ps1 zone-up`(ConfigMap 随之重生成),不能只 undo Deployment;写进批 C 的交付说明与 README 短节。
7. gateway 池 5:服务器列表与公告不缓存,开服登录潮时可能排队,Hikari 默认 30s 超时会占住 Tomcat 线程。
   - **2026-10-15 更正(补探针连锁)**:上面只写到「占住 Tomcat 线程」,漏了后果。K8s 探针原先打业务端口 8081,与业务共用 Tomcat 的 200 个平台线程(没开虚拟线程、没配 `server.tomcat.threads`、没配 `management.server.port`)。MySQL 不可达时每个碰库的请求等满 30s,每秒约 7 个这样的请求就能占满线程(`/api/server-list` 每次 `findAll` 不缓存;`/api/queue-status` 不限流,随机 `zone_id` 每次都 `findById`;assign-gate 同 zone 的并发请求 join 在准入缓存 loader 上、没有超时),liveness(period 20s、默认 timeout 1s、failureThreshold 3)约 60s 内连续失败,两个副本被一起杀;重启时 `ddl-auto: validate` 要连上 MySQL,MySQL 没恢复就进 CrashLoopBackOff,恢复后还要再等一段退避。这与 `application.yaml` management 段注释「MySQL 故障时 assign-gate 自己回 5xx,恢复后立即可用」正好相反。池大小在「MySQL 不可达」这一形态下无关,属 A3 之前就有的缺陷;A3 只让「MySQL 变慢」或「被刷」时更早耗尽连接池。
   - 修复(未编译、未测试,待 Codex 验证):① 根治:三个探针改打独立管理端口 8082(gateway ConfigMap 写 `management.server.port: 8082`,`deploy/k8s/manifests/java-svc/gateway.yaml` 三个探针同步改,契约测试钉住两边一致),actuator 用独立的连接器与线程池;jar 内不写,因为本机 `start_game.ps1` 按 8081 取 `/actuator/health`。代价:探针不再反映业务连接器能否接新连接;跨越本改动做 `kubectl rollout undo` 会让探针退回 8081 而 ConfigMap 仍是 8082,回退必须用旧 commit 重跑 `k8s_deploy.ps1`。② `connection-timeout: 3000`(§10 第 5 条),只把触发阈值抬高约 10 倍,单靠它消除不了连锁。③ 未做(可选):server-list 1s 短缓存;queue-status 先用缓存的 zone 目录判断 `zone_id` 是否存在再决定查不查库。
   - **2026-10-15 待负责人确认**:上面的 ①② 是评审直接落码,没有经过负责人拍板。现象、触发条件与依据见本条前两条更正注,要拍板的事项统一登记在 §10 第 5 条的待拍板注。
   - **2026-10-15 已定(负责人按「最标准」定)**:①② 照上面落码保留,③ 的两项一做一不做。具体:探针走独立管理端口 8082;`connection-timeout` 取 3000 毫秒;server-list 加短缓存;queue-status 不加 zone 目录前置校验。理由与改动位置见 §10 第 5 条的「已定」注。上面「待负责人确认」注保留作历史。
   - **2026-10-16 补注(代价补全)**:「探针不再反映业务连接器能否接新连接」对 readiness 同样成立。业务线程被占满或死锁时,副本仍是 Ready,不会被摘出 Endpoints,PDB 照算健康;改动前 readiness(8081)约 30s 会摘流。gateway 没有导出指标、也没有错误率告警(§10 第 10 条未定),只能人工看日志。人工处置及「MySQL 不可达时不要删 Pod」写在 `deploy/k8s/README.md` 同名一节的 2026-10-16 补注。`docs/ops/release-checklist.md` §4 与附录 E.2 第 3 级仍把 `kubectl rollout undo` 列为通用回滚手段,尚未写 gateway 例外(待同步,该文件不在本次改动范围)。
8. Collector 依赖「池不被移除」才保证 counter 单调;以后若给注册表加回收,要同时改聚合口径。
9. 契约测试的阈值提取依赖告警文件「一行一个阈值」的写法;有人把表达式改成多行会让用例红(有意为之,失败信息要写清)。
10. 凭记忆、未在本机核实的外部事实:Hikari 默认值、Spring Boot `./config/` 加载顺序、MySQL 8.0 默认 `innodb_buffer_pool_size` / `temptable_max_ram`、mysqld_exporter 指标名、go-zero 对未知配置键的处理、promhttp 对 Collect panic 的处理、K8s Deployment 滚动时不计退出中 Pod 的行为(§1.3)。
11. 本规格的每个数字都没有压测支撑;按 AGENTS §6 不得据此宣称容量结论。
12. 与并行工作的文件重叠:TiDB 文档 P1-1(`tidb-phase1-and-launch-readiness.md:910`)也列了「导出 `sql.DBStats`(与 L-08 合并)」并改 `go/data_service/internal/store/*`、各服务 `svc`。G2b 批 B 落地后它由本项承担;P1-1 若再注册一个池收集器,`RegisterSQLPools` 那一侧会因重复注册只记 ERROR、指标缺一半(观测面不挡落库,但很难察觉)。D1 第 2 阶段切 `db_first` 时改 data-service ConfigMap 的 `RouteStore` 段,要保留批 A 镜像的 `MaxOpenConn` / `MaxIdleConn` 两行。

---

## 10. 待负责人拍板

1. **改动面 30 个文件**(已达 AGENTS §10.2 的「30+ 文件」阈值,**未获授权前不得落码**):是否授权按 A / B / C 三批落地,还是要求进一步拆小(例如把 A13 的 CI 路径单独成一个 PR)。
2. 规划档参数:Z=4、E_z=Z−1、E_n=0、route store 按 db_first 计入、滚动余量 R=2(每个 Deployment 一个 surge + 一个退出中的旧 Pod)、运维余量 20;以及由此得出的 `max_connections=1800`。若目标 zone 数不是 4,给出数字后重算;若接受 R=1 的乐观口径(赌旧 Pod 的连接先于新 Pod 满载释放),上限可回到 1600、内存回到 4Gi。
3. MySQL 内存:base manifest requests 1Gi / limits 5Gi;生产是否用 overlay 设 requests = limits(Guaranteed),需要的节点规格。
4. 总账超限是否升级为部署门禁(建议:staging / prod 档 throw,dev 只 WARN);10 个 zone 的配置在单台 MySQL 上是否直接禁止。
5. Java gateway:池 5 / 2 是否接受;`connection-timeout` 是否从默认 30s 改为 3s(更快失败,5xx 语义提前)。
   - **2026-10-15 更正**:评审修复已按 3000 毫秒落码(见 §4.1.5 末尾与 §9 第 7 条的更正注),原因是 30s 叠加探针与业务共用线程池会让 MySQL 故障演变成全部副本被 liveness 杀掉。负责人仍可推翻;推翻时 jar、ConfigMap 镜像与两处测试同批改回。
   - **2026-10-15 待负责人拍板(补:探针端口与可选缓存)**:§9 第 7 条修复 ① 让三个探针改打独立管理端口 8082,这同样是评审直接落码,而且本条原文没有列这一项。请负责人一并定三件事:
     - (a)探针走 8082 及其代价:探针不再反映业务连接器能否接新连接;不能跨越这次改动做 `kubectl rollout undo`,回退只能用旧 commit 重跑 `k8s_deploy.ps1`。
     - (b)`connection-timeout` 取 3000 毫秒还是 2000 毫秒(评审建议 2–3s)。
     - (c)两项可选优化做不做:server-list 短缓存,以及 queue-status 先判 zone 是否存在。
     - 为什么要负责人定:(a)改的是 K8s 部署形态与回退口径;(b)本来就是本条的待拍板项;(c)新增缓存语义,服务器列表会最多旧一个缓存周期。
     - 不论怎么定,有两点不变。「MySQL 不可达时两副本被 liveness 一起杀掉、重启时 `ddl-auto: validate` 又要连库」是 A3 之前就有的缺陷,在这一形态下与池大小无关,A3 只让「MySQL 变慢 / 被不限流端点刷」时更早触发。只定 (b)、不定 (a),消除不了这条连锁。
   - **2026-10-15 已定**:负责人按「最标准」定为以下四项。上面的待拍板注保留作历史。全部未编译、未测试,待 Codex 验证。
     - (a)探针走独立管理端口 8082。这是根治连锁重启的做法,代价与回退口径照上面写的接受。回退只能用旧 commit 重跑 `k8s_deploy.ps1`,不跨这次改动做 `kubectl rollout undo`;这条已写进 `deploy/k8s/README.md` 的「gateway 探针、管理端口与回退口径」一节。
     - (b)`connection-timeout` 取 3000 毫秒。取连接的等待远短于 30s 默认值,又给瞬时抖动留了余量。2000 与 3000 没有实质差别,沿用已落码的值,待压测确认。
     - (c1)server-list 加短缓存。它是只读的公开列表,每次请求都查库是放大面。实现:`ServerListService` 只缓存 zone_config 的行,TTL 1 秒,与 `AssignGateService` 准入缓存同口径;过期瞬间的并发请求合并成一次 `findAll`(single-flight);查库失败不缓存,异常原样抛出,错误语义不变;探测态与「新服」判定仍按请求现算。用例见 `ServerListServiceZoneRowsCacheTest`。
       - **2026-10-16 补注**:(c1) 落码时等待方用 `join()`,没有上限。查库方挂住(半开 TCP、MDL 排队;JDBC 未配 socketTimeout / 查询超时)时,过期后到达的全部 server-list 请求都会无限期挂在同一个 future 上,线程占用随挂住时长无界增长,而探针已在 8082,副本既不摘流也不重启。已改为等待方最多等 3000ms(`ServerListService.ZONE_ROWS_LOAD_WAIT_MILLIS`,与 connection-timeout 同口径)后抛 `QueryTimeoutException` 回 5xx,恢复「错误语义不变」;查库方自身的读超时登记为后续。`AssignGateService.awaitAdmission` 有同样问题(见 §9 第 7 条的 2026-10-15 更正注),本项未改。同时补了用例:等待方超时、查库失败时全部等待方拿到同一异常,并发用例改用在途等待方计数作屏障。未编译、未测试,待 Codex 验证。
         复查补充:「恢复改动前语义」只对「取不到连接」成立。查库方挂在已取到的连接上时,在途查库只有它自己能摘,该副本后到的全部 server-list 请求都等满 3s 回 5xx,直到套接字自己断开(约 15 分钟到 2 小时);改动前同类单连接故障只拖住一个请求。概率低,根治靠 JDBC `socketTimeout`(取值待负责人定,影响 gateway 全部查询),本项不在类内另做「超时顶替查库方」。§9 第 7 条所说的 `release-checklist.md` 回滚例外已于 2026-10-16 同步。
         **2026-10-16 补注(续,已补根治)**:负责人要求「做完」,按最标准补上:gateway JDBC URL 加 `connectTimeout=3000&socketTimeout=10000`(jar 内 `application.yaml` 为单一真相,`k8s_deploy.ps1` 镜像进 ConfigMap、缺项即生成失败,契约测试新增一例钉住;取值与 Java 版 game-gateway 相同),查库方挂住最多约 10s 即失败;`AssignGateService.awaitAdmission` 同样改为限时 3000ms,到点 fail-closed 回 500 `zone_admission_unavailable`(新用例 `loaderStuck_waiterFailsClosedWithinBudget_thenCacheServesNextRequest`,并发用例加屏障)。代价:管理接口写在行锁 / MDL 上等超过 10s 会回 5xx。未编译、未测试。
     - (c2)queue-status **不加** zone 目录前置校验(有意不做)。开区瞬间,新 zone 刚写进 zone_config,还没进目录快照,前置校验会把真实排队玩家判成 `zone_not_found`(404),而本来这次轮询应当照常推进队列。这个正确性代价大于收益。探针隔离、取连接有 3 秒上限之后,刷这个端点只会让碰库的请求回 5xx,不再连带重启。随机 `zone_id` 撑大负缓存的问题,`AssignGateService` 已有 4096 条上限兜住。
       - **2026-10-16 补注(更正 (c2) 末句)**:「只会让碰库的请求回 5xx」偏乐观。业务线程池是所有端点共用的:MySQL 不可达(取不到连接)时每个碰库请求占线程约 3s,超过约 200/3≈67 次/s 就能占满一个副本;已取到连接后查询挂住则没有上限(JDBC 未配读超时);MySQL 正常时阈值是池吞吐;queue-status 还要调 login 的 gRPC(timeout 3s、retry 1)。占满后同一副本上不碰库的请求也会变慢,只是不再连带重启,且 readiness 不会把它摘出。不改「不加前置校验」的决定。
6. mysqld_exporter:是否部署(建议 MySQL Pod 的 sidecar,镜像钉版本与 digest,独立账号只给 `PROCESS, REPLICATION CLIENT, SELECT ON performance_schema.*` 并设 `MAX_USER_CONNECTIONS 3`,口令走 Secret;部署后它的连接计入运维余量)。
7. 批 A 应用到共享环境的维护窗口与执行顺序(先停服 / 排空写入,再 infra-up)。
8. 是否下调 `ZoneStoreMaxOpenConn`(访客读不需要 8 条)以抑制 Z² 增长 —— 要先确认合服 pin 后源区库的写流量是否也走这个池(会走:那是整个被合 zone 的存盘)。
9. L-04「1040 / 拿不到连接时写延后、不耗重试预算」何时排期(本规格明确不做)。
10. friend / trade / Java gateway 是否也导出池指标(gateway 需要加 `micrometer-registry-prometheus` 依赖并开放 actuator 端点,属新增依赖)。

---

## 11. Java 版(AGENTS §12)

- 判断:**本项是服务端内部(部署与观测),不涉及客户端契约**(帧格式、客户端可见 proto、tip 码、配表都不变),两版不需要同批改。
- Java 版现状(`D:\luyuan\xuanming-server-mmo`,HEAD `1da33f1`):只有 compose 部署(`deploy/compose/infra.yaml`),没有 K8s 清单;MySQL 8.4 未设 `max_connections`(`infra.yaml:28-36`,镜像默认 151,外部事实);9 个 Spring 服务带 datasource(`game-data` / `game-friend` / `game-gateway` / `game-guild` / `game-login` / `game-match` / `game-scene` / `game-team` / `game-trade` 的 `application.yaml`),全部没有 Hikari 配置,即每进程恒开 10 条,单副本合计 90 < 151。
- 状态:**待做,不阻塞本批**。Java 版上多副本或 K8s 之前需要同样的总账(显式 Hikari、Micrometer 的 `hikaricp_*` 指标、按副本数核算 max_connections)。Docs 阶段在 Java 仓库 `PARITY.md` 登记一行「G2b 连接预算:C++/Go 版已落码(未验证),Java 版待做」;本仓库 `docs/PROGRESS.md` 条目注明「对应 Java 版本号:无」。
- 注意:本仓库的 `java/gateway_node` 是 C++/Go 版自己的网关,不是 Java 版;批 A 对它的改动不需要在 Java 仓库对应。

---

## 12. 不做 / 后续登记

- L-04:go/db 遇 1040 或拿不到连接时写延后、不耗重试预算,死信加消费者与告警(`tidb-phase1-and-launch-readiness.md:815`)。本规格不改任何 1040 处理路径。
- mysqld_exporter 部署(§10 第 6 条)。
- friend / trade / gateway 的池指标(§10 第 10 条)。
- data_service / friend / trade 池补 `ConnMaxLifetime`(TiDB Phase 1 P1-1,`tidb-phase1-and-launch-readiness.md:148`、`:910`)。P1-1 同一行里的「导出 `sql.DBStats`」由本规格批 B 承担,P1-1 落码时删掉这一子项、复用 `shared/sqlpoolstats`(A12 追加状态注时写明)。
- MySQL `innodb_buffer_pool_size`、`temptable_max_ram`、CPU 规格(TiDB Phase 1 或单独的 MySQL 规格项)。
- 规划档随 Phase 2 / 合服的重算流程写进 runbook(`docs/ops/merge-zone-runbook.md` 合服后步骤加一条「重算总账」)。

## 13. 连带文档(随各批改,不写 PROGRESS)

- 批 A:`deploy/k8s/README.md`(:505、:596、:636 指向总账 + 新短节:总账函数、规划档、写路径那一行怎么看、超限 WARN 的两条出路、只改 my.cnf 不会重启 mysqld);`no-single-node-horizontal-scaling-20261001.md` §3.4 更正注之后追加「G2b 批 A 已落码(未验证):max_connections 1800、gateway 池 5、滚动余量按每 Deployment +2 实例计、总账函数与契约测试」;`tidb-phase1-and-launch-readiness.md` §2.4 表与 L-08 之后追加状态注(gateway 行改为显式 2×5、滚动余量行改为 R=2 的说明以追加注形式给出,不改原表;P1-1 的 DBStats 子项改由 G2b 批 B 承担)。
- 批 B:同上两份文档各追加一行(指标名、告警文件);`deploy/k8s/README.md` 新短节补指标与告警。
- 批 C:`player-storage-placement.md:142` 改为新键 + 旧键回落说明。
- Docs 阶段的 PROGRESS 条目要点:三批的文件数、契约测试 88→95→97、所有数值「待压测确认」、未验证项、Java 版状态。
- 批 C 的交付说明另写一句回退口径:回退必须用旧 commit 重跑 `zone-up` 重生成 ConfigMap,不能只 `rollout undo`(§9 第 6a 条)。

---

## 评审处理记录(2026-10-12,对抗式评审,只读核对代码,未运行任何命令)

核对范围:逐条回到 `mysql.yaml`、`go-svc/{db,data-service,friend,trade,gateway}.yaml`、`java-svc/gateway.yaml`、`k8s_deploy.ps1`(`:878-931`、`:936-1003`、`:3045-3110`、`:3400-3560`、`:3955-4090`、`:4780-5010`、`:5453-5470`、`:5911-6005`)、`k8s_deploy_contract.tests.ps1`、`lib/release_common.ps1`、`lib/deploy_capture.ps1`、`go/db` 的 `config.go` / `config_test.go` / `metrics.go` / `db.go` / `store_registry.go` / `proto_sql/db.go` / `dbtest`、`go/data_service` 的 `servicecontext.go` / `config.go` / `mysql.go` / 五个 store / `metrics.go` / `data_service.go`、Java gateway 的 `application.yaml` / 测试资源 / 服务类、`.github/workflows/deploy-config-tests.yml`、TiDB 文档 §2.4 / §7 / P1-1、PROGRESS 2026-10-12 条目。

### 必须改(已改进正文)

| # | 问题 | 证据 | 处理 |
|---|---|---|---|
| M1 | **滚动余量少算一个实例,规划峰值越过 1600**。Deployment 控制器算 surge 时不计退出中的 Pod,2 副本 + maxSurge 1 + maxUnavailable 0 时同一刻最多 4 个 Pod 持有连接;go/db preStop 5s 期间仍满池消费、grace 45s。按 R=2 重算,规划峰值 1436 → 1620 > 1600,原规格的契约测试会因公式少算而放行一个真实会超限的配置(超限后果是 1040 → 耗尽重试 → 死信) | `go-svc/db.yaml:35-39`、`:80-83`、`:97`;`go-svc/data-service.yaml:38-42`、`:97-100`;K8s Deployment 文档滚动更新一节(外部事实) | §1.3 补机制;§2.1 公式加 R;§2.2 表重算;规划档加 `RolloutExtraPodsPerDeployment = 2`;`Get-MysqlConnectionLedger` 用必填 `-RolloutExtraPodsPerDeployment` 取代 `-WithoutRolloutSurge`(自检传 0);`max_connections` 1600 → **1800**、limits 4Gi → **5Gi**(§2.3、§4.1.1、§7、§10 第 2 / 3 条同步);R=1 的乐观口径列为可拍板的备选 |
| M2 | `sqlpoolstats` 的 import 清单含 `time`,按设计并不使用(`WaitDuration.Seconds()` 不需要导入 time),照抄即编译失败;其余新代码的 import 增量没写(`store_registry.go`、两个 `metrics.go` 要加 `shared/sqlpoolstats`;`servicecontext.go` 要加 `database/sql`、`shared/sqlpoolstats`、`data_service/internal/metrics`) | 硬规则「import 齐全无多余」;`store_registry.go:3-21`、`servicecontext.go:3-19` 现有 import | §4.2.1 删 `time` 并注明;§4.2.2、§4.2.3 补 import 增量与「不成环」的依赖方向;注明 go.mod 无需改(`shared` 已直接 require client_golang / go-zero,db 与 data_service 已 replace shared) |
| M3 | `go vet` 的 composites 检查会报告**导入包**结构体的无键字面量;§7 的通过标准是 vet 无告警,而规格没约束 `sqlpoolstats.Sample` 的写法 | `go vet` composites 检查的既有行为 | §4.2.1 要求一律 `sqlpoolstats.Sample{Pool: …, Stats: …}`;§4.2.3 注明本包 `namedStore` 不受影响 |

### 重要(已改进正文)

| # | 问题 | 证据 | 处理 |
|---|---|---|---|
| I1 | 契约测试的输入不在 CI 触发路径里:改 Java gateway 池大小、`deploy/k8s/zones.json`、批 B 的告警阈值文件都不会跑 `deploy-config-tests`,总账与现实可以静默分家 | `deploy-config-tests.yml:30-71` 只列 `tools/scripts/**`、`bin/etc/**`、`go/*/etc/**`、`deploy/k8s/manifests/**` 等 | 新增 A13 改该 workflow 两份 `paths`;§6.1、§7 第 6 步、§8 第 14 条同步。文件数 29 → **30**,达到 AGENTS §10.2 阈值,§0 与 §10 第 1 条改为「未获授权不得落码」 |
| I2 | 「只有总账认识的 MySQL ConfigMap」只扫 zone-up 产物;全局服务(friend / trade)在 infra-up 才生成,新增一个连 MySQL 的全局服务能绕过总账 | `Apply-GlobalGoSvcManifests`(`k8s_deploy.ps1:4846-4875`)只在 infra-up / all-up 跑;既有 infra-up DryRun 都带 `-SkipGoSvc`(`k8s_deploy_contract.tests.ps1:1078`) | 同一用例内再起一次 infra-up DryRun(去掉 `-SkipGoSvc` / `-WaitReady`),断言集合 == friend、trade;计数不变 |
| I3 | 镜像 RouteStore / PlayerName 池参数后,服务 yaml 一个非法池值会让 K8s 上的 data-service 在 redis 模式下也拒启(以前取代码默认,不受影响);总账只核正整数,不核 idle ≤ open | `data_service.go:94-100`(不分 Mode 校验)、`config.go:356-362`、`:612-631`;`main` 的 `Store.Required` 拒启(`:113-119`) | §4.1.4 第 2 点写明副作用;生成器读这些值时核「正整数且 idle ≤ open」,不满足在 apply 前 throw;§5 补一行 |
| I4 | `mysql-config` 是 ConfigMap,只改 `max_connections` 不会重启 mysqld,运行值与总账 / 契约测试脱节且无报错 | `mysql.yaml:45-75`(my.cnf 在 ConfigMap)、`:77-93`(模板不引用其内容) | §4.1.1 注释与 §9 第 3 条写明两条出路;不加校验和注解(那是「任何 my.cnf 改动都重启唯一 MySQL」的行为变更,另行拍板) |
| I5 | 批 C 只 `rollout undo deploy/db` 时留下只有新键的 ConfigMap,旧二进制按默认 8/2 开池;新键若被调小,回退后实际池大于总账 | `config.go:291-297`(旧二进制只认旧键);go-zero 对未知键静默忽略(凭记忆) | §9 第 6a 条、§13 批 C 交付说明写明回退必须重跑旧 commit 的 `zone-up` |
| I6 | 与 TiDB P1-1 重叠:P1-1 也列了「导出 `sql.DBStats`(与 L-08 合并)」并改同一批文件;两边各注册一个收集器会撞重复注册、指标缺一半 | `tidb-phase1-and-launch-readiness.md:910`、`:819` | §9 第 12 条、§12、A12、§13 写明由 G2b 批 B 承担,P1-1 删该子项、复用 `shared/sqlpoolstats`;D1 第 2 阶段改 `RouteStore` 段时保留镜像的两行 |
| I7 | 负向用例 `-ZonesConfigPath deploy/k8s/zones.10zones.yaml` 用相对路径,子进程工作目录不保证是仓库根;既有 all-up 用例用的是 `Join-Path (Get-RepoRoot) …` | `k8s_deploy_contract.tests.ps1:1252` | §6.1 改为绝对路径,并写明 Z = 11(含 `-ZoneId 101`) |

### 小问题(已改进正文)

| # | 问题 | 处理 |
|---|---|---|
| m1 | zone-up 写路径调 `Get-InfraZoneIds`,zones 配置缺席时会打一条讲「mysql-init-sql 只预建」的 WARNING,对 zone-up 是误导(`k8s_deploy.ps1:5464-5466`) | §4.1.3:先 `Test-Path`,缺席时 Z=1 并在行尾标 `zones_config=missing`;§5 补一行 |
| m2 | 「E_z=0 时稳态也有 2080」只在 F=0 时成立,规划档 F=1 时是 2480 | §2.2 改正 |
| m3 | 批 A 的 `Get-MysqlConnectionPlan` 注释引用了批 B 才新建的告警文件 | §4.1.2 改为批 B 起再加那句 |
| m4 | `$global` 作变量名易与作用域修饰符混淆 | §4.1.2 改名 `$globalPools`;补一句 Mandatory `[int]` 传 0、`[bool]` 传 `$false` 可绑定 |
| m5 | `Collect` 的并发语义与 `pool="other"` 是否预置没写 | §4.2.1 补:累加器为局部变量、收集器构造后只读;`other` 不预置 |
| m6 | `dbtest.Open` 必须传 handler 参数,规格没说传什么 | §4.2.2:`dbtest.Open(t, nil)`,`Stats()` 不下发语句 |
| m7 | `-WaitReady` 的 `rollout status` 不等旧 Pod 退出完,逐 zone 等待也会有一个 grace 窗口的重叠 | §1.3、§9 第 4 条写明总账不覆盖这一段 |

### 核对无误(未改)

- §2.2 原表的算术逐格复算无误(1232 / 184 / 1436、592 / 168 / 780、920 / 500、3920 / 232 / 4172);自检锚点与 `PROGRESS.md:7178-7180` 一致。
- 五个 manifest 各恰有一行两格缩进的 `replicas:`;`friend.yaml:62`、`trade.yaml:51` 的 `# replicas>=2` 注释不会被 `^  replicas:` 命中。
- `Get-AuthoritativeScalar` 依赖的 `Get-YamlScalar` / `ConvertFrom-YamlToFlatMap` 由 `deploy_capture.ps1` dot-source 的 `release_common.ps1` 提供,AST 抽取名单够用;`data.application.yaml.*` 的拍平键已有先例(`:964`、`:1529`)。
- `go/data_service/etc/data_service.yaml:110-111`、`:175-176` 确有 `PlayerName.*` / `RouteStore.*` 池键;`openMySQLWithDSN` 对 ≤0 取 5,data_service 不会出现 `+Inf`;五个 store 的连接池字段名都是 `db`。
- `stores.PoolSamples` / `svcCtx.SQLPoolSamples` 的方法值可直接赋给命名函数类型 `sqlpoolstats.Source`,两个 main 不需要新 import;proto_sql → metrics → sqlpoolstats、svc → metrics 都不成环;`OpenPlacementStore` 全仓唯一调用方在 `store_registry.go:177-179`。
- Java gateway 只在 `-JavaSvcRegistry` 给出时按 zone 部署(`k8s_deploy.ps1:4966-4977`、`:5407`);`auth` 无 manifest 不部署;外挂 `/app/config/application.yaml` 与 Dockerfile `WORKDIR /app` 一致;`application-test.yaml` 不覆盖 hikari,A3 测的是主配置的值。
- Java 版现状(HEAD `1da33f1`、9 个服务带 datasource、无 hikari、compose MySQL 8.4 未设 `max_connections`)核对属实。

### 不采纳 / 留待拍板

- 给 MySQL Pod 模板加 my.cnf 校验和注解(让 ConfigMap 变更自动重启):会让任何 my.cnf 改动都重启全服唯一的 MySQL,属行为变更,只在 §9 第 3 条记风险。
- `friend.yaml:31`、`trade.yaml:23` 的预算注释改指总账:说法本身没错,再加两文件会到 32 个,不做。
- R=2 是否过于保守(旧 Pod 的连接可能早于 grace 到期就释放):按 §11 取舍顺序先保正确性,R=1 的回退口径写进 §10 第 2 条由负责人拍板。
