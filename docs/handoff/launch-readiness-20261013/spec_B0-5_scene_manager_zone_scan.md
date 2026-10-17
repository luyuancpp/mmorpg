# 调研规格:scene_manager 负载刷新与全库 SCAN(2026-10-16)

> **状态**:只读调研产出,**未落码**。由开服线会话 2026-10-16 的调研工作流生成(一名调研者出规格、一名核实者逐条反驳),原样保存,未经人工改写。
> **怎么用**:前半部分是调研者的规格;「# VERDICT」之后是核实者意见。**落码前必须先吸收核实意见里的 must / should**,被判 refuted 的事实以核实者为准。行号与工作区状态是 2026-10-16 03:48~04:35 的快照,工作区有多个会话在并行修改,落码前按函数名重新定位。
> **汇总、拍板项与推荐顺序**:见 [docs/PROGRESS.md](../../PROGRESS.md)「2026-10-16 开服线剩余工作调研」条目,以及交接 [handoff-launch-readiness-and-d1-20261013.md](../handoff-launch-readiness-and-d1-20261013.md) 文首 10-16 段。

## item
scene_manager 负载刷新只读本 zone:去掉「每个副本每 5 秒读全服所有节点计数」与 fullSync 里两次全库 SCAN MATCH(容量模型 §1.3 / §4.2 第 0 批、A0-4、A1;全局单点交接 §5.1 第 2 条、§7)
## already_done
无。本项没有被任何会话做掉:PROGRESS.md 2026-10-13 之后没有相关条目(grep「scene_manager」第 7900 行以后只有登录代次、八域移植、reassignSceneNode 无 CAS 的归属登记);load_reporter.go 最近一次改动是 e4ff1d165(2026-10-14),只改了 IsNodeAlive 的一条英文日志;go/scene_manager 下除 generated/pb/game/message_id.go 外没有未提交改动。

已经满足、不用再做的部分:
- 键名已经带 zone:scene_nodes:zone:{z}:load、node:zone:{z}:{n}:*、world_channels:zone:{z}:{conf}、world_channels:desired:zone:{z}。任务里「键名带 zone 前缀」这一半已经成立。
- 跨 zone 的读者已经是点查,没有全扫:ZCARD / SCARD / ZSCORE / 单 zone ZRANGE。
- nodes_by_role 对本进程 ZoneId 补 0 已经有了(metrics.go:737-766)。
- Java 版已是按 zone 的节点目录 + zone 登记集合,本身没有这个问题。
## summary
问题:按 2000 万同时在线推算(容量模型 §0.3 参数,Z=200~444、N_s=1,778~8,889;下列数字都是公式推导,未实测),scene_manager 负载刷新的代价同时随 Z 和 N_s 增长:
- 刷新读:0.8·Z·N_s ≈ 28 万~316 万 GET/s。
- 每个副本每拍要串行 2·N_s 次往返(3,556~17,778 次)。按每次 0.2~0.5 ms 计,0.7~8.9 s,已经超过 5 s 的拍长;领导者还要再加同样多的写。
- 每节点指标序列约 213 万~2,368 万条;nodes_by_role 约 8Z² ≈ 32 万~158 万条。
- 领导者每次 fullSync 跑两次 SCAN MATCH(scene_nodes:zone:*:load、world_channels:zone:*)。SCAN 遍历的是整个共享库,交接文档推算有数十亿键,单次是小时级。

根因:
- scene_manager 按 zone 部署,每 zone 2 副本,但用全局一把锁选出一个领导者。
- 数据面是全局的:C++ scene 按 playerId 在全服所有 SM 中挑一个。
- 每个副本 watch 全服 scene 节点。refreshLoadScores 对内存里全部节点逐个串行 GET 两个计数:读不分领导者,写只由领导者做。
- 「有哪些 zone」靠 SCAN 全库来发现。

做法:
1. 刷新改成只由领导者按 zone 批量做,跟随者一条 Redis 命令都不发。
   - 每拍 1 条 SADD 登记,读和写都按 zone 分批走 pipeline(每批 1024 条),每个 zone 只发一条 ZADD。
   - 读失败时保留旧分,不再按 0 写入。
   - 每节点序列只由领导者导出;nodes_by_role 由领导者发布全部 zone,跟随者只发布本 zone。
2. 不采用字面上的「每个副本只刷新本 zone」。那需要每个 zone 自己有写者,会破坏 load_reporter.go:235-239 写明的单写者不变量(跟随者的写会把死节点重新加回负载集),还要为滚动升级另加兜底。按 AGENTS §11「正确性 > 简单」取舍,每 zone 一个领导者留给 A1。领导者批量刷新正是 A1 将来按 zone 拆分时可以直接复用的形状。
3. 新增只加不减的 zone 登记集合 scene_manager:zones,两处 SCAN 改为读它:
   - 写入方:领导者刷新(先 SADD 再 ZADD)、updateNodeLoad、期望频道数的播种 / 写回。
   - 读取方:sweepStaleLoadSetMembers、孤儿清理。孤儿清理改为每个 zone 读一次 HKEYS world_channels:desired:zone:{z},并把删期望数字段放到最后一步。
   - 删除方:只有 merge_zone -clear-source-hot-state。
   - 自愈:领导者每拍对所有有活节点的 zone 重新登记。旧数据靠默认关闭的一次性 SCAN 开关补登记。

C++ 与客户端契约都不改;Java 版无需改动。
## est_files: 20
## current_facts
F1. scene_manager 按 zone 部署,每 zone 2 副本,但选主锁默认是全局一把,全服所有副本只出一个领导者
   证据: 已亲自打开:go/scene_manager/scene_manager_service.go:94-102(LeaderLockKey 为空时用 "scene_manager:leader:lock");go/scene_manager/internal/config/config.go:78-80(注释写明「服务本身 zone-agnostic,所有副本共用同一把锁」);deploy/k8s/manifests/go-svc/scene-manager.yaml:24(replicas: 2);tools/scripts/k8s_deploy.ps1 的 scene-manager 是 Scope='zone'(本次读到在 :1777),ConfigMap 写 `ZoneId: ${CurrentZoneId}`(本次读到在 :6105 一带)。注意:该文件正被他人修改,两次读取行号漂了约 60 行
F2. SM 数据面是全局的:C++ 在全服所有 SM 实例中按 playerId 挑一个,SceneManagerNodeService 不在 zone 限域类型里。所以 SM 内存里的节点表(knownNodes)和 etcd watch 现在不能收窄到本 zone
   证据: 已亲自打开:cpp/libs/engine/core/network/node_utils.cpp:267-310(GetSceneManagerEntity 遍历 SceneManager 注册表全部实例);cpp/libs/engine/core/node/system/node/node_util.cpp:115-131(IsZoneScopedNodeType 只含 Gate / Scene / Login / PlayerLocator)。该 .cpp 有他人未提交改动,只引用不改
F3. 每个副本 watch 全服 scene 节点,etcd 前缀不带 zone;C++ 节点键的格式是 SceneNodeService.rpc/zone/<z>/node_type/<t>/node_id/<id>
   证据: 已亲自打开:go/scene_manager/internal/logic/load_reporter.go:32(sceneNodePrefix = "SceneNodeService.rpc/")、:630(fullSync 全前缀 Get)、:806(全前缀 Watch);cpp/libs/engine/core/node/system/etcd/etcd_manager.cpp:47-58
F4. 刷新每 5 s 一拍,每个副本对内存里全部节点逐个串行 GET scene_count 和 player_count,读不分领导者;ZADD 负载集和 SET 类型镜像只由领导者做;fullSync 时每个副本对全部节点再做一遍
   证据: 已亲自打开:load_reporter.go:31(LoadReportInterval = 5s)、:230-256(updateNodeLoad::231-232 两次 readInt64,:240 起 isLeader 才写,:254 每个副本都调 ObserveNode)、:658-660(fullSync 对 next 全部调用)、:834-841(ticker)、:1001-1020(refreshLoadScores 持 RLock 遍历全部 knownNodes 做 I/O)。容量模型 §0.6 第 3 条(:103)已经指出这一点
F5. 只能由领导者写的理由已写在代码里:跟随者的内存镜像落后时,会把领导者刚摘掉的死节点重新 ZADD 回去,而且没有第二次 DELETE 来纠正
   证据: 已亲自打开:load_reporter.go:235-239 的注释
F6. 负载集成员与计数的写入方都是 Go 的 SM:计数由任意副本的数据面 INCR / DECR;负载集与类型镜像只由领导者写入 / 摘除。C++ 场景节点只在 etcd 注册,不写任何 scene_nodes / node:zone 键
   证据: 已亲自打开:grep nodePlayerCountKey / nodeSceneCountKey 的写入点(createscenelogic.go:427/562、enterscenelogic.go:696、instance_lifecycle.go:319-373、world_rebalance.go:274-278、world_init.go:232 等);ZADD 负载集只在 load_reporter.go:242,ZREM 在 :354(领导者)与 :929(PUT 换 zone 分支,未按领导者门控,属既有问题);git grep 'scene_nodes:' 在 cpp/ 下 0 命中
F7. 全库 SCAN 一共两处,都在领导者的 fullSync 里:清扫陈旧负载集成员用 SCAN scene_nodes:zone:*:load,孤儿频道清理用 SCAN world_channels:zone:*。fullSync 在启动、watch 中断、领导权变化时都会跑
   证据: 已亲自打开:load_reporter.go:743-797(:750 SCAN,COUNT 64)、:688-721(领导者分支依次调用 sweep 与 CleanupOrphanWorldChannels);orphan_cleanup.go:33-90(:56 SCAN,COUNT 256);leader_gate.go RequestLoadReporterResync;scene_manager_service.go 的 OnStateChange 调 RequestLoadReporterResync。全局单点交接 :187 推算换主后 SCAN 数十亿键约 4~10 小时(转引,未核实)
F8. 孤儿清理可以改用期望数 Hash 做索引:生产代码里往频道集合 SADD 的唯一地方,会先经 DesiredWorldChannelCount 播种 world_channels:desired:zone:{z} 的字段。但 deleteOrphanChannel 现在先 HDEL 期望数、后 DEL 集合;改用索引后,中途失败会让这组孤儿再也找不到
   证据: 已亲自打开:world_init.go:174(DesiredWorldChannelCount)→ :224(唯一的 Sadd(channelSetKey));world_autoscale.go:73-99(命中直接返回,缺字段时 HSETNX 播种;HSETNX 出错照常返回种子值,是漏登记口)、:102-118(Hset);world_autoscale.go:353 只往排空集合 SADD;orphan_cleanup.go:203(Hdel)早于 :212(Del setKey)
F9. 跨 zone 传送与选点读别的 zone 负载时,全部是按 zone 的点查,不需要全扫;go/match 不读 scene_nodes / world_channels
   证据: 已亲自打开:enterscenelogic.go:953(ZCARD nodeLoadKey(zone))、:1009(SCARD worldChannelsKey(targetZone, conf));load_reporter.go:1059(IsNodeAlive 用 ZSCORE);node_selection.go:37-40(单 zone ZRANGE)、:97(逐节点 GET 类型镜像,按 zone 内节点数计);git grep 在 go/match 下 0 命中
F10. 指标的现状:三个每节点 gauge 由每个副本为全服所有节点发布;nodes_by_role 每个副本发布全部 zone 并给本 zone 补 0;ResetLeaderGauges 不清每节点序列;看板对 nodes_by_role 用 sum by,会按发布者个数放大
   证据: 已亲自打开:internal/metrics/metrics.go:36-56、:466-470、:694-713、:737-766;deploy/k8s/scene-manager-dashboard.json:92(sum by (zone_id, role));deploy/k8s/scene-manager-alerts.yaml:55-100(PoolEmpty 只做 >0 / ==0 判定)、:103-125(max / stddev / avg)
F11. merge_zone 会清掉源 zone 的负载集与期望数等键,但不认识新的登记集合;它是独立 module,没有 miniredis,有 merge_integration 标签的集成测试
   证据: 已亲自打开:tools/merge_zone/scene_hot_state.go:64-76(键格式)、:319-337(SCAN 模式与固定键)、:355 起 clearSourceZoneHotState、:475-492(固定键删除);scene_hot_state_test.go:3 与 :180-210;integration_test.go:1(//go:build merge_integration)
F12. 测试设施:miniredis v2.37 的 SCAN 忽略 COUNT,一次返回全部结果,所以无法按命令次数区分 SCAN 与 SMEMBERS;它提供 SetPreHook 拦截命令(命令名大写),仓内已有用它注入故障的先例;go-redis 的 pipeline 会把 redis.Nil 当作首个错误返回,要逐条判断
   证据: 已亲自打开:miniredis v2.37.0 的 cmd_generic.go cmdScan 注释「We return _all_ (matched) keys every time」;server/server.go:204-213(Dispatch 先 ToUpper 再调 preHook);go/scene_manager/internal/logic/node_detach_deferred_test.go:64-90(installNodeDetachFaultsForTest);go-redis v9.17.3 command.go:106-113(cmdsFirstErr);go-zero v1.10.0 redis.go:1271(PipelinedCtx)、:78(Pipeliner = red.Pipeliner,带 Do)、:2694(breaker 把 redis.Nil 视为可接受)
F13. 不必改 go.mod:go/scene_manager 只把 go-redis 列为 indirect;用 go-zero 的 Pipeliner 的 Get / Set / Do 就能拿到每条命令的结果,不用直接 import go-redis
   证据: 已亲自打开:go/scene_manager/go.mod:15(go-zero v1.10.0)、:64(go-redis v9.17.3 // indirect);go/data_service/internal/routing/player_name_cache.go:141、:177 是只用 go-zero redis.Pipeliner 的先例
F14. Java 版已按 zone 设计,无需对等改动:scene 节点每 5 s 自报到按 (类型, zone) 分的目录,SM 按 zone 点读;世界频道有 zone 登记集合和每 zone 领导者;game-scene-manager 里没有 SCAN
   证据: 已亲自打开:D:\luyuan\xuanming-server-mmo\game-scene-manager/src/main/java/com/game/scenemanager/RedisSceneNodeSource.java:15-27;game-discovery/src/main/java/com/game/discovery/NodeDirectory.java:80-93(getMapCache(nodeDirectory(type, zone)).readAllValues());game-discovery/.../RedisKeys.java:66-68、:246-250;game-discovery/.../world/RedissonWorldChannelStore.java:21、:209-226(每 zone 领导者)、:273-285(registerZone / zones())
F15. 2000 万在线的量级按容量模型 §0.3 的 C_z(45k / 100k)与 c_scene(2.25k / 11.25k)推出:Z=200~444,N_s=1,778~8,889,SM 副本 2Z=400~888。§7.2 要求按在线量增长的项重新推导,不能把 S3 的数字直接乘二:刷新项 ∝ Z·N_s,PCU 翻倍时约 ×4
   证据: 已亲自打开:docs/design/capacity-model-hok-scale.md:47-50(参数)、:217(S3 行:0.8·Z·N_s GET/s,序列 6·Z·N_s)、:894-910(§7.2);全局单点交接 :288(E4:刷新 ∝ Z×节点数,nodes_by_role ∝ Z²)
## plan
- [go/scene_manager/internal/logic/zone_registry.go(新建)] 新增 zone 登记集合模块。

(1) const sceneManagerZonesKey = "scene_manager:zones"(SET,成员是 zone 的十进制)。文件头注释写清以下契约:
- 只加不减,唯一删除方是 merge_zone -clear-source-hot-state。
- 写入方:领导者负载刷新(每拍一条 SADD 覆盖快照里全部 zone)、updateNodeLoad、DesiredWorldChannelCount 播种分支、setDesiredWorldChannelCount。
- 不变量:任何 zone 的负载集出现第一个成员之前、期望数 Hash 出现第一个字段之前,这个 zone 已经登记。
- 读取方:sweepStaleLoadSetMembers、CleanupOrphanWorldChannels。
- 多个副本同时写是安全的:集合只加,多登记一个 zone 只多一次空查,不存在把死节点加回负载集那类问题。
- 键名刻意不以 scene_nodes:zone: / world_channels:zone: / world_channels:desired:zone: / node:zone: 开头,避免被旧版本的 SCAN 和 merge_zone 的模式误命中。
- 自愈:领导者每拍重新登记所有有活节点的 zone;集合与负载集同在核心实例,丢失时一起丢,所以只在这个键被单独删除时才需要引导扫描。

(2) func registerSceneManagerZones(svcCtx, zones ...uint32) error:去重;空参数直接返回;Sadd 一条命令。zone 0 不特殊处理,与旧 SCAN 覆盖范围一致。

(3) func listRegisteredZones(svcCtx) ([]uint32, error):Smembers 后用 ParseUint(…,10,32) 解析;非法成员打英文 Errorf 并跳过;结果排序后返回。

(4) func unionZones(lists ...[]uint32) []uint32:去重排序。

(5) func bootstrapZoneRegistryFromScan(svcCtx) (int, error):
- 只在 Config.ZoneRegistryBootstrapScan=true 时由 fullSync 领导者分支调用。
- SCAN scene_nodes:zone:*:load(用 NodeLoadKeyFmt Sscanf 解析)和 world_channels:desired:zone:*(用 WorldDesiredChannelsKeyFmt 解析),COUNT 512;解析出的 zone 登记进集合。
- 打英文 Infof:"[ZoneRegistry] bootstrap scan registered %d zones"。
- 出错时返回 error;已登记的保留。

日志一律用英文,注释用中文。
   工作区: 新文件,当前不存在。go/scene_manager 下只有 generated/pb/game/message_id.go 有他人未提交改动,与本项无关
- [go/scene_manager/internal/logic/load_reporter.go] (a) 新常量 loadRefreshPipelineChunk = 1024(单批命令数)。

(b) updateNodeLoad(:230-256):
- 开头 `if !isLeader() { return }`:跟随者不读、不写、不发指标。
- 领导者:先 registerSceneManagerZones(zone)。失败时 Errorf,只写类型镜像、不 ZADD,直接返回,保住「先登记后有成员」。
- 两个计数改用新函数 readNodeCounter(svcCtx, key) (int64, bool):redis.Nil 或空串视为 (0,true);其他错误或 ParseInt 失败视为 (0,false)。任一失败就不 ZADD:保留旧分,也不新增成员;记一次节点读错误指标。
- 改成先 SET 类型镜像、再 ZADD。这样新成员进负载集时已经带类型,缩短 node_selection.go:52-57「未分类节点进所有池」的窗口;类型镜像不带成员是无害的(:357-359 注释)。
- ObserveNode 只在读成功时调用。
- 保留 :235-239 单写者理由的注释并扩写。readInt64 保留,测试在用。

(c) 重写 refreshLoadScores(:1001-1020),签名不变:
- 在 RLock 内把 knownNodes 拷成 map[zone][]nodeEntry(按 etcd key 排序),同时算出 nodes_by_role 的计数,出锁后再做 I/O,不再持锁访问 Redis。
- leading := isLeader()。领导者:metrics.SetNodesByRole(全部计数, []uint32{ZoneId})。跟随者:计数只保留 Config.ZoneId 后再 SetNodesByRole(…, []uint32{ZoneId}),接着 metrics.ResetNodeSeries() 并返回。每拍都清,是为了防 OnStateChange 与本拍竞态后留下旧序列。
- 领导者调用 refreshZoneLoadSets。

(d) 新增 refreshZoneLoadSets(svcCtx, byZone),只在领导者上运行:
- 每个 zone 内按 nodeID 去重。同一身份有多条注册时,用 etcd key 字典序最小那条的类型;getNodesForPurpose 会因 isKnownNodeIdentityAmbiguous 跳过这个身份,取哪条不影响选点。
- ① registerSceneManagerZones(全部 zone)。失败时 registryOK=false,本拍 ZADD 带 XX:只更新已有成员,不新增;outcome 记 registry_failed。
- ② 读:按 1024 条一批 PipelinedCtx,每个节点 p.Get(scene_count)、p.Get(player_count),返回值存进 []interface{ Result() (string, error) }。PipelinedCtx 的返回错误不作判据(redis.Nil 也会被当作首个错误返回),逐条判断:Nil 取 0;其他错误算该节点读失败。
- ③ 写之前再判一次 isLeader(),已降级就停,outcome 记 lost_leadership。
- ④ 写:分批 pipeline。每个 zone 先对每个去重后的节点 p.Set(typeKey, typeStr, 0)(不论读成败,与现状一致),再对读成功的节点发一条 p.Do(ctx, "ZADD", loadKey, ["XX",] s1, m1, …)。分数 = int64(computeNodeLoadScore(...)),与 go-zero Zadd 的 int64 截断一致。批与批之间也判 isLeader。逐条检查错误,出错打英文 Errorf(带 zone),outcome 记 write_failed。
- ⑤ 对读成功的节点逐条目调用 metrics.ObserveNode。
- ⑥ metrics.ObserveLoadRefresh(outcome, 耗时, 节点读错误数)。outcome 优先级:write_failed > registry_failed > lost_leadership > partial > ok。

(e) fullSync:
- :658-660 的 `for _, entry := range next { updateNodeLoad(svcCtx, entry) }` 换成 `refreshLoadScores(svcCtx)`。
- 领导者分支(:688 起)在 sweepStaleLoadSetMembers 之前插入:Config.ZoneRegistryBootstrapScan 为真时调用 bootstrapZoneRegistryFromScan,出错只记日志。

(f) sweepStaleLoadSetMembers(:743-797):删掉 :748-766 的 SCAN 循环,改为 sweepZones = 快照 zone ∪ listRegisteredZones()。读失败时 Errorf,只扫快照里的 zone,与旧 SCAN 出错就 break 的退化方向一致。改写 :728-742 的注释,说明扫描范围和不变量。其余逻辑(removeNodeFromRedis / 降级即停)不动。

(g) 不改:handleWatchEvent 的结构;:927-931 PUT 换 zone 分支未按领导者门控(既有问题,不在本项);IsNodeAlive;推迟摘除与再入屏障的全部逻辑。
   工作区: git status 干净;mtime 2026-10-13 05:31;最近一次提交 e4ff1d165 只改了 :1068 一条日志。无冲突风险
- [go/scene_manager/internal/logic/orphan_cleanup.go] (a) CleanupOrphanWorldChannels(:42-90):
- 保留「World 表为空就拒绝」。
- 发现方式改为 zones := unionZones(listRegisteredZones(), GetActiveZones())。登记集合读失败时 Errorf,只用 GetActiveZones。
- 对每个 zone 调 svcCtx.Redis.Hkeys(worldDesiredChannelsKey(zone)):失败就 Errorf 并跳过该 zone;字段解析成 confId,不在 World 表里的调用 deleteOrphanChannel(ctx, svcCtx, zone, conf, worldChannelsKey(zone, conf))。
- 返回值语义不变:清掉的 (zone, conf) 组数。
- orphanScanBatch 不再使用就删除;orphanKeyPrefix 与 parseWorldChannelsKey 保留(logic_test.go:2522 在用)。

(b) deleteOrphanChannel(:117-219):把 :203 的 Hdel 期望数移到 :212 的 Del(setKey) 成功之后,作为最后一步。期望数字段就是新的发现索引,先删它再删集合,中途失败这组就永远找不到了。Hdel 失败时 Errorf 并返回 false,下次 fullSync 会对空集合重跑,幂等。屏障判定、Agones 归还等其余顺序不动。

(c) 改写文件头注释(:14-31):不再「Scan with a cursor」,改为按登记集合与期望数 Hash 点查。写明覆盖范围:唯一生产写入点 world_init.go:174→:224 先播种期望数再 SADD;HSETNX 出错的兜底路径(world_autoscale.go:89-91)和升级前的遗留数据不在覆盖范围内,后果只是残留垃圾键,不影响路由。
   工作区: git status 干净;mtime 2026-10-13 05:31。无冲突风险
- [go/scene_manager/internal/logic/world_autoscale.go] - DesiredWorldChannelCount(:73-99):命中已有字段的路径不变,不加命令;只在播种分支、Hsetnx 之前调 registerSceneManagerZones(zoneID)。失败时 Errorf 后继续:登记集合只服务清扫与孤儿清理,不能挡住补频道和进场景;领导者每拍还会对有节点的 zone 重新登记。
- setDesiredWorldChannelCount(:102-118):在 Hset 之前同样登记。
- 注释注明:这两处可能跑在任意副本上(CreateScene 兜底),登记集合只加,可以多写者。
   工作区: git status 干净;mtime 2026-10-12 03:53。无冲突风险
- [go/scene_manager/internal/metrics/metrics.go] - 包注释改为:每节点 gauge 只由领导者导出。
- 新增 loadRefreshSeconds:Histogram,Subsystem scene_manager,Name load_refresh_duration_seconds,无 label,buckets .01/.05/.1/.25/.5/1/2/5。
- 新增 loadRefreshTotal:CounterVec,label 只有 outcome。
- 新增 loadRefreshNodeErrorsTotal:Counter,无 label。
- 以上都在 register() 里注册。
- 常量 LoadRefreshOutcomeOK / Partial / RegistryFailed / WriteFailed / LostLeadership。
- func ObserveLoadRefresh(outcome string, d time.Duration, nodeErrors int)。
- func ResetNodeSeries():对 playerCount / sceneCount / loadScore 调 Reset;ResetLeaderGauges(:466-470)里也调用它。
- nodesByRole 的 Help 与 SetNodesByRole 注释改为「领导者发布 etcd 镜像里全部 zone,跟随者只发布本进程 ZoneId;两者都给本进程 ZoneId 补 0」。
- 不加 player_id 或 node 类新 label;zone_id 只出现在原有指标里。
   工作区: git status 干净;mtime 2026-10-12 03:53。无冲突风险
- [go/scene_manager/internal/config/config.go] 在 CleanupOrphanChannelsOnStartup 后面加 `ZoneRegistryBootstrapScan bool \`json:",optional"\``,默认 false。中文注释写明:
- 打开后,领导者每次 fullSync 都用全库 SCAN 把旧版本留下、此刻已没有活节点的 zone 补进 scene_manager:zones。
- 只在有历史数据的长期环境里、首轮滚动升级时打开一次,看到 [ZoneRegistry] bootstrap 日志后关掉。
- 开服规模下单次是小时级,生产不得常开。

CleanupOrphanChannelsOnStartup 的注释(:239-246)把「scans Redis」改为「按登记集合与期望数 Hash 点查」。
   工作区: git status 干净;mtime 2026-10-12 03:53。无冲突风险
- [go/scene_manager/etc/scene_manager_service.yaml] - 更新 :211-214 CleanupOrphanChannelsOnStartup 的注释(不再 SCAN)。
- 在其后加一段中文注释和注释掉的 `# ZoneRegistryBootstrapScan: false`,说明同 config.go。
- 不加生效键,K8s ConfigMap 不需要改。
   工作区: git status 干净;mtime 2026-10-12 03:53。tools/scripts/k8s_deploy.ps1 正被他人修改(M,mtime 2026-10-16 04:10),本项不碰它
- [go/scene_manager/internal/logic/load_refresh_scope_test.go(新建)] 能在 HEAD 上跑红的行为测试。只用 HEAD 已有的符号,登记集合一律写字面量 "scene_manager:zones"。

测试内建一个 helper installCommandRecorderForTest(t, mr, fail func(cmd string, args []string) string):用 mr.Server().SetPreHook 记录命令名和首个参数,忽略 HELLO / CLIENT / PING / AUTH / SELECT;fail 返回非空串时 WriteError;t.Cleanup 里卸掉 hook。用例清单见 tests 字段。

每个用例都要:clearKnownNodesForTest、resetNodeDeathQueuesForTest、t.Cleanup(SetLeaderCheckForTest(...)) 恢复;node / zone id 用 9xx 段,避免与全局 Prometheus 注册表里其他用例的序列混淆;显式设置 sc.Config.NodeLoadWeightSceneCount=1、NodeLoadWeightPlayerCount=0.01。
   工作区: 新文件
- [go/scene_manager/internal/logic/zone_registry_test.go(新建)] 会用到新符号的测试:键名钉死与不撞模式、listRegisteredZones 跳过非法成员、bootstrap 扫描。详见 tests 字段。
   工作区: 新文件
- [go/scene_manager/internal/logic/node_detach_deferred_test.go] 只改 TestSweepStaleLoadSetMembers_DefersInsteadOfZremOnDeathMarkFailure(:427 起):在给 otherZone=3 和 testZoneId 做 mr.ZAdd 之后,补 `mr.SAdd("scene_manager:zones", "1", "3")`,并加注释:生产路径先登记后加成员,这里直接 ZADD 绕过了登记,所以手工补上。断言不变。其余用例不动。
   工作区: git status 干净;mtime 2026-10-12 03:53。无冲突风险
- [tools/merge_zone/scene_hot_state.go] - 加常量 sceneManagerZonesKey = "scene_manager:zones",注释写「与 go/scene_manager/internal/logic/zone_registry.go 同值,跨 module 契约,两边测试各自钉死」。
- 加纯函数 sourceZoneRegistryMember(src uint32) string。
- clearSourceZoneHotState 在固定键删除循环(:475-492)之后加最后一步:SIsMember 判断是否登记;已登记则 rep.ZoneRegistryFound=1,非 dry-run 时 SRem,并把移除数记入 rep.ZoneRegistryRemoved。
- 必须最后做:前面任何一步失败都直接返回、保留登记,scene_manager 的清扫仍能找到这个 zone。
- hotStateClearReport 加两个字段,String() 末尾追加 zone_registry(found=%d removed=%d)。
- 头注释(:20-40)步骤列表加「5. 从 scene_manager:zones 摘掉 S」。
   工作区: git status 干净;mtime 2026-10-12 03:53。无冲突风险
- [tools/merge_zone/scene_hot_state_test.go] 新增 TestSourceZoneRegistryMember:sceneManagerZonesKey == "scene_manager:zones",sourceZoneRegistryMember(102) == "102"。纯函数测试,本 module 不引入 miniredis。
   工作区: git status 干净;mtime 2026-10-12 03:53
- [tools/merge_zone/integration_test.go(带 merge_integration 构建标签)] 新增 TestIT_ClearHotState_RemovesZoneFromSceneManagerRegistry:
- itReset 后,先在 itSceneRD 上 SADD scene_manager:zones itSrcZone 和 999。
- dry-run:断言仍是成员,rep.ZoneRegistryFound==1、ZoneRegistryRemoved==0。
- 正式执行:断言 itSrcZone 已不是成员、999 仍是成员、ZoneRegistryRemoved==1。
   工作区: git status 干净;mtime 2026-10-15 14:35(他人已提交)。只追加新用例,不改已有用例
- [deploy/k8s/scene-manager-alerts.yaml] (a) 更新两条 PoolEmpty 上方注释(:31-54):发布者从「每个 SM 都发全部 zone」改为「领导者发全部 zone,各 zone 自己的 SM 给本 zone 补 0」。表达式不变,它们只做 >0 / ==0 判定。

(b) 新增两条 warning,阈值都标「待压测基线」:
- SceneManagerLoadRefreshFailing:`sum(increase(scene_manager_load_refresh_total{outcome=~"registry_failed|write_failed"}[10m])) > 0`,for 10m。
- SceneManagerLoadRefreshSlow:`histogram_quantile(0.99, sum by (le) (rate(scene_manager_load_refresh_duration_seconds_bucket[10m]))) > 2`,for 15m。

两条都写 description 与处置:查 Redis、看 [LoadRefresh] 日志。
   工作区: git status 干净;mtime 2026-10-12 03:53
- [deploy/k8s/scene-manager-dashboard.json] 把 :92 的 `sum by (zone_id, role) (scene_manager_nodes_by_role…)` 改成 `max by (zone_id, role)`。现在每个 zone 有 2~3 个发布者(领导者 + 本 zone 副本),sum 会按发布者个数放大;旧写法本来就按 2Z 放大。:108 / :124 的 topk 不用改:序列从每个副本一份变成只有领导者一份,结果反而去重了。
   工作区: git status 干净;mtime 2026-10-12 03:53
- [docs/design/scene-creation-architecture.md] 见 doc_updates 第 1 条。
   工作区: git status 干净;mtime 2026-10-15 04:32
- [docs/design/capacity-model-hok-scale.md] 见 doc_updates 第 2 条。只追加状态注,不改原推算。
   工作区: git status 干净;mtime 2026-10-15 14:38
- [docs/design/shared-redis-split-by-purpose.md] 见 doc_updates 第 3 条。
   工作区: git status 干净;mtime 2026-10-15 14:38
- [docs/PROGRESS.md] 落码后在末尾追加一条(只追加),内容见 doc_updates 第 4 条。
   工作区: 有他人未提交改动:git diff --stat 显示 +252 行,mtime 2026-10-16 04:04,写作时约 7 分钟前,其他会话正在追加。文件配置了 merge=union;追加前必须重读末尾,用 Edit 只在文件末尾追加,不回头改旧条目
- [docs/handoff/handoff-launch-readiness-and-d1-20261013.md] 在文首进度段末尾(:28「2026-10-15 晚,续」一段之后)追加一行:「2026-10-16:§4 第 5 项中的 SM 负载刷新只读本 zone 已落码(未编译、未测试),见 PROGRESS 同日条目」。
   工作区: 有他人未提交改动:+2 行,即 :28 那段,mtime 2026-10-15 22:15。只在其后追加,不改那两行
- [D:\luyuan\xuanming-server-mmo\PARITY.md(Java 仓库)] 末尾追加一行(内容见 java_parity),只碰这个文件。
   工作区: Java 仓库里 PARITY.md 有他人未提交改动(M),只在末尾追加
## compat_and_rollout
不涉及 proto、客户端可见契约、C++ 代码和 go.mod / go.sum;Redis 键名格式不变。新增一个只加不减的 SET:scene_manager:zones。

1. 滚动升级时新旧版本并存:
   - 旧领导者 + 新跟随者:旧领导者照旧全量刷新,fullSync 照旧 SCAN;新跟随者不再读 Redis,总负载只会下降。
   - 新领导者 + 旧跟随者:新领导者按 zone 批量刷新并登记;旧跟随者照旧全量读,但不写,写入仍然只有一个来源。
   - 领导权在新旧之间来回切:旧版本不认识登记集合,继续用 SCAN;新版本用登记集合加快照 zone。快照 zone 每拍都会被新领导者登记。
2. 分数写入仍由单一写者完成,PUT / DELETE / 清扫的先后语义不变;读失败从「按 0 写」改为「保留旧分」,更保守。
3. 只有一种旧数据新版本看不到:升级前就已没有活节点、负载集里还留着陈旧成员的 zone。它不在快照里,也不在登记集合里,新的清扫找不到。
   - 生产在开服前上线这一版,没有这种数据。
   - 长期的 dev / staging 环境:首轮滚动时把 ZoneRegistryBootstrapScan 设为 true,让领导者 fullSync 扫一次;看到日志后改回 false。
   - 本机按 AGENTS §6.2 压测前会 FLUSHALL,不需要。
   - 这一步写进 scene-creation-architecture.md 的新节。
4. 孤儿清理改为按期望数 Hash 发现。它能比 SCAN 多发现「频道集合已空、期望数字段还在」的残留;但「有集合、没有期望数字段」的组合(HSETNX 出错兜底或更早的遗留)不再被发现,后果只是残留垃圾,不影响路由。删除顺序改为期望数最后删,中途失败下次 fullSync 还能找到。
5. 回滚到旧二进制:旧代码不读登记集合,照旧 SCAN,功能完全回到原样。留下的 scene_manager:zones 键无害,可以手工 DEL。期望数字段删除顺序对旧代码没有影响。
6. 指标与看板语义的变化:
   - 每节点 gauge 只由领导者导出,切主时序列从一个 pod 换到另一个 pod,中间最多空缺一个锁 TTL(30s)。告警用的是 max / stddev / avg / >0 / ==0,不受影响。
   - 看板的 sum by 改为 max by。
   - 降级时 ResetLeaderGauges 加上跟随者每拍 ResetNodeSeries,保证不残留旧序列。
7. 领导者缺位(最长约 30s)时没有人刷新分数。这与现状相同(现在的写入也只由领导者做)。
8. 部署脚本和 ConfigMap 不需要改;新配置项可选,默认 false。不要在开服后的生产上打开它。
## tests
- 【load_refresh_scope_test.go,能在 HEAD 上跑红】TestRefreshLoadScores_FollowerIssuesNoRedisCommands:
- 跟随者(SetLeaderCheckForTest 返回 false)。knownNodes:zone1 有 910、911,zone2 有 920;计数键已种。
- 装上命令记录器,依次调用 refreshLoadScores(sc) 和 updateNodeLoad(sc, zone1 的 910)。
- 断言:没有记录到任何业务命令;两个 zone 的负载集都没有成员。
- 修复前红:跟随者每个节点会发 2 个 GET。
- TestRefreshLoadScores_FollowerPublishesOnlyOwnZone:
- 跟随者,sc.Config.ZoneId=2,节点同上。
- 断言:scene_manager_nodes_by_role 只有 "2/*" 序列,没有 "1/*";scene_manager_node_load_score 里没有 node_id 为 910 / 911 / 920 的序列。
- 修复前红:跟随者会发布全部 zone 和每节点序列。
- TestRefreshLoadScores_LeaderDemotionDropsNodeSeries:
- 领导者刷新一次,断言 node 930 的 node_load_score 序列存在。
- 切成跟随者再刷新一次,断言该序列消失。
- 修复前红。
- TestRefreshLoadScores_LeaderRefreshesEveryZoneAndHealsMembership:
- 领导者。zone1 节点 940(scene_count=2,player_count=300),zone2 节点 950(scene_count=1,player_count=0);两个负载集初始为空,模拟 Redis 丢了成员。
- 刷新后断言:ZSCORE 940 = 5、950 = 1;类型镜像都存在;SMEMBERS scene_manager:zones 包含 1 和 2。
- 修复前:分数和自愈断言是绿的(回归护栏),登记断言是红的。
- TestRefreshLoadScores_ReadErrorKeepsPreviousScore:
- 领导者。负载集预置 960=100、961=100;计数 960 的 scene_count=4,961 的 scene_count=7。
- 记录器对 `GET node:zone:1:960:player_count` 返回 ERR。
- 刷新后断言:960 仍是 100,961 是 7。
- 修复前红:旧代码读错当 0,会把 960 写成 4。
- TestRefreshLoadScores_RegistryWriteFailureOnlyUpdatesExistingMembers:
- 领导者。记录器对 `SADD scene_manager:zones` 返回 ERR。负载集预置 970=100(scene_count=3);节点 971 在 knownNodes 里,但不在负载集里。
- 刷新后断言:970 的分数是 3;971 不在负载集里。
- 修复前红:旧代码没有登记这一步,会把 971 加进负载集。
- TestRefreshLoadScores_LeaderWritesOneZaddPerZone:
- 领导者。2 个 zone,每个 zone 3 个节点。
- 断言:记录到的 ZADD 恰好 2 条(每个 zone 一条),GET 恰好 12 条,SET 恰好 6 条,SADD 恰好 1 条。
- 钉住批量写的形状;修复前红:旧代码是 6 条 ZADD,没有 SADD。
- TestSweepStaleLoadSetMembers_UsesZoneRegistryNotScan:
- 领导者。记录器对所有 SCAN 返回 ERR 并记录。
- zone 3 的负载集有成员 30,并已 SADD 登记 "3";seen 里只有 zone1;调用 sweepStaleLoadSetMembers(sc, seen, []uint32{1})。
- 断言:node:zone:3:30:death_at 已写入,30 已摘出负载集,记录里没有 SCAN。
- 修复前红:SCAN 出错后 zone 3 不在扫描范围内。
- TestCleanupOrphanWorldChannels_DiscoversViaRegistryWithoutScan:
- 用 newAutoscaleCtx,记录器对 SCAN 返回 ERR。
- seedChannels(zone 5, conf 777, {7701:0}),它内部经 setDesired 完成登记;World 表里只有 autoscaleTestConfID。
- 断言:返回 1;world_channels:zone:5:777 和期望数字段 777 都已删除;没有 SCAN。
- 修复前红:SCAN 出错,返回 0。
- TestDeleteOrphanChannel_KeepsDesiredFieldUntilSetDeleted:
- 记录器对 `DEL world_channels:zone:5:777` 返回 ERR。
- 第一轮断言:返回 0,期望数字段仍在。
- 卸掉注入后第二轮断言:返回 1,字段已删。
- 修复前红:旧顺序先 HDEL,第一轮后字段已经没了。
- TestDesiredWorldChannelCount_SeedRegistersZone:
- DesiredWorldChannelCount(sc, 8, conf) 和 setDesiredWorldChannelCount(sc, 9, conf, 2) 之后,SISMEMBER scene_manager:zones 的 8 和 9 都为真。
- 再调一次 DesiredWorldChannelCount(8, conf),命中已有字段,记录器断言没有 SADD。
- 修复前红。
- 【zone_registry_test.go,用到新符号】TestSceneManagerZonesKey_PinnedAndOutsideLegacyPatterns:
- sceneManagerZonesKey == "scene_manager:zones"。
- 它不以 scene_nodes:zone: / world_channels:zone: / world_channels:desired:zone: / node:zone: 开头,不会被旧版本 SCAN 和 merge_zone 的模式命中。
- TestListRegisteredZones_SkipsMalformedMembers:SADD "2"、"abc"、"4294967296"、"1" 后,返回 [1 2] 且没有错误。
- TestBootstrapZoneRegistryFromScan_RegistersLegacyZones:
- 预置 scene_nodes:zone:12:load 的成员和 world_channels:desired:zone:13 的字段,不做登记。
- bootstrapZoneRegistryFromScan 返回 2,登记集合为 {12, 13}。
- 再调一次幂等,集合不变。
- 既有用例的调整:node_detach_deferred_test.go 的 TestSweepStaleLoadSetMembers_DefersInsteadOfZremOnDeathMarkFailure 补登记 zone 1 和 3,断言不变。以下既有用例不改,但必须仍然通过:
- nodes_by_role_metrics_test.go:TestRefreshLoadScores_ZeroFillsConfiguredZoneAndEmptyRoles(测试默认就是领导者)
- logic_test.go:TestNodeRedisStateIsScopedByZoneAndNodeID
- orphan_cleanup_autoscale_test.go 的三条用例
- world_channel_directory_test.go:TestOrphanCleanup_DropsLineNoTableAndDirectory
- logic_test.go:TestCleanupOrphanWorldChannels_RefusesEmptyConfTable
- node_detach_deferred_test.go:TestOrphanCleanup_SkipsGroupWhileOwnerDetachDeferred
- merge_zone 单测 TestSourceZoneRegistryMember:钉死键名和成员格式。
- merge_zone 集成测试(merge_integration 标签)TestIT_ClearHotState_RemovesZoneFromSceneManagerRegistry:dry-run 保留登记,正式执行摘掉源 zone,不碰 999。
- 所有用例都不依赖墙钟:不 sleep,不比较时间;直方图只确认被观测过,不断言具体数值。全局状态(knownNodes、推迟队列、领导权闸门、Prometheus 默认注册表)都在 t.Cleanup 里恢复;节点 id 用 9xx 段,与其他用例隔离。
## verify_commands
- 0. 前置:所有命令在 PowerShell 中执行,先设 `$env:GOTOOLCHAIN='local'; $env:GOPROXY='off'`。全程不改 go.mod / go.sum。命令失败后不要连续重试,保留失败用例名和第一条断言信息再汇报(AGENTS §10.2)。
- 1. 格式与静态检查。工作目录:D:\luyuan\xuanming-server-mmo-cpp\go\scene_manager。
命令:`gofmt -l internal etc`、`go vet ./...`。
通过标准:gofmt 没有输出,vet 退出码为 0。
- 2. 在 HEAD 上跑红:
- `git -C D:\luyuan\xuanming-server-mmo-cpp worktree add $env:TEMP\sm-red-loadrefresh HEAD`
- 只把工作区的 go\scene_manager\internal\logic\load_refresh_scope_test.go 复制到该 worktree 的同一路径。
- 在 $env:TEMP\sm-red-loadrefresh\go\scene_manager 下执行 `go test ./internal/logic/ -run 'TestRefreshLoadScores_Follower|TestRefreshLoadScores_Leader|TestRefreshLoadScores_ReadError|TestRefreshLoadScores_RegistryWrite|TestSweepStaleLoadSetMembers_UsesZoneRegistryNotScan|TestCleanupOrphanWorldChannels_DiscoversViaRegistryWithoutScan|TestDeleteOrphanChannel_KeepsDesiredFieldUntilSetDeleted|TestDesiredWorldChannelCount_SeedRegistersZone' -count=1 -v`
通过标准:
- 能编译(编译失败说明测试引用了新符号,应退回改测试)。
- 除 LeaderRefreshesEveryZoneAndHealsMembership 外全部 FAIL;该用例只在登记断言上 FAIL。
- 结束后执行 `git -C D:\luyuan\xuanming-server-mmo-cpp worktree remove --force $env:TEMP\sm-red-loadrefresh`。
- 3. 跑绿。工作目录:go\scene_manager。
命令:`go test ./internal/logic/ -run 'TestRefreshLoadScores|TestUpdateNodeLoad|TestSweepStaleLoadSetMembers|TestCleanupOrphanWorldChannels|TestOrphanCleanup|TestDeleteOrphanChannel|TestDesiredWorldChannelCount|TestSceneManagerZonesKey|TestListRegisteredZones|TestBootstrapZoneRegistryFromScan|TestNodeRedisStateIsScopedByZoneAndNodeID' -count=10`
通过标准:10 轮全部 PASS,用来查全局状态造成的不稳定。
- 4. 全量。工作目录:go\scene_manager。
命令:`go test ./... -count=1`。
通过标准:退出码为 0。失败时保留失败用例名与断言摘要。
- 5. merge_zone。工作目录:D:\luyuan\xuanming-server-mmo-cpp\tools\merge_zone。
命令:`gofmt -l .`、`go vet ./...`、`go test ./... -count=1`。
通过标准:全部为 0 或无输出。
集成测试:只有在 integration_test.go 头部要求的 MySQL / Redis 环境可用时才跑 `go test -tags merge_integration -run 'TestIT_ClearHotState' ./... -count=1`;没有环境就如实报告「未运行」。
- 6. 配置文件解析。工作目录:仓库根。
- `python -c "import json; json.load(open('deploy/k8s/scene-manager-dashboard.json', encoding='utf-8'))"` 应退出 0。
- `python -c "import yaml; list(yaml.safe_load_all(open('deploy/k8s/scene-manager-alerts.yaml', encoding='utf-8')))"` 应退出 0。没有 PyYAML 时报告未验证,不要安装。
- 7. 文档索引。仓库根执行 `python tools/scripts/gen_docs_index.py --check`,退出码应为 0(本项不新增文档)。
- 8. 改动范围核对。仓库根执行 `git diff --name-only -- go/scene_manager tools/merge_zone deploy/k8s docs`。
通过标准:只出现 plan 里列出的文件,外加他人原有的 go/scene_manager/generated/pb/game/message_id.go;`git diff --name-only -- go/scene_manager/go.mod go/scene_manager/go.sum tools/merge_zone/go.mod tools/merge_zone/go.sum` 没有输出。
- 9. 性能:本项不做性能结论。要量化收益,另按 AGENTS §6 跑多 zone 压测,用 stress_summarize 前后对比。在那之前,交付说明里只能写「公式推算,未实测」。
## doc_updates
- docs/design/scene-creation-architecture.md:
- :720-735 的 Redis 键表加一行:`scene_manager:zones` | Set | 出现过负载集成员或频道期望数的 zone(只加不减;merge_zone 摘除)。
- :603-623 的 list-watch 图:把「5s ticker → refreshLoadScores()」改为「领导者按 zone 批量刷新;跟随者只发本 zone 的 nodes_by_role」。
- 新增一节「负载刷新与 zone 登记集合(2026-10-16)」,内容包括:
  - 为什么不按副本刷新本 zone(单写者,load_reporter.go:235-239);
  - 批量与读失败语义;
  - 登记集合的不变量、写入方、读取方、删除方和自愈;
  - 孤儿清理按期望数 Hash 发现,以及覆盖不到的情况;
  - 新指标与告警;
  - 升级 runbook:长期环境首轮滚动打开 ZoneRegistryBootstrapScan 一次;
  - A1 剩余缺口:每 zone 选主、watch 收窄、scene 只连本 zone SM、rebalance / 分线目录 / 闲置回收 / 自动伸缩 / Agones 对账这些领导者循环仍遍历全部 zone。
- docs/design/capacity-model-hok-scale.md,只追加状态注:
- §2.2 的「scene_manager 负载刷新」行(:217):注明落码后的公式(仍按 §7.2 口径,未实测)。读为 0.4·N_s GET/s,2000 万在线时约 711~3,556;每节点序列 3·N_s;nodes_by_role 约 12·Z;跟随者为 0;领导者每拍约 7~29 次往返。
- §3 A0-4(:416)「scene_manager fullSync 的 SCAN」:标「已落码,未验证,见 PROGRESS 2026-10-16」。
- §3 A1(:437):注明「负载刷新已改为领导者按 zone 批量,单写者保留;按 zone 选主与 watch 收窄仍待」。
- §4.2 第 0 批(:664):同步状态。
- docs/design/shared-redis-split-by-purpose.md:
- :137 场景路由那一行的键清单补上 `scene_manager:zones`。
- 在该表下方加一行注:G4a「第 0 批」里 SM 那一项已按本项完成(未验证)。
- 完整的「第 0 批」章节和按 S3 / 2000 万的修订仍归交接 §4 第 5 项,本项不展开。
- docs/PROGRESS.md 末尾追加一条(只追加,先重读末尾),标题「2026-10-16 scene_manager 负载刷新只读本 zone:领导者按 zone 批量刷新 + zone 登记集合替代 SCAN(Claude,未编译、未测试,待 Codex 验证)」。内容:
- 改动文件清单;
- 设计取舍与偏离 A1 字面的理由;
- 兼容与升级步骤;
- 交给 Codex 的命令(即本规格 verify_commands);
- Java 状态:不适用,Java 已是按 zone 的目录与登记集合,PARITY 已登记;
- 明确写「容量数字为公式推算,未压测」。
- docs/handoff/handoff-launch-readiness-and-d1-20261013.md:文首进度段末尾追加一行,指向上面的 PROGRESS 条目。
- deploy/k8s/scene-manager-alerts.yaml:PoolEmpty 的注释随代码一起更新(见 plan)。
- docs/README.md 不需要改:没有新增文档。
## java_parity
不需要对等改动,只在 Java 仓库 PARITY.md 末尾追加一行登记「不适用」。依据(都已亲自打开):
- Java 的 scene 节点每 5 s 自报到按 (类型, zone) 分的目录 game:nodes:{type}:{zone}(RMapCache,条目带 TTL),见 RedisKeys.java:66-68。
- SM 只按 zone 点读,见 RedisSceneNodeSource.java:15-27、NodeDirectory.java:80-93。
- 世界频道有 zone 登记集合 worldZones() 和每 zone 一个领导者,见 RedissonWorldChannelStore.java:209-226、:273-285。
- game-scene-manager 里没有 SCAN。

本项只改服务端内部的 Redis 访问方式,不涉及客户端帧、proto、tip 码或配置表,不属于 AGENTS §12 要求两版同批修改的共享契约。

建议登记的行:| scene_manager 负载刷新只读本 zone / zone 登记集合替代 SCAN | go/scene_manager/internal/logic/{load_reporter,zone_registry,orphan_cleanup,world_autoscale}.go、tools/merge_zone/scene_hot_state.go | <落码提交号> | game-scene-manager、game-discovery | 0.1.0-SNAPSHOT | 不适用(Java 已按 zone 节点目录点读、世界频道 zone 登记集合 + 每 zone 领导者,无 SCAN) |
## risks
- 仍是全服一个领导者。分数刷新的代价从 0.8·Z·N_s 降到 0.4·N_s,但依然由单点承担,领导者缺位约 30s 内没有刷新(与现状相同)。rebalance、分线目录、闲置回收、自动伸缩、Agones 对账这些领导者循环仍遍历全部 zone;全局单点交接 :187 推算分线目录单拍 1.6~4.7s。这些归 A1「每 zone 选主」,不在本项,且要按交接 §4.3 的 C6 把循环和 watch 一起收窄。
- etcd watch 和 knownNodes 仍是全服范围(数据面需要),fullSync 时的全前缀 Range 与 resolveNodeEndpoint 的慢路径全前缀 Get(scene_node_client.go:234)不在本项。要等 C++ 改成只连本 zone 的 SM(A1)之后才能收窄。
- 登记集合不变量依赖写入方「先 SADD 后 ZADD」。升级前就已整体下线、负载集里还留着成员的 zone,需要运维打开一次 ZoneRegistryBootstrapScan,否则新清扫看不到它:这些死节点会一直按存活处理,这个 zone 里玩家的过期位置判定会保持 fail-closed(换手被拒)。缓解:开服前上线;长期环境在 runbook 里写明;开关默认关闭。
- ZoneRegistryBootstrapScan 如果在开服后的生产上常开,每次领导者 fullSync 都会全库 SCAN(小时级),等于把老问题带回来。yaml 与 config 注释已明确禁止,但没有机械拦截。可以考虑由 k8s_deploy.ps1 在 release 档拒绝该键;该脚本正被他人修改,本项不碰。
- 孤儿清理只能发现带期望数字段的频道组。HSETNX 出错兜底(world_autoscale.go:89-91)或更早遗留的「有集合、没有期望数字段」组合不会被清,后果是残留垃圾键,不影响路由。
- 读失败语义从「按 0 写」改为「保留旧分」。Redis 部分故障期间分数会更久不更新;已经通过 load_refresh_total{outcome} 与节点读错误计数暴露,并新增两条告警,阈值待压测校准。
- 每节点指标只由领导者导出,切主时序列换 pod;外部如果有按 pod 求和的看板或报表,数值会变(过去被 2Z 倍放大)。仓内只有 scene-manager-dashboard.json:92 用了 sum,已改为 max。
- 容量数字(Z、N_s、每拍往返次数、每批 1024 条)都是公式推算,没有实测(AGENTS §6)。pipeline 每批的大小对其他客户端尾延迟的影响没有测过。
- load_reporter.go:927-931 的 PUT 换 zone 分支在任意副本上 ZREM / DEL,没有按领导者门控,是既有的第二写者问题,与已登记的 reassignSceneNode 无 CAS 同类,本项不修。
- PROGRESS.md 与 launch-readiness 交接有他人并行追加或未提交改动,落文档时必须先重读再只追加。k8s_deploy.ps1 正被他人修改(mtime 2026-10-16 04:10),本项刻意不碰它。
## open_decisions

# VERDICT
## overall
规格把问题定位对了:跟随者白读,两处全库 SCAN,登记集合替代 SCAN 的方向也对。current_facts 绝大部分我逐行核对属实,只有几处行号和文件名引用有误,见 nit 第 1 条。不采用字面「每副本只刷本 zone」、保留单写者的取舍,有 load_reporter.go:235-239 支撑,能成立。Java 不需要对等改动的判断也成立。工作区方面:规格计划修改的 go/scene_manager 与 merge_zone 文件全部干净;有他人未提交改动的是 PROGRESS(+344 行,仍在追加)、launch-readiness 交接(+2)、k8s_deploy.ps1(+962,本项不碰)和 Java PARITY.md,规格对这些的处置正确。

但失败处理有两处 must 级错误,按原样落码会引入新的数据一致性风险,不能直接交付:

1. 「pipeline 返回错误不作判据、空串视为 0」会把 go-zero 熔断拒绝的批次读成全 0,重新造出「按 0 写分」,并把 outcome 记成 ok。
2. 「读失败或登记失败就不新增成员 / 改用 ZADD XX」在 Redis 部分故障时,会让活节点留在负载集外。IsNodeAlive 与 playerLocationOwnerGone 会据此判死,甚至判定整 zone 已死,与仓库一贯的 fail-closed 方向相反。

修完这两条后再处理 should 级问题:补 ops 文档与几处注释;把 bootstrap 开关改为 runbook 一次性命令或自熄标记;补「刷新停摆」告警;先红基线钉 SHA;文档状态不要写成「第 0 批已完成」。处理完即可交给实现者落码。

以上结论全部基于静态阅读,未运行任何编译或测试。
## fact_checks
- [confirmed] scene_manager 按 zone 部署、每 zone 2 副本,但选主锁默认全局一把,全服只出一个领导者
   本人打开核对:go/scene_manager/scene_manager_service.go:94-97(LeaderLockKey 为空时用 scene_manager:leader:lock);internal/config/config.go:78-80;deploy/k8s/manifests/go-svc/scene-manager.yaml:24(replicas: 2);tools/scripts/k8s_deploy.ps1 现在 :1780 才是 Scope='zone',ConfigMap 模板在 :6119-6175,ZoneId: ${CurrentZoneId} 在 :6145 一带。行号又漂了,文件仍在被他人修改(M,mtime 2026-10-16 04:18)。另:config.go:82-88 的 LeaderEligible=false 金丝雀副本永不当选,新设计下它不做任何刷新,这一点规格没写,但不影响结论
- [confirmed] SM 数据面是全局的:C++ 在全服所有 SM 实例中按 playerId 挑一个;SceneManagerNodeService 不在 zone 限域类型里
   cpp/libs/engine/core/network/node_utils.cpp:267-310(GetSceneManagerEntity 遍历整个 SceneManager 注册表);cpp/libs/engine/core/node/system/node/node_util.cpp:115-131(只有 Gate / Scene / Login / PlayerLocator)。规格说的「该 .cpp 有他人未提交改动」指错了文件:node_util.cpp 是干净的,有改动的是 network/node_utils.cpp。那份 diff 是 muduo_grpc 通道迁移(+84 行),不涉及按 zone 选 SM,前提不变
- [confirmed] 每个副本 watch 全服 scene 节点,前缀不带 zone;C++ 键格式为 SceneNodeService.rpc/zone/<z>/node_type/<t>/node_id/<id>
   load_reporter.go:32、:630(fullSync 全前缀 Get)、:806(全前缀 Watch);cpp/libs/engine/core/node/system/etcd/etcd_manager.cpp:47-58
- [confirmed] 5s 一拍;每个副本对全部节点串行 GET 两个计数,读不分领导者;写只由领导者做;fullSync 时每个副本对全部节点再做一遍;refreshLoadScores 持 RLock 做 I/O
   load_reporter.go:31、:230-256(:231-232 读,:240 isLeader 才写,:254 每个副本都 ObserveNode)、:658-660、:834-841、:1001-1020。补充:knownNodes 的写方(:557/:569 测试辅助、:653、:896、:958)都在 load reporter 这一条 goroutine 上,所以把 I/O 挪出锁外是安全的
- [confirmed] 只能由领导者写的理由已写在 load_reporter.go:235-239 的注释里
   load_reporter.go:235-239 原文就是这一理由。但 leader_gate.go:5-9 的包注释仍写「负载分刷新每个副本照常跑……幂等同值写」,与 :235-239 矛盾。落码后会更不准,规格漏了这处注释
- [confirmed] 负载集与计数的写入方都是 Go 的 SM;C++ 不写任何 scene_nodes / node:zone 键;ZADD 只在 :242,ZREM 在 :354 与 :929
   git grep 'scene_nodes:|node:zone:|world_channels:' 在 cpp/(不含 generated)下 0 命中;go/scene_manager 非测试代码里 nodeLoadKey 的 ZADD 只在 load_reporter.go:242,ZREM 在 :354 和 :929。抽查计数写入点 createscenelogic.go:426、enterscenelogic.go:696、world_rebalance.go:274-278,都是 INCR / DECR
- [confirmed] 全库 SCAN 一共两处,都在领导者 fullSync 里;换主后 SCAN 数十亿键约 4~10 小时
   两处 SCAN:load_reporter.go:750(COUNT 64)和 orphan_cleanup.go:56(COUNT 256)。调用都在 :688-721 的领导者分支。git grep 在 go/scene_manager 生产代码里再无别的 Scan / KEYS。「数十亿键、4~10 小时」只是 handoff-global-singletons-10m-ccu-20261013.md:187 的转引,数值本身未核实
- [confirmed] 孤儿清理可以改用期望数 Hash 做索引:往频道集合 SADD 的唯一地方先经 DesiredWorldChannelCount 播种;deleteOrphanChannel 现在先 HDEL 后 DEL
   world_init.go:174 先调 DesiredWorldChannelCount,:224 才 SADD,生产代码里没有别的 SADD 进 world_channels:zone:{z}:{c}(Lua 里也没有);world_autoscale.go:86-90 在 HSETNX 出错时直接返回种子值、不写字段;orphan_cleanup.go:203 的 Hdel 早于 :212 的 Del。现有孤儿用例都经 seedChannels → setDesiredWorldChannelCount(world_autoscale_test.go:45-56)铺数据,改成按索引发现后仍能找到
- [confirmed] 跨 zone 读者全部是按 zone 点查;go/match 不读这些键
   enterscenelogic.go:953 是 ZCARD,:1009 是 SCARD;load_reporter.go:1059 是 ZSCORE;node_selection.go:38 是单 zone ZRANGE;go/match 0 命中。规格漏看了一点:enterscenelogic.go:929-969 的 playerLocationOwnerGone 把「ZCARD==0」当作整 zone 已死的破坏性证据,world_rebalance.go:173 把 ZSCORE 当作存活依据。负载集成员资格本身就是安全信号,见 plan_problems 第 2 条
- [confirmed] 指标现状:三个每节点 gauge 由每个副本发布;nodes_by_role 每个副本发布全部 zone 并给本 zone 补 0;ResetLeaderGauges 不清每节点序列;看板对 nodes_by_role 用 sum by
   metrics.go:35-56、:462-470(只 Reset agonesCounterDrift / rebalancePending)、:694-713、:737-766;scene-manager-dashboard.json:92 是 sum by (zone_id, role),模板变量 :45/:52 用 label_values(scene_manager_nodes_by_role, zone_id);alerts.yaml:55-100 是 PoolEmpty 两条,:103-135 是 max / stddev / avg
- [confirmed] merge_zone 清源 zone 的负载集与期望数,不认识新登记集合;独立 module、没有 miniredis;有 merge_integration 标签的集成测试
   tools/merge_zone/scene_hot_state.go:64-79(常量)、:318-337、:355-497(clearSourceZoneHotState,固定键删除在 :475-494);scene_hot_state_test.go:3-4 的注释写明不引 miniredis;go.mod 无 miniredis;integration_test.go:1 有 //go:build merge_integration;itReset 对三个 DB 做 FlushDB(:259-262)
- [confirmed] 测试设施:miniredis v2.37 的 SCAN 忽略 COUNT;SetPreHook 收到的命令名是大写;仓内有用它注入故障的先例;go-redis pipeline 把首个错误(含 redis.Nil)作为返回值
   miniredis v2.37.0 cmd_generic.go:653、server/server.go:204-213、cmd_sorted_set.go:73(ZADD 支持 XX);node_detach_deferred_test.go:64-90。另注意 :60-63 写明 SetError 与 SetPreHook 共用同一个钩子槽位,每个 svcCtx 注入的失败要少于 5 次,否则会触发 go-zero 熔断;go-redis v9.17.3 command.go:106-113
- [refuted] (由规格推出)PipelinedCtx 的返回错误不作判据,逐条判断即可;GET 返回 redis.Nil 或空串都视为 (0, true)
   go-zero v1.10.0 的熔断挂在 pipeline 的 hook 上,见 breakerhook.go:35-41:熔断打开时直接返回 ErrServiceUnavailable,不调用 next。go-redis v9.17.3 pipeline.go:104-119 的 Exec 只把 hook 的错误作为返回值,不回填到各条命令,见 redis.go:1182-1187。结果是被熔断拒绝的批次里,每条 StringCmd.Result() 都是 ("", nil)。按规格的「空串视为 0」,整批会被读成「全部计数为 0、读取成功」。如果随后的写批被熔断放行,就会把全部分数写成 0,正是本项要消除的「按 0 写入」;写批被拒时 cmd 的 err 也是 nil,outcome 会记成 ok
- [confirmed] 不必改 go.mod:go-redis 只是 indirect,用 go-zero 的 Pipeliner 就能逐条取结果
   go/scene_manager/go.mod:15(go-zero v1.10.0)、:64(go-redis v9.17.3 // indirect);go/data_service/internal/routing/player_name_cache.go:141、:177 是先例。修第 1 条要用到的 go-zero core/breaker.ErrServiceUnavailable 也属于直接依赖 go-zero,同样不需要改 go.mod
- [confirmed] Java 版已按 zone 设计,无需对等改动
   D:\luyuan\xuanming-server-mmo 下:RedisSceneNodeSource.java:15-27;NodeDirectory.java:80-93;RedisKeys.java:66-68(nodeDirectory);RedissonWorldChannelStore.java:209-226(每 zone 一个领导者)、:273-285(registerZone / zones())。game-scene-manager 里唯一的 scan 是 Dubbo 的 scanBasePackages。引用有一处错:RedisKeys.java:246-250 是 guildRankZones,worldZones() 实际在 :324-326。PARITY.md 有他人未提交改动(+15/-2,mtime 04:20)
- [confirmed] 2000 万在线下 Z=200~444、N_s=1,778~8,889;刷新读 0.8·Z·N_s;落码后为 0.4·N_s、序列 3·N_s、nodes_by_role 约 12Z
   capacity-model-hok-scale.md:47-50(C_z、c_scene 两个参数)、:217(S3 行 0.8·Z·N_s,N_s 指全服 scene 节点数),§7.2 实际在 :897-912。按这些参数复算:28.4 万~316 万 GET/s,每拍 3,556~17,778 次往返,序列 213 万~2,368 万,nodes_by_role 32 万~158 万,落码后读 711~3,556 GET/s。全部是公式推算,未实测
- [confirmed] already_done:本项没有被其他会话做掉
   PROGRESS.md 里 grep「scene_manager:zones / 负载刷新 / refreshLoadScores / zone 登记集合 / 只读本 zone」,10-13 之后没有命中。load_reporter.go 最近一次提交是 e4ff1d165。go/scene_manager 下只有 generated/pb/game/message_id.go 有未提交改动。handoff-launch-readiness-decisions-20261015.md 里 grep scene_manager / 负载 / SCAN / 第 0 批均无命中,没有与本项冲突的决定
- [refuted] 读失败时不写入、保留旧分,比现状「按 0 写入」更保守
   对已经在负载集里的成员确实更保守。对不在负载集里的节点,规格的写法是「也不新增成员」,登记失败时还会改成 ZADD XX,结果都是不进负载集。而 IsNodeAlive(load_reporter.go:1031-1070)把「不在负载集」当作唯一的已死证据;playerLocationOwnerGone(enterscenelogic.go:949-969)把 ZCARD==0 当作整 zone 已死、可以绕过交接门的证据。规格自己的自愈用例模拟的正是「Redis 丢了成员而节点还活着」。HEAD 的 readInt64 出错按 0 处理,至少能把成员加回去;新写法在这个场景下把未知状态推向破坏性一侧,与 AGENTS §11.3 和 IsNodeAlive 注释写明的 fail-closed 方向相反
- [refuted] 部署脚本和 ConfigMap 不需要改;长期环境首轮滚动时打开 ZoneRegistryBootstrapScan 一次即可
   K8s 的 scene-manager ConfigMap 由 tools/scripts/k8s_deploy.ps1:6119-6175 的模板整份生成,里面没有这个键,也没有对应参数。运维只能 kubectl 手改,下一次 k8s_deploy 就会被覆盖。runbook 写的「设为 true、看到日志后改回」在标准部署链路上做不到
## plan_problems
- [must] pipeline 的结果判定会把「熔断拒绝」当成「读取成功、计数为 0」。
- go-zero v1.10.0 的熔断挂在 ProcessPipelineHook 上(breakerhook.go:35-41)。熔断打开时直接返回 ErrServiceUnavailable,队列里的命令不执行,err 也不回填(go-redis pipeline.go:104-119、redis.go:1182-1187)。
- 规格 plan(d)② 规定「PipelinedCtx 的返回错误不作判据,逐条判断」,plan(b) 规定「空串视为 (0,true)」。
- Google SRE 熔断在 Redis 抖动时按比例丢弃请求,所以「读批被拒、写批被放行」是常见组合。这时领导者会把全服分数写成 0,正是本项要消除的问题;同时 ObserveNode 发布全 0,load_refresh_total 记 ok,看板和告警一起失明。
- 写批被拒时,各命令的 err 同样是 nil,也会被记成成功。
   修法: 在 refreshZoneLoadSets 里按批判定:
1. `err := PipelinedCtx(...)`。若 err 非 nil、不是 redis.Nil,并且满足以下任一条件,本批整体按失败处理:`errors.Is(err, breaker.ErrServiceUnavailable)`,或者本批没有任何一条 cmd.Err() 非空。读批失败的节点进「读失败」分支;写批失败记 write_failed。
2. 单条 GET 只有 redis.Nil 视为 (0,true)。("", nil) 一律视为未执行或值损坏,按读失败处理。
3. 为便于测试,把判定抽成纯函数 classifyCounterReply(val string, err, batchErr error) (int64, bool),表驱动覆盖以下组合:
   - ("", nil, ErrServiceUnavailable) → 失败
   - ("", nil, nil) → 失败
   - ("", redis.Nil, redis.Nil) → (0, true)
   - ("7", nil, someErr) → (7, true)
4. 再加一个 miniredis 用例:预置 `SET node:zone:1:x:scene_count ""`,断言旧分保留、outcome 为 partial。
5. outcome 增加 read_failed,表示整批读失败。
- [must] 读失败或登记失败时「不新增成员」(plan b / d 的 ZADD XX),在失败下把节点存活判定推向破坏性一侧。
- 负载集成员资格就是存活信号:
  - IsNodeAlive(load_reporter.go:1055-1070):redis.Nil 判死,未知按存活。
  - playerLocationOwnerGone(enterscenelogic.go:949-969):ZCARD==0 被当作整 zone 已死,可以绕过交接门。
  - world_rebalance.go:173:ZSCORE 被当作存活依据。
- 规格把自愈当作卖点(用例 LeaderRefreshesEveryZoneAndHealsMembership 模拟的就是 Redis 丢了成员)。但同一场景下,只要计数读失败(单条错误、WRONGTYPE、熔断部分拒绝),或 SADD scene_manager:zones 失败(例如该键被写成了别的类型),活节点就回不到负载集。
- 后果:IsNodeAlive 返回 false;全 zone 都回不来时 ZCARD==0,会在属主仍在存盘时放行改派或接管,即双写 / 回档。HEAD 的 readInt64 出错按 0 处理,至少能恢复成员资格。
- 登记失败的影响面也被放大了:登记键一旦持续出错,所有新节点永远进不了负载集。
   修法: 不能因为这两类错误扣下成员资格。
1. 读失败:对已在集里的成员保留旧分;对不在集里的节点发 `ZADD NX <哨兵分> member`。哨兵取足够大的常量(如 1e12)或本 zone 当前最大分加 1,让它最后才被选中;下一拍读成功后自然纠正。
2. 登记 SADD 失败:Errorf,记 outcome registry_failed,ZADD 照常写(不加 XX)。登记每拍都会重试;清扫范围是快照 ∪ 登记集合,活着的 zone 一定在快照里,剩下的缺口只有「从没登记成功、又在领导者缺位时整体死掉」的 zone,与 bootstrap 缺口同类。
3. 文件头的不变量随之改为「尽力登记 + 每拍自愈」。
4. 测试相应改:
   - RegistryWriteFailure 用例改为断言 970 被更新、971 被加入,outcome 为 registry_failed。
   - 新增「非成员读失败 → 以哨兵分加入」。
   - 新增「成员读失败 → 保留旧分」。
- [should] 漏改的文档与注释(AGENTS §5「没写文档 = 没说过」、§11.4)。
- docs/ops/scene-node-role-split.md:
  - §4.2(:171-203)写的是「多副本、以及别的 zone 的 scene_manager 也在发布同一 zone」,发布者语义要改;残余盲区第 2 条要改成「领导者卡住会压住全部 zone,跟随者卡住只压住本 zone」。
  - §6(:277-295)仍写「Scans via SCAN cursor in batches of 256 keys」。
- go/scene_manager/internal/logic/leader_gate.go:5-9 写「负载分刷新每个副本照常跑」,落码后明显不对。
- 以下几处把 SCAN 写成现行理由,应改成「旧版本 SCAN 与 merge_zone 的模式」:world_channel_directory.go:39-40、orphan_cleanup.go:205-208、world_channel_directory_test.go:725-726。
- nodes_by_role_metrics_test.go:3-4 的头注释。
- tools/merge_zone/main.go:311 的用法说明。
   修法: 把以上各处加进 plan 与 doc_updates,只改说明文字,不动断言。ops 文档 §6 改为:按登记集合与期望数 Hash 点查;写明覆盖不到的情形,以及首轮升级的步骤。
- [should] ZoneRegistryBootstrapScan 开关既操作不了,又容易踩坑。
- K8s ConfigMap 由 k8s_deploy.ps1:6119-6175 的模板整份生成,没有这个键;kubectl 手改的值下次部署就被覆盖。
- 忘了关时,领导者每次 fullSync 都会在 load reporter 的 goroutine 上做小时级全库 SCAN,watch、刷新和推迟摘除全部停摆,等于把老问题原样带回,而且没有机械拦截。
- 它只服务一个很窄的缺口:升级前已整体下线、负载集里还留着成员、之后再也不复活的 zone。这类 zone 复活时会进快照,下一次 fullSync 照样清扫。
   修法: 二选一。
(a) 首选:删掉这个开关和 bootstrapZoneRegistryFromScan,在 scene-creation-architecture.md 的升级 runbook 里给一条一次性命令:`redis-cli --scan --pattern 'scene_nodes:zone:*:load'` 和 `'world_channels:desired:zone:*'`,解析出 zone 后 `SADD scene_manager:zones`。生产代码里不再保留全库 SCAN。
(b) 保留代码,但做成自熄:完整扫完一轮后 `SET scene_manager:zones:bootstrapped 1 NX`,之后见到标记就跳过;扫描放到独立 goroutine,只做 SADD,不阻塞 watch。同时在 k8s_deploy.ps1 加参数并配契约用例(该文件正被他人大改,需要协调)。
- [should] 监控缺最关键的一种失效:刷新根本没在跑。
- 新设计下分数和每节点序列都只来自领导者。没有领导者(选举失败、全部 LeaderEligible=false),或领导者卡在 fullSync 的 3s 重试循环时,load_refresh_total 不增长,规格新增的两条告警一条都不会响。
- 每节点看板会整片变空。
- outcome 的 partial 和整批读失败都没有告警。
   修法: 1. 新增 SceneManagerLoadRefreshStalled:`sum(increase(scene_manager_load_refresh_total[2m])) == 0 or absent(scene_manager_load_refresh_total)`,for 5m,warning;description 写明「查 scene_manager_is_leader 与 fullSync 日志」。
2. Failing 告警的 outcome 集合加上 read_failed。
3. 阈值仍标「待压测基线」。
- [should] verify_commands 第 2 步在 `HEAD` 上做「先红」验证。工作区会被其他会话整区快照提交,Codex 执行时 HEAD 可能已经包含本项改动,「先红」会变成假绿,失去回归意义。
   修法: 落码前记下 `git rev-parse HEAD`(或在 PROGRESS 条目里写明基线 SHA),第 2 步的 worktree 用这个 SHA,不用 HEAD。如果已经被快照提交,改用落码提交的父提交。
- [should] 文档状态写过了头。doc_updates 要在 shared-redis-split-by-purpose.md 写「G4a 第 0 批里 SM 那一项已按本项完成」,并在容量模型 §4.2 同步状态。但字面目标「只读本 zone」(容量模型 :437 / :664,A1)并没有做到:领导者仍读全部 zone,watch 与 knownNodes 仍是全服。另外 shared-redis-split 里目前还没有「第 0 批」章节(grep 无命中),这条注会悬空。
   修法: 改写为:「SM 负载刷新:以『领导者按 zone 批量刷新 + 登记集合替代 SCAN』缓解(未编译、未测试);字面的『每副本只读本 zone』、按 zone 选主与 watch 收窄仍归 A1」。shared-redis-split 只补 :137 的键清单,第 0 批章节等交接 §4 第 5 项落地时再写。
- [nit] current_facts 有几处引用不准。
- Java 的 worldZones() 在 RedisKeys.java:324-326,规格引的 :246-250 是 guildRankZones。
- 有未提交改动的是 cpp/libs/engine/core/network/node_utils.cpp(muduo_grpc 迁移),不是 node_util.cpp。
- k8s_deploy.ps1 的 Scope='zone' 现在在 :1780,ConfigMap 模板在 :6119-6175。
- PROGRESS.md 的 diff 已涨到 +344 行(mtime 04:27),仍在被追加。
   修法: 更正引用。落文档前按函数名重新定位,PROGRESS 只在末尾追加。
- [nit] compat 第 6 条说「告警用的是 max / stddev / avg / >0 / ==0,不受影响」,这不完全对。每节点序列只来自领导者,换主时序列会空缺最长约锁 TTL 加一拍(约 35s),超过规则评估间隔 30s,会把 WorldNodeSaturated(for 10m)和 LoadScoreDispersionLow(for 15m)的 pending 计时清零。
   修法: 在文档里如实写明:换主会重置这两条告警的 for 计时。不需要改表达式。
- [nit] 登记集合不变量的写法前后矛盾。文件头写「期望数 Hash 出现第一个字段之前,这个 zone 已经登记」,但 DesiredWorldChannelCount 的播种分支在登记失败时是「Errorf 后继续」。
   修法: 统一表述为「尽力登记,有活节点的 zone 由领导者每拍自愈」(与 must 第 2 条的修法一致)。
- [nit] TestRefreshLoadScores_LeaderWritesOneZaddPerZone 钉死「GET 12 / SET 6 / SADD 1 / ZADD 2」,测的是实现形状,不是对外契约(AGENTS §11.4)。以后合并 SET、改批次大小都会误红。
   修法: 改为断言行为上界:没有逐节点的串行往返,ZADD 条数 ≤ zone 数,GET 条数 ≤ 2×节点数。去掉 SET 的精确计数。
- [nit] 新跟随者完全不写登记集合。混合版本窗口里,旧领导者期间新出现、又在领导者缺位时整体死掉的 zone,不会进入登记集合。
   修法: 可选:每个新版本副本启动时(或每拍)对自己的 Config.ZoneId 发一条 SADD。集合只加不减,多写者安全,开销是每副本每拍一条命令。
- [nit] orphan_cleanup.go 的 parseWorldChannelsKey 改完后在生产代码里不再有调用者,只剩测试(logic_test.go:2522、world_channel_directory_test.go:729)在用。
   修法: 保留时加注释说明「仅用于守住键前缀约束 / 旧版本兼容」;或连同对应断言改成直接比较前缀。
## missed
- go-zero v1.10.0 的 core/stores/redis/breakerhook.go:35-41:熔断打开时 pipeline 不执行、也不给各条命令回填错误,规格的逐条判定会把它读成成功(must 第 1 条)
- go/scene_manager/internal/logic/enterscenelogic.go:949-969 playerLocationOwnerGone 把 ZCARD==0 当作破坏性证据;world_rebalance.go:173 用 ZSCORE 判存活。规格没有分析扣下成员资格对这两处的影响(must 第 2 条)
- docs/ops/scene-node-role-split.md §4.2(:171-203,nodes_by_role 发布者与残余盲区)和 §6(:277-295,孤儿清理仍写 SCAN)
- go/scene_manager/internal/logic/leader_gate.go:5-9 的注释「负载分刷新每个副本照常跑」
- world_channel_directory.go:39-40、orphan_cleanup.go:205-208、world_channel_directory_test.go:725-726、nodes_by_role_metrics_test.go:3-4 的注释把 SCAN 或旧发布者语义写成现状
- integration_test.go:189-224 的 registerWorldNodeFromHarness 直接调用 updateNodeLoad(依赖默认领导者),应列入「必须仍然通过」清单
- tools/scripts/k8s_deploy.ps1:6119-6175 的 ConfigMap 模板没有 ZoneRegistryBootstrapScan 的入口,规格的升级 runbook 在 K8s 上做不到
- createscenelogic.go:148:任意副本处理 CreateScene 时,会用请求里的 ZoneId 走 initWorldScenesForZone → DesiredWorldChannelCount。新逻辑下会把请求给的 zone 写进登记集合;无害,但要在契约里写明「登记集合可能含有无活节点的 zone」
- 监控:缺少「刷新停摆 / 无领导者」的告警,也没有整批读失败的 outcome
- tools/merge_zone/main.go:311 的用法说明没有提到登记集合
- 验证步骤里「先红」的基线应钉在提交号上,不能用会随快照提交漂移的 HEAD
