# 调研规格:拆 Redis 设计按 2000 万在线修订 + allkeys-lfu 过时注释(2026-10-16)

> **状态**:只读调研产出,**未落码**。由开服线会话 2026-10-16 的调研工作流生成(一名调研者出规格、一名核实者逐条反驳),原样保存,未经人工改写。
> **怎么用**:前半部分是调研者的规格;「# VERDICT」之后是核实者意见。**落码前必须先吸收核实意见里的 must / should**,被判 refuted 的事实以核实者为准。行号与工作区状态是 2026-10-16 03:48~04:35 的快照,工作区有多个会话在并行修改,落码前按函数名重新定位。
> **汇总、拍板项与推荐顺序**:见 [docs/PROGRESS.md](../../PROGRESS.md)「2026-10-16 开服线剩余工作调研」条目,以及交接 [handoff-launch-readiness-and-d1-20261013.md](../handoff-launch-readiness-and-d1-20261013.md) 文首 10-16 段。

## item
G4a 拆 Redis 设计按 2000 万在线修订(并入全局单点交接 §5.1 / §8 第 1 条:D12 / CZ-2 改写为「逻辑单一、物理按 {player_id} 分片」),以及修正 chat / friend / match / scene_manager / 部署脚本里「共享库是 allkeys-lfu」的过时注释
## already_done
本次全程只读(亲自打开或 grep 核实):
1. G4a 现稿已经有 10-14~10-15 其他会话追加的更正注,这些不用再做:
   - §2.2 A 表的原子性与锁键(shared-redis-split-by-purpose.md:71-82);
   - §2.3 排队行号(:177);
   - C++ AUTH 已落码(:219-221、:1030、:1052);
   - §3.2.1「放行锁是全局键」(:257);
   - 重启闸 / maxclients(:471、:473、:520);
   - 新键登记 login_generation、settlement pending index、sync_digest、trade leader、assetop wake(:135、:138-142)。
   本线交接 §4 第 5 项末尾要求更正「第 66、236 行锁是全局键」,已由上述 :81、:257 两条更正注完成。
2. P0-A(C++ AUTH)与 P0-E(maxclients=40000、连接总账、重启闸 -ConfirmRedisCoreRestart)已落码,但未编译、未验证。
3. 任务描述说 redis-ha「已落地」,不准确:
   - P0 交接 §6.1(handoff-global-singletons-p0-20261015.md:110)写明它「实际没有任何部署路径接入」;
   - redis-ha.yaml:3-5 只改了文件头注释。
4. 负责人 10-13 已拍板几项(见 PROGRESS.md:7737、:7745、:7746、:7748):
   - hash tag 原则「以按 player_id 分片为准」,但瓶颈文档 §6.5 至今未修订;
   - `kafka:expand:status` 死代码「保留」;
   - reply_instance 用 10 号字段;
   - db 消费组按 zone(③)是开服门槛。
5. 第 0 批四项全部未动:
   - session_reconciler.go:123 仍在 SCAN;
   - redis_client.h:22、:50 仍在 SADD dirty_keys_set;
   - key_ordered_consumer.go:45 仍是单频道;
   - load_reporter.go:231-232 读不门控。
6. 过时注释没有人改过。
## summary
一、问题与根因
- G4a 现稿按「全服 10 万~S1」写成:只按用途拆实例,第 2 批把 go/db 控制面并进全服唯一的 redis-mapping,redis-cache 可选,不上 Cluster,也不动 D12 / CZ-2。
- 负责人 10-15 把目标定为 DAU 1 亿、PCU 2000 万。按 capacity §7.2 口径推算(全部 [I]),在第 0 批和拆分都做完之后:
  - 核心仍约 119~127 万 ops/s,只计在线玩家的热数据就有 200~420 GB;
  - mapping 读约 20~47 万 ops/s,内存 100~300 GB;
  - 控制面约 62~155 万 ops/s;
  - 子表缓存 0.6~1.2 TB。
- 所以按用途拆只是第一步。核心、mapping、cache 都必须分片,或按区域单元拆开;控制面的键天然带 zone,适合按 zone 组分实例。

二、做法
1. 修订只用一份文档,即 G4a。原文不删,在每个被取代的结论处追加带日期的更正注,文末新增 §16~§26:
   - §16:2000 万口径重算;
   - §17:第 0 批四项,只写定位与接缝约束,引用另行产出的落码规格;
   - §18:控制面按 zone 组分实例。依据是 DBTask 只按 home_zone 选 topic;负责人已定保留 expand 死代码,所以这里改用「同一玩家只在其 home topic 上处理」来论证扩分区锁的作用域;D1 §5.2「映射与控制面一起丢」的论证随之失效,所以以条件写落地为前置;
   - §19:redis-cache 改为必做、Cluster 形态;
   - §20:合并全局单点 §5.1,写键族 tag 表、跨族脚本拆分清单、全局键清单、C++ 集群客户端前置、路线 R / C;
   - §21:mapping 三族 hash tag 的裁决材料:两边各自的理由、10-13 已定的原则、剩下的时机与形态问题;
   - §22:账号目录落库;
   - §23:核心内存上界与冷卸载(S1 前拍板);
   - §24 / §25:新分批表与拍板清单;
   - §26:Java。
2. 第二部分只改注释,不改结论。共 12 处:四个服务里 10 处,另加 k8s_deploy.ps1 渲染进 friend ConfigMap 的一行注释和 deploy/k8s/README.md 一行。docker-compose.yml:240、:242 是历史说明,保留。
## est_files: 22
## current_facts
F1. G4a 现稿的核心结论是:核心 + redis-queue + redis-mapping(第 2 批并入 go/db 控制面)+ 可选 redis-cache。核心上 Cluster 与核心按 zone 拆都被否决,CZ-2 / D12 视为不受影响
   证据: docs/design/shared-redis-split-by-purpose.md:12-19(实例表)、:25(核心仍是全服单主)、:199-202(§2.4 第 4 条)、:285-303(§3.2.3 并入 mapping)、:305-316(§3.2.4 可选)、:326-335(§3.3 否决表)、:914-926(§9「单实例余量远大于首日推算值」)。亲读全文
F2. 容量模型 §4.2 对 G4a 提出的修订要求:新增第 0 批(四项);控制面按 zone 组分实例;先删 expand 死代码再拆;redis-cache 改必做;1c 窗口同时改 mapping 三族 hash tag(与 db-task-pipeline §6.5 冲突);冻结窗口按键数复算;§13 第 5 条 S1 前拍板;账号目录落库;纳入 redis-match-cluster
   证据: docs/design/capacity-model-hok-scale.md:654-674(亲读);A6 两条路线 :543-572;拍板项 §6 第 6、13 条 :759、:766
F3. 2000 万目标的口径:E≈11,111/s、λ_sw≈66,667/s,峰均比 4.8,不能把旧 S3 的表直接乘二
   证据: capacity-model-hok-scale.md:3、:894-910(§7.2,亲读);S3 公式出处 :227(N_save)、:249-258(核心分解)、:266(会话对账)、:268(mapping + 控制面)、:273-274(核心内存与子表缓存)、:290-291(mapping 内存与账号目录)
F4. 全局单点交接要求把 D12 / CZ-2 改写为「逻辑单一、物理按 player_id 分片,同一玩家的键用 {player_id} hash tag」,C++ 接入集群客户端,全局键删除或拆分,账号权威落库。该交接 10-16 的订正把这部分改派给本线
   证据: docs/handoff/handoff-global-singletons-10m-ccu-20261013.md:3-10(10-16 订正)、:120-130(§4.2 跨实体多 key)、:196-205(§5.1)、:240-246(§8)。亲读
F5. 本线交接 §4 第 5 项列出 G4a 修订要求,并要求与全局单点合并成一份设计;第 10 项登记了过时注释
   证据: docs/handoff/handoff-launch-readiness-and-d1-20261013.md:130-138、:164;10-15 进度段 :7-28 写明第 5 项仍未做。亲读
F6. 负责人 10-13 的拍板:expand 死代码「保留」;reply_instance 用 10;③(db 消费组按 zone)是开服门槛;Redis 分片 hash tag「以按 player_id 分片为准」,瓶颈文档 §6.5 待修订
   证据: docs/PROGRESS.md:7737(第 5 条)、:7745(第 13 条)、:7746(第 15 条)、:7748(第 16 条)。亲读
F7. 瓶颈文档 §6.5 仍写「为 Redis Cluster 改 hash tag 键名:破坏滚动升级兼容与 merge_zone 的锁键镜像」。go/db 附录给出三条理由:破坏滚动升级兼容、merge_zone 镜像键(relocate.go:371)、运维守卫模式。godb 交接还要求围栏缓存键预留 {player_id} tag
   证据: docs/design/db-task-pipeline-bottlenecks.md:341(文件 mtime 10-13,未修订);docs/handoff/godb-pipeline-handoff-20261013-appendix.md:594、:673 第 8 条、:686;docs/handoff/godb-pipeline-handoff-20261013.md:87、:194、:231。亲读
F8. mapping 三族键名不带 hash tag,而且在多个独立 module 里各自镜像了前缀
   证据: go/shared/placement/placement.go:30、:40;go/shared/placement/route.go:21、:30;go/data_service/internal/routing/router.go:86、:110;go/data_service/internal/routetool/redisread.go:33-37;tools/data_consistency_check/main.go:222、:384、:462(SCAN player:zone:*);tools/merge_zone/main.go:96、placement_codec.go:28、fence.go:53、merged_into.go:35;go/guild/internal/logic/merge_fence.go:39。亲 grep
F9. 核心闭包键同样不带 hash tag,例如 owner_epoch 的格式是 "player:%d:owner_epoch",与 C++ 逐字镜像
   证据: go/shared/ownerepoch/ownerepoch.go:10-26、:49-55(OwnerEpochKey / HandoffKey)。亲读
F10. 第 0 批四项都还在原状
   证据: go/player_locator/internal/logic/session_reconciler.go:123(SCAN player:session:*);cpp/libs/engine/infra/storage/redis_client/redis_client.h:22、:50(SADD dirty_keys_set)与 cpp/tests/currency_test/currency_test.cpp:821、:993;go/db/internal/kafka/key_ordered_consumer.go:45(taskResultNotifyChannel 单频道)、go/login/internal/dispatcher/task_result_dispatcher.go:32、:227;go/scene_manager/internal/logic/load_reporter.go:231-232(每副本读,不门控)、:240(写才按 isLeader 门控)、:750(SCAN scene_nodes:zone:*:load)。亲读
F11. DBTask 的 9 号字段 save_seq 已在工作区(未提交),reply_instance 应取 10
   证据: proto/db/db_task.proto:20-21(git status 为 M)。亲读
F12. 控制面键里 applied、applied_epoch、重试、死信、排序锁都含 topic(即含 zone);只有扩分区锁不含 topic。生产代码里 expand 状态只读,唯一的写入只写 normal
   证据: go/db/internal/kafka/key_ordered_consumer.go:503、:648-649、:1112、:1117、:2330-2347(只读;:2343 写回 normal)、:2371(kafka:consumer:lock:<key> 不含 topic);ordering_lease.go:69-70(锁键含 topic)。亲 grep / 读
F13. DBTask 只按 home_zone 选 topic,所以一个玩家的控制面键只落在其 home zone 的 topic 下
   证据: cpp/libs/services/scene/player/system/player_lifecycle.cpp:4394-4398(注释写「选哪个 zone 的 topic 仍只看 home_zone」)。亲读
F14. C++ 每个进程只有 1 条 zone Redis 连接。G4a §2.1 写的「每个线程一条」是错的(P0-E 只在 §4.3 :520 注过,§2.1 本身没注)
   证据: cpp/libs/engine/thread_context/redis_manager.h:122(thread_local tlsRedis),全仓唯一建连点 cpp/libs/engine/core/node/system/node/node.cpp:587。亲 grep
F15. 账号目录 account:<account> 是 Redis 里的唯一权威。缺键时脚本会就地用空值初始化。FLUSHALL / FLUSHDB 会把所有 zone 的账号目录一起清掉
   证据: go/login/internal/logic/clientplayerlogin/loginlogic.go:344-375(注释:344「不是可重新加载的缓存」;脚本缺键即 SET 空账号);createplayerlogic.go:401(player_to_account);docs/design/db-write-fencing-mysql.md:712、:718。亲读
F16. K8s 核心 Redis 不设 maxmemory,也不设淘汰策略,即缺省 noeviction、没有上限;K8s 的 Redis 清单历史上从未出现过 allkeys。compose 自 10-12 起是 noeviction + 8gb
   证据: deploy/k8s/manifests/infra/redis.yaml:131、:133(两条 exec 只带 --appendonly / --maxclients / --requirepass)、:173-175;git log -S "allkeys" -- deploy/k8s/manifests/infra/redis.yaml redis-ha.yaml 结果为空;deploy/docker-compose.yml:240-256。亲读
F17. redis-ha 没有接入任何部署路径
   证据: deploy/k8s/manifests/infra/redis-ha.yaml:3-5;docs/handoff/handoff-global-singletons-p0-20261015.md:110(§6.1 顺带订正)。亲读
F18. 在 go cpp java tools deploy 下 grep allkeys-lfu 共 14 处,其中 12 处过时、2 处是历史说明
   证据: 过时 12 处:go/chat/etc/chat.yaml:44;go/chat/internal/config/config.go:37;go/chat/internal/svc/servicecontext.go:86;go/friend/etc/friend.yaml:80;go/friend/internal/config/config.go:93;go/friend/internal/logic/friend_logic.go:168;go/friend/internal/svc/servicecontext.go:197;go/match/etc/match_service.yaml:128;go/match/internal/logic/keys.go:36;go/scene_manager/internal/logic/enterscenelogic.go:603;tools/scripts/k8s_deploy.ps1:6366;deploy/k8s/README.md:761。历史说明 2 处:deploy/docker-compose.yml:240、:242。match 两处是条件句(「若配了」),字面并不错。亲 grep,并逐处读了上下文
F19. k8s_deploy.ps1:6366 位于可展开的 here-string 内(从 :6330 的 @" 开始,:6359 有 ${InfraNamespace} 插值),会渲染进 go-svc-friend-config。契约测试会渲染它,但只断言 RequireTiDB;go-svc 的 Deployment 没有 ConfigMap 内容哈希注解,改注释不会触发滚动
   证据: tools/scripts/k8s_deploy.ps1:6330、:6358-6366;tools/scripts/tests/k8s_deploy_contract.tests.ps1:2598-2599;k8s_deploy.ps1 里内容哈希只出现在 C++ 日志 sidecar(:3747、:4295)。亲读
F20. Java 版:Redis 是 Redisson 单实例;账号与角色已经在 MySQL;compose 的 redis 不设 maxmemory(即缺省 noeviction);Java 代码里没有 allkeys-lfu 注释
   证据: D:/luyuan/xuanming-server-mmo/game-discovery/src/main/java/com/game/discovery/RedisAutoConfiguration.java:34(useSingleServer);game-player-store/src/main/resources/db/game-player-schema.sql:4(account 表)、:12(player 表);deploy/compose/infra.yaml grep maxmemory 无结果;docs/porting/deploy-ci-spec.md:129 只是描述 mmorpg 旧 compose。亲 grep
## plan
- [docs/design/shared-redis-split-by-purpose.md] 按 2000 万目标修订。只追加,原文不删;H1 标题不改,免得 docs/README.md 第 208 行的索引变动。

【A. 文首修订块】在状态块(:3-8)之后追加「2026-10-16 修订注(先读)」,写清四点:
- 目标改为 DAU 1 亿 / PCU 2000 万,依据 capacity-model-hok-scale.md §7.2;
- 本文合并全局单点交接 §5.1 / §8 第 1 条,不另起文档;
- 范围从「只按用途拆、不上 Cluster」扩为「按用途拆 + 逻辑单一、物理按 {player_id} 分片」;分片部分是设计,未拍板不落码;
- §1~§15 里被取代的结论已逐处加注,以 §16~§26 为准。附新章节索引。

【B. 逐处更正注】每条以「(2026-10-16 更正,上文原文保留)」开头:
- B1 §1 第 1 条表后(:19):redis-cache 改为必做(→§19);redis-mapping「第 2 批起并入控制面」作废(→§18)。
- B2 §1 第 3 条(:25):2000 万下核心约 119~127 万 ops/s,在线热集 200~420 GB [I]。哨兵只解决可用性,容量问题见 §20。
- B3 §1 第 7 条(:33)与 §9 末(:926):「单实例余量远大于首日推算值」作废。mapping 读约 20~47 万 ops/s、内存 100~300 GB [I],必须分片(→§20 / §21)。
- B4 §2.1 C++ 行(:41):「每个线程一条连接」改为每进程一条(redis_manager.h:122 是 thread_local,但唯一建连点是 node.cpp:587)。
- B5 §2.2 G(:146):登记 redis-match-cluster 的 volatile-lru 会淘汰 match 票据和锁(capacity §4.2)。只进 §25 待拍板,不扩大本稿的实施范围(交 A8)。
- B6 §2.4 第 4 条(:199-202):「核心全服一份,所以 CZ-2 / D12 不受影响」在 2000 万下不再是终态,改写提案见 §20。
- B7 §2.5:
  - 第 1、2 条(:209-214):dirty_keys_set 进第 0 批(§17);核心内存上界见 §23;
  - 第 5 条(:223):实际是 12 处(四个服务 10 处 + k8s_deploy.ps1 渲染注释 + deploy/k8s/README.md),已于 10-16 修正,链到 PROGRESS 条目。
- B8 §3.1 表(:232-237):
  - redis-mapping 行去掉「+ D(第 2 批)」;
  - 新增 redis-dbctl-g<N> 行(§18);
  - redis-cache 行「可选」改为必做、Cluster 形态(§19)。
- B9 §3.2.3(:285-303):「并入 redis-mapping」作废,见 §18。扩分区锁的分析保留。注明负责人 10-13 定「expand 死代码保留」(PROGRESS.md:7737),所以不能照 capacity §4.2「先删 expand 死代码再拆」执行。
- B10 §3.2.4(:305-316):「可选」作废,见 §19。
- B11 §3.3 否决表(:326-335):
  - 「核心上 Cluster」重开为 §20 路线 C;
  - 「核心按 zone 拆」改为路线 R 的区域单元(不是按 zone 拆);
  - 「控制面单独成第四个实例」由 §18 取代。
- B12 §3.4(:337-345)与 §15(:1079-1145):以 §24 为准;第 2、3 批的文件清单由各自规格重列。
- B13 §4.2 拓扑规则(:485)「dbctl 必须同时启用 mapping」作废,见 §18.4。
- B14 §5.4 末段(:694):冻结窗口按键数复算。开服前键很少;规模后 N_reg=10 亿时约 3×10 亿=30 亿个键,按 5 万键/s 约需 16.7 小时 [I],不可行。规模后只能走路径 B。
- B15 §5.5(:703-721):
  - 目标实例改为本 zone 所属组的 redis-dbctl-g<N>;
  - 前提补两条:条件写已落地(§18.3);③ db 消费组按 zone 已完成(负责人定为开服门槛,PROGRESS.md:7746)。
- B16 §6.1 例子(:794)与 §6.2 表(:796-808):按 §16 重算;「1000 万玩家约 3–8GB」改按 N_reg=10 亿。
- B17 §11 第 6 条(:1017):见 §20。
- B18 §13(:1043-1064):
  - 第 1、2、3、5、11 条由 §25 取代或补充;
  - 第 13 条注明 redis-ha 至今未接入任何部署路径(redis-ha.yaml:3-5;P0 交接 §6.1)。
- B19 §14(:1066-1077):补「Java 账号目录已在库」(§26)。

【C. 文末新增章节】

§16 修订依据与 2000 万口径
- 规则:按在线人数增长的项用 §7.2 的 E=11,111/s、λ_sw=66,667/s,Z=200~445,N_s=1,778~8,889;按日活增长的项(C_n、N_reg、N_acc、P_cum)沿用 S3 值;不把 S3 表乘二。
- 推算表,全部 [I]:
  - N_save = PCU(1−s)/300 + E + λ_x(1−s) ≈ 4.4~11.1 万/s;
  - DBTask = 2·N_save ≈ 8.9~22.2 万/s;
  - 核心(第 0 批 + 拆分后,不含子表缓存、friend MGET、排队)≈ N_save + 43E + 10λ_sw + 0.8·N_s + 29C_n ≈ 119~127 万 ops/s,对 10 万参照约 12~13 倍;
  - friend 登录 MGET:E×F ≈ 55.6 万~222 万键/s;
  - mapping 读(不并控制面)≈ 2·DBTask + 2E + 3C_n ≈ 20~47 万 ops/s;
  - 控制面 ≈ 7·DBTask ≈ 62~155 万 ops/s,即每 zone 0.14~0.78 万;
  - 子表缓存 3·PCU·V ≈ 0.6~1.2 TB;
  - 核心常驻 ≥ P_cum(V+0.3 KB) ≈ 1~2 TB,其中在线热集 PCU·V ≈ 200~420 GB;
  - mapping 内存 100~300 GB;账号目录 205~360 GB;
  - 会话对账(不改的话)1.33 亿~2.97 亿 GET/s;SM 刷新(不改的话)28 万~316 万 GET/s;
  - 排队轮询 2.5·Q ≈ 500 万 ops/s;task:result 稳态扇出 5.5 万~12.3 万/s。
- 结论:拆实例只是第一步。

§17 第 0 批(新增,先于 1a;不改键族归属,不需要 -RedisSplit)
- 0-1 会话对账去掉全键 SCAN:session_reconciler.go:123。方向见 capacity A0-4 :414、全局单点 §4.3 / §5。
- 0-2 删 dirty_keys_set:redis_client.h:22、:50,连同 currency_test.cpp:821、:993。全部升级后用 UNLINK,不要用 DEL。
- 0-3 task:result 分频道:key_ordered_consumer.go:45、task_result_dispatcher.go:32、:227。DBTask 用 reply_instance=10;直接 SPUBLISH;结果 LIST 与频道同实例。
- 0-4 SM 负载刷新只读本 zone:load_reporter.go:231-232、:750。只收窄读,不含按 zone 选主;前提是核实 SM 没有进程内的按玩家状态(capacity :446)。
- 每项写「落码规格见 docs/handoff/launch-readiness-20261013/<另出的规格文件>(以落盘名为准)」。
- 接缝约束写死:
  - 0-1 的 gate 索引键不得与 player:session:<id> 在同一段 Lua 里原子绑定(否则 §20 分片时 CROSSSLOT)。可以做成尽力而为的索引,对账前复核会话本身;或者按 slot 分桶;
  - 0-2 删除也是 §20 的前置;
  - 0-3 的频道形态要能直接迁到 §19 的 Cluster。

§18 控制面按 zone 组分实例(取代 §3.2.3)
- 18.1 理由:
  - 控制面负载正比于 DBTask;
  - 键都带 topic(:503、:648、:1112、:1117,ordering_lease.go:69);
  - DBTask 只按 home_zone 选 topic(player_lifecycle.cpp:4394-4398)。
- 18.2 扩分区锁 kafka:consumer:lock:<pid>(:2371)不含 topic:
  - expand 保留,但生产代码只读状态(:2330-2347),只在读状态出错时才拿锁;
  - 同一玩家只在 home topic 上处理,竞争者都在同一组实例上,所以分组不破坏互斥;
  - 残余:合服改 home 时,源 zone 和目标 zone 若跨组,R2 要两边都查。
- 18.3 D1 §5.2「映射与控制面一起丢」不再成立:只丢 dbctl 组实例时游标缺失,顺序守卫判 Apply(docker-compose.yml:242-244 的注释)。所以第 2 批以条件写(db-write-fencing-mysql.md v0.3)落地为前置;组数取 1 时,就退化为原方案。
- 18.4 配置:
  - zones 配置加 redisDbCtlGroup 字段;
  - Get-RedisTopologyFor 按 zone 返回 DbCtl;
  - 实例名 redis-dbctl-g<N>;迁移标记仍按 zone;
  - merge_zone 的 -dbctl-redis-* 按 zone 解析,或规定合服只在组内进行(待拍板)。
- 18.5 组大小:按每实例 5 万 ops/s 预算推算为 6~35 个 zone,待 V2 实测后定。
- 18.6 对 go/db 写路径 3b 的影响:锁、游标、epoch 在 dbctl,落点在 mapping,缓存在 cache,三处不同实例,所以 3b 只能按「独立实例 11→4」口径(瓶颈文档 §6.1)。

§19 redis-cache 必做
- 0.6~1.2 TB,单实例放不下,用 Cluster。PlayerDatabase*:<id> 都是单键操作,不改名也是 Cluster 安全的;login / go/db 的缓存句柄要换成 go-redis ClusterClient(UniversalClient)。
- task:result 用 SPUBLISH 分片频道,依赖 0-3。
- 降内存的评估项:写任务只 DEL、不 SET(capacity A2 第 4 条);缩短 1h TTL。两者都要评审回档风险:login 会用子表缓存拼父键(§3.2.4)。
- 清单形态参照 redis-match-cluster.yaml,不复用 redis-purpose.yaml。

§20 逻辑单一、物理按 {player_id} 分片(D12 / CZ-2 改写提案,未拍板)
- 不变量改写:同一玩家的契约键只有一个权威存储,任何 zone 都能寻址到它(全局单点 §5.1)。
- 键族与 tag:
  - {pid}:父键;owner_epoch / handoff / location / login_generation / sync_digest / channel_switch_cooldown;player:session;player_locker;battle:lock / ctx / settlement:pending;team:player / invite;新号种子;
  - {account}:account、account_lock:create / login、login_account_sessions;
  - {team}:组队;
  - {zone}:场景路由。
- 必须拆开的跨 tag 脚本:
  - 账号外观修复(account + player_locker);
  - 离场计数(location + 场景 / 节点计数),拆成 CAS + 幂等递减;
  - 组队 S_COMMIT;
  - player_locator 9 段 Lua 里的 player:leases*,按 slot 分桶;
  - battle:settlement:pending:index;
  - router.go:299-324 在脚本内拼围栏键。
- 全局键与 SCAN 依赖:dirty_keys_set、player:leases*、scene_manager:leader:lock、task:result:notify、scene_nodes / world_channels 的 SCAN、match migrateLegacyQueues。
- C++ 前置:
  - 换集群客户端(新依赖要按 AGENTS §11.2 写理由);
  - 支持 MOVED / ASK 与地址重解析;
  - 去掉「同连接存盘后 PING 判落盘」的顺序依赖(player_battle.cpp:385-393、player_lifecycle.h:734-735)。
- 迁移:破坏性改名。核心闭包走双读窗口加回滚;Go / C++ 的 7 处镜像同批改(scenenode/locator.go:18-36)。mapping 不能双读(D1「缺键≠无记录」),见 §21。
- 路线:
  - R:区域单元 ≤50 万 PCU,即 ≥40 个单元,每个单元核心约 3.0~3.2 万 ops/s,单元内不需要 tag;
  - C:Cluster,上面全做;
  - 交负责人拍板。

§21 mapping 三族 hash tag(裁决材料)
- 现状键名与镜像点:见 current_facts。
- capacity §4.2 主张改名的理由:1c 是唯一一次全量搬键的停写窗口;不改的话,将来集群化还要再停写搬一次;规模后改名要约 16.7 小时停写。
- §6.5 反对的理由:滚动升级时新旧二进制读写不同的键,缺键会被当成无记录;merge_zone 的锁键与映射前缀镜像;运维守卫模式。注意 §6.5 的原意针对 go/db 写路径的锁、游标、epoch、缓存键,mapping 键是在 3b 合并往返时被卷进去的。
- 已定:10-13「以按 player_id 分片为准」。
- 剩下要定的三件事:
  - (a) 时机;
  - (b) 形态:停写窗口内一次改名,全部读写方(含 merge_zone、data_consistency_check、route_tool、guild、data_service SCAN、k8s_zone_rollback 守卫模式、redis_split 族表)同窗口切换,不做双读;
  - (c) hash tag 只让三族同槽;W1 / 发布脚本 / 读 home 时对围栏键的复核仍会 CROSSSLOT,需要挪进库(D1 db_first)。
- 新键名建议:pmap:{<id>}:zone|placement|maprev,待拍板。
- 推荐:开服前做 1c 就同窗口改名;1c 晚于开服就走路径 B,在新实例上以新键名从 player_route 预热。

§22 账号目录落库
- 现状:loginlogic.go:344-375、createplayerlogic.go:401;FLUSHALL 的后果见 db-write-fencing-mysql.md:712、:718;容量 205~360 GB。
- 方向:
  - 全局薄层建账号目录表,主键用 account 或其哈希(随机键,没有尾部热点);
  - 库是提交点,Redis 降为缓存;
  - GetOrInitUserAccount 的「缺键就写空账号」改为「缺键先回源库,库里也没有才初始化」;
  - 存量 account:* 在冻结建角窗口内回填;
  - 集群化时 {account} tag 与 account_lock 同槽;
  - 与密码表的位置一并定。

§23 核心内存上界与冷玩家卸载(S1 前拍板)
- 现状:父键首存后无 TTL;owner_epoch 永不删;K8s 核心不设 maxmemory(redis.yaml:173-175)。
- 卸载条件:玩家离线,且 go/db 游标已越过最后一次存盘;卸载后保留 owner_epoch。
- 与 0-3 分频道同批;与 NewPlayerSeed、合服回流一起评估。
- 结论:即使做了冷卸载,在线热集 200~420 GB 也放不下单实例,所以本节与 §20 路线绑定。

§24 新分批表
- 0;
- 1a、1b:原样,拆出的实例显式 --maxclients;
- 1c:queue、mapping,可含改名;
- 2:dbctl 按组;
- 3:cache 用 Cluster;
- 4:分片或单元化。
- 另注:redis-queue 在 2000 万下(500 万 ops/s 轮询)也要按 zone 组分或用 {zone} tag;轮询间隔归登录排队设计。

§25 待拍板:见 open_decisions。

§26 Java
- 账号目录:已做,实现不同(Java 落 MySQL)。
- 拆实例与分片:待做(Redisson 单实例,RedisAutoConfiguration.java:34)。
- 客户端契约不变,PARITY 在落码时登记。
   工作区: git status 无改动;mtime 2026-10-15 14:38:26;最近一次修改是 10-15 的 P0-E 与排队更正注,已入库。冲突风险:中。这是热门文档,其他线(P0、排队、落地恢复)常来追加更正注。落码前重读,只用 Edit 追加。
- [docs/design/capacity-model-hok-scale.md] 只追加带日期的指向注,数字不改:
1. §4.2 标题下(:655 前):本节要求已并入 G4a §16~§25。其中「先删 expand 死代码再拆」与负责人 10-13「保留」的决定冲突(PROGRESS.md:7737),G4a §18.2 给了替代论证;hash tag 原则已于 10-13 拍板(PROGRESS.md:7748)。
2. A6(:545 一带):路线 R / C 的展开与 2000 万复算见 G4a §16、§20。
3. §6 第 6 条(:759):其中「第 1c 批是否同时改 hash tag」的剩余问题见 G4a §21。
   工作区: git status 无改动;mtime 2026-10-15 14:38:41。§7 是 10-15 新加的,可能还有人在追加。重读后用 Edit。
- [docs/design/db-task-pipeline-bottlenecks.md] 在 §6.5 第 5 条(:341)后追加带日期更正注:负责人 10-13(PROGRESS.md:7748)已定「以按 player_id 分片为准」,本条不再作为否决理由。滚动升级兼容、merge_zone 镜像(relocate.go:367-372)、守卫模式这三点,改为分片迁移必须满足的约束,见 G4a §20 / §21。go/db 写路径的锁、游标、epoch 在按 zone 组分实例后天然按组,不需要 {player_id} tag。
   工作区: git status 无改动;mtime 2026-10-13 03:30:47。这份文档归 go/db 存盘管线那条线。建议本线只加注,并在 PROGRESS 里告知;或者整条交给 go/db 线去加,避免两边同时编辑。
- [docs/design/cross-zone-scene-travel.md] 在 CZ-2 表行(:33)所在表格之后追加一行带日期的注:2000 万目标下「生产 Redis 物理共享单实例」的改写提案见 G4a §20,未拍板前 CZ-2 照旧有效。正文不改。
   工作区: git status 无改动;mtime 2026-10-14 23:02:32。冲突风险低。
- [docs/design/cross-zone-matchmaking.md] 在 D12 表行(:103)所在表格之后追加同样的指向注,内容同 CZ-2。
   工作区: git status M;mtime 2026-10-16 03:31:37。其他会话正在改,本次调研期间 diff 从 +41 涨到 +51,hunk 在 :3 和 :487 一带,不碰 :103。文件是 CRLF。冲突风险中高:落码前重读,只用 Edit 追加,不要整文件重写;也可以等对方提交后再加。
- [docs/design/player-route-mapping-durability-proposal.md] 在 :458「已知未改:…9 处…」之后追加带日期的结案注:实际是 12 处(含 k8s_deploy.ps1 渲染注释与 deploy/k8s/README.md),已于 10-16 改为现状口径,结论不变。可选:在 §3.3 加一句,指向 G4a §21 的 mapping hash tag 裁决材料。
   工作区: git status 无改动;mtime 2026-10-15 10:45:50。冲突风险低。
- [docs/handoff/handoff-launch-readiness-and-d1-20261013.md] 在文首进度段末尾追加「2026-10-16 进度」:
- §4 第 5 项已并入 G4a §16~§26,注明是否已落文档;
- 第 10 项的过时注释已改,链到 PROGRESS 条目;
- 第 0 批四项的落码规格位置。
   工作区: git status M(他人未提交 +2 行);mtime 2026-10-15 22:15:56。与本项相关,是文首进度段的追加。重读后用 Edit 追加到文首块末尾,不碰对方的行。
- [docs/handoff/handoff-global-singletons-10m-ccu-20261013.md] 在文首 10-16 状态订正块末尾追加一行:§5.1 与 §8 第 1、3、5 条已并入 G4a §20、§22、§23,不另起设计。
   工作区: git status M(他人未提交 +9 行,即 10-16 的状态订正,PROGRESS.md 9044 条);mtime 2026-10-16 03:31:38。与本项直接相关。只在那段订正之后追加,不改原文。
- [docs/handoff/launch-readiness-20261013/spec_G4a.md] 文首加带日期的注:本规格对应 G4a 原稿;第 2 批「并入 redis-mapping」、第 3 批「可选」已被 G4a 2026-10-16 修订取代,以设计文档 §18、§19、§24 为准。
   工作区: git status 无改动;mtime 2026-10-15 13:56:11。冲突风险低。
- [docs/PROGRESS.md] 末尾追加一条「2026-10-16 G4a 按 2000 万修订 + allkeys-lfu 过时注释更正」,写明以下内容:
- 改动范围:只改文档与注释,未编译、未测试;
- 待拍板清单;
- 交 Codex 的命令;
- Java 版状态:对应 Java 版本号无;分片 / 拆实例待做;账号目录 Java 已在库;注释更正不适用。
文件配置了 merge=union,只追加。
   工作区: git status M;mtime 2026-10-16 03:50:11,多个会话在追加。只在文件末尾追加,不回头改已有条目。
- [go/chat/etc/chat.yaml] 把 :44 一行改为两行:
`# 只允许本地单库形态:共享库的淘汰策略不归 chat 管(compose 自 2026-10-12 起与 K8s 一样是 noeviction,此前是 allkeys-lfu),`
`# 聊天历史无 TTL,回落后会与契约 key 抢内存,共享库设了 maxmemory 写满时还会连带拒写契约 key。`
:45 的「⚠ staging/prod 必须指向独立实例且 maxmemory-policy=noeviction…」原样保留。
   工作区: git status 无改动;mtime 2026-10-12 03:53:04。无冲突风险。
- [go/chat/internal/config/config.go] :37 原文是 `	// (compose 共享库是 allkeys-lfu,会把聊天记录当缓存淘汰)。`,改为两行:
`	// (共享库的淘汰策略不归 chat 管:compose 自 2026-10-12 起是 noeviction,此前是 allkeys-lfu;`
`	// 即使不淘汰,无 TTL 的聊天记录也会挤占契约 key 的内存,共享库写满时连带拒写)。`
:36 不动。
   工作区: git status 无改动;mtime 2026-10-15 11:00:43。无冲突风险。
- [go/chat/internal/svc/servicecontext.go] :86 原文是 `		// 共享库是 allkeys-lfu,聊天历史会被当缓存淘汰,且与契约 key 抢内存。`,改为两行:
`		// 共享库的淘汰策略不归 chat 管(compose 2026-10-12 前是 allkeys-lfu,现与 K8s 同为 noeviction),`
`		// 聊天历史无 TTL,回落后与契约 key 抢内存;共享库设了 maxmemory 写满时会连带拒写契约 key。`
日志文案(:87-89)不改。
   工作区: git status 无改动;mtime 2026-10-15 11:00:43。无冲突风险。
- [go/friend/etc/friend.yaml] :80-81 两行改为四行:
`# ⚠ staging/prod 必须指向独立实例且 maxmemory-policy=noeviction:共享库的淘汰策略不归 friend 管`
`#   (compose 自 2026-10-12 起与 K8s 同为 noeviction,此前是 allkeys-lfu),一旦被配成淘汰型,`
`#   好友列表缓存被淘汰只是多打一次 MySQL,但**配额 / 限流 key 被淘汰等于限流静默失效**;`
`#   不淘汰时这些 key 也在和契约 key 抢共享库的内存。`
   工作区: git status 无改动;mtime 2026-10-15 07:56:03。无冲突风险。
- [go/friend/internal/config/config.go] :93-94 两行改为三行:
`	// ⚠ staging/prod 必须指向独立实例且 maxmemory-policy=noeviction:共享库的淘汰策略不归 friend 管`
`	// (compose 自 2026-10-12 起与 K8s 同为 noeviction,此前是 allkeys-lfu)。一旦被配成淘汰型,`
`	// 好友列表缓存被淘汰只是多打一次 MySQL,但配额 / 限流 key 被淘汰等于限流静默失效;不淘汰时也在与契约 key 抢内存。`
   工作区: git status 无改动;mtime 2026-10-15 07:55:20。无冲突风险。
- [go/friend/internal/logic/friend_logic.go] :168 原文是 `	// 共享库是 allkeys-lfu,而且那里只放跨运行时契约 key(契约 §4)。`,改为:
`	// 共享库只放跨运行时契约 key(契约 §4),它的淘汰策略也不归 friend 管(compose 2026-10-12 前是 allkeys-lfu,现为 noeviction)。`
   工作区: git status 无改动;mtime 2026-10-15 08:03:21。无冲突风险。
- [go/friend/internal/svc/servicecontext.go] :197-198 两行改为三行:
`		// 共享库的淘汰策略不归 friend 管(compose 2026-10-12 前是 allkeys-lfu,现与 K8s 同为 noeviction):`
`		// 好友缓存回落过去要和契约 key 抢内存 —— 淘汰型策略下契约 key 可能被挤掉,后果是"玩家明明在线却判成离线",`
`		// 推送静默丢;noeviction 下共享库写满则连带拒写契约 key。`
日志文案不改。
   工作区: git status 无改动;mtime 2026-10-15 12:58:27。无冲突风险。
- [go/match/internal/logic/keys.go] :36-37 两行改为三行(原文是条件句,字面不错,这里只补上现状):
`// MatchRedis 缺省回落到共享库时,评分能否存活取决于共享库的淘汰策略(compose 自 2026-10-12 起与 K8s`
`// 同为 noeviction,此前是 allkeys-lfu);一旦配成 allkeys-* 之类,评分可能被淘汰回落到默认 1500,`
`// 且无 TTL 的评分会和契约 key 抢内存 —— 生产必须配独立 MatchRedis(设计文档 §4.2 / §11)。`
   工作区: 文件本身 git status 无改动,mtime 2026-10-12 22:58:38。但 go/match 目录下另一会话有 22 个 M 文件和 5 个未跟踪文件(全球匹配:GlobalMatch、authority、global_match 等),本文件不在其中。冲突风险低。
- [go/match/etc/match_service.yaml] :128 原文是 `# 回落共享库时,共享库若配了 allkeys-lfu 之类的淘汰策略,评分可能被淘汰回落 1500;`,改为两行:
`# 回落共享库时,评分的存活取决于共享库的淘汰策略(compose 自 2026-10-12 起与 K8s 同为 noeviction,此前是`
`# allkeys-lfu);一旦配成 allkeys-* 之类的淘汰型,评分可能被淘汰回落 1500;`
:127、:129 不动。
   工作区: git status M。另一会话未提交:文件头新增 Mode 与 GlobalMatch 段,共 +14 行;mtime 2026-10-15 22:42:05。diff 只在 :1-18,不碰 :127-129(HEAD 的 :114),与本项无关。冲突风险中:建议等那个会话提交后再改;或者落码时重读,只用 Edit 改这一行,不 git add 整个文件。
- [go/scene_manager/internal/logic/enterscenelogic.go] :603 的括号内容替换后,注释块变成(:601-605,改动只在第 3、4 行):
`	// 「持有节点缓存为 0」只从两处同时为 0 推出:owner_epoch 键(observedEpoch)**与** location 里记的`
`	// epoch。location 记的是上一次派给这个节点的路由事件所带的值,它非 0 而键读到 0,说明持有节点缓存`
`	// 着 N ≠ 0、只是键单独丢了(本地 compose 在 2026-10-12 前是 allkeys-lfu,会单独淘汰它;现在 compose 与`
`	// K8s 都是 noeviction,但手工 DEL 或淘汰策略被误配时读数相同,防御保留)。此时铸出 1,持有节点`
`	// 按 max 保留 N,下一次带 guard 的存盘拿 "1" 比 N 被拒,合法持有者被当废黜销毁(踢人 + 回档)。`
代码逻辑不动。
   工作区: git status 无改动;mtime 2026-10-14 23:05:03。无冲突风险。
- [tools/scripts/k8s_deploy.ps1] :6366 一行改为三行。这一行在 :6330 起的 @" 可展开 here-string 里,替换文字不得含 $ 和反引号:
`#   **配额 / 限流 key 被淘汰等于限流静默失效**。回落时这些 key 在共享库上:共享库是 noeviction 且没设 maxmemory`
`#   (infra/redis.yaml),今天不会被淘汰,但会和契约 key 抢内存,共享库将来设了上限、写满即拒写(连带契约 key),`
`#   所以回落只是过渡。(2026-10-16 更正:原文写「共享库现在是 allkeys-lfu」,K8s 清单从未用过淘汰型策略。)`
:6363-6365 不动。改动只影响渲染出的 go-svc-friend-config 里的注释。
   工作区: git status M。他人未提交 +693 行(收件箱、P0-E 重启闸、排队等);mtime 2026-10-16 03:50:30,改动非常活跃。最近的 hunk 是 @@ -5728 +6323 的 SharedRedis 段,与 :6363-6366 相距约 40 行,不重叠,与本项无关。冲突风险中:落码前立刻重读,只用 Edit 改这一行;不 git add -A,提交时按行分。
- [deploy/k8s/README.md] :761 一行改为三行(:760、:762 不动):
`  否则缓存被淘汰只是多打一次 MySQL,**配额 / 限流 key 被淘汰等于限流静默失效**。回落时它们在共享库上:共享库是 noeviction、`
`  没设 maxmemory(\`infra/redis.yaml\`),今天不淘汰,但与契约 key 抢内存,将来设了上限写满即拒写,所以回落只是过渡。`
`  *(2026-10-16 更正:原文写「共享库是 \`allkeys-lfu\`」,K8s 清单从未用过淘汰型策略;那是本地 compose 2026-10-12 之前的配置。)*`
   工作区: git status M。他人未提交,hunk 在 :969-1043 与 :1412-1450(收件箱、Redis 总账);mtime 2026-10-16 03:30:38。不碰 :759-762。冲突风险低到中,重读后用 Edit。
- [tools/scripts/tests/k8s_deploy_contract.tests.ps1(可选)] 在 :2599 的 friend ConfigMap 断言之后加一条回归断言:
`Assert-NotMatch -Text $friendConfig -Pattern 'allkeys-lfu' -Because 'K8s 共享库是 noeviction;friend ConfigMap 注释不得再声称共享库是 allkeys-lfu(2026-10-16 更正)'`
修复前为红(当前渲染含该串),修复后为绿。
   工作区: git status M。他人未提交 +745 行;mtime 2026-10-16 03:50:52,非常活跃。可选:冲突成本高时就不加,只靠 verify 里的 git grep 兜底。
## compat_and_rollout
1. 第一部分只改文档,没有运行期影响。
   - 所有修订都用带日期的更正注追加,原文不删;
   - G4a 的 H1 不改,docs/README.md 的索引行不变,gen_docs_index --check 不受影响;
   - 分片与改名都只是设计,写明「未拍板不落码」。落码时按各批次规格单独做兼容设计:核心闭包改名走双读窗口;mapping 改名走停写窗口,不双读;cache 不拷贝。
2. 第二部分只改注释,行为不变。
   - Go 的注释与 etc/*.yaml 的注释:go-zero 加载时忽略,编译产物语义不变。
   - k8s_deploy.ps1 那一行会改变渲染出的 go-svc-friend-config 文本。kubectl apply 只更新 ConfigMap 对象;go-svc Deployment 没有 ConfigMap 内容哈希注解(只有 C++ sidecar 有,:3747),所以不会触发滚动;即使 Pod 重启也只是注释变化。
   - 没有契约测试钉住旧文本(grep tests 无命中)。
   - 回滚 = 还原这几行。
3. 建议拆成两个可独立回滚的提交:
   - 提交 1:注释更正,12 个文件;
   - 提交 2:G4a 修订与其余文档。
   提交由用户决定,不 git add -A。go/match 与 k8s_deploy.ps1 里有他人在途改动,要按行分。
## tests
- 第二部分(注释更正)不改行为,不需要新的行为测试,用以下检查代替:(1) git grep 确认在 go cpp java tools deploy 下 allkeys-lfu 只剩 deploy/docker-compose.yml 两行历史说明;(2) chat / friend / scene_manager / match 改过的 Go 文件 gofmt 无输出,go build 通过;(3) k8s_deploy.ps1 用 PowerShell AST 能解析,k8s_deploy_contract 里「带全局 Go 服务的 infra-up DryRun 必须成功」那条用例 PASS(它会渲染 go-svc-friend-config,能兜住 here-string 被插值破坏)。
- 可选的回归用例:在 k8s_deploy_contract.tests.ps1:2599 之后加 Assert-NotMatch friendConfig 'allkeys-lfu'。修复前为红,修复后为绿,不依赖墙钟。
- chat / friend 的配置加载用例(go/chat/chat_test.go、go/friend/friend_test.go、go/friend/internal/config/config_test.go 会读仓库里的 yaml)应保持通过,以确认 yaml 注释改动没有破坏格式。
- 第一部分是纯文档:python tools/scripts/gen_docs_index.py --check 退出码为 0。
- G4a 修订稿应在 §8 测试清单里新增下列用例(落码批次时才写,本次不写代码):(a) 第 4 批:为每段多键 Lua 计算 CRC16 slot,断言同一脚本的 KEYS 都落同一个 slot,C++ 与 Go 键格式用 golden 向量对拍(覆盖 scenenode/locator.go:18-36 列出的 7 处镜像);改名的双读窗口要覆盖「新键缺、旧键在」「两边都在且值不同」「回滚后旧版读得到」三种情况。(b) 第 2 批按组分实例:契约测试断言 zone 到组的映射渲染出的 db RedisClient 指向 redis-dbctl-g<N>;跨组合服的 merge_zone 要么拒绝,要么同时查两边的排序锁;-topics 精确匹配的用例保留。(c) 第 3 批:login / go/db 缓存句柄是 ClusterClient;task:result 用 SPUBLISH 分片频道;不同 login 实例只收自己的结果。(d) 账号目录:Redis 缺键时回源库,不再写空账号;并发建角时库是提交点;存量回填幂等。(e) 0-1 会话对账:索引缺项或多项时,对账前都要复核会话本身,不误删在线会话。以上都面向行为,修复前红,不依赖墙钟,具体用例名由各批规格定。
## verify_commands
- 工作目录:D:\luyuan\xuanming-server-mmo-cpp(PowerShell 7)。命令:git grep -n "allkeys-lfu" -- go cpp java tools deploy。通过标准:只剩 deploy/docker-compose.yml 里两行历史说明(「2026-10-12 由 allkeys-lfu 改为 noeviction」与「allkeys-lfu 会把它们当冷键淘汰」;行号可能因他人改动漂移,以内容为准)。
- 工作目录 go/chat:gofmt -l internal/config/config.go internal/svc/servicecontext.go; go build ./...; go vet ./internal/config/... ./internal/svc/...; go test -count=1 . ./internal/config/... ./internal/svc/...。通过标准:gofmt 无输出,其余退出码为 0。需要外部依赖而 SKIP 的用例记下来,不算失败。
- 工作目录 go/friend:gofmt -l internal/config/config.go internal/logic/friend_logic.go internal/svc/servicecontext.go; go build ./...; go vet ./internal/config/... ./internal/logic/... ./internal/svc/...; go test -count=1 . ./internal/config/...。通过标准同上。
- 工作目录 go/scene_manager:gofmt -l internal/logic/enterscenelogic.go; go build ./...。通过标准:gofmt 无输出,build 退出码为 0。
- 工作目录 go/match:gofmt -l internal/logic/keys.go; go build ./...。注意 go/match 有另一会话的在途改动(全球匹配)。build 失败时先看报错文件:本批只改注释,不可能引入编译错误,对方文件的报错按文件归属单独汇报。
- 工作目录仓库根:pwsh -NoProfile -Command '$e=$null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path tools/scripts/k8s_deploy.ps1).Path,[ref]$null,[ref]$e); if($e){$e; exit 1}else{"parse ok"}'。通过标准:输出 parse ok。
- 工作目录仓库根:先清 MMORPG_* 环境变量(Get-ChildItem Env:MMORPG_* | ForEach-Object { Remove-Item "Env:$($_.Name)" }),再运行 pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1; "exit=$LASTEXITCODE"。通过标准:「带全局 Go 服务的 infra-up DryRun 必须成功」与「开发 ConfigMap 必须显式说明未要求 TiDB」两条为 [PASS];加了可选断言的话,该断言也为 [PASS]。文件里有其他会话的在途用例,它们的红按归属单独汇报。最好先在不含本批的快照上跑一次基线,用于区分。
- 工作目录仓库根:python tools/scripts/gen_docs_index.py --check。通过标准:退出码为 0。
- 失败时保留的材料:各命令的完整输出摘要(失败用例名与首个报错),不要连续自动重试(AGENTS.md §10.2)。本批不涉及 C++ 编译,也不需要 MSBuild。
## doc_updates
- docs/design/shared-redis-split-by-purpose.md:文首修订块、逐处更正注 B1~B19、新增 §16~§26(主产出)
- docs/design/capacity-model-hok-scale.md:在 §4.2、A6、§6 第 6 条加带日期的指向注
- docs/design/db-task-pipeline-bottlenecks.md:在 §6.5 第 5 条后加带日期的注,依据 10-13 拍板第 16 条(归属 go/db 线,需协调)
- docs/design/cross-zone-scene-travel.md:在 CZ-2 表后加改写提案的指向注
- docs/design/cross-zone-matchmaking.md:在 D12 表后加指向注(他人正在改,等对方提交或用 Edit 小心追加)
- docs/design/player-route-mapping-durability-proposal.md:在 :458 后加「过时注释已改,实为 12 处」的结案注
- docs/handoff/handoff-launch-readiness-and-d1-20261013.md:文首追加 10-16 进度,说明第 5 项与第 10 项的状态
- docs/handoff/handoff-global-singletons-10m-ccu-20261013.md:在 10-16 订正后追加一行,说明 §5.1 / §8 已并入 G4a
- docs/handoff/launch-readiness-20261013/spec_G4a.md:文首注明第 2、3 批已被修订取代
- docs/PROGRESS.md:末尾追加条目,写明 Java 状态与交 Codex 的命令
- 可选,不在本项必做范围:docs/design/ 下仍有十余处把共享库写成 allkeys-lfu 的旧描述,可以另起一项统一加更正注,本项不夹带。涉及 microservice-zone-contract-20260914.md:225、character-appearance-identity.md:29、:34、:56、team-system.md:46、:1438、leaderboard-system.md:77、:143、:223、jubaozhai-market.md:150、client-access-band-routing.md:612、cross-zone-matchmaking.md:163、:178、:458
## java_parity
1. 第一部分(G4a 修订)只是设计,不涉及客户端契约(AGENTS §12「两版共享的只有客户端契约」),本次 Java 不改。修订稿 §26 写明各项在 Java 的状态:
   - 账号目录落库:Java「已做,实现不同」。account 与 player 表在 MySQL,见 game-player-store/src/main/resources/db/game-player-schema.sql:4、:12。
   - 按用途拆实例与按玩家分片:Java「待做」。原因有两条:Java 用 Redisson useSingleServer(RedisAutoConfiguration.java:34),没有 K8s 生产形态;键族与本仓不同,要按 Java 自己的键重新盘点。
   - PARITY.md 在第 1 批或分片落码时登记,本次不登记。
   - 区域单元(路线 R)改变跨单元旅行体验,属于客户端可见行为。若拍板选它,必须与客户端、Java 同批评审。
2. 第二部分(注释更正):不适用。
   - Java 代码里没有 allkeys-lfu 注释;
   - Java compose 的 redis 不设 maxmemory,即缺省 noeviction;
   - Java 仓库 docs/porting/deploy-ci-spec.md:129 等处是对 mmorpg 旧 compose 的移植快照描述,由 Java 侧自行决定是否加注;
   - PARITY 不登记(内部注释)。
## risks
- 并发编辑风险。k8s_deploy.ps1(他人 +693 行,mtime 10-16 03:50)、k8s_deploy_contract.tests.ps1(+745)、deploy/k8s/README.md、go/match 目录(22 个 M 文件)、cross-zone-matchmaking.md(本次调研期间 diff 还在增长)、两份交接、PROGRESS 都有他人在途改动。必须在落码前立刻重读,只用 Edit 改目标行,不要整文件 Write,不 git add -A;match_service.yaml 建议等对方提交后再改。
- k8s_deploy.ps1:6366 位于 @" 可展开 here-string 内。替换文字一旦含 $ 或反引号,会被插值,渲染出错误的 ConfigMap 或解析失败。已给出的替换文字不含这两种字符;验证靠 AST 解析与 infra-up DryRun 契约用例。
- §16 的 2000 万数字全部是推算 [I]:进场与退场链路的 43 条命令、V、c_scene 等都没测过。修订稿必须逐项标 [I],不得据此宣称性能结论(AGENTS §6);组大小、单元规模都要等 V2 实测再定。
- 第 0 批 0-1 若在 SetOnline / SetDisconnecting 的 Lua 里原子维护 gate 索引,会造成新的跨槽绑定,阻塞 §20 的分片。修订稿要把它写成接缝约束,交给 0-1 的规格作者:做成尽力而为的索引并在对账前复核,或者按 slot 分桶。
- 控制面按 zone 组分实例后,D1 §5.2「映射与控制面一起丢」的论证不再成立。只丢 dbctl 实例时游标缺失,顺序守卫会静默放行;所以必须以条件写落地为前置,或者暂时保持组数为 1。
- mapping 若在 1c 窗口改名,1c 的范围会扩大到所有镜像点:merge_zone 的 4 个文件、data_consistency_check、route_tool、guild、data_service 的 SCAN、回档守卫模式、redis_split 族表。漏改一个就会静默写错库。修订稿要求用契约测试把镜像清单钉住。
- 任务描述把 redis-ha 列为已落地,但它并未接入任何部署路径。修订稿不得假设核心已有 HA;§13 第 13 条与 §12 第 9 条的单点风险照旧成立。
- 负责人 10-13 定的「expand 死代码保留」与容量模型 §4.2「先删再拆」相冲突。按 AGENTS §11,以负责人决定为准,修订稿改用 home_zone 作用域来论证。若负责人改主意删掉 expand,§18.2 可以相应简化。
- 瓶颈文档 §6.5 归 go/db 线。本线加注可能与那条线的在途修订撞车,应在 PROGRESS 里点名告知,或者交给那条线来加。
## open_decisions
- 核心的路线:R 是区域单元,每单元 ≤50 万 PCU,2000 万需要 ≥40 个单元,跨单元按转区处理;C 是 Redis Cluster 加 {player_id} hash tag,加 C++ 集群客户端,加去掉同连接顺序依赖。同时决定是否把 D12 / CZ-2 改写为「逻辑单一、物理按 {player_id} 分片」(全局单点 §8 第 1 条;capacity §6 第 5、6 条)。
- mapping 三族 hash tag:原则已于 10-13 定为「以按 player_id 分片为准」(PROGRESS.md:7748)。还要定三件事:(a) 是否在 1c 映射拷贝的停写窗口里一并改名(只在开服前可行;规模后约 30 亿键、约 16.7 小时停写,只能走路径 B 重建);(b) 新键名格式(建议 pmap:{<id>}:zone|placement|maprev);(c) 瓶颈文档 §6.5 由谁加修订注。
- 控制面按 zone 组分实例:组大小初值(推算 6~35 个 zone 一组,待 V2 实测);是否以条件写(db-write-fencing-mysql v0.3)落地作为第 2 批的硬前置;跨组合服时 merge_zone 用源、目标两个 dbctl 句柄,还是规定合服只在组内进行。
- redis-cache 改为必做后的形态:用 Cluster(单键操作不用改名,但 login / go/db 的句柄要换成 ClusterClient),还是按 zone 组分实例(login 读缓存前要先知道 home 组)。另外,是否评估「写任务只 DEL 不 SET」或缩短 1h TTL 来降内存;两者都有回档风险。
- 账号目录落库:放在哪里(全局薄层 TiDB);主键用 account 还是它的哈希;与密码表的位置(全局单点 §8 第 5 条)一并定;单独立项授权。
- 核心内存上界(必须在 S1 前):冷玩家卸载阈值(离线 24 小时?)与触发条件(离线,且 go/db 游标已越过最后一次存盘);核心 maxmemory 初值与告警。另外请确认「即使冷卸载,在线热集 200~420 GB 也放不下单实例」,所以这一项与第一条的路线一起拍。
- redis-queue 在 2000 万下(排队轮询推算约 500 万 ops/s)是否按 zone 组分实例或用 {zone} tag;轮询间隔与推送化归登录排队设计。
- redis-match-cluster(chat 与 match 共用,volatile-lru 会淘汰票据和锁)是否纳入本稿;capacity §4.2 建议纳入,A8 建议拆成两个集群。

# VERDICT
## overall
事实核对：大部分成立。引用的代码行和文档行绝大多数亲自打开核实过，第 0 批四项未动、redis-ha 未接入、14 处 allkeys-lfu 的分布、Java 现状都属实。不成立或不准的有五处：
- 「10-16 订正把 D12 / CZ-2 改派给本线」不成立，合并要求的出处是本线交接 §4 第 5 项；
- 「同一玩家只落 home topic」忽略了 home_zone 未知时回落到进程 zone 的逻辑；
- 「子表缓存单键、Cluster 安全」与负责人已定的 db-write-fencing v0.4 冲突；
- ③ 消费组按 zone 的代码其实已入库，只是未验证；
- 瓶颈文档与 k8s_deploy.ps1 的工作区状态，在核实期间已经变化。

方案：不能直接照着落码，有三条 must。
1. 注释更正的替换文案都保留了 allkeys-lfu 字面量，规格自己的 grep 验证和可选回归断言改完后必然失败，前后矛盾。
2. chat 的替换文案写「聊天历史无 TTL」，事实上有 7 天 TTL 和 200 条上限，会制造新的错误注释。
3. G4a 修订完全没有登记 db-write-fencing-mysql.md v0.4（10-16）明确要求拆分稿登记的依赖：令牌发布脚本把排序锁、子表缓存、资产证据三键绑在一段 Lua 里，OwnerEpochRedis，证据键跨实例。按规格 §18 / §19 分组、上 Cluster 后，这段脚本无法原子执行；它给瓶颈文档 §6.5 拟的注也与围栏文档相反。

should 级问题集中在四类：
- §20 的 Cluster 改造清单不全：跨玩家 MGET / MULTI / SCAN、客户端可见游标、C++ FIFO 依赖；
- 核心闭包「双读窗口」迁移在新旧并存时不安全；
- 冷卸载、账号落库缺少与资产账本、滚动升级的交叉评估；
- 原稿更正注漏掉大量「dbctl = mapping」锚点，运维面（清库、守卫、连接总账）也没覆盖。

Java 对等判断（设计不改客户端、账号目录 Java 已在库、注释不适用）基本成立。但如果路线 C 改了 friend 在线目录的 SCAN 游标，就会触及客户端契约，需要与 Java 同批。

建议：先修好三条 must，把围栏依赖与 0-1 索引冲突列入待拍板，再交付。本次全程只读，没有创建或修改任何文件。
## fact_checks
- [confirmed] G4a 现稿的核心结论：核心 + redis-queue + redis-mapping（第 2 批并入控制面）+ 可选 redis-cache；否决核心 Cluster 与按 zone 拆核心；CZ-2 / D12 不受影响
   亲读 docs/design/shared-redis-split-by-purpose.md 全文：:12-19 实例表，:25 核心全服单主，:199-202 §2.4 第 4 条，:285-303 §3.2.3 并入 mapping，:305-316 §3.2.4 可选，:326-335 §3.3 否决表，:914-926 §9。注意 :23 也写了「C++ 每个线程只有一条 Redis 连接」，与 :41 是同一个错误，规格的 B4 只改了 :41。
- [confirmed] 容量模型 §4.2 对 G4a 的修订要求（第 0 批、控制面按 zone 组、先删 expand、cache 必做、1c 改 hash tag、冻结窗口复算、§13 第 5 条 S1 前拍板、账号目录落库、纳入 redis-match-cluster）
   亲读 capacity-model-hok-scale.md:655-674。A6 在 :543-572，§6 第 6、13 条在 :759、:766。§4.2 还要求「friend 登录的 MGET player:session 计入核心预算」，但规格 §16 的核心数字把它排除了。
- [confirmed] 2000 万口径：E≈11,111/s、λ_sw≈66,667/s、峰均比 4.8，不能把 S3 直接乘二
   capacity-model-hok-scale.md:897-908（§7.2）亲读，三项都在。但规格 §16 写的「§7.2 的 Z=200~445，N_s=1,778~8,889」不在 §7.2 里，是把 S3 的 Z（:196，223/100）与 N_s（:71，889~4,444）按 PCU 翻倍推出来的 [I]，出处标错了。
- [refuted] 全局单点交接要求改写 D12 / CZ-2；该交接 10-16 的订正把这部分改派给本线
   前半句成立：handoff-global-singletons-10m-ccu-20261013.md:196-205（§5.1）、:240-246（§8）亲读。后半句不成立：:3-10 的 10-16 订正只把「会话对账按 gate 建索引、删 dirty_keys_set、task:result 分频道、gate nofile、SM 只读本 zone」派给开服准备线与 decisions-20261015，没有提 §5.1 / §8 第 1 条。合并要求的出处是 handoff-launch-readiness-and-d1-20261013.md:137（§4 第 5 项，10-13 写的）。另外 :226-238 的 §7 阶段 6 还列了「scene_manager / 服务发现 / 消费组按 zone 限域、cell 划分与跨 cell 迁移协议」，规格没有覆盖。
- [confirmed] 本线交接 §4 第 5 项列出 G4a 修订要求并要求合并；第 10 项登记了过时注释；10-15 进度段写明第 5 项未做
   亲读 handoff-launch-readiness-and-d1-20261013.md:129-138、:164、:13。工作区里他人新增的 +2 行是 :28「2026-10-15 晚，续」，写明「本文 §4 第 5、7~10 项仍归本文」。
- [confirmed] 负责人 10-13 拍板：expand 保留（:7737）、reply_instance 用 10（:7745）、③ 是开服门槛（:7746）、hash tag 以 player_id 分片为准（:7748）
   亲读 PROGRESS.md:7737、:7745、:7747、:7748，内容都对。但 :7746 是第 14 条（AGENTS §6.2「条件写落码时再加」），「③ 是开服门槛」在 :7747，行号差一。③ = 消费组按 zone，见 godb-pipeline-handoff-20261013.md:180、:230。
- [confirmed] 瓶颈文档 §6.5 仍写「为 Redis Cluster 改 hash tag 键名」那条否决理由；文件 mtime 10-13，未修订
   §6.5 原文没变，现位于 db-task-pipeline-bottlenecks.md:356（HEAD 是 :341）。但工作区状态已经变了：git status 为 M，mtime 2026-10-16 03:55:20，他人在 :164 一带新增 +15 行（G6 停机顺序）。所以「无改动 / mtime 10-13」已过时，冲突风险要上调。godb 交接 :87、:194、:231 与附录 :594、:686 都已亲读。
- [confirmed] mapping 三族键名不带 hash tag，并在多个独立 module 里各自镜像了前缀
   所列 14 处逐行亲读，全部属实。清单不全，以下镜像点或读写方未列入：tools/merge_zone/placement_ops.go:54、:496、:745；go/data_service/internal/routing/route_publish.go:146、router.go:411、:487（SCAN）；go/data_service/internal/routetool/importer.go；tools/scripts/k8s_zone_rollback.ps1:655-657 守卫模式；tools/scripts/tests/k8s_zone_rollback_flush_guard.tests.ps1:58。
- [confirmed] 核心闭包键不带 hash tag：owner_epoch 的格式是 "player:%d:owner_epoch"，与 C++ 逐字镜像
   亲读 go/shared/ownerepoch/ownerepoch.go:49-55。location 键的 7 处镜像清单在 go/shared/scenenode/locator.go:18-36。
- [confirmed] 第 0 批四项都还在原状
   亲读 session_reconciler.go:123（SCAN）；redis_client.h:22、:50（SADD dirty_keys_set）；currency_test.cpp:821、:993；key_ordered_consumer.go:45；task_result_dispatcher.go:32、:227；load_reporter.go:231-232 读、:240 只在 isLeader 下写、:750 SCAN。player_locator、load_reporter、redis_client.h 都没有未提交改动。
- [confirmed] DBTask 的 9 号字段 save_seq 已在工作区（未提交），reply_instance 应取 10
   proto/db/db_task.proto:20 为 `uint64 save_seq = 9`，git status 为 M。
- [confirmed] 控制面键（applied、applied_epoch、重试、死信、排序锁）都含 topic，只有扩分区锁不含；生产代码对 expand 状态只读，唯一的写入是写 normal
   亲读 key_ordered_consumer.go:503、:649、:1112、:1117、:2330、:2343，以及 ordering_lease.go:70（规格写的 :69）。扩分区锁键 kafka:consumer:lock:%s 在核实期间从 :2371 漂到了 :2385，该文件正被他人实时改动。全仓 grep 只有 :2343 调 SetExpandStatus。
- [refuted] DBTask 只按 home_zone 选 topic，所以一个玩家的控制面键只落在其 home zone 的 topic 下
   player_lifecycle.cpp:4396-4398 的注释原文是「仍只看 home_zone（上面的回落逻辑）」。但 :4347-4356 写着：homeZoneId==0 时回落为进程 zone，并计 home_zone_unknown，而且注释明说滚动升级窗口里每次存盘都会走到这条。所以同一玩家的 DBTask 可能进非 home 的 topic，「只落在 home topic」不是无条件成立。§18.2 的互斥论证要把这条回落算进去。
- [confirmed] C++ 每进程只有 1 条 zone Redis 连接，G4a §2.1 写的「每个线程一条」是错的
   redis_manager.h:122 是 extern thread_local tlsRedis；全仓唯一的建连点是 node.cpp:587 的 tlsRedis.Connect。G4a :23 与 :41 两处都写错了，:520 的 P0-E 更正注只订正了一次。
- [confirmed] 账号目录 account:<account> 在 Redis 是唯一权威，缺键时脚本就地写空值；FLUSHALL / FLUSHDB 会把所有 zone 的账号目录一起清掉
   亲读 loginlogic.go:344-375、createplayerlogic.go:401。db-write-fencing-mysql.md 的 :712、:718 是 HEAD 行号；该文档 10-16 04:10 刚改到 v0.4，工作区里对应行在 :985、:991，内容一致。
- [confirmed] K8s 核心 Redis 不设 maxmemory 和淘汰策略，历史上从未出现 allkeys；compose 自 10-12 起是 noeviction + 8gb
   redis.yaml:131、:133 的两条 exec 只有 --appendonly / --maxclients / --requirepass，:173-175 是注释；git log -S allkeys 对 redis.yaml / redis-ha.yaml 无输出；docker-compose.yml:240-256 亲读。
- [confirmed] redis-ha 没有接入任何部署路径
   redis-ha.yaml:3-5（工作区 M），handoff-global-singletons-p0-20261015.md:110 亲读。P0 §6.1 除改文件头外还改了另外三处「脚本会处理」，规格只写了「只改了文件头注释」，说法不精确。
- [confirmed] 在 go cpp java tools deploy 下 allkeys-lfu 共 14 处，其中 12 处过时、2 处是历史说明
   git grep 结果与规格一致。k8s_deploy.ps1 那一处在核实期间从 :6366 漂到 :6367，再漂到 :6393（他人正在改，diff 已到 +720）。go/cpp/java/tools/deploy 之外只有 docs 下还有这个字符串。
- [confirmed] k8s_deploy.ps1 的 friend 注释在可展开 here-string 内，会渲染进 go-svc-friend-config；契约测试只断言 RequireTiDB；go-svc 的 Deployment 没有 ConfigMap 内容哈希注解
   friend 段的 @" 现在从 :6356 开始（规格写 :6330，已漂移），段内有 ${InfraNamespace}、${redisPassword}、$($friendSql.RequireTiDB) 插值。k8s_deploy_contract.tests.ps1:2598-2599 亲读。哈希注解只有 C++ sidecar 的 :4298，另有 zone-redis-auth 与削峰两种口令指纹，都与 friend ConfigMap 无关。tests 下没有 golden 文件。
- [confirmed] Java 版：Redisson 单实例；账号与角色在 MySQL；compose 的 redis 不设 maxmemory；Java 代码里没有 allkeys-lfu
   RedisAutoConfiguration.java:34 useSingleServer；game-player-schema.sql:4（account）、:12（player）；deploy/compose/infra.yaml:62 只带 --appendonly yes。Java 仓里 grep allkeys 无结果，Java 仓的 PARITY.md 当前有他人未提交改动。
- [refuted] （规格替换文案的前提）聊天历史无 TTL
   go/chat/internal/config/config.go:62-64 的 HistoryTTLSeconds 缺省 604800（7 天），每次写入续期；chat_logic.go:88-93 的 appendLogScript 在一段 Lua 里做 LPUSH + LTRIM 200 + EXPIRE；chat.yaml:71 也配了这一项。k8s_deploy.ps1:6236 明写「chat 的历史 LIST / 幂等 / 限速 key 都带 TTL」，所以在 K8s 用的 redis-match-cluster（volatile-lru）上恰恰会被优先淘汰。
- [refuted] （规格 §19 前提）PlayerDatabase*:<id> 都是单键操作，不改名也是 Cluster 安全的
   代码现状是单键：go/db/internal/kafka/player_cache_publish.go（未跟踪，在途）做单键 CAS。但负责人已定路线的 db-write-fencing-mysql.md v0.4 §6.2（:568-612）要换成「令牌发布脚本」，在一段 Lua 里同时访问排序锁键、子表缓存键与 player:{%d}:asset_sql_commit 证据键。§6.4（:675）明确登记「与 G4a 按用途拆分的冲突」，要求「G4a 稿要登记这条依赖」，§14 第 19 条（:1280）同此。
- [refuted] （规格 §20 前提）C++ 的同连接顺序依赖是 player_battle.cpp:385-393 与 player_lifecycle.h:734-735 两处
   行号已漂移：PING 快路径现在在 player_battle.cpp:409-421。player_lifecycle.h:740 说那处「同连接先 DEL 后 MGET」已经被一段 Lua 取代。另外还有多处依赖同连接 FIFO：player_battle.cpp:2055、:2166、:2313；redis.cpp:35-44；player_inbox_system.cpp:394（PING）；player_lifecycle.cpp:1009、:1246、:1718；player_lifecycle.h:1277。
- [refuted] （规格 B15 / §18.3 前提）③ 消费组按 zone 是待完成的前置，条件写以 db-write-fencing v0.3 为准
   ③ 的代码已入库：go/db/internal/kafka/group_handoff.go（de4960bee）实现了交接闸；k8s_deploy.ps1:185-186 有 zone / legacy 两种形状，db.yaml:14-24 有 ConsumerGroup / GroupHandoffFrom。但都未验证。条件写文档已是 v0.4（文首 :3，2026-10-16），而且条件写只在 dual 模式、只对围栏表（:16、:135）生效。
## plan_problems
- [must] 注释更正的替换文案与验证标准自相矛盾。12 处替换文案都保留了字面量 allkeys-lfu：例如「此前是 allkeys-lfu」「2026-10-12 前是 allkeys-lfu」，k8s_deploy.ps1 写「原文写「共享库现在是 allkeys-lfu」」，README 写「原文写「共享库是 `allkeys-lfu`」」。按规格的 verify 第 1 条，git grep 只剩 docker-compose 两行，改完后必然失败；可选回归断言 Assert-NotMatch friendConfig 'allkeys-lfu' 改完后仍是红的，不满足「修复前红、修复后绿」。
   修法: 二选一。(a) 文案里去掉这个字面量：历史值交给 git 历史，或写成「2026-10-12 前本地 compose 用的是 LFU 全键淘汰」并确认不再命中 grep。(b) 把验证改成只匹配过时断言的正则：git grep -nE '共享库(现在)?是 `?allkeys-lfu|Redis 是 allkeys-lfu' -- go cpp java tools deploy 应为空；Assert-NotMatch 的 Pattern 用 '共享库现在是 allkeys-lfu'。两种做法都要在 verify 里写明 docker-compose.yml:240、:242 照常命中。
- [must] chat 三处替换文案（chat.yaml、config.go、servicecontext.go）断言「聊天历史无 TTL」，事实相反：HistoryTTLSeconds 缺省 7 天、每次写入续期，LTRIM 只留 200 条（config.go:62-64、chat_logic.go:88-93；k8s_deploy.ps1:6236 也这样写）。写进去就是新的错误注释，还会误导下一个人判断 volatile-* 策略的风险。
   修法: 改成：「聊天历史带 TTL（缺省 7 天，每次写入续期；每频道只留最近 200 条），回落共享库时仍与契约 key 争内存（私聊键数随会话对数增长）；共享库设了 maxmemory 时写满会连带拒写契约 key；若共享库是 volatile-* 淘汰型，带 TTL 的历史会被优先淘汰」。staging/prod 必须独立实例且 noeviction 的结论保留。
- [must] G4a 修订没有登记 db-write-fencing-mysql.md v0.4（2026-10-16，负责人已定路线）对拆分稿提出的硬依赖，§18、§19 和瓶颈文档加注的方案会与它直接冲突：
① §6.2 令牌发布脚本在一段 Lua 里原子访问排序锁（按 §18 在 dbctl 组）、子表缓存（按 §19 在 cache Cluster）和 player:{%d}:asset_sql_commit 证据键，三者被拆到三处之后无法原子执行。§6.4（:675）要求「第 2 批之前换成值封装，或三键同实例」，并明说「G4a 稿要登记这条依赖」。
② §5.1（:442）已定：go/db 新增只读客户端 OwnerEpochRedis 指向核心实例。
③ 证据键由 go/db 的 RedisClient 写，data_service 经 Router.ClientForPlayer 读（:726），拆分后两边可能分家。
④ §14 第 19 条（:1280）要求拆分稿的回档 runbook 引用 §9.1。
此外：规格 §19 写「单键、Cluster 安全」以及「写任务只 DEL 不 SET」的降内存项，与 v0.4 的缓存围栏、证据发布语义冲突；规格要给瓶颈文档 §6.5 加的注写「锁、游标、epoch 按组分实例后不需要 {player_id} tag」，与 fencing §6.4「这些键必须落在同一个 tag 下，瓶颈文档 §6.5 的分片修订要把这两条约束算进去」相反。
   修法: G4a 新增一节（或并入 §18、§19），写清：
- go/db 的 RedisClient 在 v0.4 之后实际承载的键全集：控制面、令牌发布脚本、§5.5 推进脚本、证据键、OwnerEpochRedis 探测；
- 第 2、3 批的硬前置是「缓存围栏换成值封装」或「排序锁、子表缓存、证据三键同实例」，二者择一，进入 §25 待拍板；
- §19 删掉「单键 Cluster 安全」的断言；「只 DEL 不 SET」标为与 v0.4 冲突、需围栏线同意；
- 瓶颈文档 §6.5 的加注改为引用 fencing §6.4 的两条同 tag 约束；
- §20 的 tag 表补上 asset_sql_commit（go/shared/assetop/sql_commit.go:14，已带 {%d}）；
- 回档 runbook 引用 fencing §9.1。
fencing 文档正被「落地恢复与围栏」线改动（mtime 04:10），回指注交那条线写，不要直接编辑。
- [should] §18.2 用「同一玩家只在 home topic 上处理」论证扩分区锁按组分实例后仍然互斥，但 home_zone 未知时 DBTask 会回落到进程 zone 的 topic（player_lifecycle.cpp:4347-4356，滚动升级窗口每次存盘都会走到）。这时同一玩家可能出现在两个 topic，进而落到两个组的实例。
   修法: §18.2 补上这条残余，并把「home_zone_unknown 计数在全部 zone 稳定为 0」列为第 2 批按组切换的前置门禁之一。今天这把锁不含 topic，等于无意中顺带做了跨 topic 互斥；按组拆开之后这层保护就没了，要写明。
- [should] §20 列的 C++ 前置只有两处顺序依赖，其中 player_lifecycle.h 那处已改成 Lua，行号也漂移了。实际依赖同连接 FIFO 的地方远不止这些（见 fact_checks）。Cluster 下遇到 MOVED / ASK 重定向，或多连接，都会打破 FIFO。
   修法: §20 的 C++ 前置改写为：「逐条盘点所有依赖同连接 FIFO 的点（至少包括 player_battle.cpp:409-421、:2055、:2166、:2313；redis.cpp:35-44；player_inbox_system.cpp:394；player_lifecycle.cpp:1009、:1246、:1718；player_lifecycle.h:1277），每处改为脚本内原子或显式确认」，并注明 h:740 已经改成 Lua。
- [should] §20 的 Cluster 改造清单漏了几类会报 CROSSSLOT 或失效的操作。
① 跨玩家 MGET：friend session_reader.go:168、friend_profiles.go:37、online_directory.go:178；guild online_status_resolver.go:80、:221，push.go:174；login role_appearance_backfill.go:36、gatedrain.go:77；match team/presence.go:116；data_service router.go:411、player_name_cache.go:87。
② 跨键 MULTI：login token.go:105-112，排队的 Enqueue MULTI（键里没有 zone）。
③ 依赖 SCAN 的读：friend OnlineDirectory 的 SCAN player:session:*（online_directory.go:118），它的游标 "v1:scan:offset" 交给客户端，是客户端可见契约；data_service router.go:487 的 SCAN player:zone:*。
④ guild 在 DB 2（capacity A6 要求迁出）。
   修法: §20 补一张「多键 / 遍历操作改造表」：MGET 按 slot 拆成 pipeline；MULTI 统一 tag；SCAN 改成逐节点执行或改用索引。friend 在线目录的游标格式要变，按 AGENTS §12 列为客户端与 Java 同批评审项，写进 §26。
- [should] §20 的迁移方案写「核心闭包走双读窗口加回滚」，没有论证新旧版本并存时的安全性。owner_epoch 的 guard 存盘 Lua、location / handoff 的 CAS、scene_manager 的 INCR 都是围栏原语。滚动期间旧二进制读写旧键名、新二进制读写新键名，围栏会被绕过（双持有者、回档）。G4a §5.1 自己也以同样的理由否决了对 CAS 键的双写和对 mapping 的双读。测试 (a) 也没有覆盖新旧写者并发的场景。
   修法: 改为：核心闭包里的围栏键不做双读，走停写切换（全部写者停下或排空后一次改名），或用版本门禁（所有写者升级完成前不启用新键名）；回滚同样整体切回。测试 (a) 补「旧二进制按旧键 CAS、新二进制按新键 CAS 并存时必须被门禁拒绝」。
- [should] 路线 R 与 C 被写成二选一，与全局单点交接 §5.1 第 3 条的结论（「两者叠加」，且 100 万在线的 cell 也约 1.85e5 ops/s、0.5TB，超出单实例）不一致，没有调和。R 的每单元 3.0~3.2 万 ops/s 不含 friend 登录 MGET（capacity §4.2 要求计入核心），也没有算每单元内存，与 A6 的「3.3~4.1 万」口径差异也没有解释。
   修法: §16 / §20 的核心预算加入 friend MGET（键 / s 与命令 / s 分开列），给出每单元内存（P_cum 份额 + 在线热集），注明与 A6 口径的差异；§25 的路线项写成「单元化 + 单元内是否还要分片」，与交接 §5.1 第 3 条对齐。
- [should] B 系列更正注覆盖不全。原稿很多地方写死了「dbctl = redis-mapping」，规格只改了其中几处。漏掉的有：
- :23（每线程连接）；
- :196-198（§2.4 第 3 条，D1 论证「原样成立」）；
- :357-358、:367（§4.1 表）；
- :716-720（§5.5 步骤）；
- :760-762（§5.9「漏掉 redis-mapping 最严重」）；
- :790、:801（§6.1 / §6.2 第 2 批行）；
- :845（§7「dbctl 没带 mapping」拒绝）；
- :883、:888、:909（§8 用例）；
- :920（§9）；
- :1000（§10 第 2 批 kind 期望）；
- :1010-1014（§11 第 3 条）；
- :1034（§12 第 8 条）。
   修法: 在 §3.2.3 加一条总注「全文凡写 dbctl 并入 / 指向 redis-mapping 的地方以 §18 为准」，并在上列锚点各加一行短注。至少 §7 / §8 的「dbctl 必须带 mapping」要显式作废，否则落码者会照旧实现那道拒绝闸。
- [should] §18.3 有两处不准。
① 引用的是条件写 v0.3，但文档已是 v0.4，而且条件写只在 dual 模式、只对围栏表生效，legacy 模式下丢了 dbctl 仍会静默放行。
② 「组数取 1 就退化为原方案」不对：只有当那唯一一组就是 redis-mapping 时才是原方案，否则就是原稿 §13 第 2 条的「第四个实例」。
B15 / §5.5 还把 ③ 写成待完成的前置，但 ③ 的代码已入库（group_handoff.go，de4960bee；k8s_deploy.ps1:185-186），只是未验证。
   修法: §18.3 前置改成：「db-write-fencing v0.4 已落地，Fencing.Mode=dual 覆盖全部 DBTask 写目标表（§4.1 的启动断言列表）」。改正「组数取 1」那句的条件。B15 写成：「③ 代码已落（未验证），L-09 的全组 rebalance 问题在各 zone 切到按 zone 组之后消失」。
- [should] §17 把「0-1 的 gate 索引不得在 SetOnline / SetDisconnecting 的 Lua 里与 player:session 原子绑定」写成硬约束，但 capacity A0-4（:414）给的做法正是在这两段 Lua 里维护 gate:{uuid}:sessions。两份设计冲突，规格只在 risks 里提了一句，没有交给负责人裁决。
   修法: 把它列进 §25 / open_decisions：0-1 是按 A0-4 原子维护索引（将来 Cluster 化时再拆），还是一开始就做成尽力而为的索引、对账前复核会话本身，或按 slot 分桶。同时通知 0-1 的规格作者。
- [should] §23 冷玩家卸载只说「与 0-3 同批，与 NewPlayerSeed、合服回流一起评估」，漏了资产账本：data_service 的 GetPlayerAssetOpLedger 会读父键 blob（G4a :127、asset_op_ledger_logic.go）；fencing v0.4 的自动放弃条件之一是「父键 blob 里的账本与 SQL 证据都判未见」。父键被卸载后，可能被误判为未见，导致一物两得。
   修法: §23 的卸载条件补上：「该玩家没有未终结的资产待办（assetop outbox / 托管），且账本判定遇到父键缺失时 fail-closed（视为未知、不放弃）」，并与围栏线、全域到账线一起评审。
- [should] §22 账号目录落库没有写滚动升级顺序。旧 login 二进制仍是「缺键就写空账号」：并存窗口里一旦出现缺键，旧版会写入空 blob；新版命中 Redis 的空 blob 后不回源库，角色就从账号里「消失」。
   修法: 写明顺序：先回填库 → 全部 login 换成「缺键或空 blob 都回源库」的新版 → Redis 才降为缓存。新版遇到空 blob 也要回库核对；契约测试或部署闸要拒绝新旧混跑超过窗口。
- [should] §18（dbctl 多组实例）与 §19（cache Cluster）没有带上运维面：AGENTS §6.2 清库（k8s-redis-wipe 要遍历每个组实例和 cache 的每个主）、回档 FLUSHDB 守卫、P0-E「Redis 连接总账」按实例重算与各实例 --maxclients、按实例的监控 / 告警。
   修法: 在 §18.4 / §19 各列一条，指向 G4a §5.9、§4.6、§4.3 的 :520，并在 §24 的分批表里标出各批要改的运维脚本。
- [should] 工作区状态报告有两处已失实。
① db-task-pipeline-bottlenecks.md 现在是 M（+15 行 G6，mtime 10-16 03:55），§6.5 在 :356，不是「无改动、mtime 10-13」。
② k8s_deploy.ps1 正被实时改动：核实期间 diff 从 +693 涨到 +720，目标行从 :6366 漂到 :6393，friend here-string 从 :6356 开始，最近的他人 hunk 在 :6350-6352。
   修法: 瓶颈文档的加注交给 go/db 线写，或等对方提交后再加。k8s_deploy.ps1 一律按内容锚定（用 Edit 匹配整行原文），不写行号；落码前立刻重读，并核对 `git diff -U0` 的 hunk 不重叠。
- [nit] 引用细节有误：
- PROGRESS 的「③ 是开服门槛」在 :7747，不是 :7746；
- §16 把 Z、N_s 归给 §7.2，实际是翻倍 S3 的推算；
- 排队轮询 500 万 ops/s 里的 Q=200 万没有出处；
- B14 只给了 N_reg=10 亿时的 16.7 小时，没有同时给 capacity §4.2 的 S1 口径（约 3 亿键、约 100 分钟）。
   修法: 逐项改正：Z / N_s / Q 标 [I] 并写出推导；B14 同时列两个口径。
- [nit] deploy/k8s/README.md 的替换文案（JSON 里是 \\`）落进 Markdown 后是 \`，会渲染成字面反引号而不是代码段。
   修法: 直接写 `infra/redis.yaml` 与 `allkeys-lfu`（如果保留的话），不要带反斜杠。
- [nit] 可选回归断言塞进了「连 MySQL 的 ConfigMap 只有总账认识的那些」这个用例，与用例主题无关。
   修法: 另起一个 Test-Case，例如「friend ConfigMap 注释不得声称共享库是淘汰型」；先 Assert-Match 确认取到了块，避免 Assert-NotMatch 在 $null 上空过。
- [nit] §17 的 0-3 要求「结果 LIST 与频道同实例」，Cluster 下没必要：go/db 先写 LIST、再 SPUBLISH，订阅方用 ClusterClient 去 LPOP，自然路由到对的节点。§18.6 沿用 3b 的「独立实例 11→4」，但那是按两个实例算的，三实例（dbctl、mapping、cache）要重算。
   修法: 0-3 只要求「频道形态能直接迁到 SPUBLISH」；§18.6 标为未核实，待 3b 重算。
- [nit] 可选的 docs/design 旧描述清单漏了 cross-zone-scene-travel.md:1305、:1362、:1750、:1769。friend 的 here-string 注释（k8s_deploy.ps1:6375）、trade 注释（:6226）、README:762 都写着「共享库不许集群化」，§20 的路线 C 会推翻这一说法，但没有指向注。
   修法: 补进可选清单；规格本来就要给 CZ-2 / D12 加指向注，同批给这三处加一行「见 G4a §20（未拍板）」，或登记为后续项。
- [nit] go/match 正在做全球匹配（rating.go 新增 svcCtx.Authority 分支，启用后评分进 TiDB），keys.go:33-37 与 match_service.yaml:127-129 的「评分存 MatchRedis、无 TTL」会变成有条件成立；规格只看了「文件本身无改动」。
   修法: match 两处注释等全球匹配会话提交后再改，或与对方约定由它在补 Authority 说明时一并更正。
## missed
- db-write-fencing-mysql.md v0.4（2026-10-16）对拆分稿的依赖登记：§6.2 令牌发布脚本（排序锁 + 子表缓存 + 资产 SQL 提交证据三键原子）、§6.4 :675「G4a 稿要登记这条依赖」、§5.1 :442 的 OwnerEpochRedis、§6.6 :726 证据键读写跨实例、§14 第 19 条（:1280）回档 runbook 引用 §9.1
- 新键 player:{%d}:asset_sql_commit（go/shared/assetop/sql_commit.go:14，go/db 写、data_service 读）未进 G4a 键族表与 §20 的 tag 表
- go/db 的 RedisClient 在第 2 批之后实际承载的键不只是控制面（证据键、OwnerEpochRedis 探测、令牌脚本），§18 没有盘点
- 跨玩家 MGET 调用点（friend / guild / login / match / data_service 共 10 余处）、login 令牌 MULTI、排队 MULTI 在 Cluster 下会 CROSSSLOT
- friend OnlineDirectory 的 SCAN player:session:* 与客户端可见游标（online_directory.go:118、:72），涉及客户端契约与 Java 同批；data_service router.go:487 的 SCAN 映射
- C++ 同连接 FIFO 依赖的完整清单（player_battle.cpp:2055、:2166、:2313，redis.cpp:35-44，player_inbox_system.cpp:394，player_lifecycle.cpp:1009、:1246、:1718，player_lifecycle.h:1277）
- DBTask 在 home_zone 未知时回落到进程 zone 的 topic（player_lifecycle.cpp:4347-4356）对 §18.2 的影响，以及 home_zone_unknown 指标门禁
- ③ 消费组按 zone 的代码已入库（go/db/internal/kafka/group_handoff.go，de4960bee；k8s_deploy.ps1:185-186），G4a §5.5 / L-09 的说法要据此更正
- AGENTS §6.2 清库、回档 FLUSHDB 守卫、Redis 连接总账与 --maxclients、监控要覆盖 dbctl 各组实例与 cache Cluster 的每个主
- 冷玩家卸载与资产账本（data_service 读父键 blob、v0.4 自动放弃条件）的交互
- 账号目录落库的新旧 login 并存窗口（旧版缺键写空账号）
- G4a 原稿里「dbctl = redis-mapping」与「每线程连接」的其余锚点：:23、:196-198、:357-367、:716-720、:760-762、:790、:801、:845、:883、:888、:909、:920、:1000、:1010-1014、:1034
- 全局单点交接 §7 阶段 6（服务发现 / scene_manager / 消费组按 zone 限域、跨 cell 迁移协议）与 §5.1 第 3 条「单元化与分片叠加」的结论
- tidb-phase1-and-launch-readiness.md L-06 与 no-single-node-horizontal-scaling-20261001.md §5 仍写 redis-ha 由 -RedisReplicas 接入、拆分方向待定，需要指向注
- k8s_deploy.ps1:6226、:6375 与 deploy/k8s/README.md:762「共享库不许集群化」的说法，需要随 §20 加指向注
- spec_G4a.md 仍写 admitted:zone:{z} SET 与 SCARD（handoff-launch-readiness-decisions-20261015.md:202），规格给它加的文首注可以一并说明
