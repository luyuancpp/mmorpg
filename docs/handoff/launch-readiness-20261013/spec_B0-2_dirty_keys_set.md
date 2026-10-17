# 调研规格:删除 dirty_keys_set 的写入(C++ 存盘 Lua)(2026-10-16)

> **状态**:只读调研产出,**未落码**。由开服线会话 2026-10-16 的调研工作流生成(一名调研者出规格、一名核实者逐条反驳),原样保存,未经人工改写。
> **怎么用**:前半部分是调研者的规格;「# VERDICT」之后是核实者意见。**落码前必须先吸收核实意见里的 must / should**,被判 refuted 的事实以核实者为准。行号与工作区状态是 2026-10-16 03:48~04:35 的快照,工作区有多个会话在并行修改,落码前按函数名重新定位。
> **汇总、拍板项与推荐顺序**:见 [docs/PROGRESS.md](../../PROGRESS.md)「2026-10-16 开服线剩余工作调研」条目,以及交接 [handoff-launch-readiness-and-d1-20261013.md](../handoff-launch-readiness-and-d1-20261013.md) 文首 10-16 段。

## item
删除 dirty_keys_set 的写入(容量模型 §1.3 第 3 条 / A0-4、§4.2 G4a「第 0 批」、全局单点交接 §5.1 的 P2 行)
## already_done
无。代码里两处 SADD 仍在:redis_client.h:22、:50 我都打开看过。该文件工作区干净,mtime 2026-10-12 03:53:01,最后一次提交 440fe26b6(09-21)。docs/PROGRESS.md 里 dirty_keys 出现 0 次。handoff-launch-readiness-decisions-20261015.md 不涉及此项。10-13 以后的提交说明里也没有此项。已有的只是文档结论「没有读者」(shared-redis-split-by-purpose.md §2.5 第 1 条,capacity-model §1.3 / A0-4 / §4.2)。
## summary
【问题】C++ scene 节点每次真正写盘(周期存盘、退出 / 交接 / 回滚采纳、战斗结算与收件箱触发的存盘)执行两段 Lua 之一:redis_client.h:20-24 的普通版,或 :42-52 的 owner_epoch 守卫版。两段都在 SET 父键 `PlayerAllData:<id>` 之后,对硬编码的全局键 `dirty_keys_set` 做 SADD。

【根因】这是没有下游的「脏键集合」遗留设计。以下范围都找不到任何读者(SMEMBERS / SSCAN / SPOP / SREM / SCARD / DEL / UNLINK):本仓 C++、Go、Java 网关、tools、robot、deploy、bin、运维脚本、data_consistency_check,以及 Java 仓。这个键也没有 TTL。结果是它成了全服共享核心 Redis 上的单键,随「累计存过盘的玩家数」无界增长:
- 2000 万 PCU 时 SADD 上限约 7.8 万次/s(推算);
- 每 1 亿累计玩家约 7~10 GB,全在一个键里(粗估,未实测);
- 它是 Lua 里没有声明的键,直接挡住 Redis Cluster。

【做法】
1. 删掉两处 SADD。普通脚本常量从 `kSaveAndMarkLuaScript` 改名为 `kSaveLuaScript`:Mark 指的正是被删掉的标脏。脚本仍 `return 1`,EVALSHA / NOSCRIPT / OnSaved 流程与回包形状都不变。
2. 加一个不需要 Redis 的回归用例:通过已有的测试 peer 读客户端实际会发的两段脚本,断言每条 `redis.call` 的键参数都来自 `KEYS[]`。修复前必红。
3. 真 Redis 用例补「存盘不再写 `dirty_keys_set`」的断言。
4. 所有 scene 实例(含 Agones 里仍 Allocated 的旧实例)升级后,先用 OBJECT IDLETIME 确认没人再写也没人再读,再由运维一次性 UNLINK 存量键。禁止 DEL。

新旧版本混跑和回滚都安全,不需要开关。Java 版没有 Redis 玩家热缓存层,不适用,只在 PARITY 登记。
## est_files: 12
## current_facts
F1. 两段存盘 Lua 都硬编码写全局键 dirty_keys_set,且这个键不在 KEYS[] 里声明
   证据: 已打开核对。cpp/libs/engine/infra/storage/redis_client/redis_client.h:20-24:kSaveAndMarkLuaScript = SET KEYS[1] ARGV[1];SADD 'dirty_keys_set' KEYS[1];return 1。:42-52:kSaveIfGuardLuaScript 先比对 / 补种 KEYS[2],再 SET KEYS[1],:50 同样 SADD 'dirty_keys_set'。
F2. 异步客户端的发送路径;同步客户端已无生产调用方
   证据: 已打开核对。
- redis_client.h:906-907:ScriptSlot save_script_{kSaveAndMarkLuaScript} / guard_script_{kSaveIfGuardLuaScript}。
- :543-546:ScriptSlotFor 按 guard_key 是否为空选槽。
- :583-621:SendSaveCommand 发 EVALSHA,SHA 缺失时发 EVAL 全文。
- :293-302:重连时清 SHA 并重新 SCRIPT LOAD。
- :719-735:NOSCRIPT 时回退 EVAL。
- :749-750:注释写明普通脚本恒返回 1。
- 同步版 redis_client.cpp:68-75:MessageSyncRedisClient::Save 用 EVAL kSaveAndMarkLuaScript。它只被 cpp/tests/redis_test/redis_test.cpp:24 使用,而 redis_test 是死工程:include 路径 src/redis_client 已不存在,tools/scripts/run_cpp_tests.ps1:40-42 注明「redis_test 引用已删的 proto 树」。
F3. 生产写入方只有 scene 节点的玩家存盘
   证据: 已打开核对。
- cpp/libs/services/scene/core/system/redis.h:11:PlayerDataRedis = MessageAsyncClient<Guid, PlayerAllData>。
- redis.cpp:20:绑定 tlsRedis.GetZoneRedis()。
- player_lifecycle.cpp:4217 SavePlayerToRedisImpl:epoch≠0 走 guard 重载(:4382-4384),epoch==0 走普通 Save(:4389)。
- 全仓 grep MessageAsyncClient< 的生产使用只有这一处,其余是 currency_test / cross_zone_test 的测试引用。
F4. 调用频率:每次真正写盘执行一次 SADD,快路径跳过的不写
   证据: 已打开核对。
- 周期存盘:redis.cpp:195 默认 300 秒;:207 用 runEvery(1.0) 按 playerId % 300 分槽,每人每周期一次。
- 其余入口:player_lifecycle.cpp:2277、3008、3098、3116、3333、3433 的退出 / 交接 / 回执闸门;player_battle.cpp:127、:401 的战斗结算持久化;player_inbox_system.cpp:108、:408 的收件箱。
- 快照相等时跳过:player_lifecycle.cpp:4316 AllowsDirtySaveSkip。
- 推算(未实测):2000 万 PCU 时周期上限 2000 万/300 ≈ 6.7 万/s;加上峰时进退场约 1.1 万/s(capacity-model §7.2 的表),合计不超过约 7.8 万 EVALSHA/s,每次都对同一个键 SADD。旧 S3 表(§2.5)在 1000 万 PCU 时是 2.2~5.6 万/s。
F5. 全仓与 Java 仓都没有读者,也没有删除点
   证据: Grep(忽略大小写 dirty[_-]?keys)只命中:
- redis_client.h:22、:50;
- cpp/tests/currency_test/currency_test.cpp:821(注释)、:993(用例清理时 SREM 自己写的成员);
- 若干文档。

另用 grep -rI 补扫 bin、deploy、tools、robot、go、java、k8s、generated、run、lib、cpp/generated,0 命中。

全仓 SMEMBERS / SSCAN / SPOP 的调用点都读别的键:
- go/db/cmd/verifier/main.go:209 读 verify:enrolled:*;
- go/login/.../loginsession.go:73;
- go/scene_manager/.../orphan_cleanup.go:118-119 读频道 setKey;
- 其余在 match / world_channel。

tools/data_consistency_check/main.go:26-29 写明不连数据 Redis。k8s_zone_rollback.ps1:231-232 的共享键守卫清单里也没有它。

Java 仓 D:\luyuan\xuanming-server-mmo 只在 docs/porting/inventory/java-infra.md:279、:283、:390 出现,内容同样是「找不到读者」(build/ 下有一份同名副本)。
F6. 规模:没有 TTL、只增不减;成员是 PlayerAllData:<id>
   证据: - proto/common/database/player_cache.proto:30 的 message PlayerAllData 所在文件没有 package,所以 full_name 就是 "PlayerAllData"。
- 键的拼法见 redis_client.h:491。
- 每个曾经真正存过盘的玩家留一个成员,永不移除。
- 按 Redis 7.2 hashtable 编码粗估每成员 70~100 B(dictEntry + sds + 桶指针,含 jemalloc 取整),未实测,以 MEMORY USAGE 采样为准。据此每 1 亿累计玩家约 7~10 GB,全压在一个键上。
- 扩容时一次分配新桶数组,2^27 桶约 1 GB;DEL 会在主线程上 O(N) 地释放。
F7. 为什么是全局单点
   证据: - PlayerAllData 所在 Redis 按 CZ-2 契约全服物理共享:docs/design/cross-zone-scene-travel.md:33;shared-redis-split-by-purpose.md §2.4 第 4 条。所以所有 zone 的所有 scene 节点都在写同一个键。
- 该键不在 KEYS 中声明,在 Redis Cluster 下脚本不能执行。docs/design/cross-zone-matchmaking.md:298 已列为「共享库禁止切 Cluster」的 C++ 阻塞项;capacity-model A6 路线 C 也要求删掉它。
F8. redis_client.h 不是生成产物
   证据: - 文件位于 cpp/libs/engine/infra/storage/redis_client/,不在 generated/ 下。
- git check-ignore 没有忽略它。
- 在 tools/ 下 grep redis_client|kSaveAndMark|MessageAsyncClient,只命中 tools/scripts/stress_summarize.ps1:688 一句与 Go redis client 有关的注释,生成器不产出这个文件。
- 它编进 infra.vcxproj:31 与 cpp/libs/engine/infra/CMakeLists.txt:66。
F9. 现有测试与改动的关系
   证据: 已打开核对。
- currency_test.cpp:803-817 RedisGuardScriptComparesBeforeWrite 只按文本检查 GET KEYS[2]、SET KEYS[1]、cur == false、return 0 的顺序,删掉 SADD 不影响它。
- :938-1025 RedisGuardedSaveSemanticsAgainstRealRedis 的 cleanup 在 :993 SREM dirty_keys_set。
- MessageAsyncClientTestPeer 是友元(redis_client.h:66-68、:898-899),可以读私有的 save_script_ / guard_script_。
- 无 Redis 的用例构造方式是 `Client::HiredisPtr hiredis; Client client(hiredis);`(:408-413)。
F10. 包含方范围(决定重编译范围)
   证据: - redis_client.h 被 player_lifecycle.h:13 与 scene/core/system/redis.h:7 包含。
- player_lifecycle.h 被 21 个文件包含;redis.h 被 4 个文件包含:player_battle.cpp、player_lifecycle.cpp、scene_handler.cpp、nodes/scene/main.cpp。
- 测试侧有 bag_test、cross_zone_test、currency_test。
F11. Java 版没有对应物
   证据: - java-infra.md:279 记录 Java 版用 MyBatis 直写 MySQL game.player,外加 owner_epoch 与租约围栏,没有 Redis 热缓存层。
- 我在 Java 仓的 game-player-store、game-scene、game-pbmysql 里 grep dirty 键与 Redis 存盘脚本,没有命中。
## plan
- [cpp/libs/engine/infra/storage/redis_client/redis_client.h] 1. :20-24 常量改名为 kSaveLuaScript,删掉 SADD 行,脚本只剩 `redis.call('SET', KEYS[1], ARGV[1])` 与 `return 1`。保留 return 1 是为了让 OnSaved(:749-750 的语义)与 EVALSHA / NOSCRIPT 流程完全不变。不顺手改成裸 SET 命令:那会改变回包形状和重试路径,属于无关重构。
2. 常量上方加注释,写明:
   - 只访问 KEYS 里声明的键;
   - 2026-10-16 起不再写全服单键 dirty_keys_set,原因是无读者、无界增长、无 TTL、Lua 未声明键挡住 Cluster,并引用 capacity-model A0-4;
   - 新旧版本并存时旧节点继续写它也没有副作用;
   - 存量清理口径见 shared-redis-split-by-purpose.md §2.5 第 1 条。
3. :50 删除 kSaveIfGuardLuaScript 里的 SADD 行。:26-41 的注释末尾加一句「同样不写 dirty_keys_set,见 kSaveLuaScript」。
4. :199-201 注释「原子地写入并标脏」改为「原子地写入」。
5. :906 改为 `ScriptSlot save_script_{kSaveLuaScript};`。

其余不动:ScriptSlot、EnqueueSave、OnSaved 的逻辑都不改。
   工作区: git status 干净;mtime 2026-10-12 03:53:01;最后提交 440fe26b6(09-21)。非生成文件。同目录 redis_client.cpp 也干净。

包含方中 player_lifecycle.h / .cpp(M,10-15 22:35 / 22:36)、player_battle.cpp(M,10-16 03:21)、scene_handler.cpp(M,10-15 23:15)有别的会话未提交改动,但本项不改这些文件;redis.h / redis.cpp / nodes/scene/main.cpp 干净。

gRPC→muduo 迁移会话正在改 cpp/libs/engine/muduo_windows/src/muduo/net/ 下 6 个文件(Channel / Connector / EventLoop / TcpClient,均 M),和本文件没有重叠,但会影响同一次构建。thread_context/redis_manager.h 干净。冲突风险:低。
- [cpp/libs/engine/infra/storage/redis_client/redis_client.cpp] :70 的 `kSaveAndMarkLuaScript,` 改为 `kSaveLuaScript,`。只改这一个符号。MessageSyncRedisClient 没有生产调用方,但删掉它不在本项范围内(AGENTS §11.2)。
   工作区: git status 干净;mtime 2026-10-12 03:53:01。冲突风险:无。
- [cpp/tests/currency_test/currency_test.cpp] 1. MessageAsyncClientTestPeer(:24-97)末尾加一个只读访问器:`static std::vector<std::string_view> SaveScriptSources(const Client& client) { return {client.save_script_.source, client.guard_script_.source}; }`。注释说明它取的是客户端实际会发出的两段脚本,不经常量名,改名不影响用例。

2. 在 :817 之后新增 TEST(CurrencyTest, RedisSaveScriptsTouchOnlyDeclaredKeys),不需要 Redis。
   - 构造:`Client::HiredisPtr hiredis; Client client(hiredis);`。
   - 对两段脚本:ASSERT 非空;逐个找 "redis.call(",取第一个逗号之后、去掉前导空格的参数,EXPECT 以 "KEYS[" 开头,失败信息带出那一段脚本。
   - 同时 EXPECT_GT(调用数, 0),以及 EXPECT script.find("dirty_keys_set") == npos。
   - 注释写明:目前每条 redis.call 的第一个参数都是键,以后加无键命令(如 TIME)要同步改这里;用例关联 capacity-model A0-4。
   - 修复前红:两段脚本的 SADD 第一个参数都是 'dirty_keys_set'。

3. SyncRedisForTest(:871-912)加 `bool SIsMember(const std::string& set, const std::string& member)`,用 `SISMEMBER %b %b`,回 INTEGER 1 才为 true。

4. RedisGuardedSaveSemanticsAgainstRealRedis:
   - 第 1 步与第 3 步写入成功后各加 `EXPECT_FALSE(sync.SIsMember("dirty_keys_set", blobKey)) << "存盘不得再标记全局脏键集合";`。
   - 新增第 4 步「无 guard 存盘 → 写入且不标记」:加一个 lambda,调用 `client.Save(value, kPlayerId)`,跑 loop 直到回调或 3 秒超时;EXPECT 结果为 "saved"、父键等于新值、SIsMember 为 false。
   - cleanup 里的 `SRem("dirty_keys_set", blobKey)` 保留,并加注释:修复前的脚本会写入,保留清理是为了红跑也不留残余。
   - :821 的注释改为「用例会写并删除自己的键;清理时仍 SREM dirty_keys_set 的成员,防止修复前的二进制留下残余」。
   工作区: git status M,mtime 2026-10-16 02:23:58。别的会话的未提交改动只有 3 行::23 加 `#include "../test_config_helper.h"`;main 里加 `test_config::FindAndLoadTestConfig`。和本项无关,但必须用 Edit 局部修改,不能整文件覆盖。同目录 asset_op_*_test.cpp、client_gm_gate_test.cpp、currency_test.vcxproj 也是 M,本项不碰。另有未跟踪的 currency_test.vcxproj.metaproj,别处理它。冲突风险:中低。
- [docs/design/shared-redis-split-by-purpose.md] 1. §2.2 F 表 :128 的 dirty_keys_set 行:「写」列改为「C++ 存盘 SADD(2026-10-16 起已删除写入,未经 Codex 验证)」,TTL 列补「存量键待全部 scene 升级后 UNLINK」。
2. §2.3 :152-153 两行:「碰到的族」去掉 dirty_keys_set;:153 的常量名改为 kSaveLuaScript。
3. §2.5 第 1 条:原文保留,下面追加「(2026-10-16 更正)」小段,写明已删写入,并给出存量清理口径。清理口径放在这里,见 compat_and_rollout 第 4 条的命令序列。
4. :803 的内存增长行:去掉「+ dirty_keys_set 成员」,或标注「已停写」。
   工作区: git status 干净;mtime 2026-10-15 14:38:26。冲突风险:低。
- [docs/design/capacity-model-hok-scale.md] 在以下几处原文后追加「(2026-10-16 已落码,未经 Codex 验证;存量键清理见 shared-redis-split-by-purpose.md §2.5 第 1 条)」,数字不改:
- §1.3 第 3 条(:182)
- A0-4 第 2 小条(:415)
- §4.2 第 0 批(:662)
- A6 依赖(:568)

§2.5「核心常驻内存」行(:273)的公式说明中,dirty_keys_set 只写不读一处注明「已停写,待清存量」。
   工作区: git status 干净;mtime 2026-10-15 14:38:41。冲突风险:低。
- [docs/design/scene-owner-reentry-barrier.md] §6.2 :112-124 的「当前规则」Lua 代码块删掉 :121 的 SADD 行,在代码块后补一句「2026-10-16 起不再 SADD 全服键 dirty_keys_set(无读者,见 capacity-model A0-4)」。这一节描述的是现行规则,必须与源码一致。
   工作区: git status 干净;mtime 2026-10-15 04:32:49。冲突风险:低。
- [docs/design/new-player-first-enter-no-cold-read.md] :116「否则走 `SET` + `SADD dirty_keys_set`(:20-24)」改为「否则走只 SET 父键的普通脚本(:20-24;2026-10-16 起不再 SADD dirty_keys_set)」。
   工作区: git status 干净;mtime 2026-10-15 14:45:33。冲突风险:低。
- [docs/design/cross-zone-matchmaking.md] :298 的 C++ scene 阻塞项:在「存盘 Lua 访问未声明的全局 key `dirty_keys_set`」后追加「(2026-10-16 已删写入;hiredis 无 MOVED/ASK 与存盘脚本跨 hash tag 两项仍在)」。
   工作区: git status M,+46/-2,mtime 2026-10-16 03:31:37。别的会话改在 :3 与 :487 之后,不涉及 :298。工作副本是 CRLF,编辑时保持 CRLF。冲突风险:中。
- [docs/PROGRESS.md] 在文件末尾追加一条:「## 2026-10-16 删除存盘 Lua 的 dirty_keys_set 写入(A0-4 / G4a 第 0 批)(Claude,未编译、未测试,待 Codex 验证)」。内容包括:
- 改动文件清单;
- 无读者的证据摘要;
- 交给 Codex 的命令(同 verify_commands);
- 存量清理口径的指针;
- 「Java 版:不适用(无 Redis 玩家热缓存层),PARITY 已登记」。
   工作区: git status M,mtime 2026-10-16 03:50:11,别的会话持续在末尾追加。.gitattributes 配置了 merge=union。只在末尾追加,不改旧条目。冲突风险:低。
- [docs/handoff/handoff-launch-readiness-and-d1-20261013.md(可选)] 在文首进度区(:7-37 之间,别的会话 :28 新增行之后)加一行:「2026-10-16:删 dirty_keys_set 写入已落码,见 PROGRESS 同日条目;§4.2 第 0 批其余三项仍未做」。handoff-global-singletons-10m-ccu-20261013.md 的文首已把此项改派到本线,不再修改它。
   工作区: git status M,+2 行(:28),mtime 2026-10-15 22:15:56。冲突风险:中低。
- [D:\luyuan\xuanming-server-mmo\PARITY.md(Java 仓)] 在「功能对齐」表末尾追加一行:
- 功能:删除存盘 Lua 的全服键 dirty_keys_set 写入
- mmorpg 位置:cpp/libs/engine/infra/storage/redis_client/redis_client.h
- commit:落码提交号,落码后填
- Java 模块:—
- 版本:0.1.0-SNAPSHOT
- 状态:不适用(Java 内部:无 Redis 玩家热缓存层,MyBatis 直写 MySQL)
   工作区: git status M,+14/-2,mtime 2026-10-16 03:32:52,有别的会话未提交改动。只追加,不改既有行。冲突风险:中低。
- [D:\luyuan\xuanming-server-mmo\docs\porting\inventory\java-infra.md(Java 仓)] :390 第 8 条「dirty_keys_set 的读者」后加结论:mmorpg 全仓确认无读者,已于 2026-10-16 删除写入;Java 周期存盘不需要对应物。:283 hazard ① 同样补一句「已删除」。
   工作区: git status 干净;mtime 2026-10-12 02:00:50。build/match-global-20261015/ 下的同名副本是构建产物,不改。冲突风险:低。
## compat_and_rollout
1. **不涉及任何契约变化**:不改 proto,不改客户端可见内容,不改持久化结构。被删除的只是一个无人读取的旁路副作用。父键的写入、owner_epoch 的比对 / 补种、返回值(1 / 0)、EVALSHA → NOSCRIPT → EVAL 的回退,全部保持不变。

2. **新旧版本混跑与回滚都安全,不需要开关或灰度**:
   - 新节点不再 SADD,旧节点继续 SADD,没有读者,所以没有可观察的差异。
   - 回滚到旧二进制只是重新开始 SADD(键会被重建),无害。
   - 脚本文本变了,SHA 随之变化:每个连接通过 ScriptSlot 各自 SCRIPT LOAD,新旧 SHA 在 Redis 脚本缓存里并存,互不影响。旧 SHA 的缓存只有几百字节,不用处理。
   - scene 节点按任意顺序滚动都可以。

3. **存量清理的前置条件**:所有 scene 实例都换成新二进制。Agones Fleet 里仍处于 Allocated 的旧 GameServer 会继续写这个键,要等它们退出,或在维护窗口删掉。本机开发环境 / 压测每轮都 FLUSHALL,不用单独处理。

4. **存量清理口径(一次性运维)**:
   - 对象:共享核心 Redis。K8s 上是 `redis.<InfraNamespace>:6379` 的 DB 0(C++ 不发 SELECT)。
   - 口令:用环境变量 REDISCLI_AUTH 传入,或 kubectl exec 进 redis Pod 执行,不要把口令写在命令行上。
   - 按以下顺序执行:
     ① `OBJECT IDLETIME dirty_keys_set` **必须第一个执行**。SCARD 等读命令会刷新 LRU;OBJECT 不刷新。K8s 用 noeviction,该命令可用;LFU 策略下不可用。隔至少 10 分钟(建议跨一个完整的 300 秒存盘周期以上)再读一次,值应单调增大。增大说明已经没有写者,也没有读者;若中途归零,说明还有旧节点或未知读者,先停下排查。
     ② `TYPE dirty_keys_set`,期望 set。
     ③ `SCARD dirty_keys_set`,O(1),记录规模写进 PROGRESS。
     ④ `MEMORY USAGE dirty_keys_set`,用默认 SAMPLES 5 采样估算。**不要带 SAMPLES 0**:那是 O(N),会阻塞。也不要 SMEMBERS。
     ⑤ `UNLINK dirty_keys_set`。主线程只做 O(1) 的摘键,内存由后台 lazyfree 线程释放。redis:7.2 / latest 都支持。**禁止 DEL**:大 set 的 DEL 会在主线程上逐个释放成员,阻塞数秒。
     ⑥ `EXISTS dirty_keys_set` 期望为 0;24 小时后再查一次,确认没有旧二进制把它建回来。若被建回,找到并处理掉旧实例后重复 ⑤。
   - 兜底:如果托管 Redis 禁用或改名了 UNLINK,就循环执行 `SPOP dirty_keys_set 1000`,批与批之间短暂停顿,直到 SCARD 为 0。每批 O(1000),不阻塞。不要用 SSCAN+SREM 全量遍历,SPOP 更简单。
   - 预期效果:used_memory 下降。RSS 不一定马上回落(jemalloc)。
   - 在 staging / prod 上执行这一步属于触碰共享环境,须用户明确授权(AGENTS §9)。

5. **与回档、合服、清库的关系**:FLUSHDB 会顺带清掉这个键。k8s_zone_rollback.ps1 的共享键守卫清单里本来就没有它,merge_zone 也不读它。这些脚本都不用改。

6. **集群化**:这是 A6 路线 C 的必要条件之一,但不是充分条件。守卫脚本的 KEYS[1]=`PlayerAllData:<id>` 与 KEYS[2]=owner_epoch 键仍然没有共享 hash tag。交付说明里不得声称「已可上 Cluster」。
## tests
- 新增 CurrencyTest.RedisSaveScriptsTouchOnlyDeclaredKeys(currency_test.cpp,不连 Redis,不依赖墙钟):通过 MessageAsyncClientTestPeer::SaveScriptSources 拿到客户端实际会发的普通脚本与守卫脚本,断言每条 redis.call 的第一个参数以 KEYS[ 开头,且脚本里不含 dirty_keys_set。修复前红:两段脚本的 SADD 'dirty_keys_set' 都会违反这两条;修复后绿。它取的是客户端实际发出的脚本,不经常量名,所以常量改名不影响用例。
- 加强 CurrencyTest.RedisGuardedSaveSemanticsAgainstRealRedis(读 MMORPG_TEST_REDIS_ADDR,未设置时 GTEST_SKIP;等待都有 3 秒上限):
- 第 1、3 步写入成功后,断言 SISMEMBER dirty_keys_set <blobKey> 为 false;
- 新增第 4 步:无 guard 的普通 Save 应回调 saved、父键等于新值,且 SISMEMBER 为 false;
- 修复前在真 Redis 上必红;
- cleanup 保留 SREM,保证红跑也不留残余。
- 回归保持绿:CurrencyTest.RedisGuardScriptComparesBeforeWrite。它的文本断言(GET KEYS[2] 在 SET KEYS[1] 之前、cur == false、return 0 在写入之前)不受删除 SADD 的影响。
- 编译回归:cross_zone_test 与 bag_test 都包含 player_lifecycle.h,随之包含 redis_client.h。两者只需编过并保持原有结果,不新增用例。
## verify_commands
- 静态核对:在 D:\luyuan\xuanming-server-mmo-cpp 执行 `git grep -n "dirty_keys_set" -- cpp go java tools robot deploy bin`。通过标准:只命中 cpp/tests/currency_test/currency_test.cpp 的注释、cleanup SREM 与断言字符串。再执行 `git grep -n "kSaveAndMarkLuaScript" -- cpp`,通过标准:0 命中。
- C++ 编译,在 D:\luyuan\xuanming-server-mmo-cpp 执行,必须串行 /m:1,并发会报假的 C1041 / LNK1104。MSBuild 路径按 tools/scripts/run_cpp_tests.ps1 的 Resolve-MSBuild 取。按以下顺序:
1. `& $msbuild cpp\libs\engine\infra\infra.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
2. `& $msbuild cpp\libs\services\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
3. `& $msbuild cpp\nodes\scene\scene.vcxproj /m:1 /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`

通过标准:0 个 error。若失败点在 cpp/libs/engine/muduo_windows 或 grpc 相关文件(那里有 gRPC→muduo 会话的未提交改动),记下第一条错误,作为与本项无关的问题报告,不要去修。
- 单测,在仓库根执行。currency_test 的 main 会通过 test_config_helper 找配表,必须从仓库根启动:
1. `pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter currency_test`,通过标准:currency_test PASS,输出里 CurrencyTest.RedisSaveScriptsTouchOnlyDeclaredKeys 为 OK,RedisGuardScriptComparesBeforeWrite 仍为 OK,两个真 Redis 用例此时显示 SKIPPED。
2. 依次执行 `pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter cross_zone_test` 与 `-Filter bag_test`,通过标准:能编过,且结果与改动前一致。

失败时保留 MSBuild 的第一条 error 行和 gtest 的 FAILED 段落。
- 真 Redis 用例,只用专用的本机容器,不要指向压测或开发共用的 Redis:
1. `docker run -d --rm --name currency-redis-test -p 127.0.0.1:6390:6379 redis:7.2`
2. `$env:MMORPG_TEST_REDIS_ADDR='127.0.0.1:6390'; pwsh -File tools/scripts/run_cpp_tests.ps1 -Filter currency_test`,通过标准:RedisConnectedSaveDoesNotBypassFailedPending 与 RedisGuardedSaveSemanticsAgainstRealRedis 实际执行(不是 SKIPPED)并 PASS。
3. `docker exec currency-redis-test redis-cli EXISTS dirty_keys_set`,通过标准:输出 0。
4. `docker stop currency-redis-test`,并清掉该环境变量。
- (可选)修复前红的证据:在独立的 git worktree(基于 HEAD)里只打新增测试那一部分补丁,构建 currency_test 并运行。期望 RedisSaveScriptsTouchOnlyDeclaredKeys FAIL,两段脚本都报出 'dirty_keys_set' 不是 KEYS[]。不要在主工作区用 stash、reset 或 checkout 回退。没做这一步就如实报告「红跑证据未执行」。
- 部署后核对,在 staging 执行,需用户授权后由运维进行:按 compat_and_rollout 第 4 条的 ①~⑥ 执行。通过标准:OBJECT IDLETIME 两次读数单调增大;UNLINK 之后 EXISTS 为 0;24 小时后 EXISTS 仍为 0。SCARD 与 MEMORY USAGE 的读数记进 PROGRESS。
## doc_updates
- docs/PROGRESS.md:在末尾追加 2026-10-16 条目,内容包括改动清单、无读者的证据、给 Codex 的命令、存量清理的指针,并写明 Java 版不适用(Java 仓 PARITY 已登记)。
- docs/design/shared-redis-split-by-purpose.md:改 §2.2 F 表 :128 行与 §2.3 :152-153 行(常量改名、去掉 dirty_keys_set 族);§2.5 第 1 条追加「2026-10-16 更正」和存量清理口径;:803 行注明已停写。
- docs/design/capacity-model-hok-scale.md:§1.3 第 3 条、A0-4(:415)、A6 依赖(:568)、§4.2 第 0 批(:662)标注「已落码、未验证」;§2.5 核心常驻内存行(:273)注明已停写、待清存量。正文数字不改。
- docs/design/scene-owner-reentry-barrier.md §6.2:「当前规则」的 Lua 代码块删掉 SADD 行,补一句说明。
- docs/design/new-player-first-enter-no-cold-read.md:116:改写普通存盘路径的描述。
- docs/design/cross-zone-matchmaking.md:298:注明 dirty_keys_set 已删,其余阻塞项仍在。保持 CRLF。
- 可选:docs/handoff/handoff-launch-readiness-and-d1-20261013.md 文首进度区加一行。
- 不改:docs/handoff/launch-readiness-20261013/spec_G4a.md 与 spec_G3.md(规格副本)、handoff-backlog-2026-09-05.md、handoff-global-singletons-10m-ccu-20261013.md(文首已改派)、docs/design/guild-phase2/04-asset-channel.md:1410(B4c 历史提案里的拟议脚本,不是现行实现;该文件还有别的会话的未提交改动)。都属于历史记录,按 AGENTS §5 不回改。
- Java 仓:PARITY.md 追加一行「不适用(Java 内部)」;docs/porting/inventory/java-infra.md 的 :283 与 :390 第 8 条补上结论。
## java_parity
不需要对等的代码改动,结论是「不适用(Java 内部)」。

1. dirty_keys_set 只是 C++ scene 写 Redis 存盘脚本时的旁路副作用,不属于 AGENTS §12 所说的两版共享契约:它不是客户端帧格式,不是 proto 客户端消息,不是 tip 码,也不是配表。
2. Java 版的玩家持久化不经过 Redis 热缓存:java-infra.md:279 记录为 MyBatis 直写 MySQL game.player,外加 owner_epoch、owner_released 与租约围栏。我在 Java 仓 game-player-store、game-scene、game-pbmysql 里 grep,没有任何 dirty 键或 Redis 存盘脚本;整个 Java 仓只在移植调研文档 java-infra.md:279、:283、:390 提到这个键,而且是作为待确认的问题(第 8 条)。

按 §12 仍须登记:Java 仓 PARITY.md 追加一行,状态写「不适用(Java 内部:无 Redis 玩家热缓存层)」;java-infra.md 第 8 条注明「mmorpg 已确认无读者并删除写入,Java 周期存盘不需要对应物」。本仓 PROGRESS 条目写明对应 Java 版本 0.1.0-SNAPSHOT、状态不适用。
## risks
- 仓库外可能存在读者,静态分析无法排除:运维手工脚本、外部 redis_exporter 的 check-keys 配置等(deploy/ 里 grep 为 0)。缓解:先只删写入,存量键暂时保留;UNLINK 之前用 OBJECT IDLETIME 观测(任何读写都会刷新 LRU),出现读者就停下排查,必要时回滚二进制,回滚无害。
- 大 key 操作阻塞主线程:清理时只能用 UNLINK,或 SPOP 分批兜底。禁止 DEL、SMEMBERS、MEMORY USAGE ... SAMPLES 0。IDLETIME 必须在其他读命令之前读。
- Agones 中处于 Allocated 的旧 scene 实例可能长时间存活,UNLINK 后会把键重建。需要在 24 小时后复查 EXISTS,必要时再执行一次 UNLINK。
- 构建风险:cpp/libs/engine/muduo_windows/src/muduo/net/ 下有 6 个文件被 gRPC→muduo 会话改着、未提交,可能导致与本项无关的编译失败。redis_client.h 经 player_lifecycle.h 进入 21 个翻译单元,scene 库会大面积重编。MSBuild 必须 /m:1。
- currency_test.cpp 有别的会话未提交的改动(test_config_helper 与 main 里的配表加载),只能用 Edit 局部修改;测试运行依赖从仓库根找到配表。cross-zone-matchmaking.md 工作副本是 CRLF,而且有别的会话的改动,编辑时注意保持行尾。
- 删掉 SADD 只是 Redis Cluster 的必要条件之一:守卫脚本的两个键仍然没有共享 hash tag,C++ 侧也没有 MOVED/ASK 处理。不得夸大为「已可集群化」。
- 清理后内存回落有滞后:lazyfree 在后台释放,jemalloc 不一定马上把 RSS 还给系统。以 used_memory 为准。
- 附带观察(不在本项范围内):MessageSyncRedisClient 只剩死工程 redis_test 在用。要删得另开任务,本项不顺带处理(AGENTS §11.2)。
- 规模数字(每成员 70~100 B,SADD 上限约 7.8 万/s)都是推算,未实测。按 AGENTS §6,不得据此声称性能提升;实际规模以 staging 上 SCARD 与 MEMORY USAGE 的读数为准。
## open_decisions
- 存量 dirty_keys_set 的 UNLINK 何时、由谁在 staging / prod 的共享核心 Redis 上执行。这是触碰共享环境的运维操作,AGENTS §9 要求用户明确指定环境并授权。代码改动本身不依赖这一步,可以先合入。

# VERDICT
## overall
规格的核心结论成立,我没有找到必须推翻的点。依据如下:\n- 两处 SADD 确实存在,我已打开核实。\n- 本仓 C++、Go、Java、tools、robot、deploy、bin、run、data 以及 Java 仓,都找不到 dirty_keys_set 的读者或删除点。我用 git grep、Grep 和 grep -rIl 分别核实过。\n- redis_client.h 不是生成产物,工作区干净。\n- 方案只删 SADD、保留 return 1,EVALSHA → NOSCRIPT → EVAL 流程与回包形状都不变,新旧版本混跑和回滚都无害,不需要开关。\n- Java「不适用」的判断成立。\n- 新增的无 Redis 用例修复前必红,修复后必绿。\n\n运行期语义上没有 must 级问题,问题集中在交付一致性和运维口径:\n1. verify 的通过标准与 plan 自相矛盾:新注释会被 dirty_keys_set 的 grep 命中。\n2. 常量改名漏改 cross-zone-scene-travel.md:35。\n3. 「SADD 上限约 7.8 万/s」不是上限:漏了必真写的资产提交、战斗、收件箱和交接存盘。\n4. 用 OBJECT IDLETIME 证明「无旧写者 / 无读者」不成立:快路径会跳过存盘,AOF 子进程运行期间也不刷新 LRU。应改用版本盘点作门禁,并补上从备份恢复后的重做步骤。\n5. 清理口径应落到 docs/ops。\n6. cross-zone-matchmaking.md 与 PROGRESS.md 的工作区状态已过期,还在被并行修改,落码前要重新核对。\n\n修完这些 should 项,另一个 Claude 就可以照着落码。
## fact_checks
- [confirmed] 两段存盘 Lua 都硬编码写全局键 dirty_keys_set,且未在 KEYS[] 声明
   我亲自打开看过。redis_client.h:20-24 是 kSaveAndMarkLuaScript(SET KEYS[1] ARGV[1];:22 SADD 'dirty_keys_set' KEYS[1];return 1)。:42-52 是 kSaveIfGuardLuaScript,:50 同样是 SADD。文件有 BOM、LF 行尾;git status 干净;mtime 2026-10-12 03:53:01;最后提交 440fe26b6(09-21)。
- [confirmed] 异步客户端的发送路径;同步客户端已无生产调用方
   我亲自看过:
- redis_client.h:906-907 是 ScriptSlot;:543-546 是 ScriptSlotFor;:583-621 是 SendSaveCommand(EVALSHA / EVAL);:293-302 重连时清 SHA 并重新 LOAD;:719-735 是 NOSCRIPT 回退;:746-750 注释写明普通脚本恒返回 1。
- 友元声明在 :67-68 / :898-899。ScriptSlot 位于 :467,属 private 段(:388 起)。
- redis_client.cpp:68-75 用 EVAL kSaveAndMarkLuaScript。
- 死工程:redis_test.cpp:9 include 了不存在的 src/redis_client/redis_client.h,:24 调 c.Save;run_cpp_tests.ps1:38-41 注明它是死工程。
- [confirmed] 生产写入方只有 scene 节点的玩家存盘
   我亲自看过:
- redis.h:11 定义 PlayerDataRedis;redis.cpp:20 绑定 GetZoneRedis。
- player_lifecycle.cpp:4382-4384 是带 guard 的 Save,:4389 是普通 Save。
- git grep 'MessageAsyncClient<':生产实例化只有 redis.h:11。其余命中是 LoadFailureReason 类型引用,或 currency_test。
- [refuted] 调用频率:每次真写一次 SADD;2000 万 PCU 时 SADD 上限约 7.8 万次/s
   「每次真写执行一次」成立。快路径跳过在 player_lifecycle.cpp:4316;周期存盘在 redis.cpp:195、:207、:227。但「上限」不成立:
- 入口清单漏了资产通道:asset_op_system.cpp:134 的 g_persistFn 指向 SavePlayerForAssetCommit,:256 调用。player_lifecycle.cpp:4212-4215 传的是 allowSkipWhenPersisted=false,也就是每次资产提交必真写。
- 7.8 万只算了周期存盘(6.7 万)加进退场(1.1 万)。规格自己列出的战斗结算(player_battle.cpp:401)、收件箱(player_inbox_system.cpp:408)、跨节点交接存盘都没有计入。
- capacity-model §2.3 的 N_save 行明确写了「不含战斗结算和资产指令触发的存盘」。
- 规格引用的「旧 S3 表 2.2~5.6 万/s」出自 §2.3 存盘管线,不是 §2.5。

结论:7.8 万只是部分下界口径,不是上限。
- [confirmed] 全仓与 Java 仓没有任何读者或删除点
   我分别用三种方式核过:
1. git grep -i 'dirty[_-]?keys':非 docs 只命中 redis_client.h:22、:50,以及 currency_test.cpp:821(注释)、:993(SREM)。
2. Grep 工具全仓扫:13 个文件,全部是文档或上述三个文件。
3. 对 bin、deploy、tools、robot、run、go、java、data 逐目录 grep -rIl:0 命中。

全仓的 SMEMBERS、SSCAN、SPOP 读的都是别的键:go/db/cmd/verifier/main.go:209、loginsession.go:73、orphan_cleanup.go:118、match、scene_manager mirrors。所有 SCAN 都带了具体前缀,没有 '*' 全量扫描。

其余排查:
- data_consistency_check/main.go:26-29 写明不连数据 Redis。
- k8s_zone_rollback.ps1:649-660 的守卫清单里没有这个键。
- merge_zone 的 integration_test.go:97 只用 DBSize 判库是否为空。
- Java 仓 git grep 只命中 docs/porting/inventory/java-infra.md:279、:283、:390;game-player-store、game-scene、game-pbmysql 的 main 代码里没有 dirty 或 SADD。
- [unverifiable] 规模:没有 TTL、只增不减,成员是 PlayerAllData:<id>;每成员 70~100 B,每 1 亿累计玩家 7~10 GB
   结构部分我核实过:
- player_cache.proto 无 package,:30 是 message PlayerAllData。
- redis_client.h:491 拼出 full_name():id。
- 全仓无删除点。merge_zone 删父键时也不 SREM,所以集合里还会残留已删玩家。

字节数和总规模是推算,没有实测,按规格自己的说法以 MEMORY USAGE 采样为准。2^27 桶 × 8B ≈ 1 GiB 的算术正确。
- [confirmed] 它是全局单点:全服物理共享核心 Redis 的 DB 0,且挡住 Cluster
   我亲自看过:
- cross-zone-scene-travel.md:33 的 CZ-2 写明物理共享契约。
- k8s_deploy.ps1:3363-3367 给 zoneredis 配的是 host redis.${InfraNamespace}、db: 0。
- redis_manager.cpp 里没有 SELECT。
- cross-zone-matchmaking.md:298 把它列为阻塞项。

细节:Redis 7 只在未声明键落到非本地 slot 或不同 slot 时报错,所以「不能执行」是近似说法,结论不变。
- [confirmed] redis_client.h 不是生成产物
   - git check-ignore 返回 1,未被忽略。
- 它在 infra.vcxproj:31(ClInclude)、:85(cpp),以及 CMakeLists.txt:66。
- tools 下 grep 只命中 stress_summarize.ps1:688 与 test_stress_summarize.ps1 的 Prometheus label 名 redis_client,两者都与生成器无关。
- [confirmed] 现有测试与改动的关系(含行号)
   语义成立。RedisGuardScriptComparesBeforeWrite(工作副本 :804-818)只做文本顺序断言,删 SADD 不影响它。:993 是 SREM。无 Redis 的构造写法见 :411-412。

行号有漂移:
- 真 Redis 用例 RedisGuardedSaveSemanticsAgainstRealRedis 在工作副本 :957(HEAD :956),不是规格写的 :938。
- TestPeer 在工作副本 :25-98。
- SyncRedisForTest 在 :872-913。

原因是别的会话在 :23 加了一行 include。
- [confirmed] 包含方范围:player_lifecycle.h:13 与 redis.h:7;player_lifecycle.h 有 21 个包含方
   两个 include 行都已核实。redis.h 的包含方是 player_battle.cpp、player_lifecycle.cpp、scene_handler.cpp、main.cpp,另有 redis.cpp 用相对路径包含。我 grep 到 player_lifecycle.h 的包含文件是 22 个,与 21 有 1 个出入,不影响结论。
- [refuted] 工作区状态(规格写:cross-zone-matchmaking.md +46/-2、mtime 03:31:37;PROGRESS.md mtime 03:50:11)
   规格记录的状态已过期:
- cross-zone-matchmaking.md 现在是 +49/-2,mtime 2026-10-16 03:55:48,改动块在 :3 与 :487 之后。
- PROGRESS.md 现在 mtime 03:56:05、+252 行,工作副本是 CRLF。

这两个文件仍在被别的会话持续改动。其余文件与规格一致:redis_client.h/.cpp、shared-redis-split、capacity-model、scene-owner-reentry-barrier、new-player-first-enter 干净;currency_test.cpp 为 M,+3 行,mtime 02:23:58;Java PARITY.md 为 M,+14/-2,mtime 03:32:52。
- [confirmed] Java 版没有对应物,判「不适用」
   java-infra.md:279 写明 MyBatis 直写 MySQL、没有 Redis 热缓存层。:281 记录 Java 还缺周期存盘。Java 主代码里没有 dirty 或 SADD 存盘脚本。这个键不属于 AGENTS §12 规定的共享契约。
## plan_problems
- [should] verify_commands 的通过标准与 plan 自相矛盾。plan 会在 redis_client.h 的新注释里写入 dirty_keys_set(kSaveLuaScript 上方「不再写全服单键 dirty_keys_set」,以及守卫脚本注释「同样不写 dirty_keys_set」)。但第一条静态核对要求 `git grep dirty_keys_set -- cpp ...` 只命中 currency_test.cpp。正确实现也会被 Codex 判成失败。
   修法: 二选一:(a)源码注释里不写键名,只写「脚本只访问 KEYS[] 声明的键,见 capacity-model A0-4」;(b)把通过标准改成「redis_client.h 的 R"(...)" Lua 原文里 0 命中,注释命中允许」。以新增的 RedisSaveScriptsTouchOnlyDeclaredKeys 为机械判据。
- [should] 常量 kSaveAndMarkLuaScript 改名后,docs/design/cross-zone-scene-travel.md:35 留下悬空引用。CZ-4 原文写「`kSaveAndMarkLuaScript` 原子比对」,这句本身已经错了:比对在 kSaveIfGuardLuaScript 里。该文件不在 doc_updates 里,工作区干净,mtime 10-14。verify 只对 cpp 做 git grep,抓不到这类引用。
   修法: 把 cross-zone-scene-travel.md:35 加进 doc_updates,改成 kSaveIfGuardLuaScript。verify 的 grep 范围扩到 `-- cpp docs/design`,docs/handoff 下的规格副本作为历史记录允许命中。或者不改名:但这样名字里的 Mark 会失真,违背 §11.2「命名表达业务含义」。倾向改名并补上文档。
- [should] 频率口径把部分量写成了上限。summary 写「2000 万 PCU 时 SADD 上限约 7.8 万次/s」,但 7.8 万只含周期存盘与进退场。漏掉的有:
- 资产通道 SavePlayerForAssetCommit。asset_op_system.cpp:134、:256;player_lifecycle.cpp:4212-4215 传 allowSkip=false,每次资产提交必真写;
- 战斗结算与收件箱,规格自己列了入口却没有计入;
- 跨节点交接存盘。

capacity-model §2.3 也明说 N_save 不含战斗与资产触发的存盘。
   修法: summary、PROGRESS 条目和代码注释(如引用)统一改写为:「周期存盘 + 进退场约 7.8 万/s(推算),另加战斗结算、资产提交、收件箱、跨节点交接触发的事件驱动存盘(未量化)」。不写「上限」。§2.5 的引用更正为 §2.3。
- [should] 存量清理第 ① 步把「OBJECT IDLETIME 单调增大」当成「已经没有写者、也没有读者」的证明,这个推理不成立:
- 旧二进制只在真写时才 SADD。在线玩家快照相等时走快路径跳过(player_lifecycle.cpp:4316),低活跃时段,旧节点可以很久不写这个键;
- K8s Redis 开着 appendonly(redis.yaml:131)。BGSAVE / BGREWRITEAOF 子进程运行期间,lookupKey 不更新 LRU,读写都不会刷新 IDLETIME。

两种情况下 IDLETIME 都会继续增大,却证明不了没有旧节点或读者。
   修法: UNLINK 的前置门禁改成版本盘点:用 release-checklist §3 的版本追溯证据点,或者 kubectl 核对 scene Deployment / Fleet / GameServer 的镜像与 build-info,确认所有 scene 实例(含仍 Allocated 的 GameServer)都已换成新版本。IDLETIME 只作辅助观测。保留 24 小时后的 EXISTS 复查,再补一条:从清理前的 AOF / RDB 备份恢复 Redis 后,要重新执行 ⑤⑥。
- [should] 存量清理口径(运维一次性操作)只写进设计文档 shared-redis-split-by-purpose.md §2.5,不符合 AGENTS §5「运维手册 → docs/ops/」。发版的人按 docs/ops 或 deploy/k8s/README 走流程,看不到这一步,存量键可能长期留着。
   修法: 不新建 md(AGENTS §9)。在现有运维文档里追加一小节:可以是 docs/ops/release-checklist.md 的「发版后一次性项」,也可以放在 deploy/k8s/README.md 现有 redis-cli 清理片段(:539 附近)旁边,写明 ①~⑥ 和授权要求。设计文档 §2.5 只留指针。
- [should] 工作区冲突评估已过期。cross-zone-matchmaking.md 在规格写完后又被改过:+46 变成 +49,mtime 从 03:31 变成 03:55,工作副本是 CRLF;PROGRESS.md 也在持续追加。另外,若按下文把 player-inbox-service.md 纳入修改,它也是别的会话的 M 状态。
   修法: 落码前对每个目标文件重新执行 `git status --short` 与 `git diff --stat`,一律用 Edit 局部修改并保持行尾。cross-zone-matchmaking.md:298 只是给阻塞项补一句注解,可以降为可选,或者放到别的会话提交之后再改。PROGRESS 只在末尾追加,保持 CRLF。
- [nit] doc_updates 漏了同类引用:
- capacity-model-hok-scale.md:551(A6 路线 C「Lua 里去掉全局键:dirty_keys_set 删掉」);
- guild-phase2/08-save-owner-fence.md:44(「相等则 SET + SADD」「结构逐行相同」);
- 多处 `redis_client.h:42-52` / `:20-24` / `:31-37` / `:212` / `:491` 的行号引用会随删行与新增注释漂移,例如 player-inbox-service.md:965(M)、new-player-first-enter-no-cold-read.md:115、shared-redis-split :127 / :152 / :153 / :213。
   修法: :551 照 :568 的写法加注。08-save-owner-fence.md:44 加「(2026-10-16 起已删 SADD)」,或者列入「不改」清单并写明理由。行号引用尽量改成符号引用;不改的话,在 PROGRESS 里注明「redis_client.h 行号整体偏移」。
- [nit] plan 在 redis_client.h 常量上方写入带日期的发布史(2026-10-16 起、新旧并存无副作用、清理口径见某文档)。这属于变更记录,不是约束说明,与 §11.2「注释解释为什么、约束和风险」的要求不符,而且会过时。
   修法: 注释只保留不变量:「本脚本只访问 KEYS[] 声明的键,不得写全局键(Cluster 与容量,见 capacity-model A0-4)」。发布、混跑与清理的说明放进 PROGRESS 和运维文档。
- [nit] Java PARITY 行的状态写成「不适用(Java 内部:…)」。PARITY 里「Java 内部」这个标签是给只有 Java 才有的功能用的,见 :40、:47。本项是 mmorpg 内部改动。
   修法: 状态改为「不适用(mmorpg 内部:Java 无 Redis 玩家热缓存层,MyBatis 直写 MySQL)」,mmorpg commit 列等提交后再回填。
- [nit] 真 Redis 用例把「普通 Save 不标记」作为第 4 步,塞进注释为「Q4 守卫脚本语义」的用例,两类关注点混在一起。断言也只针对 dirty_keys_set 这一个键名,覆盖不到将来新加的其他未声明键。
   修法: 拆成独立的 TEST,比如 RedisPlainSaveWritesOnlyDeclaredKeysAgainstRealRedis。在专用容器上先清空自己的键,存盘后用 SCAN 断言库里只有 blobKey(守卫版只有 blobKey 和 guardKey)。修复前它同样会红。
- [nit] 无 Redis 用例通过友元 peer 新增访问器去读私有的 save_script_ / guard_script_。理由「常量改名不影响用例」比较弱,而且多了一处私有接口暴露。现有的 RedisGuardScriptComparesBeforeWrite 直接读常量。
   修法: 两种都能接受。推荐与现有用例一致,直接读 kSaveLuaScript / kSaveIfGuardLuaScript,不扩 peer。若保留 peer,注释写明理由:被检的是槽位里实际发出的那段脚本。
## missed
- 存盘入口 asset_op_system.cpp:134、:256:SavePlayerForAssetCommit 传 allowSkip=false,每次资产提交必真写,也就必然执行一次 SADD。频率分析里漏了这一项。
- 周期存盘的实际调用点是 redis.cpp:227。规格只引用了 :195、:207。
- docs/design/cross-zone-scene-travel.md:35:CZ-4 引用了 kSaveAndMarkLuaScript,改名后悬空。
- docs/design/capacity-model-hok-scale.md:551:A6 路线 C 提到删 dirty_keys_set,规格只改了 :568。
- docs/design/guild-phase2/08-save-owner-fence.md:44:写着「相等则 SET + SADD」「结构逐行相同」,描述的是现行脚本。
- 多处 redis_client.h 行号引用会漂移:player-inbox-service.md:965(别的会话 M)、new-player-first-enter-no-cold-read.md:115、shared-redis-split-by-purpose.md:127/:213 等。
- 恢复路径:Redis 若从清理前的 AOF/RDB 备份恢复(K8s 是 PVC + appendonly),dirty_keys_set 会回来,清理口径里要写明恢复后重新执行。
- UNLINK 前的版本盘点门禁:要用版本追溯或镜像核对,确认所有 scene 与 Allocated GameServer 都已升级。IDLETIME 做不到这一点。
- 运维手册落点:存量清理应写进 docs/ops,或 deploy/k8s/README 已有的 redis-cli 清理段,不能只写在设计文档 §2.5。
- verify 的 kSaveAndMarkLuaScript 残留 grep 只扫 cpp,没扫 docs/design。
