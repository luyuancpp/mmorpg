# 整 zone 回档清理工具 zone_rollback_discard 设计（R9）

> **状态（2026-10-19）**：设计稿，**未落码**。负责人 2026-10-18 授权「按最标准的去做，数据一致性而且可水平扩展」，下文 7 项与三个小项据此拍板。预计改动本仓 **73 个文件**（新增约 52、修改约 21），超过 AGENTS §10.2 的 30 文件门槛，**落码前须负责人确认规模与分批**。两面对抗复核（数据一致性、水平扩展与运行）各提出 14 条意见，**尚未并入设计**，见 §6；落码第一步是先把它们并入。
>
> 上游：回档语义裁决与脚本侧落码见 [zone_data_rollback.md](zone_data_rollback.md)「2026-10-18 修订」；工具契约见 `tools/scripts/k8s_zone_rollback.ps1` 文件头「zone_rollback_discard 契约」；交接见 [godb-pipeline-impl-handoff-20261015.md](../handoff/godb-pipeline-impl-handoff-20261015.md) §7.4。
>
> 调研缺口：维护围栏与三个小项的两路专项调研因认证错误（403）未跑完，下文相关决定由综合者依据另两路调研与代码作出，落码前应补一轮针对性核实。本文由设计工作流的输出整理而成，未经人工逐句改写。

## 1. 决定

| # | 决定 | 一句话理由 |
|---|---|---|
| #1 维护围栏 | 复用合服的广播围栏 `merge:in_progress:<zone>`(`placement.MergeFenceKey`),值为 `zone_rollback_discard:<rid>`,永久不设 TTL。同一段 Lua 里原子地再建 `player:save_barrier:maintenance:<zone>`=rid(`barrierrestore.MaintenanceKey`)。工具新增两个模式:`fence`(第 0 步 g:在 preflight 之后、zone-down 之前立围栏,并在有界时间内等他区在线的本 zone 玩家下线);`release`(Step 6b:按值 CAS 撤除)。他区在线的玩家「等下线,不踢」,超时退出码 5,围栏保留。 | 现网所有版本的 data_service(GetPlayerHomeZone 的 home_zone_merging、setPlayerZoneIfAbsentScript)、scene_manager(rejectHomeZoneMerging,不分腿)和 guild 都只看 EXISTS 就拦,不用改任何在线服务,滚动升级期间也生效。PITR 要几个小时,中间没有常驻进程续期,TTL 到期就是 fail-open,所以用永久键,先例是 barrierrestore Guard 的 routeGuardValue。不踢:仓库没有按归属 zone 踢人的通道,踢下线产生的退出存盘本来也要丢;「闸住入口、有界排空」就是标准的 drain 做法。 |
| #2 纪元抬升 | 做。范围是本次 purge 要处理的玩家(rollback / torn / post_t_candidate)里停服时 owner_epoch>0 的那些。把 `player:<id>:owner_epoch` 和当前世代 topic 下两张围栏表的 `consumer:applied_epoch:<topic>:<id>:player_database(_1)` 抬到 C'=owner_stop+1,键缺失也建。在每个玩家删除的同一段 Lua 里先抬后删,只升不降,可重入。MySQL 行内围栏不抬。owner_stop 取自停服基线,并要求它就是所有 epoch 来源的最大值,否则判 owner_behind,归 blocked。 | 这就是 fencing token 的「撤销全部租约」。PITR 只退回了行围栏;C++ 父快照的 owner 等值比较(kSaveWithCommitBarrierLuaScript)和 go/db 的 guardOwnerEpoch(handleTask 对所有 write 生效,与 Fencing.Mode 无关)要同时抬,在途写(epoch = applied)才会被判 stale。回档不回退 Redis,计数器只经 INCR 前进,所以 +1 足够,不需要加余量 K。storage_elsewhere 不抬:它们的库不回退,严格世代下 saveoutbox 要用旧 epoch 补投被 Step 5 丢掉的写。 |
| #3 屏障协议 | 协议世代不信操作者的声明,由事实判定。strict ⇔ Fencing.Mode=dual,且 `db:save_protocol:zone:<z>`=strict(saveoutbox.GenerationKey);legacy ⇔ Fencing.Mode=legacy、没有世代标记、64 个桶为空、屏障里没有 pending/outbox,并用 git 核对 login 镜像不含严格屏障。strict 放行:purge 先把屏障归档后删除,然后按 PITR 之后的 SQL 逐人重建三字段屏障。重建由工具自己实现,复用 shared/savebarrier、proto2mysql、authoritysql,不改 barrierrestore。(0,0) 改成 (C',1),与 AllowLegacy 同形状;torn、缺行、T 之后建角(owner>0)的玩家先物化骨架行再重建。「有 pending、无 ACK」的玩家:丢弃,逐人归档并写进报告,不重放。 | 严格屏障已是现行 C++ / login 的唯一协议,go/db 缺省也已是 dual(2026-10-18 存盘协议世代门禁),不支持 strict 的话,按现行代码部署的 zone 就回档不了。login 没有开关地检查屏障(parent_commit_barrier.go:110-111),只删不建会全员拒登。barrierrestore 的三个前提与回档冲突:要求合服围栏不存在、要求 64 桶全空、按 ≤1000 人的计划人工审定。重放等于方案 (B)。 |
| #4 blocked 与 T 之后建角 | 归类穷举,兜底 unclassified。rollback / torn / post_t_candidate 照常 purge;storage_elsewhere / home_elsewhere 跳过,只报告;legacy_owned / owner_behind / frozen / moved_in_after_t / topology_unproven / foreign_home_storage_here / unclassified 一律 blocked(退出 6)。父快照、资产证据、屏障删除前,把 DUMP+PTTL 预写归档到报告目录,删除 Lua 里用 sha1hex(DUMP) 复核,不进 Redis 隔离区。T 之后建的角色以空骨架继续玩(legacy 走冷读骨架;strict 物化骨架行)。`-AcceptBlockedPlayers` 继续在入口拒绝。 | 没有权威的创建时间(player_database 没有 created_at,rollback_logic.go 的先例也是只报告不删),而且删角色要跨服务写。空骨架正是「T 时刻库里没有这一行」的忠实还原。共享 Redis 是 noeviction,隔离区会让缓存体积翻倍;归档到盘上同样保住证据。 |
| #5 有效回档点 | 接受「回退点比 T 早一个消费延迟」为语义 A 的 RPO,不重放。dual 下用 fence_save_seq>>10 逐玩家度量实际回退点;任何一行的存盘时刻晚于 T+60s 即以 3 拒绝。ps1 打印 T 之后将被丢弃的消息条数。 | 重放就是语义 B。save_seq 的低位由毫秒时间推出,是唯一不依赖外部监控的逐玩家证据,顺带能发现 PITR 时间点不对。 |
| #6 T 之后合入 / 迁入 | 证明不了早于 T 就一律 fail-closed,不设豁免。合服看 merge:merged_into 的 dst=本 zone:run_id 的开始时刻 ≥T 即 3;早于 T 的,还要合服清单里各写步骤都 Done 且 At≤T−1s。迁入本库(落点版本≥2、有效落点=本库)要搬库清单里 switched 且 At≤T−1s,否则 topology_unproven;本库没有行的直接判 moved_in_after_t。迁出的不需要证明,只跳过并报告。 | 合服时目标区可能在线(preflight.go:837 只查源区),copy 模式合入的玩家在 Redis 键形状上与原住民无法区分,拷行若晚于 T 会被静默清成空骨架。迁出的玩家数据不在被回退的库里,不需要证明。 |
| #7 全部冷读 | 放弃 unchanged 优化,范围内的玩家一律删缓存、首登冷读。停服基线改成全部玩家表的分块校验和(pt-table-checksum 做法),外加本 zone 归属玩家的 owner_epoch 基线(64 分片 NDJSON)。冷读洪峰由网关 zone 令牌桶和 PreloadPool 承接。 | C++ 先写 Redis 父快照再发 DBTask,Step 5 丢掉的积压只存在于父快照里;「行没变」不等于「缓存不晚于 T」,正确性优先。分块校验和的大小与玩家数无关,可以按块并行、续算。 |
| (a) 死信去重 | 在死信入队处按 (topic, 原始分区, seq=offset+1) 去重:新增 HASH `kafka:dead:index:<topic>`,与 LPUSH 同一段 Lua 做 HSETNX;moveClaimedRetryToDead 同样先判重。删除标记选「死信工具同步删标记」(L-04 第 2 批工具在 LREM 的同一段 Lua 里 HDEL),不选 LPOS 复核。副本数硬上界 = 每条消息 1 份;回档时死信索引与死信同一段 Lua 进隔离区。 | LPOS 是对一个恰好在事故期增长的列表做 O(N) 扫描;HSETNX 是 O(1) 且幂等,组提交整批重推也不会多出副本。 |
| (b) IsEqual 与探针 | 剥离。在 PlayerLastPersistedSnapshotComp::Replace 里与 last_save_seq 一起 clear 两张子表的 stress_test_probe;快路径仍用 dirty_save::IsEqual,不深拷贝。下一轮压测复盘写明跳过率口径的变化。 | 探针是非业务元数据,与 last_save_seq 同类;观测手段不应改变被测行为,否则开探针的构建快路径永不命中,压测不代表生产。只在成功存盘后剥离一次,不在热路径上增加成本。 |
| (c) op="other" 告警 | 补一条 DbTaskUnknownOp:`sum by (namespace)(increase(db_task_producer_partition_total{op="other"}[10m]))>0`,for 0m,warning。 | op 是封闭集合,出现 other 就是未知生产者或载荷损坏;这类任务在 go/db 按 unsupported op 失败(key_ordered_consumer.go:2863-2865),计数早已预建,现在只缺这条规则。 |

## 2. 设计

以下全部是静态阅读结论:没有编译、没有运行,待 Codex 验证。代码与文档冲突时以代码为准。路径都相对 D:/luyuan/xuanming-server-mmo-cpp。

══════════ 0. 全流程与各项衔接(一条时序) ══════════
入口 E1–E4
→ 0a–0f:preflight,只读。
→ 【新】0g `fence`:立围栏,排空他区在线玩家。
→ 1:zone-down。
→ 1b:(i) 消费者;(ii) 静默;(iii) `stopped`(复核围栏,写分块基线与 owner 基线);(iv)。
→ 3:PITR。
→ 4a `inventory`:复核基线与 owner;三遍盘点,归类,拓扑证明,#5 度量,strict 可行性预检。
→ 4a′:停服复核。
→ 5:-ToLatest,记切点。
→ 4b `purge`:
  - P1 区级重试 / 死信族进 Redis 隔离区;
  - P2 逐人一段 Lua:复核围栏 → 纪元抬升 → 删各族键 → ZREM;
  - P3(只 strict)逐人做 SQL 规范化,并重建屏障。
→ 5b:切点复核,`verify`。
→ 6:zone-up。
→ 【新】6b `release`:撤围栏。
→ 7:清单。

几项的衔接关系:
- 围栏(#1)在 0g 立起,一直保持到 6b 撤除。stopped / inventory / purge / verify 每个模式开头都复核「两把围栏键等于本 rid 且没有 TTL」;purge 的逐人 Lua 和 strict 的重建 Lua 把这两把键放进 KEYS,原子复核。
- 纪元抬升(#2)只能在 Step 5 之后、该玩家的删除之前做:Step 5 之前还有中止路径,中止要保留 Kafka 积压;先删后抬会留下残留写者可以写回的窗口。所以做法是放进 P2 的同一段 Lua,先抬后删。
- strict 的屏障重建(#3)用 C' 做 owner:先于 C' 重建的话,残留实体能写入更高的 pending。所以重建放在 P2 全部完成之后(P3)。
- verify 最后统一核对三件事:owner 和两个 applied 都等于 C';屏障等于目标;各族键不存在。

══════════ 1. #1 zone 维护围栏 ══════════
【证据】
- 读方只看 EXISTS:
  - data_service:routing/router.go:209,GetPlayerHomeZone 读 MergeFenceKey(home);:253 setPlayerZoneIfAbsentScript KEYS[2]=merge:in_progress:{home},拒绝新建角色;
  - scene_manager:home_zone.go:176-199,rejectHomeZoneMerging「不分腿、不分首登 / 换图一律拒」,而且在第一次写 location / owner_epoch 之前就拒;
  - guild:merge_fence.go:81;
  - 契约:merge_zone/fence.go:7-28 与 route_ready.go:407-409 都写明「值的格式不是读方契约」。
- 先例:barrierrestore 的 route.go:181/204-207 Guard,用永久的 merge:in_progress:<z>=`save_barrier_restore:<id>` 加 MaintenanceKey 当维护广播围栏。
- 互斥:
  - merge_zone 获取围栏用 SetNX(fence.go:147),撞上即拒;
  - routetool 的 import / repair 只要 SCAN 到 merge:in_progress:* 就退出 3;
  - db_first 的对账 leader 遇到不带 fence_epoch 的值不删、不覆盖(MergeFenceMirrorSetScript 回 2)。
【键与值】
- K1 = placement.MergeFenceKey(zone),值 `zone_rollback_discard:<rid>`;
- K2 = barrierrestore.MaintenanceKey(zone),值 `<rid>`;
- 两把键 PTTL 都是 -1,位于 go/db 的 RedisClient 实例。deploy-facts 已核对它与 MappingRedis / OwnerEpoch / Placement 同实例同 DB。
【fence 模式】
1. 先跑 preflight 的全部公共检查。
2. 一段 Lua 获取围栏(仿 barrierrestore 的 guardScript):对 K1、K2 各自判断,不存在则 SET;已存在且值等于我方、PTTL=-1 则视为已持有(幂等);否则返回 `foreign fence` → 3。Lua 里还复核 INFO 的 run_id。
3. 排空:H 遍 SCAN player:zone:* 加 MGET,取 home=本 zone 的玩家;再按批 MGET player:<id>:location 与 handoff。
   - 用 decideLocationZone 的同一真值表区分陈旧与他区在线(scan.go 里重写一份,与 merge_zone 共用测试向量)。
   - 得到「他区在线集合」后,每 15s 只对这个集合重读;集合为空后再做一次全量确认。
   - 截止时间用单调时钟,`-drain-wait`(缺省 30m,上限 6h)。
   - 超时:写 fence-drain.json(名单),以 5 退出,围栏保留。同一 rid 重跑会接着等;放弃则执行 `-mode release`,再走中止路径。
【release 模式】
- 一段 Lua 按值 CAS 删除 K1、K2:值不等于我方 → 3,什么都不删;不存在 → 0(幂等)。
- 只在两个时机执行:Step 6 rollout 成功之后(6b),或中止路径的 zone-up 之后。
【复核】
- stopped / inventory 用 GET + PTTL 只读复核;
- purge 与 strict 的写 Lua 把 K1、K2 放进 KEYS,原子复核;
- 缺失或被改 → 3(写 Lua 返回 fence_lost)。
【政策】
- 等他区在线的玩家自然下线,不踢:
  - 围栏保证这个集合只减不增:EnterScene 不分腿一律拒绝,新建角色被 setPlayerZoneIfAbsentScript 拒绝。
  - 踢下线需要会话、gate 实例、target_instance_id 的寻址(login/internal/svc/gate_command.go),go/db 无法 import,而且踢下线产生的退出存盘本来就要被丢弃。
- 副作用(写进 runbook):
  - 围栏期间本 zone 归属玩家不能登录、不能换图,本 zone 不能建帮;
  - 全服的 route_tool import / repair 暂停;
  - 回档放弃而忘了 release,会把本 zone 钉死,所以新增一条告警(见 §8)。

══════════ 2. #2 纪元抬升 ══════════
【停服基线(stopped)】
- 对本 zone 归属玩家(H 遍,加上 M 遍里有行且 home=本 zone 的玩家),按 pid%64 分片流式写 `owner-baseline-s<NN>.ndjson`,每行 {pid, owner}。
- fingerprint-baseline.json 记下每个分片的 SHA-256。
- 同一 rid 已有 phase=stopped 的基线就不覆盖。
【U(p) 与目标(raise.go,纯函数)】
- U(p) 取以下来源的最大值:
  - owner_stop;
  - 当前世代 topic(config.DbTaskTopicForGeneration)两表的 applied_epoch(kafka.AppliedEpochKey);
  - 屏障里 pending / player_database / player_database_1 / outbox_version 的 epoch(savebarrier.Parse);
  - 资产证据 asset_sql_commit 的版本(assetop 的版本即 savebarrier.Version);
  - PITR 之后两表的 fence_owner_epoch。
  这些 Redis 来源在停服之后不会变(围栏加停服),由「owner 未变」间接证明。
- 判定:
  - owner_stop==0 且 U>0 → owner_behind;
  - owner_stop<U → owner_behind;
  - owner_stop==MaxUint64 → unclassified(溢出);
  - owner_stop==0 且 U==0 → 未铸造的号,不抬、不建键;
  - 其余 C'=owner_stop+1。
【inventory 复核】
- 现读 owner 必须逐人等于基线;不在基线里却在范围内的玩家也算违例。违例 → 3,说明围栏漏了,此时中止路径还在。
【抬升(P2 逐人 Lua 的前半段)】
- KEYS 带 owner_epoch 和两个 applied_epoch。比较用导出的 kafka.LuaDecimalGreaterFn 按十进制精确比较,不用 tonumber。
- 前置检查:
  - 类型是 string、值是规范十进制、PTTL=-1;
  - owner ∈ {owner_stop, C'},否则 owner_advanced;
  - 每个 applied 只能是缺失、≤owner_stop 或 ==C',否则 applied_ahead。
- 检查通过后 SET 三键为 C'。
- 计数 raised / already。
【verify】
- 已处理的玩家:owner 与两个 applied 必须恰好等于 C'。更大说明 Step 6 之前有人铸造过 → 3。
【不抬的】
- MySQL 行围栏:legacy 不检查行围栏;dual 有 Redis 守卫在 SQL 之前拒。
- 他区归属的玩家:不在围栏内,抬了会废黜他们正在线的实体。
- storage_elsewhere:理由见决定表。

══════════ 3. #3 屏障与协议世代 ══════════
【判定(target.go)】
- deploy-facts 的 FencingMode,加上 GET saveoutbox.GenerationKey(zone)(generation.go:37,值 GenerationStrict),加上 64 个桶的 ZCARD:
  - dual + 标记==strict → strict;
  - legacy + 标记缺失 + 桶全空 → legacy;
  - 其余(dual 而没有标记、legacy 而有标记或桶非空、值未知)→ 3。
- `-barrier-protocol` 必须等于判定结果,否则 3。
- legacy 另由 ps1 做 git 核对(Get-TargetBarrierProtocol):login 镜像提交里没有 go/login/internal/logic/pkg/dataloader/parent_commit_barrier.go,node 镜像提交里 redis_client.h 不含 kSaveWithCommitBarrierLuaScript;判不出 → 拒。
【legacy】
- 工具绝不 HSET 屏障。屏障只允许出现 player_database / player_database_1 两个 ACK 字段,P2 里归档后删除。
- 出现 pending / outbox_* 字段 → inventory 判 3(协议世代与 Redis 内容矛盾)。
【strict:P2】
- 屏障与父快照等一起「归档 + sha1 复核 + UNLINK」,并 ZREM 本人在桶里的成员。
- 「有 pending、无 ACK」或带 outbox 字段的,在 purge-players 里逐人记 pending / 两个 ACK / outbox_version / outbox_topic / 处置结果,计 discarded_uncommitted_intents。
- 不重放:按裁决 (A),T 时刻不在 MySQL 里的意图一律丢弃。归档满足 db-write-fencing-mysql.md:1555/1559 的「先保存、逐笔裁决」。
【strict:P3,对每个已处理且 C'>0 的玩家(strict.go)】
1. TiDB 强会话(authoritysql.Open(strong=true);本机 MySQL 显式 `-local-mysql`)。
2. 一个事务里对 player_database / player_database_1 做 FOR UPDATE 读。按形状定目标版本 V:
   - 两行都在、围栏相等且非零 → V=该围栏,不写 SQL;
   - 两行都是 (0,0) → V=(C',1),对两行做 UPDATE 围栏 CAS(条件 fence=(0,0),RowsAffected==1;与 barrierrestore 的 AllowLegacy 同形状);
   - torn(缺一行,或两行围栏不同):V=现存行围栏的最大值,全零时取 (C',1)。缺的那张表用 proto2mysql 的 ExistsByPK+Insert 插入只有 player_id 和围栏 V 的空消息;围栏 ≠V 的行做 CAS 改围栏(只改围栏元数据,不碰资产列);
   - 两行都缺(post_t_candidate 且 owner>0)→ V=(C',1),两表都插入骨架行。
3. 提交之后另开事务重锁重读,围栏必须等于 V。
4. 跑一段 Lua。前置检查:run_id、K1/K2、owner==C'、父快照与子表缓存与证据与 handoff 都不存在、本人不在自己的桶里、屏障缺失或恰好是三个字段都等于 V 且 PTTL=-1。通过后 HSET pending / player_database / player_database_1 = V。
5. 回滚读事务。
- 先提交 SQL,后发布 Redis:SQL 失败绝不发布 ACK。
- 重跑幂等:CAS 和 Insert 的「已存在即重读」都会收敛,Lua 接受同值的三个字段。
- 结果写进 strict-barrier-p<N>.ndjson。
【为什么骨架行】
- 严格协议的不变量是:owner≠0 ⇒ 要么「父快照加屏障」,要么「SQL 行围栏 == pending」(parent_commit_barrier.go:38-44、:110-111)。
- 只写父快照骨架不可行:父快照有 TTL(publishParentLoadScript 带 PX),过期后冷读会因 SQL 缺行被拒。
- 骨架行加 (C',1),等价于替玩家做一次「空角色首存」。scene 按 registration_timestamp==0 走新号分支,身份由 login 的 backfillPlayerIdentity 补齐,与 legacy 冷读骨架的语义一致。
【inventory 的 strict 预检】
- 只读地对范围内玩家做一遍 P3 的形状判定。主键不唯一、player_id 不符、围栏非规范、V 的 epoch 大于 C' → unclassified(strict_*),归 blocked。
【不改 barrierrestore】
- 它与回档有三处冲突:
  - restoreScript(scripts.go:20)要求 merge 围栏不存在;
  - :36-40 要求本 zone 64 个桶全空,而 storage_elsewhere 的合法成员会留在桶里;
  - Plan 是按 ≤1000 人人工审定的流程。
- 它是另一会话未跟踪的在途代码,所以只复用共享的 savebarrier / authoritysql / proto2mysql。K2 让 save_barrier_restore 无法与回档并发,这是有意的。

══════════ 4. 回档工具:七个模式 ══════════
【结构】
- 薄 CLI:go/db/cmd/zone_rollback_discard/main.go。flag、-version、退出码映射在这一层。
- 逻辑全部在 go/db/internal/zonerollback。
- 依赖:go-redis(MaxRetries=-1,先例 save_barrier_restore/main.go:92)、authoritysql、proto2mysql、config.DbTaskTopicForGeneration,以及 §5 列出的键名导出。
【公共参数(契约 :151-153 原样)】
- 原有:-deploy-facts -zone -topic-generation -redis -redis-db [-redis-password-env] -expect-run-id -mysql-dsn-env -rollback-id -barrier-protocol -report-dir -mode。
- 新增:-topology-manifest-dir、-workers(缺省 16,上限 64)、-timeout(缺省 8h)、-local-mysql。
- 口令与 DSN 只从环境变量读,不进报告。
【每个模式开头】
- deploy-facts Schema 3:ZoneId / 世代 / 分区数一致,四路 Redis 同实例同 DB,RouteStoreMode=redis,协议判定见 §3。
- Redis:INFO server 的 cluster_enabled:0,run_id==-expect-run-id。
- MySQL:SELECT DATABASE()==placement.StoreDBName(zone)。
- 玩家表 = 编译进二进制的 mysql_database_table.proto 里 OptionPrimaryKey=player_id 的消息,与 information_schema 里实际存在的表取交集(与 merge_zone 同口径);围栏表 = 带 fence_owner_epoch / fence_save_seq 列的表。
- 除 preflight、fence 外,还要复核 K1/K2。
【-version】
- 恰好两行 commit=<40hex> 与 dirty=。读 -X main.buildCommit / main.buildDirty;没注入时回落 debug.ReadBuildInfo。
【preflight(只读)】
- 原契约的检查全部保留。
- 删去「#1 落地前恒以 3 拒绝」,改为:K1/K2 不存在或已是本 rid。
- 合服与 merged_into 的 zone 级规则(#6)。
- 只靠 Redis 就能判出的拒绝,在这里先判一次。
- 打印他区在线人数,只供参考。
- 退出码 4 规则不变。
【fence / release】见 §1。
【stopped】
1. 原契约的检查:scene_nodes:zone:<z>:load 为空(scenenode.NodeLoadKey);重试租约(-lease-wait,单调时钟);handoff(-handoff-wait ≥300s);他区在线即 3。
2. 新增:K1/K2 复核;distributed:lock:kafka:ordering:<topic>: 前缀下没有存活的锁。
3. 最后写基线:
   - 每张玩家表按主键每 1000 行切块,块边界覆盖整个 uint64 空间;
   - 每块在服务端算 COUNT(*) 与 BIT_XOR(CONV(SUBSTRING(MD5(CONCAT_WS('#',全部列,ISNULL 标记)),1,16),16,10)),列按 ORDINAL_POSITION 排、反引号转义;
   - 加上 owner 基线(§2)。
   - 先写 .partial,写完 rename 落盘;文件记 phase=stopped / rid / zone / Redis TIME / UTC。
【inventory(只读,判定参数加 -report)】
1. 读基线:缺失或 phase 不对 → 3。
2. 按同一组块边界重算:全部块相同 → 3,除非 -acknowledge-no-post-t-writes;列清单不同 → 3。报告变化的块数与各表行数差。
3. owner 复核(§2)。
4. 三遍盘点(scan.go)与归类(classify.go,§6 真值表)。
5. 拓扑证明(topology.go)。
6. #5 度量:dual 下统计 fence_save_seq>>10 与 T 之差的最小 / 中位 / 最大;任一行 >T+60s → 3;legacy 记 effective_point=unknown(legacy)。
7. post_t_candidate 或 home_elsewhere 占比 >-max-post-t-ratio(缺省 0.05,且人数 >20)→ 3。
8. strict 预检。
- 输出 inventory.json(各类人数、阈值、blocked 前 1000 名)和 inventory-<class>.ndjson,SHA 记进清单。
- 有 blocked → 6。
【purge(判定参数加 -confirm / -report)】
1. 在进程内原样重跑 inventory 的全部判定,任一不过 → 3,什么都不写。
   - 这次重跑接受「owner==C'、各族键已不存在」是上一轮留下的状态;判定以停服基线为准。
2. HINCRBY 清单 pass_count 开新一轮 pass=N。
3. P1 区级:用 SCAN(MATCH 规则见 §5)找出本 topic 的 ready / :p<n> / processing / instances / instance_mode、dead 和 dead index。
   - 每个键一段 Lua:RENAMENX 到 rollback:quarantine:<zone>:<rid>:<N>:<原键>,同一段里 PEXPIRE(-quarantine-ttl,缺省 720h);
   - guard 键 UNLINK;
   - 目标已存在 → 1。
4. P2 逐人(worker 池按 pid%64 分片):
   a. pipeline 读 DUMP+PTTL 三族:父快照、asset_sql_commit、save_barrier。
   b. 追加写 purge-archive-p<N>-s<NN>.ndjson(pid、key、pttl、dump_b64、sha1),每批 fsync,文件 0600,目录 0700。
   c. 执行 purge_scripts.go 的逐人 Lua。KEYS 约 21 个:K1、K2、player:zone、player:placement、owner_epoch、两个 applied_epoch、location、handoff、父快照、各玩家表缓存、BagAllData / QuestAllData / MailAllData、sync_digest、asset_sql_commit、save_barrier、本人桶、清单。
      - 全部检查先于第一次写:run_id;K1/K2;映射与落点逐字节等于盘点值(缺键即要求仍缺);location 逐字节等于盘点值;handoff 缺失;归档键的 sha1hex(DUMP) 等于 ARGV(为空则要求键不存在);legacy 下屏障不得有 pending / outbox;抬升的前置检查。
      - 写:抬升;UNLINK 各族;ZREM 本人;HINCRBY 分族计数。
   d. 追加 purge-players-p<N>.ndjson。
5. P3(只 strict):P2 全部结束之后执行 §3。
6. 写 purge.json(累计全部轮次);停止时与正常结束时都打印分族计数。
【verify(判定参数加 -kafka-cut / -purge-report / -report,只读)】
- 原契约的检查保留:
  - 本 topic 各族为 0;
  - 没有存活租约;
  - consumer:applied:<topic>:* 用 kafka.ParseAppliedCursor 解析:v2 游标逐分区要求 seq≤切点,旧格式与各分区切点的最大值比较;
  - 逐人复核删族的键都不存在。
- 新增:
  - owner 与两个 applied 都等于 C';
  - K1/K2 仍在;
  - legacy:屏障不存在,64 个桶为空;
  - strict:屏障 HLEN=3、PTTL=-1,三个字段等于 V,两表行围栏等于 V;64 个桶里没有任何已处理玩家;
  - 重跑归类,不能出现任何仍需删除的键族或 blocked 玩家。
- 任一项不过 → 3。

══════════ 5. 键族处置表(键名全部出自导出函数,工具里不写字面量)══════════
| 键族 | 定义处 | 处置 |
|---|---|---|
| PlayerAllData:<pid> | 新 shared/playercache.ParentKey | 归档、复核后 UNLINK |
| <玩家表>:<pid>,以及 BagAllData / QuestAllData / MailAllData:<pid> | playercache.CacheKey / AuxCacheMsgTypes(写方 key_ordered_consumer.go:50 buildCacheKey,经 kafka.PlayerTableCacheKey 导出) | UNLINK,不归档 |
| player:{pid}:asset_sql_commit | assetop.SQLCommitKey | 归档后 UNLINK(fencing §9.2 要求删) |
| player:{pid}:save_barrier | savebarrier.Key | 归档后 UNLINK;strict 下 P3 重建,legacy 下绝不写 |
| player:save_pending:<z>:<b> | 新 saveoutbox.PendingIndexKey / BucketOf | ZREM 本人 |
| player:<pid>:sync_digest | playersync.Key | UNLINK |
| player:<pid>:owner_epoch | ownerepoch.OwnerEpochKey | 只按 §2 抬升 |
| consumer:applied_epoch:<topic>:<pid>:<表> | 新 kafka.AppliedEpochKey | 只按 §2 抬升 |
| consumer:applied:<topic>:* | kafka.AppliedSeqKeyPrefix / ParseAppliedCursor | 不碰,verify 与切点比较 |
| player:<pid>:handoff | ownerepoch.HandoffKey | 不碰,等待过期 |
| player:<pid>:location | scenenode.LocationKey | 不碰,判在线 |
| 重试族:ready、:p<n>、processing[:inst]、instances、instance_mode、kafka:dead:queue、新 kafka:dead:index | 新 kafka.Retry*Key / RetryDeadIndexKey | RENAMENX 进隔离区并 PEXPIRE |
| kafka:retry:guard:* | kafka.RetryGuardKeyPrefix | UNLINK |
| distributed:lock:kafka:ordering:<topic>:* | kafka.OrderingLockKeyPrefix | 只读,stopped 要求没有存活的锁 |
| player:zone / placement / maprev、merge:merged_into:*、account:*、player_to_account:*、data_service blob、会话与锁 | placement.* | 只读 |
| merge:in_progress:<z>、player:save_barrier:maintenance:<z> | placement.MergeFenceKey、barrierrestore.MaintenanceKey | 只由 fence / release 写 |
| db:save_protocol:zone:<z> | saveoutbox.GenerationKey | 只读(合服 / 回档不动它) |
| scene_nodes:zone:<z>:load | 新 scenenode.NodeLoadKey | 只读,stopped 要求为空 |

SCAN 的 MATCH 一律是 EscapeRedisGlob(含 topic 的前缀) + 边界 + '*':按实例或按键的族边界是 ':',按分区 ready 是 ':p';只采纳前缀逐字相同的键;cursor 走到 0;任一页出错即 1。

══════════ 6. 归类真值表(classify.go,纯函数 switch,Class 是带 kCount 的枚举加名字表)══════════
前置:映射与落点可解析;落点 frozen → frozen。
| home | 有效落点 | 本库有行 | 落点版本 | 类别 |
|---|---|---|---|---|
| Z | Z | 齐且围栏一致 | ≤1 或无 | rollback |
| Z | Z | 齐且围栏一致 | ≥2 | rollback,要求搬库证明,否则 topology_unproven |
| Z | Z | 缺一张或围栏不一致 | — | torn(版本 ≥2 时同样要证明) |
| Z | Z | 无 | ≤1 或无 | post_t_candidate |
| Z | Z | 无 | ≥2 | moved_in_after_t |
| Z | ≠Z | 有或无 | — | storage_elsewhere(跳过) |
| ≠Z | Z | 有或无 | — | foreign_home_storage_here(blocked:写者不在本 zone 围栏内) |
| ≠Z | ≠Z | 有 | — | home_elsewhere(跳过,是冷副本) |
| 缺 | — | 有 | — | legacy_owned(blocked,修复入口 merge_zone -backfill-home-zone) |
- 归类后对 rollback / torn / post_t_candidate 再做 U(p) 判定,不过 → owner_behind(blocked,修复入口 fencing §9.1 epoch_raise)。
- 其余畸形一律 unclassified。

══════════ 7. 水平扩展、并发与断点续跑 ══════════
- 无全局单点:所有状态都带 zone 和 rid 前缀(隔离键、清单 rollback:quarantine:<z>:<rid>:manifest、报告目录),没有全局锁,多个 zone 可以并行回档。同一 zone 与合服的互斥靠 K1 的 SetNX 语义。
- 分片与并发:
  - 固定分片 shard=pid%64(与补投桶同模),写进清单;
  - 盘点按 1000 一批:MGET、IN 列表、SCAN COUNT 1000;
  - 写 Lua 按 pipeline 每批 200 条;
  - worker 用 errgroup 加可取消的 ctx,任一 SCAN 页、SQL 或 Lua 出错就取消全部,落盘已有计数后以 1 退出;
  - 分块校验和按块并行。
  - 瓶颈是单 Redis 实例的单线程,所以只做进程内并发;分片规则已写进格式,以后加 `-shard i/n` 多进程不需要改格式。
- 断点续跑:靠幂等重放,不靠持久化 SCAN cursor(Redis 重启后 cursor 的含义会变)。
  - 每次 purge 开新一轮 pass 重扫全部范围:UNLINK 已删的键是空操作,RENAMENX 源键不在就跳过,抬升接受 ==C',P3 接受同值。
  - 基线有 .partial 可续算;NDJSON 追加并 fsync,重复条目无害。
  - 进程内存不保存任何关乎正确性的状态。
- 超时:每次 Redis 调用 3s、每个 SQL 事务 5s,都用单调时钟截止;整体 -timeout;写操作 MaxRetries=-1,不在工具内重试,由同 rid 的整体重放负责。

══════════ 8. 失败语义与退出码 ══════════
- 0:通过。
- 1:出错,可能已做了一部分;修好后用同一 rid 重跑,幂等。
- 3:拒绝,本次调用什么都没写。
- 4:目标看起来没在用(只 preflight,-allow-empty-target 放行)。
- 【新】5:只 fence 模式,围栏已立但排空超时。重跑继续等,或 release 后走中止路径。
- 6:inventory 有 blocked。purge 重跑判定时遇到 blocked 返回 3。
- 其它值按出错处理。
- 写 Lua 的错误字(owner_advanced / applied_ahead / fence_lost / route_changed / archive_mismatch / protocol_contradiction)原样打印并写进报告。
- Step 5 之后的失败只能向前,ps1 打印单步命令。
- 新增告警(scene-manager-alerts.yaml):ZoneEntryFenceHeldTooLong,`sum by (namespace)(increase(enter_scene_rejected_total{reason="home_zone_merging"}[30m]))>0`,for 2h,warning。描述写明「合服或回档围栏超过 2h;回档已结束则执行 `-mode release`,不要手工 DEL」。

══════════ 9. 与 k8s_zone_rollback.ps1 契约对齐(需改契约的地方单列)══════════
C1 新增模式 fence / release,沿用公共参数:fence 附加 -drain-wait;新增退出码 5;Get-DiscardToolExitMeaning 同步。
C2 preflight:删去 :178「#1 落地前必须以 3 拒绝」,改为「K1/K2 不存在或已是本 rid」;新增 RouteStoreMode 与协议判定。
C3 -barrier-protocol 接受 strict。删去 :1868 入口对 strict 的拒绝。Get-DeployFactsProblems:legacy⇔legacy、strict⇔dual;Schema 3 增加 RouteStoreMode(读自 go-svc-data-service-config 的 RouteStore.Mode,缺省 redis)。新增 Get-TargetBarrierProtocol,只用于 legacy 的 git 核对。
C4 stopped::182「受影响玩家两表的行指纹」改为「全部玩家表的分块校验和 + 本 zone 归属玩家的 owner 基线」;增加排序锁检查与围栏复核。
C5 inventory::185-187 的比较对象改为分块校验和;增加 owner 复核、新的类别与 blocked 列表、#5 度量、#6 证明、-max-post-t-ratio、strict 预检。
C6 purge::195-196「绝不碰 consumer:applied*」改为「游标 consumer:applied:<topic>:* 不碰;owner_epoch 与本世代两表 applied_epoch 按抬升规则只升不降抬到 C'」;增加「删除前归档到报告目录」;strict 下工具写 SQL(两表围栏元数据 CAS 与骨架行)并重建三字段屏障;死信索引与死信一起隔离。
C7 verify:增加 C'、围栏、strict 屏障与 SQL 围栏的核对;strict 下补投桶只要求不含已处理玩家。
C8 公共参数增加 -topology-manifest-dir、-workers、-timeout、-local-mysql;判定参数($judgeArgs)增加 -max-post-t-ratio。
C9 文件头 :17-18「owner_epoch 保留不动」改写。
C10 「两张玩家表」改为「OptionPrimaryKey=player_id 的全部表 ∩ 实际存在的表」,围栏表另列。
C11 流程:
- 新增 0g:preflight 通过、写 entry-state 之后,Step 1 之前。
- 新增 6b:Step 6 成功之后。
- 中止与放弃路径的提示末尾加 `<tool> -mode release`。
- Step 1b (ii) 失败文案改指 0g。
- Step 7 清单增加:围栏已撤;staging / prod 的 -GatewayRateLimit 生效;冷读洪峰监控。
- 新增只读函数:调用 kafka_offset_reset.ps1 -ToDatetime <T> 的 dry-run,与日志末端相减,打印 T 之后的消息条数(失败只告警)。
- :415 / :1875 / :2498 的 blocked 文案改为新类别与各自的修复入口;-AcceptBlockedPlayers 继续在入口拒绝。
C12 dev_tools.ps1 透传 -RollbackTopologyManifestDir / -RollbackMaxPostTRatio / -RollbackDrainWait。
不改的契约:-version 的 40 位 main.buildCommit;rid 格式;隔离键名格式;续跑规则;MATCH 规则。

══════════ 10. 小项实现 ══════════
(a) 死信去重
- 新文件 go/db/internal/kafka/dead_dedup.go:
  - 字段 `<partition>:<seq>`:Kafka 源取 msg.Partition 与 msg.Offset+1;重试源取 task.originPartition 与 task.seq;
  - deadEnqueueLua:`if redis.call('HSETNX',KEYS[2],ARGV[2],ARGV[3])==0 then return 0 end; redis.call('LPUSH',KEYS[1],ARGV[1]); return 1`。
- durable_enqueue.go:
  - submit 增加可选参数 dedupField;
  - aofGroupEnqueue 的批里,dead 目标改用 EVAL,与其余 LPUSH 同一条粘连连接、同一个 pipeline,后面照旧 WAITAOF;网络错误整批重推时 HSETNX 幂等。
- key_ordered_consumer.go:
  - persistPoisonMessage(:3205)、quarantineOrderingConflict 的 Kafka 分支传字段;
  - moveClaimedRetryToDead(:1569)的搬移脚本先 HSETNX:标记已存在就只 LREM processing,计 duplicate_dropped;
  - 没有 seq / origin 的旧收据照旧直推,计 undeduplicable。
- 指标 db_dead_letter_dedup_total{target,result},result 取 enqueued / duplicate_dropped / undeduplicable,启动时预建为 0。
- 死信工具(L-04 第 2 批,未做)与人工处置必须在 LREM 的同一段 Lua 里 HDEL 标记。
(b) 剥离探针
- last_persisted_snapshot_comp.h 的 Replace:在 clear_last_save_seq() 之后加 mutable_player_database_data()->clear_stress_test_probe() 与 mutable_player_database_1_data()->clear_stress_test_probe()。
- player_lifecycle.cpp:4275 的注释改为「探针在 Replace 里剥离」。
- 快路径 :4276-4278 仍是 dirty_save::IsEqual。新消息在快路径判定时还没 Stamp(:4311-4312),所以两侧都不带探针。
- 已核对读快照的各处只读账本字段:player_battle.cpp:351、asset_op_system.cpp:232/1137、player_inbox_system.cpp:513、player_lifecycle.cpp:3465/4333。IsPersistedPayloadCurrent 本来就两侧都剥。
(c) op="other" 告警
- db-task-partition-alerts.yaml 新增 DbTaskUnknownOp 一条规则(见决定表),描述:按 go/db 日志「unsupported op」和「DATA-ORDERING」找生产者。
- 契约文档 :166 与 launch-todo 5.3.9 改为「已设」。

Java 版(AGENTS §12):不适用,Java 没有 Kafka 存盘链。Java 仓 PARITY.md 追加一行「不适用」。

## 3. 落码批次（共 73 个文件）

### B1 共享键名导出(行为不变)（依赖：无）

把工具要用的键名下沉或导出,只包一层,不改原函数,并用测试钉死字面量。
- shared/playercache:ParentKey、CacheKey、AuxCacheMsgTypes。
- shared/scenenode:NodeLoadKey;scene_manager 改为调用它。
- kafka/keys_export.go:重试族、死信、死信索引(RetryDeadIndexKey 的权威定义)、AppliedSeqKeyPrefix、AppliedEpochKey、ParseAppliedCursor、PlayerTableCacheKey、OrderingLockKeyPrefix、EscapeRedisGlob、ScanMatch、LuaDecimalGreaterFn。
- saveoutbox/keys_export.go:PendingIndexKey、BucketOf。
不改 key_ordered_consumer.go、contract.go 这些他人在改的热文件。

文件：`D:/luyuan/xuanming-server-mmo-cpp/go/shared/playercache/keys.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/shared/playercache/keys_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/shared/scenenode/nodeload.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/shared/scenenode/nodeload_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/scene_manager/internal/logic/load_reporter.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/kafka/keys_export.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/kafka/keys_export_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/saveoutbox/keys_export.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/saveoutbox/keys_export_test.go`

### B2a 工具骨架、围栏与只读模式（依赖：B1）

- CLI:flag、-version、退出码映射。
- target:deploy-facts Schema 3、Redis 与 MySQL 身份、协议世代判定、玩家表发现。
- fence:fence / release / 围栏复核 / 排空(#1)。
- scan:M / H / P 三遍扫描与 location 判据。
- fingerprint:分块校验和与 owner 基线。
- report:原子落盘、NDJSON fsync、文件权限。
- scripts:围栏与身份 Lua。
本批实现 preflight、fence、release、stopped 四个模式。

文件：`D:/luyuan/xuanming-server-mmo-cpp/go/db/cmd/zone_rollback_discard/main.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/cmd/zone_rollback_discard/main_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/target.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/target_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/fence.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/fence_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/scan.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/scan_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/fingerprint.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/fingerprint_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/report.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/report_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/scripts.go`

### B2b 盘点:归类、拓扑证明与纪元目标（依赖：B2a）

- classify:§6 真值表,带 kCount 的枚举加名字表。
- topology:合服与搬库清单的最小只读镜像,run_id 解析,At≤T−1s 判定;testdata 与 merge_zone 共用样例。
- raise:U(p)、C' 与 owner_behind 的纯函数。
- inventory:基线比对、owner 复核、#5 度量、占比阈值、strict 只读预检,输出 inventory.json 与各类 ndjson。

文件：`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/classify.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/classify_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/topology.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/topology_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/testdata/merge_manifest_v1.json`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/testdata/relocate_manifest_v1.json`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/raise.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/raise_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/inventory.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/inventory_test.go`

### B2c purge 与 verify(两种协议共用部分)（依赖：B2b）

- ledger:清单 HASH、轮次 pass、分族计数。
- purge:P1 区级族隔离(含死信索引);P2 逐人归档,再执行逐人 Lua(围栏复核、纪元抬升、删族、ZREM、计数)。
- verify:切点、C'、围栏、各族键不存在、重跑归类。
- integration_test:环境变量门控的真 Redis 7.x 加 MySQL / TiDB 用例。
完成后 legacy 协议可用。

文件：`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/ledger.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/ledger_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/purge.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/purge_scripts.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/purge_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/verify.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/verify_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/integration_test.go`

### B2d strict 屏障重建（依赖：B2c）

P3:TiDB 强会话下锁行读,做 SQL 规范化(两表围栏 CAS、torn 对齐、骨架行 proto2mysql Insert),提交后重锁复核,再用 Lua 发布三字段屏障。另在 verify 中加入 strict 屏障与 SQL 围栏的核对。不改 barrierrestore。

文件：`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/strict.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/strict_scripts.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/strict_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/zonerollback/strict_integration_test.go`

### B3 编排脚本与契约（依赖：B2a 的 CLI 契约定稿(可与 B2b–B2d 并行)）

k8s_zone_rollback.ps1 按 §9 的 C1–C11 修改:
- 新增第 0 步 g 与 Step 6b;
- deploy-facts Schema 3;
- 放行 strict;
- 新增 Get-TargetBarrierProtocol;
- 新参数 -TopologyManifestDir、-MaxPostTRatio、-DrainWait;
- 退出码 5;
- 中止与放弃路径带 release;
- blocked 文案;
- T 之后消息条数估计;
- Step 7 清单;
- 文件头契约同步。
另外 dev_tools 透传新参数,沙箱假工具认识 fence / release 与新参数,契约测试补齐。

文件：`D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/k8s_zone_rollback.ps1`、`D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/tests/k8s_zone_rollback_discard.tests.ps1`、`D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/tests/lib/k8s_zone_rollback_sandbox.ps1`、`D:/luyuan/xuanming-server-mmo-cpp/tools/scripts/dev_tools.ps1`

### B4 小项 a:死信去重（依赖：B1(RetryDeadIndexKey);与 B2 无文件重叠,可并行;动手前与 go/db 其他在途会话核对热文件）

- 新增 dead_dedup.go(Lua 与字段编码);
- durable_enqueue 的 submit 增加 dedupField,dead 目标在组提交批里改用 EVAL;
- key_ordered_consumer.go 三处调用点与 moveClaimedRetryToDead 先判重;
- 新指标 db_dead_letter_dedup_total;
- 告警描述改为「副本已有硬上界」。

文件：`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/kafka/dead_dedup.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/kafka/dead_dedup_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/kafka/durable_enqueue.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/kafka/durable_enqueue_test.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/kafka/key_ordered_consumer.go`、`D:/luyuan/xuanming-server-mmo-cpp/go/db/internal/metrics/metrics.go`、`D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/db-task-retry-alerts.yaml`

### B5 小项 b、c:探针剥离与 op=other 告警（依赖：无(独立)）

- Replace 时剥离两张子表的 stress_test_probe,并修正 player_lifecycle.cpp:4275 的注释;
- save_seq_test.cpp 新增用例:探针快照与未 Stamp 的新消息比较相等,快路径命中;
- 新增告警 DbTaskUnknownOp。

文件：`D:/luyuan/xuanming-server-mmo-cpp/cpp/libs/services/scene/player/comp/last_persisted_snapshot_comp.h`、`D:/luyuan/xuanming-server-mmo-cpp/cpp/libs/services/scene/player/system/player_lifecycle.cpp`、`D:/luyuan/xuanming-server-mmo-cpp/cpp/tests/cross_zone_test/save_seq_test.cpp`、`D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/db-task-partition-alerts.yaml`

### B6 文档、告警描述与登记（依赖：B2d、B3、B4、B5 定稿之后(文档以代码为准)）

- zone_data_rollback 三语版:「2026-10-18 修订」节写入 R9 的决定、流程表、键族表、残余风险。
- db-write-fencing-mysql.md:§9.1 / §9.2 写回档抬升与删除证据,§7.3 / §14 第 22 条写探针剥离已定。
- scene-owner-reentry-barrier.md 与 ownerepoch.go 包注释:登记维护期只升不降的运维写入方。
- shared-redis-split-by-purpose.md:登记 rollback:quarantine:*、kafka:dead:index:*、维护围栏值与盘上归档。
- db-retry-owner-routing.md §7.10:死信去重。
- partition-contract 文档 :166。
- launch-todo:6.5、5.3.9、:522。
- merge-zone-runbook:回档围栏值,不得手工 DEL。
- scene-manager-alerts.yaml:新增 ZoneEntryFenceHeldTooLong。
- owner-epoch-alerts.yaml:注明 DbStaleOwnerWriteRejected 在回档后出现属于预期。
- PROGRESS 追加条目,注明 Java 不适用。

文件：`D:/luyuan/xuanming-server-mmo-cpp/docs/design/zone_data_rollback.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/design/zone_data_rollback_en.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/design/zone_data_rollback_zh.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/design/db-write-fencing-mysql.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/design/scene-owner-reentry-barrier.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/design/shared-redis-split-by-purpose.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/design/db-retry-owner-routing.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/design/db-task-kafka-partition-contract.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/ops/godb-pipeline-launch-todo.md`、`D:/luyuan/xuanming-server-mmo-cpp/docs/ops/merge-zone-runbook.md`、`D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/scene-manager-alerts.yaml`、`D:/luyuan/xuanming-server-mmo-cpp/deploy/k8s/owner-epoch-alerts.yaml`、`D:/luyuan/xuanming-server-mmo-cpp/go/shared/ownerepoch/ownerepoch.go`、`D:/luyuan/xuanming-server-mmo-cpp/docs/PROGRESS.md`

## 4. 测试

Claude 不执行任何构建或测试(AGENTS §10.1)。下面的命令交给 Codex 或负责人执行,全部未运行。环境变量:GOTOOLCHAIN=local、GOFLAGS=-mod=readonly、GOPROXY=https://goproxy.cn,direct。

【B1】
- 在 go/shared 下:go vet ./playercache/ ./scenenode/;go test -count=1 ./playercache/ ./scenenode/。
- 在 go/scene_manager 下:go build ./... 与 go test -count=1 ./internal/logic/。
- 在 go/db 下:go test -count=1 -run 'KeysExport' ./internal/kafka/ ./internal/saveoutbox/。
用例:
- 按字面量钉死 PlayerAllData:<id>,并与 login buildParentKey、merge_zone defaultCacheMsgTypes 的口径一致;
- topic 边界:db_task_zone_1 不得命中 db_task_zone_11 / db_task_zone_1_g2;
- 含 glob 元字符的 topic 按字面匹配;
- ParseAppliedCursor 的 v2 与旧格式;
- PendingIndexKey 与 BucketOf;
- NodeLoadKey 与原 NodeLoadKeyFmt 逐字相同。

【B2a–B2d】在 go/db 下:
- 先跑 gofmt -l ./internal/zonerollback ./cmd/zone_rollback_discard 与 go vet 同范围;
- 再跑 go test -count=1 ./internal/zonerollback/... ./cmd/zone_rollback_discard/...;
- 真库门控:ZONE_ROLLBACK_REQUIRE_REDIS=1 ZONE_ROLLBACK_REDIS_ADDR=127.0.0.1:<自起 Redis 7.x 端口> ZONE_ROLLBACK_MYSQL_DSN=<自起一次性 mysql 或 TiDB> go test -count=1 -run 'Integration' ./internal/zonerollback/。不要借用 codex-* 容器;显示 SKIP 不算通过。
用例:
- 契约 :206-208 原有的全部用例:三种 topic 并存;glob 元字符;SCAN 第 2 页失败(退出 1、分族计数正确、同 rid 重跑结果一致);首轮 purge 之后源键再次出现(进下一轮 pass);-expect-run-id 不一致;-version 格式;stopped 遇到非 stopped 阶段的基线。
- fence:
  - 获取幂等;
  - 遇到外人值(merge_zone 的 JSON 值、save_barrier_restore:<id>)→ 3;
  - 有 TTL 的同值也算外人值;
  - 排空:集合只减、超时退出 5 且围栏保留、重跑继续等;
  - release 按值 CAS,外人值 → 3,键不存在 → 0;
  - 围栏缺失时 stopped / inventory / purge / verify 都 → 3。
- fingerprint:
  - 块边界覆盖整个 uint64;
  - NULL 与空串不同摘要;
  - 列顺序变化 → 3;
  - 全部块相同 → 3,带 acknowledge 时放行;
  - .partial 可以续算。
- raise:
  - owner_stop==0 且其它来源 >0 → owner_behind;
  - applied > owner → owner_behind;
  - MaxUint64 溢出;
  - 超过 2^53 的值与非规范十进制;
  - 重跑幂等(owner==C');
  - 维护期计数器前进 → owner_advanced;
  - applied 缺键时建键;
  - applied 超前 → applied_ahead。
- classify:真值表逐格各一个用例,加兜底 unclassified;foreign_home_storage_here;frozen;落点版本 ≥2 的有证明与无证明。
- topology:
  - run_id 正则,src / dst 必须与 merged_into 一致;
  - S≥T;
  - 清单缺失、dry_run=true、同一 run_id 出现两份;
  - At 按秒截断时减 1 秒比较;
  - relocate switched 的 At 晚于 T。
- inventory:
  - post_t 占比阈值;
  - #5:seq>>10 晚于 T+60s → 3,legacy 记 unknown;
  - owner 与基线不同 → 3。
- purge:
  - 归档 sha1 不匹配(写者存活)→ 1;
  - legacy 屏障带 pending → 3;
  - 映射或 location 被改 → route_changed;
  - 先抬后删的原子性:同一段 Lua 失败时零写入;
  - 隔离 RENAMENX 撞上目标 → 1;
  - 死信索引随死信一起隔离。
- verify:C' 不等;游标超过切点;已处理玩家的键重新出现;legacy 补投桶非空。
- strict:
  - (0,0) 的两行 CAS 成 (C',1);
  - torn 时缺行插入并对齐围栏;
  - 两行都缺时插入骨架行;
  - SQL 提交后、HSET 前崩溃,重跑收敛;
  - 屏障已是同值三字段时幂等;
  - 本人仍在桶里 → 拒;
  - storage_elsewhere 的成员留在桶里不影响;
  - 重建后用 login 的 beginParentLoadScript 跑一次冷读校验,必须通过(集成用例可以内联 login 的 Lua 文本做对照);
  - SQL 失败时绝不发布 ACK。

【B3】在仓库根逐个执行 pwsh -NoProfile -NonInteractive -File tools/scripts/tests/<名>.tests.ps1,名字为:k8s_zone_rollback_discard、k8s_zone_rollback_gate_router_mode(PATH 上要有 git)、kafka_offset_reset_output_guard、dev_tools_kafka_offset_reset、k8s_deploy_contract、dev_tools_merge_zone_contract。期望 fail=0。
dry-run 冒烟(命令见 zone_data_rollback.md:519)补充期望:
- 输出里有 0g 的 `-mode fence` 与 6b 的 `-mode release`;
- 七个模式都带 -expect-run-id;
- strict 不再在入口被拒;
- Schema 3 出现 RouteStoreMode。

【B4】在 go/db 下:
- go test -count=1 -race -run 'DeadDedup|DurableEnqueue|PoisonMessage|MoveClaimedRetryToDead' ./internal/kafka/;
- 然后整包 go test -count=1 ./internal/kafka/ 与 go test ./internal/metrics/。
用例:
- 同一 (partition, seq) 组提交重推两次,死信里只有 1 份;
- 重试源副本进死信时被丢弃且计数;
- 旧收据不判重;
- WAITAOF 仍然覆盖 EVAL;
- 指标序列预建为 0。

【B5】C++ 用 MSBuild 串行 /m:1:
- 编 scene 相关库与 cross_zone_test;
- 跑 cross_zone_test --gtest_filter=SaveSeq*:DbTaskDispatch*。新用例:Replace 一份带探针的 landed 之后,快照不再含探针;与未 Stamp 的新消息 IsEqual 为真。
- 告警文件用 promtool check rules deploy/k8s/db-task-partition-alerts.yaml(如有 promtool)。

【B6】文档:python tools/scripts/gen_docs_index.py --check 只比对、不整段重写;另外人工核对 zone_data_rollback 三语版的一致性。

失败时保留 go test -v 首个失败用例的输出、ps1 的全部 [FAIL] 行,以及真库用例的容器日志。不得改断言、不得跳过测试。

## 5. 风险

1. 全部是静态结论:没有编译,没有运行任何测试。miniredis 不支持 Lua 里调用 INFO / DUMP,这部分语义必须在真 Redis 7.x 上实测(RouteReadyReadScript 也标着「未核实」)。如果实测发现 DUMP 不能在脚本里用,改为对 GET / HGETALL 做规范化编码的 sha1。
2. 围栏是永久键:回档放弃后忘了 release,本 zone 归属玩家会一直进不了场景、不能建帮,全服的 route_tool import / repair 也会暂停。靠新告警 ZoneEntryFenceHeldTooLong 和 runbook 兜底;另外,dev 单 zone 部署配 AllowGateZoneAsHomeZone=true 时,scene_manager 不经 data_service,围栏对它无效。
3. 不踢人:挂机的访客可能让 0g 等到超时(退出 5)。主动按归属 zone 踢下线要另立一项,需要 login / gate 的命令通道。
4. strict 下工具会写 MySQL:两表围栏元数据 CAS、torn 对齐、骨架行 INSERT。回档后的库与 T 时刻不再逐字节一致,只是语义等价于「T 时刻没有这一行 = 空骨架」。依赖 proto2mysql 对空消息的列编码与 go/db 写路径一致,要在真库上用 login 冷读核对。
5. T 之后建的角色以原名变成新角色,T 之后的进度全部丢失;玩家看到的回退点比 T 早一个消费延迟。都是已接受的代价,需要运营对外说明。
6. storage_elsewhere(home=本 zone,存储在别处):Step 5 会丢掉它们发往别库的在途写。strict 下由 saveoutbox 在 Step 6 之后补投(所以这些玩家不抬纪元);legacy 下没有补投,只能靠保留的父快照在下次存盘时写回——这是 Step 5 既有的性质,本批没有修。
7. epoch=0 的写者挡不住:守卫判 legacy_zero 直接放行。只能靠围栏、停服检查和 DbOwnerEpochLegacyZero 告警兜底。停服之前已经过了守卫、正卡在 SQL 里的僵尸写,依赖 namespace 删除和 PITR 耗时远大于 SQL 超时。
8. 跨服务状态不随回档回退:guild、mail、收件箱、trade、data_service 全局库。收件箱可能重投 T 之后已消费的条目。
9. owner_behind 会让回档整体停在 4a,而它的修复入口 fencing §9.1 的 epoch_raise 还没实现。
10. 规模:inventory / purge / verify 每次都要全量 SCAN player:zone:*,代价是 O(全服玩家数);分块校验和在停服后与 PITR 后各要全表读一遍,大 zone 是分钟到小时级。冷读洪峰的承载是估算,没有压测数据,不得据此下性能结论。
11. 归档文件含玩家资产明文:报告目录必须受访问控制(run/* 已被 gitignore,文件 0600)。Redis 隔离区只放区级重试族,默认 TTL 720h。
12. 热文件冲突:key_ordered_consumer.go、metrics.go、db-task-retry-alerts.yaml、scene-manager-alerts.yaml、dev_tools.ps1、player_lifecycle.cpp 目前都有他人未提交的改动。B4 / B5 / B6 动手前必须重新 git diff 分清归属,只做本项的最小增量。
13. 两套版本号口径:-version 输出 40 位 main.buildCommit,与 shared/buildinfo 的 12 位规则不同,按契约实现。
14. (b) 会改变压测里存盘跳过率与 DBTask 量的口径,下一轮压测与 R20 等历史基线不可比,复盘要写明(AGENTS §6)。
15. (a) 动的是 go/db 热路径的入队层。人工删死信条目时如果不同时 HDEL 标记,同一消息以后再也进不了死信,所以 L-04 第 2 批工具必须遵守「同一段 Lua 删标记」。
16. 不支持 Redis Cluster 和 RouteStore.Mode=db_first(以 3 拒绝,db_first 要另批设计)。G4a 拆分 Redis 实例之后,逐人 Lua 的原子性前提不成立,由第 0 步 f fail-closed。
17. legacy 协议实际上只对应「go/db 是新镜像、C++ / login 还是旧镜像」的过渡部署。工具落地之后,能部署的镜像都属于严格世代,strict 才是主路径。B2d 未通过真库验证之前,不得对现行部署执行 -Apply。

## 6. 对抗复核意见（待并入，落码前处理）

### 6.1 数据一致性面（结论：needs_changes）

1. **[high] 断点续跑和 verify 会把本工具自己写入的 C' 当成外来证据。U(p) 现读了 applied_epoch、屏障 pending/ACK、行围栏 fence_owner_epoch，但 P2 会把 applied_epoch 抬到 C'，P3 会把 (0,0)、全零 torn 和骨架行写成 V=(C',1)。之后任何一次重跑盘点判定都会得到 U=C'>owner_stop，于是判成 owner_behind 归入 blocked、退出 3。另外 save_barrier 在 P2 的删除族里，P3 重建的屏障在 verify 中会被当作「仍需删除的键族」，续跑的 P2 也会把它再归档删掉一次。设计只声明重跑接受「owner==C'、各族键已不存在」两种状态，没有覆盖 applied==C'、屏障==V、行围栏==V。**
   - 场景：legacy：P2 处理到一半时 SCAN 第 N 页出错，以 1 退出。用同一 rid 重跑 purge，第 1 步原样重跑 inventory 判定，已处理玩家的 consumer:applied_epoch:<topic>:<pid>:player_database 已是 C'>owner_stop，判 owner_behind，返回 3。此时 Step 5 已执行，只能向前，工具却没有能走通的路径。strict：P3 把两行 (0,0) CAS 成 (C',1) 并写入 pending=(C',1)；5b 的 verify 重跑归类时，屏障 epoch 与 fence_owner_epoch 都是 C'，判 owner_behind，且屏障存在、被当作待删族，verify 必然 3。设计自己列出的用例「SQL 提交后、HSET 前崩溃，重跑收敛」在这条规则下同样过不了。
   - 证据：设计 §2「U(p) 取以下来源的最大值…owner_stop<U → owner_behind」、§4 purge 第 1 步「这次重跑接受『owner==C'、各族键已不存在』」、verify「重跑归类,不能出现任何仍需删除的键族或 blocked 玩家」；go/db/internal/kafka/key_ordered_consumer.go 的 luaMarkAppliedEpoch（只升不降，抬到 C' 后不会回落）；go/login/internal/logic/pkg/dataloader/parent_commit_barrier.go 的 beginParentLoad（pending 的 epoch 进入判定）；设计 §5 键族表 save_barrier「归档后 UNLINK;strict 下 P3 重建」。
   - 改法：inventory 产出一份不可变的逐人计划：pid、class、owner_stop、C'、V、各归档键的预期 sha1、行围栏，计划文件的 SHA 写进清单。purge 和 verify 只判定「当前状态 ∈ {计划前态, 计划后态}」。U(p) 只取停服基线和 inventory 当时读到的值，显式排除本工具写入的 applied==C'、屏障三字段==V、V.epoch==C' 的行围栏。清单逐人记录 p2_done / p3_done，P2 跳过 p3_done 玩家的屏障删除。verify 对已处理玩家改为核对「状态等于计划后态」，不再走 U(p) 归类。补三个用例：P2 中途失败后重跑、P3 的 SQL 提交后崩溃再重跑、strict 下 verify 通过。
2. **[high] 缺少帮会 / 交易资产分歧闸。仓库现行的回档契约要求「任何回档」在写玩家数据之前先向 guild 查询 T 之后已终结的资产操作：有分歧默认拒绝，放行要逐条记录作为补偿依据。整区回档会把玩家钱包、背包和 asset_op_ledger 回到 T，guild、trade 库却不回退。本设计没有任何闸，也不出补偿清单，只在风险 8 里泛泛记了一句「跨服务状态不随回档回退」。**
   - 场景：玩家在 T+10min 向帮会捐 1000 金：scene 扣钱并记账（player_database），guild 侧记 APPLIED、帮会资金 +1000。整区回档后玩家钱包回到 T，1000 金还在，帮会资金不回退，凭空复制 1000。反方向，帮会商店或活动在 T 之后发的奖励随回档消失，guild 侧已终结、不会重投，玩家白白损失。全程没有告警，也没有补偿名单。
   - 证据：go/data_service/internal/logic/rollback_logic.go:128-141「回档把玩家的钱包换回快照,而帮会账本不在快照里…所以任何回档在写任何玩家数据之前…先问 guild」；go/data_service/internal/guildcheck/guild_divergence.go:126 ListDivergences；docs/design/guild-phase2/07-rollback-fail-closed.md §7.8.1；proto/common/database/mysql_database_table.proto:148-151（asset_op_ledger 与 currency 同记录）；tools/scripts/k8s_zone_rollback.ps1 Step 3 文案「不回退全局库(…guild、mail、收件箱、trade)」。
   - 改法：inventory 增加资产分歧闸：对 purge 范围内全部玩家以 since=T 查询 guild 的 ListAppliedAssetOpsSince，trade 的 assetop 表做同类查询。有分歧或超出保留期无法证明的，一律 blocked（退出 6）。只有显式给出 -accept-asset-divergence <清单文件> 才放行，并把每条分歧写进报告作为补偿依据。查询失败按拒绝处理（fail-closed）；purge 之后再复查一次。go/db 模块不能 import data_service/internal/guildcheck，需要在 zonerollback 里新增 guild internal gRPC 客户端适配，文件数相应增加。
3. **[high] home≠Z、落点=Z 的玩家（foreign_home_storage_here）的写者不在停服和围栏的覆盖范围内。C++ 按 home 选 topic，go/db 按处理那一刻的落点选库，所以这类玩家的存盘由他们 home 区那个仍在线的 go/db 写进 zone_Z_db。K1 只挡 home=Z；Step 1 只删 Z 的 namespace；relocate 冻结时的 CAS 只检查 home 区的围栏。设计要到 4a（PITR 之后）才把这类玩家归为 blocked，停服复核「zone_<id>_db 不再有写入」这一前提并不成立。**
   - 场景：玩家 P 的 home=W、落点 Z:2（搬库进来，或 pin 合服留下的）。回档 Z 期间，W 的 go/db 一直在线，持续把 P 的存盘写进 zone_Z_db，跨越停服基线、PITR 和影子库替换。4a 判 foreign_home_storage_here，退出 6，走中止路径「换回原库」，W 的 go/db 在窗口内写进影子库的存盘随影子库一起丢掉。strict 下屏障 ACK 已等于 pending，SQL 行却是旧版本：父快照过期后冷读报「SQL 子表围栏尚未对齐父快照提交屏障」，补投器认为已 ACK、不会补投，玩家卡死。同一窗口里，relocate 把 W 归属的玩家搬进或搬出 Z 库也不受 K1 约束：搬进的行被 PITR 抹掉，搬出的行读的是回退中的库。
   - 证据：go/shared/placement/placement.go 包注释「选库只在 go/db 一处发生:按处理那一刻的记录选」；cpp/libs/services/scene/player/system/player_lifecycle.cpp 的 GetDbTaskTopic(homeZoneId, …)（topic 只按 home_zone 选）；tools/merge_zone/relocate.go:278 placementCAS{… FenceZone: rd.Home}，placement_ops.go:530-531（只追加 mergeFenceKey(FenceZone)）；tools/scripts/k8s_zone_rollback.ps1 Step 1b(iii)「zone_<id>_db 不再有写入」。
   - 改法：preflight 只靠 Redis 就能判出这一类，应与 stopped 一起硬拒：SCAN player:placement:*，有效落点=Z 且 home≠Z（包括 frozen 且 storage=Z）即退出 3，先于 0g 和 Step 1 执行；stopped 再复核一次；拓扑清单目录里有未结束、源库或目标库含 Z 的 relocate run，同样拒绝。tools/merge_zone 的 relocate 和 pin 在冻结与切换的 CAS 里，额外要求源库、目标库所在 zone 的 MergeFenceKey 都不存在（改 relocate.go、placement_ops.go 并补测试，计入文件数）。
4. **[medium] 屏障协议按现网事实判定（Fencing.Mode 加世代标记），但 Step 6 部署的是 -NodeImage 指定的回滚目标镜像。工具重建或删除屏障的形状，可能与回档后实际运行的 C++ / login / go/db 协议不一致。设计只在 legacy 分支上用 git 核对镜像，strict 分支不核对目标镜像。**
   - 场景：现网是 strict（新 C++、go/db dual、标记为 strict），回档同时把 node 回滚到 T 时、尚无严格屏障的旧 sha。工具按 strict 重建 pending=V；旧 C++ 走 kSaveIfGuardLuaScript，只写父快照、不推进屏障。下次登录 login 发现父快照 last_save_seq≠seq(V)，报「父快照水位与提交屏障不符」，全员拒登。反过来，现网判为 legacy、目标镜像却是 strict 时，工具只删不建屏障，owner≠0 的玩家全部报「历史玩家父快照与提交屏障缺失」。如果目标 db 镜像是 legacy 而 Redis 里有 strict 标记，CheckLegacyAllowed 会让 go/db 拒启。这些都发生在 Step 5 之后，没有退路。
   - 证据：tools/scripts/k8s_zone_rollback.ps1:300-307（-NodeImage 必填，「回到哪一版」由操作者显式指定）；go/db/internal/saveoutbox/generation.go 的 CheckLegacyAllowed；go/login/internal/logic/pkg/dataloader/parent_commit_barrier.go:101-111；cpp/libs/engine/infra/storage/redis_client/redis_client.h 的 kSaveWithCommitBarrierLuaScript（缺 owner 键即报 missing owner）。
   - 改法：协议判定同时覆盖 Step 6 将要部署的 node / login / db 镜像。Get-TargetBarrierProtocol 对 strict 也做核对：目标 node 提交含 kSaveWithCommitBarrierLuaScript、目标 login 提交含 parent_commit_barrier.go、目标 db 渲染的 Fencing.Mode 为 dual。现网判定与目标判定不一致时，在第 0 步拒绝，并说明要先统一镜像协议。
5. **[medium] #6 只凭 merge:merged_into 判定「T 之后合入」，没有拿合服、撤销清单里的 PlayerIDs 与 post_t_candidate 求交。merged_into 一旦被撤销路径或人工清理掉，或者合入规模低于占比阈值（5% 且多于 20 人），copy 模式合入的玩家就会被当成 T 之后建角，变成空骨架。**
   - 场景：T+1h 把 15 人的小区 W 以 copy 模式合入 Z，事后 merged_into 被撤销流程或人工删掉，合服清单仍在。PITR 后这 15 人在 Z 库没有行，home=Z，落点无记录，归为 post_t_candidate，又低于占比阈值。strict 下物化空骨架，legacy 下冷读得到空骨架；他们的真实数据只剩 W 库的冷副本，玩家被静默清空。
   - 证据：tools/merge_zone/merge_run.go:586-617（copy 模式步骤 1 先拷行，步骤 5 才写 merged_into 并改映射）；tools/merge_zone/unmerge.go:136（撤销路径）；设计 §4 inventory 第 7 步 -max-post-t-ratio（缺省 0.05，且人数 >20）。
   - 改法：inventory 必须读取 -topology-manifest-dir 下所有 dst=Z 或 src=Z 的合服 / 撤销 / 搬库清单，把清单里的 PlayerIDs 与 post_t_candidate 求交，命中一律判 moved_in_after_t（blocked）。merged_into 记录的 run_id 必须能对上一份清单，缺清单即 topology_unproven。post_t_candidate 中 owner_stop>0 的玩家，逐人列进报告，并要求一份人工确认文件，不能只靠比例阈值放行。
6. **[medium] #5「任一行存盘时刻晚于 T+60s 即以 3 拒绝」会误拒合法数据，而且发现不了 PITR 恢复得过早。save_seq 不是时钟：进程水位可以被种子抬到墙钟之后 60s，超界的种子会挂成个人下限，不受上界约束。**
   - 场景：某玩家曾从时钟超前的节点或被手工改过的库载入，个人下限超前一天，T 之前的正常存盘 fence_save_seq>>10 也大于 T+60s。inventory 以 3 拒绝，又没有豁免开关，整区回档无法执行。另一个方向：PITR 实际恢复到了 T−3h，所有 seq 都早于 T，检查照样通过，多丢 3 小时数据而无人察觉。
   - 证据：cpp/libs/services/scene/player/system/save_seq.h:23/35/106（Observe 接受不超过 (nowMs+60s)<<10 的种子，Next=max(last+1, nowMs<<10)）；player_lifecycle.cpp:4165-4176（超界种子挂成 PlayerSaveSeqFloorComp 个人下限，进程水位不抬）。
   - 改法：改为分布判定加逐人清单：超出 T+60s 的行数超过阈值才拒绝；少量离群的行写进报告，并要求 -acknowledge-seq-outliers <文件>。另加下界判定：停服基线里 seq>>10 落在 (T−Δ, T] 区间的行，PITR 后应仍然存在且值相同；PITR 后的 max(seq>>10) 明显早于 T 时，判为 PITR 过早并拒绝。
7. **[medium] 「列清单不同 → 3」，加上骨架行按编译期 proto 插入，使跨越 schema 变更的回档走不通。而最常见的灾备触发恰恰是一次有问题的发布，这类发布往往带加列。**
   - 场景：T+30min 发布了一个加列的版本（go/db 启动时 ADD COLUMN），出事后回档到 T。PITR 后的库没有新列，inventory 发现列清单不同，以 3 拒绝。即使放宽这一条，P3 用新 proto 的列 INSERT 骨架行也会在旧 schema 上失败，而且发生在 Step 5 之后。
   - 证据：设计 §4 inventory 第 2 步「列清单不同 → 3」、§3 P3「proto2mysql 的 ExistsByPK+Insert 插入」；proto/common/database/mysql_database_table.proto:140-142（老库由 cmd/migrate 只做 ADD COLUMN）；k8s_zone_rollback.ps1:161-162（工具必须与现网 go/db 同一提交，也就是新 proto）。
   - 改法：基线和重算都按两次 information_schema 的列交集计算，列差异写进报告并要求显式确认。strict 预检按 PITR 后实际存在的列做一次骨架行 INSERT 可行性检查（列存在、NOT NULL 列有默认值），不可行就在 Step 5 之前拒绝。P3 只写实际存在的列。
8. **[medium] 死信去重的索引字段只有 <partition>:<seq>，不校验内容。HSETNX 返回 0 即视为重复，直接不入队。offset 一旦被复用，真正的新死信就会被静默丢掉，而调用方仍按「已入队」ACK 了 Kafka 位点。**
   - 场景：dev 或 staging 删除并重建同名 topic（或按 AGENTS §6.2 清了 broker 数据目录）而没清 Redis，offset 从 0 重新计数。一条新的毒消息 (p3, seq=42) 命中旧索引，HSETNX 返回 0，没有 LPUSH，消费者照常 ACK，这笔存盘永久丢失，指标上看不出异常。moveClaimedRetryToDead 同样只 LREM processing。另外，人工 LREM 死信时没有 HDEL，同一消息之后再被重投也会被丢弃。
   - 证据：设计 (a) deadEnqueueLua「if HSETNX(...)==0 then return 0 end」，字段取 msg.Offset+1 / task.seq；go/db/internal/kafka/key_ordered_consumer.go 的 wrapRetryPayload（[seq 8B][partition 4B]，seq=offset+1）；AGENTS.md §6.2（压测前允许清 broker 数据目录）。
   - 改法：索引字段的值存载荷 SHA1（或 task_id+sha1）。HSETNX 失败时比对这个值：相同才判为重复（duplicate_dropped）；不同则照常 LPUSH，并计 collision 指标、触发告警。索引键名带上 topic 实例标识，或者让 topic 重建 / offset 重置流程同步删除索引。现有会把条目搬出死信的人工 runbook，也一并要求在同一段 Lua 里 HDEL。
9. **[medium] P1 按整键 RENAMENX 加 PEXPIRE 720h，把本 topic 下所有玩家的死信和重试收据都放进带 TTL 的隔离区，其中包括不在回档范围内的 storage_elsewhere 玩家，而他们的目标库并不回退。**
   - 场景：home=Z、落点在全局库的玩家有一笔存盘因排序冲突进了死信，目标库不回档，这笔仍需人工裁决。P1 把整个 kafka:dead:queue:<topic> 改名隔离后，DbTaskDeadLetterNonEmpty 随即静默；720h 后隔离键过期，这笔待裁决的写连同证据一起消失。legacy 下，这类玩家的重试收据同样随 Step 5 和 P1 一起被丢弃。
   - 证据：cpp/libs/services/scene/player/system/player_lifecycle.cpp（topic 与 player:save_pending:<homeZone> 都按 home 选，storage_elsewhere 玩家的写也走本 zone 的 topic）；设计 §4 purge P1（RENAMENX + PEXPIRE -quarantine-ttl 缺省 720h）；go/db/internal/kafka/store_failure.go:171-196 sampleRetryQueues（死信长度告警的数据来源）。
   - 改法：P1 对死信和重试条目逐条解码 player_id 分流：purge 范围内的进隔离区；其余留在原队列，或者进一个不设 TTL 的隔离键，并逐条写进报告、保留告警。TTL 只作用于已确认属于回档范围的条目。报告里列出受影响的 storage_elsewhere 玩家。
10. **[medium] 永久围栏 K1 可能被合服工具的现有提示文案引导人手工删除。merge_zone 释放和续期围栏时按子串匹配 run_id，有误续期、误删的面。guild 的围栏读取是可选配置，「不用改任何在线服务」这个前提不完全成立。**
   - 场景：PITR 期间有人对 W→Z 执行合服，SetNX 失败，提示文案让他「确认没有 merge_zone 进程后手工 DEL merge:in_progress:Z」。他照做，回档围栏失效，本 zone 归属的玩家可以在他区重新进场并铸造 owner；下一个模式虽然会以 3 拒绝，但如果已经过了 Step 5，就只能向前。如果显式传入的 -RollbackId 恰好包含某次合服的 run_id，一个残留的 merge_zone keepAlive 会把永久的 K1 PEXPIRE 成带 TTL 的键（fail-open）。guild 没配 MergeMarkerRedis 时，建帮、经济类写不受围栏约束。
   - 证据：tools/merge_zone/fence.go:151-155（「DEL %s manually」）、audit_checks.go:546-550（「DEL them once no merge_zone process is alive」）、merge_run.go:1367-1371；fence.go:73-86 的 releaseFenceScript / refreshFenceScript 用 string.find(cur, ARGV[1], 1, true)；go/guild/internal/config/config.go:60-71（「整段可缺失 = 闸门不生效」）。
   - 改法：merge_zone 遇到非 JSON 值，或以 zone_rollback_discard: / save_barrier_restore: 开头的值时，提示改为「不得手工删除，由持有方工具 -mode release 撤除」（改 fence.go 和 audit_checks.go，计入文件数）。rid 入口校验拒绝形如 merge run_id（src-dst-时间戳-随机串）的值。K2 的值也加 zone_rollback_discard: 前缀，避免与 barrierrestore 的 MaintenanceID 混淆。deploy-facts 记录 guild 的 MergeMarkerRedis 是否配置，未配置时 preflight 告警或拒绝。
11. **[low] 同一 rid 的写模式之间没有执行互斥。purge.json 由进程整体落盘，后写的覆盖先写的，而 verify 以它为输入逐人复核。**
   - 场景：操作者在第一个 purge 还没结束时，另开一个终端用同一 rid 重跑。两个进程各开一轮 pass，后结束的进程写出的 purge.json 覆盖了先结束那份的已处理名单，verify 的逐人复核因此漏人，漏掉的玩家缓存是否删净没有被核对。
   - 证据：设计 §7「没有全局锁」、§4 purge 第 6 步「写 purge.json(累计全部轮次)」、verify「判定参数加 -purge-report」。
   - 改法：每个写模式在 rollback:quarantine:<z>:<rid>:lease 上取一把带令牌、PX 续期的租约，写 Lua 里校验令牌。已处理名单只从 Redis 清单和各 pass 的 ndjson 汇总生成，不依赖单个 purge.json 文件。
12. **[low] 设计依赖他人未提交的在途代码，引用的行号也已漂移。barrierrestore 整个包和 saveoutbox/generation.go 目前都是未跟踪文件；player_lifecycle.cpp 正被他人修改，设计引用的 :4275-4278、:4311-4312 已对不上。**
   - 场景：另一会话改名或删除 barrierrestore，或调整 GenerationKey 的格式，本工具就会编译失败或键名漂移。「工具与线上 go/db 同一提交」的校验，也要求这些文件已经提交。B5 若照旧行号动手，可能改错位置。
   - 证据：git status：?? go/db/internal/barrierrestore/、?? go/db/internal/saveoutbox/generation.go、M cpp/libs/services/scene/player/system/player_lifecycle.cpp；当前快路径在 player_lifecycle.cpp:4332-4334，探针盖章在 4375-4376。
   - 改法：MaintenanceKey 下沉到 shared/savebarrier（或在工具内定义常量，用测试钉死字面量，并在 barrierrestore 存在时交叉断言）。B1 开工前确认 generation.go 已提交，否则在 keys_export 里定义 GenerationKey 并钉死字面量。B5 动手前重新 git diff 定位行号。
13. **[low] 排空的首次全量扫描与在途的 EnterScene 有竞态：scene_manager 先通过 RPC 读 merging 标志，之后才 CAS location，两步不是原子的，所以「他区在线集合只减不增」在围栏刚立起的几秒里不严格成立。**
   - 场景：一次跨区 EnterScene 在围栏写入前 1ms 通过了检查，随后才完成 location CAS。fence 模式如果立即全量扫描，集合为空后马上「再全量确认一次」，两次扫描都可能早于这次 CAS，于是 0g 判为通过；要等到 stopped 才以 3 拒绝，那时 zone 已经停服。
   - 证据：go/scene_manager/internal/logic/home_zone.go:186-188（读到 merging 后拒绝，放行路径在后续步骤才写 location）；go/data_service/internal/logic/rollback_logic.go:144-150 的 guildSettleDelay（立栅栏后先沉降等待的先例）。
   - 改法：获取围栏后先等待 ≥ HomeZoneLookupTimeout + EnterScene 处理上界（建议 30s，同 guildSettleDelay），再做首次全量扫描；集合为空后再隔一个沉降期复扫一次，才判为排空完成。
14. **[low] torn 的处置规则只因为当前 player_database_1 只有探针和围栏列才是安全的；设计已把玩家表推广为全部 OptionPrimaryKey=player_id 的表，以后加列就会失效。分块校验和的 SQL 写法也有精度和分隔歧义问题。**
   - 场景：以后给 player_database_1 加了业务列，或者有新表纳入围栏：torn 时把内容较旧那一行的围栏抬到 V，会把跨表不一致的状态标成「已提交」，工具替玩家绕过了严格协议本来要拦的情形。BIT_XOR(CONV(...)) 不做 CAST AS UNSIGNED 时，字符串到数字的隐式转换可能丢精度；CONCAT_WS('#') 在列值含 '#' 时会碰撞，「全部块相同」可能误判。
   - 证据：proto/common/database/mysql_database_table.proto:171-187（player_database_1 只有 player_id / stress_test_probe / fence_*）；设计 C10（「两张玩家表」改为全部 OptionPrimaryKey=player_id 的表）；设计 §4 stopped 第 3 步的校验和表达式。
   - 改法：torn 处置加一条断言：落后那张表除 player_id、探针、fence_* 外有任何非默认的业务列，即判 blocked（torn_cross_table）。校验和按 pt-table-checksum 的写法：BIT_XOR(CAST(CONV(SUBSTRING(MD5(...),1,16),16,10) AS UNSIGNED))，列值用长度前缀编码或逐列 MD5 后再拼接；并在真 TiDB 上验证。

### 6.2 水平扩展与运行面（结论：needs_changes）

1. **[high] 工具压在全服共用的单实例 Redis 上(所有 zone 的同步登录路径都走它),设计里没有任何限速、在途上限或按延迟退避;全键空间 SCAN 重复跑十几遍。**
   - 场景：purge 时 16 个 worker(上限 64)各推一批 200 段 Lua,每段在 Lua 里做 sha1hex(DUMP 父快照);同时用 pipeline 批量取 DUMP,父快照每人几十 KB,经端口转发慢慢读回。Redis 主线程被占满,回包堆在服务端输出缓冲里(没设 maxmemory,也没设 limits)。后果:其它所有 zone 的 login 拿锁 / EnterGame 超时(EnterLookupTimeout 1.5s),scene_manager 选主锁抖动;最坏情况 Redis Pod 被 OOMKill,全服掉线。SCAN 一次要扫整个实例的键空间(MATCH 在服务端过滤),每名玩家约 30 个键:9 张表缓存、9 个 consumer:applied、PlayerAllData、owner_epoch、location 等。几个 zone 同时回档,负载成倍叠加。
   - 证据：deploy/k8s/manifests/infra/redis.yaml 文件头写明:共享 Redis 在同步登录路径上、仍是单实例;:173-175 不设 maxmemory、保持 noeviction;没有 client-output-buffer-limit 和 lazyfree 配置。tools/scripts/k8s_deploy.ps1 的 Redis 连接总账注释:所有 zone 共连 infra namespace 里同一个 Redis。设计 §7:-workers 缺省 16、上限 64,pipeline 每批 200,SCAN COUNT 1000。§4 P2a 用 pipeline 读 DUMP+PTTL,P2c 在 Lua 内做 sha1hex(DUMP)。全量 SCAN 次数:fence 的 H 遍加排空后全量确认、stopped 的 H/M、inventory 的 M/H/P 三遍、purge 原样重跑 inventory 再加 P1、verify 重跑归类再加 consumer:applied:<topic>:*,合计 12 次以上。
   - 改法：(1) 加全局令牌桶 -redis-ops-per-sec,缺省取保守值;加在途字节上限 -max-inflight-bytes,如 8MiB,按 STRLEN/MEMORY USAGE 分批。Redis 阶段 worker 缺省 4、上限 8,并显式设 go-redis PoolSize=workers+2(不设则默认 10×GOMAXPROCS)。(2) 自适应退避:每批测 PING RTT 或本批 p99,超阈值就暂停并加倍等待。(3) SCAN COUNT 降到 500,页间 sleep。(4) 砍掉重复的全量 SCAN。围栏立起后 home=Z 集合只减不增(前提是另一条 backfill 漏洞修好),stopped 落一份 H 集合快照(64 分片 NDJSON,SHA 记进清单);inventory 和 purge 读快照并逐人 MGET 复核,只在 verify 末尾做一次全量 SCAN 兜底。P1 重试族按已知键名直接枚举:ready、p0..p{N-1}、legacy processing、instances 里登记的实例;只保留一次 processing 前缀 SCAN 兜孤儿。(5) ps1 第 0 步统计 merge:in_progress:* 里值以 zone_rollback_discard: 开头的个数,超过 N 就拒绝。这是准入控制,不是正确性锁。(6) 验收由 Codex 在压测环境测:purge 期间其它 zone 的 entergame p99 退化不超过约定阈值。
2. **[high] 父快照常驻、没有 TTL,所以「删除前把 DUMP 归档到报告目录」实际上是把整个 zone 的玩家数据全量导出,经端口转发写到操作者本机。**
   - 场景：一个 50 万玩家的 zone,父快照每人约 40KB:原始约 20GB,base64 后约 27GB。经 kubectl port-forward(单流、容易断)要传几个小时。中途一断就退出 1,重跑又要先重跑 inventory 的全部判定(见另条)。本机磁盘写满时 fsync 失败,purge 停在半途。玩家资产明文散落在笔记本上。
   - 证据：go/login/internal/logic/pkg/dataloader/new_player_seed.go:70 注释:scene 首次存盘的裸 SET 会去掉 TTL。sync_loader.go:138 的 playerAllDataParentTTL=24h 只在冷读组装时生效。k8s_zone_rollback.ps1 契约 :159 写明 -RedisHost 可以是端口转发;报告目录是 run/logs/zone-rollback/<rid>/。设计 §4 P2a/b:对每个已处理玩家 DUMP 父快照、asset_sql_commit、save_barrier,base64 写进 purge-archive-p<N>-s<NN>.ndjson。
   - 改法：(1) 归档范围收窄到「证据」:save_barrier 和 asset_sql_commit 全量归档(体积小);父快照只归档 strict 下带 pending/outbox 的玩家和 blocked 名单。其余玩家:inventory 在 Lua 里算 sha1hex,只回传 40 字节,purge 的删除 Lua 复核这个 sha1,DUMP 不出 Redis。(2) 全量证据改为停服后给共享 Redis 的 AOF/RDB 留一份副本(ps1 打印运维步骤),或者把工具改成集群内 Job 加 PVC 运行,不走端口转发。(3) inventory 输出预计归档字节数(MEMORY USAGE 抽样累加),超过 -max-archive-bytes 或报告目录可用空间就以 3 拒绝。
3. **[high] 排空和 stopped 判「他区在线」用的是 decideLocationZone,它只判 location 属于哪个 zone,判不了玩家是否真在线。location 没有 TTL,节点崩溃留下的陈旧记录会让排空永远等不完。**
   - 场景：区 Y 的某个 scene Pod 被 OOMKill,上面 30 名 home=Z 玩家的 location 留在 Y,节点已经不在 load 集里,这些玩家之后再没登录。fence 排空会一直把这 30 人算成「他区在线」:30m~6h 后退出 5,重跑还是 5;stopped 的「他区在线即 3」也永远过不去。操作者只能放弃回档,或者手工 DEL location,而那会破坏 scene_manager 交接门禁的证据。
   - 证据：设计 §1 fence 排空:用 decideLocationZone 的同一真值表区分陈旧与他区在线。tools/merge_zone/scene_hot_state.go:277 的 decideLocationZone 只按 zone_id 或 scene:{id}:zone 判归属,不看节点存活。scene_manager/internal/logic/changesceneutil.go 的 UpdatePlayerLocation 是 location 唯一写入口,写入不带 PX;只有 LeaveScene(leavescenelogic.go deletePlayerLocationGuarded)会删。scene_manager 自己判陈旧记录用的是 enterscenelogic.go 的 playerLocationOwnerGone / playerLocationOwnerDead:scene_nodes:zone:<z>:load 的 ZCARD,load_reporter.go:1055 的 IsNodeAlive(ZSCORE),reentryBarrierBlocks;另有 awaitingPlacementExpired。
   - 改法：把 scene_manager 的存活判据下沉到 go/shared/scenenode:纯函数 LocationLiveness 加对应读法,scene_manager 和工具共用,共用测试向量。判法:节点在 scene_nodes:zone:<z>:load 里 → 在线;不在 load 集且 node:zone:<z>:<n>:death_at 已过再入屏障 → 陈旧,可忽略;等待落点按重定向票据是否过期判;读失败或判不出 → 按在线处理(保守)。fence-drain.json 分三类列出:在线 / 陈旧已忽略 / 判不出。新增约 2 个 shared 文件,scene_manager 改 1~2 个,计入总文件数。
4. **[high] 围栏能不能挡住在他区的本 zone 玩家,取决于全服每个 zone 的 data_service 和 scene_manager 都不早于 2026-09-29。设计只核对目标 zone,所以「不改在线服务、滚动升级期间也生效」不成立,旧镜像下会 fail-open。**
   - 场景：zone Y 的 data-service 镜像还停在 2026-09-20,被全局发现选中来应答。home=Z 玩家在 Y 换图时拿到 HomeZoneMerging=false,被放行:location 和 owner_epoch 被推进,存盘写进 topic Z。好的情况是 stopped/inventory 的 owner 复核以 3 拒绝;坏的情况是这发生在 Step 5 切点之后、6b 之前,T 之后的写在 zone-up 后被消费,落进回档后的库。还有一种:以后任何一个 zone 切到 db_first 且 ReadRequiresReady=true,就绪标记不可信时 data_service 回源读库里的 zone_merge_fence 表,只写在 Redis 的 K1 被绕过。
   - 证据：scene_manager/internal/logic/home_zone.go:175:旧版 data_service 不填这个字段,读作 false。home_zone_merging 字段和 rejectHomeZoneMerging 由 4493b3c9f(2026-09-29)引入,git log -S 可查。tools/scripts/k8s_deploy.ps1 的 DataServiceRpc 注释:data-service 按 zone namespace 各部署一份,但发现键 dataservice.rpc 不分 zone,任一 zone 的 scene_manager 都可能由任一 zone 的实例应答。data_service/internal/routing/route_read.go 的 getPlayerHomeZoneAndMergeFenceReady:缓存不可信时调 routeReader.GetHomeAndFence,即 store/route_reads.go:44 的 LEFT JOIN zone_merge_fence。设计 §1 决定 #1 原话「现网所有版本…都只看 EXISTS」。ps1 的 Get-DeployFactsProblems 只读目标 zone 的配置。
   - 改法：ps1 第 0 步 f 和 0g 加全局核对:kubectl 列出所有 zone namespace 加 infra 里的 data-service、scene-manager 镜像 tag,逐个检查 git merge-base --is-ancestor 4493b3c9f <tag>,任一不满足或判不出就拒绝。所有 data-service ConfigMap 必须 RouteStore.Mode=redis 且 ReadRequiresReady 不为 true;否则拒绝,或另批设计让工具同时写 zone_merge_fence。stopped 和 inventory 再核一次,防回档期间别的 zone 发布了旧镜像。runbook 写明:回档窗口内冻结全服 data-service / scene-manager 的发布。
5. **[medium] 全表分块校验和跑在全服共用的 MySQL 上(1 核、128Mi buffer pool),按块并行、不限速,而且 purge 每次重跑都要再全表读一遍。**
   - 场景：一个 20 万玩家、9 张表的 zone:全列 MD5 吃满唯一的 1 核,128Mi buffer pool 被冷数据冲掉。其它 zone 的 go/db 落库延迟飙升、Kafka lag 上涨、冷读登录超时。16~64 条连接远超工具预留 OpsReserve=20,碰上其它 zone 滚动发布时逼近 max_connections=1800。purge 因一次 Redis 抖动退出 1,重跑又是一次全表校验和:停服、inventory、每次 purge,合计至少 3 次全表读。
   - 证据：tools/scripts/k8s_deploy.ps1:所有 zone 的 go/db、data_service、Java gateway 与全局的 friend、trade、player-inbox 共用 infra namespace 里同一台 MySQL;Get-MysqlConnectionPlan 里 OpsReserve=20(含 merge_zone、route_tool、人工会话)。deploy/k8s/manifests/infra/mysql.yaml:172-178:innodb_buffer_pool_size 默认 128Mi,「1 核仍是开服硬瓶颈」;:90 max_connections=1800。设计 §4 stopped:每块 BIT_XOR(…MD5(CONCAT_WS(全部列…)));§7「分块校验和按块并行」;purge 第 1 步「在进程内原样重跑 inventory 的全部判定」,包括重算全部块。
   - 改法：(1) 照 pt-table-checksum 的完整做法来:单连接串行(最多 2 路);按目标块耗时 0.5s 自适应块大小;每块前查 Threads_running,超阈值就暂停(TiDB 用低优先级资源组,并调低 tidb_distsql_scan_concurrency)。MySQL 连接池 SetMaxOpenConns 不超过 2,写进 OpsReserve 说明。(2) 校验和只在 stopped 和 inventory 各算一次,块级摘要写进 inventory.json,SHA 记进清单。purge 和 verify 不重算,只复核 inventory.json 的 SHA 和「停服后库上没有写者」的 Redis 证据:围栏、owner 基线、foreign_home_storage_here=0、没有进行中的搬库(见另条)。
6. **[medium] 进度模型是全有或全无:任何一个错误都取消全部,不做逐项重试,每次重跑都从头盘点。规模一大,长时间的 purge 很难跑完,每次重跑还给共享存储造一次尖峰。**
   - 场景：一个 4 小时的 purge 在第 3 小时因为端口转发断了一次、或单个 pipeline 超过 3s,退出 1。重跑先重做全部判定(多遍全键空间 SCAN 加全表校验和),再从 0 扫全部玩家。高负载下反复失败,可能永远跑不完。
   - 证据：设计 §7:worker 用 errgroup 加可取消的 ctx,任一 SCAN 页、SQL 或 Lua 出错就取消全部、以 1 退出;写操作 MaxRetries=-1,不在工具内重试,由同 rid 的整体重放负责;每次 purge 开新一轮 pass 重扫全部范围;每次 Redis 调用 3s。AGENTS.md §11.3:重试只用于可重试且幂等的操作,须有超时、次数上限、退避与抖动。
   - 改法：SCAN 用同一 cursor 重发、只读 SQL、以及逐人的「读 sha1 → 归档 → Lua」(Lua 接受已抬升、已删除,可重入),遇到网络类错误都做有界重试:3 次,指数退避加抖动。只有命中 owner_advanced / applied_ahead / route_changed / fence_lost / archive_mismatch 这类逻辑拒绝字才整体停。清单按 64 个分片记 shard_done,只作重跑时跳过的提示,正确性仍靠幂等重放;已完成分片在 verify 里抽检。Redis 超时按批大小和在途字节计算,不用固定 3s。
7. **[medium] 围栏只挡 home=Z,挡不住「落点在 Z」和几个离线工具。搬库、钉落点、回填映射都能在回档窗口内改变回档范围,甚至写进正在做 PITR 的库。**
   - 场景：(a) Z 正在 PITR,运维把一批 home=Y 的玩家 relocate 进 zone_Z_db。relocate 按 home=Y 查围栏,不受 K1 阻挡,行写进正在恢复的库:恢复后要么被覆盖,要么留下半截。inventory 判出 foreign_home_storage_here 退出 6 时,PITR 已经做完了。(b) inventory 报 legacy_owned blocked,操作者按提示跑 merge_zone -backfill-home-zone(它不查围栏)。这些玩家变成 home=Z,却不在 stopped 的 owner 基线里,inventory 按「不在基线里却在范围内」以 3 拒绝。停服基线在 PITR 之后无法重做,回档卡死在 4a。
   - 证据：tools/merge_zone/relocate.go:278 placementCAS{FenceZone: rd.Home};pin_placement.go:98 FenceZone: zone;placement_ops.go:472 只查 merge:in_progress:{FenceZone}。go/data_service/internal/routetool/importer.go:40 ImportNote:relocate / pin-placement / backfill 不立围栏,冻结窗口由人保证。tools/merge_zone/backfill_home_zone.go:230 wp.SetNX(player:zone…) 不查 merge:in_progress。设计 §2「这些 Redis 来源在停服之后不会变(围栏加停服)」;§6 foreign_home_storage_here 只在 inventory(PITR 之后)判 blocked;legacy_owned 的修复入口是 merge_zone -backfill-home-zone;§2 inventory「不在基线里却在范围内的玩家也算违例 → 3」。
   - 改法：(1) relocate 和 pin-placement 的 CAS 把源落点 zone、目标落点 zone 的 merge:in_progress 也放进 KEYS,存在即拒。backfill_home_zone 改成在一段 Lua 里先查 merge:in_progress:{zone} 再 SETNX。(2) preflight、fence、stopped 三处只读判 foreign_home_storage_here(player:zone≠Z 且有效落点=Z,只看 Redis 就能判)和 legacy_owned(本库有行而无映射,只读 SQL 可判),存在就以 3 拒绝,要求在立围栏之前修好。(3) -topology-manifest-dir 下有涉及 Z、还没走到 switched 或 aborted 的 relocate 清单就以 3 拒绝。tools/merge_zone 预计多 4~6 个文件(含测试),计入总数。
8. **[medium] 新告警 ZoneEntryFenceHeldTooLong 的指标名和分组标签都写错了,照设计原文落地会永远不触发。而且它只能靠有人重试进场才有样本。**
   - 场景：回档在 Step 4 前放弃,走中止路径 zone-up,但忘了 release。本 zone 归属玩家全部进不了场景,告警规则查的是一个不存在的序列,不会报警,只能等玩家投诉。zone 一直没 zone-up、或玩家已经放弃重试时,同样没有信号。
   - 证据：go/scene_manager/internal/metrics/metrics.go:33 const subsystem = "scene_manager",所以真实指标名是 scene_manager_enter_scene_rejected_total,标签是 {zone_id, reason};zone_id 是 gate zone,见 home_zone.go 的 rejectHomeZoneMerging。deploy/k8s/scene-manager-alerts.yaml 现有规则一律用 scene_manager_enter_scene_rejected_total,按 zone_id 分组。设计 §8 写的是 sum by (namespace)(increase(enter_scene_rejected_total{reason="home_zone_merging"}[30m]))>0。
   - 改法：表达式改为 sum by (zone_id) (increase(scene_manager_enter_scene_rejected_total{reason="home_zone_merging"}[30m])) > 0,补 promtool 规则测试。再加两道不依赖流量的兜底:ps1 Step 7 硬断言 K1、K2 都已不存在;k8s_deploy 的 zone-up 结束时只读 GET merge:in_progress:<zone>,如果值以 zone_rollback_discard: 开头、而报告目录里没有未完成的回档 entry-state,就打印醒目 WARN 和 release 命令。
9. **[medium] release 继承了「每个模式开头」的全部前置检查(deploy-facts、MySQL 身份、玩家表交集、镜像提交一致)。撤围栏会被这些和撤围栏无关的检查卡住,zone 被永久封锁。**
   - 场景：中止路径:Step 3 已经恢复到影子库后放弃,原库改了名、影子库没换回,SELECT DATABASE() 或表交集过不了,release 以 3 拒绝,本 zone 一直封着。另一种:Step 6 用不同的 go/db 镜像起了 zone,工具与镜像提交不一致被拒。还有一种:新开终端里 DSN 环境变量没了。
   - 证据：设计 §4「每个模式开头」:deploy-facts Schema 3;MySQL SELECT DATABASE()==placement.StoreDBName(zone);玩家表 ∩ information_schema。C1:新增 fence / release,沿用公共参数(含 -mysql-dsn-env、-deploy-facts、-barrier-protocol)。契约 :160-162:用 -deploy-facts 的 DbImage 复核构建身份。k8s_zone_rollback.ps1:2467:先恢复到影子库再替换,原库改名保留。
   - 改法：release 只做三件事:INFO 的 run_id 等于 -expect-run-id、cluster_enabled:0、按值 CAS 删 K1 和 K2。不读 deploy-facts、不连 MySQL、不比镜像。只需 -redis、-expect-run-id、-zone、-rollback-id 四个参数;值不等于本 rid 时打印当前值。fence 同样只依赖 Redis 和 preflight 已落盘的结果。补测试:MySQL 不可达、deploy-facts 缺失时 release 仍成功。
10. **[low] kafka:retry:guard 键里 topic 不在前缀位置,按设计的 MATCH 规则拼不出正确的按 topic 匹配,容易误删全服的键。而这些键 5 分钟就自然过期,根本不需要删。**
   - 场景：实现者照键族表用 RetryGuardKeyPrefix 做 SCAN MATCH kafka:retry:guard:*,UNLINK 掉全服所有 zone 的认领 / ack 防重记录。其它 zone 的 go/db 在 go-redis 内部重发时可能重复 POP、重复 ack,还白白多一次全键空间 SCAN。
   - 证据：go/db/internal/kafka/retry_ownership.go:103-104:retryGuardKey = "kafka:retry:guard:" + kind + ":" + processingKey + ":" + nonce,processingKey 形如 kafka:retry:processing:<topic>[:inst];:88 retryGuardTTL = 5 * time.Minute。设计 §5 键族表:kafka:retry:guard:* 用 kafka.RetryGuardKeyPrefix,处置 UNLINK;MATCH 规则为 EscapeRedisGlob(含 topic 的前缀) + 边界 + '*'。
   - 改法：从 purge 里删掉 guard 族:停服加 PITR 远超 5 分钟,这些键早已过期。verify 只对 kafka:retry:guard:{claim,ack}:kafka:retry:processing:<转义后的 topic> 加 ':' 边界这两种显式模式断言为 0。补测试钉住 db_task_zone_11 的 guard 键不被碰到。
11. **[low] 隔离键靠 PEXPIRE 720h 过期释放。共享实例没开 lazyfree-lazy-expire,大列表或大 HASH 到期时会在主线程同步释放。**
   - 场景：30 天后某个时刻,主动过期循环同步释放一个几十万元素的死信列表或死信索引 HASH,主线程阻塞几十到几百毫秒,全服登录同时抖动。时间点不可预期,当时也没人值守。
   - 证据：deploy/k8s/manifests/infra/redis.yaml 没有 lazyfree-lazy-expire(Redis 缺省 no),实例单点且在同步登录路径上。设计 §4 P1:RENAMENX 后 PEXPIRE(-quarantine-ttl,缺省 720h),对象包括 kafka:dead:queue:<topic> 列表和新增的 kafka:dead:index:<topic> HASH;这两者会长期累积。
   - 改法：隔离键不靠 TTL 释放:新增 -mode quarantine-drop,人工复核后用 UNLINK 删除(UNLINK 对大键是异步的);PEXPIRE 只作远期兜底,写进 runbook。也可以单独申请在 redis.yaml 打开 lazyfree-lazy-expire yes,但这是持久配置变更,需负责人批准。
12. **[low] 复用 merge:in_progress 带来全服副作用和误操作面:任何一个 zone 的回档围栏都会挡住全服的 route_tool;合服撞上围栏时的报错文案会引导操作者手工 DEL;RemapHomeZoneForMerge 只看围栏在不在。**
   - 场景：Z 回档持有围栏数小时(排空最多 6h,再加 PITR),这期间全服 route_tool import/repair 都跑不了。另一名操作者对 Z 发起合服撞上围栏,照报错提示手工 DEL,回档中途围栏就没了;只有到下一个复核点才会发现。有人持 admin token 误调 RemapHomeZoneForMerge(source=Z),会因为「围栏存在」被放行。
   - 证据：go/data_service/internal/routetool/importer.go:18 和 repair.go:22:SCAN merge:in_progress:*,有一把就退出 3。tools/merge_zone/fence.go:155-157 的报错原文:if that run is dead, DEL %s manually after confirming no other merge_zone process is alive。data_service 的 constants.ErrCodeMergeFenceMissing:RemapHomeZoneForMerge 只要求源区围栏存在,不校验持有者。
   - 改法：merge_zone 的 acquireMergeFence 撞围栏时识别 zone_rollback_discard: 前缀,报错改为「回档进行中,禁止 DEL,请用 zone_rollback_discard -mode release」。route_tool 只对自己作用范围内的 zone 的围栏拒绝;不改的话,在 runbook 里写明全服暂停的代价。RemapHomeZoneForMerge 不校验持有者,登记为残余风险。tools/merge_zone/fence.go 及其测试计入总文件数。
13. **[low] K2 的键名来自另一会话还没提交(未跟踪)的 barrierrestore 包,回档工具和别人的在途代码有编译耦合。**
   - 场景：B2a 编译依赖一个未提交、可能改名或被放弃的包。对方改了 MaintenanceKey 的拼法,回档与 save_barrier_restore 之间的互斥就静默失效。
   - 证据：git status 显示 ?? go/db/internal/barrierrestore/ 和 ?? go/db/cmd/save_barrier_restore/。设计用 K2 = barrierrestore.MaintenanceKey(restore.go:421),同时声明「不改 barrierrestore」。
   - 改法：B1 把 MaintenanceKey 定义在已跟踪的 go/shared/savebarrier,用测试钉住字面量 player:save_barrier:maintenance:<z>,回档工具只依赖这一份。barrierrestore 落地时改为引用它,由对方会话来改。
14. **[low] 重开之后的冷读洪峰没有真正的节流:zone 令牌桶缺省值很高,登录排队又还没放开。**
   - 场景：6b 撤围栏的一瞬间,排队重试的玩家以最高 2000/s(burst 5000)涌入。#7 删了全部缓存,每人一次 9 表冷读,经 go/db Z 打到 1 核的共享 MySQL,其它 zone 的落库跟着排队。
   - 证据：tools/scripts/k8s_deploy.ps1 削峰档位:ZoneDefaultRps=2000、ZoneDefaultBurst=5000,标注 pending stress-test validation;$script:LoginQueueMergeGateCleared=$false,登录排队不可用。mysql.yaml 注释「1 核仍是开服硬瓶颈」。设计 #7:范围内玩家一律删缓存、首登冷读。
   - 改法：ps1 Step 6 给回档 zone 传显式的低档 zone rps,作为重开档位,具体数值由 Codex 压测给出。Step 7 观察 MySQL 负载和 Kafka lag 后再逐级调高。没有压测数据之前,不写「冷读洪峰由令牌桶承接」。

