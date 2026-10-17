# K8s One-Click Open-Server

This directory contains Kubernetes-only deployment assets for opening game zones.

## Scope

- `single zone`: open one zone (for example, `yesterday` zone).
- `all zones`: open all zones defined in a JSON or YAML config file.
- This flow is Kubernetes-only and does not include Docker Compose or local process startup scripts.

## Ops Runbook

- Day-2 operations and release/rollback steps: `docs/ops/k8s-open-server-runbook.md`.

## Directory Layout

- `manifests/infra/`: infra resources applied per namespace (`etcd`, `etcd-maintenance`, `redis`, `redis-match-cluster`, `kafka`, `mysql`, `loki`). `etcd` is a 3-replica StatefulSet with one PVC per member (see "etcd:3 副本 StatefulSet" below); `etcd-maintenance` 是 MVCC 历史压缩 CronJob `etcd-compact`(见该节「维护」) and `kafka` is a **single-broker** StatefulSet with one PVC (see "Kafka:StatefulSet + PVC" below); `redis` / `mysql` are still single-replica Deployments. `redis-match-cluster` 是 match 私有的 Redis Cluster(StatefulSet + 一次性建群 Job,Job template 落地后不可变,改建群脚本要先删 Job 再 apply)。`loki` 是 C++ 日志 sidecar 的写入目标(Deployment + `loki-data-pvc` 10Gi,retention 7 天),**只在 sidecar 开着且没给 `-LokiPushUrl` 时才随 `infra-up` apply** —— 关了 sidecar 或指向外部 Loki 时它不占集群资源(见 Optional Flags 的 `-NoCppLogSidecar`)。
- `manifests/go-svc/`: Go micro-service K8s manifests (`db`, `data-service`, `login`, `player-locator`, `scene-manager`, `match`, `trade` + its `trade-migrate` schema Job, `friend` + its `friend-migrate` schema Job). `guild` has no manifest / ConfigMap yet (see "snowflake 缓存目录 / PlayerId 号段 / data_service 全局库" below). trade 见下面「聚宝斋 trade」,friend 见下面「好友 friend」。
- `Dockerfile.go-svc`: multi-stage Dockerfile for building Go service images.
- `zones.sample.json`: sample multi-zone definition file (JSON).
- `zones.sample.yaml`: sample multi-zone definition file (YAML).

## Quick Start

1. Ensure `kubectl` is installed and your kube context works.
2. Stage Linux runtime files under `deploy/k8s/runtime/linux/`.
3. Build and push your C++ node runtime image.
4. Run one of the commands below from repo root.

## Runtime Image Flow

C++ 节点镜像有两条现役路径(2026-09-16 按代码核对;详表与风险见 `deploy/k8s/AGENTS.md`「C++ 节点镜像:两条路径」):

| 路径 | 编译在哪 | 调用方 | 构建入口自带门禁 |
|---|---|---|---|
| `deploy/k8s/Dockerfile.runtime` | 镜像外:Linux 主机 `build_linux.sh` → `k8s-stage-runtime` 拷进 `deploy/k8s/runtime/linux/` | `k8s-build-image` / `k8s-push-image` / `k8s-release-zone` / `k8s-release-all`(`tools/scripts/k8s_image.ps1`,`-DockerfilePath` 默认值)、`publish_images.ps1` 的 cpp 族 | 仅 `release-zone` / `release-all` 构建前跑不可变 tag 校验 + `release_preflight.ps1`;`build-image` / `publish_images.ps1` 不跑 |
| `deploy/k8s/Dockerfile.cpp` | 镜像内全量编译(gRPC `v1.83.0` 起) | `k8s-build-all`、`.github/workflows/cpp-build-ci.yml`(只到 `--target builder`) | 无(`k8s-build-all` 只构建) |

- 部署侧门禁与镜像出自哪个 Dockerfile **无关**:`k8s_deploy.ps1 -ReleaseProfile staging|prod` 的 `zone-up` / `all-up` / `infra-up` 对任意 `-NodeImage` 都做不可变 tag 校验与 `release_preflight.ps1`。上表只说明构建入口自己多不多查一次。
- 版本化发布(`publish_images.ps1` → `make_release.ps1`,见下方「发布打包」)走 `Dockerfile.runtime`。
- ⚠️ 两条路径的运行层**不等价**。按代码核对到的差异(不保证穷尽):`Dockerfile.cpp` 额外带了 `/usr/local/lib/*.so*` 并跑 `ldconfig`、带了 `data/scene_nav_bin/`、把表文件名转小写、apt 多装 `libssl3t64` 与 `libzstd1`;`Dockerfile.runtime` 与 staging 契约里都没有这些(二进制若动态链接 libssl / libzstd,runtime 路径的镜像会在加载共享库时起不来)。runtime 路径的镜像真实起过一次 scene 之前,不要假定二者可互换。
- Do not use the repository root `Dockerfile` as a K8s image(它是 C++ 开发工具链镜像)。
- Required staging layout is documented in `deploy/k8s/runtime/README.md`. 根 `.dockerignore` 不得排除 `deploy/k8s/runtime/linux/`(`Dockerfile.runtime` 以仓库根为 context COPY 它)。
- Current manifests assume Linux containers. Windows `.exe` outputs under `bin/` are not deployable to this image.

### Preflight Runtime Image

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-image-preflight
```

### Build C++ Nodes

容器内全量编译(`Dockerfile.cpp`,等价于 `k8s-build-all` 的 C++ 段;首次 30–60 分钟):

```bash
# 从仓库根执行 — 产出含 gate + scene + battle 的完整运行镜像
docker build -f deploy/k8s/Dockerfile.cpp -t mmorpg-node:<tag> .
# 只导出分离符号 → ./debug-symbols/symbols/*.debug(symbols 阶段把文件放在 /symbols/ 下,-o 导出整个阶段根目录)
docker build -f deploy/k8s/Dockerfile.cpp --target=symbols -o ./debug-symbols .
```

或在 Linux 主机上直接编译(`Dockerfile.runtime` 路径的前置步骤)。仓库里**没有** `build_grpc_linux.sh`:
`build_linux.sh` 不带 `--skip-deps` 时会先 `git submodule update --init --recursive`,
再跑 `tools/archived/setup_dependencies.sh` 编 gRPC / muduo / librdkafka / yaml-cpp / hiredis / zlib(已装且源码 stamp 未变则跳过)。

```bash
# 产出 bin/{gate,scene,battle};--split-debug 另出 bin/symbols/*.debug(publish_images.ps1 归档用)
bash tools/scripts/build_linux.sh --relwithdebinfo --split-debug
# 内存紧张时限制并行度
bash tools/scripts/build_linux.sh --relwithdebinfo --split-debug --jobs 4
```

### Stage Runtime Files

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-stage-runtime `
  -BinarySourceRoot D:/linux-build/bin `
  -ZoneInfoSource bin/zoneinfo `
  -TableSource generated/tables
```

This copies Linux `gate` / `scene` / `battle` binaries plus local `zoneinfo` and generated tables into `deploy/k8s/runtime/linux`.

### Build Runtime Image

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-build-image `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node `
  -ImageTag v1
```

### Push Runtime Image

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-push-image `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node `
  -ImageTag v1
```

### One-Command Release One Zone

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-release-zone `
  -ZoneName yesterday `
  -ZoneId 101 `
  -OpsProfile managed-cloud `
  -ImageRepository ghcr.io/luyuancpp/mmorpg-node `
  -ImageTag v1 `
  -WaitReady
```

## Go Micro-Service Image Flow

Go services use `deploy/k8s/Dockerfile.go-svc` (multi-stage build). Build context is the `go/` directory.
Each service gets its own image: `{registry}/mmorpg-{service}:{tag}`.

| Service | Image Name |
|---------|------------|
| db | `mmorpg-db` |
| data-service | `mmorpg-data-service` |
| login | `mmorpg-login` |
| player-locator | `mmorpg-player-locator` |
| scene-manager | `mmorpg-scene-manager` |
| match | `mmorpg-match` (global pool, deployed by `infra-up`, not per zone) |
| trade | `mmorpg-trade` (global pool, deployed by `infra-up`; the same image also runs the `trade-migrate` Job) |

### Build all Go service images

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-build-images `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1
```

### Push all Go service images

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command go-svc-push-images `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1
```

### Deploy Go services alongside a zone

Pass `-GoSvcRegistry` to include Go services in zone deployment.
Each service image is derived as `{GoSvcRegistry}/mmorpg-{svc}:{GoSvcTag}`:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday -ZoneId 101 `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:latest `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1 `
  -WaitReady
```

Omit `-GoSvcRegistry` or pass `-SkipGoSvc` to deploy only C++ nodes.

## Java Service Image Flow

Java services use `deploy/k8s/Dockerfile.java-svc` (multi-stage build). Build context is the `java/<service>/` directory.

| Service | Image Name |
|---------|------------|
| gateway | `mmorpg-gateway` |

### Build Java service image

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command java-svc-build-image `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1
```

### Push Java service image

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command java-svc-push-image `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1
```

### Deploy Java services alongside a zone

Pass `-JavaSvcRegistry` to include Java services in zone deployment:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday -ZoneId 101 `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:latest `
  -GoSvcRegistry ghcr.io/luyuancpp -GoSvcTag v1 `
  -JavaSvcRegistry ghcr.io/luyuancpp -JavaSvcTag v1 `
  -WaitReady
```

Omit `-JavaSvcRegistry` or pass `-SkipJavaSvc` to skip Java service deployment.

### gateway 探针、管理端口与回退口径(2026-10-15,G2b §9 第 7 条;未编译、未测试,待 Codex 验证)

- **探针打独立管理端口 8082**:`k8s_deploy.ps1` 生成的 `java-svc-gateway-config` 写 `management.server.port: 8082`,
  `manifests/java-svc/gateway.yaml` 的 startup / readiness / liveness 三个探针都打 8082,容器 ports 声明 `8082`(名 `management`)。
  两边由契约测试「Java gateway 探针走独立管理端口」按 DryRun 产物比对,任一处单改都会红。
  原因:探针原先与业务共用 8081 的 Tomcat 线程池,MySQL 慢 / 不可达、Redis 卡住或不限流的端点被刷时业务线程耗尽,
  liveness 跟着超时,两个副本被一起杀,重启时 `ddl-auto: validate` 又要连库。actuator 换到独立连接器与线程池后,这条连锁断开。
  代价:探针不再反映业务端口能否接新连接,业务线程卡死时 liveness 仍是 UP,要靠日志与上游错误率发现。
- **8082 只在集群内、只给 kubelet**:Service `gateway` 只暴露 8081,Ingress 只路由 `/api`。K8s 上 `/actuator/*`(health、info)只在 8082,
  8081 上不再有;排障或核对 `/actuator/info` 的版本号用 `kubectl -n <zone ns> port-forward deploy/gateway 18082:8082`,
  再 `curl http://127.0.0.1:18082/actuator/health`。仓库目前没有 NetworkPolicy;以后加时要放行 kubelet 到 8082 的探针流量
  (多数 CNI 默认放行本节点发起的流量,以实际 CNI 为准)。以后若开放 `/actuator/prometheus`(G2b §10 第 10 条),抓取端口同样是 8082。
- **本机不受影响**:jar 内 `application.yaml` 刻意不写 `management.server.port`,`start_game.ps1` 与排障文档照旧按 8081 取 `/actuator/health`。
- **回退不要跨这次改动做 `kubectl rollout undo`**:undo 只把 Pod 模板退回(探针回到 8081),ConfigMap 仍把 actuator 放在 8082,
  三个探针全部失败(startupProbe 约 600s 后杀容器)。回退一律用旧 commit 重跑 `k8s_deploy.ps1 zone-up`,ConfigMap 与 Pod 模板同批生成。
  滚动期间旧 Pod 的容器若被重启,可能读到已经同步进卷的新 ConfigMap(8082),而探针仍是旧模板的 8081,这个 Pod 起不来,等滚动把它替换掉即可。
- **相关的两处应用侧改动**(同一条修复):Hikari `connection-timeout: 3000`(取不到连接最多等 3s,原默认 30s;由 ConfigMap 镜像,见下面
  「MySQL 连接总账」);`/api/server-list` 的 zone_config 行改为 1s 短缓存,过期瞬间的并发请求只查一次库,查库失败不缓存。
  `/api/queue-status` 有意不加「先按 zone 目录判断 `zone_id` 是否存在」的前置校验:开区瞬间新 zone 还没进目录快照时会把真实排队玩家判成
  `zone_not_found`;探针已隔离、取连接有 3s 上限之后,刷这个端点只会让碰库的请求回 5xx,不再连带重启。
- **2026-10-16 补注(readiness 同样不再摘流;发现与处置)**:上面「代价」只举了 liveness。readiness 同样打 8082、只看 `readinessState`
  (应用代码不在运行期改它),业务 Tomcat 线程(默认 200)被占满或业务代码死锁时,这个副本仍是 Ready:不会被摘出 Service 的 Endpoints,
  继续分到约一半请求;PDB(minAvailable 1)也照样算它健康,drain 节点时可能先驱逐正常的那个副本。改动前 readiness 在 8081 上
  (默认 timeout 1s、period 10s、failureThreshold 3)约 30s 就会把它摘流。上面说的「上游错误率」目前没有落地手段:gateway 的 actuator
  只暴露 health、info,没有导出指标(开放 `/actuator/prometheus` 需新增 micrometer 依赖,见 G2b §10 第 10 条),仓库里也没有 gateway
  或 Ingress 的错误率告警,现阶段只能人工看日志和客户端 / Ingress 侧报错。处置:只有单个副本卡住、另一副本正常且 MySQL 可达时,用
  `kubectl -n <zone ns> delete pod <卡住的 Pod>` 让 Deployment 重建(直接 delete 不经 PDB);MySQL 本身不可达、两个副本都慢时不要删 Pod,
  新 Pod 启动时 `ddl-auto: validate` 要连库,会进 CrashLoopBackOff,正是本改动要断开的连锁。
  另外,上面「刷这个端点只会让碰库的请求回 5xx」偏乐观:业务线程池是所有端点共用的。MySQL 不可达(取不到连接)时每个碰库请求占线程约 3s,
  碰库请求超过约 200/3≈67 次/s 就能占满一个副本;MySQL 正常时阈值是 5 条连接的池吞吐;queue-status 还要调 login 的 gRPC
  (`login.grpc.timeout-ms: 3000`、`retry: 1`)。占满后同一副本上不碰库的请求也会排队变慢或超时,只是不再连带重启。
- **2026-10-16 补注(server-list 等待方上限;JDBC 读超时缺口;回滚清单未同步)**:
  - `/api/server-list` 的合并查库落码时等待方用无上限的 `join()`:查库方一旦挂在已取到的连接上(半开 TCP、zone_config 上的 MDL 排队),
    过期后到达的全部请求都会无限期挂在同一个 future 上,线程占用随挂住时长无界增长,而探针在 8082 上照样是绿的。已改为等待方最多等
    3000ms(与 `connection-timeout` 同口径)后回 5xx。这只在「取不到连接」时恢复了改动前「最多等 3s 回 5xx」的语义:查库方挂在
    已取到的连接上时,在途查库只有它自己能摘,这个副本上后到的全部 server-list 请求都会等满 3s 回 5xx,直到那条套接字自己断开
    (半开 TCP 按内核重传约 15 分钟,在等回包时按 keepalive 约 2 小时)。改动前这种单连接故障只拖住拿到那条连接的请求,合并查库把它
    放大成整个副本的 server-list 不可用;概率低(查库方恰好在查询中途遇到连接异常),根治靠下面的 JDBC 读超时。
    (2026-10-16 续)已补:gateway 的 JDBC URL 带 `connectTimeout=3000&socketTimeout=10000`(jar 内 `application.yaml` 为单一真相,
    `k8s_deploy.ps1` 从它的查询串镜像进 `java-svc-gateway-config`,缺项即生成失败,契约测试钉住两边一致;取值与 Java 版
    game-gateway 相同)。查库方挂住最多约 10s 即失败、摘掉在途查询,上面的不可用窗口收到约 10s。代价:管理接口的写在行锁 / MDL
    上等超过 10s 会以通信异常回 5xx。`/api/assign-gate`、`/api/queue-status` 的准入缓存等待方(`AssignGateService.awaitAdmission`)
    也已改为最多等 3000ms,到点 fail-closed 回 500 `zone_admission_unavailable`。
  - `connection-timeout` 只限取连接。两份 JDBC URL(jar 内 `application.yaml` 与 `k8s_deploy.ps1` 生成的 `java-svc-gateway-config`)都没配
    `socketTimeout`、也没配查询超时,已取到连接后的查询本身没有上限,查库方仍可能占住 1 个线程与 1 条连接,登记为后续。
    `/api/assign-gate` 的准入缓存等待方(`AssignGateService.awaitAdmission`)仍是无上限 `join()`(改动前就有,G2b §9 第 7 条已记),本次未改。
  - gateway 的上一个 revision 早于 2026-10-15 探针改端口时,不做 `rollout undo`,用旧 commit 重跑 `k8s_deploy.ps1 zone-up`。
    [docs/ops/release-checklist.md](../../docs/ops/release-checklist.md) §4「快速回滚单个 Deployment」与附录 E.2 第 3 级已于 2026-10-16
    追加这条 gateway 例外,并注明 E.2 里的 `deploy/gateway-node -n mmorpg` 是 2026-05 的旧名(当前 Deployment 叫 `gateway`,位于各 zone namespace)。

## 发布打包(版本库外制品 + release manifest)

标准与分批方案见 [docs/design/release-packaging-standard-20260914.md](../../docs/design/release-packaging-standard-20260914.md),
发布步骤清单见 [docs/ops/release-checklist.md](../../docs/ops/release-checklist.md)。本节只列入口与约定。

- **制品根**:环境变量 `MMORPG_ARTIFACT_ROOT`,缺省 `<仓库父目录>/artifacts`,**不在仓库内**。布局:
  `{snapshots|releases}/images/<版本>/{images/*.tar, images-manifest.json, build-info.json, symbols/*.debug, sha256sums.txt}`、
  `{snapshots|releases}/images/latest.json`、`releases/manifests/<vX.Y.Z>.{json,md}`。
  版本目录已存在即拒绝覆盖(不可变),先写 `.tmp-*` 再整目录 rename(原子);快照轨目录名 `g<sha12>`,发布轨 `vX.Y.Z`。
- **镜像 tag 与版本**:给 `-Version vX.Y.Z`(或 `MMORPG_RELEASE_VERSION`)时 tag = `vX.Y.Z-<sha12>`,否则 `<sha12>`;
  发布轨拒绝脏树。所有运行镜像带 OCI label(`org.opencontainers.image.{version,revision,created,source,title}`)
  与 `/app/BUILD_INFO`;`publish_images.ps1` 核对 `org.opencontainers.image.revision` == 当前 commit,不符即拒绝。
  出事时 `kubectl exec <pod> -- cat /app/BUILD_INFO` 确认 Pod 实际跑的提交。
- **每个镜像单独 `docker save`**:规避"两个镜像层链完全相同时批量 save 丢一个"的坑;`import_images.ps1` 逐个 load 后核对镜像 ID。
- **发布集合只有 cpp / go / java 三族**(`-Families`);`Dockerfile.robot` / `Dockerfile.sandbox-mock` 是测试工具镜像,不进制品目录。
  cpp 族走 `k8s_image.ps1`(`Dockerfile.runtime`),打包前先在 Linux 主机 `build_linux.sh --relwithdebinfo --split-debug` + `k8s-stage-runtime`。
- **推 registry 是受限操作**(AGENTS §9):只推用户明确指定的 dev registry,AI 不执行 push;推送后用 `-DigestsOut` 记录 digest 再交给 `make_release.ps1`。

```powershell
# 快照轨(工作树必须干净):构建三族镜像 → 逐个 docker save → snapshots/images/g<sha12>/
pwsh -File tools/scripts/publish_images.ps1

# 发布轨:版本号注入镜像 → releases/images/v1.2.3/
# -Registry 填最终要推送的 registry 前缀:镜像 ref(含 images-manifest.json 里的 ref)按它命名,下面推送时才对得上;
# 不填则出包与推送都用各镜像脚本默认的 ghcr.io/luyuancpp(与 docs/ops/release-checklist.md §2.1 ③ 一致)。
pwsh -File tools/scripts/publish_images.ps1 -Version v1.2.3 -Registry <registry>
# CI 重跑幂等:-SkipBuild 只收集已按当前提交构建好的镜像(-Registry 必须与构建时相同,否则按另一个前缀找镜像);
# -SkipIfExists 在同一提交的版本目录已存在时 exit 0
pwsh -File tools/scripts/publish_images.ps1 -Version v1.2.3 -Registry <registry> -SkipBuild -SkipIfExists
# 本机没有 Linux staging 时只发 Go 与 Java(快照轨同理,去掉 -Version / -Registry)
pwsh -File tools/scripts/publish_images.ps1 -Version v1.2.3 -Registry <registry> -Families go,java

# (可选)推送并把 digest 合并写进同一个 JSON。约束:
#  - <registry> 与出包时的 -Registry 完全一致(cpp 族对应 -ImageRepository <registry>/mmorpg-node):
#    push 只推本地已有的同名 ref、不重新打 tag;make_release.ps1 要求 digest 记录与 manifest 的 ref 逐一对应;
#  - 在同一提交的干净检出上执行,只推出包时实际发布了的镜像族;
#  - 记录文件放仓库外:放在仓库内就是未跟踪文件,git status 变脏,下一条带 -Version 的命令会以"不接受脏工作树"拒绝。
$digests = Join-Path (Split-Path $PWD.Path -Parent) 'image-digests-v1.2.3.json'   # 当前目录 = 仓库根
pwsh -File tools/scripts/k8s_image.ps1      -Command push-image -ImageRepository <registry>/mmorpg-node -Version v1.2.3 -DigestsOut $digests
pwsh -File tools/scripts/go_svc_image.ps1   -Command push-all   -Registry <registry> -Version v1.2.3 -DigestsOut $digests
pwsh -File tools/scripts/java_svc_image.ps1 -Command push       -Registry <registry> -Version v1.2.3 -DigestsOut $digests

# release manifest:CHANGELOG.md 必须有 "## [1.2.3]" 段 → releases/manifests/v1.2.3.md 与 .json(.json 最后落,是"已发布"哨兵)
# 纯离线交付、没推 registry 时省略 -ImageDigestsFile
pwsh -File tools/scripts/make_release.ps1 -Version v1.2.3 -ImageDigestsFile $digests

# 目标机:先校验再落地(缺省 deploy/offline-images/v1.2.3,不入库,根 .dockerignore 已排除)→ 离线导入
pwsh -File tools/scripts/fetch_images.ps1 -Channel release -Version v1.2.3
pwsh -File tools/scripts/import_images.ps1 -Dir deploy/offline-images/v1.2.3

# 快照清理:默认 dry-run,加 -Force 才删;releases/ 永不触碰
pwsh -File tools/scripts/artifacts_retention.ps1 -KeepLast 10
```

## Ops Recommendation

- Config format: prefer YAML for daily operations. It is the normal Kubernetes ops format and easier to review during change windows.
- Keep JSON only for tool interoperability or automated generators.
- External gate exposure:
  - Managed cloud K8s: prefer `-GateServiceType LoadBalancer`.
  - Self-hosted / bare metal K8s: prefer `-GateServiceType NodePort` behind an external L4 load balancer.
  - Exposing the Service is only half of it, and the missing half is **not** in this script: clients never learn gate's address from the Service. `login` reads gate's etcd-registered endpoint — the cluster-internal `POD_IP` — and hands it to the client verbatim (`go/login/internal/svc/servicecontext.go`, `CandidatesForZone`). Until `login` is taught to translate `node_id` to an externally reachable address, external clients cannot connect no matter how the Service is configured. The deploy script warns on any non-`ClusterIP` setting for this reason.
  - The gate process deliberately does not try to discover its own external address. It binds `0.0.0.0` and registers its pod identity; deciding what the outside world should dial is the job of whoever hands the address out.
  - **(2026-09-29 更正:上面两条已被集群外入口 D76–D93 取代,原文保留作历史)** 最终没有采用"login 按 `node_id` 翻译地址"(D76 否决的方案④),而是**进程自报客户端可达地址**:`-ClientEntryMode external` 下 gate / battle 在发布 etcd 之前把 `NodeInfo.client_endpoint`(`proto/common/base/common.proto:33`)填好,login / scene_manager / battle 三个出口按同一规则选地址(Go `go/shared/clientendpoint/select.go:41` `Select`,C++ `client_endpoint::ClientFacing`)。`endpoint` 仍是集群内身份,只在 `podip` 模式下被当成客户端地址下发 —— 所以**上面那段"Service 怎么配都连不上"的结论对默认的 `podip` 仍然成立**,集群外玩家必须用 external。external 下 gate 是 StatefulSet + 每序号 Service `gate-<i>`,见下方 Optional Flags「集群外客户端入口」与 `docs/design/k8s_gate_exposure_guidance.md`;权威设计见 `docs/design/k8s-client-entry.md`(D76–D93、「运维手册」、「kind 端到端验证」)。**未上集群验证**,待 Codex / 用户按 kind 端到端验收。
- Do not treat `LoadBalancer` as the universal default. If the cluster does not have a mature, production-grade LB implementation, `NodePort` plus an external L4 balancer is usually the more stable choice.
- Internal-only services should stay inside the cluster and do not need external exposure.
- Stability baseline per zone: `centre=1`, `gate=2`, `scene=4`.
- 日志采集的常驻开销(默认开着 sidecar 时要算进容量):
  - 每个 C++ Pod 多一个 sidecar 容器,`requests` `20m` / `64Mi`、`limits` `256Mi`。按上面那条基线,一个 zone 是 `gate=2` + `scene=4` 共 6 份,再加不分 zone 的 battle 池。
    - "6 份"只对**默认编排 + legacy 单 scene 池**成立(`-SceneOrchestrator deployment` + `-SceneReplicas`)。`-SceneOrchestrator agones` 下 scene 那部分按 **Fleet 当前副本数**算(开了 `-AgonesAutoscale` 还会随房间容量上下浮动,不是一个固定数);scene 角色拆分档(`-SceneWorldReplicas` / `-SceneInstanceReplicas`,见 `docs/ops/scene-node-role-split.md`)会把 scene 拆成 `scene-world` + `scene-instance` 两个池,份数按两池之和算。
  - infra 侧多一个 Loki,`requests` `100m` / `256Mi`、`limits` `1Gi`,外挂 `10Gi` PVC。
  - 资源紧张时两条路:`-NoCppLogSidecar` 整个关掉(代价是 C++ 业务日志无人采集),或 `-LokiPushUrl` 指向集群外已有的 Loki(此时 `infra-up` 不再部署集群内 Loki,只剩 sidecar 那份开销)。
- You can encode this choice directly with `-OpsProfile managed-cloud` or `-OpsProfile bare-metal`.

### Open One Zone

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName yesterday `
  -ZoneId 101 `
  -OpsProfile managed-cloud `
  -WaitReady `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:latest
```

### Check One Zone

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-status -ZoneName yesterday
```

### Close One Zone

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName yesterday
```

## Open All Zones (One-Click)

1. Copy either `deploy/k8s/zones.sample.json` to `deploy/k8s/zones.json`, or
  `deploy/k8s/zones.sample.yaml` to `deploy/k8s/zones.yaml`.
2. Edit zone names, IDs, and replica counts.
3. Run:

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-up `
  -ZonesConfigPath deploy/k8s/zones.yaml `
  -OpsProfile managed-cloud `
  -NodeImage ghcr.io/luyuancpp/mmorpg-node:latest
```

### Check All Zones

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-status -ZonesConfigPath deploy/k8s/zones.yaml
```

### Close All Zones

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-all-down -ZonesConfigPath deploy/k8s/zones.yaml
```

## Runtime Behavior

- Namespace naming: `<NamespacePrefix>-<ZoneName>` (default prefix is `mmorpg-zone`).
- Node config (`base_deploy_config.yaml`, `game_config.yaml`) is generated into a `ConfigMap` per zone.
- `ClusterId` (from `-ClusterId`, default `0`) is written into that ConfigMap's `base_deploy_config.yaml` and into the `login` / `scene-manager` / `match` Go ConfigMaps. It is the high 5 bits of the snowflake worker field (`docs/design/node-id-overhaul-plan-20260908.md` §5): one value per Kubernetes cluster, set once by ops, never per zone.
- Node pods use:
  - `POD_IP` from Kubernetes Downward API.
  - `RPC_PORT`/`NODE_PORT` env vars (fixed per role: gate `18000`, scene `20000`). The value must fall inside the engine's per-role TCP range — gate `10000-19999`, everything else `20000-35535` — since the gRPC port is derived as TCP+30000. A node now refuses to start (retrying, nothing published to etcd) if the requested port is taken, rather than silently picking a different one.
  - `GRPC_SERVER_MAX_POLLERS` **已退役**(`docs/design/grpc-muduo-transport.md` §15.11):第 4 批起 scene / battle 的 gRPC 服务端跑在节点 EventLoop 上(muduo + nghttp2),没有 poller 线程,进程不再读这个变量(设了只打一行 WARN)。Deployment、Fleet 与客户端入口生成器都不再注入它。并发上限改由服务端内建:每连接 `MAX_CONCURRENT_STREAMS=256`、全节点在途 1024,超限回 `RESOURCE_EXHAUSTED`。
  - scene / battle 的 readinessProbe 探 gRPC 口(TCP+30000):新服务端对不发 HTTP/2 preface 的探针连接静默处理(不写字节、只打 DEBUG、计 `closed_before_preface`);停机排空期间监听已关,探针失败属预期。
- **C++ 业务日志不在容器 stdout**:Linux 下 muduo 的 `LogToConsole` 那一句包在 `#ifdef WIN32` 里,业务日志只落 `/app/bin/logs/cpp_nodes/<节点>.*.log`。`kubectl logs` 对 gate / scene / battle 只看得到启动行和 gRPC / librdkafka 写在 stderr 上的东西 —— 查不到业务日志不是没打,是采集口不同。默认的 Alloy sidecar 正是为此读文件送 Loki(见 Optional Flags 的 `-NoCppLogSidecar`)。
  - 这个目录**不会无限长**:muduo 每 8MiB 滚一个新文件且从不删旧文件,所以业务容器的启动命令里带了一个每 5 分钟跑一次的清理循环,按时间只留最近 8 个 `.log`(≈64MiB);`node-logs` 这个 emptyDir 另有 `sizeLimit: 2Gi` 作远端兜底。`kubectl exec` 进去只看得到最近这几个文件,更早的要去 Loki 查。
  - **采不到的东西**:容器 stdout 上的 gRPC / librdkafka stderr,以及崩溃现场文本(glibc 断言、`terminate called`、abort 栈)——它们只在 `kubectl logs` 里,Pod 一重建就没了。Loki 里的 `level="fatal"` 只覆盖代码里显式 `LOG_FATAL` 的分支。
- A `gate-entry` Service is created per zone namespace for external TCP access.
  **(2026-09-29 更正,D90)** 上面这句已不成立:单一 `gate-entry` 只在 `-ClientEntryMode podip` **且 gate 副本数恰为 1** 时生成
  (`tools/scripts/lib/k8s_client_entry.ps1:509` `Test-GateEntryServiceWanted`,`tools/scripts/k8s_deploy.ps1:4994-4999`)。
  原因:login 签的票据绑 `gate_node_id`,gate 不符即以 `token_gate_node_mismatch` 断开(`cpp/nodes/gate/handler/rpc/client_message_processor.cpp:1052-1057`),
  副本数 ≥2 时 Service 随机分流,约一半票据会被拒。其余情形(podip 多副本、external)不生成,并在删旧形态时清掉残留
  (`lib/k8s_client_entry.ps1:1894-1895`、`:1908-1910`)。而且 podip 下 login 下发的是 PodIP,`gate-entry` 本身并不能让集群外客户端连上;
  集群外入口一律走 external 的每序号 Service `gate-<i>`。`gate-entry` 的 spec 也没有 `externalTrafficPolicy`(`k8s_deploy.ps1:2405-2424`,默认 Cluster,会 SNAT)。

## Optional Flags

- `-SkipInfra`: deploy only node workloads。跳过的是**整个基础设施阶段**(`Apply-Infra`),不只是最早那三个:`etcd` / `redis` / `redis-match-cluster` / `kafka` / `mysql`,加上**条件部署**的 `loki`(只在日志 sidecar 开着且没给 `-LokiPushUrl` 时才会 apply),以及 `kafka-topic-init` Job、不分 zone 的 battle 池和全局池服务(`match` / `trade`)。只对 `all-up` / `all-down` 有意义(`zone-up` 本来就不碰 infra)。**代价**:跳过 infra = 集群内 Loki 不会被部署,而 C++ Pod 上的日志 sidecar 照装不误 —— 于是 sidecar 有配置却没有推送目标,见下面 `-LokiPushUrl` 后面那条"半配置状态"。
- `-ClusterId`: deployment-level cluster number, `0..31`, default `0`. Becomes the `cluster` segment of every snowflake id minted in this cluster, so `infra-up` and every `zone-up` / `all-up` against the same cluster must pass the same value (the script reads one parameter for both paths; `zones.json` deliberately has no per-zone override). Leave it at `0` unless you are standing up a second, independent cluster that must never collide with the first. Only `tools/scripts/k8s_deploy.ps1` accepts it today; `dev_tools.ps1`'s `k8s-*` wrappers do not forward it yet.
- `-DryRun`: print kubectl commands without applying.
- `-WaitReady`: wait for `centre` / `gate` / `scene` deployments to roll out. On `infra-up` / `all-up` it also waits for the `etcd` StatefulSet to reach quorum before the global `match` service is applied, and then for the `kafka-topic-init` Job to complete, so the audit topics exist with their contracted partition counts before any zone is applied (see "Kafka 审计 topic 预建" below). `infra-kafka-topics -WaitReady` waits for the same Job. For the global `trade` service on the `dev` profile it also waits for `deploy/mysql` and then for the `trade-migrate` Job to reach Complete before the `trade` Deployment is applied; a Failed Job (or a timeout) aborts the release. On `staging` / `prod` that migrate-Job gate always runs, with or without `-WaitReady` (port-decisions D-14 item 4; see "聚宝斋 trade" below).
- `-WaitTimeoutSeconds`: rollout wait timeout per deployment (default `180`). It is also the base budget of the two `trade-migrate` waits: the pre-delete check that waits for a previous in-flight Job to finish (runs on every profile and waits until the Job has a completion/failure condition and every Pod owned by its UID is Succeeded or Failed; terminating Pods and unknown/query-failure states never permit deletion), and the poll of the new Job for Complete / Failed (always on `staging` / `prod`, on `dev` only with `-WaitReady`). Each of those two waits uses `max(-WaitTimeoutSeconds, 300)` (`$GoSvcMigrateJobMinWaitSeconds` in the script), because a normal migrate Pod needs scheduling + image pull + the `wait-mysql` initContainer (up to 150 + 8 s) + the migration itself. The `deploy/mysql` rollout wait in front of the Job still uses `-WaitTimeoutSeconds` alone, so for the first `infra-up` on a fresh cluster pass a larger value (e.g. `600`); otherwise a slow MySQL pull / initdb is reported as a timeout and the release stops (re-running the same command resumes: an in-flight Job is waited on, never deleted).
- `-KubeContext`: pass an explicit kube context.
- `-KubeConfig`: pass an explicit kubeconfig path.
- `-NamespacePrefix`: change namespace prefix.
- `-CentreReplicas`, `-GateReplicas`, `-SceneReplicas`: per-zone replica overrides for `k8s-zone-up`.
- `-GrpcServerMaxPollers`:**已退役**,默认 `0`。只为兼容旧命令保留:传入大于 `0` 的值只打一条 `Write-Warning`,取值被忽略,任何清单都不再写 `GRPC_SERVER_MAX_POLLERS`(原因见上面 Runtime Behavior)。
- `-GateServiceType`: `ClusterIP`, `NodePort`, or `LoadBalancer`.
- `-GateServicePort`: external gate service port.
- `-OpsProfile`: `managed-cloud`, `bare-metal`, or `custom`.
- With `-OpsProfile custom`, the baseline default is `-GateServiceType NodePort` unless overridden.
- `-GoSvcRegistry`: Docker registry for Go micro-services (e.g. `ghcr.io/luyuancpp`). If not set, Go services are skipped.
- `-GoSvcTag`: Image tag for Go service images (default: `latest`).
- `-SkipGoSvc`: explicitly skip Go service deployment even if `-GoSvcRegistry` is set.
- `-NoCppLogSidecar`: turn the C++ log sidecar off. 默认是**开着**的:每个 C++ Pod(`gate` / `scene` / `battle`,普通 Deployment 与 Agones Fleet 两条路径一样)以 k8s 原生 sidecar 形态多带一个 Alloy 容器 —— 放在 `initContainers` 里并带 `restartPolicy: Always`,只读共享业务容器的 `node-logs` 卷,直接读 `/app/bin/logs/cpp_nodes/*.log` 推 Loki,全程不经过 stdout(为什么必须读文件见 Runtime Behavior 里 C++ 日志那一条)。选原生 sidecar 而不是普通容器,是因为 Agones 给 GameServer Pod 写死 `restartPolicy: Never`(普通容器形态下 sidecar 一 OOM 就永远停着,而 Agones 的健康检查只看游戏容器、不会把 GameServer 置 Unhealthy,日志会在这个 Pod 的余生里静默断流),以及 Pod 终止时 kubelet 会等业务容器退完再停原生 sidecar(scene 那 60 秒 drain 里写的存档 / 踢人日志才追得完)。关掉它就回到"C++ 业务日志无人采集",只给"集群里没有 Loki 也不打算部署"的场景用。
- `-CppLogSidecarImage`: sidecar 镜像,默认 `grafana/alloy:v1.10.0`。**版本钉死是有原因的**:`v1.19.2` 的 `loki.source.file` 有读取位置回归(重启后几乎整文件重读),升级前先跑 `docs/ops/grafana-loki-local-logs.md` §5.3 的读取位置自检。
- `-LokiPushUrl`: sidecar 的推送地址。留空(默认)= 写 infra namespace 里的 Loki,即 `http://loki.<InfraNamespace>:3100/loki/api/v1/push`(`manifests/infra/loki.yaml`,也只有这种情况 `infra-up` 才部署它);指向集群外 / 已有的 Loki 时写完整 push 地址,例如 `http://loki.observability:3100/loki/api/v1/push`。**改它要挑时机**:Alloy 的配置正文哈希以 `mmorpg.io/cpp-log-sidecar-config-hash` 注解打进 pod 模板(只改 ConfigMap 不会让跑着的 Alloy 重读配置,而模板不变时 `kubectl apply` 是 no-op),所以改推送地址或改解析规则 = 下次 `zone-up` 滚动重启该 zone 的 C++ 节点,Agones 侧是 Fleet RollingUpdate 换 GameServer。**别漏了 battle**:battle 是不分 zone 的全局池,由 `Apply-Infra` → `Apply-BattlePool` 部署在 infra namespace(连它那份 sidecar ConfigMap 也建在 infra namespace),`zone-up` 根本不碰它 —— 它要等下一次 `infra-up` / `all-up` 才跟着滚。
- 上面三个参数 `dev_tools.ps1` 与 `k8s_image.ps1` 都已透传(与 `-ClusterId` 不同,不必绕开包装脚本直接调 `k8s_deploy.ps1`)。两个字符串参数在包装脚本里默认留空 = **不覆盖**,不会把空的 `image:` 渲染进清单;镜像默认版本这个事实源只存在于 `tools/scripts/k8s_deploy.ps1` 一处。
- **"sidecar 装上了但推无可推"这个半配置状态要知道**:集群内 Loki 只随 `infra-up` 部署,所以只跑 `zone-up`、跑 `all-up -SkipInfra`、或在存量集群上直接升级这三种走法,都会得到"每个 C++ Pod 里 sidecar 跑得好好的、推送目标压根不存在"的组合。脚本对此**只做只读探测、不阻断**:用默认 Loki(没给 `-LokiPushUrl`)时跑一次 `kubectl -n <InfraNamespace> get svc loki --ignore-not-found`,查不到就 `Write-Warning` 提示去跑 `infra-up` / 给 `-LokiPushUrl` / 用 `-NoCppLogSidecar`,然后照常 apply;`-DryRun` 下整段跳过,`-LokiPushUrl` 指向集群外 Loki 时无从判断也不探。表现是**部署全绿、一条 C++ 日志都查不到**(Alloy 的推送失败只写在它自己的 stdout 里,脚本不会失败)。处置见 `docs/ops/grafana-loki-local-logs.md` §6。
- `-GateRouterMode`(2026-09-29 补登记;权威口径是 `tools/scripts/k8s_deploy.ps1:140-164` 的参数注释,决策 turn-based §22 D75 / port-decisions D-12 修订):
  gate 的客户端 RPC 路由模式,只写进 gate 的环境变量 `GATE_CLIENT_RPC_ROUTER`。**K8s 默认 `"1"`**(路由模式:gate 只连 `client-rpc-router`,
  chat / friend / trade 等只承诺路由模式的服务经它可达);`"0"` = 旧的逐服务直连,只作回退。只收 `"0"` / `"1"`,拼错在入口就拒。
  C++ 进程默认值(`cpp/nodes/gate/gate_router_mode.h`,未设即直连)与 `gate_security_test` 的默认值断言一字不改,翻转只落在部署层。
  两种模式下战斗都只走客户端 ↔ battle 直连,gate 不中继战斗(turn-based §22 D66),本开关与战斗无关。
  - **前置**:路由模式下 gate 的依赖门等 ClientRpcRouter + Scene(不再是 Login)。`infra-up` 必须已把 `client-rpc-router` 部署就绪 ——
    不带 `-SkipGoSvc`、给了 `-GoSvcRegistry`;否则 zone-up 之后 gate 卡在依赖门,登录 / 匹配 / 聊天全部 `no_target`。
    **脚本没有运行时拦截**,只有参数注释。`k8s_image.ps1` 的 `release-zone` / `release-all` 从不传 `-GoSvcRegistry`、不部署任何 Go 服务,
    全新集群只走发布路径时同样会卡住 —— 先跑一次带 Go 服务的 `infra-up`。
  - **回退 `"0"` 的后果**:chat / friend / trade 及所有只承诺路由模式的服务**同时**不可达;gate 已不再中继战斗(D66),没有任何兜底。
    回退前先用 killswitch 关掉这些方法并公告(D-12)。
  - **回退态不粘滞**:本值每次部署都原样重写进 gate env,脚本不从集群读回旧值。以 `"0"` 回退运行的 zone,之后**每一次**重新部署
    (日常 `zone-up` / `release-zone`、合服后的 `zone-up`、`k8s_zone_rollback.ps1` 的 Step 6,以及经 `dev_tools.ps1` / `k8s_image.ps1` 的入口)
    都必须显式再传 `-GateRouterMode 0`,否则静默落回 `"1"`;回退的原因(如路由服不可用)若仍在,gate 就卡在依赖门。
    只有回滚脚本在 `-Apply` 且留空时有第 0 步预检拦截(`docs/design/zone_data_rollback.md`「## 3. 整 Zone 灾难恢复级回档」下的「2026-09-29 修订」),其余入口没有。
  - **验证状态**:K8s 上从未以路由模式跑过含 gate 的链路(09-15 kind 隔离环境只验过 router→chat,见 `docs/design/microservice-zone-contract-20260914.md` §16)。
    "K8s 上以路由模式跑通一次 battle-smoke"已由用户豁免为**事后补验**(D65 / D75)。补验之前,不能声称 K8s 默认部署下登录、匹配、聊天可用。
- **集群外客户端入口**(D76–D93;设计 `docs/design/k8s-client-entry.md`,参数速查见其「运维手册」;权威参数注释 `tools/scripts/k8s_deploy.ps1:166-233`,
  生成器、preflight 与就绪等待在 `tools/scripts/lib/k8s_client_entry.ps1`)。以下全部**未上集群**,待 kind 端到端验证:
  - `-ClientEntryMode podip|external`,默认 `podip`。podip:gate / battle 通告 PodIP,只有集群内 robot / 压测连得上。
    external:gate 为 StatefulSet(`podManagementPolicy: Parallel`、`updateStrategy: OnDelete`、PDB `maxUnavailable: 0`)+ 每序号 Service `gate-<i>`,
    gate 在发布 etcd 之前自报 `NodeInfo.client_endpoint`,login / scene_manager 下发它;battle 的形态由 `-BattleOrchestrator` 决定。
  - `-BattleOrchestrator deployment|agones`,默认 `deployment`,只对 `infra-up` / `all-up`(不带 `-SkipInfra`)生效。
    external + agones = battle Agones Fleet(`portPolicy: Dynamic`,生产推荐);external + deployment = battle hostPort 20000,
    每节点 1 个、无忙碌保护,只用于验证与回退(D86,preflight 警告)。与 `-SceneOrchestrator` 相互独立。
  - `-ClientPublicHost`:gate 与 battle 共用的客户端可达主机(裸主机名或 IPv4,不带 scheme / 端口;kind 填 `127.0.0.1`)。
    `-GateClientHostTemplate`:如 `gate-{ordinal}.{zone}.example.com`,优先级高于 `-ClientPublicHost`;`-GateServiceType LoadBalancer` 时必填,
    gate 副本数 >1 时必须含 `{ordinal}`。两者都留空时 gate 取节点 `status.hostIP`。
    多节点 + NodePort + 默认 `Local` 时**不要只给 `-ClientPublicHost`**:所有序号自报同一主机,Pod 不在该主机所在节点的 gate 流量会被丢弃、不可达,
    preflight 不告警;改用 `{ordinal}` DNS 模板或 LoadBalancer(D89,`docs/design/gate-connection-admission-control.md` §4.2.1)。单节点(kind)不受影响。
  - `-GateNodePortBase`(默认 `30000`):external + NodePort 时 `gate-<i>` 的 nodePort = base + i,gate 自报同一个端口;只给单 zone 的 `zone-up` 用。
  - `-GateExternalTrafficPolicy Local|Cluster`(默认 `Local`,D89):保留玩家真实源 IP;`Cluster` 会 SNAT,gate 按源限流(G9)不可用,preflight 警告。
  - `-RequireClientEndpoint auto|true|false`(默认 `auto` = external → true、podip → false):login / scene_manager 下发 gate 地址时是否必须用自报地址。
    逐个 zone 切 external 的窗口里,已切 zone 显式传 `false`(否则跨 zone `RedirectToGate` 会跳过未切 zone 的 gate),全部切完再回 `auto`(`k8s_deploy.ps1:180-187`)。
    podip 配 `true` 在任何写操作之前拒绝。
  - `-GatewayIngressHost` / `-GatewayIngressClassName`(默认 `nginx`)/ `-GatewayIngressTlsSecret` / `-GatewayTrustedProxies`(逗号分隔 CIDR):
    gateway Ingress **由 `zone-up` 随 gateway 生成**(只在本次确实部署 Java 服务时),只路由 `/api`;`infra-up` 上给只会得到"被忽略"警告。
    配了 host 就必须给 trusted proxies(preflight 报错,否则全体玩家共用一个限流桶,D91)。留空 host 不生成、也**不删除**已有 Ingress。
  - `-LoginDevPasswordAuth`(switch):集群内 login 的开发口令认证,**只允许 `-ReleaseProfile dev`**;共享口令读环境变量
    `MMORPG_LOGIN_DEV_PASSWORD_SHARED_SECRET`,经 Secret `login-dev-password` 注入,绝不进 ConfigMap。不传 = login 没有任何口令认证配置(fail-closed)。
  - `-AllowDisruptiveSwitch`(switch):确认本次就是要切换形态或改客户端地址来源(整台 gate 踢人、在打的战斗作废)。
    不给时,只要会踢人的另一种形态在集群里存在,或现有 gate / battle 的入口形态与地址来源会被改写,部署就在任何写操作之前整条拒绝。只在维护窗口里显式加。
  - `-ConfirmRedisCoreRestart`(switch,2026-10-15 P0-E):确认本次 `infra-up` / 不带 `-SkipInfra` 的 `all-up` 可以重启共享 Redis(redis-0,全服登录中断并重放 AOF)。
    `redis.yaml` 的 Pod 模板有变化(或线上 `sts/redis` 还没有模板摘要注解)而不给时,在 `Apply-Infra` 的任何写操作之前拒绝。`dev_tools.ps1` / `k8s_image.ps1`
    只在被指定时透传。**本项合入后第一次 `infra-up` 必须带它**,见「Redis 连接上限与总账」。
  - **模式参数不粘滞**:每次部署按本次取值生成,漏传就落回默认 `podip` / `deployment`;以 external 运行的环境每次重跑都要照传同一组值
    (漏传时由上一条的集群现状核对在写操作之前拒绝)。
  - **上线口径**:集群外部署须与 `-GateRouterMode 1` 在同一窗口启用 `-ClientEntryMode external`(`k8s_deploy.ps1:176`),不再按旧计划"第 4 批才翻 GateRouterMode"。
    只翻路由模式而不开 external,集群外玩家仍连不上 gate 与 battle(两者都通告 PodIP)。
  - **zones 配置的 `gateNodePortBase`**(D88):external + NodePort 时每个 zone 一个;`all-up` 多 zone 时每个 zone 必须显式写(不从 zoneId 推导),
    各段 `[base, base + gate 副本数 − 1]` 不能重叠,建议收在 K8s 静态子段 `30000–30085`(超出只警告,越过 `32767` 拒绝)。
    YAML 里该键**单独占一行、行尾不写注释**(脚本的 YAML 回退解析读不到带行尾注释的行)。示例:`zones.sample.{json,yaml}`(30000 / 30010)、
    `zones.10zones.yaml`(30000–30072,步长 8)。podip 模式不读它。
  - **gate 滚动与排空**:external 的 gate 是 `OnDelete`,滚动一律走 `tools/scripts/k8s_gate_drain.ps1`(标 draining → 等 drained → `-DeletePod`);
    kind 上带 `-KubeContext kind-mmorpg -KubeConfig "$env:TEMP\kind-mmorpg.kubeconfig"`(与下方 kind 小节同一口径;脚本两个参数都支持,`k8s_gate_drain.ps1:91-92`、`:159-160`)。PDB `maxUnavailable: 0` 会挡住 `kubectl drain` 与 cluster-autoscaler,维护节点前先按此排空。
- **包装入口的透传口径**(2026-09-29,ingress 2b / 2c / 2d 批 WP12 与对齐包):
  - `dev_tools.ps1` 与 `k8s_image.ps1` 的 `-GateRouterMode`、`-GateServiceType` 与全部集群外入口参数一律**默认留空 = 不覆盖**,非空才透传
    (`-GateNodePortBase` 用 `-1` 表示未指定);默认值只留在 `k8s_deploy.ps1` 一处。本文「好友 friend」一节里"`dev_tools.ps1` 不透传 `-GateRouterMode`"的说法已作废。
  - `dev_tools.ps1` 的 `k8s-infra-up` / `k8s-zone-up` / `k8s-all-up` 透传全部参数。`k8s-zone-rollback` 转发 `-GateRouterMode`、gate / Ingress 这组、
    `-GateServiceType`、`-RequireClientEndpoint`、`-LoginDevPasswordAuth`、`-AllowDisruptiveSwitch`、
    `-RollbackConfirmNoGatewayIngress`(转为回滚脚本的 `-ConfirmNoGatewayIngress`,`dev_tools.ps1:1602`;namespace 已删、第 0 步要求对 Ingress 表态时用,
    见 `docs/design/zone_data_rollback.md`「## 3. 整 Zone 灾难恢复级回档」下的「2026-09-29 修订」)、显式给的 `-ReleaseProfile` 与 `-KubeContext` / `-KubeConfig`,
    **不转发 `-BattleOrchestrator`**(zone 回滚不碰 battle 全局池)。以上为主要转发项,完整清单以 `dev_tools.ps1:1592-1617` 为准。
  - `k8s_image.ps1`(以及 `dev_tools.ps1` 的 `k8s-image-*` / `k8s-release-*`)只透传 gate / battle 侧 6 个参数(`-ClientEntryMode`、`-ClientPublicHost`、
    `-GateClientHostTemplate`、`-GateNodePortBase`、`-GateExternalTrafficPolicy`、`-BattleOrchestrator`),外加 `-GateRouterMode`、`-AllowDisruptiveSwitch`、
    `-RequireClientEndpoint`(经这条路只参与预检、不改写 login / scene_manager 的 ConfigMap,给了在入口打告警)。
    `-Gateway*` 与 `-LoginDevPasswordAuth` 经 `k8s-image-*` / `k8s-release-*` **显式给了即报错**(`dev_tools.ps1:791-801`):`k8s_image.ps1` 不部署 Go / Java 服务,
    要用请走 `k8s-zone-up` / `k8s-all-up` 或直接调 `k8s_deploy.ps1`。`-BattleOrchestrator` 经这条路只对 `release-all`(不带 `-SkipInfra`)生效。
  - `-GateServiceType` 默认值统一为留空、跟随 `k8s_deploy.ps1`。行为变化只有一处:经 `k8s_image.ps1`、`-OpsProfile custom`、没显式给 `-GateServiceType` 时,
    由原来的 `LoadBalancer` 变为 `NodePort`(managed-cloud / bare-metal 两档由 `Apply-OpsProfileDefaults` 强制,不变)。`k8s_image.ps1` 默认
    `-OpsProfile managed-cloud`(强制 LoadBalancer),所以经它发布 external 时必须给 `-GateClientHostTemplate`,否则被 preflight 拒绝。
  - 存量缺陷已修:`dev_tools.ps1` 的 `k8s-image-preflight` / `k8s-build-image` / `k8s-push-image` / `k8s-release-zone` / `k8s-release-all` 自提交 `31e4d1d4c`
    起因多传 4 个 Agones 参数在参数绑定阶段就失败,现已去掉并对 `-AgonesHighDensity` / `-AgonesAutoscale` 显式拒绝。这条链很久没人跑通,首次使用先加 `-DryRun`。
  - 以上透传与拒绝**未运行**,契约测试 `tools/scripts/tests/k8s_client_entry_contract.tests.ps1`、`k8s_deploy_contract.tests.ps1`、
    `k8s_zone_rollback_gate_router_mode.tests.ps1` 待 Codex 验证。
- **削峰(G4b):`-GatewayRateLimit` / `-LoginQueue`**(2026-10-14;设计 `docs/design/open-server-rate-limit-design.md` §十一,规格
  `docs/handoff/launch-readiness-20261013/spec_G4b.md`;权威口径是 `tools/scripts/k8s_deploy.ps1` 的参数注释与常量表 `$script:PeakShavingLaunchTier`)。
  状态:已落码,**未编译、未测试**,待 Codex 验证。下面的阈值全部是开服档起步值,**待压测确认**(L-14),不能据此下容量结论(AGENTS §6)。
  - 两个参数都取 `auto|on|off`,缺省 `auto` = 跟随 `-ReleaseProfile`:dev 关(ConfigMap 与 G4b 之前逐字相同),staging / prod 开。
    `off` 只用于事故止血与基线压测;`on` 只用于 dev 档联调。取值拼错在参数绑定时就失败。
    - `-GatewayRateLimit`(gateway Bucket4j 限流):开时 gateway ConfigMap 的 `gate.rate-limit` 下写 `enabled: true`、`zone-default-rps: 2000`、
      `zone-default-burst: 5000`、`ip-rps: 5`、`ip-burst: 20`、`account-cooldown-ms: 5000`,与 `trusted-proxies` 共用同一个 `rate-limit:` 键;
      不写 `wave.*`(开服波次要一个开服时刻,属运营决策)、`zone-overrides` 和没有消费方的 `queue-timeout-ms`。关时不写,落回 jar 内的 `enabled: false`。
    - `-LoginQueue`(login 登录排队):开时 login ConfigMap 写 `Queue` 段:`Enabled: true`;`DispatcherLockKey` = login.yaml 的值加 `:zone:<本 zone>`(见下面 ②);
      `ZoneCapacityOverride` 只含本 zone 一项 `5000`,这是**本 zone 同时在线上限,不是速率**;其余键(`DispatchInterval` / `AdmitTTL` / `QueueEntryTTL` /
      `SoftCapMultiplier` / `DefaultRetryAfterMs` / `DispatcherLockTTL`)从 `go/login/etc/login.yaml` 镜像。`Secrets` 下另写 `QueueToken.Value`(见下一条)。
    - 与开关无关:gateway ConfigMap 的 `spring.data.redis` 在 `MMORPG_REDIS_PASSWORD` 非空时写 `password`(staging / prod 必须设该变量;dev 档没设时回落空串,不写)。
      Bucket4j 的 Lettuce 客户端只从这个属性取口令,而 release 档的 Redis 带 `--requirepass`;不写口令时限流过不了 Redis 认证,要么每次调用都 fail-open 放行,
      要么 gateway 建连接时就起不来(取决于 Lettuce 的握手行为,未实测)。口令与 go-svc ConfigMap 一样是明文。
  - **登录排队的合入门槛(2026-10-15 更正注:上面「staging / prod 开」目前只对 `-GatewayRateLimit` 成立)**。G4b-2 的 login 段已先于合入门槛
    随 `e4ff1d165` / `ebccaf04d` 入库(越序),`k8s_deploy.ps1` 用脚本常量 `$script:LoginQueueMergeGateCleared`(当前 `$false`)机械拦截:
    - 三项门槛(`docs/handoff/launch-readiness-20261013/spec_G4b.md` §7「合入门槛」):① G1b 规格指定的多 zone 用例(L-01 验收:每个 zone 的队列都能出队)
      由 Codex 跑绿;② G4b-G 由 Codex 跑绿;③ `spec_G4b-G.md` §8.4 的真实 Redis 最小冒烟跑通,命令原文与输出摘要记进 `docs/PROGRESS.md`。
    - 门槛未清时:`-LoginQueue auto` 在任何档位都解析为关(写路径打印是 `login_queue=off(-LoginQueue auto)`,下文计划行示例里的 `login_queue=on` 要等门槛清掉),
      本次部署 login 的非 dev 档另打 `[PeakShaving]LoginQueueGate=closed` 警告;`-LoginQueue on` 在参数解析后、任何 kubectl 之前拒绝(`*-down` / `*-status` 带它也拒绝)。
      login ConfigMap 不写 `Queue` / `Secrets.QueueToken`,`MMORPG_LOGIN_QUEUE_TOKEN_SECRET` 也不要求。`-GatewayRateLimit` 不受影响。
    - 放开:三项都满足、`docs/PROGRESS.md` 有记录之后,由负责人在同一提交里把常量改为 `$true`,并在常量注释里写上那条 PROGRESS 条目的标题;
      契约测试 `k8s_deploy_contract.tests.ps1` 里断「门槛未清」产物的两例(用例 19 / 20)随之改写。
    - 门槛未清时手改 login ConfigMap 或 `go/login/etc/login.yaml` 的 `Queue.Enabled` 同样违规(文档约束,脚本拦不住)。清门槛本身要跑的真实 Redis 冒烟
      用本地栈 + scratchpad 里的 login 配置副本,不经过 `k8s_deploy.ps1`,不受这道门禁影响。
    - 契约测试用的环境变量 `MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED=1` 只在 `-DryRun` 下视为门槛已清;不带 `-DryRun` 时只要设置了它,脚本就在任何写操作之前拒绝。
      运维机器上不要设它。
  - **环境变量 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`**:login 排队令牌的 HMAC 密钥,写进 login ConfigMap 的 `Secrets.QueueToken.Value`(与兄弟项 `InternalAuth` 同为明文)。
    staging / prod、排队开、且本次部署 login(`zone-up` / `all-up`,给了 `-GoSvcRegistry`、不带 `-SkipGoSvc`)时**必填**:非空、不是占位串、至少 32 位,
    并且与 `MMORPG_GATE_TOKEN_SECRET`、`MMORPG_INTERNAL_AUTH_SECRET` 都不同(原样比较,不 trim);不满足即在任何写操作之前拒绝,报错只点变量名、不回显值。
    `-LoginQueue off`、`infra-up`、`-SkipGoSvc` 都不要求它。dev 档可以不设:不写 `Secrets.QueueToken`,login 宽松档回落 GateToken 密钥并打 WARN;
    生产模式开了排队却没有合格的 QueueToken,login 拒绝启动(`go/login/internal/config/secrets.go`)。
    - 用随机串:login 的占位判定按前缀(以 `secret`、`example`、`test-secret`、`change-me`、`local-dev` 等开头一律算占位,`secrets.go` 的 `placeholderPrefixes`),
      比部署侧的精确匹配严,撞上前缀的值能过部署检查、login 却 CrashLoopBackOff。值里不要有 `"` 和 `\`(会破坏生成的 YAML)。
    - 轮换会自动滚动 login(见下),在排玩家手里的 token 随之失效、重新入队,排位丢失;只在维护窗口轮换。
  - **写路径打印**:`zone-up` / `all-up` / `infra-up` 打一行(`-DryRun` 也打,`*-down` / `*-status` 不打),排在密钥解析之前:
    ```text
    PeakShaving: gateway_rate_limit=on(-GatewayRateLimit auto) login_queue=on(-LoginQueue auto) profile=prod initial values (pending stress-test validation) zone=2000/s burst=5000 ip=5/s burst=20 cooldown=5000ms queue_capacity=5000/zone
    ```
    常量表被改坏时 throw `内部错误:…`:限流阈值(rps < 1、burst < rps、冷却为负,本次部署 gateway 且限流开时)与排队容量(不在 1..uint32,
    本次部署 login 且排队开时)都在这一行之后、任何写操作之前就查;`go/login/etc/login.yaml` 缺 `Queue.*` 镜像键也在这里 throw(`生成 ConfigMap 失败:…`)。
    另有三条英文 WARNING,只告警、不阻断,只在本次确实部署对应服务时打;开头都是不含空格的锚点(控制台可能在空格处折行,按锚点 grep):
    - `[PeakShaving]GatewayRateLimit=off`:非 dev 档关了限流;
    - `[PeakShaving]LoginQueue=off`:非 dev 档关了排队;
    - `[PeakShaving]LoginQueueGate=closed`(2026-10-15 新增,上文「三条」更正为四条,四个锚点互不为前缀):非 dev 档按档位本该开排队,
      却因合入门槛未清被关(见上面「登录排队的合入门槛」)。这时不再打上一条:它说的「不粘滞、下次部署恢复」在门槛未清时不成立;显式 `-LoginQueue off` 仍打上一条;
    - `[PeakShaving]GatewayTrustedProxies=unset`:限流开着却没给 `-GatewayTrustedProxies`。IP 桶按 socket 对端分:集群内直连的 robot 按 Pod IP 共用一个桶(5/s、突发 20),
      前面若有未登记的 L7 代理,全体玩家共用一个桶。已有 Ingress 却不给 trusted proxies 的情形由入口预检直接拒绝,不走这条警告。
  - **参数不粘滞**(与 `-GateRouterMode` 同一口径):每次部署按本次取值生成,不从集群读回。以 `off` 止血之后,下一次部署 —— 日常 `zone-up`、合服后的 `zone-up`、
    `k8s_zone_rollback.ps1` 的 Step 6、经 `dev_tools.ps1` / `k8s_image.ps1` 的入口 —— 不传就按档位恢复为开。
    (2026-10-15 更正,上句原文保留:合入门槛未清时这一条对 login 排队不适用,不传 `-LoginQueue` 在任何档位都是关,见上面「登录排队的合入门槛」。)
  - **切换自动滚动**:开着时 gateway 与 login 的 Pod 模板各带一条注解 `mmorpg.io/peak-shaving-hash`(gateway 的指纹输入 = 整段 `rate-limit` 文本 + Redis 口令,
    排在管理面口令指纹 `mmorpg.io/gateway-admin-api-key-hash` 之后;login 的 = `Queue` 段 + 排队密钥),关时注解消失。开关、阈值、可信代理网段、Redis 口令、
    排队密钥任一变化 → 模板变 → apply 即滚动,不需要手工 `rollout restart`(ConfigMap 只在进程启动时读,靠的就是这条注解)。
    既有缺口:只改 Redis 口令时 login 不滚动(go-svc 的 ConfigMap 都没有内容哈希)。
  - **不要用 `kubectl rollout undo` 关排队或关限流**:它只退 Pod 模板(镜像与注解),不退 ConfigMap,排队 / 限流照旧开着。止血一律走下一条。
  - **① 止血要直接调 `k8s_deploy.ps1`**:`dev_tools.ps1`、`k8s_image.ps1`、`k8s_zone_rollback.ps1` 都**不透传**这两个参数(它们透传 `-ReleaseProfile`,`auto` 随档位开启),
    事故时在包装入口上找不到参数。`zone-up` 会按本次参数重写该 zone 的全部工作负载,所以除了加上的 `off`,其余参数(模式、地址、Ingress、档位、镜像 tag)必须与该 zone
    上一次部署一致;镜像 tag 留空 = 当前工作树的 git 短 sha,不一定是线上那一版,要显式给。环境变量照常设好(`MMORPG_REDIS_PASSWORD` 等;关排队时不要求排队密钥):
    ```powershell
    # 先加 -DryRun 跑一遍,核对 "PeakShaving:" 那一行是 off、渲染无误,再去掉 -DryRun 执行。只关一项就只给那一项。
    pwsh -File tools/scripts/k8s_deploy.ps1 -Command zone-up -ZoneName <zone> -ZoneId <id> -ReleaseProfile prod `
      -NodeImage <线上 node 镜像> -GoSvcRegistry <registry> -GoSvcTag <线上 tag> -JavaSvcRegistry <registry> -JavaSvcTag <线上 tag> `
      <该 zone 上一次部署的其余参数照传> `
      -GatewayRateLimit off -LoginQueue off
    ```
    关排队后,在排玩家轮询 `/api/queue-status` 拿到 EXPIRED(gateway 回 410),丢掉 token 重走 assign-gate 快速通道。是否给包装入口补透传待拍板(规格 §10 #9)。
  - **② K8s 上实际的排队 leader 锁键是 `dispatcher:lock:login_queue:zone:<id>:zone:<id>`**:ConfigMap 写的 `DispatcherLockKey` 已带本 zone,而 login(G1b 之后)把配置值当前缀,
    再拼一次 `:zone:<Node.ZoneId>`。每个 zone 仍恰好一个 leader,选主日志是 `[loginqueue] elected dispatcher leader zone=<id> key=dispatcher:lock:login_queue:zone:<id>:zone:<id>`。
    排查时按这个键查;`docs/ops/login-queue-stress-runbook.md` 与 `docs/design/login-queue-2026-05.md` 里的 `dispatcher:lock:login_queue:zone:<id>` 是本地 / compose 默认配置下的键名,
    在 K8s 上不适用。显式带 zone 是纵深防御:`kubectl rollout undo` 把 login 退回 G1b 之前的镜像、ConfigMap 仍开着排队时,旧镜像原样使用配置值,仍按 zone 隔离。
  - **③ 对已在运行的 staging / prod zone 首次打开排队之前**,先核对该 zone 当前在线(各 gate 在 etcd NodeInfo 里自报的 `player_count` 之和,login 算余量用的就是它)
    低于 `LoginQueueZoneCapacity`(5000)。否则排队一开,新登录全部入队,直到在线降到 5000 以下才出队,dispatcher 不会因此报错(规格 R20)。
    超过的先调大常量(见下一条),或先只开 gateway 限流(`-LoginQueue off`)。
    (2026-10-15 补:「首次打开排队」还要先过上面的合入门槛;门槛未清时排队在任何环境都关着,本条暂不适用。)
  - **调阈值**:阈值只在常量表 `$script:PeakShavingLaunchTier`(不设覆盖参数);改常量后重跑部署,gateway / login 按指纹自动滚动。限流阈值另外要**清桶**:
    Bucket4j 只在桶不存在时按新配置建桶,而持续有流量时桶的 TTL 一直被刷新、不会过期,所以改 rps / burst 后旧桶一直按旧配置跑
    (推断:`AssignGateRateLimiter` 用 `build(key, supplier)`、没有配置替换,依 Bucket4j 8.x 语义,未实测)。在维护窗口对共享 Redis(`redis.<InfraNamespace>`,
    staging / prod 要带口令)执行:
    ```bash
    redis-cli --scan --pattern 'rl:zone:*' | xargs -r redis-cli DEL
    redis-cli --scan --pattern 'rl:ip:*'   | xargs -r redis-cli DEL
    ```
    代价:清空后每个桶会重新拿到一次满额 burst。只是关掉限流不需要清:`rl:*` 空闲约 1h(外加重填到满的时间)后按 TTL 自然过期。
  - **观测缺口**:gateway 没有限流指标;K8s 的 login ConfigMap 没有 `Prometheus` 段,`login_queue_*` 在集群里抓不到。开服当天看日志与回包码:gateway 启动时打
    `Bucket4j Redis client -> …`(限流开)或 `RateLimiter disabled …`(关);Redis 出错时逐请求 WARN `rate-limit fail-open due to backend error`;login 的选主日志见 ②。
  - 已知风险(保护不了 EnterGame、IP 桶误伤 NAT、先扣 zone 令牌再判 IP 桶、排队轮询压在共享 Redis 等)见设计稿 §十一。

## Important Notes

- The default node image is a placeholder. Replace `-NodeImage` with your real image.
- The script assumes Linux containers and `/app/bin` runtime layout.
- Deleting a zone currently deletes the entire namespace (`k8s-zone-down`).
- `infra-down` / `all-down` 删的是整个 infra namespace,**`loki-data-pvc`(10Gi)连同里面已采集的全部 C++ 日志一起删**(与 etcd / kafka / mysql 的 PVC 同一性质)。排障还要用的日志先导出,或者用 `-LokiPushUrl` 把 sidecar 指向集群外的 Loki —— 那种情况下集群内根本不部署 Loki,删 namespace 也不碰日志。
- Do not use `LoadBalancer` in production unless the cluster provides a real, mature LB implementation. Otherwise use `NodePort` plus an external L4 balancer.

## etcd:3 副本 StatefulSet + PVC(2026-09-08,node-id-overhaul-plan §7 Phase 0.5)

`manifests/infra/etcd.yaml` 现在是:ClusterIP Service `etcd`(仍是 `etcd.<InfraNamespace>:2379`,所有 ConfigMap 不用改)
+ headless Service `etcd-headless`(成员间 peer 地址)+ `PodDisruptionBudget minAvailable: 2`
+ StatefulSet `etcd` 3 副本、`podManagementPolicy: Parallel`、每成员一块 `8Gi` PVC(集群默认 StorageClass)。
成员表是静态的(`etcd-0/1/2.etcd-headless.<ns>.svc.cluster.local:2380`),成员重启后凭 PVC 里的数据以原身份回到集群。

- **为什么**:所有节点 id / 发号槽位都是 etcd lease 绑定的。单副本 etcd 一重启(哪怕只是 Pod 漂移),全部 lease 消失,
  每个 C++ / Go 节点都会收到 ownership lost 并自 fence 退出。验收口径:`kubectl -n mmorpg-infra delete pod etcd-1`,
  没有任何 gate / scene / login 被 fence。
- **首次从旧单副本切换**:旧的 `Deployment etcd` 用的是 emptyDir,里面的数据**不迁移**,也不需要迁移 ——
  那里只有 lease 绑定的节点注册键与 snowflake 槽位、killswitch 规则,没有任何玩家数据(玩家数据在 MySQL / Redis)。
  `infra-up` 会先 `kubectl delete deployment etcd --ignore-not-found` 再 apply StatefulSet
  (两者 kind 不同、名字相同,`apply` 不会回收旧 Deployment;而 Service 的 selector 会同时命中新旧 Pod,不删就是脑裂)。
  切换期间 etcd 短暂不可用;切完之后**所有节点必须重启一次重新注册**(node id 都在启动时重新申领,
  `kubectl -n <zone-ns> rollout restart deploy` 即可)。killswitch 规则(`/mmorpg/killswitch/...`)如果有,需要重新 put。
- **`etcdctl` 怎么进**:不再有 `deploy/etcd`,用 StatefulSet 或指定成员:
  `kubectl exec -n mmorpg-infra sts/etcd -- etcdctl endpoint status --cluster -w table`(任一成员)或
  `kubectl exec -n mmorpg-infra etcd-0 -- etcdctl get --prefix --keys-only LoginNodeService.rpc/`。
- **PVC 生命周期**:删 StatefulSet 不删 PVC,删 namespace(`infra-down`)才删。`infra-down` → `infra-up` 之后是一套空 etcd,
  同样按上一条重启节点。
- **kind / minikube**:单节点也能跑(反亲和是 preferred),但 PVC 要有默认 StorageClass(kind 自带 local-path)。
  `infra-status` 现在会列 `sts` / `pvc` / `pdb`;etcd 起不来先看 PVC 是否 Pending。
- **维护:MVCC 历史压缩(2026-10-15,P0-C 阶段 1)**:etcd 不压缩就保留每个 key 的全部历史,本仓库每 zone 每秒约 6 个
  revision(Go 发号水位等),写到配额(默认约 2GiB,待实测)就 NOSPACE:新注册、失租重注册、gate 上报、发号水位全部失败。
  `manifests/infra/etcd-maintenance.yaml` 是独立的 ConfigMap + CronJob `etcd-compact`:每 5 分钟用线性一致读取当前 revision,
  压缩到 `current - 1000000`(`KEEP_REVISIONS`,与 compose 的 `ETCD_AUTO_COMPACTION_RETENTION` 同值,取值待实测 / 压测复核);
  不做 defrag、不解除告警,有活动告警时以 2 退出让 Job 失败可见。`infra-up` 在 `etcd.yaml` 之后 apply 它,`infra-status` 列
  `cronjob`。它不碰 StatefulSet `etcd`,apply 不滚动 etcd。
  - **有真实存档的环境不要为它跑 `infra-up`**(会连带 apply `mysql.yaml`,L-04 约束见
    `docs/handoff/godb-pipeline-handoff-20261013.md` §2.4),只 apply 这一个清单;它带 `__INFRA_NAMESPACE__` 占位,单独 apply 时要先替换:
    `(Get-Content -Raw deploy/k8s/manifests/infra/etcd-maintenance.yaml).Replace('__INFRA_NAMESPACE__', 'mmorpg-infra') | kubectl --context <ctx> apply -n mmorpg-infra -f -`。
  - 手工触发一次:`kubectl --context <ctx> -n mmorpg-infra create job --from=cronjob/etcd-compact etcd-compact-manual-<时间戳>`;
    看结果:`kubectl -n mmorpg-infra get jobs -l app=etcd-maintenance`、`kubectl -n mmorpg-infra logs job/<名字>`。
  - defrag 只手工、逐成员做;NOSPACE 的处置顺序、compose 环境的手工压缩、配额(3GiB)与阶段 2,全在
    `docs/ops/etcd-maintenance-runbook.md`。
  - **阶段 2(给 StatefulSet 加压缩 / 配额 env、钉镜像 tag@摘要、加抓取注解)会滚动 3 个成员,未做**:前置是 C++ etcd 流重建
    (`docs/design/grpc-muduo-transport.md` §10.2 第 2 批)验证通过。`tools/scripts/tests/etcd_maintenance_contract.tests.ps1`
    断言 `etcd.yaml` 里还没有这些 env,防止提前滚动。

## snowflake 缓存目录 / PlayerId 号段 / data_service 全局库(2026-09-08,node-id-overhaul-plan §3.5 / §6 / §2.0c)

生成器(`tools/scripts/k8s_deploy.ps1`)与静态 manifest 新增的键。凡是"脚本常量 + 静态文件写死"的组合,改一处必须同步改另一处。

- **`SnowflakeCacheDir: /tmp/snowflake`**(`login` / `scene-manager` / `match` 的 Go ConfigMap)与 **`SNOWFLAKE_CACHE_DIR=/tmp/snowflake`**
  (C++ `gate` / `scene` 的 Deployment 与 Agones Fleet 的环境变量;只有 scene 启用发号槽会写,gate 收到什么也不做)。
  路径常量在脚本顶部 `$SnowflakeCacheDir`;`manifests/go-svc/{login,scene-manager,match}.yaml` 与生成的 C++ Deployment / Fleet
  都在同一路径挂一个 **`emptyDir`**(卷名 `snowflake-cache`)。它是每个 Pod 自己的启动期草稿 —— etcd 不可达时凭它续用上次的槽位(2h 内),
  不跨 Pod 共享、不需要持久卷,Pod 重建就重新申领。Go 默认的 `../../run/snowflake` 在容器里落到 `/run/snowflake`,
  能不能写取决于镜像是否以 root 跑、根文件系统是否只读,部署侧不赌这个。
  三份 go-svc manifest 的 `mountPath` 是写死的(脚本不替换),改 `$SnowflakeCacheDir` 时一起改。
- **login ConfigMap 新增 `DataServiceRpc` + `IdSegment`**:`DataServiceRpc` 走 etcd 发现 `dataservice.rpc`,`Timeout: 3000`,
  `NonBlock: true`(弱依赖:data-service 没起 login 照常起服);`IdSegment: { Enabled: true, Step: <go/login/etc/login.yaml 的 IdSegment.Step>, FallbackToSnowflake: false }`。
  `Enabled` 必须**显式写 true**(整块不写 = 静默回到旧的纯 snowflake 发号,不报错);`Step` 从服务 yaml 取,`Enabled` / `FallbackToSnowflake` 是部署决策,模板里写死并注释。
  后果:**建角要求 data-service 可达且全局库可用**,领段失败即建角失败(§6.4 的决定,不回退 snowflake)。
  data-service 目前仍按 zone namespace 各部署一份(`$GoSvcCatalogue` 里不是 Global),但 go-zero 发现键 `dataservice.rpc` 不分 zone、全局库也只有一份 ——
  多实例共用一张 `id_segment` 表是设计内的(store 里 `SELECT ... FOR UPDATE` 把同 `biz_tag` 的领段串行化,再以 `version` 兜底),不需要改。
- **data-service ConfigMap 新增 `SnapshotMySQL` / `Schema` / `Kafka`**(以前没有这三段,Go 默认 `127.0.0.1:3306/testdb`,K8s 上必然连不上,三个 store 置 nil):
  - `SnapshotMySQL`:`mysql.<InfraNamespace>:3306`,账号沿用 db 服务同一组注入值(`MMORPG_MYSQL_USER` / `MMORPG_MYSQL_PASSWORD`,dev 回落 root),
    `DBName` = 脚本顶部 `$GlobalDbName`(`mmorpg_global`,刻意不叫 testdb)。
  - `Schema.AutoMigrate`:dev 档取服务 yaml 的值(`true`,起了就能用);staging / prod 固定 `false`(多副本并发 ALTER 互相 MDL 阻塞),
    部署阶段单进程跑一次 `data_service -f /app/etc/data_service.yaml -migrate`,与 db 服务的 `AutoMigrateSchema` 同一条纪律。
  - `Kafka`:brokers `kafka.<InfraNamespace>:9092`;topic 名 / 分区数 / 消费组 / 保留期全部从 `go/data_service/etc/data_service.yaml` 取
    (分区数是不可变契约,`EnsureTopics` 会拒绝与 broker 现状不一致的值)。Kafka 不可达不影响 Load / Save,只记日志并后台重试。
  - `Kafka.TopicGeneration`:同样从服务 yaml 镜像(当前 `1`),有效 topic 名 = `<基名>_g<N>`,当前集群消费第几代 topic 在 ConfigMap 里一眼可见;
    改分区数 = 这个数 +1,绝不原地扩分区,见下面「Kafka 审计 topic 预建」。
  - `IdSegment`:`AllowAutoSeed` dev 档取服务 yaml 的值(`true`),staging / prod 固定 `false`(与 `Schema.AutoMigrate` 同一条纪律;为什么见下面「恢复全局库」一条);
    `BootstrapTags: [player, guild, item, txlog, snapshot, trade_listing, guild_asset_op]` 与服务 yaml / Go 的 `DefaultIdSegmentBootstrapTags` 同一份清单,新增永久身份两边同加
    (`trade_listing` = 聚宝斋 listing_id,2026-09-14 加,见下面「聚宝斋 trade」;`guild_asset_op` = 帮会资产指令 op_id,2026-09-20 帮会二期 B5a 加,
    消费表在独占库 `mmorpg_guild`。**存量集群要先让 data-service 跑一次 `-migrate` 建出这一行,再上 guild 的经济功能**,否则 guild 取不到号、经济写 RPC 全部失败)。
  - `manifests/go-svc/data-service.yaml` 的 Deployment **2026-10-08 起是 2 副本 + RollingUpdate(maxSurge 1 / maxUnavailable 0)+ 反亲和 + 同文件 PDB**
    (docs/design/no-single-node-horizontal-scaling-20261001.md §4)。以前钉成 `Recreate` + `replicas: 1` 的理由(快照去重靠「单条语句 + 间隙锁」,
    两个实例互相等锁)已经过时:去重现在是唯一键 `uk_snapshot_guid_nz` + ON DUPLICATE KEY;消费者是 consumer group,分区在实例间独占分配;
    发号靠行锁 + version。多副本的两个服务侧前提:启动期迁移由库级命名锁串行;ConfigMap 写 `Store.Required: true`,store 装配不起来就拒启。
  - **建表 / 种号段行改由 `data-service-migrate` Job 做**(`manifests/go-svc/data-service-migrate.yaml`,目录条目 `MigrateJob`):
    `Apply-OneGoSvc` 在 ConfigMap 之后、Deployment 之前 apply 它,staging / prod 恒等它 Complete(与 trade / friend 同一条门禁)。
    每个 zone 各跑一份,迁的是同一个全局库,由 `GET_LOCK` 串行;锁忙以退出码 3 重试。以前这一步靠人手工跑 `-migrate`,全新集群上没人跑。
  - 连接预算:每个 data-service 实例最多 35 条 MySQL 连接(三个 store 各 5 + 名字注册表 20;`RouteStore` 切 db_first 后再 +20),
    按 zone 数 × 副本数计入共用 MySQL 的总账。怎么加总、`max_connections` 怎么取,见下面「MySQL 连接总账」一节,不要在这里手算。
- **全局库预建**:`infra-up` 的 `mysql-init-sql` ConfigMap 现在多生成一份 `02_k8s_global_db.sql`
  (`CREATE DATABASE IF NOT EXISTS mmorpg_global` + `GRANT ALL ... TO 'appuser'@'%'`),data-service 启动期 / `-migrate` 在里面按 proto 建
  `transaction_log` / `player_snapshot` / `rollback_audit_log` / `id_segment` 四张表。全集群一份,不按 zone 拆。
  initdb 只在 mysql PVC 为空时执行:**存量 PVC 要手工执行这两句**
  (`kubectl exec -n mmorpg-infra deploy/mysql -- sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -e "CREATE DATABASE IF NOT EXISTS mmorpg_global; GRANT ALL PRIVILEGES ON mmorpg_global.* TO '"'"'appuser'"'"'@'"'"'%'"'"';"'`),
  否则 data-service 只剩 `schema auto-migrate failed`,login 建角全部失败。
  本地 compose 的对应物是 `deploy/mysql-init/00_init_zone_dbs.sql` 顺带建的 `testdb`(`go/data_service/etc/data_service.yaml` 用),同样只对空数据卷生效。
- **恢复全局库前须先核对 `id_segment.max_id` ≥ 消费表最大号(这是 ID 安全事件,不是普通的库恢复)**:`id_segment` 每行是一种永久身份的发号水位
  (`biz_tag` = `player` / `guild` / `item` / `txlog` / `snapshot` / `trade_listing` / `guild_asset_op`;`max_id` 是下一个要发出去的号,领段 = `SELECT ... FOR UPDATE` 后 `max_id += step`)。
  把 `mmorpg_global` 从备份恢复,就是把水位倒回备份时刻,而备份之后领走的号早已成了消费侧的真实主键(各 zone 库 `player_database.player_id`、
  `guild.guild_id`、玩家 blob 里 bag 的 item guid、Kafka 两个 topic 里的 tx_id / snapshot_id)—— 水位一回退,下一次领段就把这些号**再发一遍**:
  login 的 `INSERT ... ON DUPLICATE KEY UPDATE` 会静默覆盖别人的角色行,流水表的 `INSERT IGNORE` 会静默丢掉**新**流水。顺序必须是:
  1. 先停 data-service,并重启所有持有号段的进程(login / guild / scene):它们内存里还攥着恢复点之后领到的段,不清掉,抬水位也挡不住它们把段发完。
  2. 对每个 `biz_tag` 拿到消费侧最大号:`player` = **每个 zone 库**的 `SELECT MAX(player_id) FROM player_database` 取最大;`guild` = `SELECT MAX(guild_id) FROM guild`;
     `item` = 各 zone 库玩家 blob 里 bag 的最大 guid;`txlog` / `snapshot` = Kafka `transaction_log_topic_g<N>` / `player_snapshot_topic_g<N>` 里的最大 tx_id / snapshot_id
     (`transaction_log` / `player_snapshot` 表与 `id_segment` 同库,恢复后同库 `MAX()` 不是独立证据);
     `trade_listing` = `SELECT MAX(listing_id) FROM mmorpg_trade.trade_listing`(独占库,不在全局库里,`-migrate` 不会替你查);
     `guild_asset_op` = `SELECT MAX(op_id) FROM mmorpg_guild.guild_asset_op`(同样是独占库;op_id 重发会撞 `guild_asset_op` 主键,捐献 / 兑换写不进去)。
     登记 / 核对 / 恢复这些库时,聚宝斋独占库 `mmorpg_trade`、好友独占库 `mmorpg_friend` 与 `mmorpg_global` 要分开处理,别只恢复其中一个
     (`mmorpg_friend` 不产号、没有 `id_segment` 水位问题,但它与各 zone 库的 `player_database` 有引用关系:
     只恢复好友库会留下指向已不存在玩家的好友边 —— 那是 `tools/data_consistency_check` 的 `checkFriendOrphans` 要查的东西)。
  3. `SELECT biz_tag, max_id FROM id_segment`,凡 `max_id` ≤ 对应最大号的:`UPDATE id_segment SET max_id = <最大号> + 1 + 余量 WHERE biz_tag = '<tag>'`(余量 ≥ 一个 step)。
  4. 然后才跑 `-migrate` / 起 data-service。`-migrate` 自带的地板校验(`raiseIdSegmentFloor`)只在**同一个库**里查得到 `player_database` / `guild` 时才抬水位 ——
     K8s 上 player 表在各 zone 库、全局库里没有,它只会打 Info,不代表安全;`item` / `txlog` / `snapshot` 根本没有自动校验。
  `IdSegment.AllowAutoSeed` 在 staging / prod 固定 `false` 正是为这一刻:行不见了(恢复到更早的备份 / 库被 drop 重建)时 data-service 不会自作主张从 1 播种,
  而是 `ErrCodeIdSegmentUnknownTag` 拒绝领段、不写任何行,把决定留给人;dev 档 `true` 只是让空库起了就能建角。
- **guild 还没有 K8s ConfigMap / manifest**(存量缺口:`$GoSvcCatalogue` 没有它,`manifests/go-svc/` 也没有 `guild.yaml`,`go_svc_image.ps1` 之外没人碰它)。
  补的时候除了 MySQL DataSource / Redis 之外,还要写 `ClusterId`、`SnowflakeCacheDir`(+ 同名 emptyDir)、`DataServiceRpc`、`IdSegment`,
  形状照 `go/guild/etc/guild.yaml`;guild_id 号段与 PlayerId 走同一个 `AllocateIdSegment`、同一张 `id_segment` 表(不同 `biz_tag`)。

## 聚宝斋 trade:独占库 `mmorpg_trade` + `trade-migrate` Job(2026-09-14,port-decisions D-14 / docs/design/jubaozhai-market.md)

trade 是全局池服务(`$GoSvcCatalogue.trade`,`Global = $true`):`infra-up` / `all-up` 在基础设施阶段部署到 `mmorpg-infra` 一份,`zone-up` 跳过。

- **可达性**:只承诺路由服模式。客户端经 gate → `client-rpc-router` → trade;K8s 默认 `-GateRouterMode "0"`,
  此时 Deployment 起得来、探针绿,但玩家够不着(与 chat 同一已知缺口)。内部方法 `TradeAdmin.SeedListing` 不经 gate(gate 不认它的消息号),
  只在 `Mode ∈ {dev, test}` 时可用,否则回 gRPC `PermissionDenied`。
  **(2026-09-29 更正)** K8s 默认已改为 `-GateRouterMode "1"`(turn-based §22 D75,`k8s_deploy.ps1:163-164`),默认部署下 gate → `client-rpc-router` → trade
  这条路已接上,trade **按设计可达,但尚未在 K8s 上实跑补验**(D65 / D75 豁免的事后补验项)。前置:`client-rpc-router` 已随 `infra-up` 部署就绪;
  集群外玩家还需 `-ClientEntryMode external`(podip 下 gate 通告 PodIP)。以 `"0"` 回退运行的环境仍然够不着,见 Optional Flags 的 `-GateRouterMode`。
- **工作负载**:`manifests/go-svc/trade.yaml` = Service `trade`(50800)+ Deployment(`replicas: 2`、podAntiAffinity、grpc 探针、
  `preStop sleep 5`、Downward API `POD_IP`、metrics 9230)+ 同文件 PDB `trade-pdb`。ConfigMap `go-svc-trade-config` 由脚本生成。
- **ConfigMap 键**与 `go/trade/etc/trade.yaml` / `go/trade/internal/config` 逐字一致;契约值只从服务 yaml 取,且只在生成 trade 自己的 ConfigMap 时求值
  (yaml 缺键即 fail-closed,不会拖挂别的服务的 ConfigMap):
  - 从服务 yaml 取:`Name`、`Timeout`、`LeaseTTL`、`MySQL.DBName` / `MaxOpenConn` / `MaxIdleConn`、`DataServiceRpc.Timeout`、
    `Market.Scope` / `DefaultPageSize` / `MaxPageSize` / `MaxPage` / `MaxFavoritesPerPlayer`;`IdSegment.Step` / `MinStep` / `MaxStep` 有就镜像、没有就整行不写。
  - 部署侧写死:`ListenOn: 0.0.0.0:50800`、`Etcd.Key: ""`(D-13)、`ZoneId`(命令行 `-ZoneId`)、`MetricsListenAddr: ":9230"`、`KillSwitchPrefix: ""`、
    `MySQL.Host: mysql.<InfraNamespace>:3306`、账号 = `MMORPG_MYSQL_USER` / `MMORPG_MYSQL_PASSWORD` 注入值(dev 回落 root)、
    `DataServiceRpc.Etcd.Key: dataservice.rpc`(`NonBlock: true`,Breaker 关)、`IdSegment.Enabled: true` / `FallbackToSnowflake: false`。
  - **按档位锁死**:`Mode` 与 `Schema.AutoMigrate` 只有 `-ReleaseProfile dev` 取服务 yaml 的值;staging / prod 固定 `Mode: pro`
    (SeedListing 造数入口关死)与 `AutoMigrate: false`(表只由 Job 建)。
- **建库**:`mmorpg_trade` 只登记在 `deploy/mysql-init/00_init_zone_dbs.sql`(`CREATE DATABASE ... utf8mb4_unicode_ci` + `GRANT ALL ... TO 'appuser'@'%'`),
  `infra-up` 的 `mysql-init-sql` ConfigMap 把整个目录原样带入;脚本**不**另生成建库 sql(D-14 第 5 条,`02_k8s_global_db.sql` 不是范式)。
  initdb 只在 mysql PVC 为空时执行:**存量 PVC 要手工执行**
  (`kubectl exec -n mmorpg-infra deploy/mysql -- sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -e "CREATE DATABASE IF NOT EXISTS mmorpg_trade DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci; GRANT ALL PRIVILEGES ON mmorpg_trade.* TO '"'"'appuser'"'"'@'"'"'%'"'"'; FLUSH PRIVILEGES;"'`),
  否则 `trade-migrate` 以退出码 1 失败(日志点名库名),发布在 trade Deployment 之前中断。本地 compose 的存量卷同理,见该 sql 文件里的注释。
- **建表**:`manifests/go-svc/trade-migrate.yaml` 一次性 Job,与 trade Deployment 同镜像、同 ConfigMap,`args: ["-f", "/app/etc/trade.yaml", "-migrate"]`
  (只连 MySQL 跑 `go/schemamigrate`,不起 gRPC、不连 etcd / data-service)。目录条目用 `MigrateJob = "trade-migrate.yaml"` 登记,
  以后的建表服务照这个字段加,不要另写 Job 调度逻辑。
  - **门禁**(D-14 第 4 条,**不受 `-WaitReady` 控制**):`staging` / `prod` 恒等 Job Complete 才 apply trade Deployment;
    `dev` 档只在 `-WaitReady` 下等(dev 的 `AutoMigrate=true`,服务启动期自己也会在同一把锁下迁移)。
  - **等待预算**:两段 Job 等待(下面的在途检查与终态轮询)各用 `max(-WaitTimeoutSeconds, 300)` 秒。默认 180s 装不下
    "调度 / 拉镜像 + initContainer 等 3306(≤ 158s)+ 迁移",所以脚本有下限 `$GoSvcMigrateJobMinWaitSeconds = 300`;
    它与 `trade-migrate.yaml` 的 `WAIT_MYSQL_TIMEOUT_SECONDS` 联动,**改其一必须核对另一个**。Job 前面的 `deploy/mysql` rollout 等待
    仍只用 `-WaitTimeoutSeconds`:全新集群首次 `infra-up` 建议带 `-WaitTimeoutSeconds 600`,否则 MySQL 拉镜像 / initdb 慢会被判超时、中断发布。
  - **顺序**(`Apply-OneGoSvc` → `Apply-GoSvcMigrateJob`):apply ConfigMap →(门禁)等 `deploy/mysql` 滚动完成 →
    检查上一次 `trade-migrate` 是否在途 → `kubectl delete job trade-migrate --ignore-not-found` → apply Job →(门禁)轮询 Job 终态 → apply trade Deployment。
    Job **Failed 或超时即中断发布**:打印 describe / Pod 列表 / `kubectl logs job/trade-migrate`,trade Deployment 不 apply。
  - **不删在途 Job**:迁移 runner 执行 DDL 前先在 `schema_migrations` 写 `dirty=1`、成功后才清零。删掉正在跑的 Job(Pod 被 SIGTERM)
    打断 DDL 会把台账留在 dirty,之后每次 `-migrate` 都以 1 退出、被 FailJob 直接判失败,只能人工清台账。
    所以脚本删之前先读 Job 和所属 Pod 状态:Job 不存在才可直接继续;存在时须有 `Complete` / `Failed` / `FailureTarget` 条件,且按该 Job UID 核对所属 Pod 全部为 `Succeeded` / `Failed`,才可删除。仍在途、状态未知或查询失败时在同一预算(`max(-WaitTimeoutSeconds, 300)`)内等待,等不到就中断发布且**不删**。
    kafka-topic-init 没有台账,才能无脑先删再建;**不要手工删除仍有非终态所属 Pod 的 `trade-migrate` Job**。`ACTIVE=0` 不含正在 terminating 的 Pod,不能单独作为安全删除判据。
    发布门禁的轮询同时认 `Complete` 与 `Failed`(以及 1.31+ 的 `FailureTarget`),失败条件出现即中断发布,不必等 Pod 清理完;重跑时的删前检查仍须核对 UID 所属 Pod 均终态。不用 `kubectl wait --for=condition=complete`,否则 FailJob 要白等到超时。
  - **退出码**:0 成功 → Complete;3 锁忙 → 按 `backoffLimit: 4` 退避重试;1 失败 / 4 需人工(类型漂移、缺主键、表清单异常)→
    `podFailurePolicy` 的 `FailJob` 立即失败。`podFailurePolicy` 需要 **Kubernetes ≥ 1.26**,且 Pod 必须 `restartPolicy: Never`。
    Go panic(2)、OOMKilled(137)与 initContainer 失败走 backoff 重试;节点驱逐(`DisruptionTarget`)不计次数。
  - **initContainer `wait-mysql`**(同一镜像里 busybox 的 `nc`)等 `mysql.<InfraNamespace>:3306` 可连,预算 `WAIT_MYSQL_TIMEOUT_SECONDS=150`
    (按 `/proc/uptime` 单调计时;截止后最多再跑一轮 `nc -w 3` + `sleep 5`,硬上限 158s;超时以 1 退出,走 backoff 重试):
    mysql 的 readinessProbe 走 localhost socket,initdb 期间 Pod 就可能 Ready 而 3306 还没开;不等的话 `-migrate` 以 1 退出,被当成不可重试。
  - 只有 `dev` 档不带 `-WaitReady` 时脚本才只打印核对命令、不等 Job:自己看 `kubectl -n mmorpg-infra get job trade-migrate`(COMPLETIONS `1/1`)
    与 `kubectl -n mmorpg-infra logs job/trade-migrate`。`infra-status` 的 `job` 一栏也会列它。
  - staging / prod 的 trade 启动路径只跑一次只读 plan,有待执行语句或需人工项就拒绝启动(CrashLoop)并打印补救命令;
    先看 Job 日志修根因,**不要**把 `Schema.AutoMigrate` 改成 true 绕过。
  - **重跑迁移**:先核对 `trade-migrate` 的 `status.conditions` 与 `metadata.uid`,确认该 UID 所属 Pod 均为 `Succeeded` / `Failed`,再跑一次同一条 `infra-up`
    (满足上述删前条件的 Job 会先删再建,迁移幂等;仍有非终态所属 Pod、状态未知或查询失败时脚本会先等,等不到就再次中断且不删)。`ACTIVE=0` 不能代替所属 Pod 终态检查;上一次因**超时**中断时,旧 Job 可能还在 backoff 重试或执行 DDL,尤其要先看这一步。
    日志里是 **dirty 台账**(`schemamigrate.ErrDirty`,退出码 1)时重跑不会自己好:先人工核对 `mmorpg_trade.schema_migrations` 的 dirty 行与表的实际结构,
    确认那条迁移做完或补做后再清 dirty,然后重跑。
- **号段**:listing_id 走 data-service 号段(`biz_tag = trade_listing`),data-service ConfigMap 的 `IdSegment.BootstrapTags` 已加。
  staging / prod 的 `AllowAutoSeed` 固定 `false`:**存量集群要先让 data-service 跑一次 `-migrate`**(INSERT IGNORE,幂等)把 `trade_listing` 行建出来,
  否则 trade 领段拿到 `ErrCodeIdSegmentUnknownTag`,SeedListing 回 `kServiceUnavailable`。恢复 `mmorpg_global` 时的水位核对见上一节。
- **连接数**:`MySQL.MaxOpenConn`(20)× 2 个副本计入共用 MySQL 的总账(「全局」一项),`trade-migrate` Job 的连接算在运维余量里;
  总账与 `max_connections` 的取值见下面「MySQL 连接总账」一节。
- **上线前未做(P3)**:`mmorpg_trade` 的 audit auditor / data_consistency_check、TiDB BR 按库恢复清单;
  `trade_listing.market_zone` 带 home_zone 语义,合服改写步骤在 `tools/merge_zone`。

## 好友 friend:独占库 `mmorpg_friend` + `friend-migrate` Job(2026-09-18,port-decisions D-14 / docs/design/friend-port-20260918.md)

friend 与 trade 同形:全局池服务(`$GoSvcCatalogue.friend`,`Global = $true`),`infra-up` / `all-up` 在基础设施阶段部署到 `mmorpg-infra` 一份,`zone-up` 跳过。
下面只写与 trade 不同的地方,共性(Job 门禁 / 等待预算 / 不删在途 Job / 退出码 / 重跑迁移)一律照上面「聚宝斋 trade」那节,不重复一遍。

- **可达性**:只承诺路由服模式(D-12)。客户端经 gate → `client-rpc-router` → friend;K8s 默认 `-GateRouterMode "0"`,
  此时 Deployment 起得来、探针绿,但玩家够不着(与 chat / trade 同一已知缺口)。内部方法(`NotifyFriendEvent` 等下行)
  客户端直呼一律回 gRPC `PermissionDenied`,由路由服翻成信封级 `kServiceUnavailable`(D-9)。
  **(2026-09-29 更正)** 同「聚宝斋 trade」一节:K8s 默认已是 `"1"`(D75),friend 在默认部署下**按设计可达、待 K8s 事后补验**;
  前置与集群外口径(`client-rpc-router` 已部署、集群外玩家需 `-ClientEntryMode external`)相同,以 `"0"` 回退运行时仍不可达。
- **工作负载**:`manifests/go-svc/friend.yaml` = Service `friend`(50400)+ Deployment(`replicas: 2`、podAntiAffinity、grpc 探针、
  `preStop sleep 5`、Downward API `POD_IP`、metrics 9180)+ 同文件 PDB `friend-pdb`。ConfigMap `go-svc-friend-config` 由脚本生成。
  **不挂 snowflake 卷**:好友边/拉黑仍用玩家对主键；2026-10-15 起申请另有唯一 `request_id`，由 data_service 的 `friend_request` 号段生成，失败不回退 Snowflake。
- **端口五处登记必须逐字一致**:`go_services.ps1` 的 `Port = 50400`、`go/friend/etc/friend.yaml` 的 `ListenOn`、
  `k8s_deploy.ps1` 的 `$GoSvcCatalogue.friend.Port`、生成的 ConfigMap 里的 `ListenOn`、本 manifest 的 Service / `containerPort` / 探针。
- **建库**:`mmorpg_friend` 只登记在 `deploy/mysql-init/00_init_zone_dbs.sql`,`infra-up` 的 `mysql-init-sql` ConfigMap 原样带入。
  initdb 只在 mysql PVC 为空时执行:**存量 PVC 要手工执行**
  (`kubectl exec -n mmorpg-infra deploy/mysql -- sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -e "CREATE DATABASE IF NOT EXISTS mmorpg_friend DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci; GRANT ALL PRIVILEGES ON mmorpg_friend.* TO '"'"'appuser'"'"'@'"'"'%'"'"'; FLUSH PRIVILEGES;"'`),
  否则 `friend-migrate` 以退出码 1 失败(日志点名库名),发布在 friend Deployment 之前中断。本地 compose 的存量卷同理。
- **建表**:`manifests/go-svc/friend-migrate.yaml`,`args: ["-f", "/app/etc/friend.yaml", "-migrate"]`,
  按 `proto/friend/friend_table.proto` 建 `friend` / `friend_request` / `friend_block` / `friend_capacity` 四张表。
  目录条目用 `MigrateJob = "friend-migrate.yaml"` 登记。`WAIT_MYSQL_TIMEOUT_SECONDS=150` 与 `trade-migrate.yaml` 取同一个值,
  同样与 `$GoSvcMigrateJobMinWaitSeconds`(300)联动,改其一必须核对另一个。
- **全服 TiDB 入口（2026-10-15）**:staging/prod 部署全局 friend 时必须显式传 `-FriendTiDBHost "<同一权威入口>:4000"`，全部副本连接同一逻辑库，服务启动验证真实 TiDB，并为每个物理连接固定 RC 与 pessimistic。脚本不创建 TiDB 集群/库/账号，须先准备 `mmorpg_friend` 与注入账号权限。dev 不传则保留 infra MySQL，显式传入也会打开 `RequireTiDB` 验证；`zone-up` / `all-up -SkipInfra` 不部署 friend，不要求此参数。不得把按 zone 独立库拼接当成全服强一致。
- **存量申请升级**:先停旧 friend 写者，再用新二进制 `-migrate`。新增 `request_id` 列可自动补齐，旧表的普通索引不会自动新建；缺索引时迁移明确 exit 4，必须审阅当前 proto 生成的 `friend_request` 索引定义、补建后再迁移到 exit 0，禁止跳过门禁。当前生成名为 `idx_friend_request_2`、列为 `request_id`；新建库由 proto 一次生成。旧 pending 的零 ID 由新服务安全补号，保留原时间，客户端须带非零 request_id；旧业务写者与新协议不能混跑。实际 TiDB 8.5.2 的旧表升级/缺索引拒绝/补索引再迁移已验证，见 `docs/design/friend-port-20260918.md` §9.6。
- **⚠ 旧建表路径已退役**:`deploy/mysql-init/guild_friend_tables.sql` 过去在 initdb 里把 friend 三张表建进**共享库 `mmorpg`**(= `MYSQL_DATABASE`;该文件没有 `USE` 语句,initdb 按 `--database=$MYSQL_DATABASE` 落库,**不是 `zone_N_db`**),
  还带一段 `friend_capacity` 回填与 `friend_capacity_backfill_v1` 门禁。2026-09-18 两段一起删除(D-10 修订)。
  **不要把它们加回去**:两处都建表 → schemamigrate 把 initdb 建出来的旧结构判成类型漂移(退出码 4,需人工),friend 起不来。
  **存量卷上那三张陈旧表现在仍躺在 `mmorpg` 库里**:删 sql 不会删掉已经建出来的表。清理时去 `mmorpg` 找,不要去 `zone_N_db` 找 —— 那里从来没有过 friend 表。
  `guild_schema_migration` 表保留,guild 的积分迁移门禁还在用。
- **Redis**:`FriendRedis`(好友列表缓存 / 申请配额 / 限流)在 K8s 上**不配**,回落共享库。
  缓存键已带 hash tag、Cluster 安全;但 staging/prod 若要给它配独立实例,该实例必须 `maxmemory-policy=noeviction` ——
  共享库是 `allkeys-lfu`,缓存被淘汰只是多打一次 MySQL,**配额 / 限流 key 被淘汰等于限流静默失效**。
  跨运行时契约 key `player:session:{id}`(判好友在不在线)永远走共享库,共享库不许集群化(C++ 侧 hiredis 没有集群客户端)。
- **备份与恢复**:开发 MySQL 上的 `mmorpg_friend` 由既有 `mysql-backup-cronjob.yaml` 的 mysqldump 覆盖；迁到独立 TiDB 后该 Job 不会覆盖新入口，须另按 TiDB BR/PITR 方案纳入备份与演练，不能沿用“备份侧不用改”的旧结论。恢复清单必须包含好友库及全局 `friend_request` 号段水位；跨库号段不会自动从消费表推高，水位绝不能回退到已发 ID。禁止单独回档号段导致旧请求 ID 被复用，并核对玩家引用的孤儿边。生产备份/恢复尚未在本批部署或演练。
- **连接数**:开发共库时 `MySQL.MaxOpenConn`(20)× 2 个副本仍计既有 MySQL 总账，迁移 Job 算运维余量；独立 TiDB 时另核连接与故障预算，现有 MySQL 总账保守保留该份额，不能据此认定 TiDB 容量已验收。
- **Kafka 命令 topic 代号不一致(存量全仓缺口,2026-09-28 已修,下面保留原记录)**:friend 的 S2C 推送经 `gate-cmd_g<N>` 到目标 zone 的 gate。
  `bin/etc/base_deploy_config.yaml` 当前是 `CommandTopicGeneration: 2`,`kafka-topic-init` 预建 `gate-cmd_g2`、C++ gate 也消费 g2;
  而 **没有任何 go-svc manifest 注入 `KAFKA_COMMAND_TOPIC_PARTITIONS` / `KAFKA_COMMAND_TOPIC_GENERATION`**,
  `go/shared/kafkacmd` 因此在 K8s 上回落到编译期默认 256 / **1**,Go 侧(login / scene-manager / player-locator / guild / friend)
  发出的 gate 命令落到 `gate-cmd_g1`,无人消费、静默丢失。本地 `start_game.ps1` 会注入这两个变量,所以本机与 robot 冒烟看不到这个问题。
  修法是让 `k8s_deploy.ps1` 从 `base_deploy_config.yaml` 统一注入到所有 go-svc Deployment(一处真相),
  **不要**在单个 manifest 里补一行写死的值 —— 那会把部署级常量复制成第二份真相,下次换代号必漏改。
  **已按此修复**:`Apply-OneGoSvc` 在任何集群写操作之前调用 `Add-GoSvcCommandTopicEnv`,把两个变量插进每个 go-svc
  唯一的 `env:` 段;manifest 手写这两个变量、没有 / 多于一个 `env:` 段都会让部署直接失败(fail-closed)。
  契约测试 `k8s_deploy_contract.tests.ps1` 钉住 zone 内服务的注入值,`k8s_migrate_gate.tests.ps1` 覆盖全局服务与形状校验。
- **上线前未做(P3)**:`mmorpg_friend` 的 audit auditor;`-GateRouterMode` 默认仍是 `"0"`(与 chat / trade 同一缺口);
  `dev_tools.ps1` 的 `k8s-*` 包装不透传 `-GateRouterMode`。合服:friend 三张表里没有 `zone_id` / `home_zone` 列,
  合服后行自动存活,`tools/merge_zone` 只需把审计连到 `mmorpg_friend`,不需要改写数据。
  **(2026-09-29 更正)** 上面两条关于 `-GateRouterMode` 的缺口已关闭:默认值已翻为 `"1"`(D75,K8s 路由模式 battle-smoke 为事后补验项);
  `dev_tools.ps1` 与 `k8s_image.ps1` 都已加"留空不覆盖、非空透传"的 `-GateRouterMode`(见 Optional Flags「包装入口的透传口径」)。audit auditor 缺口不变。

## 玩家收件箱 player-inbox:独占库 `mmorpg_player_inbox` + `player-inbox-migrate` Job + 运维口令 Secret + 告警(2026-10-15,docs/design/player-inbox-service.md 第 4 批)

> 状态:已落码,**未编译、未测试**,待 Codex 验证。服务本体(`go/player_inbox`)由第 2 批落码;本节只管部署接线。
> 运维手册(上线顺序、回滚下限、epoch-raise、库恢复、毒消息、批量处置):`docs/ops/player-inbox-runbook.md`。

player-inbox 与 trade / friend 同形:全局池服务(`$GoSvcCatalogue['player-inbox']`,`Global = $true`),`infra-up` / `all-up` 在基础设施阶段部署到
`mmorpg-infra` 一份,`zone-up` 跳过。共性(Job 门禁 / 等待预算 / 不删在途 Job / 退出码 / 重跑迁移)照「聚宝斋 trade」一节。不同之处:

- **调用方不是客户端**:C++ scene 按 etcd 前缀 `PlayerInboxNodeService.rpc` 直连 `PlayerInbox.FetchItems / AckItems`;运维直连
  `PlayerInboxAdmin`(GM 处置 / 作废 / 查询 / 对账)。与 `-GateRouterMode` 无关。两份前缀表都已登记:`bin/etc/base_deploy_config.yaml`
  与 `New-NodeConfigMapYaml`。C++ deadline `GrpcClient.CallDeadlineMs.PlayerInboxNodeService` 进了预算门禁(见「C++ gRPC 客户端 deadline」)。
  收件箱是**弱依赖**:不进 scene 的 DependencyGate,不可用时 scene 只退避、玩家照常进场(设计 IB-7)。
- **工作负载**:`manifests/go-svc/player-inbox.yaml` = Service `player-inbox`(50950)+ Deployment(`replicas: 2`、podAntiAffinity、grpc 探针、
  `preStop sleep 5`、Downward API `POD_IP`、metrics 9270)+ 同文件 PDB `player-inbox-pdb`。ConfigMap `go-svc-player-inbox-config` 由脚本生成,
  值从 `go/player_inbox/etc/player_inbox.yaml` 镜像(`Name` / `Timeout` / `LeaseTTL` / `MySQL.DBName|MaxOpenConn|MaxIdleConn` / `DataServiceRpc.Timeout` /
  `Kafka.WriteTimeoutMs` / `PlayerInbox.EnqueueTypes` 与六个后台循环参数,dev 档另镜像 `Mode` / `Schema.AutoMigrate`);`MySQL.Host`、`CoreRedis.Host`、
  `Kafka.Brokers`、`DataServiceRpc.Etcd.Key` 的**值**由模板按集群形态写死,但**键**必须在服务 yaml 里出现,生成期逐个核对(go-zero 对写错的键静默忽略)。
- **端口登记必须逐字一致**:`go_services.ps1` 的 `Port = 50950`、服务 yaml 的 `ListenOn` / `MetricsListenAddr ":9270"`、`$GoSvcCatalogue` 的 `Port`、
  生成的 ConfigMap、本 manifest 的 Service / `containerPort` / 探针 / `prometheus.io/port`(D14)。
- **运维口令**(`PlayerInbox.AdminToken`,设计 §1.6):常驻,**不与 data_service 回档口令共用**。部署侧从环境变量 `MMORPG_PLAYER_INBOX_ADMIN_TOKEN`
  读入 infra namespace 的 Secret `player-inbox-admin-token`(key `admin-token`),经 `secretKeyRef` 注入容器的 `PLAYER_INBOX_ADMIN_TOKEN`;
  ConfigMap 只写变量名 `PlayerInbox.AdminTokenEnv: PLAYER_INBOX_ADMIN_TOKEN`,口令值不进 ConfigMap / manifest / 仓库。
  - **Go 侧前提**:`go/player_inbox` 的配置要认 `PlayerInbox.AdminTokenEnv`(非空时从该环境变量取口令;与字面 `AdminToken` 二选一)。
    2026-10-15 第 2 批的 config 只有字面 `AdminToken`;在它补上之前,要注入口令的部署在写操作之前被拒(`Assert-PlayerInboxAdminTokenEnvDeclared`),
    不注入口令时运维接口照常停用。
  - **未注入(任何档位)**= 不建 Secret、不注入 env,运维接口停用(fail-closed,不是免鉴权,设计 §1.6「为空就停用」),写路径打一行
    `player-inbox: MMORPG_PLAYER_INBOX_ADMIN_TOKEN unset ...`;不为它挡住整次 `infra-up`。上一次带口令部署过、这次没带,残留 Secret 在新 Deployment apply 之后删除。
  - **注入了**:staging / prod 占位或短于 32 位、任何档位短于 16 字节(服务端下限)、与 Gate / InternalAuth / MySQL / gateway 管理面口令相同,
    都在任何写操作之前拒绝部署。
  - 轮换:Pod 模板注解 `mmorpg.io/player-inbox-admin-token-hash` 是口令指纹(SHA-256 前 12 位;manifest 里是占位,由 `Apply-OneGoSvc` 替换,
    未注入时写 `disabled`),换了口令重跑 `infra-up` 即滚动,旧口令随旧 Pod 退场。**不要绕过脚本直接 `kubectl apply` 本 manifest**。
- **建库**:`mmorpg_player_inbox` 只登记在 `deploy/mysql-init/00_init_zone_dbs.sql`。**存量 PVC 要手工执行**
  (`kubectl exec -n mmorpg-infra deploy/mysql -- sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -e "CREATE DATABASE IF NOT EXISTS mmorpg_player_inbox DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci; GRANT ALL PRIVILEGES ON mmorpg_player_inbox.* TO '"'"'appuser'"'"'@'"'"'%'"'"'; FLUSH PRIVILEGES;"'`),
  否则 `player-inbox-migrate` 以退出码 1 失败,发布在 Deployment 之前中断。
- **建表**:`manifests/go-svc/player-inbox-migrate.yaml`,`args: ["-f", "/app/etc/player_inbox.yaml", "-migrate"]`,按 `proto/player_inbox/player_inbox_table.proto`
  建 `player_inbox_stream` / `player_inbox_item` / `player_inbox_audit`(入口 `go/player_inbox/player_inbox.go` 的 `-migrate`,照 go/trade)。
  TiDB 上的索引预切不在 Job 里做,见 runbook「TiDB 索引预切」。
- **账本列不在这个库**:消费端账本是 zone 库 `player_database.inbox_ledger`(字段 20),由各 zone 的 go/db `cmd/migrate` 加列;
  上线顺序第 1 步(runbook §上线顺序)。合服前审计 `tools/merge_zone` 的 `player_inbox` 行会核对源 / 目标 zone 库都已有这一列。
- **入箱开关**:`PlayerInbox.EnqueueTypes` 镜像服务 yaml(各档位一致,白名单进 git、可审),每项必须是 `PlayerInboxMessageType` 的枚举名,否则生成期 throw。
  第一次部署时必须为空(`[]`);**全部 scene 打开 `consumer_enabled` 之后**才在服务 yaml 里填上并重跑 `infra-up`(上线顺序第 6 步)。
- **消费开关**:新参数 `-PlayerInboxConsumer off|on`(默认 off,不粘滞)写进 `node-config` 的 `game_config.yaml` → `player_inbox.consumer_enabled`;
  只写这一个键,`periodic_fetch_minutes` 缺席时 scene 加载器取默认 30。off 就是回滚下限(上线第 4 步);全部 zone 的 scene 升级完成后
  才逐 zone `zone-up -PlayerInboxConsumer on` 并滚动 scene(第 5 步;ConfigMap 变了不会自己重启 C++ 进程)。包装入口(dev_tools / k8s_image)不透传它。
- **连接数**:`MySQL.MaxOpenConn`(以服务 yaml 为准,规划按 20)× 2 副本已登记进 MySQL 连接总账(`PlayerInboxReplicas` / `PlayerInboxMaxOpen`);
  按公式手算规划档峰值 1620 → 1700(未运行)。
- **告警**:`deploy/k8s/player-inbox-alerts.yaml`(PrometheusRule,与 `owner-epoch-alerts.yaml` 同法安装):
  ```bash
  kubectl apply -n observability -f deploy/k8s/player-inbox-alerts.yaml
  ```
  三组:`player-inbox.correctness`(critical:围栏倒退、epoch 超前、CursorAhead、Ack 异常结果、对账不一致)、`player-inbox.stuck`(warning:
  已投递未确认最老 > 1h、`max_delivery_count ≥ 5`、提醒积压 > 5 分钟、独占库连接池用满、SETTLED_UNKNOWN)、`player-inbox.info`(未决条目趋势,不打扰人)。
  连接池指标是 client_golang 的 `go_sql_*{db_name="player_inbox"}`,不在 `mysql-connection-alerts.yaml` 的 `mmorpg_sql_pool_*` 覆盖范围内。
  扫描型 gauge 只由领导者产出,规则一律 `max()`;scene 侧信号只在 `[PlayerInbox] metric=...` 日志里,LogQL 值班查询写在告警文件头。
  抓取前提:集群里要有指向 9270 的 ServiceMonitor / PodMonitor(或按注解抓取的 job),否则规则全部是哑的。
- **合服**:收件箱表按全局唯一的 player_id 存,没有 zone 列,合服不搬不改;`tools/merge_zone` 的合服前审计多了 `player_inbox` 一行(见 `docs/ops/merge-zone-runbook.md`)。

## MySQL 连接总账(2026-10-13,G2b:docs/handoff/launch-readiness-20261013/spec_G2b.md)

> 状态:已落码,**未编译、未测试**,待 Codex 验证。本节所有数值(1800、5Gi、各池大小、余量、告警阈值)都是按公式推算或凭经验取的,**待压测确认**,不能据此下容量结论(AGENTS §6)。

所有 zone 的 go/db、data-service、Java gateway 与全局的 friend、trade、player-inbox(2026-10-15 起)**共用 infra namespace 里那一台 MySQL**(`manifests/infra/mysql.yaml`,单副本)。
以前每个服务各自写一句「要算进 `max_connections`」,没有任何地方把它们加起来:`max_connections` 曾是 500,2 个 zone 的稳态恰好 500、零余量,4 个 zone 的稳态就要 920。
超限的后果不只是变慢:go/db 把 1040(Too many connections)当普通错误,耗尽 3 次重试后进没有消费者的死信,存盘不会自动落库(读代码推断,未实测)。
现在这笔账是部署脚本里的可执行函数,文档里不再手算。

- **总账在哪**:`tools/scripts/k8s_deploy.ps1`。
  - `Get-MysqlConnectionPlan`:规划档(见下),`max_connections` 按它的峰值取值。
  - `Get-MysqlConnectionLedgerInputs`:输入全部从权威文件读 —— 各服务 yaml 的池上限、各 manifest 的 `replicas`(`Get-ManifestDeploymentReplicas`)、
    `mysql.yaml` 的 `max_connections`(`Get-MysqlMaxConnectionsFromManifest`)。缺键、或 `max_connections` / `replicas` 不是恰好一处即 throw(fail-closed)。
  - `Get-MysqlConnectionLedger`:纯函数,算出每实例 / 每 zone / 稳态 / 滚动余量 / 峰值 / 余量。
  - 同一批池参数也镜像进 ConfigMap(db 的 `Placement` 池键、data-service 的 `PlayerName.*` 与 `RouteStore.MaxOpenConn` / `MaxIdleConn`、
    gateway 的 `spring.datasource.hikari.maximum-pool-size` / `minimum-idle`),让总账的输入等于集群实际值。清单与核对规则在 `Get-MysqlPoolMirror`:
    池上限必须是正整数,空闲数必须是不超过上限的整数(Go 服务下限 1 —— 0 在 Go 侧等于没配、取默认;gateway 的 `minimum-idle` 允许 0),
    db 的 `Placement.ExtraStoreConnMaxIdleSeconds` 必须是正整数。写路径入口的 `Assert-MysqlPoolMirror` 先于任何集群写操作把三个目标都核一遍
    (不看 `-SkipGoSvc` / `-SkipJavaSvc`,`infra-up` 也核),不合格即 throw;生成 ConfigMap 取值时再核一次。
    (2026-10-15 补:gateway 另镜像 `spring.datasource.hikari.connection-timeout`(现值 3000 毫秒,取连接的等待上限,不进总账),
    核对规则是不小于 250 的整数 —— HikariCP 把 0 当成无限等、低于 250 拒启。来由与探针改端口见上面「gateway 探针、管理端口与回退口径」。)
- **公式**(Z = zone 数;E_z = 每个 go/db 实例打开的额外 zone 家族库数;E_n = 非 zone 落点库数;F = route store 是否 db_first,取 0 / 1;R = 滚动期间每个 Deployment 多出的实例数):
  ```text
  db 每实例 = 60 + 8·E_z + 8·E_n       # Database.MaxOpenConn + Placement.ZoneStoreMaxOpenConn / NonZoneStoreMaxOpenConn(每个额外库一个池)
  ds 每实例 = 3·5 + 20 + 20·F          # snapshot / transaction_log / id_segment 各一个池 + 名字注册表 + route store(只在 db_first 时)
  gw 每实例 = 5                        # Java gateway 的 Hikari maximum-pool-size
  每 zone   = 2·db + 2·ds + 2·gw       # 各 2 副本
  全局      = 2·20(friend) + 2·20(trade) + 2·20(player-inbox,2026-10-15 起;池上限取 go/player_inbox/etc/player_inbox.yaml)
  steady    = Z·每 zone + 全局
  surge     = R·(db + ds + gw + 20 + 20 + 20)
  peak      = steady + surge + 运维余量
  ```
- **规划档**:Z=4、E_z=3(最坏情形:每个 go/db 实例都打开了其余全部 zone 的库,打开后常驻)、E_n=0(Phase 2 未开)、F=1(D1 第 2 阶段会切 db_first,提前计入)、R=2、
  运维余量 20(迁移 Job、备份、探针、merge_zone / route_tool、人工会话,估计值)。**峰值 1620,`max_connections = 1800`,余量 180(约 10%)**。
  (2026-10-15 更正:加入 player-inbox 两副本 × 20 后,按公式手算峰值为 1700、余量 100(约 6%),未运行;池上限以 player_inbox.yaml 的实际值为准,
  契约测试「规划档峰值 ≤ max_connections」会按实际值复核。余量收窄后再加全局池就要先重做规划档。)
  上限刻意不抬到「不可能触发」:它要保留保护作用。
  - R=2 的含义:滚动发布时每个 Deployment 按多 2 个实例算 —— maxSurge 的 1 个新 Pod,加 1 个仍在优雅退出、连接还没释放的旧 Pod
    (Deployment 控制器算 surge 时不计退出中的 Pod;K8s 行为,未在本机核实)。
  - 余量只覆盖「同一时刻一个 zone 在滚」。`all-up` 不带 `-WaitReady` 时各 zone 的滚动会重叠;带了,上一个 zone 仍在退出的旧 Pod 也可能与下一个 zone 重叠一个 grace 窗口(go/db 45s)。
    发布时用 `-WaitReady` 或逐 zone `zone-up`。
  - 不在规划档里:合服留下、已不在 zones 配置里的源区库(合服后按实际库数重算);login 的 `PasswordAuth`(K8s ConfigMap 不写该段,一旦启用每 zone 再 +2×20)。
  - `zones.10zones.yaml` 那种 10 个 zone 的配置按同一公式(Z=10、E_z=9)峰值是 4404,在单台 MySQL 上无解,与池怎么调无关(E_z=0 时稳态也有 2480):
    要上 10 个 zone 先完成 TiDB Phase 1。
  - 改规划档 = 重算 `max_connections` 与 MySQL Pod 内存,`mysql.yaml` 的两处注释、告警文件里额外库数的两个阈值一起改。
    Phase 2 打开 `PinOnCreate`(E_n ≥ 1,且非 zone 池要调大)之前必须重做规划档。
- **部署时怎么看**:`zone-up` / `all-up` / `infra-up` 的写路径打印一行(`*-down` / `*-status` 不打):
  ```text
  MySQL connection ledger: zones=<Z> steady=<稳态> peak=<峰值> max_connections=<上限> headroom=<余量>; plan zones=4 peak=<规划峰值> headroom=<规划余量>
  ```
  分号前是**本次部署**的账(E_z 取 Z−1,其余取规划档),分号后是规划档。`headroom` 为负就是超限。Z 按命令计,同一 zoneId 只算一次(2026-10-15 起):
  - `zone-up`:zones 配置里的 zone ∪ 本次 `-ZoneId`(`-ZoneId` 不在配置里也算进去:这个 zone 确实要部署);
  - `all-up` / `infra-up`:只数 zones 配置里的 zone。这两条命令不为 `-ZoneId` 部署 zone 级工作负载(db / data-service / gateway),
    它在这里用作全局 Go 服务与 battle 的 ZoneId、预建库的输入,本身不产生每 zone 的连接,不论缺省值 101 还是显式传入都不并入 ——
    以前并入,配置不含 101 时(例如 `zones.10zones.yaml` 的 1..10)Z 凭空多 1,可能打出虚假的超限告警。
  这与 `infra-up` 预建 `zone_<id>_db` 的口径(`Get-InfraZoneIds`:zones 配置 ∪ `-ZoneId`)有意不同:那边是给之后的 `zone-up -ZoneId` 预建库,并入是对的。
  按仓库当前值手算(未运行):`zones.json` 两个 zone 时是 `zones=2 steady=592 peak=948 max_connections=1800 headroom=852; plan zones=4 peak=1620 headroom=180`。
  (2026-10-15 加入 player-inbox 后同样手算、未运行:`zones=2 steady=632 peak=1028 ... headroom=772; plan zones=4 peak=1700 headroom=100`。)
  zones 配置文件缺席时 Z 取 1(`zone-up` 即 `-ZoneId` 那一个;`infra-up` 取总账允许的下限 1;`all-up` 在更早的入口预检读 zones 配置时就报错退出),
  行尾多一个 `zones_config=missing`。
- **超限**:任一份峰值大于 `max_connections` 时打一条英文 WARNING,文本以 `[MySQLConnectionLedger]OverLimit:` 开头(这一段刻意不留空格:控制台会按窗口宽度在空格处折行,留了就 grep 不到)。**只警告、不阻断部署**(是否升级为门禁待拍板)。两条出路:
  1. 先完成 TiDB Phase 1(`docs/design/tidb-phase1-and-launch-readiness.md`);
  2. 重做 `Get-MysqlConnectionPlan`,并同步 `mysql.yaml` 的 `max_connections` 与内存 limits。不要只把 `max_connections` 调大:
     内存按每条连接约 1MiB 常驻估算(待实测),limits 不跟上就是把 1040 换成 OOMKill,整台 MySQL 重启。
- **改 `mysql.yaml` 的两个坑**:
  - **只改 my.cnf 不会重启 mysqld。** `max_connections` 在 ConfigMap `mysql-config` 里,Deployment 模板不引用它的内容:只改这一行时运行中的实例仍是旧值,
    而总账与契约测试已经按新值算了,没有任何报错。两条路:维护窗口内 `kubectl -n <infra> rollout restart deploy/mysql`;或 `SET GLOBAL max_connections = <新值>`
    (只改运行值,立即生效,内存 limits 必须先到位;重启后仍读 `mysql.yaml`,所以文件里必须已经是新值)。
    **不要用 `SET PERSIST`**:它把值写进 datadir(在 PVC 上)的 `mysqld-auto.cnf`,启动时优先于 my.cnf(MySQL 8.0 行为,凭记忆,未在本机核实),
    用过之后再改 `mysql.yaml` + 重启都不生效,而总账与契约测试照样按文件里的值通过;已经用过的,先 `RESET PERSIST max_connections` 再改。
    改完用 `SHOW VARIABLES LIKE 'max_connections'` 核对实际值:容器 open files 上限过低时 MySQL 会自己把它压低,
    只在启动日志里留一条警告(MySQL 行为,未在本机核实)。
  - **改 Deployment 模板(例如内存)会触发 `Recreate`,全服唯一的 MySQL 停一次**,所有 zone 的数据层一起停。G2b 把内存 requests / limits 从 512Mi / 1Gi 改成 1Gi / 5Gi,
    所以**第一次 apply 这份 `mysql.yaml` 就属于这种**,只能在维护窗口做。停机期间 go/db 的写任务连不上库,走的还是「耗尽重试 → 死信」那条链。
    **L-04(连接错误时写延后、不耗重试预算)落地并部署之前,不要在有真实存档的环境 apply**;确需先 apply,就在维护窗口内先停写,
    确认 db_task lag、重试队列、死信都为 0 再动。见 `docs/handoff/godb-pipeline-handoff-20261013.md` §2.4。
- **观测**:
  - 指标 `mmorpg_sql_pool_*{sql_client,pool}`(`go/shared/sqlpoolstats`):把 `sql.DBStats` 按**池类别**聚合,label 里没有库名。
    共 10 个:`pools`、`max_open_connections`、`open_connections`、`in_use_connections`、`idle_connections`、`wait_count_total`、`wait_duration_seconds_total`、
    `max_idle_closed_total`、`max_idle_time_closed_total`、`max_lifetime_closed_total`。
    - go/db:`sql_client="db"`,端口 **:9160**,`pool` = `home`(本 zone 库)/ `extra_zone` / `extra_non_zone`。
      K8s 上 go/db 的 `/metrics` 以前是关的(ConfigMap 不写 `MetricsListenAddr` = 不起端口);现在 db ConfigMap 写 `MetricsListenAddr: ":9160"`,
      与 `manifests/go-svc/db.yaml` 的 `metrics` 端口、`prometheus.io/port` 注解三处必须一致(契约测试钉住)。
      只部署 manifest 而 ConfigMap 没写这一行时抓取会失败,`owner-epoch-alerts.yaml` 的抓取前提也不成立。
    - data-service:`sql_client="data_service"`,端口 :9260,`pool` = `snapshot` / `transaction_log` / `id_segment` / `player_name`;`player_route` 只在 `RouteStore.Mode=db_first` 时出现。
    - friend / trade / Java gateway 没有导出池指标。
  - 告警 `deploy/k8s/mysql-connection-alerts.yaml`(PrometheusRule,与 `owner-epoch-alerts.yaml` 同法安装):
    ```bash
    kubectl apply -n observability -f deploy/k8s/mysql-connection-alerts.yaml
    ```
    两组 5 条。`sql-pool.client` 组 3 条用上面的池指标,现在就有数据:`SqlPoolSlowAcquire`(取连接排队慢)、`SqlPoolSaturated`(池长期用满,只作趋势)、
    `DbExtraStorePoolsBeyondPlan`(额外库数超过规划档;两个阈值与 `Get-MysqlConnectionPlan` 的 `ExtraZoneStores` / `NonZoneStores` 同源,契约测试核对)。
    `mysql.connection-budget` 组 2 条(`MysqlConnectionsRefused` / `MysqlConnectionsNearMax`)需要 mysqld_exporter:仓库里还没有部署它(待拍板),
    在那之前这一组没有数据、不会触发也不会误报,也就是说 **MySQL 服务端的连接数现在仍然看不到**。
- **额外落点库池的旋钮与回退口径(批 C)**:go/db 原来的一个旋钮 `Placement.ExtraStoreMaxOpenConn` 拆成 `ZoneStoreMaxOpenConn` / `ZoneStoreMaxIdleConn`(每个额外 zone 家族库)
  与 `NonZoneStoreMaxOpenConn` / `NonZoneStoreMaxIdleConn`(每个非 zone 落点库),另加 `ExtraStoreConnMaxIdleSeconds`(默认 60,冷库的池回到 0 条);
  旧键只作回落(`docs/design/player-storage-placement.md` §6.1)。db ConfigMap 写的是这五个新键。
  **回退批 C 必须用旧 commit 重跑 `k8s_deploy.ps1 zone-up` 重新生成 ConfigMap,不能只 `kubectl rollout undo deploy/db`**:
  undo 只换回旧镜像,ConfigMap 里仍只有新键,旧二进制不认它们、按默认 8 / 2 开池(go-zero 对未知配置键静默忽略,凭记忆,未核实)。
  新键若已被调到 8 以下,回退后的实际池就比总账大。
- **验证(交 Codex,未运行)**:`pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1`。本项新增 9 例(批 A 7、批 B 2),当前文件 89 → 98:
  规划档与 `zones.json` 的峰值 ≤ `max_connections`;公式自检(复现 2026-10-12 核算的 Z=4 → 920、Z=2 → 500);ConfigMap 镜像值 == 服务 yaml;
  连 MySQL 的 ConfigMap 只有总账认识的那几个;写路径打印那一行;10 个 zone 打超限警告且不阻断;db 指标端口三处一致;告警阈值与规划档同源。
  `.github/workflows/deploy-config-tests.yml` 的触发路径已加上 gateway 的 `application.yaml` 与 `deploy/k8s/*.yaml` / `*.json`,改这些输入也会跑契约测试。
  (2026-10-15 更正注:上面的「89 → 98」只是本项合入当时的计数,此后该文件又陆续加了用例(削峰 G4b、登录排队合入门槛等),用例数不再写死,
  判据以 `Complete-TestRun` 汇总的 fail=0 为准。)

## Redis 连接上限与总账(2026-10-15,P0-E:docs/handoff/global-singletons-p0-findings-20261014.md「## P0-E」)

> 状态:已落码,**未编译、未测试**,待 Codex 验证。`MAXCLIENTS=40000`(负责人定)、GOMAXPROCS 按 2、运维余量 50、80% 告警水位都是按公式推算或拍板取的,
> **取值待实测 / 压测复核**(AGENTS §6),不能据此下容量结论。

所有 zone 的 Go 服务连接池、Java gateway、每个 C++ 进程与全局池服务**共连 infra namespace 里那一个 Redis**(`manifests/infra/redis.yaml`,单实例)。
以前它没有显式 `maxclients`:上限是 Redis 缺省 10000,而且会被容器 nofile 硬限悄悄压低(需要 maxclients + 32 个 fd,不够就自己下调,只在启动日志里留一条;外部常识,待实测)。
连接数随 zone 数线性增长,而 go-redis 的池没有后台回收,连接数停在历史并发峰值;到线之后新连接被拒,而且**客户端多半看不出原因**(见下)。

- **上限在哪**:`redis.yaml` 的启动薄壳里 `MAXCLIENTS=40000` 只定义一次,两条 `exec redis-server`(有口令、无口令)都带 `--maxclients "$MAXCLIENTS"`;
  启动前打印一行 `redis fd limit: soft=<软限> hard=<硬限> need=<MAXCLIENTS+32> maxclients=<MAXCLIENTS>`,硬限不够时再打一行 `ERROR: nofile hard limit ...`。
  **硬限不够不拒启**:唯一的 Redis 起不来等于全服停,这是有意的 fail-open,靠这行日志、部署后核对与(第 3 批的)告警兜住;补救是调节点上容器运行时的 `LimitNOFILE`
  (K8s 清单层没有声明 ulimit 的字段)。不设 `timeout`(会踢掉常驻的空闲池连接与 C++ 唯一的那条连接),就绪探针不改。
  `deploy/docker-compose.yml` 的 redis 同值 `--maxclients 40000`(本机要 `--force-recreate redis` 才生效,见该文件注释)。
  静态契约:`tools/scripts/tests/redis_maxclients_contract.tests.ps1`(恰好定义一次、两条 exec 都带、打印 fd 限值、compose 同值、不 exit)。
- **总账在哪**:`tools/scripts/k8s_deploy.ps1` 的「Redis 连接总账与重启闸」一节,形状与纪律同上面的 MySQL 连接总账。
  `Get-RedisConnectionPlan`(规划档;zone 数与滚动余量取自 `Get-MysqlConnectionPlan`)、`Get-RedisMaxClientsFromManifest`(读 `MAXCLIENTS`,不是恰好一处即 throw)、
  `Get-RedisPoolMirror`(go-redis 直连池上限,读服务 yaml,不是正整数即 throw)、`Get-RedisConnectionLedgerInputs`(每副本上限常数表,每行带代码出处;
  显式池上限经 `Get-RedisPoolMirror` 读;副本数读各 manifest)、`Get-RedisConnectionLedger`(纯函数)、`Write-RedisConnectionLedger`(写路径打印)。
- **显式池上限(第 3 批,2026-10-16)**:连共享 Redis 的 go-redis 直连客户端都显式设了 `PoolSize`,值在各服务 yaml 里(当前都是 20 = 以前 K8s 上
  GOMAXPROCS 按 2 时的缺省值,所以总账数字不变),ConfigMap 生成器镜像同一个键、总账读同一个键,池上限不再随 CPU limit 漂:

  | 服务 | 键(服务 yaml) | ConfigMap |
  |---|---|---|
  | login | `go/login/etc/login.yaml` 的 `Node.RedisClient.PoolSize` | go-svc-login-config |
  | db | `go/db/etc/db.yaml` 的 `ServerConfig.RedisClient.PoolSize` | go-svc-db-config |
  | data-service | `go/data_service/etc/data_service.yaml` 的 `DevRedis.PoolSize` | go-svc-data-service-config |
  | player-locator | `go/player_locator/etc/player_locator.yaml` 的 `RedisClient.PoolSize` | go-svc-player-locator-config |
  | trade | `go/trade/etc/trade.yaml` 的 `SharedRedis.PoolSize` | go-svc-trade-config |
  | player-inbox | `go/player_inbox/etc/player_inbox.yaml` 的 `CoreRedis.PoolSize` | go-svc-player-inbox-config |

  guild 2026-10-16 起由接入 K8s 那条线一并计入(`Get-RedisPoolMirror -Target guild` 读 `RedisClient` / `PlayerLocatorRedis` / `MergeMarkerRedis`
  三段的 `PoolSize`),ConfigMap 由它自己的生成器写。
  Go 侧语义:0 / 不写 = go-redis 缺省 10 × GOMAXPROCS(兼容旧 ConfigMap,滚动升级期间旧配置照常加载),负数起服即失败;部署侧要求正整数,
  写路径入口算总账时先于任何集群写操作把这些键读一遍,不合格就拒绝(`Redis 连接池镜像:...`)。各服务启动日志打一行 `pool_size=<生效值>`,
  用来核对 ConfigMap 里的值真的被加载(go-zero 对不认识的键静默忽略)。只设 `PoolSize`,不设 `MaxActiveConns`:Pub/Sub 专用连接走 `NewConn`、
  不占池名额,但受 `MaxActiveConns` 约束,池满时订阅会直接失败(go-redis v9.16.0 / v9.17.3 / v9.18.0 的 `newConn`)。
  **改值 = 重算总账**:看部署时打印的 `Redis connection ledger` 一行;池等满时调用方排队(go-redis `PoolTimeout`),db 可看 `db_redis_pool_wait_total` /
  `db_redis_pool_timeouts_total`。本机 Windows 以前是 10 × 核数,现在同样是 20 —— 压测对比时注意这一口径变化。
  **设不了的**:go-zero 封装的客户端(data-service 的 `MappingRedis`、scene-manager、match、chat、friend 的共享库句柄)—— go-zero v1.9.2 / v1.10.0 的
  `redis.RedisConf` 没有池参数,`redisclientmanager.go` 不设 `PoolSize`、固定 `MinIdleConns: 8`,同一进程同一地址共用一个池。
  这些仍按缺省池(10G)记账,CPU limit 的输入钉子只对它们生效。
- **公式**(P = 该服务 yaml 里的显式 `PoolSize`;G = GOMAXPROCS,只用于缺省池。Go 1.27.2 容器感知,CPU limit 500m / `"1"` 预期都是 2;**未实测,待 S6**;
  按 2 记。go-redis v9 缺省池上限 = 10 × G。池没有后台回收,一律按池上限记账):
  ```text
  login           每副本 = P + 1       # go-redis 池 + 任务结果 Pub/Sub 专用连接
  db              每副本 = P + 2       # 主池 + WAITAOF 专用客户端(go/db/internal/kafka/durable_enqueue.go,代码写死 2;两项 WaitAOF 都写 0 时不建,按开计)
  player-locator  每副本 = P
  data-service    每副本 = P + 10G     # DevRedis(go-redis,显式)+ MappingRedis(go-zero,缺省池;K8s 上两段都指向共享 Redis)
  scene-manager   每副本 = 10G         # go-zero,缺省池
  gateway(Java)  每副本 = 2           # Lettuce 共享连接 + 限流专用连接(staging / prod 缺省开限流;dev 关时实际 1,按 2 记)
  match / chat / friend 每副本 = 10G   # go-zero,缺省池
  trade           每副本 = P + 1       # 资产通道 + 落地唤醒订阅(资产通道开关缺省关,按开计)
  guild           每副本 = 3P + 1 + 4  # 全局库 / 在线查询 / 合服标记三个显式池 + 落地唤醒订阅 + 摘要 bump 专用池(代码写死 4)
  player-inbox    每副本 = P           # 核心 Redis 一个 go-redis 池,没有 Pub/Sub
  mail            每副本 = 8           # 代码里写死 PoolSize 8
  gate / scene / battle 每进程 = 1     # node.cpp 唯一的 tlsRedis.Connect:每进程一条,不是每线程
  每 zone = 2·(P+1) + 2·(P+2) + 2·(P+10G) + 2·P + 2·10G + 2·2 + gate 台数 + scene 台数
  全局    = 3·2·10G + 2·(P+1) + 2·(3P+5) + 2·P + 2·8 + battle 台数   # match / chat / friend、trade、guild、player-inbox、mail 各 2 副本
  steady  = Z·每 zone(含各 zone 的 gate / scene)+ 全局
  surge   = R·(各客户端每副本之和 + 3)   # R = 2,同 MySQL 总账;C++ 按 gate / scene / battle 三个工作负载计
  peak    = steady + surge + 运维余量 50   # 探针、运维 redis-cli、merge_zone / route_tool 等一次性工具(估计值)
  ```
  P=20、G=2、缺省台数(gate 2、scene 4、battle 1)时每 zone 256 条(其中 C++ 6 条),全局 349 条(含 guild 130、mail 16 条)。
  db 的 +2 与 guild 一行是 2026-10-16 存盘线、guild 接入 K8s 线加进 `Get-RedisConnectionLedgerInputs` 的;数字以代码表与部署时打印的总账一行为准。
  - player-inbox 2026-10-16 起计入:核心 Redis 一个 go-redis 池(`go/player_inbox/internal/svc/servicecontext.go` 的 `openCoreRedis`,
    围栏上界、在线提醒读 location 与后台循环选主锁共用),池上限 `CoreRedis.PoolSize`,没有 Pub/Sub。
  - 不计入(接上时在常数表加一行):client-rpc-router(不连 Redis)、
    login 的 PasswordAuth 失败配额池(K8s 不写该段,启用后每 login 副本 +4)、match 的 MatchRedis 与 chat 的 ChatRedis(在 redis-match-cluster 上)。
  - 契约测试钉住:显式池那几行等于服务 yaml 的值、ConfigMap 镜像同一个值、yaml 写 0 / 负数 / 非整数时部署侧 throw;
    有缺省池的行「manifest 的 CPU limit ≤ G 核」—— 调大这些服务的 CPU limit 会悄悄放大缺省池,那时测试变红,逼着重算这一列。
- **取值**:规划档(Z=4,来自 MySQL 规划档)峰值 2027;`MAXCLIENTS=40000` 时 80% 水位 32000,按每 zone 256 条约能承载 121 个 zone,需要 40032 个 fd。
  契约测试钉住「规划档峰值 ≤ 80% × MAXCLIENTS」。Z≈220 或目标形态(每 zone gate 5~22、scene 9~44,login 4 核)落地时要重算。
  单实例按连接数撑到 100 个 zone 只是算术:命令量约 3 个满载 zone 就到顶,这个上限真正的用处在按用途 / 按玩家拆分之后(每个拆出的实例各自显式 `--maxclients`、各自记账)。
  每条连接的内存是外部常识(空闲几 KB,最坏约 20KB),4 万条最坏约 0.8GB;`redis.yaml` 只有 512Mi 的 requests、没有 limits。
- **部署时怎么看**:`zone-up` / `all-up` / `infra-up` 的写路径在 MySQL 那一行之后打印(`*-down` / `*-status` 不打):
  ```text
  Redis connection ledger: zones=<Z> steady=<稳态> peak=<峰值> maxclients=<上限> warn_at=<80% 水位> headroom=<余量>; plan zones=4 peak=<规划峰值> headroom=<规划余量>
  ```
  zone 集合与 MySQL 那一行同一口径(契约测试比对两行的 `zones=` 相等);各 zone 的 gate / scene 台数按命令行或 zones 配置算(拆分池取两池之和,Agones 自动伸缩取 `-AgonesMaxReplicas`)。
  按仓库当前值手算(未运行;含 mail、guild、db 的 WAITAOF 客户端):`zones.json` 两个 zone 的 `zone-up` 是 `zones=2 steady=861 peak=1515 maxclients=40000 warn_at=32000 headroom=38485; plan zones=4 peak=2027 headroom=37973`。
  任一份峰值超过 80% 水位时打一条英文 WARNING,以 `[RedisConnectionLedger]OverWarnRatio:` 开头,**只告警、不阻断**(与 MySQL 那份同一纪律,是否升级成门禁两份一起拍板)。
- **部署后核对**:非 DryRun 且带 `-WaitReady` 的 `infra-up` / `all-up`,在 redis-0 就绪后 exec 进 Pod 读 `CONFIG GET maxclients`,与清单的 `MAXCLIENTS` 比较
  (口令取 Pod 自己的 `REDIS_PASSWORD`,经 `REDISCLI_AUTH` 交给 redis-cli,不经本机命令行、不进日志)。相等打 `Redis maxclients check OK`;
  不等打 `[RedisMaxClients]Mismatch:`(多半是 nofile 硬限把它压低了,看 `redis fd limit:` 与 `ERROR` 两行),读不到打 `[RedisMaxClients]CheckFailed:`。都只告警。
- **重启闸 `-ConfirmRedisCoreRestart`**:`redis.yaml` 的 Pod 模板(命令、env、探针、镜像、resources 任何一处)一变,apply 就重启唯一的 redis-0 ——
  **全服登录中断并重放 AOF**。所以 `Apply-Infra` 在它的任何写操作之前先比较:
  - 渲染后的 `spec.template` 段(去掉 YAML 注释与空行;启动薄壳字面块里的行原样保留)的 SHA-256 前 16 位,写在 StatefulSet **自己的** metadata 注解
    `mmorpg.io/redis-pod-template-digest` 上(不在 Pod 模板里,改它不滚动);
  - 线上既没有 `sts/redis` 也没有旧的 `deployment/redis` → 放行(全新集群);线上注解与本次摘要相同 → 放行;不同,或线上没有这条注解 →
    没给 `-ConfirmRedisCoreRestart` 就拒绝,报错写明「apply 会重启 redis-0,全服登录中断并重放 AOF」,本次不做任何写操作;
    给了开关就放行并打 `[RedisCoreRestart]Confirmed:` 告警。
  - 线上没有 `sts/redis`、但还有 2026-09-19 之前的 `deployment/redis`(emptyDir)→ 同样要 `-ConfirmRedisCoreRestart`:`Apply-Infra` 会先删它再建
    StatefulSet,Redis 停机、数据不迁移(以前这种集群被当成全新集群放行)。查询 `sts` / `deployment` 时 kubectl 失败一律拒绝(fail-closed)。
  - DryRun 不查集群,只打 `Redis restart gate: skipped in DryRun ...` 一行。只改注释不触发。
  - **盲区**:比较的是上次经本脚本 apply 的模板(注解),不是线上真实的 `spec.template`。有人用 `kubectl edit` / `set image` / `patch` 带外改过
    `redis.yaml` 里声明了的字段(镜像、resources、命令、探针)时,闸判「unchanged」放行,而 apply 的三方合并会把它们改回清单值、照样重启 redis-0;
    所以放行那一行写的是 `pod template unchanged since the last apply by this script ... would be reverted`,不是「不会重启」。
    对 sts/redis 带外改过之后,先把同样的改动落进 `redis.yaml` 再跑 infra-up。`rollout restart` 加的 `restartedAt` 注解不在清单里,三方合并保留它,不受影响。
    用 server-side dry-run 的模板与线上模板比较可以补上,待负责人拍板与实测。
    另一个盲区:`kubectl delete sts redis --cascade=orphan` 之后(改 volumeClaimTemplates 之类不可变字段的常规做法)redis-0 Pod 与 PVC 仍在服务,
    闸查不到 sts、也查不到旧 Deployment,按「全新集群」放行;apply 新建的 StatefulSet 收养孤儿 Pod,模板不一致就重建 redis-0。做过这类操作后,
    下一次 infra-up 放进维护窗口。
  - redis.yaml 在 `Apply-Infra` 里只读一次:过闸算摘要、实际 apply、`-WaitReady` 部署后核对的期望 MAXCLIENTS 都取自同一次读取的内容
    (两次读之间文件被改时,不会出现「闸按旧内容放行、apply 新模板」,也不会拿新值去核对刚 apply 的旧值)。
  - 带 `-WaitReady` 确认重启时,`Wait-ForStatefulSetReady` 的时限是 `-WaitTimeoutSeconds`(缺省 180s),而 `startupProbe` 给 AOF 重放留了 300s、
    大库重放可能更久:等待超时会 throw,**其后的步骤全部没做** —— maxclients 核对、等 `kafka-topic-init` Complete、battle 池(含新的
    `battle-node-config`)、全局 Go 服务,all-up 还有全部 zone。必须重跑同一条命令(安全:注解已更新,会判 unchanged);
    或一开始就给大一点的 `-WaitTimeoutSeconds`。
  - `dev_tools.ps1`(`k8s-infra-up` / `k8s-all-up` / `k8s-release-all`)与 `k8s_image.ps1`(`release-all`)只在被指定时透传同名开关。
  - 它取代了 G4a 设计稿(`docs/design/shared-redis-split-by-purpose.md` §4.2)里「给了 `-RedisCoreMaxMemory` 就必须给 Confirm」的同名开关规划,语义更宽。
  - **运维后果:本项合入后第一次 `infra-up`(以及不带 `-SkipInfra` 的 `all-up`、`k8s-release-all`)必须带 `-ConfirmRedisCoreRestart`** ——
    线上的 `sts/redis` 还没有摘要注解,而且这次 `maxclients` 改动本身就改了 Pod 模板。在维护窗口之前,这类命令被拒是预期行为;
    zone 级发布用 `zone-up` 或 `all-up -SkipInfra`,不受影响。之后模板不变的 `infra-up` 不再需要它。
- **上线顺序**(已有环境;AI 不碰集群,由负责人或运维执行):
  1. **到线之前**先在线调大(立即生效、不重启;不持久,redis-0 任何一次重启都回到清单值):
     ```bash
     kubectl -n <infra> exec redis-0 -c redis -- sh -c 'if [ -n "$REDIS_PASSWORD" ]; then REDISCLI_AUTH=$REDIS_PASSWORD; export REDISCLI_AUTH; fi; exec redis-cli config set maxclients 40000'
     ```
     运行期可调属外部常识,执行后用下面第 3 步的 `CONFIG GET` 核对。
  2. 维护窗口内让清单生效(= 重启一次 redis-0):先停写,确认 db_task 积压、重试队列与死信都为 0(存盘关键写补上 WAITAOF 之前,任何一次 Redis 重启都按维护窗口处理;
     `docs/handoff/godb-pipeline-handoff-20261013.md` §3.5),再
     `pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-infra-up -WaitReady -ConfirmRedisCoreRestart <其余照常的参数>`。
     计划内重启走 SIGTERM,退出前会刷 AOF(30 秒 grace);丢写只在非正常退出。`infra-up` 会连带 apply `mysql.yaml`(其 Recreate 约束见上一节),建议两次重启并进同一个窗口。
  3. 核对:输出里有 `Redis maxclients check OK`;`kubectl -n <infra> logs redis-0 -c redis` 里有 `redis fd limit:` 一行、没有 `ERROR`、没有 Redis 自己下调 maxclients 的日志。
  4. 回滚:在线 `CONFIG SET maxclients <旧值>`;清单回退需要再重启一次(同样要 `-ConfirmRedisCoreRestart`)。以后要调大:先在线改,再改清单,等下一次窗口。
- **排查**(都在 redis-0 里执行,口令同上经 `REDISCLI_AUTH`):`redis-cli config get maxclients`;`redis-cli info clients`(`connected_clients`);
  `redis-cli info stats`(**`rejected_connections`**,因上限被拒的累计数)。`CLIENT LIST` 不好用:全仓没有客户端设连接名,login / db 连 lib-name 也是空的。
  - **客户端日志里不一定出现 `max number of clients`,以服务端 `rejected_connections` 为准**:go-redis v9 握手时把 HELLO 收到的任何 Redis 错误当成「不支持 HELLO」吞掉,
    Go 侧多半只看到 EOF / 连接重置,release 档还可能是 `failed to authenticate`(容易与「C++ 不发 AUTH」那一项混淆)。
    C++ 的 `Redis disconnected` / `Redis connect attempt failed` 两行 2026-10-16 起带 `reason=<hiredis errstr>`:hiredis 在建连后、发出第一条命令前
    收到的错误回复(到线时的 `ERR max number of clients reached`)会原样进 errstr;更常见的是错误回复被当成 AUTH 的应答
    (`Redis AUTH rejected by ...` 一行带服务端原文)或排队命令的应答,之后断线一行是 `reason=Server closed the connection`。
  - **到线之后 redis-cli 自己也进不去**(Redis 不给管理连接留名额,外部常识,待实测),所以**在线调大必须在到线之前做**;已经到线时只能先停掉一部分客户端腾出名额,
    或重启 redis-0(回到清单值)。
- **接入 redis-ha / 按用途拆分 / 分片时**:每个 redis-server 都要显式 `--maxclients` 并按用途重算总账;redis-ha 还要算 haproxy:节点上限 ≥(代理副本数 + maxSurge)× `maxconn` + 余量
  (现在 3 × 20000,见 `redis-ha.yaml` 文件头)。按用途拆分后每个进程对每个实例各开一个池,单实例连接数下降、进程总连接数上升;按 player_id 分片后每片的连接数约等于今天单实例的总数。
- **第 3 批**:go-redis 直连客户端显式 `PoolSize`(见上「显式池上限」)与 C++ 断连日志带原因已于 2026-10-16 落码(未编译、未测试)。
  **未做**:告警(connected_clients ÷ maxclients、rejected_connections 增量、生效值 < 清单值;依赖 redis_exporter,待拍板,届时照 `mysql-connection-alerts.yaml` 写);
  go-zero 客户端的池上限(要换掉 go-zero 封装,不在本批)。player-inbox 的 `CoreRedis.PoolSize` 已于 2026-10-16 补上。在告警落地前只有部署后核对与输入钉子。
- **验证(交 Codex,未运行)**:`pwsh -NoProfile -File tools/scripts/tests/redis_maxclients_contract.tests.ps1`(第 1 批静态契约);
  `pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1`(第 2 批:规划档 ≤ 80%、输入钉子、公式自检、写路径打印、重启闸四种判定与拒绝文本、
  旧 Deployment 的拒绝、读线上那一步的 fail-closed、摘要语义、infra-up DryRun 的注解、两个包装入口透传;第 3 批:显式池上限等于服务 yaml、
  ConfigMap 镜像 PoolSize、PoolSize 非法时 fail-closed)。Go 侧与 C++ 侧的编译与单测命令见 `docs/PROGRESS.md` 2026-10-16「P0-E 第 3 批」条目。

## Kafka 审计 topic 预建(kafka-topic-init,2026-09-08,node-id-overhaul-plan §2.0c)

data_service 的两条落库消费者(`transaction_log_topic` / `player_snapshot_topic`,C++ scene 生产、key=player_id)对分区数有**不可变契约**
(`go/data_service/etc/data_service.yaml` 的 `Kafka.TransactionLogPartitions: 6` / `Kafka.SnapshotPartitions: 3`,`kafkautil.EnsureTopics` 会拒绝与 broker 现状不一致的值)。
而 broker(compose 与 `manifests/infra/kafka.yaml`)都开着 `auto.create.topics.enable` 且 `num.partitions=1`:只要 scene 先于预建发出第一条消息,
topic 就被自动建成 1 分区,EnsureTopics 从此永远报 `partition contract mismatch`,两条消费者一条都起不来,唯一症状是每 30s 一条 Error 日志,
流水 / 快照全被保留期吃掉。所以 topic 必须由**部署侧**在任何 scene 起来之前按契约预建,不能依赖自动创建:

- **有效 topic 名带代号**:`<基名>_g<Kafka.TopicGeneration>`,当前是 `transaction_log_topic_g1`(6 分区)/ `player_snapshot_topic_g1`(3 分区),
  `retention.ms` = `Kafka.RetentionMs`(30 天)。生产者(C++)与消费者(data-service)必须用同一个有效名。
- **compose**:`deploy/docker-compose.yml` 的 `kafka-topic-init` 一次性容器在 per-node topic 之外预建这两个 topic(`create_topic` 现在带分区参数;
  代号来自 `KAFKA_INIT_AUDIT_TOPIC_GENERATION`,默认 1),建完核对 `PartitionCount`,不符则退出 1(`docker compose ps -a` 里能看到)。
- **K8s**:`manifests/infra/kafka-topic-init.yaml` 是一个一次性 Job,`infra-up` 在全部 infra manifest(`Apply-Infra` 的 `$infraManifests`)之后、任何 zone 之前 apply 它
  (`Apply-KafkaTopicInitJob`);占位 `__KAFKA_AUDIT_*__` 由脚本从 `go/data_service/etc/data_service.yaml` 填(基名 / 分区数 / 保留期 / 代号,
  与 data-service ConfigMap 同源,manifest 里不写任何数字)。Job 自己等 broker 可达(最多 5 分钟),`kafka-topics.sh --create --if-not-exists`,
  再 `--describe` 核对 `PartitionCount`,不符即失败(`backoffLimit: 6`,不会假装成功)。每次 `infra-up` 都先
  `kubectl delete job kafka-topic-init --ignore-not-found` 再 apply —— Job template 不可变,而且必须能重跑(见下面「Kafka Pod 重启」)。
- **`-WaitReady` 会等它 Complete**(`infra-up` / `all-up`,在 etcd quorum 之后):这样 `all-up` 里的 zone 一定晚于 topic 建好。
  不带 `-WaitReady` 时,`zone-up` 之前自己核对 `kubectl -n mmorpg-infra get job kafka-topic-init`(COMPLETIONS `1/1`)/
  `kubectl -n mmorpg-infra logs job/kafka-topic-init`;`infra-status` 现在会列 `job`。
- **改分区数 = 代号 +1,绝不原地扩分区**:原地 `--alter --partitions` 会重映射 key=player_id 的哈希,同一玩家的流水顺序就断了,
  而且 EnsureTopics 的 `__mmorpg_partition_contract_*` 标记 topic 也会与新值冲突。正确做法是**离线换代:停写 → 排空 → 三方同改 → 消费者先起、生产者后起**,
  完整步骤见 [`docs/ops/audit-topic-generation-bump-runbook.md`](../../docs/ops/audit-topic-generation-bump-runbook.md)。
  **(2026-10-13 更正)** 这里以前写的是「建出 `*_g2` → 滚 data-service 与 scene → 老 topic 排空后再 `--delete`」:data-service 一切到 g2
  就不再消费 g1,而 scene 没滚完之前仍在写 g1 —— g1 的尾部无人消费,保留期一到就丢。必须先停掉所有 scene、等 g1 的 lag 归零,再切。
  data-service ConfigMap 里镜像了 `Kafka.TopicGeneration`,当前集群消费第几代一眼可见。
- **什么时候必须重跑预建**:Kafka 已经是 StatefulSet + PVC(见下面「Kafka:StatefulSet + PVC」),**普通的 Pod 重启不再丢 topic**。
  仍然必须重跑的是这三种情况:① 第一次从旧的 `Deployment kafka`(emptyDir)切过来;② `infra-down` 删过 namespace(PVC 跟着删);
  ③ 有人手工删过 topic。这三种情况下 topic 是空的,任何在跑的 scene 一发消息就会把 `*_g1` 自动建成 1 分区。
  做法是**在让 scene 产消息之前**跑 `pwsh -File tools/scripts/k8s_deploy.ps1 -Command infra-kafka-topics -WaitReady`
  (止血路径,不过发布门禁;`dev_tools.ps1` 还没透传这个命令),之后再 `rollout restart` 各 zone 的 scene 与 data-service。
  如果晚了一步、Job 因 `PartitionCount=1` 失败,删掉那个被抢建的 topic 再重跑:
  `kubectl -n mmorpg-infra exec sts/kafka -- /opt/kafka/bin/kafka-topics.sh --bootstrap-server localhost:9092 --delete --topic <名>`。
- **`kubectl exec deploy/kafka` 已失效**:broker 是 StatefulSet,用 `sts/kafka` 或 Pod 名 `kafka-0`。
- 核对 broker 现状:`kubectl -n mmorpg-infra exec sts/kafka -- /opt/kafka/bin/kafka-topics.sh --bootstrap-server localhost:9092 --describe --topic transaction_log_topic_g1`
  (看 `PartitionCount: 6` 与 `Configs: retention.ms=2592000000`)。
- **保留期是声明出来的,不是继承来的**:`--create --config retention.ms=...` 只在**新建那一刻**生效,已存在的 topic
  (被生产者抢先 auto-create 的、或上一代 Job 用别的值建的)会静默吃 broker 默认保留期。所以 Job 与 compose 的 init
  在建完之后都会再用 `kafka-configs.sh --alter --add-config retention.ms=...` 声明一次并读回核对,不一致就失败。
  与分区数不同,`retention.ms` 是可变配置,重设它没有任何寻址风险。规则见下面「Kafka 保留期」。

## 控制面命令 topic:per-node topic 已停建(2026-09-08)

同一个 `kafka-topic-init` 现在还预建两个**控制面命令 topic**:`gate-cmd_g<N>` 与 `scene-cmd_g<N>`,
分区数取自 `bin/etc/base_deploy_config.yaml` 的 `Kafka.CommandTopicPartitions`(当前 256)/
`Kafka.CommandTopicGeneration`(当前 2),`retention.ms=3600000`(1 小时)。
Go 服务侧的同一对值由 `k8s_deploy.ps1` 的 `Add-GoSvcCommandTopicEnv` 从同一文件注入(见上面 friend 一节「Kafka 命令 topic 代号」条)。
完整论证见 `docs/design/control-plane-topic-partitioning-20260908.md`。

- **`gate-{id}` / `scene-{id}` / `centre-{id}` 这些 per-node topic 不再被创建**。compose 里那三个
  `KAFKA_INIT_GATE_COUNT` / `KAFKA_INIT_SCENE_COUNT` / `KAFKA_INIT_CENTRE_COUNT` 循环已经删掉,
  K8s 侧本来也没建过。**已经存在的旧 topic 不需要删**:生产者切换后没人再写,按各自的 `retention.ms`
  自然过期;想早点回收磁盘就手工 `--delete`(先确认 `--describe --group gate-group-<id>` 已无成员)。
- **为什么改**:一个节点一个 topic 在 ~10 万 scene 进程的目标规模下,单集群 10 万 topic 会压垮
  controller 元数据、每分区内存与 rebalance 时间;更要命的是 topic 名和 consumer group 名里都带
  被 `node_allocator` 立刻回收复用的 `node_id`,冻结但仍是 group 成员的前任会和继任者一起待在
  `gate-group-5` 里,Kafka 只把分区判给其中一个 —— 判给前任时,继任者的登录绑定 / 进世界 / 踢人 /
  战斗绑定与结算全部被前任消费掉再按 `target_instance_id` 丢弃,继任者一条也收不到。
  现在按 `partition = node_id % P` 寻址、消费端 `assign` 自己的分区不进 group,这条路径就不存在了。
- **分区数是不可变契约**,与审计 topic 同一条纪律:改分区数 = `Kafka.CommandTopicGeneration` +1
  换一批新 topic,绝不原地 `--alter --partitions`(会把 `node_id % P` 重映射,命令落到没人 assign
  的分区上,静默全丢而且 Kafka 不报错)。改的时候四处必须同拍:`bin/etc/base_deploy_config.yaml`、
  `go/shared/kafkacmd/command_topic.go` 的默认值(或 `KAFKA_COMMAND_TOPIC_PARTITIONS` /
  `KAFKA_COMMAND_TOPIC_GENERATION` 环境变量;K8s 上的 go-svc 由 `Add-GoSvcCommandTopicEnv` 从 base_deploy_config
  自动注入,本机由 `start_game.ps1` 注入,都不用单独改)、`deploy/docker-compose.yml` 的
  `KAFKA_INIT_COMMAND_*`、以及本 Job(值由 `Apply-KafkaTopicInitJob` 从 base_deploy_config 读)。
- **消费端有门禁**:gate/scene 启动时向 broker 核对该 topic 的分区数,不等于契约就**起不来**
  (`KafkaConsumer::init` 的 `partition contract mismatch`)。所以 topic 被生产者抢先 auto-create 成
  1 分区时不会静默降级,而是节点起不来 + Job 失败,两处都能看见。
- **迁移窗口**:`Kafka.DisableLegacyPerNodeTopic` 默认 `false`,gate/scene 在消费新 topic 的同时
  继续用老的 group 订阅方式消费 `gate-{id}` / `scene-{id}`。全部生产者(含 Go login)切到新 topic 之后
  才置 `true`;在那之前老路径上的 P1 仍然存在。**本机**(`bin/etc/base_deploy_config.yaml`)至今仍是 `false`,由运维确认后显式翻。
- **K8s 已关老路径(2026-10-15,P0-B 批 2,`docs/handoff/global-singletons-p0-findings-20261014.md`)**:
  `New-NodeConfigMapYaml` 生成的 `node-config` 与 `battle-node-config` 写 `DisableLegacyPerNodeTopic: true`(模板常数,刻意不从
  bin/etc 镜像,否则一次改动会同时翻本机与所有 K8s 环境);同批把 `Kafka.GroupID` 从写死的 `game-consumer-group` 改为镜像
  bin/etc(本机是空串)。契约测试 `node ConfigMap 的 Kafka.GroupID 从 bin/etc 镜像、不再写死共享组名…` 钉住这两件事。
  - **核实依据**(2026-10-15 全仓静态 grep,无运行证据):已没有任何生产者写 `<类型>-<node_id>`。
    C++ 的 battle 发 GateCommand / SceneCommand 走 `ResolveCommandRoute`(`battle_room_manager.cpp`),scene 的生产点只写
    `db_task` / 审计 / 告警 topic,`LegacyPerNodeTopic()` 只剩单测引用;Go 的 login(`gate_command.go`,2026-09-08 起)、
    scene_manager、player_locator、match、friend、guild 全部经 `shared/kafkacmd` / `shared/kafkautil` 写 `<类型>-cmd_g<N>`,
    chat / trade / team / client_rpc_router 不连 Kafka;Java gateway 不连 Kafka。`go/login/internal/svc/gate_command_test.go`
    机械守住 login 不再写 `gate-<id>`。
  - **为什么必须同批**:老路径的组名取 `Kafka.GroupID`(`node_kafka_command_handler.h` 的 `RegisterKafkaCommandHandler`,空串时
    回落 `<类型>-group-<node_id>`)。只把 GroupID 改空、不关老路径,gate/scene 的老路径就换到没有已提交位点的新组,从 earliest
    重放老 topic 里保留期内的消息(非空且不匹配的 `target_instance_id` 会被过滤,空实例号的不会)。关掉老路径后 GroupID 在
    K8s 上已无 C++ 读者;battle 只生产,它在旧二进制上拿不到 broker 的缺陷随 GroupID 改空一并消除(新二进制已在批 1 修掉)。
  - **上线影响**:
    1. **改 ConfigMap 不会自动滚 Pod**:C++ 节点的 Pod 模板只带 `mmorpg.io/cpp-log-sidecar-config-hash`(只哈希 sidecar 配置)
       与有口令时的 `mmorpg.io/zone-redis-auth-hash`(只哈希 zone Redis 口令,见「C++ 节点的 zone Redis 口令」),
       不带整份 node-config 的哈希,`kubectl apply` 之后跑着的 gate / scene / battle 仍是旧配置,要手动 `kubectl rollout restart`
       (Agones 编排的 scene / battle 不吃 rollout restart,要让 Fleet 换一批 GameServer)。
    2. **重启 Deployment 形态的 battle 会作废正在打的局**;挑没有对局的窗口。battle 是 infra namespace 里的全局池,
       `battle-node-config` 只由 `infra-up` / `all-up` 渲染,`zone-up` 不碰它。
    3. **关老路径之前**确认两件事,任一条不满足就先把模板那一行改回 `false` 再部署(否则老版本生产者发的命令没人消费):
       线上 login 及其它 Go 生产者的镜像都已是写新 topic 的版本(login 在 2026-09-08 的 `11ba15a0f` 切换);老 topic `gate-<id>` / `scene-<id>` 的写入速率与积压都为 0
       (`kafka-consumer-groups.sh --describe --group game-consumer-group` 的 LAG 全 0,或老 topic 根本不存在)。
    4. **回退**:把 `New-NodeConfigMapYaml` 里那一行改回 `false`,重新渲染并重启节点。注意回退时 GroupID 已是空串,老路径会用
       `<类型>-group-<node_id>` 新组从 earliest 读老 topic,回退前先确认老 topic 里没有已被处理过的残留。
    5. 取值待实测 / 压测复核:K8s 上的战斗链路仍是事后补验项(`deploy/k8s/AGENTS.md`),在那之前不要写「K8s 上 battle 已验证可用」。

## Local Trial on kind (2026-09-03 实跑记录)

`infra-up` 在本机 kind(v0.33.0 / node v1.37.0,Docker Desktop 29 引擎)上跑通 A 档
(etcd / redis / redis-match-cluster / kafka / mysql + 全局 match)时踩到的坑,都是
kind 本地环境特有,不影响真集群:

> **本节混着两个日期的东西,别当成同一次实跑看。** 上面那份 A 档清单是 2026-09-03 实跑时的形态,**里面没有 loki**;
> `loki` 与 C++ 日志 sidecar 是 **2026-09-19 这一轮**才加进来的(`loki` 还是条件部署:sidecar 开着且没给 `-LokiPushUrl` 时才随 `infra-up` apply)。
> 下面凡是标了"2026-09-19 新增"的命令与条目,都不是 2026-09-03 那次观察到的,而是按本轮代码补的;
> 日志 sidecar 的端到端形态与实测结论见 `docs/ops/grafana-loki-local-logs.md` §7。

> **(2026-09-29 更正:建集群的方式已被取代,下面代码块里的 `kind@latest` 与 `kind create cluster --name mmorpg` 两行保留作历史,不要再照抄。)**
> 旧写法没带 `--config`,拿不到集群外入口验证要的端口映射;也没带独立 kubeconfig,会把**默认** kubeconfig 的 current-context 切到 kind-mmorpg
> —— 本机默认 context 是别的项目的集群,集群外入口 D93 明令不碰。现在一律这样建(工作目录 = 仓库根;先跑 `deploy/k8s/kind-config.yaml` 文件头的「宿主机端口预检」):
>
> ```powershell
> go install sigs.k8s.io/kind@v0.33.0                      # 钉版本:kind-config.yaml 钉了 kindest/node:v1.35.8@sha256,只保证与 kind v0.33.0 配套
> $env:KUBECONFIG = "$env:TEMP\kind-mmorpg.kubeconfig"     # 独立 kubeconfig,不改默认文件
> kind create cluster --name mmorpg --config deploy/k8s/kind-config.yaml --kubeconfig $env:KUBECONFIG
> kubectl --context kind-mmorpg get node                   # 通了再动手
> # k8s_deploy.ps1 一律带 -KubeContext kind-mmorpg -KubeConfig "$env:TEMP\kind-mmorpg.kubeconfig"
> # helm 一律带 --kube-context kind-mmorpg --kubeconfig "$env:TEMP\kind-mmorpg.kubeconfig";手工 kubectl 一律带 --context kind-mmorpg
> kind delete cluster --name mmorpg --kubeconfig $env:KUBECONFIG
> ```
>
> - 风险口径:真正的风险是「没设 `$env:KUBECONFIG` 又没带 `--kubeconfig`」(新开窗口、换终端、子进程丢环境变量)—— 这时 kubectl / helm / `k8s_deploy.ps1`
>   会静默落到默认 current-context。每条命令显式点名 `kind-mmorpg` 是 fail-closed 的兜底:默认 kubeconfig 里没有这个 context,环境变量丢了会直接报 context 不存在,
>   而不是部署到别的集群。独立文件丢了用 `kind export kubeconfig --name mmorpg --kubeconfig <同一路径>` 重新导出。`kind load` 按 `--name mmorpg` 找节点容器,不读 kubeconfig。
> - `extraPortMappings` 只能在建集群时指定:按旧命令建出来的同名集群必须 `kind delete` 后按新配置重建,**会丢全部 PVC 数据**(mysql / etcd / kafka),重建前要执行者确认。
> - 用新配置重建后节点从上面记录的 v1.37.0 变成 v1.35.8(Agones 1.58 与 ingress-nginx v1.15.x 只支持到 1.35,原因见 `kind-config.yaml` 注释);
>   Agones 端口段是 7100–7109(避开 redis-cluster 的 7000–7005),20000 / 30000 / 30001 要求先停宿主机上直跑的 C++ 节点。
> - 集群外入口的端到端验收(阶段 A 不装 Agones、阶段 B 装 Agones)见 `docs/design/k8s-client-entry.md` 的「kind 端到端验证」一节;gate / battle 严禁用 port-forward 验证,
>   HTTP 的 gateway 可以。宿主机上跑 robot 不要用默认的 `robot/etc/robot.yaml`(satoken → 127.0.0.1:18080 会打到 ingress 上拿 404),用 password 认证的专用 yaml,
>   并给集群内 login 开 `-LoginDevPasswordAuth`(仅 dev 档)。
> - 节点 / 宿主重启后 kubelet 对 `kind load` 过的镜像去 Docker Hub 校验、Pod 卡 `ErrImagePull`(backlog P2-03)**仍未处理**,照下文 `kubectl delete pod` 重建;
>   P2-03 落地时在已有的 `kind-config.yaml` 上追加 `kubeadmConfigPatches`,不重写该文件、不动 `extraPortMappings`。
> - 以上命令本会话**未运行**(本机未装 kind),待 Codex / 用户验证。

```powershell
. E:\work\tools\buildenv.ps1                       # GOPROXY / go 工具链
go install sigs.k8s.io/kind@latest                 # 装到 $(go env GOPATH)\bin   ← 2026-09-29 已被取代:改 @v0.33.0,见上方更正
kind create cluster --name mmorpg                  # kubectl context 自动切到 kind-mmorpg   ← 2026-09-29 已被取代:须带 --config 与独立 --kubeconfig
pwsh -File tools/scripts/go_svc_image.ps1 -Command build-all -Registry local -Services match
kind load docker-image local/mmorpg-match:<tag> --name mmorpg
# 2026-09-19 新增(本轮才有,2026-09-03 那次实跑没有这一段):观测栈的两个 Docker Hub 镜像 ——
# infra-up 会条件部署 Loki,battle 池的 C++ Pod 还带一个 Alloy sidecar,两个都要预载进 kind。
# docker save 不会替你去拉:宿主本地没有这个镜像时它直接报 "No such image",所以每条 save 前先 pull。
# Hub 上的多平台镜像必须走 save --platform + image-archive,理由见下面第二条。
docker pull grafana/loki:3.5.8
docker save --platform linux/amd64 -o loki.tar grafana/loki:3.5.8
kind load image-archive loki.tar --name mmorpg
docker pull grafana/alloy:v1.10.0
docker save --platform linux/amd64 -o alloy.tar grafana/alloy:v1.10.0
kind load image-archive alloy.tar --name mmorpg
pwsh -File tools/scripts/k8s_deploy.ps1 -Command infra-up -GoSvcRegistry local -ZoneId 101
```

- **kind 节点不共享宿主 Docker 的镜像缓存/镜像源**,直接从 Docker Hub 拉基础镜像
  (redis / kafka / mysql)极慢,而 kubelet 默认串行拉镜像,一个卡住的拉取会把
  所有 Pod 钉在 `ContainerCreating`。把宿主已有镜像 `kind load` 进去即可解开。
- **Docker 29 的 containerd 镜像存储下 `kind load docker-image` 对 Hub 拉下来的多平台
  镜像会失败**(`ctr: content digest sha256:...: not found`,因为 `docker save` 输出
  的 index 引用了本地没有的其他平台清单)。改用
  `docker save --platform linux/amd64 -o x.tar <image>` + `kind load image-archive x.tar`。
  本地 `docker build` 出来的单平台镜像(Go 服务)不受影响。
- **(2026-09-19 新增)漏预载 `grafana/alloy:v1.10.0` 的症状与"业务坏了"一模一样**:sidecar 是 **k8s 原生 sidecar**
  —— 写在 `initContainers` 里、带 `restartPolicy: Always`,所以**它拉不到镜像时业务容器一次都不会启动**:
  Pod 卡在 `Init:ErrImagePull` / `Init:ImagePullBackOff`,`kubectl describe pod` 的报错在 **Init Containers** 一节
  (不在 Containers 一节,按老习惯只扫 Containers 会看漏);gate Pod 起不来 → `gate-entry` Service 没有后端 →
  **整个 zone 连不进来**,但这**不是业务代码的问题**。只看 `rollout` 超时基本想不到是观测容器拖的。
  临时不想要就加 `-NoCppLogSidecar`,正经做法是按上面那段 pull + save + `kind load` 把镜像预载进 kind。
  (明知这个代价仍选原生 sidecar 的理由见 Optional Flags 的 `-NoCppLogSidecar`:普通容器形态在 Agones 那条路上
  会让日志在 Pod 余生里**静默**断流,而拉不到镜像本来也会让 Pod 不 Ready —— 两种都是故障,原生 sidecar 至少报得直白。)
  漏预载 `grafana/loki:3.5.8` 则是另一种表现:部署一路成功,只是一条 C++ 日志都查不到。
- `kind load` 之后老 Pod 仍卡在原来的拉取请求上时,直接 `kubectl delete pod`
  让控制器重建,新 Pod 走 `IfNotPresent` 立刻起来。
- `mysql-backup-pvc` 是 ReadWriteMany,kind 自带的 local-path 不支持,会一直 Pending;
  只有备份 CronJob 用它,mysql 本体不受影响。
- go/db 的镜像以前在任何环境都构建不了:`go/db/go.mod` 把 proto2mysql replace 到仓库外
  (`../../../proto2mysql`),以 `go/` 为 build context 带不进去。B 档已修:见下一节。

### B 档:zone-up(2026-09-03 实跑记录)

在 A 档 infra 之上把 `yesterday`(zone_id=1)拉起来:Go 五服务 + Java 网关,C++ 节点用占位镜像。

```powershell
. E:\work\tools\buildenv.ps1
# Go 服务镜像(db 需要宿主上检出 E:\work\proto2mysql;所有服务都会打包 generated/tables)
pwsh -File tools/scripts/go_svc_image.ps1 -Command build-all -Registry local -Tag <tag>
# Java 网关镜像(Dockerfile.java-svc 的 build context 是 java/gateway_node)
pwsh -File tools/scripts/java_svc_image.ps1 -Command build -Registry local -Tag <tag>
# C++ 节点本档不构建:用任意小镜像占位,gate/scene Pod 会 CrashLoop/RunContainerError,属预期
docker tag busybox:1.36 local/mmorpg-node:<tag>
# 2026-09-19 新增(本轮才有,2026-09-03 那次实跑的 zone-up 还没有 sidecar;端到端形态见
# docs/ops/grafana-loki-local-logs.md §7):gate/scene Pod 默认带 Alloy sidecar,这个镜像也要预载
#(Loki 在 A 档已载入)。同样先 pull:宿主没有该镜像时 docker save 直接报 "No such image",它不会去拉。
docker pull grafana/alloy:v1.10.0
docker save --platform linux/amd64 -o alloy.tar grafana/alloy:v1.10.0
kind load image-archive alloy.tar --name mmorpg
# 逐个 docker save --platform linux/amd64 + kind load image-archive(见上节)
pwsh -File tools/scripts/k8s_deploy.ps1 -Command zone-up -ZoneName yesterday -ZoneId 1 `
    -GoSvcRegistry local -JavaSvcRegistry local -NodeImage local/mmorpg-node:<tag> `
    -GateReplicas 1 -SceneReplicas 1
```

- `-NodeImage` 显式给出时 go/java tag **跟随它的 tag**,所以占位镜像的 tag 必须与
  Go/Java 镜像一致(或显式传 `-GoSvcTag/-JavaSvcTag`)。
- 不要带 `-WaitReady`:它先等 gate,占位镜像的 gate 永远不 Ready 会直接超时。
- 实跑修掉的四个真 bug(都不是 kind 特有):
  1. **Go 镜像里没有策划表**:login / player-locator / scene-manager 启动 `LoadTables`
     (TableDir 默认 `../../generated/tables`)直接 `log.Fatalf` CrashLoop。
     `Dockerfile.go-svc` 现在以 `--build-context tables=<仓库>/generated/tables` 带入
     并落到 `/generated/tables`(WORKDIR /app 向上归一化正好命中默认值);同时**文件名转小写**
     —— 生成的 loader 读 `actoractioncombatstate.json`,导表产物却是 `ActorActionCombatState.json`,
     Windows 不分大小写所以本地从没暴露,Linux 一定读不到。根因在导表/生成器命名不一致,镜像里只是兜底。
  2. **db 的 gRPC 服务从未 Start**:`go/db/db.go` 自 fc9377336 起只 `MustNewServer` 没 `Start()`,
     进程打印 STARTED 却不监听 6000、不注册 etcd `db.rpc`;compose 没探针一直没暴露,K8s 的
     grpc readiness/liveness 永远超时 → 0/1 + 反复重启。已补 `go s.Start()`。
  3. **dev 档 login 拒启**:生成器 dev 档回落占位密钥,而 login 的 secrets 门禁只在 go-zero
     `Mode=dev/test` 才降级为 WARN,ConfigMap 里没写 Mode(默认 pro)→ panic
     "HMAC 密钥配置不合格"。现在 dev 档 login ConfigMap 写 `Mode: dev`(与 go/login/etc/login.yaml 一致),
     staging/prod 不写,门禁不变。
  4. **`-WaitReady` 会等根本没部署的 auth**:`$JavaSvcCatalogue` 有 auth 条目但
     `manifests/java-svc/auth.yaml` 不存在,Apply 只告警跳过,Wait 却照等 → 必超时。已改为 manifest 不存在则跳过。
- **占位镜像别用节点上 kubelet 已经拉过的镜像 ID 重打 tag**:一开始 `docker tag busybox:1.36 local/mmorpg-node:<tag>`
  再 `kind load`,gate/scene 一直 `ErrImagePull`(kubelet 明明配了 IfNotPresent 还去 Docker Hub 拉
  `docker.io/local/mmorpg-node`)。原因是 k8s 1.33+ 的 `KubeletEnsureSecretPulledImages`:
  `/var/lib/kubelet/image_manager/pulled/` 里有 busybox 那个镜像 ID 的 `ImagePulledRecord`
  (credentialMapping 只有 `busybox`),同一个镜像 ID 换个仓库名引用就必须重新验证拉取权限。
  换一个节点上从未被 kubelet 拉过的镜像(alpine:3.20)做占位就正常了;`crictl inspecti` 能查到不等于 kubelet 会用。
- 镜像构建慢的两个来源:`go mod download` 每个服务重下一遍(Dockerfile 无 cache mount);
  temurin 基础镜像从 Docker Hub 直拉约 100KB/s,`docker.m.daocloud.io` 当日返回 unavailable,
  `docker.1ms.run/library/eclipse-temurin:23-jdk` 可用,拉完 `docker tag` 成官方名即可命中本地缓存。

#### B 档验收清单(2026-09-04 复核通过)

```powershell
# 1. Go 五服务 + Java 网关 Ready;gate/scene(占位镜像)CrashLoop 属预期
kubectl get pods -n mmorpg-zone-yesterday
# 2. login / player-locator / scene-manager 已注册进 etcd(zone/1 前缀)
kubectl exec -n mmorpg-infra etcd-0 -- etcdctl get --prefix --keys-only LoginNodeService.rpc/zone/1
#    → LoginNodeService.rpc/zone/1/node_type/5/node_id/{2,3};另有 db.rpc/ login.rpc/ playerlocator.rpc/
#      dataservice.rpc/ scenemanagerservice.rpc/ 的 go-zero 发现键
# 3. MySQL 初始化 SQL 生效:zone_config 在 mmorpg 库,zone_1_db / zone_2_db / zone_101_db / zone_102_db 已建;
#    mmorpg_global(data-service 全局库,02_k8s_global_db.sql)已建且 appuser 有权(2026-09-08 起,存量 PVC 需手工补,见上节)
#    mmorpg_trade(聚宝斋 trade 独占库,00_init_zone_dbs.sql 原样带入)已建且 appuser 有权(2026-09-14 起,存量 PVC 需手工补,见「聚宝斋 trade」)
#    mmorpg_friend(好友 friend 独占库,同一个 00_init_zone_dbs.sql)已建且 appuser 有权(2026-09-18 起,存量 PVC 需手工补,见「好友 friend」)
kubectl exec -n mmorpg-infra deploy/mysql -- sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -e "SELECT * FROM mmorpg.zone_config"'
# 4. Java 网关 zone 目录
kubectl port-forward -n mmorpg-zone-yesterday svc/gateway 18081:8081
curl http://127.0.0.1:18081/api/server-list
#    → {"zones":[{"zone_id":1,"name":"zone-1","status":"MAINTENANCE",...}]}
#      status 是 MAINTENANCE 而不是 OPEN:zone_config.manual_status=0 且没有任何 gate 注册
#      (本档 gate 是占位镜像),C 档 C++ 镜像上来后才会变 OPEN,不是网关的 bug。
```

- **login 起得比 player-locator 早会 fatal 重启几次**:`NewServiceContext` 用 `zrpc.MustNewClient` 同步拨
  `playerlocator.rpc`,etcd 里还没有就 `logx.Must` 直接退出。两副本各重启 3~13 次后随 player-locator Ready
  自愈,不是 bug;要消除只能把客户端改成 lazy dial 或给 login 加 initContainer 等依赖。
- **宿主重启后老 Pod 可能卡 `ErrImagePull`**:kind 节点重启时 kubelet 对已 `kind load` 的镜像仍会去 Docker Hub
  校验(`lookup registry-1.docker.io ... server misbehaving` / `EOF`),gateway 两副本卡了 40 分钟;
  `kubectl delete pod -l app=gateway` 让控制器重建就走 IfNotPresent 立刻起来(与 A 档一节同一个坑)。
- 网关日志里 `login.rpc channel up (zone=1): 127.0.0.1:53000` 三行是 `application.yaml` 里给本地 compose 的静态兜底,
  K8s 上真正生效的是随后 `login.rpc discovery routing changed: {1=2}` 那条 etcd 动态发现,可以忽略。

## data_service 按 C++ 约定注册 + IdSegments 号段配置(2026-09-08,node-id-overhaul-plan §6 / §7.5)

- **为什么**:scene 的 item / txlog / snapshot guid 改由 `DataService.AllocateIdSegment` 发号,scene 经 etcd 前缀
  `DataServiceNodeService.rpc` 发现 data_service;以前 data_service 只写 go-zero 自己的 `dataservice.rpc` 键,C++ 看不见,
  scene 永远过不了 DependencyGate("item segment ready")。
- **data_service 现在多写两把 key**(同一把租约、`LeaseTTL` 秒,keepalive 续租;租约丢失走 CAS 重夺 / 重新分配 node_id,
  `go/data_service/internal/noderegistry` 是 scene_manager 同名包的副本):
  - `DataServiceNodeService.rpc/zone/<ZoneId>/node_type/26/node_id/<N>` → protojson `NodeInfo`(Endpoint / GrpcEndpoint = `POD_IP:9000`)
  - `DataServiceNodeService.rpc/allocated/node_type/26/node_id/<N>` → node uuid(node_id 跨 zone 唯一占位)
  注册在 gRPC 端口真正 accept **之后**才发生(旁路拨端口);失败即 panic(CrashLoopBackOff 可见),关停时删 key + Revoke。
- **新配置键**(`go/data_service/etc/data_service.yaml` ↔ `go-svc-data-service-config`):`ZoneId`(写部署 zone,`${CurrentZoneId}`;
  DataServiceNodeService **不是** zone-scoped 类型,C++ 发现侧不按 zone 过滤,任何 zone 的 scene 都能发现任何 zone 的 data_service)、
  `LeaseTTL`(从服务 yaml 取,默认 60)。`manifests/go-svc/data-service.yaml` 加了 Downward API `POD_IP`(与 login.yaml 同)。
- **C++ 节点 ConfigMap**(`base_deploy_config.yaml`)新增:`service_discovery_prefixes` 里的 `DataServiceNodeService.rpc`,
  以及**顶层 `IdSegments:` 列表**。旧的 `ItemIdSegment:` 段不再生成。
  - **键名 / 结构以 C++ 读法为准,不能自己发明**:`cpp/libs/engine/config/config.cpp:126-140`(`readBaseDeployConfig`)
    只认顶层 `IdSegments` 列表,逐项显式读 `Kind` / `Enabled` / `InitialStep` / `MinStep` / `MaxStep` 五个键,
    填进 `IdSegmentConfig{repeated IdSegmentKindConfig kinds}`(`proto/common/base/config.proto:107`)。
    **写错形状不会报错**:缺键 = proto 默认值 0、整块名字不对 = `kinds` 为空,scene 的 Enable 校验拒绝 →
    DependencyGate "id segments ready" 永不放行,玩家进不来;而这份 ConfigMap 是 readOnly 整目录挂到 `/app/bin/etc`、
    **完全遮蔽**镜像里那份的,所以这里错就是线上错。2026-09-08 修正前生成器写的是一个 C++ 根本不读的
    `GuidSegment` 顶层映射(`Enabled` + `Kinds` 列表,键名 `Tag` / `Step`),没有任何一个键被读到。
  - **生成器不再抄常数**:`k8s_deploy.ps1::Get-AuthoritativeYamlBlock` 把 `bin/etc/base_deploy_config.yaml` 里整个
    `IdSegments:` 块(含每个 Kind 的取值说明注释)逐行原样搬运进 ConfigMap。单一真相在那份文件 ——
    改 step / 加种类(pet / guild …)只改那里,ConfigMap 不可能漂移;文件里没有 `IdSegments:` 则生成期直接 throw(fail-closed)。
  - 当前值(来自那份文件):`item` InitialStep 20000 / [1000, 1000000];`txlog` 50000 / [2000, 2000000];`snapshot` 2000 / [200, 100000]。
    `Kind` 名要与 data-service 的 `IdSegment.BootstrapTags` 及 `id_segment` 表的种子行一一对应(生产缺行 = `ErrCodeIdSegmentUnknownTag`,不自动补种)。
- **审计 topic 世代号 `_g<N>` 已改成配置驱动,ConfigMap 现在生成 `AuditTopicGeneration:`(2026-09-09)**:
  C++ 侧留在代码里的只有**基名**——`cpp/libs/modules/transaction_log/transaction_log_system.h` 的
  `kTransactionLogTopicBase = "transaction_log_topic"`、`cpp/libs/modules/snapshot/snapshot_system.h` 的
  `kPlayerSnapshotTopicBase = "player_snapshot_topic"`;后缀由 `cpp/libs/modules/audit/audit_topic.h::AuditTopicName`
  在发送时按 `BaseDeployConfig.audit_topic_generation`(`proto/common/base/config.proto` 字段 20)拼出来,
  `config.cpp::readBaseDeployConfig` 逐键读 `AuditTopicGeneration`,缺键 / 0 = 第一代 `_g1`
  (与 Go `topicForGeneration` 的 `generation == 0 → 1` 逐字同规则)。
  改这个值不再需要改 C++ 代码,也不需要重出 node 镜像 —— 改配置 + 重启节点即可。
  - **ConfigMap 的值从消费者那份 yaml 取**:`New-NodeConfigMapYaml` 用 `Get-YamlScalar` 读
    `go/data_service/etc/data_service.yaml` 的 `Kafka.TopicGeneration`(缺席回落 1,与 C++ / Go 的 0→1 规则一致),
    而不是从 `bin/etc/base_deploy_config.yaml` 再抄一份。同一个键还喂着 data-service 自己的 ConfigMap
    (`$dsKafkaTopicGeneration`)和 `kafka-topic-init` 预建的 topic 名 —— 生产者、消费者、预建 Job 三边同源,不会各说各话。
  - **漂移风险已消除,但两侧仍须同批次动**:K8s 上改 `Kafka.TopicGeneration` 后必须重跑 `zone-up`(重新生成 node ConfigMap)
    + 重跑 `infra-kafka-topics` 预建新一代 topic,并滚更 gate/scene 让它们重新读配置;
    本地 / 裸机跑的节点用的是 `bin/etc/base_deploy_config.yaml` 的 `AuditTopicGeneration`,那一份要与
    `go/data_service/etc/data_service.yaml` 的 `Kafka.TopicGeneration` 手工保持相等(两份 yaml 各自是各自部署形态的真源)。
    忘了任何一边的症状仍然是静默的:流水 / 快照进了一个没人消费的 topic,30 天后被保留期吃掉。
    `tools/scripts/tests/k8s_deploy_contract.tests.ps1` 里有一条契约用例钉住"node ConfigMap 的 `AuditTopicGeneration`
    == data-service 的 `Kafka.TopicGeneration`"。
- 验收:

```powershell
kubectl exec -n mmorpg-infra etcd-0 -- etcdctl get --prefix --keys-only DataServiceNodeService.rpc
#  → DataServiceNodeService.rpc/zone/<Z>/node_type/26/node_id/1 与 DataServiceNodeService.rpc/allocated/node_type/26/node_id/1
kubectl logs -n mmorpg-zone-<name> deploy/data-service | grep NodeRegistry
#  → [NodeRegistry] DataService registered: ... endpoint=<POD_IP>:9000 lease_ttl=60s
```

## C++ gRPC 客户端 deadline:GrpcClient 块搬运 + 预算门禁(2026-09-28,grpc-client-deadline-failure-callback.md §4.4)

C++ 节点(gate / scene / battle)发出的每次 unary gRPC 调用现在都带 deadline,按**目标**节点类型配在
`bin/etc/base_deploy_config.yaml` 的顶层 `GrpcClient.CallDeadlineMs`(键 = `ENodeType` 枚举名,单位毫秒)。
没列出的类型、`0`、不认识的键都落到 C++ 内置默认 10000(`grpc_call_tag.h` 的 `kDefaultGrpcCallDeadlineMs`,后两者另记 ERROR)。

- **node ConfigMap 整块搬运**:`node-config` 与 `battle-node-config` 以 readOnly 整目录挂到 `/app/bin/etc`,**完全遮蔽**镜像里那份。
  `New-NodeConfigMapYaml` 用 `Get-AuthoritativeYamlBlock -Key 'GrpcClient'` 把整块(含行尾注释)逐行原样搬进去,与 `IdSegments` 同法;
  文件里没有 `GrpcClient:` 则生成期直接 throw。漏搬的症状是静默的:K8s 上所有目标落回 10000,而号段 `fetchTimeoutSec`
  (= DataService deadline + 1s)、换图在途 TTL(= SceneManager deadline + 1000)都从 deadline 派生,会跟着偏离仓库口径。
- **预算门禁(上游比下游宽)**:`zone-up` / `infra-up` / `all-up` 在任何集群写操作之前调用 `Assert-GrpcClientDeadlineBudget`,
  核对「C++ deadline ≥ 目标 Go 服务 zrpc `Timeout` + 2000」,不满足就拒绝部署并逐项点名;`*-down` / `*-status` 不经过它。

  | 目标(`CallDeadlineMs` 键) | 服务端超时的真源 | 当前 deadline / 服务端 Timeout |
  |---|---|---|
  | `SceneManagerNodeService` | `go/scene_manager/etc/scene_manager_service.yaml` | 10000 / 8000 |
  | `DataServiceNodeService` | `go/data_service/etc/data_service.yaml`(不写 = go-zero 默认 2000) | 4000 / 2000 |
  | `ClientRpcRouterNodeService` | `go/client_rpc_router/etc/client_rpc_router.yaml` | 8000 / 6000 |
  | `MatchNodeService` | `go/match/etc/match_service.yaml`(gate 直连模式) | 7000 / 5000 |
  | `LoginNodeService` | `go/login/etc/login.yaml`(gate 直连模式) | 12000 / 10000 |
  | `PlayerInboxNodeService` | `go/player_inbox/etc/player_inbox.yaml`(scene 调 FetchItems / AckItems;2026-10-15 起,见「玩家收件箱 player-inbox」) | 6000 / ≤ 4000(服务 yaml 显式写) |

  判定口径:这六项必须在 `CallDeadlineMs` 里显式写成正整数(缺席 / 0 = 落到隐式默认,不算数);服务 yaml 不写 `Timeout` 按
  go-zero `RpcServerConf.Timeout` 的 `default=2000` 算(go.mod 钉的 v1.9.2 / v1.10.0 相同),写了就必须是正整数(0 = go-zero 不装超时拦截器,
  服务端没有上界);服务 yaml 的 `MethodTimeouts` 逐条核对:每一条都要满足「deadline ≥ 该方法超时 + 2000」,除非该方法登记在 `k8s_deploy.ps1` 的
  `Get-GrpcClientMethodsNotCalledFromCpp`(C++ 节点从不调用;今天是 data_service 回档的三个 RPC,通过时那一行会列出来)。
  这张表只认「顶格不带引号的键 + 缩进的块式列表」,超时只认带单位的写法(`300s`),读不懂一律拒(2026-10-09 起,
  此前是出现即拒;`docs/design/grpc-client-deadline-failure-callback.md` §4.4)。`Battle` / `Etcd` 没有 Go 服务端超时,不在表里;
  chat / friend / guild / team / trade 不在 C++ 节点白名单里,只经路由服。
- **改服务端超时 = 两处同拍**:调大上表任一服务的 `Timeout` 必须同时调 `CallDeadlineMs` 对应项,否则下一次 `*-up` 被门禁拒绝。
  改完重跑 `zone-up`(以及 `infra-up`,battle 用的是 `battle-node-config`)重新生成 ConfigMap,并滚更 C++ 节点让它们重新读配置。
- **DataService 2500 → 4000(2026-09-28)**:第一批落码时写的是 2500,与该块自己注释的「+2000 余量」不符(data_service 服务端默认 2000);
  改为 4000 后号段 `fetchTimeoutSec` 从 3.5s 变成 5s。login 的 `Timeout` 原为 100000(行尾注释写 10s),
  2026-09-29 确认是笔误,改为 10000,`LoginNodeService` 同步由 102000 改为 12000(`docs/design/grpc-client-deadline-failure-callback.md` §4.3)。
- **login ConfigMap 的 `Timeout` 改为镜像 `go/login/etc/login.yaml`(2026-09-29)**:以前 `New-GoSvcConfigMapYaml` 在模板里写死 `100000`,
  而预算门禁核对的是 login.yaml,两边一分家,门禁放行的就不是集群里生效的值。现在与 scene-manager / match / 路由服同法镜像,
  读不到或不是正整数时生成期直接 throw。镜像落地时两边都是 100000,行为不变;随后值已改正为 10000(见上一条),ConfigMap 自动跟随。
  以后再改只需动 login.yaml 与 `LoginNodeService` 两处,生成器不用动。
- 契约测试:`tools/scripts/tests/k8s_deploy_contract.tests.ps1` 的「C++ gRPC 客户端 deadline 预算」一节 —— 两份 node ConfigMap 的块与权威文件逐项相等、
  门禁先于任何 `kubectl` 写操作、K8s 生成物(login 镜像 login.yaml 的 Timeout、data-service 不写 Timeout)同样满足不等式、判定口径自检(含 `MethodTimeouts` 逐条核对、解析器与时长换算)、
  「C++ 不调用」登记表的守护(C++ 不直接调用、gate 不按消息号转发、没有过期项),
  以及把 DataService 改回 2500 的配置副本必须被拒并只点名这一项;login ConfigMap 的 `Timeout` == login.yaml 在「login ConfigMap 关键值」用例的键对表里逐键钉住。

## C++ 节点的 zone Redis 口令(2026-10-15,P0-A 第二批,docs/handoff/global-singletons-p0-findings-20261014.md)

C++ 节点(gate / scene / battle)连共享 Redis(`redis.<InfraNamespace>:6379`)的口令,K8s 上取自 `MMORPG_REDIS_PASSWORD`,
与 Go 服务、gateway、服务端 Secret `redis-auth` 是同一个变量、同一个值。

- **口令怎么到达 C++(方案 A)**:`New-NodeConfigMapYaml` 把它明文写进 `node-config`(zone namespace)与 `battle-node-config`(infra namespace)
  的 `game_config.yaml` → `zoneredis.password`。与各 Go 服务 / gateway 的 ConfigMap 同一做法:改走 Secret 不缩小暴露面(同 namespace 能读这份
  ConfigMap 的人也能读那几份)。`-DryRun` 会把它连同其它 ConfigMap 一起打出来(既有行为)。dev 档没设该变量时写空串,C++ 不发 AUTH,与改动前相同。
  C++ 进程另认环境变量 `ZONE_REDIS_PASSWORD`(覆盖 yaml,本机 / 验收用),K8s 模板不注入它。
- **C++ 的行为(第一批)**:口令非空时每条新连接(含断线重连)先 `AUTH`,回 `+OK` 才对业务可见;被拒、发不出去或超时一律 fail-closed ——
  zone Redis 对业务保持不可用,每个重连周期重试一次并打带 `metric=zone_redis_auth_failed` 的 ERROR,进程不退出、Pod 仍 Ready
  (「zone Redis 已认证」的就绪条件另起一项)。启动日志只打 `Zone Redis auth: on|off`,不打口令。
- **生成期保护**:非 dev 档口令为空,或口令含 `"`、`\`、换行与其它控制字符(它要原样写进多份 YAML 双引号标量),在任何 `kubectl` 写操作之前拒绝,
  报错只点变量名、不回显值。
- **指纹注解**:挂 `/app/bin/etc` 的 C++ 工作负载在口令非空时,Pod 模板带 `mmorpg.io/zone-redis-auth-hash`(只哈希口令,不哈希整份 node-config ——
  否则 P0-B 这类无关改动也会滚动 gate 与 battle)。C++ 只在启动时读配置,只改 ConfigMap 不会让进程重读;指纹把「换口令」变成 Pod 模板变化:
  - gate / scene / battle 的 Deployment:apply 即滚动 —— gate 断开该 Pod 上的玩家,scene 迁走玩家,battle 作废在打的局。这是期望行为;
  - Agones Fleet(scene / battle):新建 GameServerSet,已 Allocated 的实例打完才退 —— 它们一直用启动时读到的旧口令(首次开口令时是空口令),
    Redis 带新口令重启后会一直认证失败或 NOAUTH,直到实例退出,**不会自行恢复**;高密度 scene 只要还有一个房间就一直是 Allocated。
    处置见下面上线步骤的第 3 步前置;
  - `-ClientEntryMode external` 的 gate StatefulSet 是 `OnDelete`:模板换了不自动重建,按 `tools/scripts/k8s_gate_drain.ps1` 逐个排空删 Pod。
- **上线步骤**(给已在运行、Redis 还没开口令的环境开口令,维护窗口内做):
  1. 先发带 AUTH 的 C++ 镜像(第一批)。旧二进制忽略 `zoneredis.password`,Redis 一开口令就全线 NOAUTH,所以镜像必须先行;
     新镜像配空口令时行为与今天相同,可以提前滚。
  2. 维护窗口内带 `MMORPG_REDIS_PASSWORD` 重跑 `infra-up`(建 Secret `redis-auth`、写 `battle-node-config`)与各 zone 的 `zone-up`(写 `node-config`)。
     staging / prod 必须设该变量(≥12 位);dev 档设了同样生效。
  3. **前置(只针对 `-SceneOrchestrator agones` / `-BattleOrchestrator agones`)**:重启 Redis 之前,确认每个 Fleet 都只剩当前这一代
     GameServerSet(旧的已删除,或 DESIRED 与 CURRENT 都是 0),而且旧一代里没有 Allocated 实例。**先等 Fleet 滚动结束再查**:
     SceneManager 分配时只按 zone / role / build 标签选、不分代,第 2 步只改配置时 build 标签不变,滚动中旧一代的 Ready 实例在你查完之后、
     Redis 重启之前仍可能被分配出去。Fleet 名:scene(拆分池时是 scene-world 与 scene-instance)在各 zone namespace,battle 在 InfraNamespace:
     ```bash
     kubectl -n <ns> get gameserverset -L agones.dev/fleet                               # 每个 Fleet 只应剩一行(或旧的 DESIRED/CURRENT 为 0)
     kubectl -n <ns> get gs -L agones.dev/fleet,agones.dev/gameserverset                 # 不按 Fleet 名过滤,免得填错名字查出空结果
     ```
     旧一代里的 Allocated 实例启动时读的是旧 `node-config`(旧口令、空口令,或第 1 步之前的旧二进制 —— 它们根本不认口令),Redis 带新口令起来后
     会一直 fail-closed 或 NOAUTH 到实例退出;battle 在空口令时还会把 NOAUTH 当成「已销账」删掉结算外发箱条目(`BattleRoomManager::ProbeAndRetryOne`,
     交接 handoff-global-singletons-p0-20261015.md §3 第 7 条那张卡片)。高密度 scene 只要还有房间就一直 Allocated,等它自己退出可能永远等不到,
     所以维护窗口里要**主动删除**这些旧实例(`kubectl -n <ns> delete gs <名字>`;代价:scene 迁走 / 断开实例上的玩家,battle 作废在打的局),
     删干净再往下走。Deployment 形态没有这个问题。
     然后手动重启 Redis(首次开口令必须做):`kubectl -n <InfraNamespace> rollout restart statefulset/redis`。`--requirepass` 只在容器启动时从
     Secret 读,脚本只建 / 更新 Secret,不重启 Redis。重启 = 一次 Redis 停机(在线会话与锁清空)。
  4. C++ Pod 因指纹注解变化自动滚动 —— 只对 Deployment 和 Fleet 里**未分配**的实例成立(Allocated 的旧实例按第 3 步前置处理,external gate
     按上一条手动排空)。用新模板起来、但早于 Redis 带口令重启的 Pod,发出的 AUTH 会被还没设口令的 Redis 拒绝,按重连间隔重试,
     Redis 带口令起来后自行恢复(这一段未实测)。
  - **通过标准**:每个 C++ 节点日志都出现 `Zone Redis auth: on` 与 `Redis AUTH ok to <host:port>`,之后不再有 `metric=zone_redis_auth_failed`;
    Redis 的 `INFO errorstats` 里 NOAUTH 计数不再增长。
  - 换口令同样走第 2–4 步;新旧口令并存期间总有一侧被拒,所以同样要维护窗口。首次开服是冷启动(Secret 先于 `redis.yaml` 落地),不存在滚动问题。
  - Service `redis` 不得重建:C++ 只在启动时解析一次地址。
- **取值待实测 / 压测复核**:方案 A、指纹注解与上面的步骤都没有在集群上跑过;C++ 与部署两批都未经 Codex 编译 / 测试。
- 契约测试:`tools/scripts/tests/k8s_deploy_contract.tests.ps1` 的 3a 节(`zone Redis 口令…`)与 battle 段(`battle:带口令的 infra-up…`),
  `tools/scripts/tests/k8s_client_entry_contract.tests.ps1` 的「库:-PodAnnotations 与 sidecar 配置指纹…」。

## Kafka:StatefulSet + PVC(2026-09-08,routing-identity-audit-20260908.md R06)

`manifests/infra/kafka.yaml` 现在是:ClusterIP Service `kafka`(仍是 `kafka.<InfraNamespace>:9092`,所有 ConfigMap /
`base_deploy_config.yaml` 都不用改)+ headless Service `kafka-headless` + `PodDisruptionBudget minAvailable: 1`
+ StatefulSet `kafka`(**1 个或 ≥3 个 broker**,由 `-KafkaBrokers` 决定,见下面「Kafka:多 broker」)、每个 broker 一块 PVC
(集群默认 StorageClass;大小 2026-10-16 起按盘预算算、线上已有时沿用线上值,见下面「Kafka 磁盘预算与 db_task retention.bytes」,以前写死 `20Gi`),
数据目录由 `KAFKA_LOG_DIRS=/var/lib/kafka/data` 显式指定。本节其余内容以单 broker(dev 档)为例。

- **为什么**:旧写法是单副本 Deployment + `emptyDir`,broker 一重启(漂移、OOM、升级)**全部 topic、分区元数据与已提交 offset
  一起消失**。消费者随后静默把 topic 自动重建成 1 分区(broker 开着 `auto.create.topics.enable` + `num.partitions=1`),
  停机窗口里产出的每一条控制面命令在 librdkafka 的 5 分钟消息超时后直接丢,`kafka-topic-init` 还得手工重跑。
- **顺带修掉的一个真 bug**:旧 manifest 把 `emptyDir` 挂在 `/tmp/kafka-logs`,而 apache/kafka 镜像 combined 模式的默认
  `log.dirs` 是 `/tmp/kraft-combined-logs` —— 那块卷根本不是数据目录,日志一直写在容器可写层上。现在 PVC 的 `mountPath`
  与 `KAFKA_LOG_DIRS` 是同一个路径,离线断言里有一条专门盯这个。
- **⚠️ 第一次切换会丢掉现有的全部 topic 数据**。PVC 是空的,broker 会重新 format 存储目录。`infra-up` 会先
  `kubectl delete deployment kafka --ignore-not-found` 再 apply StatefulSet(两者 kind 不同、名字相同,`apply` 不会回收旧
  Deployment;而 Service `kafka` 的 selector `app=kafka` 会同时命中新旧 Pod,客户端和 controller quorum 会在
  "旧 emptyDir broker" 与 "新 PVC broker" 之间随机落点 —— 两边各有一套 topic 和 offset,等于脑裂)。
  **切完必须重跑 `-Command infra-kafka-topics -WaitReady`**(同一次 `infra-up` 里已经跑了一遍;单独止血时用这条),
  否则审计 topic 与 `gate-cmd_g1` / `scene-cmd_g1` 会被抢先的生产者 auto-create 成 1 分区,gate/scene 的分区契约门禁随即让它们起不来。
  切换期间 Kafka 短暂不可用,这段时间产出的控制面命令会丢(与任何一次 broker 重启同性质),建议在没有玩家的窗口做。
- **dev 档是单 broker,staging / prod 档默认 3 个**(2026-10-01 起,见下一节)。**不要手工把 `replicas` 改大**:
  多 broker 要同时成立四件事(每个 Pod 广播自己的地址 / 选举组成员表 / 全部 topic 的副本数 / `min.insync.replicas`),
  它们由 `k8s_deploy.ps1 -KafkaBrokers` 一起渲染;只改一个数字 = 没有任何冗余,还会路由错乱。
- **PDB 是 `minAvailable: 1`,也就是不允许自愿驱逐**。整个控制面、`db_task` 落库、审计流水都只有这一个 broker,
  自动化的节点排空 / 集群升级顺手赶走它,停机窗口里的命令就没了。代价是 **`kubectl drain <node>` 会挂在这个 Pod 上**;
  真要腾空节点时用 `kubectl delete pod kafka-0 -n mmorpg-infra`(自愿删除不过 PDB,Pod 随即重建)或
  `kubectl drain --disable-eviction`。PVC 是 `ReadWriteOnce`:用 local-path 之类的本地盘时 Pod 只能回到原节点。
- **探针**:`startupProbe` / `readinessProbe` 是 API 级的 `kafka-broker-api-versions.sh`,`livenessProbe` 才是 TCP。
  原因是 KRaft 启动时 SocketServer 先 bind 9092,要到最后一步才 `enableRequestProcessing` —— **日志恢复期间 TCP 连得上、
  请求不受理**,用 TCP 做 readiness 会让 Service 把还没恢复完的 broker 放进 endpoint。
  两个 exec 探针都在命令里把 `KAFKA_HEAP_OPTS` 覆盖成 `-Xms16m -Xmx64m`:容器 env 里的 `-Xms512m -Xmx1g` 会被
  `/opt/kafka/bin/*.sh` 全部继承,和 broker 自己的堆叠在同一个 memory limit 里必 OOMKilled。**这是这个文件最容易踩的雷**。
  `startupProbe` 给到 5 分钟(30 × 10s),非正常关闭后的日志恢复不会被 liveness 误杀。
- **文件描述符**:K8s 没有声明 ulimit 的字段,容器运行时的 nofile 软限因运行时/版本而异(Docker Desktop 给 1048576,
  不少 containerd 配置只给 1024)。光控制面命令 topic 就有 2×256 个分区,每分区至少 3 个打开文件,再加审计 / `db_task` /
  迁移窗口里残留的 per-node topic,以及每个客户端连接一个 fd —— 1024 是必然 `Too many open files`,而症状是分区离线、
  客户端连不上,不是一条清楚的报错。所以容器的 `command`/`args` 是一层薄壳:先 `ulimit -n <硬限>`(软限升到硬限不需要特权),
  再 `exec /__cacert_entrypoint.sh /etc/kafka/docker/run`。**换镜像版本时要核一遍入口路径**(换版本时把引用换成新的钉版引用,见下面「镜像钉版本」):
  `docker image inspect apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837 --format '{{json .Config.Entrypoint}} {{json .Config.Cmd}}'`;路径不对会立刻
  CrashLoop 并打出 `not found`,不会静默降级。
- **`CLUSTER_ID` 现在显式钉住**。镜像的 launch 脚本读的是 `CLUSTER_ID`(不是 manifest 里一直写着的 `KAFKA_CLUSTER_ID`),
  它自带的默认值恰好同值,所以以前"碰巧"是对的。换 PVC 之后这件事变要命:`meta.properties` 里记着 format 时用的 cluster id,
  与启动时的值不一致 broker 会拒绝启动。
- **资源**:`requests` 300m / 1Gi,`limits` 2 CPU / **2Gi**(原 1536Mi)。换 PVC 之后 page cache 才真正起作用,
  加上探针 JVM 会短暂再占几十 MB,1536Mi 配 `-Xmx1g` 的非堆余量太薄。堆本身仍由 `-KafkaProfile` 决定(`$KafkaHeapOpts`)。
- **PVC 生命周期**:删 StatefulSet 不删 PVC,删 namespace(`infra-down`)才删。`infra-down` → `infra-up` 之后是一套空 Kafka,
  按上面第三条重跑 `infra-kafka-topics`。
- **kind / minikube**:PVC 要有默认 StorageClass(kind 自带 local-path)。`infra-status` 会列 `sts` / `pvc` / `pdb`;
  kafka 起不来先看 PVC 是否 Pending。`-WaitReady` 下 `infra-up` 会在 etcd quorum 之后、`kafka-topic-init` 之前等
  `sts/kafka` Ready。

### 镜像钉版本(2026-10-15)

- **四处统一钉** `apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837`:
  `manifests/infra/kafka.yaml`(同时显式设 `imagePullPolicy: IfNotPresent`)、`manifests/infra/kafka-topic-init.yaml`、
  `deploy/docker-compose.yml` 的 `kafka` 与 `kafka-topic-init`。四处的 tag 与 digest 必须完全相同,改一处就是四处同改。
- **为什么**:以前是浮动 tag `apache/kafka:latest`,且没设 `imagePullPolicy`(`:latest` 缺省 `Always`)。PVC 化之后,
  一次重启就可能让新版本 broker 去读旧版本写下的日志目录与 KRaft 元数据,版本在无人察觉时漂移。另外 group coordinator
  与 OffsetDelete(`kafka-consumer-groups.sh --delete-offsets`)的语义会随镜像变化,go/db 存盘消费组按 zone 拆分后的
  交接与 `tools/scripts/kafka_offset_reset.ps1` 的收尾都依赖它们。
- **钉版不改变运行版本的依据**:2026-10-15 查 Docker Hub,`4.3.1`(2026-06-23 推送)与 `latest`(2026-06-24 推送)
  是同一个 index digest。这只是 registry 上的对比:正在跑的 broker 拉的是 Pod 创建时的 `latest`,本机 compose 用的是
  本地缓存的 `latest`,两者都可能更旧。
- **合入前先核对(V0,由负责人或 Codex 执行,不阻塞落码)**:
  1. 集群(`-n` 换成实际的 `-InfraNamespace`):
     `kubectl get pod -n mmorpg-infra -l app=kafka -o jsonpath='{range .items[*]}{.metadata.name}{" "}{.status.containerStatuses[0].imageID}{"\n"}{end}'`,
     每个 broker 的 imageID 都要含 `sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837`;
  2. 本机缓存里有该镜像时:`docker image inspect apache/kafka:latest --format '{{json .RepoDigests}}'`,结果要含同一个 digest。

  任一不一致,这次钉版的发布就要按一次 broker 版本升级来评估:KRaft `metadata.version`、日志目录兼容性、能否回退,
  由负责人决定是否另开窗口。本机命名卷 `kafka-data` 可以在下一次压测前按 AGENTS.md §6.2 清掉。
- **apply 之后的影响**:镜像字符串变了,下一次 `infra-up` 会滚动重启 kafka StatefulSet。单 broker 档等于一次 broker 重启:
  数据在 PVC 上、topic 不丢,但重启期间产出的控制面命令会丢(与上面「第一次切换」那条说的同性质),所以在没有玩家的窗口做,
  完成后跑一次 `-Command infra-kafka-topics -WaitReady` 复核分区契约。多 broker 档按 PDB(`minAvailable: N−1`)逐个滚动。
- **升级步骤**(换版本时):
  1. `docker buildx imagetools inspect apache/kafka:<新版本>`,取输出里的 `Digest:` 行(多平台 index 的 digest,
     不是某一个平台的 manifest digest);
  2. 四处同改为 `apache/kafka:<新版本>@<digest>`;
  3. 本机 `docker compose -f deploy/docker-compose.yml up -d kafka kafka-topic-init`:`kafka-topic-init` 必须以 0 退出,
     `docker exec kafka /opt/kafka/bin/kafka-topics.sh --version` 显示新版本,并按上面「文件描述符」一条核一遍入口路径;
  4. 在新版本上重新核对 go/db 交接与运维脚本依赖的 `kafka-consumer-groups.sh` 行为,逐条记下原始输出与退出码:
     对 Stable 组执行 `--reset-offsets --execute`;`--describe` 一个不存在的组(文案是 `does not exist` 还是
     `GroupIdNotFoundException`);对不存在的组执行 `--reset-offsets --to-datetime --execute` 能否建出位点;
     组 Stable 但没有成员订阅该 topic 时执行 `--delete-offsets`;有成员订阅时的输出(预期含 `GROUP_SUBSCRIBED_TO_TOPIC`)。
     完整清单与 4.3.1 上的原始输出记录在 `docs/design/db-consumer-group-per-zone.md` §11;
  5. 跨大版本前先读 Kafka 官方升级说明(KRaft `metadata.version` 的升级与能否回退),再在没有玩家的窗口 `infra-up`。

### 未改、但值得知道的一件事

- 堆仍是 `-Xms512m -Xmx1g`(`prod-like`)。2×256 个命令分区 + 审计 + 各 zone 的 `db_task` 在 1G 堆上偏紧,
  真上量前应该按分区总数复核一次。

## Kafka:多 broker(2026-10-01,docs/design/no-single-node-horizontal-scaling-20261001.md §2)

`-KafkaBrokers <n>` 决定 broker 数。缺省 0 = 按档位:`-ReleaseProfile` 不是 dev,或 `-OpsProfile` 不是 custom → **3**;其余 → 1。
只接受 1 或 ≥3。脚本启动时会打一行 `Kafka topology: brokers=… controllers=… replication_factor=… min_insync_replicas=…`。

- **拓扑**:前 3 个 Pod(`kafka-0..2`)兼任 controller,组成 3 票的 KRaft 选举组(挂 1 个仍有多数派);第 4 个起只当 broker。
  topic 副本数 3、`min.insync.replicas` 2:任何一个 broker 倒下,`acks=all` 的生产者仍然写得进去。
  PDB 是 `minAvailable: N−1`(一次最多自愿驱逐一个),Pod 之间带 preferred 反亲和(尽量一台机器一个)。
- **客户端不用改配置**。所有服务仍然只写 `kafka.<InfraNamespace>:9092`:这个 Service 只做 bootstrap 入口,
  之后客户端按每个 broker 自己广播的 `kafka-<n>.kafka-headless.<ns>.svc.cluster.local:9092` 直连。
- **zone-up 与 infra-up 必须用同一套档位参数**(或同一个 `-KafkaBrokers`)。Go 服务建自己的 topic(`db_task_*`、`match-results` 等)
  用的副本数来自注入的 `KAFKA_TOPIC_REPLICATION_FACTOR`;两边对不上时,3 broker 集群上会建出 1 份副本的 topic,
  随后 `acks=all` 的写入被 broker 以 `NOT_ENOUGH_REPLICAS` 拒掉(有意的 fail-closed)。看启动行的 `replication_factor=` 是否一致。
- **1 → 3 不能原地切换,必须重建**(脚本会拒绝并提示)。原因:单 broker 的选举组只有 kafka-0 一票,把成员表改成三票并扩容时,
  两个空白的新节点可以先凑成多数派、用一份空的元数据日志当选,kafka-0 回来时被截断,全部 topic 元数据消失。
  重建流程(**有停机窗口,Kafka 里的未消费消息全部丢弃**,在没有玩家的窗口做):
  1. 停掉所有 zone(`all-down` 或逐个 `zone-down`)与全局 Go 服务,确认 `db_task` 已被 db 消费完(否则丢的是玩家存档)。
  2. `kubectl -n <infra> delete statefulset kafka`,再 `kubectl -n <infra> delete pvc -l app=kafka`(PVC 不删,新集群会读到旧的单票元数据)。
  3. `k8s_deploy.ps1 -Command infra-up -KafkaBrokers 3 -WaitReady …`(同一次里会重跑 `kafka-topic-init`,按 3 份副本重建契约 topic)。
  4. 重新 `zone-up`。各服务启动时 `EnsureTopics` 按 3 份副本建自己的 topic。
- **3 → N 是普通扩容**:只改 `replicas`,新 Pod 只当 broker,不动选举组。但**新 broker 不会自动接手已有 topic 的分区**,
  要用 `kafka-reassign-partitions.sh` 把副本挪过去。缩容(N → 更小)脚本一律拒绝:被摘掉的 broker 上还有分区副本。
- **副本数不够的 topic 会被拦下**:`kafka-topic-init` 与 `kafkautil.EnsureTopics` 都读回已存在 topic 的副本数,低于要求即失败。
  修法:topic 是空的就删掉重建;有数据就用 `kafka-reassign-partitions.sh` 补副本(副本数不参与寻址,可以原地改,不需要换代号)。
- **上线前必须在 kind 上实测的一件事**:就绪探针用的 `kafka-broker-api-versions.sh` 会逐个询问集群成员。
  预期是别的 broker 不在时它对那个节点报错、整体仍以 0 退出;若实测发现杀掉一个 broker 会让其余 broker 也变 NotReady,
  说明这个预期不成立,要把就绪探针换成只看本机的判据。步骤:三 broker 起来后 `kubectl -n <infra> delete pod kafka-1`,
  观察 `kafka-0` / `kafka-2` 是否保持 `1/1 Ready`,同时跑一轮 robot login-test。
- **未做**:独立 controller 节点(现在是 combined 模式,Apache 建议关键环境把 controller 拆出去);镜像仍是浮动 tag。

## Kafka 保留期:一条规则(2026-09-08,routing-identity-audit-20260908.md R11)

> **任何"消费者可能落后"的 topic,保留期必须大于消费者可能落后的最长时间,并留足余量;而且必须显式声明,不许继承 broker 默认值。**

落后多久算封顶,取决于消费者:

- C++ 的 gate / scene 消费者把 `max.poll.interval.ms` 钉在 **900000**(15 分钟,
  `cpp/libs/engine/infra/messaging/kafka/kafka_consumer.cpp`)。也就是说 broker 眼里一个消费者"合法地"消失 15 分钟仍然算活着,
  回来时必须还能读到那 15 分钟的消息。保留期低于 900s 时它读不到的不是"旧数据",是 BindSession / RoutePlayer / KickPlayer,
  而且 **Kafka 一个错都不报** —— 表现是玩家卡在登录或进世界。
- `db_task` 那条链的封顶不是 poll 间隔而是 **MySQL 故障时长**:消费者停多久,积压就要留多久,否则丢的是玩家存档。

### 现状表(2026-09-08 盘点)

| topic | 谁建 | 保留期 | 消费者 | 判定 |
|---|---|---|---|---|
| `gate-cmd_g<N>` / `scene-cmd_g<N>` | `kafka-topic-init`(Job + compose) | **显式 1h** | C++ gate / scene(assign) | ✅ 4 × 900s |
| `transaction_log_topic_g<N>` / `player_snapshot_topic_g<N>` | 同上 + data-service `EnsureTopics` | **显式 30d** | data-service | ✅ |
| `db_task_zone_<N>` | login / db 启动时 `EnsureTopics` | **显式**,见下 | go/db | ✅(本轮修复) |
| `match-results` | match `StartResultConsumer` | 显式 7d(match 侧常量) | match | ✅ |
| `gate-<id>` / `scene-<id>`(迁移窗口残留) | broker auto-create | broker 默认值 | C++ legacy group 订阅(只剩本机;K8s 自 2026-10-15 起 `DisableLegacyPerNodeTopic: true`,不再订阅也不再 auto-create) | ⚠️ 只能靠 broker 默认值兜底,本轮把它抬到 900s 以上 |
| `game-events` | compose(K8s 侧从不建) | 本轮改为显式 1h | **无**(没有节点为它注册消费回调;`KafkaManager::Init` 自 2026-10-15 起不做任何顶层 subscribe,与 `Kafka.GroupID` 无关。K8s node ConfigMap 的 GroupID 同日改为镜像 bin/etc 的空串) | ✅ |
| `__mmorpg_partition_contract_*` | `kafkautil.EnsureTopics` | `cleanup.policy=compact`,只是个标记 | 无 | ✅ 不受保留期影响 |

### 本轮改了什么

- **broker 默认保留期**(`$KafkaBrokerRetentionMs` → `KAFKA_LOG_RETENTION_MS`):`dev` 60s → **30 分钟**,
  `prod-like` 300s → **1 小时**,`custom/default` 300s → **1 小时**。它是所有"没有自己 `retention.ms`"的 topic 的保留期,
  也就是迁移窗口里 auto-create 出来的 `gate-<id>` / `scene-<id>` —— **R11 描述的正是这一条**。
  `deploy/env/kafka.dev.env` / `kafka.prod-like.env` 与 `deploy/docker-compose.yml` 的内联默认值同步改。
- **`db_task_zone_<N>`**(`$KafkaDbTaskRetentionMs` → login ConfigMap 的 `Kafka.RetentionMs`):
  `dev` 300s → **1 小时**,`prod-like` 600s → **6 小时**,`custom/default` 900s → **24 小时**。
  除了"低于 900s"之外,这里还有一个更难发现的问题:**login 与 db 都会在启动时对同一个 `db_task_zone_<N>` 执行
  `IncrementalAlterConfigs`**(`kafkautil.EnsureTopics`),而 db 的 K8s ConfigMap 根本不写 `RetentionMs`、走 Go 结构体
  默认值 86400000(24h),login 的 ConfigMap 写的是脚本注入值。于是这个 topic 的保留期会**随两个服务的重启顺序在
  15 分钟和 24 小时之间反复横跳**。把 `custom/default` 对齐到 86400000 只消掉了 prod 档的横跳:
  **2026-10-16 更正(S5b,B-15)**:那次对齐之后 db 的 ConfigMap 仍不写 `RetentionMs`,dev / prod-like 档的保留期照样在 1h / 6h 与 24h 之间横跳。
  现在 db 与 login 两个 ConfigMap 的 Kafka 段都写 `RetentionMs` 与 `RetentionBytes`,从生成器里同一个变量渲染,所有档位两边同值,
  契约测试断言两个 ConfigMap 相等。真要改保留期,改 `Apply-KafkaProfileDefaults`(或传 `-KafkaDbTaskRetentionMs`),
  不要只改一个服务的 etc yaml(K8s 上 ConfigMap 覆盖它)。
- **保留期改为"声明"而非"创建时附带"**:`kafka-topics.sh --create --config retention.ms=...` 只在新建那一刻生效。
  一个已经存在的 topic(被生产者抢先 auto-create、或上一代 Job 用别的值建的)会静默继承 broker 默认值。
  所以 K8s Job 与 compose 的 init 现在都在建完之后再 `kafka-configs.sh --alter --add-config retention.ms=...`
  并 `--describe` 读回核对,不一致就退出 1。与分区数不同,`retention.ms` 是可变配置,重设没有寻址风险。

### 加新 topic 时

先回答一句话:**这个 topic 的消费者最长可能落后多久?** 然后把保留期设成它的 2 倍以上,并在建 topic 的地方
显式写 `retention.ms`(`kafkautil.TopicSpec.RetentionMs` 或 init 脚本的 `ensure_topic` / `ensure_retention`)。
不要留给 broker 默认值 —— 那个值是给"没有契约的 topic"用的下限,不是给你的 topic 用的。

### 离线校验(本轮改动全部不碰集群)

```powershell
# 1) 语法:0 errors
pwsh -NoProfile -Command "$e=$null;$t=$null;[System.Management.Automation.Language.Parser]::ParseFile('tools/scripts/k8s_deploy.ps1',[ref]$t,[ref]$e)|Out-Null; $e.Count"
# 2) 渲染(bogus context,永不连集群)
pwsh -File tools/scripts/k8s_deploy.ps1 -Command infra-up -DryRun -KubeContext no-such-context-dryrun-only
pwsh -File tools/scripts/k8s_deploy.ps1 -Command zone-up -ZoneName zone-1 -ZoneId 1 -GoSvcRegistry local.test -DryRun -KubeContext no-such-context-dryrun-only
# 3) compose
docker compose -f deploy/docker-compose.yml config
```

再用 PyYAML 断言渲染结果:StatefulSet 有 `volumeClaimTemplates` / 三个探针 / PDB、Service `kafka` 名字与
9092+9093 端口没变、PVC 的 `mountPath` 等于 `KAFKA_LOG_DIRS`、两条 topic-init 路径都对四个 topic 显式设了保留期、
渲染后的 broker 默认保留期 > 900000。manifest 里嵌的 shell 可以用
`yaml.safe_load` 抽出来跑 `sh -n`(Job 的 `command[2]`、kafka 的 `args[0]`、compose 的 `entrypoint[2]`)。

## Kafka 磁盘预算与 db_task retention.bytes(2026-10-16,S5b;未编译、未测试,待 Codex 验证)

规格:`docs/handoff/godb-pipeline-impl-20261015/S5-dirty-snapshot-retention-spec.md`(B-10 / B-11 / B-15 / B-16 / B-17 / B-18);
设计:`docs/design/db-task-delivery-and-retention.md` §B;容量依据:容量模型 A0-3。

以前 `db_task_zone_<id>` 每分区只有 broker 缺省的 `retention.bytes`(dev 128MiB,其余 512MiB)兜底,与 PVC 大小无关:
zone 一多,Σ 保留上限就超过盘(容量模型估算是 PVC 的 2.5~5.8 倍),写满盘时整个 broker 停摆;盘又写死 `20Gi`。
现在 `k8s_deploy.ps1` 在写路径入口(zone-up / all-up / infra-up)算一份盘预算,同时决定 PVC 大小与 db_task 的 topic 级
`retention.bytes`,两者在同一个公式里对账。

### 公式(`Get-KafkaDbTaskRetentionBytesTarget` / `Get-KafkaDiskPlan`)

记 R = 每 zone 的 db_task 设计字节率,T = min(缓冲目标, db_task retention.ms),P = 每 zone 的 db_task 分区数
(`go/db/etc/db.yaml` 的 `ServerConfig.Kafka.PartitionCnt`,现为 10),seg = segment 字节数,
Z = zone 数(zones 配置 ∪ `-ZoneId`;非 DryRun 时再加上线上名字既不在 zones 配置里、也不是 `-ZoneName` 的 zone namespace,每个算一个),
A = 审计 topic 分区数(`data_service.yaml` 的 `TransactionLogPartitions` + `SnapshotPartitions`,现为 6 + 3 = 9,按 broker 缺省
`retention.bytes` 记),reserve = 其它 topic 预留(GiB),Gi = 1073741824:

```text
target      = roundUp(R × T × 1.2 ÷ P, 64MiB) + seg                        # 1.2 = 分区间倾斜余量;+seg:只删整个已关闭的 segment
required    = 1.3 × (Z × P × (target + seg) + A × (broker 缺省 retention.bytes + seg) + reserve × Gi)   # 每个 broker 各持有每个分区一份(上界)
RequiredGi  = max(20, ceil(required ÷ Gi ÷ 10) × 10)
EffectiveGi = -KafkaStorageGi(> 0)
            > 线上 StatefulSet kafka 在:min(它的模板, 最小的实际 PVC)                # pvc_source = explicit / live / live-pvc / computed
            > StatefulSet 不在、PVC 在(--cascade=orphan 之后):最小的实际 PVC
            > RequiredGi
capacity    = (EffectiveGi × Gi ÷ 1.3 − A × (broker 缺省 + seg) − reserve × Gi) ÷ (Z × P) − seg
生效值      = target ≤ capacity ? target : floor(capacity, 64MiB)                 # 后者 = FitLimited,按盘收缩
```

`floor(capacity, 64MiB)` 连 2 个 segment 都不到时 throw(每分区只剩活跃 segment,缓冲约等于零);其余情况**只告警、不阻断**
(负责人 2026-10-13 拍板)。生效值同时渲染进 `go-svc-db-config` 与 `go-svc-login-config` 的 `Kafka.RetentionBytes`
(两边同值,`RetentionMs` 也一样,契约测试钉住),两个服务启动时经 `kafkautil.EnsureTopics` 在 topic 级设置它。

### 缺省值(`Apply-KafkaProfileDefaults`;参数 0 = 按档位)

| `-KafkaProfile` | 设计速率 `-KafkaDbTaskDesignBytesPerSec` | 缓冲目标 `-KafkaDbTaskBufferTargetMs` | db_task retention.ms | segment | 其它预留 `-KafkaOtherTopicsReserveGi` | target retention.bytes | 2 个 zone 的 RequiredGi | 按设计速率的缓冲 |
|---|---|---|---|---|---|---|---|---|
| dev | 262144(256 KiB/s) | 3600000(1h) | 1h | 16MiB | 1 | 150994944(144MiB) | 20(公式 ≈ 7.0,取下限) | ≈ 71 分钟 |
| prod-like | 1048576(1 MiB/s) | 7200000(2h) | 6h | 32MiB | 2 | 973078528(928MiB) | 40(≈ 33.2) | ≈ 124 分钟 |
| prod(缺省) | 3145728(3 MiB/s) | 7200000(2h) | 24h | 32MiB | 4 | 2785017856(2656MiB) | 80(≈ 79.7) | ≈ 121 分钟 |

- broker 数与 RF 不进公式(2026-10-16 R5a 复核更正,以前按「RF ÷ broker 数」均摊):一个 broker 对同一分区至多持有一份副本,
  每个 broker 的份额按「持有全部分区各一份」取上界。均摊只在副本已在各 broker 间分匀时成立,而 3 → N 扩容是放行的、新 broker
  不会自动接手已有分区(见「Kafka:多 broker」):`kafka-reassign-partitions.sh` 之前老的 3 个 broker 仍持有全部分区。
  例:prod、线上 20Gi、2 个 zone、`-KafkaBrokers 5` 时按均摊会把生效值算成 805306368,老 broker 上光 db_task 与审计、预留就要约 26.2GB,超过 20Gi;
  现在与 3 broker 相同(268435456)。单 broker(RF 1)与 3 broker(RF 3)结果不变,≥ 4 个 broker 时偏保守(没有「已均衡」的开关,YAGNI)。
- 缓冲目标取 2h 是全局单点交接对 db_task 缓冲的下限(≥ 2h)。prod / prod-like 的 retention.bytes 按 2h 算,会先于 24h / 6h 的
  retention.ms 生效:按设计速率,DB 故障超过约 2 小时就开始丢存盘积压,回档的 TargetTime 也不能早于这个窗口。账本行会就此打
  `[KafkaDiskLedger]BufferBelowRetention:` 告警(每次 prod 档部署都会出现,属预期;要消掉就调大缓冲目标并按下面的 runbook 扩盘)。
- 显式 `-KafkaDbTaskRetentionBytes <B>` 覆盖 target(仍会按盘收缩);负数、或小于 2 个 segment 在写路径入口拒绝。
- 设计速率是推算值,`implied_db_task_buffer_min` 只是估算,不是承诺的缓冲时长。

### 账本行(每次 zone-up / all-up / infra-up 打一行)

```text
Kafka disk ledger: brokers=1 rf=1 zones=2 db_task_partitions=10 segment_bytes=33554432 db_task_retention_bytes=2785017856 target_retention_bytes=2785017856 pvc_gib=80 pvc_source=computed required_gib=80 implied_db_task_buffer_min=121 db_task_retention_ms_min=1440 design_bytes_per_sec=3145728 audit_partitions=9 other_topics_reserve_gib=4 live_sts_gib=none live_pvc_min_gib=none live_extra_zones=unread
```

(按仓库当前值手算,未运行。)字段:`db_task_retention_bytes` = 生效值(渲染进 ConfigMap 的那个),`target_retention_bytes` = 目标值;
`pvc_gib` / `pvc_source` = 本次用的盘及其来源(explicit / live / live-pvc / computed);`required_gib` = 按目标值需要的盘;
`implied_db_task_buffer_min` = 按设计速率估算的缓冲分钟数;`db_task_retention_ms_min` = retention.ms 折成分钟;
`live_sts_gib` / `live_pvc_min_gib` = 线上 StatefulSet 模板 / 各块 PVC 实际容量的最小值(`none` = 没有,DryRun 一律 `none`),`pvc_gib` 在 live 时取两者中小的;
`live_extra_zones` = 线上不在 zones 配置里的 zone namespace 数,已计入 `zones`(`unread` = DryRun 没读)。zones 配置缺席时
行尾带 `zones_config=missing`。随后一行 `Kafka disk ledger note:` 提醒 retention.bytes 是 topic 级设置。

- **FitLimited**(`[KafkaDiskLedger]FitLimited:` 告警):现有的盘装不下目标值,retention.bytes 已按盘收缩。例:prod 档、线上 20Gi、2 个 zone →
  从 2785017856 收缩到 268435456(256MiB),按设计速率约能缓冲 10 分钟。真要恢复目标,按下面的 runbook 扩盘到 `required_gib`。
  **收缩后的值低于 topic 现值时(例如 broker 缺省 512MiB → 256MiB),先确认 db 消费 lag 为 0 再发布**:db / login 滚动重启时
  `EnsureTopics` 立刻下调 topic 级 retention.bytes,Kafka 在下一次 retention 检查(`retention.check.interval`,2 分钟)内就删掉
  超出部分的最旧积压,不管 db 读没读到。zone-up 常常正是在 DB 故障期间为修 db 而跑的,这时删掉的就是积压的存盘;
  lag 不为 0 时先扩盘(下面的 runbook),或等 db 追平再发布。
- **ZonesConfigMissing**(`[KafkaDiskLedger]ZonesConfigMissing:` 告警,2026-10-16 起):zones 配置缺席时 Z 只数 `-ZoneId`
  (非 DryRun 再加上线上其它 zone namespace),还没部署的 zone 数不到,retention.bytes 可能超量。`-ReleaseProfile dev` 只告警;
  **staging / prod 直接拒绝部署**(含 infra-up:它也按 Z 定 PVC 大小),用 `-ZonesConfigPath` 指向登记了共用这套 Kafka 的全部 zone 的配置。
- **LiveZonesNotInConfig**(`[KafkaDiskLedger]LiveZonesNotInConfig:` 告警):线上有 zone namespace 不在 zones 配置里(例如用 `zone-up -ZoneId`
  单独部署、没登记的 zone),已按一个 zone 计入 Z。补进 zones 配置(或 zone-down 掉它),否则不同入口算出的 Z 不一样。
  名字以 `-NamespacePrefix-` 开头的 namespace 都算(不看里面有没有 go/db,多算只会偏保守);列 namespace 失败一律拒绝部署。
  **残余风险**:zone-down 删的是 namespace,但那个 zone 的 db_task topic 仍按旧的 retention.bytes 留着数据,直到 retention.ms 到期(prod 24h),这里数不到。
  zone-down 之后 retention.ms 之内部署别的 zone 时,这块残留按超量承诺对待:用下面「已存在的 topic 立即改配置」把那个 topic 的 retention.bytes 调小。
- 线上盘同时读两处:StatefulSet 的 `volumeClaimTemplates`,以及每块 `kafka-data-kafka-<i>` 的实际容量(`status.capacity`;
  还没绑定的 PVC 取它的 `requests`)。StatefulSet 在时 retention.bytes 按两者中小的算,kafka.yaml 照旧渲染线上模板(模板不可变);
  StatefulSet 不在(扩盘 runbook 第 ③ 步 `--cascade=orphan` 之后)但 PVC 在时按最小的实际 PVC 算并以它为模板(`pvc_source=live-pvc`)。
  只 patch 了 PVC、没重建 StatefulSet 时按旧模板(更小的)算,方向是保守的;反过来,有一块 PVC 没扩成、停在 `FileSystemResizePending`,
  也按那块实际的盘算。同 namespace 里所有名字匹配的 PVC 都算(含已缩容留下的旧序号 PVC,偏保守)。
- 线上大小只认 `<整数>Gi`;读不出、别的单位、kubectl 失败一律拒绝部署(fail-closed)。DryRun 不查集群,一律按「线上没有」算。

### 扩盘 runbook(StatefulSet 的 volumeClaimTemplates 不可变)

显式 `-KafkaStorageGi N` 与线上不同时脚本拒绝:比线上模板大 = `expand-refused`(附本 runbook),比线上小 = `shrink-refused`(PVC 不能缩);
N 大于最小的实际 PVC = `pvc-not-expanded`(某块盘还没扩到 N:先完成第 2 步并确认 CAPACITY),StatefulSet 在不在都判。
zone-up 同样判(它按这块盘算 retention.bytes;按一块线上并不存在的盘算出来的值会写爆真实的盘)——这是相对规格多出的一处,规格只在 infra-up / all-up 判。

1. `kubectl get sc <StorageClass> -o jsonpath={.allowVolumeExpansion}` 必须是 `true`;不支持扩容的 StorageClass 只能按「Kafka:多 broker」的重建流程换盘。
2. 逐个 broker:`kubectl -n <infra> patch pvc kafka-data-kafka-<i> -p '{"spec":{"resources":{"requests":{"storage":"<N>Gi"}}}}'`,
   `kubectl -n <infra> get pvc kafka-data-kafka-<i>` 确认 CAPACITY 已是 `<N>Gi`;需要文件系统扩容的驱动(状态里有 `FileSystemResizePending`)
   逐个重启 broker(`kubectl -n <infra> delete pod kafka-<i>`,等它 Ready),多 broker 时每重启一个都确认 under-replicated 分区为 0:
   `kubectl -n <infra> exec kafka-0 -c kafka -- /bin/sh -c "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-topics.sh --bootstrap-server localhost:9092 --describe --under-replicated-partitions"` 输出为空。
3. `kubectl -n <infra> delete statefulset kafka --cascade=orphan`:只删 StatefulSet 对象,Pod 与 PVC 保留,broker 不停。
4. `k8s_deploy.ps1 -Command infra-up … -KafkaStorageGi <N>`(建议带:脚本核对 N 不大于最小的实际 PVC、否则 `pvc-not-expanded` 拒绝;
   2026-10-16 起不带也不会按公式值建模板,而是按最小的实际 PVC 建,`pvc_source=live-pvc`,某块没扩成时模板就停在旧大小)。
   ③ 与 ④ 之间跑的 zone-up 同样按最小的实际 PVC 算 retention.bytes。新 StatefulSet 收养原有 Pod 与 PVC;之后
   `kubectl -n <infra> get sts kafka -o jsonpath={.spec.volumeClaimTemplates[0].spec.resources.requests.storage}` 应为 `<N>Gi`。
5. 对**每个** zone 重跑 zone-up,db / login 重启时按新盘设置 retention.bytes;不想为此滚动 go/db 时用下一节的 `--alter` 立即生效,
   ConfigMap 等下次正常发布再收敛。

### 已存在的 topic 立即改配置 / 回滚

exec 进 broker 跑工具时必须把 `KAFKA_HEAP_OPTS` 覆盖成小堆(理由见上面「探针」一条:容器 env 的大堆会被工具继承,和 broker 叠在同一个 memory limit 里 OOMKilled):

```bash
# topic 名 = db_task_zone_<ZoneId>,TopicGeneration > 1 时带 _g<N>(与 Get-DbTaskTopicName 同一规则)
kubectl -n <infra> exec kafka-0 -c kafka -- /bin/sh -c "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-configs.sh --bootstrap-server localhost:9092 --entity-type topics --entity-name db_task_zone_<id> --alter --add-config retention.bytes=<B>,retention.ms=<T>"
kubectl -n <infra> exec kafka-0 -c kafka -- /bin/sh -c "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-configs.sh --bootstrap-server localhost:9092 --entity-type topics --entity-name db_task_zone_<id> --describe"
# 回滚:去掉 topic 级配置,回到 broker 缺省
kubectl -n <infra> exec kafka-0 -c kafka -- /bin/sh -c "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-configs.sh --bootstrap-server localhost:9092 --entity-type topics --entity-name db_task_zone_<id> --alter --delete-config retention.bytes"
```

- 顺序:**先扩盘再调大**;调小立即生效(下一次 retention 检查,`retention.check.interval` 为 2 分钟),被删的就是最旧的积压。
- `<B>` 取账本行的 `db_task_retention_bytes`,`<T>` 取 ConfigMap 的 `RetentionMs`;两者都写在 db 与 login 的 ConfigMap 里,下次 zone-up 会收敛到同一组值。
- **加 zone 之后**:新 zone 按新的 Z 拿到更小的 retention.bytes,先部署的 zone 在下次 zone-up 之前仍保留旧值,期间是超量承诺。
  要对**全部** zone 重跑 zone-up,或对每个 db_task topic 用上面的 `--alter`。
- 回滚 S5b:topic 级配置在回滚后仍留在 topic 上,本身无害;要撤就用上面的 `--delete-config`。旧二进制读到 ConfigMap 里的新键
  `RetentionBytes` 会怎样尚未核实(go-zero `conf.MustLoad` 对未知键的行为,规格附录 F9),若会报错,回滚要连同旧的 ConfigMap 生成器一起回退。

### kind / 本机

缺省 `-KafkaProfile prod`:kind 上的新集群会申请 80Gi 的 PVC。local-path 不强制容量,但 retention 上限也随之约是以前的 5 倍,
高写入压测可能写满宿主盘。**本机 / kind 建议显式传 `-KafkaProfile dev`**(PVC 20Gi,target 144MiB)。

### 本轮不做

- lag 秒级告警(B-12):go/db 还没有导出消费 lag 指标,积压逼近缓冲时仍没有主动告警;账本行的 `implied_db_task_buffer_min` 是以后告警阈值的依据。
- 关掉旧的「一节点一 topic」路径(B-13,`DisableLegacyPerNodeTopic`):不在本节做。K8s 的 node ConfigMap 已由全局单点 P0-B 批 2
  (2026-10-15)改写 `true`,见上面「K8s 已关老路径」;本机 `bin/etc` 仍是 `false`。已经 auto-create 出来的 `gate-<id>` / `scene-<id>`
  在 broker 缺省保留期到期前仍占盘,由其它 topic 预留(`-KafkaOtherTopicsReserveGi`)覆盖。
- 预留只是固定值,没有按 broker 缺省 retention.bytes 给控制面 topic 取最坏值;到大规模时(控制面约 4.8 GB/h 每 broker)要调大 `-KafkaOtherTopicsReserveGi`。
- **验证(交 Codex,未运行)**:`pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1` 末尾「S5b 保留期与 Kafka 盘预算」一节
  (纯函数的缺省值与各判定路径、两个 ConfigMap 的 RetentionMs / RetentionBytes 相等、账本行、kafka 清单的 `storage:`、负向参数;
  2026-10-16 R5a 复核补:5 broker 取上界、模板与实际 PVC 取小 / `live-pvc` / `pvc-not-expanded`、线上 zone namespace 并入 Z、
  zones 配置缺席的告警与非 dev 档拒绝、FitLimited 的 lag 提醒)。

## go/db 存盘管线告警(PrometheusRule,手工 apply;2026-10-16 登记,未在集群上 apply 过)

仓库不部署 Prometheus,下面两份与 `owner-epoch-alerts.yaml` 同法手工安装;上线清单是 `docs/ops/godb-pipeline-launch-todo.md` §2.5
(2.5.2 / 2.5.8)。抓取前提:go/db 的 `:9160`(db ConfigMap 的 `MetricsListenAddr`)有 ServiceMonitor / PodMonitor,否则规则全部是哑的。

- `deploy/k8s/db-task-retry-alerts.yaml`:存储不可用时的延后与冷却、死信、重试队列(`db-task.retry-and-dead-letter`)、排序锁租约
  (`db-task.ordering-lease`)、消费组交接闸(`db-task.consumer-group-handoff`)。
  ```bash
  kubectl apply -n observability -f deploy/k8s/db-task-retry-alerts.yaml
  ```
- `deploy/k8s/db-task-partition-alerts.yaml`:db_task 读写分区契约(`db-task.partition-contract`,指标 `db_task_producer_partition_total`)。
  **安装时机**:写规则 `DbTaskWritePartitionContractBroken`(critical)在 go/db 观测上线后即可 apply;读规则 `DbTaskReadPartitionMismatch`
  必须等 login 全部滚到按契约分区的版本之后才放开 —— 滚动期间旧版 login 的读按 sarama 默认 FNV 分区,必然不符,会误报。
  先上写规则时把读规则注释掉再 apply,login 滚完后放开、重 apply 同一文件。
  ```bash
  kubectl apply -n observability -f deploy/k8s/db-task-partition-alerts.yaml
  # 回滚
  kubectl delete -n observability -f deploy/k8s/db-task-partition-alerts.yaml
  ```
