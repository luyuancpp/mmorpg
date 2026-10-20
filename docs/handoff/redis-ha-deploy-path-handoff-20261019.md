# 交接：共享 Redis 高可用形态接进部署路径（2026-10-19）

> 写给换账号后接手的会话。接手前先按 `AGENTS.md`「会话启动门禁」读一遍，再读本文。
> **状态一句话**：代码、测试与文档都已写完，**全部未运行**（契约测试、DryRun、kind 复测都没跑）。下一步是交给 Codex 验证，再由负责人提交。

## 1. 需求与范围

负责人原话：「先把 redis-ha（一主两从加哨兵）接进部署路径，按文件头列的前置做：maxclients、连接总账、WAITAOF 副本数。只写代码和文档，给出 Codex 验证命令。」

协作约定（AGENTS §10.1，以及负责人一贯的要求）：Claude 只写代码和文档；协议生成、编译、测试、冒烟交给 Codex。没有 Codex 结果之前，不得写「已验证 / 测试绿」。

背景：
- origin 的 `43284eb0b`（2026-10-16）已经把 `redis-ha.yaml` 接进 `k8s_deploy.ps1`。
- 负责人 10-19 用 `fc3ff045e` 把 origin/main 合进本地 main，`c63b22dca` 是合并后的 HEAD。
- 本次在这个基础上补齐上面三项前置，并修掉合并与评审中发现的问题。

## 2. 哪些已入库、哪些还没提交

截至 2026-10-19 写本文时，HEAD 是 `c63b22dca`。同一工作区里有别的会话在并行改文件，所以**提交时按下面的清单逐个 add，不要 `git add -A`**。

### 2.1 已在 HEAD 里（负责人的快照 / 合并提交带进去的）

- `deploy/k8s/manifests/infra/redis-ha.yaml`：
  - `MAXCLIENTS=40000` 与 `PROXY_MAXCONN=13000` 写进各自的 Pod 模板；
  - `redis-start.sh` 的参数表带 `--maxclients "$MAXCLIENTS"`，并打印 `redis fd limit:` 一行；
  - haproxy 的 global / defaults 两处 maxconn 由 `PROXY_MAXCONN` 生成；
  - 两个摘要注解；
  - 文件头改成现状描述。
- `tools/scripts/dev_tools.ps1`、`tools/scripts/k8s_image.ps1`：新增 `-RedisReplicas`，非 0 才透传。
- `tools/scripts/k8s_deploy.ps1` 的第一版改动：参数、拓扑函数、Apply-Infra 接线、WAITAOF 按拓扑、跟随线上、代理总账。但这一版里**同名函数有两份**，见 §2.2 第一条。

### 2.2 本会话改了、尚未提交（我的文件）

| 文件 | 内容 |
|---|---|
| `tools/scripts/k8s_deploy.ps1` | ① 删掉合并留下的第二份 Redis 拓扑函数（origin 旧版排在后面）。PowerShell 中后定义的函数会覆盖先定义的，解析时不报错，导致跟随线上的逻辑静默失效。② 半截切换闸：`Get-LiveRedisOrphanPodOrdinals`、`Get-RedisReplicasFromOrphanOrdinals`、`Get-LiveRedisReplicas`（返回一次读数 `{Replicas; Source; Ordinals}`），重启闸新增 `orphan-pods` 判定。③ 拒绝文本按读数来源书写：`Format-RedisLiveShape`、`Get-RedisLiveShapeManualFix`。④ 几处文案：Upgrade 告警提示「不想切就传 -RedisReplicas 1」，单实例拒绝文本改为「所选 Redis 清单」。 |
| `tools/scripts/k8s_gate_drain.ps1` | Redis 失败时的提示改为 `Get-GateDrainRedisFailureHint`，两种形态都写清楚；DESCRIPTION 与注释同步。exec 目标 `statefulset/redis -c redis` 已在 HEAD 里。 |
| `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | 见 §4.2。 |
| `tools/scripts/tests/redis_maxclients_contract.tests.ps1` | 新增 T6（高可用 MAXCLIENTS）、T7（PROXY_MAXCONN）。 |
| `tools/scripts/tests/k8s_gate_drain.tests.ps1` | exec 目标 `statefulset/redis -c redis` 的断言已在 HEAD 里；本次只新增断言，钉住失败提示的两种形态。 |
| `deploy/k8s/README.md` | 「Redis 连接上限与总账」与「共享 Redis:高可用形态」两节：开关、连接上限、重启闸、切换与回退（含中途失败重跑、第 4 步重跑 zone-up 并重启 db / data-service、孤儿 Pod）、运维、已知限制、验证。 |
| `go/db/etc/db.yaml`、`go/data_service/etc/data_service.yaml` | **只改注释**（WaitAOFNumReplicas 由部署脚本按拓扑渲染；本文件保持 0）。值没动。 |
| `docs/design/no-single-node-horizontal-scaling-20261001.md` | §5 补「2026-10-19 补齐」、WAITAOF 的代价、验证命令。 |
| `docs/design/shared-redis-split-by-purpose.md` | §2.5、§4.3、§13 第 13 条追加更正注。 |
| `docs/design/tidb-phase1-and-launch-readiness.md` | §6.1、L-06、L-12、§9.5 改为已接入。 |
| `docs/design/player-route-mapping-durability-proposal.md` | §12.1 表格、§12.2 第 2 步，新增「WAITAOF 与线上形态一致」复核项。**这个文件里混着别的会话的未提交改动**（标题行、§13 等），只提交自己的 hunk，或者和对方协调后一起提交。 |
| `docs/design/db-retry-owner-routing.md` | §1.2、§7.12、§10.2、§13.2：NumReplicas 改为按拓扑渲染。 |
| `docs/design/k8s-client-entry.md` | 379 / 596 / 750 行：exec 目标（379 行已在 HEAD 里）；本次补 596 行的验证项，以及「hairpin 只适用于 etcd 和单实例 Redis」。 |
| `docs/ops/release-checklist.md` | §2.1 ⑦ 新增「共享 Redis 单实例 → 高可用」发布前项；§6.1 现状。 |
| `docs/ops/merge-zone-runbook.md` | 约 884 行 P2 门禁第 3 条：K8s 上 WaitAOF 不手填。**这个文件其余十几处 hunk 是别的会话改的**，不要一起提交。 |
| `docs/ops/godb-pipeline-launch-todo.md` | 9.5.6 标为「部署侧已落码，未运行」。 |
| `docs/PROGRESS.md` | 末尾「2026-10-19 共享 Redis 高可用接入补齐…」一条。文件里还有别的会话追加的条目。 |
| `docs/handoff/redis-ha-deploy-path-handoff-20261019.md` | 本文。 |
| `docs/README.md` | 交接分类里登记本文一行（按字母序排在 `repo-layout` 之前）。文件里还有别的会话的未提交改动。 |

不是我的、不要动：`tools/scripts/dev_tools.ps1`（工作区里的改动属于别的会话）、`tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1`、`deploy/k8s/db-task-retry-alerts.yaml`、`deploy/k8s/persist-schema-alerts.yaml`，以及其它 robot/、docs/ 下的改动。

## 3. 设计要点（为什么这样做）

1. **拓扑只有一个来源。** `Get-RedisTopology`（`-RedisReplicas` + 档位；0 = 非 dev 或非 custom 取 3，否则取 1）同时决定三件事：apply 哪份清单、WAITAOF 副本份数（`Get-MappingRedisTopology` → `Get-RouteStoreWaitAOFNumReplicas`，单实例 0、高可用 1）、部署后核对几个节点。原来的常量 `$script:MappingRedisTopology` 已删除，契约测试钉住它不得再出现。
2. **db 的 WaitAOFNumReplicas 不再镜像 db.yaml。** db.yaml 同时给本机 compose 单实例用，写 1 会让本机 go/db 的 WAITAOF 启动探针等不到副本而拒启。所以 K8s 上按拓扑渲染，NumLocal / TimeoutMs 仍镜像 db.yaml。data-service 同理。
3. **zone 写路径跟随线上**（`Resolve-RedisTopologyFromLive`，只在 zone-up / all-up -SkipInfra 且部署 zone Go 服务、非 DryRun 时执行）。原因：staging / prod 缺省是 3，而线上可能还是单实例。如果不跟随线上，zone-up 和 zone 回滚的 Step 6 会渲染出与线上不符的 WAITAOF：要么停服之后才被拒，要么静默降级。显式传入的值与线上不同，就在写操作之前拒绝。
4. **代理必须比 Redis 先到线。** 用 `Get-RedisHaLimits` 在生成期核对 `PROXY_MAXCONN × (2 + 1) + 1000 ≤ MAXCLIENTS`，并要求与 redis.yaml 同值；总账另起一行 `Redis proxy ledger:`，按单个代理 80% 的水位告警（一个代理不在时，它的客户端全部重连到另一个）。
5. **重启闸在 orphan 删除之前读线上状态。** 所以 1 → 3 照样要求 `-ConfirmRedisCoreRestart`。线上与本次都是高可用时传 `-RollingHandover`，文本改为「逐个滚动重启，每个 Pod 先交接主」。
6. **半截切换。** 1 → 3 的 orphan 删除之后如果 apply 失败，重跑时按孤儿 `redis-<n>` Pod 推算线上副本数：仍判为 upgrade，重启闸判 `orphan-pods`，要求确认。高可用 → 单实例的 README 回退会走「unchanged + orphan-pods」。孤儿序号不连续、或只剩两个时拒绝，并且只给出人工整理的步骤，不建议传一个参数绑定阶段就会被拒的 `-RedisReplicas` 值。
7. **gate 排空不再 exec `svc/redis`。** 高可用形态下这个 Service 背后是 haproxy，镜像里没有 redis-cli；改为 exec 进 `statefulset/redis -c redis`，`-h` 仍然连 Service（经代理落到主）。

## 4. 验证（交 Codex，均未运行）

### 4.1 命令

工作目录为仓库根目录；四条命令互不依赖。失败时保留首个失败用例的输出，不要改断言。

```bash
pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
```
```bash
pwsh -NoProfile -File tools/scripts/tests/redis_maxclients_contract.tests.ps1
```
```bash
pwsh -NoProfile -File tools/scripts/tests/k8s_gate_drain.tests.ps1
```
```bash
pwsh -NoProfile -File tools/scripts/k8s_deploy.ps1 -Command infra-up -DryRun -SkipPreflight -SkipGoSvc -SkipJavaSvc -WaitReady -BattleReplicas 0 -RedisReplicas 3 -InfraNamespace contract-ha -NodeImage registry.invalid/test/mmorpg-node:0123456789ab
```

第 4 条的通过标准：
- 退出码 0；
- 输出含 `Redis topology: replicas=3 manifest=redis-ha.yaml high_availability=true waitaof_replicas=1`、`Redis HA limits:`、`rollout status deployment/redis-proxy`；
- 没有残留的 `__REDIS_` 占位。

### 4.2 本次相关用例（便于定位失败）

`k8s_deploy_contract.tests.ps1`：
- origin 的 6 条：拓扑判定、连接上限不等式、实测教训、口令字符集、`-RedisReplicas 3` 的 DryRun 产物、dev 基线与 prod 缺省。
- 新增：zone 拓扑判定、`Resolve-RedisTopologyFromLive`（含孤儿来源）、拓扑变更闸的拒绝文本、孤儿 Pod 推算、重启闸 `orphan-pods`、重启闸 `-RollingHandover`、Apply-Infra 顺序（AST）、写路径门禁（AST）、prod 代理总账行、包装入口透传 `-RedisReplicas`。
- 改过：db WAITAOF 用例（prod 期望 1）、data-service RouteStore 缺省产物用例（prod 期望 1、`redis_topology=ha`）、mapping 拓扑用例（不得再有常量）、`Get-LiveRedisCoreTemplateDigest` 读线上用例（假 kubectl 加了 `get pods` 分支）、Apply-Infra 只读一次 Redis 清单的 AST 用例（条件允许 `$redisTopology.Manifest`）。

`redis_maxclients_contract.tests.ps1`：T1–T7。

`k8s_gate_drain.tests.ps1`：「Redis 走 login 配置的地址…」一条。

### 4.3 已做的核对（不等于验证）

- 改过的 7 个 PowerShell 脚本做过语法解析，0 错误；
- `k8s_deploy.ps1` 与契约测试按 AST 查过，没有重复的函数定义，也没有重复的用例名；
- `redis-ha.yaml` 做过 YAML 解析。
- 两轮多代理静态评审：
  - 第一轮 4 条发现，确认 2 条，都已修：半截切换闸、gate 排空提示误指 hairpin；
  - 第二轮 3 条确认，是同一件事：孤儿读数下的拒绝文本，已修。
  - **修完之后没有再评审。**

## 5. 剩余事项

| # | 事项 | 谁来做 | 备注 |
|---|---|---|---|
| 1 | 跑 §4.1 四条命令 | Codex | 失败时先判断是测试写错还是代码错，再改；不要改断言凑绿。 |
| 2 | 提交 | 负责人 | 按 §2.2 逐个文件 add；`player-route-mapping-durability-proposal.md`、`merge-zone-runbook.md`、`PROGRESS.md` 混着别的会话的 hunk。 |
| 3 | `deploy/k8s/AGENTS.md` 第 24、82 行仍写「redis single replica」 | 负责人确认后改 | 按 G4a §13 第 14 条，AGENTS 文件的改动需要负责人确认。 |
| 4 | Java 仓 `D:\luyuan\xuanming-server-mmo\PARITY.md` 登记一行「不适用（部署形态）」 | 提交后 | Java 版本号 0.1.0-SNAPSHOT。 |
| 5 | 在 kind 上复测连接上限改动，并在 staging 演练 1 → 3 | 负责人 / 运维（AI 不碰集群） | 按 README「切换与回退」：含第 4 步，以及一次「orphan 删除后中断再重跑」；删主 Pod、杀主进程，用 robot 登录压测观察中断时长与丢写。 |
| 6 | 修完之后补一轮评审 | 接手会话 | 重点看 `Get-LiveRedisReplicas` 改成返回对象后的所有调用点，以及 `Format-RedisLiveShape` / `Get-RedisLiveShapeManualFix` 的用例。 |

### 5.1 评审代理发现、但没改的文档疑点（不在本次范围内，或需要负责人定）

- `deploy/k8s/README.md`：
  - 「连接上限」里「1991 ≤ 10400」是 10-16 的数字，上一节手算的规划峰值是 2027，db_first 口径还要再 +40。三处口径不一致，以部署时打印的数字为准。
  - 文件末尾 D1 一节（另一会话写的）说「改了 redis.yaml 的 Pod 模板 … 重启 redis-0」，没有提到高可用形态是逐个滚动。
  - 「C++ 节点的 zone Redis 口令」上线步骤第 3 步按单实例写。高可用形态下首次开启口令时，主从 masterauth 与哨兵 auth-pass 在滚动期间的新旧口令并存问题没有实测依据，建议负责人确认步骤。
- `docs/ops/godb-pipeline-launch-todo.md`：§9.6 仍写「9.1–9.5 全部待做」；9.1.3 只核 redis-0，高可用形态应逐个核；9.5.2 没提到 prod 缺省高可用时 ConfigMap 是 1/1/1500。
- `go/data_service/etc/data_service.yaml` 第 181–183 行 FenceShards 注释说「K8s ConfigMap 不镜像」，实际已镜像（取 16），注释过时。
- `docs/design/player-route-mapping-durability-proposal.md` 约 1045–1055 行是 10-15 的历史记录（WaitAOF 要手补、FenceShards 没镜像），要不要加订正注由负责人定。
- `docs/ops/release-checklist.md` 文首的「Updated:」没有更新。
- `docs/design/shared-redis-split-by-purpose.md` §4.4 引用的 `k8s_gate_drain.ps1:630-640` 行号与 exec 方式都已变化，只在 §13 第 13 条的注里提了一句。

### 5.2 已知限制与代价（写进 README 的，提醒接手者别当成缺陷再修一遍）

- 切到高可用之后，必须对每个 zone 重跑 zone-up，再 `rollout restart deploy/db` 与 `deploy/data-service`；只改 ConfigMap 不会滚动它们。
- 三个 Pod 中同时坏两个时，写入被拒（`min-replicas-to-write 1`）。切主后几秒内 WAITAOF 确认会失败：go/db 重投、data-service 重试；这期间刚启动的 go/db 会被启动探针拒启。
- `Resolve-RedisTopologyFromLive` 不识别 2026-09-19 之前的旧 Deployment redis。评审判定这类集群不存在：项目尚未上线，没有存量部署。这类集群必须先跑 infra-up 迁移（legacy-deployment 分支会要求确认）。
- 只有不传 `-GoSvcRegistry` 的入口（如 `k8s_image.ps1 release-zone`）不查线上，因为它不渲染 WAITAOF。

## 6. 关键位置索引（按名字 grep，行号会漂移）

- `tools/scripts/k8s_deploy.ps1`：
  - 参数 `-RedisReplicas`；
  - 拓扑相关：`Get-RedisTopologyFor` / `Get-RedisTopology`（`$script:RedisReplicasFollowedLive`）/ `Get-MappingRedisTopology(For)` / `Get-LiveRedisOrphanPodOrdinals` / `Get-RedisReplicasFromOrphanOrdinals` / `Get-LiveRedisReplicas` / `Format-RedisLiveShape` / `Get-RedisLiveShapeManualFix` / `Get-RedisTopologyChangeVerdict` / `Assert-RedisTopologyChangeIsSafe` / `Get-RedisZoneTopologyDecision` / `Resolve-RedisTopologyFromLive`；
  - 高可用前置与核对：`Assert-RedisPasswordSentinelSafe` / `Get-RedisHaLimits` / `Get-RedisHaScriptsDigest` / `Test-RedisLiveMaxClients -Nodes`；
  - 重启闸：`Get-RedisCoreRestartVerdict`（`orphan-pods`）/ `Get-LiveRedisCoreTemplateDigest`（`OrphanPods`）/ `Assert-RedisCoreRestartConfirmed`（`-RollingHandover`）；
  - 其余：`Write-RedisConnectionLedger`（代理总账段）、`Apply-Infra` 的 Redis 分支、db 与 data-service ConfigMap 的 WaitAOF 渲染、写路径门禁块里的 `Resolve-RedisTopologyFromLive`、启动行 `Redis topology:`。
- `tools/scripts/k8s_gate_drain.ps1`：`Get-GateDrainRedisExecTarget`、`Get-GateDrainRedisFailureHint`、`Invoke-GateDrainRedis`。
- 运维说明：`deploy/k8s/README.md`「共享 Redis:高可用形态」；总纲：`docs/design/no-single-node-horizontal-scaling-20261001.md` §5。

## 7. 本会话更早的工作（已入库，只留指针）

登录取逻辑服 / 物理服的瓶颈分析，以及随后几批实现，都已经由负责人的快照提交入库：
- gate 目录 list-watch 缓存；
- 共享的 p2c 选 gate（`go/shared/gatepick`）；
- scene_manager 排空过滤；
- 账号自愈加锁 + CAS；
- 运维原子摘孤儿；
- DataServiceRpc 打开熔断；
- 排空脚本的镜像沉淀等待。

结论与上线前复核步骤见 `docs/design/player-route-mapping-durability-proposal.md` §12.1 / §12.2（登记为 `tidb-phase1-and-launch-readiness.md` §7 L-18、`release-checklist.md` A.1）。各批的验证状态以 `docs/PROGRESS.md` 对应条目为准。**开服 / 上线相关的会话要主动按 §12.2 复核，不沿用 10-18 的推算结论。**

## 8. 双版本（AGENTS §12）

不适用。这是纯部署形态，Java 版不共用本仓的 K8s 清单；对应 Java 版本号 0.1.0-SNAPSHOT。`PARITY.md` 待登记，见 §5 第 4 条。
