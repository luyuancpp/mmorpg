# 玩家存储落点:当前 HEAD 复验清单(交 Codex)

> **日期**:2026-10-12
> **基线**:HEAD `73b852f32`(main)
> **编写**:Claude。按 AGENTS.md §10.1,本清单里**一条命令都没跑过**,所有「通过标准」都是期望值,不是已有结果。命令、用例名、用例数、环境变量名都对着代码与 git 核过(只读)。
> **执行**:Codex,在有完整工具链的机器上执行。本检出 `D:\luyuan\xuanming-server-mmo-cpp` 跑不了:PATH 上没有 go / gofmt / pwsh 7 / gcc / MSBuild,`third_party/grpc` 子模块未初始化,Docker 守护进程没在跑。
> **关联**:[player-storage-placement.md](../design/player-storage-placement.md)(落点设计,v2.4);[merge-zone-runbook.md](../ops/merge-zone-runbook.md);PROGRESS.md「2026-09-28 玩家存储落点」条目(09-29 第一轮验证的清单与结果,本清单在它的基础上更新);[no-single-node-horizontal-scaling-20261001.md](../design/no-single-node-horizontal-scaling-20261001.md) §3.5(go/db 多实例的验证要求)。

## 0. 为什么要复验

落点方案随 `4493b3c9f`(2026-09-29)进 main。同日按用户指示由 Claude 跑过一轮(Go 全过、merge_zone 集成 83 条 PASS、`cross_zone_test` 105/105),那次的运行证据只覆盖当时那棵树。之后落点代码本身没再改,下列文件自 `4493b3c9f` 起 `git log` 为空:

- `go/shared/placement`;
- `go/data_service/internal/routing`;
- `go/scene_manager/internal/logic/home_zone.go`;
- login 的 `Placement` 配置与测试;
- C++ 的 `GetDbTaskTopic` 及其调用点。

但它的宿主进程和邻接代码被下面这些提交改过,其中多数写明「未编译、未测试」:

| 提交 | 模块 | 改了什么、和落点的关系 | 之后的运行证据 |
|---|---|---|---|
| `be6b4ab1d`(10-08) | go/db | 重试收据的 processing 列表按实例拆分(`kafka:retry:processing:{topic}:{实例名}` + 租约 ZSET `kafka:retry:instances:{topic}` + 认领 Lua,新增 `retry_ownership.go`)。落点的 `frozen_deferred` / `store_unavailable` / `recheck_moved` 都经 `deferFailedTask` 进这条重试通路;停机释放 ordering lock(relocate R2 等的就是这把锁)改用脱离取消的 context。新配置 `Kafka.RetryInstanceId` / `RetryLeaseSeconds` | **无**。10-08 只有一次构建了 db.exe(PROGRESS 约 6785 行,记录在 `d3c2fca3c`),那棵树的 go/db 与 `4493b3c9f` 完全相同;含 `be6b4ab1d` 的 go/db 从没编出过 exe |
| `be6b4ab1d` | tools/merge_zone | P4 门禁与 `kafka_db_task_queues` 审计改为先 ZRANGE 登记表,再逐实例 LLEN | **无** |
| `160e7bf4f`(10-01) | go/shared/kafkautil | `EnsureTopics` 新增副本数契约(环境变量 `KAFKA_TOPIC_REPLICATION_FACTOR`,已有 topic 的 RF 不够时 go/db 拒启) | **无** |
| `1bc9d4899`(10-08) | go/data_service | 迁移取 `GET_LOCK`;`Store.Required` 时 store 装配失败即拒启(会让 `RegisterPlayerZone` 不可用,建角 fail-closed) | **无**。10-08 Codex 的 data_service 回归(64 个顶层用例)基线是 `3399732ae`,包含 `4b8ff21d4` 但不含本提交 |
| `2a203a3a4`(10-09) | go/scene_manager、tools/merge_zone | EnterScene 加切线预检 2b(在 3b 归属查询之前,3b 相对次序不变);merge_zone 热状态清理多扫两个前缀 | scene_manager 切线定向 106 项(Codex 10-09,不含 HomeZoneMerging 回归);merge_zone 无 |
| `c6ba557e4` 等(GO-2) | go/scene_manager | owner_epoch 回滚只进不退,改的是第 7 步与跨区重定向分支 | 无全量记录 |
| `e7d9c9611`(09-29) | go/login | 新增 `GateDrain` / `RequireClientEndpoint` 配置。`TestEtcYamlPlacementBlockIsOff` 加载完整 `login.yaml`,所以会连带依赖这两段 | 无 |
| `10cca6b66`(10-09) | proto / C++ | 装备属性:只改了 proto 源与 xlsx,**没有导表、没有 proto-gen**,`table.vcxproj` 登记了 28 个磁盘上不存在的文件。`cross_zone_test.cpp` 已引用 `ItemEntry.equip` | C++ 编不过(Codex 10-10:scene 库 110 errors) |

结论:Go 侧与部署契约可以直接复验;C++ 侧被与落点无关的生成物缺口挡住(第 13 步,是门禁)。

## 1. 执行规则

1. **按步骤顺序执行**;每步失败保留日志与摘要,不连续重试,不改代码,不改 `go.mod` / `go.sum`(命令要求改时停下报告)。
2. **SKIP 不算通过**。merge_zone 集成测试遇到 SKIP 会 `os.Exit(0)`,data_service 集成测试在 Redis 不可达或有残留键时 `t.Skip`,两者表面上都是 `ok`。每步通过标准都带「无 SKIP」与用例计数。
3. **不污染共享数据**:严禁对共享 Redis 执行 `FLUSHALL` / `FLUSHDB`。merge_zone 集成测试只用 DB 9 / 10 / 11;data_service 路由集成测试必须指向一次性空 Redis。拉取 Docker 镜像属于下载,先征得用户同意。
4. **Windows Application Control**(CodeIntegrity 3077 / 3033)拦截测试 exe 时,停下记录事件,不绕过策略,按环境问题报告。
5. **失败归因**:落点代码自 `4493b3c9f` 未改,所以 HEAD 上的失败先在 `4493b3c9f` 上复跑同名用例来判断。用 `git worktree add $env:TEMP\placement_base 4493b3c9f` 建基线,基线也失败的是既有问题;只在 HEAD 上失败的,对照第 0 节的表格归到对应提交。
6. 全程进程级环境变量,不 `go env -w`:

```powershell
$env:GOTOOLCHAIN = 'local'
$env:GOFLAGS     = '-mod=readonly'
$env:GOPROXY     = 'https://goproxy.cn,direct'
Remove-Item Env:KAFKA_TOPIC_REPLICATION_FACTOR, Env:KAFKA_INTEGRATION_BROKER, Env:MMORPG_RELEASE_VERSION -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $env:TEMP\placement-reverify | Out-Null
```

## 2. 步骤

### 第 0 步:基线与工具链探针

工作目录:仓库根。

```powershell
git rev-parse HEAD
git diff --stat 73b852f32 HEAD -- . ':!docs' ':!AGENTS.md'
git status --porcelain=v1 | Tee-Object $env:TEMP\placement-reverify\status-before.txt
git submodule status
go version
go env GOVERSION GOROOT GOMODCACHE GOPROXY GOTOOLCHAIN GOFLAGS CGO_ENABLED CC
pwsh -NoProfile -Command '$PSVersionTable.PSVersion.ToString()'
Get-Command gcc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
docker version --format '{{.Server.Version}}'
```

通过标准:

- HEAD 是 `73b852f32`,或在它之上只多了文档提交(第二条输出为空);
- `go/ cpp/ proto/ data/ generated/ tools/ robot/ deploy/` 下没有未提交改动;只有 `docs/`、`AGENTS.md` 的改动时可以继续;
- `go version` ≥ go1.26.5(go/db、go/data_service 的 go.mod 写的是 1.26.5);pwsh 7.x;
- gcc 有没有决定第 10 步走哪个方案;docker 有没有决定第 4、8 步的集成测试能不能跑。缺工具就只跑不依赖它的步骤,缺项写进报告,不临时安装。

### 第 1 步:生成物一致性(只读,决定要不要重新生成)

工作目录:仓库根。

```powershell
git grep -n DataService_GetPlayerAssetOpLedger_FullMethodName -- go/proto/data_service/data_service_grpc.pb.go
git grep -c -e 'GetStorageId()' -e 'GetHomeZoneMerging()' -- go/proto/data_service/data_service.pb.go
git grep -n 'GetDbTaskTopicGeneration()' -- go/proto/common/base/config.pb.go
git grep -n -e 'GetClientChannelPick()' -e 'GetOwnerEpochAfterRollback()' -- go/proto/scene_manager/scene_manager_service.pb.go
git grep -n 'db_task_topic_generation() const' -- cpp/generated/proto/common/base/config.pb.h
git log --oneline 4493b3c9f..HEAD -- go/shared/go.mod go/shared/go.sum go/data_service/go.mod go/data_service/go.sum go/scene_manager/go.mod go/scene_manager/go.sum go/login/go.mod go/login/go.sum go/db/go.mod go/db/go.sum tools/merge_zone/go.mod tools/merge_zone/go.sum
```

通过标准:

- 前 5 条全部命中;
- 最后一条输出为空。

满足就判定 **Go 侧复验不需要先跑 `go/build.bat`、proto-gen 或导表器**。不要跑 `go/build.bat`:它第 3 行是 `pause`,非交互会挂住,而且只重生成 goctl 包装,不产出 pb。

### 第 2 步:go/shared(placement + kafkautil)

工作目录:`go\shared`。

```powershell
gofmt -l ./placement ./kafkautil
go vet ./placement/... ./kafkautil/...
go test -count=1 -v ./placement/... ./kafkautil/... 2>&1 | Tee-Object $env:TEMP\placement-reverify\shared.log
```

通过标准:

- gofmt 无输出,vet 退出码 0,两个包都 `ok`。
- placement 的 9 个用例 PASS:`TestParseValidVectors`、`TestParseMalformedVectors`、`TestKeys`、`TestStoreDBNameRoundTrip`、`TestStorageIDFromDBNameRejectsOutsideFamilies`、`TestValidateRejectsStableWithRunID`、`TestSameRouteIgnoresFrozen`、`TestParseHomeZone`、`TestEffectiveStorage`。
- kafkautil(`160e7bf4f`,从未跑过)的 `TestEnsureTopics*`、`TestParseReplicationFactor`、`TestPartitionContractMarkerIsStableAndUnambiguous` 全部 PASS;`TestEnsureTopicsImmutablePartitionContractIntegration` 显示 SKIP 是预期(需要 `KAFKA_INTEGRATION_BROKER`)。

### 第 3 步:go/db(落点宿主进程,`be6b4ab1d` 改过,最要紧)

工作目录:`go\db`。

```powershell
gofmt -l db.go cmd/migrate internal/config internal/kafka internal/logic/pkg/proto_sql internal/metrics internal/svc
go vet ./...
go build ./...
go build ./cmd/migrate ./cmd/verifier ./cmd/data_stress
```

通过标准:

- gofmt 无输出;vet、两条 build 退出码 0。
- `gofmt -l .` 若列出 `internal/locker/locker.go`,这是 09-29 就记录的既有问题。
- 若列出 `cmd/verifier`、`cmd/data_stress` 下的文件:这两个目录的 gofmt 状态从来没有记录过,而 `cmd/` 自 `4493b3c9f` 起没改动。先在基线 worktree 上复核,确认是既有问题,不要归到 `be6b4ab1d`。

**3a. 落点定向(`placement_route_test.go` 全部 22 个顶层用例)**

```powershell
go test -count=1 -v ./internal/kafka/ -run '^(TestDecideWritePlacement_EveryCellOfTheTable|TestDecideReadPlacement_EveryCellOfTheRules|TestParsePlacementSnapshot|TestRecheckPasses_Table|TestHandleTask_WritePlacementCells|TestHandleTask_PlacementRedisDownIsALookupError|TestHandleTask_MissingPlacementRouterFailsClosed|TestHandleTask_ReadPlacementCells|TestHandleTask_ReadMissingRequiredAnswersWithFailure|TestHandleTask_ReadLookupErrorIsRetried|TestFrozenDeferral_NeverExhaustsTheRetryBudget|TestPlacementLookupError_ConsumesTheRetryBudget|TestStoreUnavailable_WriteKeepsBudgetReadSpendsIt|TestPostWriteRecheck|TestPostWriteRecheck_NewerWriteWinsOverTheReplay|TestPostReadRecheck|TestCoalescedBatch_PlacementGuardsTheSurvivor|TestNewKeyOrderedKafkaConsumer_RequiresPlacementDependencies|TestMarkPlacementCapability|TestPlacementCapabilityHeartbeat|TestPlacementCapabilityHeartbeat_StopsWhenTicksClose|TestRetryReschedule_FrozenLogsDebugStoreDownLogsError)$' 2>&1 | Tee-Object $env:TEMP\placement-reverify\godb_placement.log
(Select-String -Path $env:TEMP\placement-reverify\godb_placement.log -Pattern '^--- PASS: Test').Count
go test -count=5 ./internal/kafka/ -run '^(TestPlacementCapabilityHeartbeat|TestPlacementCapabilityHeartbeat_StopsWhenTicksClose|TestRetryReschedule_FrozenLogsDebugStoreDownLogsError)$'
```

通过标准:

- `ok`,顶层 PASS 计数 **22**,无 FAIL / SKIP;`-count=5` 那条 5 次全过。
- 重点看 `TestFrozenDeferral_NeverExhaustsTheRetryBudget`(冻结 5 轮 RetryCount 停在 2,解冻后落到 `player_store_1000000_db`)和 `TestStoreUnavailable_WriteKeepsBudgetReadSpendsIt`。
- 注意:这些用例用的是 `retryOwner=nil` 的旧共享键,验证的是延后与预算豁免本身,**不经过** `be6b4ab1d` 的按实例认领(覆盖缺口见第 3 节)。

**3b. 重试归属定向(`be6b4ab1d` 新增,从未跑过)**

```powershell
go test -count=1 -v ./internal/kafka/ -run '^(TestRetryOwnership_.*|TestResolveRetryInstanceID|TestRetryLeaseDuration|TestRetryConsumer_.*)$' 2>&1 | Tee-Object $env:TEMP\placement-reverify\godb_retry_owner.log
(Select-String -Path $env:TEMP\placement-reverify\godb_retry_owner.log -Pattern '^--- PASS: Test').Count
go test -count=5 ./internal/kafka/ -run '^(TestRetryOwnership_RunRenewsLeaseAndReclaimsPerTick|TestRetryOwnership_ReclaimedButAliveInstanceReRegistersOnClaim|TestRetryConsumer_ClaimsThroughOwnershipIntoInstanceList)$'
```

通过标准:顶层 PASS 计数 **16**(`retry_ownership_test.go` 15 个,加 `key_ordered_consumer_test.go` 的 `TestRetryConsumer_LegacyPayloadWithoutPartitionGoesToDeadQueue`);`-count=5` 5 次全过。

另外,`TestRecoverRetryProcessingRestoresEveryReceipt` 已被 `be6b4ab1d` 删除,和 09-29 的结果对比时不要把它当成缺失。这里失败就是 `be6b4ab1d` 的问题,不是落点引入的。

**3c. go/db 全量**

```powershell
go test -count=1 ./... 2>&1 | Tee-Object $env:TEMP\placement-reverify\godb_all.log
```

通过标准:

- 退出码 0;`cmd/migrate`、`internal/config`、`internal/dbguard`、`internal/kafka`、`internal/logic/pkg/proto_sql`、`internal/migrate`、`internal/stresstest` 七个包都是 `ok`。
- 不加 `-short`,所以 `TestConcurrentWorkers_FinalStateIsGlobalMax` 会真跑。
- 全部只用 miniredis 与内存驱动,不需要真 Redis / MySQL / Kafka;miniredis 要能监听 127.0.0.1。

### 第 4 步:go/data_service

工作目录:`go\data_service`。

```powershell
gofmt -l ./internal/routing ./internal/server
go build ./...
go vet ./...
go test ./internal/routing/... ./internal/server/... -count=1 -v -run "TestKeyContractsMatchSharedPlacement|TestRegisterPlayerZone_|TestRegisterAndGetPlayerZone|TestIsMergeInProgress|TestGetPlayerHomeZone|TestDecodeHomeZoneAndMergeFenceReply|TestBatchGetPlayerHomeZone|TestRemapHomeZoneForMerge_" 2>&1 | Tee-Object $env:TEMP\placement-reverify\ds_unit.log
go test ./... -count=1
```

通过标准:

- gofmt 无输出;build、vet 退出码 0。
- routing 的 `TestRegisterPlayerZone_*` 10 个、`TestGetPlayerHomeZoneAndMergeFence*`、`TestDecodeHomeZoneAndMergeFenceReply`、`TestKeyContractsMatchSharedPlacement` 全部 PASS。
- server 的 `TestGetPlayerHomeZone_ReportsMergeFence`、`TestRegisterPlayerZone_StorageIdPinsPlacementOnlyOnFirstWrite`、`TestRegisterPlayerZone_RefusedWhileZoneIsMerging` PASS。
- 全量全绿。全量若在 `internal/store`(GET_LOCK)、`internal/svc`(MissingStores)、`internal/config`(Store.Required)、`internal/logic`(回档 / 账本)变红,归 `1bc9d4899` / `4b8ff21d4` 等,不是落点;报告时逐包写明。

**4b. 路由 Redis 集成测试(必须用一次性空 Redis)**

```powershell
docker run -d --rm --name ds-it-redis -p 127.0.0.1:6390:6379 redis:7.2   # 本地没有该镜像时需拉取,先征得同意
$env:DATA_SERVICE_IT_REDIS_HOST = '127.0.0.1:6390'
go vet -tags=integration ./internal/routing/...
go test -tags=integration ./internal/routing/... -count=1 -v 2>&1 | Tee-Object $env:TEMP\placement-reverify\ds_routing_it.log
(Select-String -Path $env:TEMP\placement-reverify\ds_routing_it.log -Pattern '--- SKIP').Count
docker rm -f ds-it-redis
Remove-Item Env:DATA_SERVICE_IT_REDIS_HOST
```

通过标准:

- 退出码 0,共 **31** 个顶层用例(单测 26 + 集成 5)全 PASS,SKIP 计数 0。
- 5 个集成用例:`TestMappingRedisEffectiveDBIsZero`、`TestRegisterPlayerZone_RealRedisNeverOverwrites`、`TestMergeFence_RealRedisBlocksRegisterAndGatesRemap`、`TestPlacementScripts_RealRedis`、`TestGetPlayerHomeZone_RealRedisMissingIsTyped`。
- **禁止**不设 `DATA_SERVICE_IT_REDIS_HOST` 就跑:那样会回落到 `etc/data_service.yaml` 的 127.0.0.1:6379。它的清理会删掉 DB0 里所有 `player:zone:*`,而空库检查只扫一页 SCAN。

**4c.(可选)store 集成测试(覆盖 `1bc9d4899` 的 GET_LOCK)**:需要 `DATA_SERVICE_IT_MYSQL_HOST` / `_USER` / `_PASSWORD`(口令取 `deploy/docker-compose.yml` 里 mysql 服务的 root 口令,不写进日志)。

```powershell
go test -tags=integration ./internal/store/... -count=1 -v -run 'TestMigrateSchema_ConcurrentFirstBootIsSerialized|TestMigrateSchema_LockBusyReturnsSentinel'
```

既有失败:`TestMigrateSchema_UpgradesLegacyHandWrittenTables` 从 09-29 起就红(`transaction_log` 列默认值与 proto 不符),不是落点问题,不要把它算进本次结论。

**4 附录:2026-10-12 第 0 批 / 1a 批后的期望(追加;上面的原有段落不改)**

映射持久化的第 0 批、第 1a 批([player-route-mapping-durability-proposal.md](../design/player-route-mapping-durability-proposal.md) §11.2)在工作区落码、未提交,改了 `go/data_service/internal/routing`、`go/shared/placement` 等。本清单上面的计数是按 HEAD `73b852f32` 写的。

- **在干净的 HEAD worktree 上复验**:上面的计数照旧。
- **在含第 0 批 / 1a 批改动的检出上复验**(提案 §11.2 的验证顺序就是这样):按下面的期望替换。第 1 节第 5 条「落点代码自 `4493b3c9f` 未改」对 `internal/routing` 与 `go/shared/placement` 不再成立:这两处只在当前检出上失败、在基线 worktree 上不失败的,先归到第 0 批 / 第 1a 批。

第 4 步:

- routing 单测顶层用例从 26 变为 **54**:HEAD 26,第 0 批 +3(`TestBatchGetPlayerHomeZone_MalformedValueFailsTheBatch`、`TestGetAllPlayerIDsInZone`、`TestGetAllPlayerIDsInZone_GetErrorFailsTheScan`),删 remap −3(`TestRemapHomeZoneForMerge_DryRun` / `_Apply` / `_RefusedWithoutFence`),`route_publish_test.go` +28。用 `go test ./internal/routing/... -list .` 核数(54 行用例 + 1 行 ok)。
- `-run "TestRegisterPlayerZone_"` 仍然恰好命中 routing 的 **10** 个:新用例一律用 `TestDBFirstRegister_*`、`TestPublishRoute*`、`TestPublishRouteScript_*` 等前缀,不会被选中,也不会打乱上面「`TestRegisterPlayerZone_*` 10 个」的计数。
- `TestRemapHomeZoneForMerge_` 现在只匹配 server 的 `TestRemapHomeZoneForMerge_AlwaysFailedPrecondition`:RPC 已停用,四种情况(token 未配、token 错、token 对、token 对且有围栏)都回 FailedPrecondition + `ErrCodeNotImplemented`、Redis 零变更,**预期 PASS**。原来 server 的 5 个 remap 用例与 routing 的 3 个都已删除。
- server 的 `TestGetPlayerHomeZone_ReportsMergeFence`、`TestRegisterPlayerZone_StorageIdPinsPlacementOnlyOnFirstWrite`、`TestRegisterPlayerZone_RefusedWhileZoneIsMerging` 仍在,照旧要求 PASS。
- 全量 `go test ./...` 多出第 1a 批的默认单测:store 的假驱动契约测试与不连库单测、config、metrics、logic、server、`internal/routetool`、`cmd/route_tool`。它们都不需要外部依赖;红了归第 1a 批。

4b:

- 带 integration tag 时顶层用例共 **63** 个(单测 54 + 集成 9)。
- `TestMergeFence_RealRedisBlocksRegisterAndGatesRemap` 改名为 **`TestMergeFence_RealRedisBlocksRegister`**(去掉了 remap 那几步)。
- 新增 4 个集成用例:`TestPublishRoute_RealRedisSameConnection`、`TestPublishRoute_RealRedisWaitAOF`、`TestPublishRouteScript_RealRedisParity`、`TestDBFirstRegister_RealStores`。
- **一次性 Redis 要开 AOF**:`docker run -d --rm --name ds-it-redis -p 127.0.0.1:6390:6379 redis:7.2 redis-server --appendonly yes`。不开的话 `TestPublishRoute_RealRedisWaitAOF` 会因 appendonly 关着而 SKIP,而它与 `TestPublishRoute_RealRedisSameConnection` 都必须 PASS(提案 §11.2 验证第 4 步)。
- 只配 Redis、没配 `DATA_SERVICE_IT_MYSQL_*` 时,允许 **`TestDBFirstRegister_RealStores` 一个 SKIP**(它要求两边都配);两边都配了就必须 PASS。其余 SKIP 计数仍须为 0。
- `itKeyPatterns` 多了 `player:maprev:*`:空库检查与清理也覆盖这族键。

4c:两个 `TestMigrateSchema_*` 用例不变。迁移现在多建四张路由表、种 `route_meta(id=1)`;失败时先看是不是路由表那段(形状守卫、meta 种行)。

其它步骤受影响的计数:

- 第 2 步:`go/shared/placement` 顶层用例从 9 变为 **15**,新增 `route_test.go` 的 6 个(`TestRouteKeys`、`TestRevVectors`、`TestCompareRevLengthBeforeLexicographic`、`TestClassifyPublishReply`、`TestPublishOutcomeStringsAreStableLowCardinality`、`TestRouteFenceShard`);gofmt / vet 同时覆盖新文件 `route.go`。
- 第 9 步:`k8s_deploy_contract` 从 87 变为 **88**,新增「data-service ConfigMap 显式写 RouteStore.Mode: redis(dev / prod 两档),服务 yaml 也是 redis」,必须 PASS。

本清单跑完之后,按提案 §11.2「第 0 批交 Codex 的验证」与「第 1a 批交 Codex 的验证」继续。

**4 附录补充:第 1a 批热点修复(G1a)之后的期望(2026-10-12,追加;上面的段落不改)**

G1a(提案 §11.2「第 1a 批热点修复」)同样在工作区落码、未提交,改的是第 1a 批的路由表与 route_tool。在含 G1a 的检出上复验时,上面附录的期望按下面更正;只在当前检出上失败、在基线上不失败的,先归到第 0 批 / 第 1a 批 / G1a 之一(G1a 改动的文件见提案该段的改动清单)。

- 第 2 步:`go/shared/placement` 顶层用例从 15 变为 **16**,新增 `TestRouteRowShard`(`RouteRowShards == 16` 与取模向量)。
- 第 4 步:routing 顶层用例 **54 / 63 不变**(G1a 不碰 routing)。全量 `go test ./...` 多出 G1a 的不连库单测(store 的 DDL 契约 / 游标序 / 语句生成、假驱动的行片 / 导入键序 / 错片行 / 探针 P4 / `PrepareTiDB`;`internal/routetool` 的跨片翻页与 `tidb-prepare` 输出;`cmd/route_tool` 的参数与「tidb-prepare 不连 Redis」),红了归 G1a。
- 4c:迁移现在建**五**张路由表(多 `route_reconcile`)、种两行(`route_meta`、`route_reconcile` 各 id=1);`player_route` 主键是 `(route_shard, player_id)`。本机 `testdb` 若跑过 G1a 之前的 1a 迁移,会被形状守卫拒绝(报错里带 G1a 提示),处理见提案 G1a 段 HR4:确认无数据后 DROP 全部路由表再 `-migrate`,不确定时先问负责人。另:route store 建池多了探针 P4(`chk_route_shard` 实测),所有建 route store 的真库用例要求 MySQL ≥ 8.0.16。
- 第 9 步:`k8s_deploy_contract` 仍 **88**、`k8s_zone_rollback_flush_guard` 仍 **8**(G1a 不碰这两份脚本)。

### 第 5 步:go/scene_manager

工作目录:`go\scene_manager`。

```powershell
gofmt -l ./internal
go build ./...
go vet ./internal/...
go test ./internal/logic/ -run "HomeZoneMerging" -count=1 -v
go test ./internal/logic/ -run "TestChannelSwitchStartsCooldown|TestEnterScene_HomeZone|TestEnterScene_TravelSecondLegRejectsUnmappedHomeZone" -count=1 -v
go test ./internal/logic/ -run "HomeZoneMerging|HomeZone|ChannelPick|ChannelSwitch|Handoff" -count=1 -v
go test ./internal/logic/ -run "EnterScene|HomeZone|Handoff|Reseed|OwnerEpoch|ChannelPick|ChannelSwitch" -count=1
go test ./internal/logic/ -count=1 -run "TestChannelErrorCodes|TestChannelPick|TestChannelSwitchConfig|TestChannelSwitchStartsCooldown|TestConfig|TestEnterSceneFingerprint|TestIsChannelSwitch|TestLocateWorldChannel|TestOrphanCleanup|TestParseWorldChannelLineNumbers|TestPublishWorldChannelDirectory|TestWorldChannelDirectoryKeyFmt|TestWorldChannelDirectoryTTLSeconds|TestWorldChannelLineNumbers|TestWorldChannelState"
go test ./... -count=1
```

倒数第二条是 [world-channel-switch.md](../design/world-channel-switch.md) 第 1 步列的 15 组正则,覆盖 `2a203a3a4`。scene_manager 自 `da392ef89`(Codex 10-09 跑切线 106 项时的基线)以来只改了注释和少量测试行,所以那次的证据基本覆盖 HEAD,但那次不含 HomeZoneMerging。

通过标准:

- build、vet 退出码 0。
- HomeZoneMerging 的 5 个用例全 PASS:`TestEnterScene_HomeZoneMergingGatesSameZoneLanding`、`…RejectsBeforeSameNodeEpochReseed`、`…RejectsTravelFirstLegWithoutSideEffects`、`…RejectsTravelSecondLeg`、`…IgnoredWhenUnmapped`。
- `TestChannelSwitchStartsCooldown` PASS,证明合服拒绝码 21 不起算切线冷却;`TestEnterScene_HomeZoneNotQueriedWithoutGateRoute` PASS。
- 回归正则与全量都绿。owner_epoch / 切线类用例红而 HomeZoneMerging 绿的,归 GO-2 或 `2a203a3a4`。
- `owner_epoch_crosslang_test` 会读 C++ 头文件里的 Lua 字面量,必须在完整检出里跑。

### 第 6 步:go/login

工作目录:`go\login`。

```powershell
gofmt -l ./internal/config
go vet ./internal/config/... ./internal/logic/pkg/homezone/... ./internal/logic/clientplayerlogin/... ./internal/svc/...
go test ./internal/config/... -run "Placement|EtcYaml|LoginYaml" -count=1 -v
go test ./internal/logic/pkg/homezone/... -run TestRegisterPlayerZone -count=1 -v
go test ./internal/logic/clientplayerlogin/... -run "RegisterHomeZone|CreatePlayer" -count=1 -v
go build ./...
go test ./... -count=1
```

通过标准:

- 以下用例全部 PASS:
  - `TestPlacementStorageIDForNewPlayer`、`TestPlacementBlockMissingMeansOff`;
  - `TestLoginYamlSpellsOutPlacementKeys`(2 个子测试);
  - `TestPlacementKeysProbeRejectsTypos`(4 反例 + 正例);
  - `TestEtcYamlPlacementBlockIsOff`;
  - `TestRegisterPlayerZone`;
  - `TestRegisterHomeZonePlacementStorageID`、`TestRegisterHomeZone`;
  - `TestCreatePlayer_RegistersHomeZoneThenPersists` / `_HomeZoneRegistrationFailsClosed` / `_ReleasesLockOnHomeZoneFailure`。
- build 与全量退出码 0。
- `TestEtcYamlPlacementBlockIsOff` 若报 `conf.Load` 失败,先看报错是不是出在 `GateDrain` / `RequireClientEndpoint` 段;是的话归 `e7d9c9611`。

### 第 7 步:go/test.ps1 L1

工作目录:`go`。

```powershell
pwsh -NoProfile -File test.ps1
```

通过标准:依次打印 `>>> go test L1 [db] (...)` 与 `>>> go test L1 [shared] (./placement/...)`,最后一行 `L1_TESTS_OK`,退出码 0。

### 第 8 步:tools/merge_zone

工作目录:`tools\merge_zone`(独立 go module)。

**8a. 静态检查与单测(不连任何外部服务)**

```powershell
gofmt -l .
go vet ./...
go vet -tags merge_integration ./...
go build ./...
go test -count=1 -v ./... 2>&1 | Tee-Object $env:TEMP\placement-reverify\mz_unit.log
(Select-String -Path $env:TEMP\placement-reverify\mz_unit.log -Pattern '^--- PASS: Test').Count
(Select-String -Path $env:TEMP\placement-reverify\mz_unit.log -Pattern '--- FAIL|^FAIL|panic:').Count
go test -count=1 -v -run '^(TestDbTaskTopicAndQueueKeys|TestDbTaskQueueKeysCoverEveryInstance|TestSourceZonePatternsAndFixedKeys|TestSceneHotStateKeys)$' .
git status --porcelain -- .
```

通过标准:

- gofmt 无输出,两条 vet 与 build 退出码 0。
- 顶层 PASS **158** 个(09-29 是 157,`be6b4ab1d` 新增 `TestDbTaskQueueKeysCoverEveryInstance`),FAIL / panic 为 0。
- 定向 4 个用例 PASS,其中后两个对应 `2a203a3a4`。
- 跑完 `git status` 无输出(`merge_zone.exe` 被 `.gitignore` 忽略)。

**8b. 落点编解码测试向量与重试键名**(工作目录:仓库根)

```powershell
& 'C:\Program Files\Git\bin\bash.exe' -c "diff <(sed -n '/^var codecVectors/,/^}/p;/^var malformedVectors/,/^}/p' go/shared/placement/placement_test.go) <(sed -n '/^var codecVectors/,/^}/p;/^var malformedVectors/,/^}/p' tools/merge_zone/placement_codec_test.go) && echo VECTORS_IDENTICAL"
git grep -n -e '"kafka:retry:' -e '"kafka:dead:' -e 'kafka:retry:queue:%s' -e 'kafka:dead:queue:%s' -- go/db/internal/kafka/retry_ownership.go go/db/internal/kafka/key_ordered_consumer.go tools/merge_zone/preflight.go
cd go\db; go test -count=1 -v -run '^TestRetryOwnership_KeyNames$' ./internal/kafka/; cd ..\..
```

通过标准:

- 打印 `VECTORS_IDENTICAL`。必须写 Git Bash 全路径:在 PowerShell 里直接敲 `bash` 会解析成 WSL 的。
- go/db 与 merge_zone 两边的 `kafka:retry:queue:` / `kafka:retry:processing:` / `kafka:retry:instances:` / `kafka:dead:queue:` 前缀逐一对得上。
- `TestRetryOwnership_KeyNames` PASS,它的字面量与 `merge_unit_test.go` 的 `TestDbTaskTopicAndQueueKeys` 相同。

**8c. 集成测试(真 MySQL + 真 Redis + 真二进制)**

前置:地址写死在 `integration_test.go`,环境变量改不了:

- MySQL root@127.0.0.1:3306(≥ 8.0,要有 CREATE / DROP DATABASE、TRIGGER 与 `performance_schema` 读权限);
- Redis 127.0.0.1:6379 的 DB 9 / 10 / 11 必须为空。

测试开始和结束都会 DROP 7 个一次性库(`zone_901_db`、`zone_902_db`、`player_store_1000901_db`、`merge_zone_it_db` / `_guild` / `_trade` / `_friend`),并 FlushDB 9 / 10 / 11。

```powershell
docker ps --format '{{.Names}} {{.Image}} {{.Ports}}'      # 确认 3306 / 6379 上是 compose 的 mysql / redis 容器,不是别人的实例
docker exec redis redis-cli -n 9 DBSIZE; docker exec redis redis-cli -n 10 DBSIZE; docker exec redis redis-cli -n 11 DBSIZE
go test -tags merge_integration -count=1 -v ./... 2>&1 | Tee-Object $env:TEMP\placement-reverify\merge_zone_it.log; "exit=$LASTEXITCODE"
(Select-String -Path $env:TEMP\placement-reverify\merge_zone_it.log -Pattern 'SKIP:|FATAL:').Count
(Select-String -Path $env:TEMP\placement-reverify\merge_zone_it.log -Pattern '^--- PASS: TestIT_').Count
(Select-String -Path $env:TEMP\placement-reverify\merge_zone_it.log -Pattern '^\s+--- PASS: TestIT_').Count
(Select-String -Path $env:TEMP\placement-reverify\merge_zone_it.log -Pattern '--- FAIL|^FAIL|panic:').Count
Select-String -Path $env:TEMP\placement-reverify\merge_zone_it.log -Pattern 'preflight P4 OK: .*\(3 keys incl\. per-instance processing lists\)'
docker exec redis redis-cli -n 9 DBSIZE; docker exec redis redis-cli -n 10 DBSIZE; docker exec redis redis-cli -n 11 DBSIZE
```

通过标准:

- exit=0;`SKIP:` / `FATAL:` 0 次。
- 顶层 `TestIT_` PASS **67** 个:`TestIT_Gap_*` 18、`TestIT_Placement_*` 9、`TestIT_Relocate_*` 7、`TestIT_StorageAudit_*` 1,其余为既有端到端用例。
- 缩进子用例 PASS **16** 个。67 + 16 = 83,与 09-29 记录的「83」同一口径。
- FAIL / panic 为 0。
- 必须出现 `preflight P4 OK: ... (3 keys incl. per-instance processing lists)`,证明 `be6b4ab1d` 的新 P4 路径执行过。
- 跑完 DB 9 / 10 / 11 都是 0,7 个一次性库不存在。
- 与本区间改动相关、要逐个看的用例:
  - `TestIT_Preflight_PassesOnACleanZone`;
  - `TestIT_Preflight_BlocksOnEachHazard/retry_queue_not_empty`、`/dead_queue_not_empty`;
  - `TestIT_Gap_Unmerge_RefusesALiveTargetZoneBeforeAnyWrite/target_retry_queue_not_drained`;
  - `TestIT_Audit_ExitsTwoWhenAHandleIsMissing`、`TestIT_Audit_OnlineGateActuallyBlocks`;
  - `TestIT_Gap_ClearHotState_SweepsZoneZeroLocationsBeforeDeletingScenes`(会走 `2a203a3a4` 的两条新 SCAN 模式)。
- 默认 10 分钟超时;若报 `test timed out after 10m0s`,加 `-timeout 30m` 重跑并注明。

### 第 9 步:PowerShell 契约测试

工作目录:仓库根。

```powershell
pwsh -NoProfile -NonInteractive -File tools/scripts/tests/dev_tools_merge_zone_contract.tests.ps1; "exit=$LASTEXITCODE"
pwsh -NoProfile -File tools/scripts/dev_tools.ps1 -Command help | Select-String 'Player storage placement commands'
pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/k8s_migrate_gate.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/start_game_command_contract.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/go_services_ports.tests.ps1
```

通过标准:

- `dev_tools_merge_zone_contract`:`total=16 pass=16 fail=0`,其中退出码透传那一例要看到 0 / 1 / 2 原样透传、编译失败为 2。
- help 能找到 `Player storage placement commands`。
- `k8s_deploy_contract`:`total=87 pass=87 fail=0`(09-29 是 54),必须 PASS 的有:
  - 「node ConfigMap 的 DbTaskTopicGeneration 必须与 go/db、go/login 的 Kafka.TopicGeneration 三方一致」;
  - 「data-service 的 MappingRedis 不写 DB 键;go/db 读落点记录的 Redis 必须与它同址且为 DB 0」;
  - 「db 是多副本形态:2 副本滚动更新 + PDB + 反亲和」;
  - 「data-service 是多副本形态:2 副本滚动更新 + PDB + 反亲和,ConfigMap 要求 store 必备,迁移 Job 先于 Deployment」(`1bc9d4899`);
  - 「db ConfigMap 必须带非空的库名白名单」。
  - 87/87 是在 `bf4655ea3` 上实跑的。之后 `cpp/` 改了 29 个手写文件,而有一条用例会扫描全部手写 C++ 找 `Rollback*` 客户端符号,所以检出里必须有完整的 `cpp/` 树;预期仍过。
- `k8s_migrate_gate`(`1bc9d4899` 改过):`fail=0`,其中「data-service 的迁移门禁:ConfigMap → Job → Deployment,且 Deployment 是多副本形态」与「Add-GoSvcTopicReplicationEnv:与命令 topic 契约叠加注入,手写同名变量被拒」(`160e7bf4f`)必须 PASS。总数有循环展开,以实际输出为准。
- `start_game_command_contract`:`total=14 pass=14 fail=0`;`go_services_ports`:`total=18 pass=18 fail=0`。
- 不跑 `k8s_client_entry_contract` / `artifacts_lib` / `publish_images`:它们 10-09 起有既有失败,与落点无关。

### 第 10 步:-race

```powershell
# 方案 A:本机有 gcc
cd go\db; $env:CGO_ENABLED = '1'; go test -count=1 -race ./internal/kafka/... ./internal/logic/pkg/proto_sql/... 2>&1 | Tee-Object $env:TEMP\placement-reverify\godb_race.log; cd ..
pwsh -NoProfile -File test.ps1 -Race; cd ..
# 方案 B:没有 gcc 但 Docker 可用(拉 golang:1.26.5 镜像先征得同意;测的是 Linux 构建,报告里注明)
# 挂载只读,避免容器里的 go 改写仓库;容器里拉依赖模块同样属于下载,一并征得同意
docker run --rm -v "${PWD}:/src:ro" -w /src/go/db -e CGO_ENABLED=1 -e GOFLAGS=-mod=readonly -e GOPROXY=https://goproxy.cn,direct golang:1.26.5 go test -race -count=1 ./internal/kafka/... ./internal/logic/pkg/proto_sql/...
# 方案 C:都不行就写「-race 未跑」,不要安装 gcc
```

通过标准:

- 输出里没有 `WARNING: DATA RACE`;方案 A 的 `test.ps1 -Race` 输出 `L1_TESTS_OK`。
- 重点是 `be6b4ab1d` 的续租 / 回收循环与 worker 并发、能力标记心跳 goroutine,这些从未在 -race 下跑过。

### 第 11 步:重编 db.exe 并核对版本戳

工作目录:仓库根。

只在第 3 步全绿之后做。

```powershell
if (Test-Path bin\go_services\db.exe) { Copy-Item bin\go_services\db.exe $env:TEMP\placement-reverify\db.exe.bak; Get-FileHash bin\go_services\db.exe -Algorithm SHA256 }
pwsh -NoProfile -File tools/scripts/go_services.ps1 -Command build -Services db
go version -m bin\go_services\db.exe | Select-String 'proto2mysql|buildinfo|-ldflags'
Get-FileHash bin\go_services\db.exe -Algorithm SHA256
git status --porcelain=v1 --untracked-files=all
```

通过标准:

- 输出 `[ok] db`;`shared/buildinfo.Commit` 的提交号前缀是 `73b852f3234e`(`rev-parse --short=12`);依赖里有 `proto2mysql v0.2.0`。记录新旧两份 SHA256。
- `-dirty` 后缀:只要 `git status --porcelain` 有任何输出(含未跟踪文件)就会带上。只有当这些输出全部落在 `docs/` 或 `AGENTS.md` 下时才允许 `-dirty`,并把那几行记进报告;否则换到干净检出上编。
- 10-08 构建的 db.exe 不含 `be6b4ab1d`,不能拿来做第 12 步,也不能和新版混跑:旧版看不到按实例的 processing 列表。

### 第 12 步(可选):需要真依赖的补充核对

1. **dev_tools 的 INFRA 退出码透传**(09-29 清单第 9 条末项,当时是否做过未核实):

   ```powershell
   Test-NetConnection 127.0.0.1 -Port 6399 -InformationLevel Quiet   # 必须 False
   pwsh -NoProfile -File tools/scripts/dev_tools.ps1 -Command merge-zone-audit -MergeSourceZone 901 -MergeTargetZone 902 -MergeRedisAddr 127.0.0.1:6399 -MergeMappingRedisAddr 127.0.0.1:6399 -MergeMySqlDsn 'root:x@tcp(127.0.0.1:1)/none'; "exit=$LASTEXITCODE"
   ```

   通过标准:exit=2,输出含 `NOT a clean bill of health`,`kafka_db_task_queues` 行以 `INFRA:` 开头。

2. **按实例 processing 列表的真 Redis 探针**(补 `be6b4ab1d` 在集成测试里的缺口;临时容器 6391,用完删除):

   ```powershell
   docker run -d --rm --name mz-p4-probe -p 127.0.0.1:6391:6379 redis:7.2
   docker exec mz-p4-probe redis-cli ZADD kafka:retry:instances:db_task_zone_901 9999999999999 db-probe:6000
   docker exec mz-p4-probe redis-cli RPUSH kafka:retry:processing:db_task_zone_901:db-probe:6000 payload
   cd tools\merge_zone; go build -o $env:TEMP\mz_p4_probe.exe .; cd ..\..
   & $env:TEMP\mz_p4_probe.exe -mode audit -source-zone 901 -target-zone 902 -redis-addr 127.0.0.1:6391 -mysql-dsn 'root:x@tcp(127.0.0.1:1)/none' -table-list-json generated\data\mysql_database_table_list.json | Select-String 'kafka_db_task_queues'
   docker rm -f mz-p4-probe; Remove-Item $env:TEMP\mz_p4_probe.exe
   ```

   通过标准:`kafka_db_task_queues` 是 block(`[!]`),notes 列出 `kafka:retry:processing:db_task_zone_901:db-probe:6000=1`,不以 `INFRA:` 开头。整体退出码会因 MySQL 项而是 2,不作判据。

3. **本机双 db 实例**:验证租约、孤儿回收、多副本下的能力标记,并端到端核对 merge_zone 读的键就是 go/db 真实写的键。

   前置:
   - 只能在 Redis 里没有他人数据的隔离环境做,必须用第 11 步的 db.exe;
   - Kafka 9092 要起着(`EnsureTopics` 连不上 go/db 就 panic;10-10 时 Codex 机器上的 Kafka 容器是 Exited,启动它要先征得同意),`KAFKA_TOPIC_REPLICATION_FACTOR` 不设;
   - `zone_1_db` 已迁移:`cd go\db; go run ./cmd/migrate -f etc/db.yaml -command up`,库不存在时加 `-create-database`。
   - 当前造不出真实存盘流量(robot 与 C++ scene 都编不过),所以只用假收据验证认领、回收与标记,报告里写「真实存盘流量下的多实例未验证」。

   ```powershell
   pwsh -NoProfile -File tools/scripts/go_services.ps1 -Command start-exe -Services db -Counts db=2
   docker exec redis redis-cli ZRANGE kafka:retry:instances:db_task_zone_1 0 -1 WITHSCORES   # 记下两个实例的真实成员名
   pwsh -NoProfile -File tools/scripts/dev_tools.ps1 -Command merge-zone-capability-check -MergeDbCapabilityZones 1; "exit=$LASTEXITCODE"
   docker exec redis redis-cli LPUSH "kafka:retry:processing:db_task_zone_1:<:6001 实例的真实成员名>" verify-orphan-1
   pwsh -NoProfile -File tools/scripts/dev_tools.ps1 -Command merge-zone-audit -MergeSourceZone 1 -MergeTargetZone 2 | Select-String 'kafka_db_task_queues'
   pwsh -NoProfile -File tools/scripts/go_services.ps1 -Command status   # 记下 :6001 实例的 PID
   Stop-Process -Id <:6001 实例 PID> -Force
   # 约 45 秒后:
   docker exec redis redis-cli ZRANGE kafka:retry:instances:db_task_zone_1 0 -1
   docker exec redis redis-cli LRANGE kafka:dead:queue:db_task_zone_1 0 -1
   docker exec redis redis-cli TTL db:capability:zone:1   # 间隔 ≥ 35 秒连读两次
   pwsh -NoProfile -File tools/scripts/dev_tools.ps1 -Command merge-zone-capability-check -MergeDbCapabilityZones 1; "exit=$LASTEXITCODE"
   pwsh -NoProfile -File tools/scripts/go_services.ps1 -Command stop -Services db
   # 约 95 秒后:
   docker exec redis redis-cli EXISTS db:capability:zone:1
   pwsh -NoProfile -File tools/scripts/dev_tools.ps1 -Command merge-zone-capability-check -MergeDbCapabilityZones 1; "exit=$LASTEXITCODE"
   docker exec redis redis-cli LREM kafka:dead:queue:db_task_zone_1 0 verify-orphan-1
   ```

   通过标准:
   - 两个实例的启动日志各有一行 `retry receipt ownership ready: topic=db_task_zone_1 instance=<名>` 和 `[placement] capability marked: db:capability:zone:1=placement-routing-v1 ttl=1m30s refresh=30s`;ZRANGE 有两个成员;第一次 capability-check exit 0。
   - 注入假收据后,`merge-zone-audit` 的 `kafka_db_task_queues` 行为 block,详情里有 `kafka:retry:processing:db_task_zone_1:<真实实例名>=1`。这才是 merge_zone 与 go/db 键名一致的端到端证据;单测两边钉的是同一组字面量,算自证。
   - kill 之后约 40 秒内,存活实例打出 `reclaimed abandoned retry receipts: ... orphans=1`。只有真有在途收据时才打这行,所以必须先注入。ZRANGE 只剩一个成员;假收据因载荷畸形进入死信。
   - 只停一个副本时,两次 TTL 都在 60~90 之间(另一个副本仍在续写),capability-check 仍 exit 0。全部停掉 95 秒后 `EXISTS` = 0,capability-check exit 1。
   - 单测里没有双心跳用例,多副本下的能力标记只能靠这一步验证。
   - 停机归还路径(SIGTERM → release)本机验证不到:本机 stop 用的是 `Stop-Process -Force`。要到 kind 上 `kubectl delete pod` 才能做,本步不覆盖。

### 第 13 步:C++(门禁,默认不做)

落点的 C++ 部分(`GetDbTaskTopic`、`config.cpp` 读世代号、`player_lifecycle.cpp` 的唯一调用点)自 `4493b3c9f` 未改,09-29 编译链接通过、`DbTaskTopicGeneration.*` 4 个 PASS。HEAD 上编不过,全是与落点无关的既有问题:

1. `10cca6b66`(装备属性)没有导表与 proto-gen:`table.vcxproj` 登记的 28 个装备表产物不存在,`cpp/generated/proto` 没有 `has_equip` / `CombatAttributes`,而 `cross_zone_test.cpp` 已经在用 `ItemEntry.equip`。
2. `grpc_client.vcxproj` 登记的 guild_internal / match_internal 包装 4 个文件不存在。
3. `node.h` 的两个裸指针成员(现在在 :248、:286)仍会被检查拦下。

**09-29 的绕法(`-p:BuildProjectReferences=false` 链接旧 `table.lib`)这次不能再用**:问题已经从链接期变成编译期,而且拿旧 `modules.lib` 配新头文件会在运行期损坏内存。

只读前置核对(工作目录:Codex 的完整检出):

```powershell
Select-String -Path cpp\generated\table\table.vcxproj -Pattern 'Include="([^"]+\.(h|cpp|cc))"' -AllMatches | % { $_.Matches } | % { $_.Groups[1].Value } | ? { -not (Test-Path (Join-Path 'cpp\generated\table' $_)) }
Select-String -Path cpp\generated\grpc_client\grpc_client.vcxproj -Pattern 'Include="([^"]+\.(h|cpp))"' -AllMatches | % { $_.Matches } | % { $_.Groups[1].Value } | ? { -not (Test-Path (Join-Path 'cpp\generated\grpc_client' $_)) }
Select-String -Path cpp\generated\proto\common\component\item_base_comp.pb.h -Pattern 'has_equip'
```

- 前两条有输出(缺文件)或第三条无命中:**停下,本步记为「被既有生成物缺口阻塞」**,不在本清单里补。
- 补齐需要用户或负责人批准:按 [equipment-attributes.md](../design/equipment-attributes.md) §9 做导表与全量 proto-gen。
- 生成器的默认配置会写仓外的 `../mmorpg-client`,必须先用配置副本改写落点(AGENTS §9)。
- `proto/message_id.txt` 的 244–249 已发号,必须保留。

前置全部满足后,MSBuild 严格串行 `/m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 -p:SkipNoRawPointerMemberCheck=true`。

编译顺序:table → proto → proto_helpers → core → config → infra → muduo(`cpp/libs/engine/muduo_windows`)→ session → thread_context → third_party → rpc → grpc_client → modules → battle → `cpp/libs/services/gate`(gate 库,不是 gate 节点)→ scene(静态库)→ gate 节点、scene 节点 → `cpp\tests\cross_zone_test\cross_zone_test.vcxproj`。

- `cross_zone_test.vcxproj` 的 AdditionalDependencies 链接了上面这些库,但工程里没有任何 ProjectReference,所以这些库必须事先编好;也可以按 `game.sln` 的依赖来编(sln 依赖是否齐全未核实)。
- modules 与 scene 必须同批重编。
- 若报 `max` 宏错误,按 PROGRESS(约 6928 行)的做法给 `cross_zone_test` 加 `NOMINMAX`。

然后:

```powershell
pwsh tools/scripts/run_cpp_tests.ps1 -Filter cross_zone_test
build\cpp\tests\cross_zone_test.exe --gtest_filter=DbTaskTopicGeneration.*
build\cpp\tests\cross_zone_test.exe
```

通过标准:

- 每个工程 0 error。
- `DbTaskTopicGeneration.*` 4 个 PASS:`FirstGenerationKeepsLegacyNameWithoutSuffix`、`ZeroMeansUnconfiguredAndEqualsFirstGeneration`、`LaterGenerationsAppendSuffix`、`GenerationDoesNotChangeZoneRouting`。
- 全量 **166** 个全 PASS(09-29 是 105)。
- 退出码非 0 却没有 `[  FAILED  ]` 行,说明中途 `LOG_FATAL`,不算绿。

### 第 14 步:范围外(只登记,本清单不执行)

- **压测**:按 AGENTS §6.2 全流程,在隔离环境做:先存 `prev-summary.txt` 基线、清数据、`stress_snap.ps1`、`stress_summarize.ps1`,再出对比表。go/db 每条任务多两次 MGET 的开销至今没测过,没有对比表不下性能结论。
- **真实集群演练**:pin 合服、relocate、反向 relocate 各一次并记耗时;kind 上 data_service / db 两副本删 Pod。

## 3. 清理

全部跑完(或中途停止)后:

1. 基线 worktree:`git worktree remove --force $env:TEMP\placement_base; git worktree prune`。
2. 临时容器 `ds-it-redis`、`mz-p4-probe` 删除;`DATA_SERVICE_IT_*` 环境变量移除。
3. merge_zone 集成测试中途超时或 panic 时,TestMain 末尾的清理不会执行。补救办法:重跑一次(TestMain 开头会先 DROP),或手工 DROP 第 8c 步列的 7 个库;DB 9 / 10 / 11 只有在跑前确认为空时才可 FLUSHDB;删除 `%TEMP%\merge_zone_it_<pid>.exe`。
4. 双 db 实例(第 12 步):
   - 假收据要 `LREM` 掉。
   - 本机 stop 走 `Stop-Process -Force`,release 不会执行:登记表里会留下过期成员,ordering lock 要到 TTL(约 2 分钟)才释放,紧接着跑 merge_zone 要等锁过期。
   - `run\etc\go_services` 下的派生 yaml 可删,`run\logs\go_services\*.log` 留作证据。
5. db.exe 回滚:放回 `db.exe.bak` 即可。但新版被 `-Force` 停过之后不能直接换回旧版,旧版看不到按实例的列表。要先起一个新版实例等它回收;或者手工把 `kafka:retry:processing:db_task_zone_1:*` 逐个搬回 `kafka:retry:queue:db_task_zone_1`,再 ZREM 登记表。
6. 最后用 `status-before.txt` 比对,确认工作区没有新增改动。

## 4. 回报格式与结果回写

- **每步给出**:实际命令、工作目录、退出码、用例计数、SKIP 计数、日志路径(`$env:TEMP\placement-reverify\*.log`)。失败时保留完整日志、首个 FAIL 前后各 50 行,以及在基线 worktree 上复跑同名用例的日志,并按第 1 节第 5 条归因。
- **环境信息**:执行机器、Go 版本、HEAD、`git status` 前后对比;集成测试另记 `docker exec mysql mysql --version`、`docker exec redis redis-server --version`(compose 里两者都是 `:latest` 镜像)。
- **CodeIntegrity 拦截时**:附 `Get-WinEvent -LogName 'Microsoft-Windows-CodeIntegrity/Operational' -MaxEvents 30` 里 3077 / 3033 事件的摘要。
- **结果回写**(由收到回报的会话来做,回报到达前任何人不得写「通过」):
  - `docs/PROGRESS.md` 追加一条(只追加,不改旧条目)。09-28 条目里「TestIT_* 83 个顶层用例」的说法要在新条目里更正为「67 顶层 + 16 子用例」。
  - [player-storage-placement.md](../design/player-storage-placement.md) 文首「运行状态」;
  - [merge-zone-runbook.md](../ops/merge-zone-runbook.md) 文首状态与 §12;
  - [server-merge-gap-fixes.md](../design/server-merge-gap-fixes.md) C' 表下的运行状态注;
  - [no-single-node-horizontal-scaling-20261001.md](../design/no-single-node-horizontal-scaling-20261001.md) §3、§4 标题里的「未编译、未跑测试」,以及 §3.5、§4.5 的验证结果;
  - [world-channel-switch.md](../design/world-channel-switch.md) 验证步骤第 2 步(merge_zone 覆盖 `2a203a3a4`)的执行状态;
  - 本文件末尾加一节「执行结果」。
- **未执行的步骤**(-race、C++、第 12 步)如实写「未做」及原因。
- **交付说明**按 AGENTS §12 写明 Java 版状态:落点对 Java 不适用,PARITY 行已登记在 Java 仓库。

## 5. 已知覆盖缺口(建议后续补测,不在本清单)

1. 落点的三种延后(`frozen_deferred` / `store_unavailable` / `recheck_moved`)在单测里只走 `retryOwner=nil` 的旧共享键,没有用例把「按实例认领」和「冻结延后」串起来跑。建议补一条:用 `newRetryOwnership` 认领后对冻结写调用 `handleTask`,断言收据在 `processing:{topic}:{实例}` 与 ready 之间往返、`RetryCount` 不变。
2. merge_zone 集成测试只往 ready 和死信里塞数据,没写登记表与按实例列表;P4 的 ZRANGE 路径和 fail-closed 分支只有纯函数单测(第 12 步第 2 条是临时探针)。建议给 `TestIT_Preflight_BlocksOnEachHazard` 加一个 per-instance processing 子用例。
3. P4 能查全按实例列表,依赖 go/db 的一条不变量:非空的按实例列表一定在登记表里(认领脚本先 ZADD 再 RPOPLPUSH;回收与归还只在 LLEN==0 时 ZREM)。merge_zone 没有用 `SCAN kafka:retry:processing:{topic}:*` 做第二道防线。
4. 两个 go/db 实例并发消费同一 key 的保序(重试跨实例、ordering lock + applied 游标)只有单进程模拟用例。
5. K8s 的 db ConfigMap 两个副本共用,**绝不能写 `Kafka.RetryInstanceId`**:写了两个副本就同名,会互相收走对方在途的收据。现在 `tools/scripts` 与 `deploy` 下 grep 不到这个键,所以是安全的,但没有契约测试守住这一点。
6. 切线预检 2b 在 3b 合服围栏之前:归属区正在合服的玩家主动选线时,可能拿到 22 / 23 / 24 而不是 21;跨节点第一跳回 18 时已写入 `player:{id}:channel_switch_cooldown`,于是先被扣冷却,重发后才拿到 21。这不是 location / owner_epoch,不违反设计 §8.2,但属于围栏前的写入。没有任何用例同时置位 `ClientChannelPick` 与 HomeZoneMerging。建议补两条:同节点选线 + 合服 → 21 且不写冷却键;跨节点第一跳 → 18 且写冷却,重发 → 21。
7. `Stop()` → `releaseRetryOwnership` 的顺序,以及 ctx 已取消时 `releaseOrderingLock` 仍能释放(relocate R2 等的那把锁),都没有单测;本机 `-Force` 停止验证不到。
8. `be6b4ab1d` 之后,db_task 换代前的排空核对要查到每个实例的 processing 列表([db-task-kafka-partition-contract.md](../design/db-task-kafka-partition-contract.md) 已改)。落点设计与 runbook §14 里提到排空的地方是否需要同步,未逐条核对。
