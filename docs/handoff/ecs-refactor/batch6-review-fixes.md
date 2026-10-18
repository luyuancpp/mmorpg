# Codex 任务单：第 6 批：ECS 改造审查修复（2026-10-16，第 1 版）

> **谁执行**：Codex。总规则（生成产物不手改、MSBuild 串行、新增用例修复前红修复后绿、注释中文写「为什么」）、三类初始化口径、与其他会话的协调方式、回执格式，见 [README.md](README.md) §2–§6，本单不重复。
> **来源**：[正式审查报告](review-report-20261015.md)。每条的复核证据见报告附录 B，第一波原始发现见 [审查交接](review-handoff-20261015.md) 附录 A。
> **本版性质**：补丁级，写到函数和语句。每条的「改法」「回归测试」取自对抗式复核者的结论，主审按 §2.3 的冲突裁决做了合并和改写。
> **行号**：取自 2026-10-16 的工作区。gRPC→muduo 迁移、RPC 通道、收件箱、落地恢复等会话仍在改同一批文件，实施前一律按函数名重新定位，不覆盖他人改动。
> **状态**：2026-10-16 交 Codex 实施。4 项核心拍板（§2.1）与 24 项默认决定（§2.2）负责人均已确认。
> **恢复实施**：负责人已确认 T10 值表登记当前 13 个 protobuf 组件，另外 3 个普通 C++ 组件只列类型名；已解除 §8 记录的暂停，继续按原顺序完成第 6 批、验证并同步 Java，不 commit、不 push。
> **修法偏好（负责人）**：删掉多余的东西，不再加一层防护。本单所有条目都按这个方向写；实施中如果觉得需要「再加一个守卫」，先停下，在回执里写明理由。

## 1. 目标

收口审查确认的 1 条 high、20 条 medium 和全部 low，合并重复后共 69 个任务，外加 T10 调试工具和 T11 规则文档收口。

主线是前 5 批的通病：本该删掉的事件、组件和死代码被保留下来，再写防护兜住。具体有四类：

- dispatcher 承担必需步骤，或者零监听，再为不存在的监听者写重入复核；
- 为生产走不到的分支写回滚、半填和兜底；
- 失去最后一个调用方的 API、宏、组件仍然保留，有的还加了守卫和死亡测试；
- 新增用例对基线同样全绿，证明不了修复。

同时修掉复核确认的可达缺陷：W1-40 creators 准入、W2-b1_runtime-01 对尸体施法中止整次施法、W1-33 批量退出 O(N²)。还要修掉已经必红或无效的测试：W1-05、W1-06、W1-17。

## 2. 实施前必读

### 2.1 核心拍板（负责人 2026-10-16 已全部决定，均按审查推荐）

| 拍板项 | 涉及任务 | 审查推荐 | 负责人决定（2026-10-16） |
|---|---|---|---|
| 1. 私有场景 creators 准入的位置与口径（两版统一） | T4.1（W1-40） | A：撤掉 C++ 末端准入，回到两版同基线（creators 不做准入）；Go、Java 不改代码，只改 PARITY | **A**，按 T4.1 实施 |
| 2. 拒存退出的处置 | T2.1（W1-01） | B：退出流程拿到 kRejected 直接调 DestroyDeposedPlayer，不存盘丢弃，不写 A1′、交接标记，不发改派 | **B**，按 T2.1 实施，规则文档第 61 行随 T11 改 |
| 3. ActorNetIds 的客户端三项确认（F080） | T7.10（W1-31） | A：确认客户端在换场景、重连、换节点时清空 actor 表，号源维持现状，只补文档 | **A**：负责人确认客户端清空 actor 表；号源不动，按 T7.10 只改文档 |
| 4. C004 之后是否删除 proto `PlayerItemBlockComp` | T1.10（W1-11） | A：整份删除 `item_comp.proto`，Java 同批删除 | **A**，按 T1.10 删除 proto 并改文档 |

### 2.2 本单默认决定（负责人 2026-10-16 确认全部维持）

负责人已确认下表 24 项全部按默认决定实施，本单于同日交 Codex。「否决后的退路」一列只作记录；实施中如发现某项默认决定行不通，停下来在回执里说明，不要自行改走退路。

| 任务 | 默认决定 | 否决后的退路 |
|---|---|---|
| T1.6（W1-14） | 内部不变量失败仍返回 kInvalidTableData，只补专门日志与 Debug 断言 | 在 Tip.xlsx 新增 kBagInternalError，两版同批 |
| T1.8（W1-10） | 删除战斗消耗与强化、镶嵌之后的 MergeAndCompact(kMergeOnly) | 保留调用，只改注释 |
| T2.2（W1-02） | 删除 BeginTravelHandoff 写标记前的整份 marshal 预检 | 保留，只改变量名 |
| T3.1（W1-23） | 删除 PlayerLoginEvent | 保留为唯一的可选通知，写明监听者契约 |
| T3.4（W1-33、W1-37） | 删除整个 Pin/Unpin API 和远端清扫 | 保留 Pin，加 AoiPinnedByComp 反向索引（两种做法都要删掉全量扫描） |
| T3.5（W2-cross_sem-02） | ConditionEvent 不再经 dispatcher，不加深度计数 | 升级条件保留 dispatcher 通知 |
| T3.9（W1-38） | DestroyDeposedPlayer 的两个 remove 保留，只改注释 | 一并删除 |
| T4.2（W2-cross_sem-01） | 删除 OnSceneCreated / OnSceneDestroyed，入口直接计数（W1-36 作废） | 保留事件，按 W1-36 写监听者契约并删回滚 |
| T4.4（W2-fit-02） | 场景创建、销毁收进 scene 库的单一函数；不建 scene_id 索引；两入口的 config_id 校验暂不统一 | 只做 T4.3 |
| T5.1（W1-16、W2-cross_sem-03） | Kafka 入口改为节点本地直接调用生成的 handler，删除 Register / UnRegister / RegisterNodeEvents | 只在设计文档记录风险 |
| T5.3（W1-18） | 整步删除 local_event_cleanup | 只删 addIngressOption 那一段 |
| T6.1（W2-b1_runtime-01） | 对尸体施法恢复改造前语义：零伤害，效果和后摇照常 | 其他语义属玩法议题，另行决定 |
| T6.3（W2-b1_runtime-02） | 无相位技能照旧受理，只启动冷却、不建上下文 | 在 ReleaseSkill 直接拒绝（需要新 tip，两版同批） |
| T6.7（W2-fit-04） | Buff 改存 uint64 caster，本批做 | 挂起 |
| T7.1（W2-b1_runtime-05） | 整套删除 MoveSpeedComp | 退回基线形态，只在 UpdateMoveSpeed 按需写 |
| T7.6（W2-cross_sem-10） | 保留 state_persistent 字段，已加载玩家恒为 true | 字段改 reserved（客户端可见，两版同批） |
| T7.7（W2-b1_runtime-07） | 保留旧格式兼容，模板改成先并后解 | 删除读侧兼容，另立项 |
| T7.8（W1-24） | 66 号消息现在跟齐 Java | 继续挂在 PARITY「mmorpg 待做」 |
| T8.3（W2-b2-03） | 删除 SameInstanceOnEveryThread 与 atomic | 用例保留并改名 NotThreadLocalStorage，atomic 照样删除 |
| T8.4（W2-b2-04） | 删除 4 条对基线也全绿的用例 | 保留其中两条并改名 |
| T8.8（W2-b2-08） | 删除 NodeInfoListComp，Java 同步副本同批删除 | 暂不处理 |
| T8.9（W2-fit-07） | 等迁移会话提交相关文件后再做 | 并入迁移会话收尾 |
| T8.12（W1-29） | 保留用例，标注为类型契约固定用例 | 删除 |
| T9.1（W2-b5_types-01） | 只收口读取函数，不统一 0 与 kInvalidGuid 两种哨兵 | 统一哨兵，另立项 |

本单明确不做：S1 ECS 用法棘轮脚本（W2-fit-05 拍板 A）；NodeDirectory 节点目录（W2-fit-06，见报告 §4.2）。

### 2.3 修法冲突的裁决

复核者之间没有结论分歧。下面是不同条目的修法互相冲突时主审的取舍：

1. **W1-36 与 W2-cross_sem-01**：W1-36 主张保留 OnSceneCreated / OnSceneDestroyed，用监听者契约收口；W2-cross_sem-01 主张删除事件，入口直接计数。取后者：Agones 计数是必需记账；事件删掉后，通知后复核与回滚也一并消失。W1-36 不单独实施。
2. **W1-22 与 W2-cross_sem-02**：W1-22 原修法保留 `ConditionEvent(kConditionLevelUp)` 作为一对多通知，但 ConditionEvent 唯一的监听者是必需的任务推进。取 W2-cross_sem-02，直接调用 PlayerMissionSystem::OnCondition。scene_local_events.cpp 按 W1-23 → W2-cross_sem-02 → W1-22 的顺序串行改。
3. **W2-fit-01 与 W2-cross_sem-02**：前者要求按 overview 加深度上限 32；后者论证深度计数经过 Accept 会归零，而 CheckAccept 已经保证每个任务对同一玩家至多完成一次。取后者，不加深度计数。
4. **W1-22 与 W2-b1_runtime-06**：W1-22 删掉 Buff 的事件注入和重入防御之后，删除回调链里就没有路径能回到 AddOrUpdateBuff 了，BuffPendingRemoveBuffs 连 removalPending 字段都不再需要。T6.6 先用 grep 核对，再整套删除；只有找到可达的重入路径时，才按 removalPending 的写法做。
5. **W1-23 与 W1-40**：两条都改 cross_zone_test 的 SceneEntryPermissionTest。先做 T3.1，删掉依赖人造通知的用例；T4.1 只改剩下的用例。
6. **W1-24 与 W2-b5_types-05**：两条改同一个 attribute_sync 模板，两处都改完后一次重生成。
7. **W1-23 与 W2-b1_runtime-09**：W1-23 第 (5) 条把 4137 改成判 InitializeOnCreate 的返回值，W2-b1_runtime-09 再改成无条件调用、返回 void。按 T3.1 → T3.2 顺序做，最终形态以 T3.2 为准。

### 2.4 与其他会话的协调

- 每个文件动手前执行 `git status --porcelain -- <文件>` 和 `git log -3 --oneline -- <文件>`。有他人未提交改动就在其基础上改，不回退、不覆盖。
- 已知在途：
  - gRPC→muduo 迁移会话：node_utils.cpp、rpc_client.h、node.cpp、node_connector.cpp、registration_manager.cpp、client_message_processor.cpp、gRPC 模板与生成树、grpc_client_lifecycle_test.cpp。T8.3、T8.9 和所有重生成步骤受影响。
  - RPC 通道会话：player_service_interface.h（批 7 退役 InvokePlayerService）、scene_handler.cpp、gate_service_handler.cpp。
  - 收件箱会话：gm_sanction_inbox_executor.cpp、player_inbox_*。
  - 存盘管线、落地恢复会话：player_lifecycle.cpp、asset_op_system.cpp、db_task_dispatch。
- 重生成前确认迁移会话的模板已经定型。重生成后只核对本单相关产物的差异，其他差异属对方的预期改动，不要回退。

## 3. 覆盖范围

| 任务 | 编号 | 严重度 | 一句话 |
|---|---|---|---|
| T1.1 | W1-05 | medium | bag_selection_order_test 有 5 例没跟上「扣空即释放 / 还原跳过零堆」的新语义，当前必红，其… |
| T1.2 | W1-06 | medium | VectorBatchNeverEvictsWhenSegmentIsUnavailable 的 ② ③ 仍按旧闸门「号… |
| T1.3 | W1-15 | low | BagJavaParityTest::TearDown 把 item 号段恢复成 bag_instance_data 的… |
| T1.4 | W1-07 | low | FlatLayout 是唯一非 final 的具体布局，今后它的派生类漏写 Clone 会在首次入包后被切片；目前所有派… |
| T1.5 | W1-13 | low | 旧档零章补盖依赖调用方「先对全包 Raise 再逐条 Insert」两步协议：bag_marshal 写了两遍，Bag … |
| T1.6 | W1-14 | low | AddItems 有五处内部不变量失败复用表数据错误码 kInvalidTableData，其中三处没有专门日志；同文件… |
| T1.7 | W1-09 | low | 实例层和桥层的注释仍写着「空堆保留占格、等后续并堆回填 / 由整理统一回收」，与「扣空当场两层成对释放、加载跳过零堆」的… |
| T1.8 | W1-10 | low | 调用方仍按旧口径写：「入包失败可能已改包」「扣空留僵尸堆」。beforeCounts 补差、items 部分发放分支、m… |
| T1.9 | W1-12 | medium | batch3 §4.7 任务单与挑战意见要求的背包设计文档更新大半没落实：没有 §14 规则表，§8「全部已知差异」没登… |
| T1.10 | W1-11 | low | C004 只删了 C++ 的 PlayerItemBlockList，proto 的 PlayerItemBlockCo… |
| T2.1 | W1-01 | low | 退出流程遇到拒存一律保留实体:拒存原因全是永久的,保留永远存不了盘,只会让退出收不了尾、重连复用僵尸并扩大丢失量;但当前… |
| T2.2 | W1-02 | low | 三处交接预检各构造一份整份 PlayerAllData 后丢弃。StartTravelHandoff 的预检拒绝没有调用… |
| T2.3 | W1-03 | low | 结算 RequestSettlementPersist 中 PersistFn 返回 false 的注释列了一个在这里走… |
| T2.4 | W1-04 | low | SavePlayerToRedisImpl 头注释仍写「只有三个调用方」传 allowSkipWhenPersisted… |
| T3.1 | W1-23，合并 W1-32、W2-fit-05 | medium | 生产中零监听或只有空监听的 6 个事件仍在 trigger，还配了通知后重入复核和人造重入测试；复核分支一旦走到会留下半… |
| T3.2 | W2-b1_runtime-09，合并 W2-cross_sem-09 | low | InitializeOnCreate 只在新建的玩家实体上调用一次，却写成了可重入的契约：投影已有来源、保留脏位、返回走… |
| T3.3 | W1-34 | medium | 玩家离场序列（AOI 清理→摘 SceneEntityComp 与 Hex）在 HandleEnterScene 和 D… |
| T3.4 | W1-33，合并 W1-37 | medium | 每次离场都全量扫描全节点 AoiListComp，疏散、排空、停机这几条批量退出路径因此变成 O(N²) 的主循环同步阻… |
| T3.5 | W2-cross_sem-02，合并 W2-fit-01 | medium | 任务链的自动发奖、自动接后续、“完成任务 X”推进和战斗击杀进度都是必需步骤，现在仍经 dispatcher 转给 no… |
| T3.6 | W1-22 | medium | 沉默状态和 GM 调级后的重算都是只有一个消费者的必需步骤，现在仍经 dispatcher 转发：不装订阅的宿主里会静默… |
| T3.7 | W1-27 | low | OnNodeRemoveEvent::sceneNodeType 是无人写、无人读的死字段；SkillExecutedE… |
| T3.8 | W1-30 | low | 事件结构体化后仍有注释描述旧机制：mission.cpp 的“截成 uint32”注释和多余别名、CreateScene… |
| T3.9 | W1-38 | low | 第 5 批后有 4 处注释或文档与实现不符(skill.cpp、player_lifecycle.cpp、anomaly… |
| T4.1 | W1-40 | high | Codex 把废弃 scene_test 里的 creators 断言做成了生产准入，而且只在 C++ 节点末端生效。非… |
| T4.2 | W2-cross_sem-01，合并 W2-fit-03、W1-36 | medium | Agones 单元计数这个必需步骤仍经 dispatcher 一对一转发(唯一监听者只调 OnUnitCreated/O… |
| T4.3 | W2-b5_types-02，合并 W2-cross_sem-08 | low | 已有 PlayerSceneSystem::FindSceneById,但两个入口仍各手写 2 份 scene_id 全… |
| T4.4 | W2-fit-02 | low | 场景创建和销毁的业务逻辑在 muduo 与 gRPC 两个入口各写一份（去重、字段组装、排空、销毁，第 1 批又各加一份… |
| T5.1 | W1-16，合并 W2-cross_sem-03 | medium | Kafka 入口的 13 个必需命令（4 个 scene、9 个 gate，外加 gate 信封回退）仍经 tlsEcs… |
| T5.2 | W1-17 | medium | 白名单单测把上界写死为 49，没有覆盖已进入白名单的 49 号 PlayerInboxNotifyEvent，也拦不住以… |
| T5.3 | W1-18 | medium | 一次性的事件迁移逻辑被固化成生成器每轮必跑的步骤：用正则改写三份手工维护的构建清单，并以 attribute_sync_… |
| T5.4 | W1-19 | low | 已删除的 DispatchProtoEvent 和 SceneEventHandler 仍被注释与规范引用，任务单 4.… |
| T5.5 | W1-20 | low | 两份节点模板和模板 README 没有跟上新 hook 契约：只声明 KafkaCommandType 而缺 Dispa… |
| T5.6 | W1-21 | low | 控制面 topic 文档里的 scene 白名单数量和生成状态都已过时；事故文档 F001/F002 没有按任务单 4.… |
| T6.1 | W2-b1_runtime-01 | medium | 落点时目标已死,被 CalculateSkillDamage 的 false 当成数据缺失,整次施法被中止。对尸体施法、… |
| T6.2 | W2-b1_runtime-04 | medium | 一次普通技能施放要做 6+N 次 CanContinueSkillCast,外加 2+N 次 HasRequiredTa… |
| T6.3 | W2-b1_runtime-02 | low | 没有前摇相位的技能类型(开关/激活/普攻/被动)施放后,SkillContext 永不回收。目前没有玩家拥有这类技能,属… |
| T6.4 | W2-b1_runtime-03 | low | cooldown_id=0(无冷却)的技能也会创建冷却表并写入 0 号条目,之后每次施放都会在 CheckCooldow… |
| T6.5 | W1-26 | low | 同一个不可信的 target_id 在一次 ReleaseSkill 里被反查最多 6 次；uint64 版的 Reso… |
| T6.6 | W2-b1_runtime-06 | medium | BuffPendingRemoveBuffs 实际上只是 RemoveBuff(set) 一次同步调用内的「正在删除」标… |
| T6.7 | W2-fit-04 | low | 目标身上的 BuffEntry 仍存着施法者 SkillContextCompMap 里同一个 shared_ptr，但… |
| T6.8 | W2-b5_types-04 | low | 四个具名容器组件的 explicit 基类转换构造在全仓没有调用方,X() = default 只是因为写了这两个构造才… |
| T7.1 | W2-b1_runtime-05 | medium | MoveSpeedComp 在生产代码中没有任何读者，第 1/5 批却把它定成每个 actor 的必需派生组件，配齐了创… |
| T7.2 | W2-b1_runtime-08，合并 W2-cross_sem-04 | low | 入场落位 EnsureValidEnterLocation 对持久组件 Transform、默认技能 RegisterP… |
| T7.3 | W2-b1_runtime-10 | low | StoreBattleFreezeForWrite 只是一行透传的 helper；它把三处先确认不存在、再 emplac… |
| T7.4 | W2-cross_sem-05 | low | BuildBattleSnapshot 在 all_of 前置校验之后，仍保留 7 处“缺组件就给默认值或跳过”的判空分… |
| T7.5 | W2-b1_runtime-11 | low | PetSystem::BuildBattleSnapshot 的注释说缺组件时玩家照常进战斗,实际唯一调用方已先 fai… |
| T7.6 | W2-cross_sem-10 | low | 任务列表读路径在持久组件 MissionsContainerComp 缺失时仍返回 kSuccess，并下发整份“全部未… |
| T7.7 | W2-b1_runtime-07 | low | 旧格式快照(只有顶层 bag_data)的背包在派生属性算完后才恢复,DerivedAttributesComp 漏算装… |
| T7.8 | W1-24 | low | 66 号 ActorBaseAttributesS2C 只在战斗状态那一路带 entity_id，移动和速度增量的 en… |
| T7.9 | W1-25 | low | ActorNetIdComp 定义在公共头里、可以随意写，导致 Of、Resolve、Assign 各自再做一遍反查自检… |
| T7.10 | W1-31 | low | actor net id 的唯一性范围（同时在线的节点之间唯一，进程重启后同一 node_id 会重新发同样的号）与 J… |
| T8.1 | W2-b2-01，合并 W2-fit-08 | low | NodeContextManager 的每节点类型「全局实体」三件套随 etcd 迁移失去最后一个调用方，仍连同越界 F… |
| T8.2 | W2-b2-02，合并 W2-cross_sem-06 | medium | 过渡用的空 TRANSFER_ERROR_MESSAGE 宏已无调用方，注释还写着「生成头仍有调用」；任务单 C2 的前… |
| T8.3 | W2-b2-03 | medium | NodeInfo 只读门面的 atomic 发布和「返回旧值」的 exchange 只为测试存在；SameInstanc… |
| T8.4 | W2-b2-04 | low | ecs_context_test 5 条会话索引用例中有 4 条测的是 std::unordered_map、threa… |
| T8.5 | W2-b2-05 | low | 玩家接口契约写着「RpcDoneCallback 延后完成同样遵守」，但玩家 handler 拿不到 done；engi… |
| T8.6 | W2-b2-06 | low | C4.3 指名的 player-features-ui.md:22 没改，仍教「写 TLS tip 由 TRANSFER… |
| T8.7 | W2-b2-07 | low | C3 没按挑战意见收尾：用例名和注释仍指向已删除的 GlobalEntity/globalRegistry；末尾换上的 … |
| T8.8 | W2-b2-08 | low | NodeInfoListComp 唯一使用方是已删除的 ServiceNodeList 单例，现在两个仓库都没有任何手写… |
| T8.9 | W2-fit-07 | medium | 出站连接 UP 后的节点握手（必需步骤，生产唯一监听者是 Node）仍由 RpcClient 经 tlsEcs.disp… |
| T8.10 | W1-28 | low | gate 节点句柄类型化后残留旧整数逻辑：GetEffectiveNodeEntity 成了多余包装，ResolveSe… |
| T8.11 | W2-b5_types-06，合并 W2-fit-09 | low | gate 把 scene 节点 registry 叫 sceneRegistry、节点句柄叫 sceneEntity,与… |
| T8.12 | W1-29 | low | “同 index 不同 version 判为换节点”用例是类型契约固定用例，不是修复前会红的回归测试，回执把它称作“回归… |
| T8.13 | W2-fit-06 | low | 节点表仍是「map + 动态属性袋」式 registry：查找靠线性扫描 view，出站/入站角色靠组件组合区分，握手定… |
| T9.1 | W2-b5_types-01 | low | 读 PlayerGuidComp 的私有 GuidForLog 在 7 个文件里各有一份,还有 4 处守护段内联三元表达… |
| T9.2 | W2-b5_types-03 | low | grid.cpp 新增的注释声称 try_get 前必须先 valid(),与 EnTT 语义和组件访问规则表矛盾;另有… |
| T9.3 | W2-b5_types-05 | low | 属性同步模板已改成消费后整体 remove DirtyMaskComp,循环里逐字段 dirtyMask.reset 成… |
| T9.4 | W2-cross_sem-07 | low | robot 冒烟场景的注释和失败文案仍按旧机制写（“拒绝码必须写 TLS tip”“TRANSFER 宏总会物化 err… |
| T9.5 | W1-39 | low | 3 份设计文档仍描述已删除的 ScenePlayers 反向集合 |

另有 T10（负责人点名要的「打印实体全部组件」调试工具）和 T11（W1-35 以及多条任务的文档部分）。不在本单的条目见 §5。

## 4. 实施步骤

### T1 背包：测试跟上新语义、注释与文档收口（独立，可最先做）

只动 cpp/libs/modules/bag/**、bag 测试、背包相关调用方注释与设计文档。与其他组没有文件交叉。

#### T1.1 W1-05（medium）bag_selection_order_test 有 5 例没跟上「扣空即释放 / 还原跳过零堆」的新语义，当前必红，其中 2 例已构造不出它要测的空堆

- 位置：cpp/tests/bag_test/bag_selection_order_test.cpp:149,cpp/tests/bag_test/bag_selection_order_test.cpp:214,cpp/tests/bag_test/bag_selection_order_test.cpp:218-230,cpp/tests/bag_test/bag_selection_order_test.cpp:249-264,cpp/libs/modules/bag/bag_system.cpp:352-355,cpp/libs/modules/bag/bag_system.cpp:1368-1371

**改法**

只改 cpp/tests/bag_test/bag_selection_order_test.cpp：

1) ExpectLowestSlotsDrained：把第 149 行 `EXPECT_EQ(0u, StackSize(bag, kGuidB));` 替换为 `EXPECT_EQ(nullptr, bag.GetItemCompByGuid(kGuidB)); EXPECT_EQ(kInvalidSlot, bag.GetItemPosByGuid(kGuidB)); EXPECT_EQ(2u, bag.OccupiedGridCount());`，原第 151 行的 IsLayerConsistent 保留。

2) DrainWithoutReceiptUsesTheSameSlotOrder：第 214 行做同样替换，末尾加 `EXPECT_TRUE(bag.IsLayerConsistent());`。

3) 整例删除 DrainSkipsEmptyStackWithoutReceipt（第 218-230 行）。Bag 层造不出零堆；「零堆不进回执」已由 ItemStoreTest.DrainRankIsReadOncePerNonemptyCandidate 覆盖，「还原丢弃零堆」已由 BagJavaParityTest.RestoreDropsZeroStacksWithoutOccupyingSlotsOrConsumingStamps 覆盖。

4) 删除 Bag 层的 MergeNeverPicksAnEmptyStackAsSurvivor（第 249-264 行），在 ItemStoreTest 区新增同名用例：`TEST(ItemStoreTest, MergeNeverPicksAnEmptyStackAsSurvivor) { ItemTableManager::Instance().Load(); ItemStore store; ASSERT_NE(nullptr, store.Insert(MakeInstance(kGuidA, 0, 1))); ASSERT_NE(nullptr, store.Insert(MakeInstance(kGuidB, 3, 2))); ASSERT_NE(nullptr, store.Insert(MakeInstance(kGuidC, 4, 3))); store.MergePartialStacks(); EXPECT_EQ(7u, store.Find(kGuidB)->size()); EXPECT_EQ(0u, store.Find(kGuidA)->size()); EXPECT_EQ(0u, store.Find(kGuidC)->size()); EXPECT_EQ((GuidVector{kGuidA, kGuidC}), store.CollectEmptyInstances()); }`。MergePartialStacks 要查物品表，所以必须先调用 Load()。

**回归测试**

工程 cpp/tests/bag_test（bag_test.exe），由 Codex 执行：`bag_test.exe --gtest_filter=BagSelectionOrderTest.*:ItemStoreTest.*`。修复前：ClampedDrainTakesLowestSlotFirst、ExactDrainTakesLowestSlotFirst、DrainWithoutReceiptUsesTheSameSlotOrder、DrainSkipsEmptyStackWithoutReceipt、MergeNeverPicksAnEmptyStackAsSurvivor 共 5 例失败（缺陷本身）。修复后全绿。另做两项变异验证，证明修正后的用例确实能守住规则：① 临时删掉 bag_system.cpp:352-355 的 DestroyItem，三个扣减用例在 `EXPECT_EQ(nullptr, bag.GetItemCompByGuid(kGuidB))` 处变红；② 临时去掉 ItemStore::IsMergeCandidate 的 `item.size() > 0`，新的 ItemStoreTest.MergeNeverPicksAnEmptyStackAsSurvivor 变红（A 成为幸存者得到 7，B 变为 0）。验证完恢复。

#### T1.2 W1-06（medium）VectorBatchNeverEvictsWhenSegmentIsUnavailable 的 ② ③ 仍按旧闸门「号源不可用不许腾位」断言，与「先规划到不动点、按最终规划铸号」的新契约冲突，当前必红

- 位置：cpp/tests/bag_test/bag_test.cpp:4063-4098,cpp/tests/bag_test/bag_test.cpp:4033,cpp/tests/bag_test/bag_test.cpp:1957,cpp/libs/modules/bag/bag_system.cpp:668-692

**改法**

改 cpp/tests/bag_test/bag_test.cpp 的 VectorBatchNeverEvictsWhenSegmentIsUnavailable：

1) 用例改名为 `VectorBatchMintsOnlyWhatTheFinalPlanNeedsWhenSegmentIsUnavailable`，第 4063 行注释改为「vector 重载三种形状：需铸号则拒且不腾位 / 全预设、需腾位则按 FIFO 淘汰且不铸号 / 全预设且放得下则不淘汰」。

2) 删除第 4081-4082 行「不许靠腾位解决」的旧理由，② 改为：`evicted.clear(); EXPECT_EQ(kSuccess, bag.AddItems(std::vector<InitItemParam>{preassigned}, &evicted)); ASSERT_EQ(1u, evicted.size()); EXPECT_EQ(940001u, evicted.front().guid); EXPECT_EQ(nullptr, bag.GetItemCompByGuid(940001)); EXPECT_NE(nullptr, bag.GetItemCompByGuid(940003)); EXPECT_EQ(2u, bag.OccupiedGridCount()); EXPECT_TRUE(bag.IsLayerConsistent());`，并注释「最终规划不需要铸号；淘汰后才要铸号的情形见 BagJavaParityTest.PostEvictionMintFailure…」。

3) ③ 改用新的预设 guid：`InitItemParam another = MakeItem(kNonStack1, 1); another.itemPBComp.set_item_id(940004); bag.SetCapacityForRestore(3); evicted.clear(); EXPECT_EQ(kSuccess, bag.AddItems(std::vector<InitItemParam>{another}, &evicted)); EXPECT_TRUE(evicted.empty()); EXPECT_EQ(3u, bag.OccupiedGridCount()); EXPECT_NE(nullptr, bag.GetItemCompByGuid(940004));`。

4) 第 4033 行注释改为「批量路径同样：最终规划与铸号都在任何修改之前」，第 1957 行把「(ReserveBatch)」改为「(AddItems 批量入口)」。生产代码不改。

**回归测试**

工程 cpp/tests/bag_test，由 Codex 执行：`bag_test.exe --gtest_filter=BagSegmentNotReadyTest.*`。必须整组跑，本用例第 4066 行依赖同组前面的用例把号段置为不可用。修复前：VectorBatchNeverEvictsWhenSegmentIsUnavailable 在第 4085/4087/4089/4093/4094/4095 行失败。修复后改名的用例为绿。变异验证：在 Bag::AddItems 的 PlanEviction 之后临时加回旧闸门（`if (!victims.empty() && !ItemStore::CanMintGuid()) return kBagAddItemInvalidParam;`），新的 ② 变红；把铸号数临时改为按淘汰前计划计算，BagJavaParityTest.PostEvictionMintFailurePreservesOldItemsLayoutAndReceipts 变红。验证完恢复。

#### T1.3 W1-15（low）BagJavaParityTest::TearDown 把 item 号段恢复成 bag_instance_data 的 [900000,1000000) 而不是基线 [1,2^54)，默认顺序下被后续夹具掩盖，乱序或换序运行时会与硬编码 guid 撞号

- 原严重度 low，复核结论「降级」
- 位置：cpp/tests/bag_test/bag_java_parity_test.cpp:124

**改法**

把 cpp/tests/bag_test/bag_java_parity_test.cpp 第 124 行改为 `EXPECT_TRUE(ArmParitySegment(1, uint64_t{1} << 54));`。只改这一行即可，不必为此新建共享头文件。7 份 Arm\*Segment 和基线常量重复，日后整理时可再收拢，不是本条的必要修法。

**回归测试**

不需要独立回归用例。理由：这是夹具恢复状态的卫生问题。默认顺序下 PlayerEquipTest 紧随其后会重装号段，过滤运行又保持注册顺序，任何确定性用例在修复前都不会红；而专门写一个「检查上一个夹具恢复了什么」的用例，本身就依赖执行顺序，违反测试纪律。替代验证（静态）：`git grep -n "TearDown" -A8 -- cpp/tests/bag_test | grep -E "Arm[A-Za-z]*Segment\("`，确认每个 item 号段的 TearDown 参数都是 `(1, uint64_t{1} << 54)`。

#### T1.4 W1-07（low）FlatLayout 是唯一非 final 的具体布局，今后它的派生类漏写 Clone 会在首次入包后被切片；目前所有派生类都已覆盖，属于潜在的维护陷阱

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/modules/bag/container_layout.h:144,cpp/libs/modules/bag/container_layout.h:150-153,cpp/libs/modules/bag/bag_system.cpp:753,cpp/libs/modules/bag/bag_system.cpp:772-773

**改法**

AddItems 不加任何分支。只改切片唯一可能发生的地方：在 FlatLayout::Clone（container_layout.h:150-153）函数体第一行加 `assert(typeid(*this) == typeid(FlatLayout) && "FlatLayout 的派生类必须自己覆盖 Clone");`（引入 \<typeinfo>、\<cassert>），然后把 container_layout.h:60-61 的注释改为「FlatLayout::Clone 会在 Debug 下断言动态类型」。FixedSlotLayout 等已覆盖 Clone 的子类不会调到父类版本，不受影响。

**回归测试**

bag_test 工程，在 bag_java_parity_test.cpp 新增 TEST(BagLayoutCloneTest, FlatLayoutSubclassWithoutCloneOverrideDiesInDebug)。在用例内定义 `class ForgetfulLayout final : public FlatLayout { public: using FlatLayout::FlatLayout; bool HasSlotSemantics() const override { return true; } };`，不覆盖 Clone；断言 `EXPECT_DEBUG_DEATH({ auto c = ForgetfulLayout(3).Clone(); (void)c; }, "Clone");`。修复前 Clone 静默返回一个 FlatLayout，进程不会死，Debug 下用例红；修复后断言触发，用例绿。Release 下 EXPECT_DEBUG_DEATH 只执行语句、不检查死亡，所以必须用 Debug 配置跑。

#### T1.5 W1-13（low）旧档零章补盖依赖调用方「先对全包 Raise 再逐条 Insert」两步协议：bag_marshal 写了两遍，Bag 只暴露半个不变量

- 位置：cpp/libs/services/scene/player/system/bag_marshal.cpp:204-211,cpp/libs/services/scene/player/system/bag_marshal.cpp:249-255,cpp/libs/modules/bag/bag_system.h:505-512,cpp/libs/modules/bag/bag_system.h:526-528,cpp/libs/modules/bag/bag_system.cpp:1360-1363,cpp/libs/modules/bag/item_store.cpp:51-58

**改法**

1. 在 Bag 上新增一个批量还原入口，把两遍扫描收进去：`void Bag::RestoreItems(std::vector<std::pair<ItemComp, uint32_t>> items)`。函数体先 `for (const auto &[item, pos] : items) if (item.size() > 0) store_.RaiseAcquireSeq(item.acquire_seq());`，再 `for (auto &[item, pos] : items) InsertItemForRestore(std::move(item), pos);`。

2. 删除 Bag::RaiseAcquireSeqForRestore 的声明（bag_system.h:527-528）和实现（bag_system.cpp:1360-1363）。

3. bag_marshal::Unmarshal 中，固定包合并成一个循环：在原 216-231 行循环里保留越界 bag_type 的 LOG_WARN，其余条目 push 进 `std::array<std::vector<std::pair<ItemComp, uint32_t>>, kBagTypeCount>`，然后对每个包调一次 `bags.bags[t].RestoreItems(std::move(perBag[t]))`。删掉 204-211 的 Raise 循环。

4. 动态包删掉 249-255 的 Raise 循环，改为把条目收集进 vector 后调一次 bag.RestoreItems。

5. ItemComp 重载的 InsertItemForRestore 改为 private，生产代码只能走 RestoreItems。位置参数重载保留给测试直调，注释写明「逐条调用时零章按调用顺序盖在当前水位，混有非零章的批量还原必须走 RestoreItems」。

**回归测试**

bag_test 工程，bag_java_parity_test.cpp 新增 TEST_F(BagJavaParityTest, BagLevelRestoreStampsLegacyZeroAfterLaterExistingStamp)。步骤：Bag bag; bag.SetProfile(BagProfile::Flat(3)); 构造 A(item_id=kOldStack, config 10, size 1, acquire_seq 0, pos 0) 和 B(item_id=kOldOther, config 10, size 1, acquire_seq 70, pos 1)，按 A 在前、B 在后的顺序交给 Bag 的公开还原入口（修复后为 bag.RestoreItems({{A,0},{B,1}})）。断言：A 的 acquire_seq==71，B==70；随后 AddItem(ParityGrant(3,1)) 新实例的序号为 72。修复前 Bag 公开面只有逐条 InsertItemForRestore，按同样顺序调用得到 A==1，与断言不符，用例红。原有 LegacyZeroSequencesFollowEveryExistingStampInTheirOwnBag 继续守 marshal 路径，保持绿。

#### T1.6 W1-14（low）AddItems 有五处内部不变量失败复用表数据错误码 kInvalidTableData，其中三处没有专门日志；同文件多处函数内 tab 与空格缩进混排

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/modules/bag/bag_system.cpp:676-679,cpp/libs/modules/bag/bag_system.cpp:699-703,cpp/libs/modules/bag/bag_system.cpp:725-730,cpp/libs/modules/bag/bag_system.cpp:747-750,cpp/libs/modules/bag/bag_system.cpp:754-759,cpp/libs/modules/bag/bag_system.cpp:401-404,cpp/libs/modules/bag/bag_system.cpp:903-923,cpp/libs/modules/bag/bag_system.cpp:1367-1371
- 本单口径：默认选「拍板」中的 A：返回码保持 kInvalidTableData，只补专门日志与 Debug 断言，不动 Tip.xlsx。

**改法**

1. 保留这五处检查。给 678、702、749 补上与 727 同格式的专门 LOG_ERROR，写明是哪条不变量（例如 "plan produced fewer instances than preassigned guids"、"eviction policy returned guid not in store"、"plan/commit instance count mismatch"）及玩家 guid。五处都加 `assert(false && "<同一句>")`，让 Debug 下的用例能直接暴露。

2. 返回码在负责人拍板前保持 kInvalidTableData。若决定新增内部错误码，就在 Tip.xlsx 的 bag 组加一行 kBagInternalError（fault=1），跑导表器后把这五处换成新码，Java 版同批改。

3. 把 RemoveItemByPos 401-404、AddItem/AddItems 590-784、PlanEviction 899-923 和 1024、InsertItemForRestore 1360-1371 的缩进统一为文件既有的 tab 风格，单独提交，不夹带逻辑改动。

4. 可选：把 634-666 的校验与汇总循环抽成私有函数 `ValidateAndAggregate(itemsToAdd, stackUnits, separateInstances, preassigned)`，AddItems 其余部分不拆。

**回归测试**

默认方案只改日志、断言和缩进，行为不变，不需要新增红测试；用现有 bag_test 全量（BagJavaParityTest、BagSelectionOrderTest、BagTest 等）兜底。若负责人选择新增 kBagInternalError，则在 bag_test 新增 TEST(BagAddItemsInternalErrorTest, ForeignVictimFromPolicyReportsInternalError)：自定义 `class ForeignVictimPolicy final : public IEvictionPolicy { GuidVector SelectVictims(std::size_t, const ItemStore&, const IContainerLayout&) const override { return {999999}; } };`；Bag 设为 Flat(1) 并 InsertItemForRestore 一件不可叠加物品，SetEviction(ForeignVictimPolicy)，再带 evictedOut 调 AddItems(ItemCountMap{{3,1}}, &evicted)。断言返回 kBagInternalError、包内容不变、evicted 为空。修复前返回 kInvalidTableData，用例红。这条用例会进 702 的断言，所以要用 Release 跑或包成 EXPECT_DEBUG_DEATH，二者选一。

<details><summary>复核者给出的拍板选项</summary>

内部不变量失败用什么返回码。选项 A：保持 kInvalidTableData，只补专门日志和 Debug 断言。代价是错误码层面仍与表数据错误混淆，但行为不变，不动客户端契约。选项 B：在 Tip.xlsx 的 bag 组新增 kBagInternalError（fault=1），C++ 与 Java 同批改。好处是监控和日志能按码区分内部 bug，代价是改动客户端可见契约，并且这五条路径在当前实现下都不可达，收益有限。推荐 A。

</details>

#### T1.7 W1-09（low）实例层和桥层的注释仍写着「空堆保留占格、等后续并堆回填 / 由整理统一回收」，与「扣空当场两层成对释放、加载跳过零堆」的现行实现相反；CanMintGuid(s) 已无生产调用方，注释却仍说铸号型写入必须先调用它

- 位置：cpp/libs/modules/bag/item_store.h:121-123,cpp/libs/modules/bag/item_store.h:145-150,cpp/libs/modules/bag/item_store.h:162-163,cpp/libs/modules/bag/item_store.h:177-183,cpp/libs/modules/bag/item_store.cpp:437-438,cpp/libs/modules/bag/item_store.cpp:469-474,cpp/libs/modules/bag/bag_system.cpp:517-522,cpp/libs/modules/bag/bag_system.h:87,cpp/libs/modules/bag/bag_service.h:212,docs/design/bag-instance-layout-split.md:114

**改法**

按负责人「能删就删」的偏好处理，不新增调用：

1) 删除 ItemStore::CanMintGuid() 与 CanMintGuids(std::size_t)，即 item_store.h:177-183 和 item_store.cpp:469-488。item_store.cpp:437 的注释改为「MintGuid 会被批量入包连续调用」。bag_test.cpp 里 16 处 ItemStore::CanMintGuid()/CanMintGuids(n) 断言改成 tlsGuidSegmentRegistry.Get(GuidKind::kItem).Available() > 0 或 >= n，3742-3744 那组直接断言 Available()==10。docs/design/bag-instance-layout-split.md:114 的「guid 发号」行删去 CanMintGuid。

2) 注释按现状改写：item_store.h:121-123 改为「抽光的堆 size 变 0，实例层不销毁；Bag 必须当场成对 DestroyItem（DrainOneConfig 已这样做）」；147-150 改为「size==0 只会是 MergePartialStacks 刚清空的组员，紧接着由 CollectEmptyInstances 回收；扣到 0 已当场释放，旧档零堆在加载时跳过」；163 删去「并堆刻意允许往空堆回填」（item_store.cpp:166 同改）。bag_system.cpp:517-522 只保留「① 刚清空的可叠加堆」；bag_system.h:87 和 bag_service.h:212 删去「以及 size==0 的僵尸实例」。

**回归测试**

不需要。改动只涉及注释，外加删除一个没有生产调用方的函数，入包、扣除、整理的行为都不变。BagJavaParityTest.ServiceMintFailureReportsUnmutatedAndKeepsBothOldInstances 与 BagSegmentNotReadyTest.\* 继续守护「号段不足时整批零写入」，只需把其中的 CanMintGuid 前置断言换成 Available() 写法。

#### T1.8 W1-10（low）调用方仍按旧口径写：「入包失败可能已改包」「扣空留僵尸堆」。beforeCounts 补差、items 部分发放分支、mutated 出参、扣除后的 ReclaimEmptyStacks/MergeAndCompact 在原子提交和扣空即释放之后都已不可达或不再需要，属于该删没删的防护

- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:794-797,cpp/libs/services/scene/battle/system/player_battle.cpp:818-856,cpp/libs/services/scene/battle/system/player_battle.cpp:1101-1104,cpp/libs/services/scene/player/system/asset_op_system.cpp:503-542,cpp/libs/modules/bag/bag_service.cpp:142-149,cpp/libs/modules/bag/bag_service.cpp:178-181,cpp/libs/modules/bag/bag_service.h:56-63,cpp/libs/services/scene/player/system/player_equip.cpp:440-454,cpp/libs/services/scene/player/system/player_equip.cpp:1271-1275,cpp/libs/services/scene/player/system/player_equip.cpp:1385-1386
- 本单口径：默认选 A：删除战斗消耗与强化/镶嵌之后的 MergeAndCompact(kMergeOnly)。

**改法**

1) player_battle.cpp:818-856 整段替换为：`auto result = BagService::AddItems(player, inventory, gained, TX_ITEM_AWARD, battleId, extra); if (result != kSuccess) { LOG_WARN << "…main bag rejected, whole batch redirected to temporary: … err=" << result; result = BagService::AddItems(player, bags->bags[kTemporary], gained, TX_ITEM_AWARD, battleId, extra); }`。删除 beforeCounts、remainder 和 `result = kSuccess` 分支，注释改为引用 bag_service.h 的「任何拒绝都不修改原包」。

2) player_battle.cpp:794-797 删除注释和 MergeAndCompact(kMergeOnly) 调用（见 owner_decision）；1101-1104 删除 `if (item.size()==0) return;`。

3) 删除 BagService::AddItems(ItemCountMap) 的 `bool *mutated` 参数，包括 bag_service.h:56 的说明行与 63 行的参数、bag_service.cpp:142 的参数、145-149 和 178-181 两处赋值。asset_op_system.cpp:503-542 改为：去掉 `bool mutated` 与 `&mutated` 实参；`if (error != kSuccess) { if (error == kBagItemNotStacked) Answer(RETRY, kAssetBagFull); else if (error == kInvalidParameter) RejectAndRecord(...kAssetBlocked...); else Answer(RETRY, error); return; } anyMutated = true;`，删除 536-542 的部分发放分支（货币段的 partial 逻辑保留）。bag_java_parity_test.cpp:161-167 去掉 mutated 变量和 EXPECT_FALSE(mutated)，返回码和「包未变」的断言保留。docs/design/guild-phase2/04-asset-channel.md:417、485-488、1318-1321 加注「2026-10-15 起 Bag 候选提交，mutated 出参已删，物品失败恒零改动」。

4) player_equip.cpp：删除 ReclaimEmptyStacks(440-449) 以及 1271-1275、1385-1386 两处调用，强化和镶嵌后对应的「整理会让 instance 失效」注释一并删掉；452-453 改为「同 config、不带 equip 段、没到堆叠上限」，去掉「被抽空还没回收的空堆」。RemoveItemsByGuid 的 mutated（bag_service.cpp:391）与本条无关，不动。

**回归测试**

写不出修复前会红的测试：这里删的都是不可达分支或已失去目的的调用，可观察行为不变，属于 §11.4「确实无法先写」的情形。替代验证：(a) 保留 BagJavaParityTest.ServiceMintFailureReportsUnmutatedAndKeepsBothOldInstances（去掉 mutated 断言后仍断言返回 kBagAddItemInvalidParam 且 CaptureParityBag 前后相等），它守护删除所依赖的「失败零改动」契约；(b) 在 cpp/tests/bag_test/player_battle_settlement_test.cpp 补 SettlementMainBagFullSendsWholeDropBatchToTemporary：主包占满、临时格为空，结算掉落 {A:3,B:2}，断言主包不变、临时格 A=3、B=2、流水 2 条；(c) PlayerEquipTest.EnhanceReclaimsTheMaterialStackItEmptied 与镶嵌对应用例在删掉 ReclaimEmptyStacks 后仍须为绿，以证明扣空释放由 DrainOneConfig 独立保证。

<details><summary>复核者给出的拍板选项</summary>

删除战斗消耗后与强化/镶嵌后的 MergeAndCompact(kMergeOnly) 会带来一处行为变化：这几步之后不再顺手把同 config 的零头堆合并，也不再为被合并的实例写退役流水。选项 A（推荐）：删除。原目的「回收零堆」已由扣空即释放保证，入包时 PlanStackIntoExistingStacks 仍会先填零头，零头不会无限增长。选项 B：保留合并，但把注释改成「战后顺手合并零头」，并接受每场战斗多一次全包扫描和退役流水。

</details>

#### T1.9 W1-12（medium）batch3 §4.7 任务单与挑战意见要求的背包设计文档更新大半没落实：没有 §14 规则表，§8「全部已知差异」没登记本批客户端可见的变化，bag-rule-policy-layering.md 一字未改、仍把已删除的 ReserveOrEvict/ReserveForBatchAdd 写成现行形状

- 原严重度 low，复核上调为 medium
- 位置：docs/design/bag-instance-layout-split.md:293-304,docs/design/bag-instance-layout-split.md:271,docs/design/bag-instance-layout-split.md:114,docs/design/bag-instance-layout-split.md:609,docs/design/bag-rule-policy-layering.md:417-437,docs/design/bag-rule-policy-layering.md:449-466,docs/design/bag-rule-policy-layering.md:555,docs/design/bag-rule-policy-layering.md:567,docs/design/bag-rollback-feasibility-analysis.md:121-149

**改法**

1) bag-instance-layout-split.md 在「## 关联」前新增「## 14. 实例层去 entt、选择顺序与原子提交（2026-10-15）」，按 batch3 §4.7 写规则表：实例序 =(acquire_seq, guid)；并堆、合并分组与幸存者、回收回执、淘汰、整理平局都按实例序，空堆不参与合并；扣除按槽位号升序再按实例序（写全来源「Java 仓库 scene-battle-spec D22」，不只写编号）；批量按 config 升序；扣空即两层释放；加载跳过零堆，在全包最大序号之后补旧零章；Bag::AddItems 先算淘汰不动点并铸齐 ID，再在仓库和布局的 Clone 候选上提交，失败零改动、允许烧号，任何成功入包都会使 ItemComp\* 借用失效；写盘固定包按 bag_type/slot、动态包按 bag_id/slot。附上与 Java Bag.java 的对照，其中写盘顺序和老档 seq0 补章列为仍有差异，不写「已对齐」。§8 表追加 7 号行「2026-10-15 第 3 批起灌哪堆 / 扣哪堆 / 留哪堆 / 批量占格 / 整理平局改为显式顺序，客户端可见，详见 §14」，同时把开头的「行为不变」改为「拆分期行为不变；后续有意变化见 #7」。§6 不变量 5 改为「入包一律在候选副本上 plan → mint → commit，失败只丢弃副本」。2) bag-rule-policy-layering.md：在 §6.2(3) 表、§6.3（449-466）、555 行和 562 行前各加一行「2026-10-15 起 ReserveOrEvict/ReserveForBatchAdd/ReserveBatch 已删除，由 Bag::AddItems 候选提交取代；下文为历史」；432 行「悬空的 entity」改为「已销毁的 guid」；567 行补「2026-10-15 起 StackFill 存 guid，失效时 Find 为空并记 LOG_ERROR，不再有 entt UB」。3) 可选：bag-rollback-feasibility-analysis.md 顶部状态说明追加一句「2026-10-15 起 ItemStore 不再使用 entt，§1.4 的 itemRegistry_ 已不存在」。

**回归测试**

不需要。只改设计文档，不影响运行期行为。

#### T1.10 W1-11（low）C004 只删了 C++ 的 PlayerItemBlockList，proto 的 PlayerItemBlockComp（注释还引用不存在的 Bag::IsItemBlocked）和 exploit_loss_prevention / bag-service-srp-refactor / 04-asset-channel 文档仍把单人物品封禁和 blockList 参数写成现状

- 位置：proto/common/component/item_comp.proto:4-11,docs/design/exploit_loss_prevention.md:25,docs/design/exploit_loss_prevention.md:28,docs/design/exploit_loss_prevention.md:136,docs/design/exploit_loss_prevention.md:169,docs/design/exploit_loss_prevention_en.md:28,docs/design/exploit_loss_prevention_zh.md:28,docs/design/bag-service-srp-refactor.md:6-33,docs/design/bag-service-srp-refactor_en.md:6-33,docs/design/bag-service-srp-refactor_zh.md:6-33,docs/design/guild-phase2/04-asset-channel.md:483
- 本单口径：**负责人已拍板（2026-10-16，核心拍板 4）：选 A，整份删除 item_comp.proto，Java 同步副本同批删除并登记 PARITY。** 文档部分与 proto 删除同批做。重生成用 no-unity 配置，不碰客户端仓；客户端仓里对应的生成类留到下一次获客户端授权的生成时删除，届时由客户端任务确认没有手写引用。

**改法**

proto：整份删除 proto/common/component/item_comp.proto。它从未被引用或持久化，不涉及字段号复用，不受 §4.3 的 reserved 约束。按 AGENTS §4 跑 proto-gen-build/proto-gen-run（未获客户端授权时用 proto_gen.no-unity.yaml）重新生成，核对 cpp/generated/proto/CMakeLists.txt:62、proto.vcxproj(.filters)、go/proto、generated/proto/\*、robot/vendor 中 item_comp 的产物已消失；若 CMakeLists/vcxproj 不由生成器维护，就手工删掉对应一行。按 §12 请 Java 会话同批删除 game-proto/src/main/proto/proto/common/component/item_comp.proto，并在 PARITY.md 登记。文档：exploit_loss_prevention{,_en,_zh}.md 删掉第 25 行（Bag::BlockItem）和第 28 行（PlayerItemBlockComp）两张表行；136 行改为只写 IsCurrencyBlocked，并注明「单人物品封禁已于 2026-10-15 C004 删除，物品只有全服 GainBlockService」；169 行第 8 条删掉或改为同样的说明。bag-service-srp-refactor{,_en,_zh}.md 顶部加「2026-10-15：PlayerItemBlockList 与 blockList 参数已按 C004 删除，下文为 2026-03-26 历史」。04-asset-channel.md:483 的签名去掉 `const PlayerItemBlockList& blockList,`（417 行的「blockList 取法照 player_mission.cpp」同删）。

**回归测试**

不需要。改动只涉及没有任何引用的 proto 消息和文档，运行期行为不变；完整性由重新生成后全量编译确认，编译由负责人自己做。

<details><summary>复核者给出的拍板选项</summary>

PlayerItemBlockComp 要不要一并删除。A（推荐）：整份删掉 item_comp.proto，Java 同批删除。它从未被引用或持久化，删除没有兼容风险，符合「不新增单人 GM 或持久化契约」的决定，代价是两版各重新生成一次。B：保留消息，只把注释改成「未使用；单人封禁已于 C004 删除，留作将来需求」。没有调用者却留着契约，违反 YAGNI，后来的人还会误以为功能半存在。

</details>

### T2 存盘与退出：拒存处置、交接预检与注释

只动 player_lifecycle.{h,cpp}、player_battle.cpp 注释与对应测试。该文件有其他会话在途改动，按函数名定位，不覆盖他人改动。

#### T2.1 W1-01（low）退出流程遇到拒存一律保留实体:拒存原因全是永久的,保留永远存不了盘,只会让退出收不了尾、重连复用僵尸并扩大丢失量;但当前生产代码到不了拒存分支,属防御分支的处置口径问题,原修法(releaseMarkSuppressed + FinishExitAfterPersist)还会经改派写出交接标记

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/services/scene/player/system/player_lifecycle.cpp:3099, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3009, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2236, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3338, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3408, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3438, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4487, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2648, docs/design/ecs-component-access-rules.md:61, cpp/tests/cross_zone_test/cross_zone_test.cpp:1183, cpp/tests/cross_zone_test/db_task_dispatch_test.cpp:577
- 本单口径：**负责人已拍板（2026-10-16，核心拍板 2）：选 B，退出流程拿到 kRejected 一律调 DestroyDeposedPlayer 不存盘丢弃，不写 A1′、交接标记，不发改派。** 按下面的改法实施；规则文档第 61 行随 T11 第 10 条一起改。

**改法**

原则:退出流程拿到 kRejected,一律直接调用现有的 DestroyDeposedPlayer,不存盘地丢弃实体。它不存盘、不写 A1′ 和任何交接标记、不发改派;作废票据并计数;摘场景、摘会话后 DestroyPlayer(内部会调 DbTaskDispatchSystem::OnPlayerGone)。不新增标志位、不新增辅助函数,也不走 FinishExitAfterPersist。

1. HandleExitGameNode 3099-3104:把 return 前的保留改成 `LOG_ERROR << ..."; discarding without persisting (metric=save_marshal_refused)"; DestroyDeposedPlayer(exitingPlayerId, "exit_save_refused"); return;`。3090 行已保证 exitingPlayerId 有效且映射到本实体。

2. 重踢分支 3009-3014:同样改为 `DestroyDeposedPlayer(retainedPlayerId, "exit_save_refused"); return;`。

3. HandlePlayerAsyncSaved 2236-2244:删掉 `get_or_emplace<PlayerLastPersistedSnapshotComp>(...).Replace(message)`,改为 `DestroyDeposedPlayer(playerId, "exit_save_refused"); return;`。实体已经没了,不再走函数末尾的快照更新。

4. FinishExitOrAwaitDbTaskAck 3338-3345:改为 `DestroyDeposedPlayer(playerId, "exit_save_refused"); return true;`。返回 true 的语义就是“实体已销毁”,调用方据此不再触碰实体。

5. OnDbTaskDispatchSettled 3408-3414 与 3438-3443:两处都改为 `DestroyDeposedPlayer(playerId, "exit_save_refused"); return;`。

6. HandlePlayerSaveRejected 4487-4492:只对 `any_of<UnregisterPlayer>(playerEntity)` 的实体改为 DestroyDeposedPlayer;在线实体维持现状,在线拒存另立项(batch1-verify 已建议踢线重载)。

7. HandleExitGameNode 3117 与 HandlePlayerAsyncSaved 2282 的 `!= kQueued` / `!= kAlreadyPersisted` 分支:只把其中的 kRejected 分流到 DestroyDeposedPlayer;kHandoffAlreadyRequested 保持“等在途存盘落地”。

8. SavePlayerToRedisImpl 4245-4268 的顺序不动。交接已发起时被拒,走 DestroyDeposedPlayer 会保留旧标记,按 I1 它是真的,与 ConcludeHandoffAfterMarkSent 的处理一致(设计文档 1066 行);重载时 A2′ 会删掉它。不需要原审查说的把 kHandoffAlreadyRequested 提前。

9. DestroyDeposedPlayer 非 routine 分支的 WARN 文案“(ownership moved away)”改为“(local state abandoned)”,避免拒存场景下误导排障。

10. 文档:ecs-component-access-rules.md:61 改为:“缺组件拒存不是已落盘证据:退出、异步比较和交接不得据此宣称收敛,不得写 A1′、交接标记或发改派。拒存原因都是永久的,保留的实体永远存不了盘,所以退出流程遇到拒存时用 DestroyDeposedPlayer 不存盘丢弃,玩家重登时从最后一次完好落盘重载。”同时在 batch1-codex-receipt.md“与原任务单不一致之处 1”下追加一条更正批注(不改原文)。

**回归测试**

工程 cpp/tests/cross_zone_test(cross_zone_test.vcxproj,含 db_task_dispatch_test.cpp)。

① 新增 ExitPersistEcs.RejectedExitIsDiscardedNotReused:ScopedPersistedPlayerCleanup;player=MakeOnlinePlayer(kSessionAtExit);ASSERT_TRUE(MarkPersisted(player));记下 exit_release_stats::Get().decisions 各项之和 before,以及 ExitReleaseMarksInFlight();remove\<PlayerAssetOpLedgerComp>(player);HandleExitGameNode(player, kClientDisconnect)。断言:EXPECT_FALSE(valid(player));EXPECT_EQ(tlsEcs.GetPlayer(kExitPlayerId), entt::null)(重连不可能再复用);SessionMap().count(kSessionAtExit)==0;decisions 之和等于 before(A1′ 根本没进判定);ExitReleaseMarksInFlight() 不变。修复前实体仍有效,首条断言红。

② 把现有 5 个用例改断言并改名:cross_zone_test.cpp 的 MissingPersistedComponentDoesNotFinishInline、ExhaustedRekickWithMissingComponentRetainsEntity、LandedCallbackWithMissingComponentDoesNotConverge、StaleEpochResaveRejectedRetainsExitingEntity、IssuedHandoffCannotHideMissingPersistedComponent,改为 ...DiscardsWithoutReleaseMark。统一断言:实体已销毁、playerList 与 SessionMap 已清、A1′ 判定计数不变。Issued 用例另断言 relocate / withdraw 都没有发出:WithdrawHandoffMark 待撤回表大小不变;若无访问口,改断 travel_handoff_stats exitWins 不变。

③ db_task_dispatch_test.cpp:577 DbTaskDispatchEcs.ExhaustedRekickWithMissingComponentRetainsEntity 改为:EXPECT_FALSE(valid(player));Delta(before, kExitUnsynced)==0;Delta(before, kOrphanUnsynced)==1(DestroyPlayer→OnPlayerGone 回收了欠账条目);EXPECT_FALSE(NeedsResave(kDispatchPlayerId));SessionMap().count(kDispatchSession)==0。

④ 新增 ExitPersistEcs.StaleEpochRejectOnOnlinePlayerKeepsEntity,固定范围:没有 UnregisterPlayer 的在线实体被拒时仍然保留。修复前后都绿,只防误扩大。

以上 ①②③ 修复前都会红。按 AGENTS §10.1,由负责人或 Codex 编译运行,Claude 未运行。

<details><summary>复核者给出的拍板选项</summary>

拒存退出的处置(当前生产代码到不了,只决定将来加载不变量被破坏时的失败形态):

A 保留实体(现状)。好处:字面上从不在没有落盘证据时销毁实体,实体可留作现场。代价:拒存原因全是永久的,实体永远存不了盘;退出一直不收尾,IsSaveInFlight 恒为真;停机靠 15s 看门狗,排空和疏散票据消费不掉,重踢也不会触发;30s 租约内重连会经 CancelExitOnReconnect 复活实体,之后所有存盘都被拒,丢失量随在线时长无界增长;Go 资产指令每 500ms 重查一次 durable,永不定案;战斗锁不销账,玩家开不了下一场;交接中的实体要冻到应答或 70s 上限。规则文档第 61 行的这条口径已经被另一会话照搬。

B 不存盘丢弃(DestroyDeposedPlayer,不写 A1′、不写标记、不改派)。好处:I0、I1 和 A1′ 的语义都成立;丢失量等于上次完好落盘后的增量,与宕机相同;重登从 Redis 重载,资产按 I7 重投并补应用,结算由登录钩子补应用,都能收敛;改动只是 6–7 处把“保留”换成一行现有函数调用,可读性更好。代价:现场实体没了,只剩日志;没有 A1′,30s 租约内被挑到别的节点的重连会收到可重试的 18,直到租约监视器调 LeaveScene(≤30s);排空中仍连着的客户端,票据被作废、不踢线,要等它自己重连(都只伤活性)。

B′ 原审查建议(releaseMarkSuppressed + FinishExitAfterPersist):数据结局与 B 相同,对排空中的在线玩家活性更好(会被改派走),但 DispatchEmergencyRelocate 会写 "E:now" 交接标记,违反 I1 字面契约和第 61 行,需要另改规则才能成立,不推荐。

推荐 B。若只想先止住错误口径继续扩散,最低限度也要改第 61 行(第 10 条),代码可另批跟进。

</details>

#### T2.2 W1-02（low）三处交接预检各构造一份整份 PlayerAllData 后丢弃。StartTravelHandoff 的预检拒绝没有调用点日志；BeginTravelHandoff 的预检保护不了任何东西。提交点 kRejected 分支是负责人要求保留的复核，不是缺陷

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/player/system/player_lifecycle.cpp:4743-4749, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4791-4796, cpp/libs/services/scene/player/system/player_lifecycle.cpp:5270-5277, cpp/libs/services/scene/player/system/player_lifecycle.cpp:5703-5711, cpp/tests/cross_zone_test/cross_zone_test.cpp:1891, cpp/tests/cross_zone_test/cross_zone_test.cpp:1913
- 本单口径：默认选 A：删除 BeginTravelHandoff 写标记前的整份 marshal 预检。

**改法**

1) StartTravelHandoff（4746-4749）保留预检，它是负责人规格要求，用例 TravelPersistEcs.IncompletePlayerNeverStartsHandoff 钉着它。在 return 前补一行调用点日志：`LOG_ERROR << "[ZoneTravel] handoff precheck refused for player " << playerId << ": persisted component missing (see the [PlayerSave] line above)";`，并把 `verifiedState` 改名为 `discardedProbe`。4743-4744 的注释补半句「当前代码中预检与提交点之间没有摘持久组件的路径，4792 分支只作提交点复核」。4792-4796 不动。

2) BeginTravelHandoff：删除 5270-5277 整段（注释 + PlayerAllData verifiedState + MarshalPlayerForSave + AbortTravelHandoff）。同步删除 cross_zone_test.cpp:1913 的 LandedCallbackWithMissingComponentAbortsBeforeMark，以及 1023-1033 的 g_missingPersistedBeforeMarkLines、CountMissingPersistedBeforeMark。这一项等负责人拍板，见 owner_decision。

3) B5（5705-5711）保留，只把 `verifiedState` 改名为 `discardedProbe`。

4) 不新增 HasAllPersistedComps，不改 player_data_loader.h.tmpl 和两个 loader。

**回归测试**

cross_zone_test 工程，用例 TravelPersistEcs.IncompletePlayerNeverStartsHandoff（cross_zone_test.cpp:1891）。仿照 CountMissingPersistedBeforeMark 新增计数器 g_handoffPrecheckRefusedLines，统计含子串 "handoff precheck refused" 的行。在调用 StartTravelHandoff 前后用 muduo::Logger::setOutput 挂上、恢复，新增断言 EXPECT_EQ(g_handoffPrecheckRefusedLines, 1)。修复前没有这行日志，计数为 0，用例红。

若负责人选择删除 BeginTravelHandoff 预检：把 1913 的用例改写为 LandedCallbackWithMissingComponentReachesMarkGate。保持原夹具（摘 BattleSettlementLedgerComp），若 MakeReplyTestPlayer 没挂 epoch，先 emplace_or_replace\<PlayerOwnerEpochComp>(player).epoch = 7。驱动 HandlePlayerAsyncSaved(kExitPlayerId, landed)，断言："persisted component missing before writing the handoff mark" 出现 0 次，"zone redis not connected" 出现 1 次（单测宿主没有 Redis）。修复前第一项计数为 1，用例红。

<details><summary>复核者给出的拍板选项</summary>

BeginTravelHandoff 写标记前的完整性预检要不要删？

选项 A（推荐）：删除。标记的语义是 Redis ⊇ 冻结内存，进入这里之前一定刚有一笔 marshal 成功的存盘，事后内存缺组件推翻不了标记。删掉后，在这种不可能状态下目标节点会加载完整存档，源实体不存盘销毁，结果比现在好。代价是删掉一个钉住假想行为的用例和它的日志计数器。

选项 B：保持现状，只改变量名。代价是多一次整份 marshal；在不可能状态下，玩家会被解冻成一个此后每次存盘都被拒的实体（与 W1-01 同类后果）。

</details>

#### T2.3 W1-03（low）结算 RequestSettlementPersist 中 PersistFn 返回 false 的注释列了一个在这里走不到的来源（交接/退出跳过），又漏了实际可达的拒存；原修法把「交接已发起」写进新注释同样不准

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:401-407

**改法**

把 player_battle.cpp 403-404 两行注释替换为：

// false 只表示没有排入新写:要么盘上已是这一份(快路径),要么存盘被拒(缺身份 / 持久组件、存盘序号耗尽,保存入口已打 ERROR)。

// 交接 / 退出在途已被上面 IsSettlementApplicable 挡掉,走不到这里。哪种都不能当成「已落盘」—— durable 一律由 IsSettlementDurable 按字节判。

DEBUG 文案改为 "[PlayerBattle] Settlement save not queued(unchanged or refused), player_id="。

同函数 372-373 行「这两个窗口里 SavePlayerToRedis 开头直接跳过写盘」也不准确，但基线就已存在，不是本批引入。负责人同意的话可在同一提交里改成「这两个窗口里本节点无权或不应再压结算存盘」；否则不动。

**回归测试**

不需要：只改注释和 DEBUG 级日志文案，日志文案不是对外契约，也没有分支行为变化。

#### T2.4 W1-04（low）SavePlayerToRedisImpl 头注释仍写「只有三个调用方」传 allowSkipWhenPersisted=false，实际 5 处，与 cpp 内清单不一致

- 位置：cpp/libs/services/scene/player/system/player_lifecycle.h:1438-1446, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4298-4301

**改法**

player_lifecycle.h 1440 的「只有三个调用方:」改为「调用方:」，在 1444-1446 的 B5 条目后追加一条：

//   \* 下线收尾的 DBTask 回执闸门(FinishExitOrAwaitDbTaskAck / OnDbTaskDispatchSettled):快照只证明那份字节进了 Redis,

//     DbTaskDispatchSystem 记着 MySQL 欠账(NeedsResave)时必须按当前内存整份重发,不能走快路径。

同时删掉 player_lifecycle.cpp 4298-4301 的调用方枚举，改为一句「调用方清单与理由见头文件 SavePlayerToRedisImpl 的注释」，只留一个权威来源；4303-4305 关于 DBTask 欠账的说明保留。不给 SaveResult 枚举值新增注释。

**回归测试**

不需要：纯注释改动，没有行为变化。

### T3 进程内事件与 dispatcher 收口（本单主线，scene_local_events.cpp 串行改）

T3.1–T3.6 顺序固定，每步都会改 scene_local_events.cpp、player_lifecycle.cpp 或 aoi.cpp;T3.7–T3.9 在其后不分先后。每完成一步跑一次本步列出的测试再进入下一步。T4、T6、T7 依赖本组。

#### T3.1 W1-23（medium）生产中零监听或只有空监听的 6 个事件仍在 trigger，还配了通知后重入复核和人造重入测试；复核分支一旦走到会留下半成品状态

- 合并：W1-32、W2-fit-05
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:227-228,252-270, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2909-2927, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4095-4171,4176-4180,4199, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2793-2797, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2886-2899, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3089-3093, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4538-4552, cpp/libs/modules/mission/system/mission.cpp:107-110, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:52-63,83-125,170-186, docs/design/ecs-component-access-rules.md:59,83
- 本单口径：默认选「拍板」中的 A：删除 PlayerLoginEvent。W2-fit-05 的文档部分并入 T11。

**改法**

思路：删掉这些事件和复核，不另加防护。(1) 删除事件结构体：scene_events.h:8 的 BeforeLeaveScene；actor_events.h:5-7 的 InitializeActorCompsEvent 及其注释；player_events.h:6、8 的 RegisterPlayerEvent、InitializePlayerCompsEvent；mission_events.h:8 的 OnAcceptedMissionEvent。PlayerLoginEvent 是否删除见负责人拍板。proto/event_id.txt 已经是 reserved，proto 不用改。(2) 删除 trigger：mission.cpp:107-110；InitPlayerFromAllData 的 4152-4155 与 4162-4171；DetachFromScene 的 2909-2910 与 2926-2927；HandleEnterScene 的 252-253 与 260。(3) 把 AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene&) 改为 AoiSystem::OnLeaveScene(entt::entity entity)：改 aoi.h:6、14 和 aoi.cpp:215-216；aoi_system_test.cpp:348-350、503-505 与 aoi_delivery_test.cpp:197-199、229-231、243、266-268 改为直接传实体。(4) scene_local_events.cpp：删除 RegisterPlayerEventHandler（52-63）、InitializePlayerCompsEventHandler（83-94），以及 170、172、183、185 行的 connect/disconnect。(5) 删除随之不可达的复核。HandleEnterScene：删 227 行的“通知可以同步搬移组件存储”注释、255-258 行和 261-270 行，第 1 步改为调用 W1-34 的 LeaveCurrentScene(player)；player_scene.h:48 的注释改为“输入无效或无入场权限返回 false”。DetachFromScene：删 2913-2916。DestroyPlayer：2886-2891 改回无条件的 tlsEcs.playerList.erase(playerId)，2893-2899 改为无条件调用 DbTaskDispatchSystem::OnPlayerGone(playerId)，并删掉“通知里重建出的新实体”这段注释。HandleExitGameNode：删 3090-3093。DestroyDeposedPlayer：删 4546-4552，并删掉 4538-4540 注释里“让 DetachFromScene 触发的事件看到…”这条理由。InitPlayerFromAllData：删 4095 的 notificationOwnership 和 4096-4121 的 canContinueInitialization lambda；4137 改为 if (!ActorAttributeCalculatorSystem::InitializeOnCreate(player)) return entt::null;；删 4141；4176-4180 改为 if (!stillOwnsEntry) return entt::null;；删 4199（EnterScene 返回 true 时，canContinueEntry 已保证 GetPlayer==player）。EnterScene 里跟在真实业务调用后的 canContinueEntry 和 EntryOwnershipGuard 不在本条范围，保留。(6) 删除人造重入测试。cross_zone_test.cpp：4275-4363 的 ReplacePlayerDuringLeaveNotification 和两个用例；5418-5463 的 RevokeSceneAccessOnLeave、TakeOverEntryDuringNotification；5539-5666 的 AccessRevokedByLeaveNotificationIsRecheckedBeforeBinding、LeaveNotificationIndexTakeoverKeepsOldActorFromBindingTarget、LeaveNotificationSameActorSessionTakeoverStopsOldEntry、LoginNotification\* 两例、EnterSceneStopsAfterLeaveNotificationDestroysPlayer。player_feature_persistence_test.cpp：80-130、167-182、218-273 的观察者类；InitRevokedAfterNotificationsCleansOnlyItsActorAndSession、InitializationNotificationIndexOrSessionTakeoverPreservesNewOwner、InitializationStopsAfterEachNotificationDestroysOrReplacesPlayer 三个用例；847-853、900-906 的连接，以及 878、915-917 的通知计数断言（ExpectLoadedActorProjections(created) 这类返回后的断言保留）；296、300、308、316、457 处的 OnAcceptedMissionEvent 计数。另外删 player_mission_system_test.cpp:93 与 player_feature_handler_test.cpp:157 的 clear\<OnAcceptedMissionEvent>。(7) 文档：ecs-component-access-rules.md:59 改为“默认技能、任务 scope、派生投影由加载链直接完成，不发初始化通知”；:83 改为“没有消费者不发事件；dispatcher 只做可有可无的一对多通知；监听者不得销毁或替换实体，不得改 SceneEntityComp、playerList 或会话；trigger 之后不写恢复分支”。

**补充（来自 W1-32）**

见 W1-23。DestroyDeposedPlayer 和 HandleExitGameNode 的提前 return、canContinueInitialization、notificationOwnership、HandleEnterScene 通知后的复核与 CopyFrom 重取，以及相应的人造重入测试，都在 W1-23 的删除清单里。EnterScene 中跟在真实业务调用之后的 EntryOwnershipGuard 和 canContinueEntry 保留。

**补充（来自 W2-fit-05）**

见 W1-23（它的修法第 (7) 条已覆盖 :83、:59 的改写，以及「没有消费者不发事件；dispatcher 只做可有可无的一对多通知」）。下面只补 W1-23 没有的部分：

1. ecs-component-access-rules.md：在「## Overview」之后、「## Three Access Patterns」之前新增一节「## 哪些数据进 registry」，不超过 8 行，内容按 ecs-fit-overview §2 判据 (1)–(5) 精简。

   - 进 registry：大量同构实体并且被 view 按组件组合批处理的数据，或者需要按实体 O(1) 挂载、随实体销毁回收的数据。

   - 不进 registry：进程单例（归对象）、请求级数据（放栈上或作参数）、连接与网络资源、跨实体共享对象与私有索引。

   - registry 内的单例只允许是该 registry 的派生索引，放 ctx()，现例 ActorNetIdIndex。

   - 多个 registry 用自定义实体类型区分，现例 SceneEntity。派生索引只有一个维护入口。

   - dispatcher 判据直接引用 W1-23 写进 :83 的那句，不再写一遍。

   - 不复制 overview 的 10 个子系统结论表：那是设计稿，ScenePlayers 已删、节点表移出尚未做，多处已过时。只加一行「现存例外：节点 registry 上的 RpcClientPtr / MuduoGrpcChannelPtr 属存量，迁出见 W2-fit-06」。

2. ecs_zh.md、ecs.md、ecs_en.md 第 10 行：删掉末句「单例组件不免除这些所有权约束。」，换成「registry 内单例只允许是该 registry 的派生索引（放 `ctx()`）；进程单例、请求级数据和网络资源不进 registry，判据见组件访问规则。」三份同改。

3. 审计文档的「适用性总评」与 N1–N7 不交给 Codex：README §5 规定审计文档的 HEAD 状态由审查方更新，所以放进本轮正式报告时一并追加，每条一行写现状。

   - 已处理：N3（目标侧技能上下文镜像已删）、N4（engine 层 5 个发送封装 git grep 零命中）、N5（ScenePlayers 与 scene_test 已删）。

   - 见其他条目：N2 见 W1-22 / W1-23 / W2-fit-01 / W2-fit-03；N6 见 W2-fit-02；N7 见 W2-fit-06。

   - 待核：N1（CurrencyComp.debts 与 PlayerCurrencyComp.debts 仍是两份，下发是否过期）。

4. 棘轮脚本不在本条直接要求，见 owner_decision。

**回归测试**

不需要“修复前红”的运行期用例。理由：要删的是生产不可达的通知和恢复分支，只有安装人造监听者才能走到。分支和触发条件会一起删除，写不出不依赖人造监听者、又能“修复前红、修复后绿”的用例。替代验证：(1) 静态检查：git grep -nE "BeforeLeaveScene|InitializeActorCompsEvent|InitializePlayerCompsEvent|RegisterPlayerEvent|OnAcceptedMissionEvent|canContinueInitialization|notificationOwnership" -- cpp 结果为空（PlayerLoginEvent 按拍板结果另查）。(2) 由 Codex 跑 bag_test、cross_zone_test、aoi_test、aoi_delivery_test，以下现有无 sink 用例必须保持绿：PublicSceneSwitchesWithoutBusinessSinkAndCleansOldAoi、DestroyPlayerWithoutDetachLeavesNoAoiResidue、NewPlayerInitializesDefaultSkillsAndMissionScopeWithoutPlayerEventSinks、RegisteredPlayerInitializesActorProjectionsWithoutBusinessEventSinks、DuplicateEnterIsIdempotent、BeforeLeaveSceneDoesNotRecreateMissingGridList（改名后的版本）。

<details><summary>复核者给出的拍板选项</summary>

PlayerLoginEvent 怎么处理。A：删除事件和只打日志的 handler，把 LOG_INFO 挪到 EnterScene 第 5 步；DuplicateEnterIsIdempotent 不再计数事件，改为只断言 PlayerEnterGameStateComp；以后每日重置、欢迎消息这类必需步骤在第 5 步直接调用。B：保留它作为唯一的可选一对多通知，在 player_events.h 写明监听者契约（不得销毁或替换玩家，不得改 SceneEntityComp、playerList 或会话），删除 2797 的 canContinueEntry 复核，只在 Debug 下 assert，并删掉 LoginNotification\* 两例。推荐 A：与已删除的 OnServerStart、BeKillEvent 口径一致；handler 的所有分支都是“待接”，符合 YAGNI。

</details>

#### T3.2 W2-b1_runtime-09（low）InitializeOnCreate 只在新建的玩家实体上调用一次，却写成了可重入的契约：投影已有来源、保留脏位、返回走不到的 bool，还配了两条人造场景测试

- 合并：W2-cross_sem-09
- 位置：cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.h:15-19, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:119-136, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4135-4140, cpp/tests/bag_test/player_feature_persistence_test.cpp:937, cpp/tests/bag_test/player_feature_persistence_test.cpp:1013, docs/design/ecs-component-access-rules.md:57
- 本单口径：在 T3.1 之后做：T3.1 把 4137 改成判返回值，本步再改成无条件调用、返回 void。

**改法**

1) actor_attribute_calculator.h:15-19：签名改为 `static void InitializeOnCreate(entt::entity actorEntity);`，注释改为「actor 创建流程在 Unmarshal 之后对新实体直接调用一次，挂上空的同步投影 CombatStateFlagsComp；不能重复调用」。2) actor_attribute_calculator.cpp:119-136：函数体只保留 `tlsEcs.actorRegistry.emplace<CombatStateFlagsComp>(actorEntity);`。用 emplace 而不用 get_or_emplace，是为了让重复创建在调试版触发 EnTT 的 "Slot not available" 断言。valid 检查、UpdateMoveSpeed 调用、ProjectCombatStateFlags 调用和 return true 一并删除，因为新实体上没有来源，投影本来就是空的。MoveSpeedComp 那一行按 W2-b1_runtime-05 删除。ProjectCombatStateFlags 删除后只剩 ResetCombatStateFlags 一个调用方，可以内联回去，同时删掉匿名命名空间。3) player_lifecycle.cpp:4137-4140：改为在 Unmarshal 之后无条件调用 `ActorAttributeCalculatorSystem::InitializeOnCreate(player);`，去掉 `canContinueInitialization() ||` 和 if/return 分支。这一点与 W1-23 的修法一致，两条同批落地即可。4) 删除 player_feature_persistence_test.cpp 的 937 号测试 ActorCreationProjectsExistingSourcesAndPreservesRuntimeAndDirtyData 和 1013 号测试 ActorCreationRejectsNullAndStaleWithoutChangingPoolsOrReplacement。保留 922 号测试，去掉其中对 bool 的 ASSERT_TRUE，MoveSpeedComp 断言按 W2-05 删除。5) docs/design/ecs-component-access-rules.md:57：删除「重复调用重算投影,不清零 DerivedAttributesComp 或 HP/MP……也不清除加载 / 写入口已有的脏标记」这句，改为「新建实体时调用一次」。

**补充（来自 W2-cross_sem-09）**

见 W2-b1_runtime-09。补充对方没写的部分:

① actor_attribute_calculator.h:18-19 删掉“实体无效时返回 false…”这行注释,签名改为 `static void InitializeOnCreate(entt::entity actorEntity);`,注释写明“调用方保证 actorEntity 是刚创建的有效实体”。

② actor_attribute_calculator.cpp:122-127 整段 valid 检查删除,135 行 `return true;` 删除。

③ player_lifecycle.cpp:4137-4140 整个 if 删除,改为在 Unmarshal 之后无条件写一行 `ActorAttributeCalculatorSystem::InitializeOnCreate(player);`,不再在此处调用 canContinueInitialization()。

④ 测试:player_feature_persistence_test.cpp:925 的 `ASSERT_TRUE(ActorAttributeCalculatorSystem::InitializeOnCreate(actor));` 改成直接调用;整例删除 ActorCreationRejectsNullAndStaleWithoutChangingPoolsOrReplacement(1013-1036),它只为测这个不可达的 false 而存在。b1_runtime-09 已要求删 937 用例,982/1003 两处随之消失;如果负责人决定保留 937,这两处同样改成直接调用。

不采纳原文的备选方案(保留 bool 再拆条件走 DestroyPlayer + RefuseInheritedEntry),那是给不可达分支再加一层防护。

**回归测试**

不需要。理由：本条收窄的只是生产中走不到的契约（重复调用、无效句柄、新实体上已有来源），可达路径的行为不变，写不出修复前会红的行为测试。替代验证：调整后的 922 号测试 ActorCreationBuildsOnlyRequiredProjectionsWithoutDirtying，以及经 InitPlayerFromAllData 调用 ExpectLoadedActorProjections 的各条用例必须保持绿；另用机械门禁检查，`git grep -n "InitializeOnCreate(" -- cpp` 的生产命中只剩 4137 一处，且没有 `!ActorAttributeCalculatorSystem::InitializeOnCreate` 形式的判返回值写法。

#### T3.3 W1-34（medium）玩家离场序列（AOI 清理→摘 SceneEntityComp 与 Hex）在 HandleEnterScene 和 DetachFromScene 各写了一份，同类分叉以前造成过 Hex 泄漏

- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:248-271, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2902-2928, cpp/libs/services/scene/player/system/player_lifecycle.h:1450-1452
- 本单口径：在 T3.1 之后做：统一出口里直接调 AoiSystem::OnLeaveScene，不再 trigger。

**改法**

(1) 在 player_scene.h 新增 static void LeaveCurrentScene(entt::entity player);，并把 DetachFromScene 里关于 Hex 成对回收的理由搬到这里的注释，写清：AOI 先用仍在的 Hex/SceneEntityComp 清理格子和观察者，再成对摘除；不在场景里时 no-op。(2) player_scene.cpp 中的实现：if (!tlsEcs.actorRegistry.valid(player) || !tlsEcs.actorRegistry.all_of\<SceneEntityComp>(player)) return; AoiSystem::OnLeaveScene(player); tlsEcs.actorRegistry.remove\<SceneEntityComp, Hex>(player);（W1-23 未落地时沿用 BeforeLeaveSceneHandler，并把 trigger 放在最后，trigger 后不写复核）。(3) HandleEnterScene 第 1 步（248-271）整段替换为 if (oldScene != entt::null) LeaveCurrentScene(player);（oldScene == scene 的情况在 233 已经早退）。(4) 删除 PlayerLifecycleSystem::DetachFromScene（player_lifecycle.cpp:2902-2928 与 player_lifecycle.h:1450-1452），不保留透传包装。2882、3089、4191、4545 四个调用点直接调用 PlayerSceneSystem::LeaveCurrentScene；player_exit_intent.h:73 与 player_lifecycle.cpp:4539 注释里的 DetachFromScene 同步改名。

**回归测试**

不需要“修复前红”用例：两份副本目前行为一致，本条只是把它们收成一个出口，修复前没有可观察的行为差异。护栏：现有 cross_zone_test 的 PublicSceneSwitchesWithoutBusinessSinkAndCleansOldAoi（换场景路径）和 DestroyPlayerWithoutDetachLeavesNoAoiResidue（销毁路径）修复后必须保持绿。新增一例：在 cross_zone_test 的 SceneEntryPermissionTest 夹具下加 LeaveCurrentSceneClearsBindingHexAndWatchers，调用 PlayerSceneSystem::LeaveCurrentScene(player) 后断言 !any_of\<SceneEntityComp, Hex>(player)、watcher 的 AoiListComp 不含 player、source 的 SceneGridListComp 各格都不含 player；再调用一次，断言状态不变（no-op）。静态检查：git grep -n "remove\<SceneEntityComp" -- cpp/libs 只命中 LeaveCurrentScene 一处。

#### T3.4 W1-33（medium）每次离场都全量扫描全节点 AoiListComp，疏散、排空、停机这几条批量退出路径因此变成 O(N²) 的主循环同步阻塞，而这次扫描只兜住了生产中没有调用方的远端 Pin

- 合并：W1-37
- 位置：cpp/libs/services/scene/spatial/system/aoi.cpp:235-246, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2912, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3089, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3506, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3525-3528, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3552-3558, cpp/nodes/scene/main.cpp:191-197, cpp/libs/services/scene/player/system/player_scene.cpp:254
- 本单口径：默认选「拍板」中的方案 A：删除整个 Pin/Unpin API 与远端清扫，W1-37 随之消解。

**改法**

必做部分：删除 AoiSystem::BeforeLeaveSceneHandler 末尾的远端清扫，即 aoi.cpp:235-246（三行注释、remoteWatchers 的 view 循环和通知循环），离场清理恢复为只走 7 格：valid → 清空自己的表 → RemoveEntityFromGrid → BroadcastEntityLeave。ecs-component-access-rules.md:83 里「销毁兜底与解除 Pin 也清远端观察关系」一句按下面的拍板结果改写；docs/PROGRESS.md 追加一条，写明离场与批量退出的复杂度回到 O(7 格人数)。Pin 的远端清理按负责人拍板二选一（见 owner_decision）。方案 A，删除 Pin：删除 AoiSystem::PinAoiEntity、AoiSystem::UnpinAoiEntity（aoi.h:16-19、aoi.cpp:249-281）和 InterestSystem::PinAoiEntity（interest.h:20-22、interest.cpp:150-153、interest_system_mock.cpp:94-97）。删除专测远端清扫和 Unpin 的用例：aoi_delivery_test 中的 LeavingSceneNotifiesRemotePinnedWatcherExactlyOnce、MissingSpatialStateDoesNotSkipRemoteRelationshipCleanup、UnpinRemoteTargetImmediatelySendsDestroy、UnpinNearbyHiddenTargetDropsForcedVisibilityAndSendsDestroy，以及 aoi_system_test.cpp:586-725 的 Pin/Unpin 用例。只拿 Pin 构造优先级的用例（aoi_system_test.cpp:440/441/483/1130，aoi_delivery_test.cpp:107/158/172）改为 InterestSystem::AddAoiEntity(w, t, AoiPriority::kPinned)。三份 aoi_priority_design\*.md 的 Pin/Unpin 一节改为「未接入；接入时须同批设计目标离场的远端清理，不得全节点扫描」。方案 B，保留 Pin：新增按需组件 AoiPinnedByComp{EntityUnorderedSet watchers}，唯一创建入口放在 AoiSystem::PinAoiEntity：AddAoiEntity(kPinned) 成功后 get_or_emplace\<AoiPinnedByComp>(target).watchers.insert(watcher)。UnpinAoiEntity 确认条目是 kPinned 后，从 target 的 watchers 中 erase(watcher)。BeforeLeaveSceneHandler 中被删掉的扫描改为：try_get\<AoiPinnedByComp>(entity)，逐个 watcher 执行「valid + try_get\<AoiListComp> + entries.erase(entity)>0 才 Notify」，最后 remove\<AoiPinnedByComp>(entity)。

**补充（来自 W1-37）**

如果 W1-33 选方案 A（删除 Pin），本条随之消解，不必单独处理。否则：删除 InterestSystem::PinAoiEntity 的声明（interest.h:20-22）、定义（interest.cpp:150-153），以及 interest_system_mock.cpp:94-97 的同名函数。AoiSystem::PinAoiEntity 的函数体改为 return InterestSystem::AddAoiEntity(watcher, target, AoiPriority::kPinned);（选 W1-33 方案 B 时，再在这里维护反向索引）。测试中 10 处 InterestSystem::PinAoiEntity 一律改为 AoiSystem::PinAoiEntity，aoi_test 和 aoi_delivery_test 若缺 include 则补 "spatial/system/aoi.h"。interest.h 上「解除钉住必须经 AoiSystem::UnpinAoiEntity」的注释移到 aoi.h 的 Pin/Unpin 声明处。三份 aoi_priority_design\*.md 的 Pin/Unpin 一节和 API 表改为 AoiSystem::PinAoiEntity/UnpinAoiEntity，Unpin 语义按 aoi.h:16-17 改写（仍可见时恢复空间优先级，否则立即摘除并发销毁）。

**回归测试**

工程 aoi_delivery_test，新增用例 LeaveTouchesOnlyNeighborGridObservers。步骤：target=Spawn(0,0)；nearby=Spawn(1,0)；otherScene=tlsEcs.sceneRegistry.create()；elsewhere=Spawn(0,0)，在 Update 之前把 tlsEcs.actorRegistry.get\<SceneEntityComp>(elsewhere).sceneEntity 改为 otherScene；执行 AoiSystem::Update(0)；ASSERT_TRUE(InterestSystem::AddAoiEntity(elsewhere, target))，人为放一条不在邻格内的普通条目，代表任意非邻格的 AOI 参与者；deliveries.clear()；以 BeforeLeaveScene{target} 调用 BeforeLeaveSceneHandler。断言：EXPECT_EQ(0, DestroysFor(elsewhere, target))；EXPECT_TRUE(tlsEcs.actorRegistry.get\<AoiListComp>(elsewhere).Contains(target))，消息写「离场只访问邻格观察者（及钉住者），不扫全节点」；nearby 的 AoiListComp 不再含 target。修复前全量扫描会 erase 掉 elsewhere 的条目并发 ActorListDestroy，两条断言都会红。选方案 B 时保留并跑 LeavingSceneNotifiesRemotePinnedWatcherExactlyOnce（远端 Pin 仍收到 1 次销毁）；选方案 A 时该用例随 Pin 一起删除。由 Codex 构建 aoi_delivery_test.vcxproj（/m:1）并运行。

<details><summary>复核者给出的拍板选项</summary>

Pin 的远端清理怎么处理，需要负责人二选一。方案 A（推荐）：删除整个 Pin/Unpin API 和远端清扫。Pin 从引入至今在生产中没有调用方，按 YAGNI 删除改动最小，也最可读；代价是将来 buff/skill 接入 Pin 时，要连同目标离场的远端清理一起重新设计，并需要改写约 8 个用例和 3 份设计文档。方案 B：保留 Pin，加 AoiPinnedByComp 反向索引，离场代价降到 O(钉住者)；代价是为一个没有生产调用方的功能新增一个按需组件和三处维护点。这样 AoiSystem::PinAoiEntity 有了实际职责，W1-37 随之消解。两种方案都必须删除全量扫描。

</details>

#### T3.5 W2-cross_sem-02（medium）任务链的自动发奖、自动接后续、“完成任务 X”推进和战斗击杀进度都是必需步骤，现在仍经 dispatcher 转给 nodes 层的唯一监听者：services/modules 层离开 scene 组合根就不完整，测试只能把 nodes 文件编进来，监听入口缺持久组件时还会静默返回

- 原严重度 medium，复核结论「降级」；合并：W2-fit-01
- 位置：cpp/libs/modules/mission/system/mission.cpp:343-414, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:24-50, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:79-80, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:153-165, cpp/libs/services/scene/battle/system/player_battle.cpp:1834-1844, cpp/libs/services/scene/player/system/player_mission.cpp:137-184, cpp/tests/missions_test/missions_test.cpp:33, cpp/tests/bag_test/player_mission_system_test.cpp:87
- 本单口径：默认选「拍板」中的 A:ConditionEvent 不再经 dispatcher。**不加深度计数**(W2-fit-01 的深度上限建议不采纳：CheckAccept 拒绝已接取和已完成的任务，链长天然有界；overview §8 挑战意见也指出深度计数经过 Accept 会归零）。

**改法**

① mission.h 新增 struct MissionFollowUps { std::vector\<uint32_t> autoGrant; std::vector\<uint32_t> chainAccept; std::vector\<uint32_t> completed; }。MissionSystem::HandleConditionEvent 和 OnMissionCompletion 在 onlyMissionId 之前加出参 MissionFollowUps& out。OnMissionCompletion 里原来往 awardEvents、acceptEvents、conditionEvents 三个向量里填，现在改为分别写入 out.autoGrant、out.chainAccept、out.completed。删除 403-414 的三段 trigger，以及 352-355 讲 enqueue 和悬空引用的注释，只保留“先收集、等状态落定后再处理”的理由。

② PlayerMissionSystem 新增公开函数 OnCondition(entt::entity player, const ConditionEvent& fact)，依次做：

   - !valid 或 IsCrossZoneFrozen 时直接返回；

   - MissionsContainerComp 或 kPlayerMission scope 缺失时 LOG_ERROR 后返回；

   - 否则调 HandleConditionEvent(fact, missions, config, followUps)，再调 ApplyFollowUps(player, followUps)。

   ApplyFollowUps 放在 player_mission.cpp 的匿名命名空间，按原顺序执行：先逐个 ClaimReward（失败时 LOG_WARN，保留可领位）；再逐个 Accept(player, kPlayerMission, id, TimeSystem::NowMillisecondsUTC())（失败时 LOG_WARN）；最后为每个 completed 构造一个 kConditionCompleteMission 事实，递归调 OnCondition。不加深度计数，环由 CheckAccept 终止。PlayerMissionSystem::Accept 里第 153 行和第 180 行的两处 HandleConditionEvent 改为把待办收集进一个局部 followUps，函数末尾统一调一次 ApplyFollowUps。

③ player_battle.cpp 第 1842 行的 tlsEcs.dispatcher.trigger(progress) 改为 PlayerMissionSystem::OnCondition(player, progress)。1714-1723 的预检保留，它属于副作用前的校验，不是兜底。

④ scene_local_events.cpp：

   - 删除 AcceptMissionEventHandler、ConditionEventHandler、OnMissionAwardEventHandler 三个函数；

   - 删除 RegisterMission、UnregisterMission 两个函数，以及 RegisterAll、UnregisterAll 里对它们的调用；scene_local_events.h 删除对应的两个声明；

   - PlayerUpgradeEventHandler 第 79-80 行改为直接调用 PlayerMissionSystem::OnCondition（随 W1-22 一起搬进 PlayerAttributeSystem::OnLevelChanged）。

   mission_events.h 删除 OnMissionAwardEvent。AcceptMissionEvent 和 ConditionEvent 保留为进程内参数结构体，注释写明“不经 dispatcher”。OnAcceptedMissionEvent 按 W1-23 删除。

⑤ 测试改动：

   - 删除 missions_test.cpp:33 的 RegisterMission，以及 player_mission_system_test.cpp:87 的 RegisterMission 和 90-95 的 Unregister 与 dispatcher.clear；从 missions.vcxproj:160 和 bag_test.vcxproj:185 去掉 nodes 里的 scene_local_events.cpp。

   - player_mission_system_test.cpp 的 Progress() 改调 PlayerMissionSystem::OnCondition。

   - AutoRewardFailureRemainsClaimableAndDoesNotRepeatAfterManualRetry 里的 dispatcher.update 和 trigger(retry) 改为再调一次 ClaimReward，并断言返回 kRewardAlreadyClaimed。

   - AutoRewardIsGrantedWithoutDispatcherUpdate 删掉三行 dispatcher.size 断言。

   - player_battle_settlement_test.cpp 删掉 SetUp 和 TearDown 里的 ConditionEvent 观察 sink 与 tryReentry 重入模拟：直接调用之后已经没有重入入口；kills 断言改为先接一个击杀 7001/7002 的任务，再看任务进度。

**补充（来自 W2-fit-01）**：只采纳下面第 ②–④ 条（删除与改调用），**不采纳第 ① 条的深度上限**。

见 W2-cross_sem-02。补充对方可能没写的部分：

① 深度必须按挑战意见（ecs-fit-overview:677-678）贯穿：Accept 拆成只收集、不驱动的 AcceptImpl(player, scope, missionId, nowMs, MissionFollowUps& out)；ApplyFollowUps 处理 chainAccept 时调 AcceptImpl，depth 一路往下传。player_mission.cpp:153 和 :180 在 Accept 内部调 HandleConditionEvent 的两处改为往同一个 out 里收集。

② 删除：

- mission.cpp:349-357 的「不能边处理边派发 / 不能用 enqueue」注释，以及 :400-402 的重入安全论证。

- AcceptMissionEvent、OnMissionAwardEvent、ConditionEvent 三个结构体。

- scene_local_events.cpp:23-49 的三个 handler 和 :153-165 的 RegisterMission/UnregisterMission，以及 RegisterAll/UnregisterAll 里对它们的调用。

- missions.vcxproj:160、bag_test.vcxproj:185 的 ClCompile。

- missions_test.cpp:17/33 和 player_mission_system_test.cpp:7/87/90 的 include 与注册调用。

③ scene_local_events.cpp:79-80 的升级条件和 player_battle.cpp:1842 的击杀进度，改为直接调 PlayerMissionSystem::OnCondition。

④ 测试里直接调用 HandleConditionEvent 的地方都要跟着改签名或改走 OnCondition：missions_test.cpp 共 10 处（145/153/183/188/193/201/209/218/249/278），player_mission_system_test.cpp:441/442/447/557/560，player_feature_handler_test.cpp:191。若保留不带 out 参数的薄重载给测试用，要注明它不驱动后续任务。

**回归测试**

工程：bag_test，在 cpp/tests/bag_test/player_battle_settlement_test.cpp 里新增 fixture PlayerMissionWithoutSinkTest，以复用 PlayerBattleSettlementTestAccess。用例名：BattleKillCompletesAndAutoGrantsWithoutAnyDispatcherSink。

SetUp 照 PlayerMissionSystemTest 搭：PlayerGuidComp、PlayerBagsComp 并对每个 bag 调 SetPlayerGuid、PlayerMissionSystem::Initialize、ArmMissionTestItemSegment；再补上 BaseAttributesComp、DerivedAttributesComp（max_health 设为 100）、CurrencyComp、PlayerCurrencyComp、BattleSettlementLedgerComp；在 playerList 登记；调用 PlayerBattleSystem::SetPersistFnForTest 注入计数假实现。全程不调用任何 scene_local_events 的注册函数，并先断言 tlsEcs.dispatcher.sink\<ConditionEvent>().empty()。

步骤：

1. ASSERT_EQ(kSuccess, PlayerMissionSystem::Accept(player, 0, 4, 1000))，任务 4 是“击杀怪物 1、自动发 4 件物品”；

2. Inventory().SetCapacityForRestore(4)；

3. 构造 BattleSettlementData：player_id 填该玩家 guid，battle_id 填 801，添加一条 defeated_monsters{monster_config_id=1, count=1}；

4. EXPECT_TRUE(PlayerBattleSettlementTestAccess::Apply(player, settlement))。

断言：EXPECT_TRUE(Missions().IsComplete(4))；EXPECT_FALSE(Missions().IsClaimable(4))；背包物品总数等于 4。

修复前没有任何 sink，任务 4 的进度保持 0，IsComplete 为假，用例变红；修复后改为直接调用，用例变绿。另外，missions_test 去掉 RegisterMission 之后，现有的任务链（自动接续）用例在修复前同样会红，可作为第二份证据。

<details><summary>复核者给出的拍板选项</summary>

W1-22 的修法写的是“OnLevelChanged 之后只保留 ConditionEvent(kConditionLevelUp) 作为一对多通知”，与本条冲突。

选项 A：ConditionEvent 完全不再经 dispatcher，OnLevelChanged 直接调 PlayerMissionSystem::OnCondition。后果是任务进度不再依赖 nodes 层注册，测试也不用再链接 nodes 文件。

选项 B：按 W1-22 原文保留 dispatcher 通知。后果是只剩升级这一路继续依赖 RegisterAll，在没装订阅的宿主里静默失效，测试仍要把 scene_local_events.cpp 编进去。

推荐 A。另外，W1-22、W1-23 和本条都要改 scene_local_events.cpp，需要串行落地，建议顺序是 W1-23 → 本条 → W1-22。

</details>

#### T3.6 W1-22（medium）沉默状态和 GM 调级后的重算都是只有一个消费者的必需步骤，现在仍经 dispatcher 转发：不装订阅的宿主里会静默失效，skill_test 只能手工复刻接线，Buff 系统还为任意监听者的重入写了防御代码和专项测试

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:85,cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:94,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:65,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:139,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:171,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:176,cpp/libs/services/scene/player/system/player_attribute.cpp:1142,cpp/libs/services/scene/actor/actor_events.h:8,cpp/libs/services/scene/player/player_events.h:7,cpp/tests/skill_test/skill_test.cpp:209,cpp/tests/buff_test/buff_test.cpp:686,cpp/libs/services/scene/combat/buff/system/buff.cpp:232
- 本单口径：**修法第二部分第 1 条的最后一点作废**：升级后的任务条件改为直接调 PlayerMissionSystem::OnCondition（见 T3.5)，不再 trigger ConditionEvent。在 T3.5 之后做。

**改法**

一、沉默状态改为直接调用

1. combat_state.h/.cpp 把签名改为显式参数：

   - static void AddCombatState(entt::entity actor, uint64_t sourceBuffId, uint32_t stateType);

   - RemoveCombatState 同形。

   同时删掉两个前置声明和对 actor_events.h 的 include。函数体里把 addEvent.xxx/removeEvent.xxx 换成参数。注释“事件里的 entity 是外部数据(buff 生命周期投递...)”改为“调用方是 Buff 生命周期钩子”。

2. buff_impl.cpp:85-101：

   - AddSilenceState 改为 CombatStateSystem::AddCombatState(parent, buff.buffPb.buff_id(), kActorCombatStateSilence);

   - RemoveSilenceState 改为 CombatStateSystem::RemoveCombatState(parent, buffId, kActorCombatStateSilence);

   - include 由 actor_events.h 换成 combat_state/system/combat_state.h。

3. 删除 actor_events.h:8-19 的 CombatStateAddedEvent/CombatStateRemovedEvent。

4. 删除 scene_local_events.cpp 中的 handler（:139-147）、接线（:176-177、:189-190）和 :16 的 include。

5. proto/event_id.txt 里的墓碑行不动。

二、GM 调级后的重算改为直接调用

1. GmSetLevel 中 :1142-1146 改为直接执行：

   - Recalculate(player, RecalcReason::kLevelChanged); PushPanel(player);

   - PetSystem::RecalculateAll(player, PetSystem::RecalcReason::kLevelChanged); PetSystem::PushList(player);

   - 最后只保留真正的一对多通知：ConditionEvent progress{player, static_cast\<uint32_t>(eConditionType::kConditionLevelUp), {level}, level}; tlsEcs.dispatcher.trigger(progress);

   这样做有先例：player_battle.cpp:1808、:1835 已在同一库中直接调用 PetSystem 并 trigger ConditionEvent。

2. 删除 player_events.h:7 的 PlayerUpgradeEvent、scene_local_events.cpp:65-81 的 handler 和 :171/:184 的接线。

3. 同步改两处注释：

   - player_attribute.h:96 改为“GM:设等级,直接重算并推面板与宠物列表”；

   - player_battle.cpp:1811 不再提“PlayerUpgradeEvent 事件壳”，改为“升级后果见 PlayerAttributeSystem::GmSetLevel,经验系统接入时抽出共用”。

三、随之删掉只为任意监听者写的重入防御和测试

1. 删除 buff_test.cpp:686-745 的三个事件注入用例，以及辅助函数 CheckIndependentBuffDuringBonusRemoval。

2. 删除 skill_test.cpp:209 与 :226 的手工接线。

3. player_feature_persistence_test.cpp:954 改为 CombatStateSystem::AddCombatState(actor, kBuffInstance, kActorCombatStateSilence)。

4. BuffSystem 中三处防御的唯一来源是这两个事件，按负责人“优先删除”的口径一并删除：

   - AddOrUpdateBuff :236-240 OnBuffStart 之后的存活复核，以及 :232 的注释；

   - OnBuffStart :489-509 改回依次直接调用三个 impl；

   - RemoveBuff(set) :278-285 的预标记循环（只保留第二个循环的“级联清理后按存活 id 处理”）。

   删前用 git grep 确认 buff_impl/modifier_buff_impl/motion_modifier_impl 中不再出现 BuffSystem:: 或 dispatcher。

**回归测试**

以下用例都不调用 RegisterAll，在当前代码上应为红，修复后为绿。

① skill_test / SkillPrecheckPolicyTest.BuffPermissionRefusalPreservesPriorPhaseAndContext（改造现有用例）

- 步骤：只删除 SetUp 的 :209 和 TearDown 的 :226 手工接线，其余不变。

- 断言：:275 的 ASSERT_TRUE(states->states().contains(kActorCombatStateSilence)) 在修复前失败，因为事件没有订阅者，CombatStateCollectionComp 不会被创建。

② 新增 buff_test / BuffListLifecycleTest.SilenceBuffAddsAndRemovesCombatStateWithoutEventWiring

- 步骤：表里配一条 buff_type=kBuffTypeSilence、duration=-1 的 Buff；在新建 actor 上 AddOrUpdateBuff 得到 buffId。

- 断言：try_get\<CombatStateCollectionComp>(actor) 非空，states 含 kActorCombatStateSilence，且 sources 含 buffId。之后 BuffSystem::RemoveBuff(actor, buffId)，断言 states 不再含沉默。

③ 新增 bag_test / PlayerEquipAttributeTest.GmSetLevelRecalculatesDerivedAttributesWithoutEventWiring

- 步骤：使用夹具中已加载的 player，记录 oldMax = get\<DerivedAttributesComp>(player).max_health()；选一个 IsValidLevel 为真且大于 kLevel 的等级 newLevel，调用 PlayerAttributeSystem::GmSetLevel(player, newLevel)，断言返回 kSuccess。

- 断言：get\<DerivedAttributesComp>(player).max_health() > oldMax；BuildPanel 得到的 panel.level()==newLevel，且面板属性与再调一次 Recalculate(kLevelChanged) 的结果一致。

- 修复前为红的原因：bag_test 只有 player_mission_system_test 装了 RegisterMission，没有装 PlayerUpgradeEvent 的订阅，派生属性保持旧值。

#### T3.7 W1-27（low）OnNodeRemoveEvent::sceneNodeType 是无人写、无人读的死字段；SkillExecutedEvent 的 skillId/skillTableId 从不赋值，唯一消费者 BuffSystem::OnSkillExecuted 是空 TODO

- 位置：cpp/libs/engine/core/node/system/node/node_events.h:10,cpp/libs/services/scene/combat/combat_events.h:6,cpp/libs/services/scene/combat/skill/system/skill.cpp:19,cpp/libs/services/scene/combat/skill/system/skill.cpp:932,cpp/libs/services/scene/combat/buff/system/buff.cpp:588,cpp/libs/services/scene/combat/buff/system/buff.h:7,cpp/libs/services/scene/combat/buff/system/buff.h:83,cpp/libs/services/scene/scene.vcxproj:329,cpp/libs/services/scene/scene.vcxproj.filters:161

**改法**

1. node_events.h：删除 :10 的 uint32_t sceneNodeType{0};。

2. skill.cpp：删除 :932-935 的四行，并删除 :19 的 #include "services/scene/combat/combat_events.h"。

3. buff.h：删除 :7 的 struct SkillExecutedEvent; 前置声明和 :83 的 static void OnSkillExecuted(SkillExecutedEvent& event);。

4. buff.cpp：删除 :588-591 的 OnSkillExecuted 定义。

5. 删除 cpp/libs/services/scene/combat/combat_events.h 整个文件，以及 scene.vcxproj:329、scene.vcxproj.filters:161 的 ClInclude 行。

6. 将来真要做“技能执行后的 Buff 反应”时，再按实际需要定义参数，并在构造点填全。

**回归测试**

不需要。这是纯删除死代码，没有可观察的行为变化，写不出修复前会红的用例。替代做法是机械门禁：git grep -n -e SkillExecutedEvent -e OnSkillExecuted -e sceneNodeType -e combat_events.h -- cpp 的结果必须为零。

#### T3.8 W1-30（low）事件结构体化后仍有注释描述旧机制：mission.cpp 的“截成 uint32”注释和多余别名、CreateScene 回滚注释里的“AOI 格子表”、属性同步注释里的“Guid(uint64_t)组件”；回滚分支本身在生产中也已不可达

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/modules/mission/system/mission.cpp:345,cpp/nodes/scene/handler/rpc/scene_handler.cpp:754,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:62,cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:107
- 本单口径：修法第 3 条（删 CreateScene 回滚）由 T4.2 一并完成，本步只做第 1、2 条；第 4 条见 T5.4 与 T7.8。

**改法**

1. mission.cpp OnMissionCompletion：删除 :345-347 的两行注释和别名，把 :373、:388、:394 的 playerEntityValue 改为 playerEntity。

2. actor_attribute_calculator.cpp:107-108 的注释改为“序列化器从 PlayerGuidComp 取值(只有玩家有;NPC 不带 entity_id,见 W1-24),客户端按 ActorCreateS2C.guid 建立的 guid→actor 映射归属”。

3. CreateScene 两个入口推荐删除死防御：

   - scene_handler.cpp：在 emplace SceneInfoComp 的块内填好字段后，立刻执行 response->mutable_scene_info()->CopyFrom(sceneInfo)，然后再 trigger(OnSceneCreated)；删除 :752-771 的 trigger 后重查和回滚分支，日志改用 request 字段。

   - scene_node_service.cpp:60-78：做同样修改。

   - 在 scene_events.h 的 OnSceneCreated 注释中写明“监听者只做计数等通知,不得改动或销毁场景实体”。

   - 如果负责人决定保留 F096 防御，最小修法是只把两处注释里的“(Agones 计数、AOI 格子表)”改为“(Agones 计数)”。

4. cpp/AGENTS.md:69 按 W1-19 修，proto 的 entity_id 注释按 W1-24 修，本条不重复执行。

**回归测试**

不需要。第 1、2、4 项只改注释或别名，没有行为变化。第 3 项删除的分支在生产中不可达（唯一监听者不碰 registry），tests 中也没有任何 OnSceneCreated 订阅，写不出修复前会红的用例。替代验证：现有 CreateScene 相关测试（幂等去重、创建成功返回 scene_info）保持绿；再加一条 git grep 门禁，cpp 中 "AOI 格子表" 与 "Guid(uint64_t)组件" 的命中必须为零。

<details><summary>复核者给出的拍板选项</summary>

CreateScene 的 trigger 后重查和回滚分支是否删除（它来自审计 F096）。

- 选项 A：删除。代价是将来若有人加了会删场景的 OnSceneCreated 监听者，就会失去兜底，需要用 scene_events.h 的契约注释来约束。好处是两个入口各少约 20 行不可达代码和一段难懂的补发撤账逻辑。

- 选项 B：保留，只改注释。代价是继续维护无法测试的不可达分支。

推荐 A，与“dispatcher 只做可有可无的通知、不为不存在的监听者写防御”的口径一致，并且应与 W1-23 的删除清单同批执行。

</details>

#### T3.9 W1-38（low）第 5 批后有 4 处注释或文档与实现不符(skill.cpp、player_lifecycle.cpp、anomaly_detector.h、ecs-fit-overview.md);gate/main.cpp 那处并没有过时

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:64,cpp/libs/services/scene/player/system/player_lifecycle.cpp:4543,cpp/libs/modules/transaction_log/anomaly_detector.h:80,docs/handoff/ecs-refactor/ecs-fit-overview.md:121
- 本单口径：默认：player_lifecycle.cpp 4546-4547 的两个 remove 保留，只改注释。

**改法**

1) skill.cpp:删除匿名命名空间里的 StoreSkillContextForWrite(第 62-74 行整段,连同第 64 行注释),把它的函数体搬进 AddSkillContext(第 228 行),写成 if (!tlsEcs.actorRegistry.valid(caster)) return MAKE_ERROR_MSG(kThisEntityIsInvalid, ...); tlsEcs.actorRegistry.get_or_emplace\<SkillContextCompMap>(caster).emplace(context->skillid(), context); return kSuccess;。保留第 229-230 行注释,并补一句「SkillContextCompMap 的唯一写入口:首次写入时延迟创建,失效实体不创建」。同时把 skill_comp.h:17 里的「StoreSkillContextForWrite」改为「AddSkillContext」。

2) player_lifecycle.cpp DestroyDeposedPlayer:把第 4543-4545 行注释换成一行「交接标记与冻结成对摘除;实体随后销毁,离场链路不读这两个组件」。也可以连同第 4546-4547 行的两个 remove 一起删掉,因为之后的 DetachFromScene、RemovePlayerSession、DestroyPlayer 都不读它们,实体随即被 DestroyEntity 回收。

3) anomaly_detector.h/.cpp:删除没有生产调用方的 ClearPlayer(声明在 .h:81,实现在 .cpp:234-240),同时删掉 anomaly_detector_test.cpp 第 82 行和第 115 行这两处调用。第 80 行注释改为「ClearAll:测试隔离用,清空所有玩家窗口但不销毁玩家;登出不需要清理,窗口随玩家实体销毁」。第 77 行 GetCurrencyGainCount 的注释补上「只读查询,不创建窗口;目前仅测试观测用」。

4) ecs-fit-overview.md:121 改为「组件是纯数据、不嵌 registry。TimerTaskComp 可搬移,in_place_delete 只为指针稳定、不是正确性前提;具名容器组件允许继承 std 容器,基类转换必须 explicit(见 ecs-component-access-rules.md)」。

5) gate/main.cpp:148 不改。

**回归测试**

不需要。四处都是注释或文档修正,外加删除无生产调用方的代码(内联只剩一个调用方的 StoreSkillContextForWrite、删 ClearPlayer),不改变运行期行为。保持 skill_test 与 anomaly_detector_test(PlayerAnomalyWindowTest 其余 4 个用例)全绿即可,由 Codex 执行。

<details><summary>复核者给出的拍板选项</summary>

player_lifecycle.cpp:4546-4547 的两个 remove 是只改注释保留,还是一并删除?保留:零风险,只是多两行冗余。删除:更符合「删掉多余的东西」,但如果将来有人给 BeforeLeaveScene 挂上会检查 Frozen 的监听者,需要重新评估。推荐只改注释、保留 remove,因为 DestroyDeposedPlayer 是废黜路径,不值得为两行代码承担行为变更风险。

</details>

### T4 场景入口：creators 准入、Agones 计数与场景生命周期单一入口

依赖 T3.1（删除 SceneEntryPermissionTest 里依赖通知的用例后，T4.1 只改剩下的用例）。scene_handler.cpp、scene_node_service.cpp 都是生成式 handler，只改守护段；两文件有其他会话在途改动。顺序 T4.2 → T4.3 → T4.4。

#### T4.1 W1-40（high）Codex 把废弃 scene_test 里的 creators 断言做成了生产准入，而且只在 C++ 节点末端生效。非创建者按号加入镜像、组队跟进镜像、开启去重后复用镜像这三条合法流程都会被拒，scene_manager 已提交的 location 和人数随之与节点状态不一致，C++ 和 Java 两版对客户端的行为也分叉了

- 两位复核者（流程可达性、两版契约）均判「成立 / high」
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:196,cpp/libs/services/scene/player/system/player_scene.h:42,cpp/libs/services/scene/player/system/player_lifecycle.cpp:2484,cpp/libs/services/scene/player/system/player_lifecycle.cpp:2636,cpp/libs/services/scene/player/system/player_lifecycle.cpp:2725,cpp/libs/services/scene/player/system/player_lifecycle.cpp:4079,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:36,cpp/nodes/scene/handler/rpc/scene_handler.cpp:166,cpp/tests/cross_zone_test/cross_zone_test.cpp:5482,cpp/tests/bag_test/player_feature_persistence_test.cpp:80
- 本单口径：**负责人已拍板（2026-10-16，核心拍板 1）：选方向 A，撤掉 C++ 末端准入，回到两版同基线（creators 不做准入）。** 两位复核者独立确认 high。Go、Java 都不改代码；Java PARITY 第 128 行的 B5 改为「已决：不做准入，过时单测已删」，第 182 行补「C++ 曾加 3022 准入，已撤回，两版仍同基线」。测试改动在 T3.1 之后做（见 §2.3 第 5 条）。

**改法**

推荐删除(方向 A，需负责人确认，见 owner_decision)：

1) player_scene.cpp PlayerSceneSystem::CheckEnterScene(Guid, SceneEntity) 删掉第 196-197 行的 creators 分支，函数只保留「场景有效 + SceneInfoComp 存在 → kEnterSceneNotFound」和「playerId!=0 ? kSuccess : kInvalidParameter」。其余调用点(HandleEnterGameNode 预检、EnterScene、canContinueEntry、InitPlayerFromAllData、HandleEnterScene 及其 :261 复核、s2s EnterScene)不动，它们的场景存在性检查是 F091 的合法修复。

2) player_scene.h:42-44 的注释改成「纯校验：场景须存在于本节点、身份非零；creators 只用于镜像自动进场回显，不承担准入语义(两版同基线，见 PARITY B5)」。

3) 改测试载体，不删覆盖面：cross_zone_test 的 CreatorKeyAllowsEntryEvenWhenStoredFlagIsFalse 改为 NonCreatorJoinsMirrorById(creators={kExitPlayerId+99}，期望 kSuccess 并绑定 target)。RestrictedSceneRejectsUnknownMissingAndZeroGuidWithoutMutation 去掉 identity==0(未知 GUID)这一支，零/缺 GUID 改为期望 kInvalidParameter。LifecycleRejectsBeforeSessionOwnershipAndExitSideEffects 把 context scene_id 改成不存在的号，期望 kEnterSceneNotFound，不再改 creators。AccessRevokedByLeaveNotificationIsRecheckedBeforeBinding 的 RevokeSceneAccessOnLeave::OnLeave 改为 tlsEcs.sceneRegistry.remove\<SceneInfoComp>(target)，期望 kEnterSceneNotFound。player_feature_persistence_test 的 RevokeEntryDuringInitialization::OnPlayer 同样改为移除目标场景的 SceneInfoComp；InitRejectsMissingOrRestrictedDestination... 删掉 restricted 分支，只保留 missing。

4) 文档：docs/handoff/ecs-refactor/README.md §8「场景权限与创建确认」一行与 batch5 回执改为「creators 不做准入」；PROGRESS 追加一条；Java PARITY.md B5 记为「已决：不做准入，过时单测已删」。Go 和 Java 都不用改代码。

**回归测试**

修复前全部会红(当前返回 3022 或 kRejected)：

1) cpp/tests/cross_zone_test，SceneEntryPermissionTest.NonCreatorJoinsMirrorById。步骤：target 的 SceneInfoComp 设 mirror_config_id=1、creators={kExitPlayerId+99:true}。断言：EXPECT_EQ(kSuccess, CheckEnterScene(player,target))，ASSERT_TRUE(HandleEnterScene(player,target))，SceneEntityComp.sceneEntity==target。

2) 同工程，SceneEntryPermissionTest.EnterGameNodeAdmitsTeammateIntoLeaderMirror。步骤：creators 同上；context.enterInfo.scene_id=target 的 scene_id，session=kSessionAtExit，enter_gs_type=LOGIN_RECONNECT。断言：HandleEnterGameNode(kExitPlayerId, ctx)!=kRejected，玩家绑定 target，SessionMap 中 kSessionAtExit 映射到 kExitPlayerId。这条模拟队员跟随和同节点按号加入。

3) cpp/tests/bag_test，PlayerFeaturePersistenceTest.InitAdmitsNonCreatorIntoPrivateMirror。步骤：ScopedInitializationScene 的目标场景设 creators={kPlayerId+1}，再调 InitPlayerFromAllData(data, ctx)。断言：返回值!=entt::null，playerList 中有 kPlayerId，实体绑定目标场景。这条模拟跨节点按号加入和去重复用的自动进场。

去重复用的端到端回归(Go TestCreateScene_EchoesCreatorIds 已绿，配上用例 3 即可覆盖 C++ 侧)，以及 Java robot 的 CrossNodeScenario 对 C++ 服跑一次，交 Codex 冒烟。

<details><summary>复核者给出的拍板选项</summary>

需负责人拍板：creators 到底算不算准入。

方向 A，撤掉 C++ 末端准入，回到两版同基线(推荐)。

要改的文件：player_scene.cpp(删 196-197)、player_scene.h(注释)、cross_zone_test.cpp(SceneEntryPermissionTest 四个用例换载体，并新增 2 个回归)、player_feature_persistence_test.cpp(RevokeEntryDuringInitialization 和 InitRejects... 换载体，并新增 1 个回归)、README §8、batch5 回执、PROGRESS，以及 Java PARITY.md 的 B5 状态。

后果：行为与基线和 Java 版逐字节一致，客户端契约不变，3022 仍然不发；Go/Java 都不用改代码；改动只有约 6 个文件。代价是镜像不防外人：拿到镜像号的人都能进，但这正是 PARITY 当前登记的口径。

方向 B，准入挪到 scene_manager 选场景之前，两版统一实现。

要改的文件：

- go/scene_manager/internal/logic/createscenelogic.go：createInstance 把 CreatorIds 写进 Redis，例如 scene:{id}:creators SET。去重命中时 SADD 新调用者。rollbackInstanceAllocation 和 destroyInstanceInternal(instance_lifecycle.go)要删这个键。

- enterscenelogic.go：resolveSceneForEnter 之后、handoff 门、owner_epoch 补种、AtomicIncr、placePlayerLocation 之前加只读判定，拒绝时不改任何状态。需新增错误码，并在 C++ 的 enter_scene_reply::Classify / DispatchEnterSceneReply 里映射成 3022。

- 必须定放行规则：队员跟随(服务器代发、不带 client_channel_pick 的请求是否放行？要不要查队伍)、疏散/排空改派、死节点 resolveScene 重建(:1271 不带 creators)。

- 同时删掉 C++ 的 player_scene.cpp:196-197，scene_manager 是唯一权威，不留末端检查。

- Java scene-manager 同批实现，PARITY 加一行；Java robot 的 CrossNodeScenario.java 和 RobotOptions.java 的期望要改成被拒；tip 3022 变成两版客户端可见行为。

后果：Go + C++ + Java + robot + 设计文档都要改，是一项新功能，需要产品先定「谁可以进私有镜像」，滚动升级期间新旧 scene_manager 并存会出现准入不一致。

推荐 A：B5 本来就是挂起待定的可选项，负责人的任务只要求迁移「有效」断言，基线生产代码里 creators 从未承担准入。若以后确实需要私有镜像，再按 B 另立设计文档单独做，不放在 ECS 改造里夹带。

</details>

#### T4.2 W2-cross_sem-01（medium）Agones 单元计数这个必需步骤仍经 dispatcher 一对一转发(唯一监听者只调 OnUnitCreated/OnUnitDestroyed),两个 CreateScene 入口还为不存在的“监听者删场景”各留一套不可达的回滚

- 合并：W2-fit-03、W1-36
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:533,cpp/nodes/scene/handler/rpc/scene_handler.cpp:609,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:56,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:139,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:127,cpp/libs/modules/scene/scene_events.h:6,cpp/AGENTS.md:69,docs/design/agones-scene-node-high-density.md:135
- 本单口径：**W1-36 的修法（保留事件、写监听者契约）作废**，以本条为准：删除 OnSceneCreated/OnSceneDestroyed，入口直接计数。回归测试目标按「拍板」②的 (b)：随 T4.4 收进 scene 库函数后在 cross_zone_test 里测。

**改法**

直接调用,删掉事件:

① SceneHandler::CreateScene(scene_handler.cpp):在 emplace SceneInfoComp 的块内、creators 循环之后加 `response->mutable_scene_info()->CopyFrom(sceneInfo);`;块后直接调 `agones::GameServerLifecycle::Instance().OnUnitCreated(entt::to_integral(sceneEntity));`(createPermit 活到函数返回,顺序正确);删掉 518-519 关于 trigger 悬空的注释,以及 533-559 的 trigger、createdInfo 重取、回滚分支和 CopyFrom(\*createdInfo);561-565 的 LOG_INFO 改用 request 字段。

② SceneNodeGrpcImpl::HandleCreateScene(scene_node_service.cpp)按同样方式处理:删 41-42 的注释和 56-80 段,在块内 CopyFrom 后直接调 OnUnitCreated,82-86 的日志改用 request 字段。

③ SceneHandler::DestroyScene(scene_handler.cpp:609-612)和 HandleDestroyScene(scene_node_service.cpp:139-141):把 OnSceneDestroyed 的 trigger 换成 `agones::GameServerLifecycle::Instance().OnUnitDestroyed(entt::to_integral(targetEntity));`,紧接着 destroy。targetEntity 是本次调用刚从 view 取到的,中间没有通知,不需要再补 valid。

④ scene_events.h 删除 OnSceneCreated/OnSceneDestroyed 两行,保留 BeforeLeaveScene,并改第 5 行注释;scene_local_events.cpp 删除 127-137 两个 handler、174-175 与 187-188 四行 sink,以及不再使用的 agones include(第 17 行)。

⑤ 文档:cpp/AGENTS.md:69 改为“scene 的计数点是 CreateScene/DestroyScene 两个入口里对 OnUnitCreated/OnUnitDestroyed 的直接调用(场景目录落地后收进其 Create/Destroy),不得经事件转发”;docs/design/agones-scene-node-high-density.md:135、414、438 同步修改。

**补充（来自 W2-fit-03）**

见 W2-cross_sem-01。补充它可能没写到的点：

1) 在两个入口的 emplace SceneInfoComp 之后，直接调用 `agones::GameServerLifecycle::Instance().OnUnitCreated(entt::to_integral(sceneEntity))`。

- 调用点必须在当前函数内：muduo 路径的 createPermit 和 gRPC 路径 wrapper 里的 permit 都要活到这一调用之后，否则 permit 结束时会看到零单元而请求 Ready。

- 去重命中分支不计数，保持现状。

2) 两个 DestroyScene 在 `sceneRegistry.destroy` 之前直接调用 OnUnitDestroyed，key 与创建时一致。

3) 删除：

- scene_events.h:6-7 的两个结构体（保留 BeforeLeaveScene）

- scene_local_events.cpp:127-137、174-175、187-188

- scene_handler.cpp:533-558，以及 :610-612 的 trigger

- scene_node_service.cpp:56-79，以及 :139-141 的 trigger

- 两处「sceneInfo 引用只在这个块里用 / F096」那段为 trigger 写的注释

4) 回包直接 `response->mutable_scene_info()->CopyFrom(sceneInfo)`：中间已经没有 trigger，引用不会失效，所以不必再 try_get 重取。

5) 修改 Go 侧 scene_node_client.go:119-120 的注释。

若同时按 W2-fit-02 合并入口，这两处直接调用应写在合并后的唯一函数里。

**W1-36 的修法作废**（以本条为准）；它指出的「回滚分支为不存在的监听者而写」随本条删除事件一并消失。

**回归测试**

目前没有任何测试工程编译 scene_handler.cpp 或 scene_node_service.cpp,需要新增或扩充测试目标(见 owner_decision)。用例 SceneCreateDestroyCountsAgonesUnitWithoutLocalEventRegistration:GameServerLifecycle::Instance().StartDisabled();不调用 scene_local_events::RegisterAll;记 before = UnitCount();构造 CreateSceneRequest(config_id=1, scene_id=4242),调用 SceneHandler{}.CreateScene(nullptr, &req, &resp, nullptr)。断言 EXPECT_EQ(before + 1, UnitCount()) 且 EXPECT_EQ(4242u, resp.scene_info().scene_id())。再调 DestroyScene(scene_id=4242),断言 EXPECT_EQ(before, UnitCount())。修复前计数只靠未注册的监听者,UnitCount 保持 before,第一条断言会红。HandleCreateScene 是私有成员,所以走 muduo 入口。

<details><summary>复核者给出的拍板选项</summary>

① 与 W1-36 的修法冲突。W1-36 建议保留事件、“改用监听者契约收口”并删掉回滚;本条建议整个删掉事件,改为直接调用。推荐本条:必需记账不走 dispatcher,且删掉事件后 W1-36 要防的 trigger 后重验与回滚都不复存在。选 W1-36 则继续保留一对一事件和“宿主漏注册即静默丢计数”的风险。② 回归测试需要一个编译 scene_handler.cpp 的测试目标。可选做法:(a) 把 scene_handler.cpp 加进 bag_test(它已编 nodes/scene/handler/local_event/scene_local_events.cpp),代价是引入整套 RPC handler 依赖;(b) 随 W2-fit-02/W2-b5_types-02 把创建/销毁收进一个场景域函数,计数只调一处,测试放在链接 scene.lib + infra 的 aoi_test 或 bag_test。推荐 (b):同时消除“两入口手工保持同步”,计数点也只剩一个。

</details>

#### T4.3 W2-b5_types-02（low）已有 PlayerSceneSystem::FindSceneById,但两个入口仍各手写 2 份 scene_id 全表扫描(共 4 份);原修法漏了 gRPC 建场入口没有 scene_id==0 校验,直接替换会改变 0 号去重语义

- 原严重度 low，复核结论「降级」；合并：W2-cross_sem-08
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:498,cpp/nodes/scene/handler/rpc/scene_handler.cpp:583,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:25,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:96,cpp/libs/services/scene/player/system/player_scene.cpp:182
- 本单口径：gRPC 建场入口补 scene_id==0 拒绝是本步必做，否则替换后 0 号会重复建场景。

**改法**

只改守护段内的代码。

(1) SceneHandler::DestroyScene:删掉 `SceneEntity targetEntity = entt::null;` 到循环结束这 11 行,换成 `const SceneEntity targetEntity = PlayerSceneSystem::FindSceneById(sceneId);`。SceneNodeGrpcImpl::HandleDestroyScene 做同样替换。两处后面的 `== entt::null` 判断原样保留。

(2) SceneHandler::CreateScene:把「Idempotent: deduplicate」整块换成

`if (const SceneEntity existing = PlayerSceneSystem::FindSceneById(request->scene_id()); existing != entt::null) { response->mutable_scene_info()->CopyFrom(tlsEcs.sceneRegistry.get<SceneInfoComp>(existing)); LOG_INFO << "CreateScene: scene_id=" << request->scene_id() << " already exists (entity=" << entt::to_integral(existing) << "), returning existing"; return; }`

入口已有的 0 校验保留。

(3) SceneNodeGrpcImpl::HandleCreateScene:先在函数开头补 `if (request->scene_id() == 0) { LOG_ERROR << "[gRPC] CreateScene: scene_id is 0"; return; }`,与 RPC 入口一致,否则替换后 0 号会重复建场景;再按 (2) 替换去重块。

(4) 两个文件按需补 include "player/system/player_scene.h"。scene_handler.cpp 里「Keep this block in sync」的注释暂时保留,等 fit-02 合并时一起删掉。

**补充（来自 W2-cross_sem-08）**

见 W2-b5_types-02。补充:

① SceneNodeGrpcImpl::HandleCreateScene 守护段开头补上与 muduo 入口相同的门禁:`if (request->config_id() == 0 || request->scene_id() == 0) { LOG_ERROR << "[gRPC] CreateScene: config_id/scene_id must be non-zero"; return; }`,然后再把 24-37 的去重换成 FindSceneById。

② 是否把 FindSceneById 从 PlayerSceneSystem 挪到场景域,随 W2-fit-02 的场景目录一并决定,本条不单独挪。

③ 不改 s2s_player_scene_handler.cpp 的前置 CheckEnterScene,也不改 HandleEnterScene 的内部校验。

**回归测试**

不需要先红的回归测试。(1)(2) 是行为等价的去重,改前改后结果一样,写不出修复前会红的用例。唯一的行为差异是 gRPC 入口 scene_id==0 改为拒绝,但目前没有任何测试工程链接 scene_node_service.cpp 或 scene_handler.cpp,测不到。如果负责人选 owner_decision 的 B 方案,把创建/销毁核心收进 libs,可以在已链接 player_scene.cpp 的 cross_zone_test 里加 CreateSceneRejectsZeroSceneId:用 scene_id=0 调两次核心创建函数,断言 tlsEcs.sceneRegistry.view\<SceneInfoComp>().size()==0,且回包没有 scene_info。照搬 gRPC 现行逻辑时这条会红。

<details><summary>复核者给出的拍板选项</summary>

A. 只做本条:替换 4 份扫描,并给 gRPC 建场补 0 校验。改动小,约 30 行,但两套创建/销毁主体(建实体、OnSceneCreated、回滚、排空销毁)仍各写一份,靠注释「手工保持同步」,也仍然测不到。

B. 按 fit-02 一并收口:把创建/销毁核心挪进 libs 的场景系统(例如在 PlayerSceneSystem 旁新增 SceneLifecycle::Create(const CreateSceneRequest&, CreateSceneResponse&) 和 Destroy(uint64_t sceneId)),两个入口只保留各自的 Agones permit 和参数校验,然后直接调用。这样能多删大约 60 行重复主体和同步注释,并且可以单测。

推荐 B。如果 fit-02 的场景目录(O(1) 索引)这次不做,至少先做 A。

</details>

#### T4.4 W2-fit-02（low）场景创建和销毁的业务逻辑在 muduo 与 gRPC 两个入口各写一份（去重、字段组装、排空、销毁，第 1 批又各加一份回滚），只靠「Keep in sync」注释同步，前置校验已经出现分叉。scene_id 扫描部分见 W2-b5_types-02

- 原严重度 medium，复核结论「降级」
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:471-476,cpp/nodes/scene/handler/rpc/scene_handler.cpp:496-559,cpp/nodes/scene/handler/rpc/scene_handler.cpp:569-616,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:19-87,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:90-144,cpp/libs/services/scene/player/system/player_scene.cpp:182-189
- 本单口径：默认选「拍板」中的 (1) 不建 scene_id 索引、(2) 暂不统一两入口的 config_id 校验。

**改法**

按顺序：先按 W2-cross_sem-01 删除事件和回滚，再按 W2-b5_types-02 让入口改用 FindSceneById，最后合并剩下的重复。

1) 新增 cpp/libs/services/scene/world/scene_lifecycle.{h,cpp}（属于 scene.lib，测试能链接），只放两个函数：

- `static const SceneInfoComp& CreateScene(const CreateSceneRequest& request)`：create 实体、emplace SceneInfoComp、写 scene_config_id / scene_id / mirror_config_id / dungeon_config_id / creators，再直接调用 Agones OnUnitCreated。

- `static std::size_t DestroyScene(SceneEntity scene)`：先调 PlayerLifecycleSystem::BeginSceneDrain，返回值>0 就原样返回；否则调 OnUnitDestroyed，再 sceneRegistry.destroy，返回 0。

2) 两个入口只保留各自传输层的东西：

- muduo 入口：非零校验、非阻塞 Agones 许可、日志前缀。

- gRPC 入口：许可仍在 wrapper 里。

- 两边都先 `FindSceneById` 做幂等，命中就回包；未命中才调 `SceneLifecycle::CreateScene` 并 `CopyFrom` 返回值。DestroyScene 同理：先 FindSceneById，找不到就按各自的日志返回，找到再调 `SceneLifecycle::DestroyScene` 并按返回值打日志。

3) 删除 scene_handler.cpp:514-516 与 :600-601 两段「Keep in sync」注释，以及 scene_node_service.cpp 里重复的字段组装和排空长注释（长注释挪进 scene_lifecycle.cpp 的 DestroyScene）。

4) 不加 scene_id 索引，不建 SceneDirectory 类。

5) 不要顺手把非零校验加到 gRPC 路径：Go 侧不校验 config_id，统一前需另行确认（见 owner_decision）。

**回归测试**

纯结构合并，行为不变，写不出修复前「行为红」的用例。新函数在修复前不存在，只会编译失败，不算回归证据。替代验证：在 cross_zone_test（链接 scene.lib）新增契约用例 SceneLifecycleTest：

(a) 同一 scene_id 经入口逻辑创建两次，sceneRegistry 里只有一个带 SceneInfoComp 的实体；第二次回包与第一次相同。

(b) 场景里放一名住户，DestroyScene 返回>0 且实体仍在。

(c) 空场景 DestroyScene 返回 0，之后 FindSceneById 返回 null，Agones UnitCount 回到创建前。

请 Codex 在合并前后各跑一次 cross_zone_test 全量，作为等价性证据。

<details><summary>复核者给出的拍板选项</summary>

(1) 是否为 scene_id 建索引：建索引要靠 on_destroy 维护第二份事实源，换来 O(1) 查找；不建则保持线性扫描。目前没有压测或 profiling 数据显示扫描是瓶颈，推荐不建，等有数据再说。(2) 两个入口的前置校验是否统一：统一后 gRPC 路径也会拒绝 config_id=0，如果 Go 有合法的 config_id=0 场景就会被误拒；不统一则保留现有分叉。推荐先保留分叉，并在 scene_lifecycle.h 注释里写明，再由 Go 侧确认 config_id 是否恒为非零后统一。

</details>

### T5 Kafka 入口：必需命令直接调用、白名单测试与一次性迁移代码

改生成器(tools/proto_generator/protogen)与模板后必须重生成；生成产物不手改。另一会话在改 gRPC 模板，重生前确认对方模板已定型，重生后只核对本组相关产物的差异。

#### T5.1 W1-16（medium）Kafka 入口的 13 个必需命令（4 个 scene、9 个 gate，外加 gate 信封回退）仍经 tlsEcs.dispatcher.trigger 转发；没有监听者时静默丢弃，却照样返回 kDispatched。这是任务单 4.4 定下的写法，生产路径目前到不了丢消息的情形

- 原严重度 medium，复核结论「降级」；合并：W2-cross_sem-03
- 位置：tools/proto_generator/protogen/internal/generator/cpp/service_register_info.go:363-384,tools/proto_generator/protogen/internal/template/rpc_event_registry.cpp.tmpl:53-75,tools/proto_generator/protogen/internal/template/service_header.tmpl:37-39,tools/proto_generator/protogen/internal/template/gate_kafka_command_router.cpp.tmpl:43,tools/proto_generator/protogen/internal/template/event_handler.cpp.tmpl:8,cpp/nodes/scene/handler/event/scene_kafka_command_router.cpp:19-24,cpp/generated/rpc/service_metadata/rpc_event_registry.cpp:2101-2140
- 本单口径：默认选 A，并与 W2-cross_sem-03 合并为一次生成器改动：只为白名单事件生成 handler，路由直接调用，Register/UnRegister/RegisterNodeEvents 整套删除。负责人若否决（改动约 8–12 个文件），退回 C：只在设计文档记录风险，本步不做。

**改法**

删掉 dispatcher 这一跳，改为在节点本地直接调用已生成的静态 handler，不新增任何防护：

1. 从共享 rpc 库删除派发：删 rpc_event_registry.cpp.tmpl:53-75 的 DispatchSceneCommandEvent / DispatchGateCommandEvent；删 service_header.tmpl:37-39 的 KafkaEventDispatchResult 枚举和两个声明；删 service_register_info.go:363-384 拼装 sceneDispatchCases / gateDispatchCases 的循环，以及 ServiceInfoCppData 的 SceneDispatchCases / GateDispatchCases 字段。

2. gate：gate_kafka_command_router.cpp.tmpl 的载荷分支（:27-33）改成按白名单事件生成 switch，每个 case 写 `contracts::kafka::X event; if (!event.ParseFromString(command.payload())) { LOG_WARN ...; return; } GateEventHandler::XHandler(event); return;`。信封回退 :43 的 `tlsEcs.dispatcher.trigger(event);` 改为 `GateEventHandler::{{.EventMessageName}}Handler(event);`。类名和方法名沿用 event.go 生成 handler 时的 ClassName / HandlerName 规则。

3. scene：仿照 gate，新增 scene_kafka_command_router.cpp.tmpl，生成到 cpp/nodes/scene/handler/event/scene_kafka_command_router.cpp，替换现有的手写文件。switch 内直接调用 BattleEventHandler::BattleSettlementEventHandler / BattleConfirmedEventHandler、TeamEventHandler::PlayerTeamRefreshEventHandler、PlayerInboxEventHandler::PlayerInboxNotifyEventHandler；default 分支记 LOG_ERROR（not allowed）。

4. event.go 组装 event_handler.cpp.tmpl:8/:15 的 Register/UnRegister 列表时，跳过 Ingress != eventIngressNone 的事件。入口事件不再 connect/disconnect sink，handler 守护段的内容不变。

5. IsSceneCommandEvent / IsGateCommandEvent 白名单头文件保留，路由开头的白名单判定保留。

**补充（来自 W2-cross_sem-03）**

优先删除，不加防护。(1) event.go 新增 `func kafkaIngressMessages(protoRelativeDir string, file os.DirEntry, messages []string) []string`：按 event_id.go:91-95 的方式拼出 protoInclude，从 globalProtoEventList 里挑出 ProtoInclude 相同且 Ingress != eventIngressNone 的 MessageName，保持 proto 中的顺序。该列表在 pipeline 中已由 ReadAllProtoEvents 在 GenerateAllEventHandlers 之前填好。(2) generateEventHandlerFiles 在 :206 之后执行 `eventMessages = kafkaIngressMessages(...)`；结果为空就直接 return，不写 .h/.cpp。(3) GenerateAllEventHandlers 在 :319 和 :333 之前，先把 logicEventFiles 和 kafkaContractFiles 过滤成“至少含一个入口事件”的文件列表；spawn 协程和调用 GenerateEventHandlersTemplate（:344、:352）都只用过滤后的列表，汇总 Register 与生成文件保持一致。(4) 生成器不会删除陈旧产物，重生后要手删：cpp/nodes/gate/handler/event/match_event_handler.{h,cpp}、player_event_handler.{h,cpp}，以及 cpp/nodes/gate/CMakeLists.txt:66、:76，gate.vcxproj:25、:27、:45、:49，gate.vcxproj.filters 中对应行；scene 侧删 player_migration_event_handler.{h,cpp}，以及 cpp/nodes/scene/CMakeLists.txt:89、scene.vcxproj:202、:231、scene.vcxproj.filters 中对应行。gate_event_handler 重生后会自动去掉 PlayerDisconnectedEvent 的 sink 和空 handler。(5) 若负责人采纳 W1-16 方案 A（路由直接调用生成的静态 handler），本条过滤之后剩下的全是入口事件，Register 列表就是空的。这时不要再按 W1-16 第 4 步去“跳过入口事件”，而是直接删掉 event_handler.cpp.tmpl / event_handler.h.tmpl 里的 Register/UnRegister、event_handler_total.\*.tmpl 的汇总，以及 node_entry.h:26/:85 的 RegisterNodeEvents，handler 只作为静态函数供路由调用。(6) 可选：按 player_migration_event.proto 文件头的说明删除该 proto，27/41 号转为墓碑（2 个，未超过 maxNewTombstonesPerRun=4）。

**回归测试**

工程：tools/proto_generator/protogen（go test ./internal/generator/cpp/...）。新增用例 TestKafkaIngressDispatchCallsHandlerDirectly：用 installGateRouterDescriptors 装入一个标注了入口的 AllowedEvent，渲染 gate_kafka_command_router.cpp.tmpl 和新的 scene 路由模板，断言输出包含 `GateEventHandler::AllowedEventHandler(event)`，且不包含 `dispatcher.trigger`。再渲染 event_handler.cpp.tmpl，断言入口事件不出现 `sink<...AllowedEvent>().connect`。修复前，模板 :43 和生成的 case 都是 `tlsEcs.dispatcher.trigger(event)`，断言会红。同时，现有 TestGateKafkaRouterEmptyWhitelistStillRendersRejectingOutput 里“不含 tlsEcs.dispatcher.trigger(event)”的断言需同步改为“不含任何 Handler(event) 调用”。

<details><summary>复核者给出的拍板选项</summary>

任务单 4a-4 原本规定保留 trigger，改成直接调用会扩大第 4 批的范围，涉及生成器、4 个模板、scene/gate 路由和生成器测试，约 8-10 个文件。选项 A：按上面的修法改为节点本地直接调用（推荐）。dispatcher 不再承担必需命令，gate 的 Kafka 入口也不再依赖 tlsEcs，代价是一次生成器改动和重生成。选项 B：保留 trigger，在前面判 sink().empty() 并返回 kNoHandler。改动小，但等于再叠一层防护，和“优先删除”的偏好相悖。选项 C：维持现状，只在设计文档里记录风险。生产路径目前可达性为零，但返回值继续名不副实。推荐 A。

</details>

#### T5.2 W1-17（medium）白名单单测把上界写死为 49，没有覆盖已进入白名单的 49 号 PlayerInboxNotifyEvent，也拦不住以后把新事件号（≥49）误标进白名单

- 位置：cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:13,cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:25,cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:32

**改法**

kafka_command_whitelist_test.cpp：

1. 加 `#include "rpc/service_metadata/rpc_event_registry.h"`，只用其中的 kMaxEventCount。

2. 第 13 行改为 `constexpr std::array<uint32_t, 4> kSceneEvents{42, 45, 48, 49};`，注释补一句“49 = PlayerInboxNotifyEvent（player_inbox 在线提醒）”。

3. 第 25 行、第 32 行改为 `for (uint32_t eventId = 0; eventId <= kMaxEventCount; ++eventId)`，循环会覆盖 kMaxEventCount 本身，期望值仍由 Contains 给出（为 false）。

4. 保留现有的 UINT32_MAX 断言。不加 static_assert 之类的额外防护：循环上界跟随 kMaxEventCount 增长，已经能拦住新误标的号。

**回归测试**

工程：cpp/tests/kafka_command_test（kafka_command_test.vcxproj）。用例：KafkaCommandWhitelist.SceneAcceptsOnlyItsPublishedIngressEvents / GateRejectsInternalAndOtherTopicEvents。步骤：按修法改好上界后，先不把 49 加入 kSceneEvents，运行测试，应在 eventId=49 处红（期望 false，实际 true），以此证明新上界真的覆盖到 49。再加入 49，应变绿。变异验证：在生成的白名单头文件里临时给 IsGateCommandEvent 加 `case 49u:`，旧测试仍是绿的，新测试会红。

#### T5.3 W1-18（medium）一次性的事件迁移逻辑被固化成生成器每轮必跑的步骤：用正则改写三份手工维护的构建清单，并以 attribute_sync_option 为锚点硬编码补登 event_ingress_option。迁移早已完成，这段逻辑现在只剩新增 Fatal 的副作用

- 位置：tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:84-87,tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:90,tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:141,tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:157-192,tools/proto_generator/protogen/cmd/pipeline.go:46,tools/proto_generator/protogen/internal/prototools/local_event_cleanup_test.go:83-134
- 本单口径：默认选 A：整步删除 local_event_cleanup。

**改法**

首选：整步删除。

- 删除 tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go 和 local_event_cleanup_test.go；

- 删除 cmd/pipeline.go:46 的 `runner.RunTaskWithWG("prototools.PruneOrphanedLocalEventOutputs", ...)`；

- 清单条目已在 CMakeLists.txt:88、proto.vcxproj:91/211、filters:330/671，此后按 AGENTS §4 惯例手工维护。

如果负责人要保留常驻的陈旧产物清理，应在 prototools/builder.go 的 copyProtoToDir（:66-75）里，复制前对 destDir 执行 os.RemoveAll，做成通用清理，与 generator/go/unified.go:33 同法；清单仍由人工维护。不保留只针对 common/event 的特例。

最小替代（不推荐）：只删 :84-87、:90 的 `optionErr == nil` 实参、:141 的 addIngressOption 形参，以及 :157-192 整段，同时删掉 local_event_cleanup_test.go 中对 event_ingress_option 的断言（:83-134 的相关部分）。

**回归测试**

整步删除时不需要“修复前会红”的单测：被删的代码没有剩余行为可测。验证方式交给 Codex：go test ./internal/prototools/... 通过；执行 proto-gen-build 和 proto-gen-run（no-unity 配置）后，git status 中 cpp/generated/proto/CMakeLists.txt、proto.vcxproj、.filters 零差异。如果采用最小替代，在 local_event_cleanup_test.go 新增 TestSyncLocalEventProjectToleratesOptionHeaderInCMake：调用 `syncLocalEventProject("CMakeLists.txt", "  common/options/event_ingress_option.pb.cc\n  common/options/event_ingress_option.pb.h\n", nil)`，断言 err == nil 且返回值与输入相同。修复前，:188-190 的计数为 2 != 1，会返回“登记不完整或含未识别条目”，用例会红。

<details><summary>复核者给出的拍板选项</summary>

是否保留生成器的常驻陈旧产物清理。选项 A：整步删除（推荐）。与仓库“生成器不删陈旧产物、清单手工维护”的现行做法一致，代价是以后删 proto 时要手工清理产物和清单。选项 B：在 copyProtoToDir 复制前对 staging 目录 RemoveAll，做成通用清理，只覆盖 staging 副本，清单仍手工维护，改动约 1 个函数。选项 C：只删 addIngressOption 段，保留 common/event 的正则清单改写，清单仍有两套事实源。推荐 A；若担心 staging 副本残留，可再加 B。

</details>

#### T5.4 W1-19（low）已删除的 DispatchProtoEvent 和 SceneEventHandler 仍被注释与规范引用，任务单 4.4 第 6 条的零命中门禁没有达成

- 位置：cpp/nodes/scene/handler/event/battle_event_handler.cpp:21, cpp/nodes/scene/handler/event/team_event_handler.cpp:19, cpp/nodes/scene/handler/event/player_inbox_event_handler.cpp:19, cpp/libs/services/scene/player/system/player_team.cpp:38, go/login/internal/svc/gate_command.go:106, tools/proto_generator/protogen/internal/generator/cpp/event_id.go:455, cpp/AGENTS.md:69, docs/design/team-system.md:1603, docs/design/team-system.md:1932, docs/design/scene-enter-session-fence.md:126, docs/design/agones-scene-node-high-density.md:135

**改法**

只改注释和文档，不加任何代码：

1. battle_event_handler.cpp:21、team_event_handler.cpp:19、player_inbox_event_handler.cpp:19：在 ///\<\<\< BEGIN WRITING YOUR CODE 守护段内，把“DispatchProtoEvent”改为“DispatchSceneCommandEvent(白名单)”。

2. player_team.cpp:38：改为“由 scene_kafka_command_router.cpp 经 DispatchSceneCommandEvent 白名单解包后进程内分发到 TeamEventHandler”。

3. go/login/internal/svc/gate_command.go:106：改为“gate 的生成路由对非空 payload 走 DispatchGateCommandEvent(白名单含 BindSessionEvent=32)”，后面“事件已在注册表里”一句改为“事件已在 gate 白名单里”。

4. event_id.go:455：改为“墓碑号不进白名单，旧节点发来的墓碑号在 Dispatch\*CommandEvent 返回 kNotAllowed 并记 ERROR 日志”。

5. cpp/AGENTS.md:69：改为“Scene room counting hooks only `OnSceneCreatedHandler` / `OnSceneDestroyedHandler` in `nodes/scene/handler/local_event/scene_local_events.cpp`”，后半句不变。

6. docs/design/team-system.md:1603、:1932，scene-enter-session-fence.md:126，agones-scene-node-high-density.md:135-136：同步改为新函数名和新文件路径。

验收：git grep -n DispatchProtoEvent -- cpp go tools 结果为零；git grep -n SceneEventHandler -- cpp/AGENTS.md docs/design 结果为零。

**回归测试**

不需要：改的全是注释和设计文档，没有可执行行为。验收改用上面两条 git grep 零命中检查。

#### T5.5 W1-20（low）两份节点模板和模板 README 没有跟上新 hook 契约：只声明 KafkaCommandType 而缺 DispatchKafkaCommand，照抄就会撞上 static_assert，也看不出该接哪个白名单

- 位置：cpp/nodes/_template/main.simple.cpp.example:14, cpp/nodes/_template/main.with_context.cpp.example:19, cpp/nodes/_template/README.md:32, cpp/libs/engine/core/node/system/node/node_entry.h:87

**改法**

1. 在 main.simple.cpp.example 和 main.with_context.cpp.example 的 YourNodeHooks 里，KafkaCommandType 下方加上：

    // 必须显式指定派发函数：路由里只能调用生成的白名单派发(Dispatch\<入口>CommandEvent)，不得直接 tlsEcs.dispatcher.trigger。

    // 新节点类型没有现成入口时，先在 protogen event_ingress.go 增加 eventIngress 枚举项与文件位置规则，再生成派发函数。

    static void DispatchKafkaCommand(const std::string& topic, const KafkaCommandType& command) { DispatchYourNodeKafkaCommand(topic, command); }

   并在文件头加 #include "handler/event/your_node_kafka_command_router.h"。

2. README.md 的 THooks Convention 代码块整段换成 node_entry.h:33-43 的真实形状：TableLoadHandler、KafkaCommandType + static DispatchKafkaCommand、ClientEndpointSourceFactory；删除不存在的 EventHandlerType 和 struct KafkaHandler。checklist 第 22 行改为“Kafka command proto and DispatchKafkaCommand in YourNodeHooks”。

**回归测试**

不需要：.example 和 README 不参与任何构建目标，属于文档类改动。真正的契约已经由 node_entry.h:87 的 static_assert 在编译期守住，scene 和 gate 两个真实节点都已满足。

#### T5.6 W1-21（low）控制面 topic 文档里的 scene 白名单数量和生成状态都已过时；事故文档 F001/F002 没有按任务单 4.5 第 2 条改为指向新方案

- 位置：docs/design/control-plane-topic-partitioning-20260908.md:312, docs/design/control-plane-topic-partitioning-20260908.md:314, docs/ops/incident-scene-ecs-gameplay-20261014.md:37, docs/ops/incident-scene-ecs-gameplay-20261014.md:83, docs/ops/incident-scene-ecs-gameplay-20261014.md:130

**改法**

1. control-plane 文档 :312 改为“当前开放 scene 的结算、结算确认、队伍刷新、收件箱提醒 4 个事件（42/45/48/49）”，gate 部分不动。

2. :314 改为“生成产物已随 526780b4e/de4960bee 落盘；编译与运行验收待执行”。

3. 事故文档 :37 状态列改为“源码已按 OptionKafkaIngress 白名单收口（见 control-plane-topic-partitioning-20260908.md §9「2026-10-15 事件入口边界」），运行验收待做”。

4. :83 改为描述现状：入口为生成的 DispatchSceneCommandEvent / DispatchGateCommandEvent，未标注事件返回 kNotAllowed 并记 ERROR；删掉指向已删除默认派发的描述。

5. :130 的复选框保持未勾，文字改为“运行验收：非法事件被拒、BindSessionEvent 等合法入口无遗漏”。

附带的测试修法（如果另立条目）：kafka_command_whitelist_test.cpp 中 kSceneEvents 改为 std::array\<uint32_t,4>{42,45,48,49}，两个循环上界从 49 改成一个远大于当前最大号的常量（如 256），让以后新开放的事件必须同步更新快照。

**回归测试**

本条不需要：只改文档，没有可执行行为。附带发现若另立条目，则在 kafka_command_test 工程的 KafkaCommandWhitelist.SceneAcceptsOnlyItsPublishedIngressEvents 中把循环上界改为 256、kSceneEvents 补上 49。修改上界但不补 49 时，eventId=49 断言会红（白名单为 true、快照为 false），可以证明旧测试漏掉了 49。

### T6 技能与 Buff：施法链只在定时器入口复核、上下文与冷却、Buff 待删标记

依赖 T3.6(W1-22 把沉默改为直接调用之后，Buff 的重入入口才消失）。顺序 T6.1 → T6.2，其余不分先后。skill.cpp 与 buff.cpp 无其他会话在途改动时直接改。

#### T6.1 W2-b1_runtime-01（medium）落点时目标已死,被 CalculateSkillDamage 的 false 当成数据缺失,整次施法被中止。对尸体施法、或目标在前摇期间被打死时,会跳过效果和后摇;这与改造前、与致命一击路径、与 Java 版都不一致

- 原严重度 high，复核结论「降级」；独立二审：降级 / medium
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:790-843,cpp/libs/services/scene/combat/skill/system/skill.cpp:926-929,cpp/libs/services/scene/combat/skill/system/skill.cpp:873-878,cpp/libs/services/scene/combat/skill/system/skill.cpp:99-108
- 本单口径：两位复核者一致：成立，从 high 降为 medium。本单默认选 A（恢复改造前语义，零伤害，效果和后摇照常）。

**改法**

只删不加:

1) 删除 CalculateSkillDamage 中 802-824 的三段:valid 检查、PlayerFrozenComp 检查、IsTargetDead 检查。删后命中尸体照常计算伤害,由 DealDamage:876 现有的死亡守卫落成零伤害,OnSkillExecuted、TriggerSkillEffect、后摇照常执行,与基线和 Java 版一致。CalculateSkillDamage 剩下的 false(无上下文、无表行、无 LevelComp)都是真正的不变量破坏,926 的 Abort 语义随之正确。

2) HandleSkillSpell 908-910 的注释还引用「CalculateSkillDamage 里对 PlayerFrozenComp 的那段」,那段已删,注释同步删改。

3) 不在 HandleSkillSpell 新增 IsTargetDead 分支。

**回归测试**

skill_test 工程,新增 SkillUtilTest.GeneralSkillOnDeadTarget_StillEntersRecovery。

准备:scene;caster 与 target 都用 CreateLoadedPlayerInScene;tlsEcs.actorRegistry.get\<BaseAttributesComp>(target).set_health(0)。

前提断言:1 号技能 recovery_time()>0,且 IsSkillOfType(1,kGeneralSkill)。

仿照 GeneralSkill_PhaseComponentsEndWithTheirPhase 手工给 caster emplace SkillContextCompMap,写入一条上下文:skillid=91,skilltableid=1,caster=to_integral(caster),target=to_integral(target)。

同步调用 SkillSystem::HandleGeneralSkillSpell(caster, 91),不跑 loop。

断言:try_get\<RecoveryTimerComp>(caster) 非空且 skillId==91;get\<SkillContextCompMap>(caster).contains(91);target 血量仍为 0。

修复前 CalculateSkillDamage 返回 false → AbortSkillCast,上下文被删、没有 RecoveryTimerComp,用例会红。

<details><summary>复核者给出的拍板选项</summary>

对尸体施法应该得到什么结果,需要拍板。

A(推荐):恢复改造前语义,零伤害,效果和后摇照常走。只删三段代码,与 Java 版一致,ECS 批不夹带玩法变更。

B:死目标不结算伤害和 buff,但仍进后摇。在 HandleSkillSpell 和 TriggerSkillEffect 加 IsTargetDead 早退(不 Abort),属玩法变化,Java 版要同步。

C:施法期在 ValidateTarget 就拒绝尸体目标,不消耗冷却。这会改变客户端可见的拒绝码,两版要同批改,也会改变机器人压测的拒绝分布。

推荐先做 A;B / C 作为玩法议题另行决定。

</details>

#### T6.2 W2-b1_runtime-04（medium）一次普通技能施放要做 6+N 次 CanContinueSkillCast,外加 2+N 次 HasRequiredTargetComps(1/2 号技能有 8 个效果,即 14+10 次);CheckState 还重复了 348 行的 ValidateSkillUsage。只有定时器回调入口需要复核。描述成立,但原修法需更正:TriggerSkillEffect 不需要返回 bool,388 的复核应移到 HandleChannelFinish,还要删掉一条随之失效的测试

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:372-384,cpp/libs/services/scene/combat/skill/system/skill.cpp:387-388,cpp/libs/services/scene/combat/skill/system/skill.cpp:430-452,cpp/libs/services/scene/combat/skill/system/skill.cpp:472-483,cpp/libs/services/scene/combat/skill/system/skill.cpp:694-726,cpp/libs/services/scene/combat/skill/system/skill.cpp:891-892,cpp/libs/services/scene/combat/skill/system/skill.cpp:314-318,cpp/tests/skill_test/skill_test.cpp:743-759
- 本单口径：在 T6.1 之后做（两条都改 HandleSkillSpell / CalculateSkillDamage)。

**改法**

1) HandleSkillSpell:删 892。签名改为 bool,skill.h:54 同步改。918 和 926 处 AbortSkillCast 之后改为 `return false;`;找不到上下文时 return false;其余早退(目标为空、目标 InBattle)和函数末尾 return true。

2) HandleGeneralSkillSpell(372-384)改为四行:

   if (!CanContinueSkillCast(casterEntity, skillId)) return;

   if (!HandleSkillSpell(casterEntity, skillId)) return;

   TriggerSkillEffect(casterEntity, skillId);

   HandleSkillRecovery(casterEntity, skillId);

3) TriggerSkillEffect:删 695、711-714、719-723,仍为 void。

4) HandleChannelSkillSpell:450-452 改为 `if (!HandleSkillSpell(casterEntity, skillId)) return;`。

5) HandleSkillRecovery:删 388。在 HandleChannelFinish 中,481 的 remove 之后、482 之前加 `if (!CanContinueSkillCast(casterEntity, skillId)) return;`。这是移动不是新增,复核只放在定时器回调里。

6) CheckState:删 315,只留 TryPerformAction。

7) 161 行注释改为「只在定时器回调入口复核」。

8) 删除测试 DirectEffectDispatchMissingRequiredDataEndsCastWithoutApplyingBuffs(skill_test.cpp:743-759)。它测的是被删的内部入口复核;真实定时器入口已由三条 DelayedImpact\* 覆盖。

**回归测试**

不需要修复前会红的用例:这是行为不变的删除重构,被删的检查在任何可达路径上结论都与入口那一次相同,构造不出修复前后行为不同的输入。

替代验证:skill_test 中以下用例须保持全绿,它们覆盖保留下来的定时器入口和 918 的中止路径:

- 三条 DelayedImpactMissing{BaseAttributes,Level,DerivedAttributes}\*

- ImpactOnTargetMissingDerivedAttributesDoesNotDamageOrConsumeBonusBuff

- ChannelTickMissingRequiredDataEndsChannelWithoutConsumingBonusBuff

- GeneralSkill_PhaseComponentsEndWithTheirPhase

- HandleChannelFinish_EndsChannelPhase

- 两条 SkillPrecheckPolicyTest

另外,DirectEffectDispatchMissingRequiredDataEndsCastWithoutApplyingBuffs 按修法第 8 条删除。

#### T6.3 W2-b1_runtime-02（low）没有前摇相位的技能类型(开关/激活/普攻/被动)施放后,SkillContext 永不回收。目前没有玩家拥有这类技能,属于潜伏缺陷;改造前就存在,不是第 1 批引入的

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:251-269,cpp/libs/services/scene/combat/skill/system/skill.cpp:646-663,cpp/tests/skill_test/skill_test.cpp:884-896
- 本单口径：默认选 (a)：照旧受理无相位技能，只启动冷却、不建上下文。

**改法**

ReleaseSkill(skill.cpp:251-269)在 CreateSkillContext 之前算 `const bool hasCastPhase = IsSkillOfType(skillRow->id(), kGeneralSkill) || IsSkillOfType(skillRow->id(), kChannelSkill);`,把 258-259 的 CreateSkillContext/AddSkillContext 与 264 的 SetupCastingTimer 一起包进 `if (hasCastPhase) { ... }`。无相位技能不建上下文,与 Java 版 SkillService 一致。不要加「建了再 HandleSkillFinish」的收尾。CheckSkillPrerequisites 已经校验过施法者有效,所以 AddSkillContext 原先兼带的 valid 检查不会丢。

**回归测试**

skill_test 工程,新增 SkillUtilTest.ReleaseNoCastPhaseSkillLeavesNoContext。

准备:scene = tlsEcs.sceneRegistry.create();caster 与 target 都用 CreateLoadedPlayerInScene(scene);request = TargetedRequest(target),request.set_skill_table_id(4)。4 号技能是开关类,targeting_mode 首项为 kTargetedSkill。

前提断言:ASSERT_FALSE(IsSkillOfType(4,kGeneralSkill) || IsSkillOfType(4,kChannelSkill))。

执行与断言:ASSERT_EQ(kSuccess, SkillSystem::ReleaseSkill(caster,&request));EXPECT_FALSE((any_of\<SkillContextCompMap, CastingTimerComp>(caster)))。

修复前 SkillContextCompMap 里留有 1 条,用例会红。

<details><summary>复核者给出的拍板选项</summary>

无相位技能类型(被动/开关/激活/普攻)目前没有任何执行逻辑,ReleaseSkill 要不要受理它们,需要拍板。

选项 (a):照旧受理,只启动冷却、不建上下文。与 Java 版一致,不动协议。

选项 (b):在 ReleaseSkill 直接拒绝。要在 Tip.xlsx 新增码,而且是客户端可见契约,Java 版要同批改。

推荐 (a);(b) 等这类技能真正实现时再定。

</details>

#### T6.4 W2-b1_runtime-03（low）cooldown_id=0(无冷却)的技能也会创建冷却表并写入 0 号条目,之后每次施放都会在 CheckCooldown 里打一条 LOG_ERROR。问题在改造前就有,第 1 批没有让它变坏;新问题只是这种写法违反了组件访问规则「真正启动冷却时才创建」

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:186-199,cpp/libs/services/scene/combat/skill/system/skill.cpp:599-601,cpp/libs/engine/core/time/system/time_cooldown.h:15,cpp/tests/skill_test/skill_test.cpp:239,cpp/tests/skill_test/skill_test.cpp:257

**改法**

1) SkillSystem::StartCooldown:在 valid/nullptr 检查之后、get_or_emplace 之前加 `if (skillTable->cooldown_id() == 0) { return; }  // 0 = 无冷却组,Cooldown 表 id 从 1 起`。

2) CheckCooldown 不改。

3) 改 cpp/tests/skill_test/skill_test.cpp 中 SkillPrecheckPolicyTest::ExpectRejectionPreservesOldCast 的第 239 行和第 257 行:旧施法用的是 2 号技能(cooldown 0),修复后不再有冷却表,把这两行的 get\<CooldownTimeListComp> 快照对比改为 `EXPECT_FALSE(tlsEcs.actorRegistry.any_of<CooldownTimeListComp>(caster));`,在被拒施法前后各断言一次(被拒的 1 号技能同样不得启动冷却)。

**回归测试**

skill_test 工程,新增 SkillUtilTest.StartCooldown_NoCooldownSkillCreatesNothing。

第一段:caster = CreateSkillActor();row2 = SkillTableManager::Instance().FindById(2).first;ASSERT_EQ(0u, row2->cooldown_id());调用 SkillSystem::StartCooldown(caster, row2);EXPECT_FALSE(all_of\<CooldownTimeListComp>(caster))。

第二段:另取 caster2,先 StartCooldown(caster2, FindById(1).first),再 StartCooldown(caster2, row2);EXPECT_FALSE(get\<CooldownTimeListComp>(caster2).cooldown_list().contains(0))。

修复前两段都红:组件被建出来,且写入了 key 0。

#### T6.5 W1-26（low）同一个不可信的 target_id 在一次 ReleaseSkill 里被反查最多 6 次；uint64 版的 ResolveSkillTarget 没有按任务单删掉，ValidateTarget 还手写了一遍与 ValidateSkillTargetEntity 重复的检查

- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:110-112, cpp/libs/services/scene/combat/skill/system/skill.cpp:201-249, cpp/libs/services/scene/combat/skill/system/skill.cpp:251-269, cpp/libs/services/scene/combat/skill/system/skill.cpp:507-534, cpp/libs/services/scene/combat/skill/system/skill.cpp:633-634

**改法**

1) 在 SkillSystem::ReleaseSkill 入口、CheckSkillPrerequisites 之前只解析一次：

   `const entt::entity target = ValidateSkillTargetEntity(casterEntity, actor_net_id::Resolve(tlsEcs.actorRegistry, request->target_id()));`

2) 删除 uint64 版 ResolveSkillTarget（:110-112）。

3) 给 CheckSkillPrerequisites、ValidateTarget、LookAtTargetPosition、BroadcastSkillUsedMessage、CreateSkillContext 都加一个 entt::entity target 参数。ApplySkillHitEffectIfValid 的签名改为 (caster, entt::entity target)。各函数内部删掉自己的解析：

   - :204 改为 `else if (target != entt::null)`；

   - :218 改为 `context->set_target(target == entt::null ? 0 : entt::to_integral(target))`；

   - :243 删除，直接使用参数；

   - :633 改为 `if (target != entt::null)`。

4) ValidateTarget 的 kTargetedSkill 分支，把 :507-534 换成：

   `if (target == entt::null) return MAKE_ERROR_MSG(kSkillInvalidTargetId, ... reason=unresolved_or_not_in_caster_scene_or_frozen); if (any_of<InBattleComp>(target)) return kSkillInvalidTarget; return kSuccess;`

   这样就删掉了与 ValidateSkillTargetEntity 重复的 valid 和 Player/Npc 检查。:528 的注释改为指向 ValidateSkillTargetEntity。

5) skill.h 的声明，以及 skill_test 中直接调用 ValidateTarget、CheckSkillPrerequisites、BroadcastSkillUsedMessage 的地方，同步改为传入先解析好的 target。

**回归测试**

写不出修复前会红的测试：几次解析是同步进行的，结果一致，外部可观察行为不变。

替代验证：

1) skill_test 现有的 ValidateTarget_\* 和 ReleaseSkill 系列用例改签名后全部为绿，尤其是 ValidateTarget_RejectsLocalEntityIntegerAndPreviousNetworkIdentity、TargetInAnotherScene、FrozenTarget。

2) grep 门禁：`rg -c "actor_net_id::Resolve" cpp/libs/services/scene/combat/skill/system/skill.cpp` 为 1；`rg -n "ResolveSkillTarget" cpp` 为 0 行。

#### T6.6 W2-b1_runtime-06（medium）BuffPendingRemoveBuffs 实际上只是 RemoveBuff(set) 一次同步调用内的「正在删除」标记，却作为 registry 组件反复 emplace/remove；它的「帧末移除」API 只有测试在用，表行缺失时还会每帧重试并刷错误日志

- 位置：cpp/libs/services/scene/combat/buff/system/buff.cpp:277-296, cpp/libs/services/scene/combat/buff/system/buff.cpp:309-343, cpp/libs/services/scene/combat/buff/system/buff.cpp:372-377, cpp/libs/services/scene/combat/buff/system/buff.cpp:428-431, cpp/libs/services/scene/combat/buff/system/buff.cpp:795, cpp/libs/services/scene/combat/buff/system/buff.h:42-46, cpp/libs/services/scene/combat/buff/comp/buff_comp.h:29-36, docs/design/ecs-component-access-rules.md:45, docs/design/ecs-component-access-rules.md:95
- 本单口径：**与 T3.6(W1-22)联动**:W1-22 删除事件注入后，Buff 删除回调链里已没有任何路径能回到 AddOrUpdateBuff。先 `git grep -n "AddOrUpdateBuff" -- cpp/libs/services/scene/combat/buff` 核对 OnBuffRemove/OnBuffDestroy 链。若确认不可达，连 removalPending 也不要加：整套删除 BuffPendingRemoveBuffs、MarkBuffForRemoval、RemovePendingBuffs、预标记循环与 HandleExistingBuff 的检查；若找到可达路径，再按下面的 removalPending 写法做。两种做法下本条的回归测试都适用。

**改法**

1) buff_comp.h 的 BuffEntry 新增 `bool removalPending{false};  // 仅运行期：RemoveBuff(set) 本次调用内正在删除，叠层时跳过`。BuffEntry 是 C++ 结构，不是落库的 proto，不违反 §11.6。删除 29-36 行的 BuffPendingRemoveBuffs。2) buff.cpp RemoveBuff(set) 第一轮改为 `if (auto* buffList = tlsEcs.actorRegistry.try_get<BuffListComp>(parent)) if (auto it = buffList->find(removeBuffId); it != buffList->end()) it->second.removalPending = true;`，第二轮不变；278-279 的注释改为「先给整批条目置 removalPending，前一条的移除回调就不会把新施加的 Buff 叠进待删的后一条」。3) 删除 MarkBuffForRemoval（309-326）、RemovePendingBuffs（328-343）、buff.h:42-46 的两个声明、Update:795 的 `BuffSystem::RemovePendingBuffs(target, buffListComp);`，以及 OnBuffExpire:372-377。4) HandleExistingBuff:428-431 改为 `if (buffComp.removalPending) continue;`。5) 表行缺失导致 OnBuffExpire 提前返回时，标记会留在这条删不掉的条目上，条目从此不再参与叠层，也不会每帧重试，这是期望行为，不需要额外处理。6) 测试：buff_test.cpp 删除 391 号 MarkBuffForRemoval_PendingSetLivesOnlyUntilFrameEnd；570 号 PendingOwnerRemoval_RemovesEveryBonusChild 改为 `BuffSystem::RemoveBuff(actor, UInt64Set{id})` 后执行 Update，或者直接删除，因为 OwnerExpires_RemovesEveryBonusChild 已覆盖；747 号改为 `RemoveBuff(actor, UInt64Set{buffId})` 后执行 Update，只断言 BuffListComp 已摘；340 行和 player_feature_persistence_test.cpp:138 去掉 BuffPendingRemoveBuffs。7) docs/design/ecs-component-access-rules.md：删除 45 行的表项，95 行的具名类型列表去掉 BuffPendingRemoveBuffs。W2-b5_types-04 的具名容器清单相应少一个。

**回归测试**

buff_test 新增 BuffListLifecycleTest.RemoveBuffSet_UnremovableEntryLeavesNoExtraStateAcrossFrames。步骤：actor = create()；emplace\<BuffListComp>(actor)[900001].buffPb.set_buff_table_id(一个表里不存在的 id，例如 UINT32_MAX)；调用 BuffSystem::RemoveBuff(actor, UInt64Set{900001})；连续执行两次 BuffSystem::Update(0.0)；然后遍历 tlsEcs.actorRegistry.storage()，统计 contains(actor) 的池数量。断言：数量 == 1（只有 BuffListComp），且 BuffListComp 仍含 900001。修复前，表行缺失使 OnBuffExpire 提前返回，待删集合残留，帧末每次重新挂回，actor 同时在 BuffPendingRemoveBuffs 池里，数量为 2，用例红；修复后为 1，用例绿。这个用例不点名组件类型，修复前后都能编译，也不依赖执行顺序。另外保留 BuffCombatIdleTest.OwnerRemovalCallback_DoesNotConsumeIndependentBuff 和 ResetCallback_DoesNotConsumeIndependentBuff：如果漏改 HandleExistingBuff 的 removalPending 检查，这两条会红，守的是重入叠层语义。

#### T6.7 W2-fit-04（low）目标身上的 BuffEntry 仍存着施法者 SkillContextCompMap 里同一个 shared_ptr，但 buff 侧只读它的 caster()，这个值 buffPb.caster 里已经有。问题是跨实体共享所有权和多余的 API 耦合，没有行为后果

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/services/scene/combat/buff/comp/buff_comp.h:12-18,cpp/libs/services/scene/combat/buff/system/buff.cpp:75-81,cpp/libs/services/scene/combat/buff/system/buff.cpp:214-227,cpp/libs/services/scene/combat/buff/system/buff.cpp:406-416,cpp/libs/services/scene/combat/buff/system/buff.cpp:677-689,cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:131-134,cpp/libs/services/scene/combat/buff/system/buff.h:32,cpp/libs/services/scene/combat/skill/system/skill.cpp:724
- 本单口径：默认：与本组同批做，形状用 uint64 caster（不引入 BuffSource)。

**改法**

不新增 BuffSource，直接把上下文参数换成施法者整数，并删掉 BuffEntry 里的指针。

1) buff_comp.h：从 BuffEntry 删除 `SkillContextPtrComp skillContext;`。头文件改为 include "time/comp/timer_task_comp.h"，不再 include skill_comp.h。

2) 把 buff.cpp:63 匿名命名空间里的 `kNoBuffCaster` 移到 combat/buff/constants/buff.h，注释写「0 = 无施法者，依赖 EcsContext 已退役 0 号句柄」。

3) buff.h / buff.cpp 中 AddOrUpdateBuff(3 参)、AddOrUpdateBuffWithOwner、HandleExistingBuff、OnBuffRefresh、AddTargetSubBuffs 的 `const SkillContextPtrComp& abilityContext` 一律改为 `uint64_t caster`。2 参的 AddOrUpdateBuff 传 kNoBuffCaster，替代 nullptr。

4) buff.cpp 的具体改法：

- ProcessedCasterOf 改为 `return buffRow.no_caster() != 0 ? kNoBuffCaster : caster;`

- :215-218 改为 `newBuff.buffPb.set_caster(caster);`

- 删除 :227 的 `newBuff.skillContext = abilityContext;`

- :414 改为 `if (buffRow->no_caster() == 0 && caster == kNoBuffCaster) return false;`

- :679 改为 `const uint64_t ownerCaster = buffComp.buffPb.caster();`，:688-689 传 ownerCaster

- 改正 :66-72 关于子 buff「没有技能上下文」的注释

5) buff_impl.cpp:131-134 改为 `const uint64_t caster = buff.buffPb.caster();`，再调 `AddTargetSubBuffs(target, buffTable, caster)`。

6) 删除 ModifierBuffImplSystem::OnBuffRefresh 和 MotionModifierBuffImplSystem::OnBuffRefresh 的声明与定义（空函数、零调用）。

7) skill.cpp:724 改为 `BuffSystem::AddOrUpdateBuff(targetEntity, effect, skillContext->caster());`

8) buff_test 里的 ContextFromCaster 改为直接返回 `entt::to_integral(caster)`。:248 的 `SkillContextPtrComp{}` 改为 kNoBuffCaster。

SkillContextCompMap 和 SkillContextPtrComp 别名在本条中不动。

**回归测试**

skill_test，SkillUtilTest，新增用例 FinishedCastIsNotRetainedByTargetBuffs。

步骤：

1. scene = sceneRegistry.create()；caster、target 用 CreateLoadedPlayerInScene 创建。

2. request = TargetedRequest(target)，即 1 号技能，cast_point>0。ASSERT ReleaseSkill 返回 kSuccess。

3. 从 caster 的 SkillContextCompMap 取唯一的 skillId，记下 `std::weak_ptr<SkillContextComp> observer = contexts->at(skillId);`

4. 用现有的 RunUntil 等到 caster 表里不再含这个 skillId（完整走完前摇、落点、后摇和 HandleSkillFinish）。

断言：

- 前置：ASSERT target 身上有 BuffListComp 且非空。若真实表里 1 号技能的 effect 不产生持续 buff，就换一行 effect 带 duration>0 或 infinite_duration 的技能表。

- EXPECT_TRUE(observer.expired())。

修复前，目标 BuffEntry.skillContext 仍持有该指针，observer 未过期，必红；修复后绿。整个用例只用 SkillContextCompMap 和 ReleaseSkill，修复前后都能编译。

另加一条语义护栏（修复前后都绿，不算回归）：buff_test 里「按施法者隔离的 buff 在 caster=kNoBuffCaster 时不叠层，每次新建一条」，防止换成整数后把无施法者当成普通施法者。

<details><summary>复核者给出的拍板选项</summary>

(1) 现在做还是挂起？S4 属于总评里「参考」级的结构改动，当前没有行为缺陷。现在做的代价是改 buff 和 skill 的公开签名及两份测试，约 10 个文件；挂起的后果只是共享所有权的异味继续存在。推荐和第二波其他 buff/skill 修复同批做，因为改动是纯删除，风险低。(2) 形状：按总评用 BuffSource{caster, skillId, skillTableId}，还是直接 uint64 caster？skillId 和 skillTableId 现在没有任何读者。推荐 uint64 caster，以后真要按技能归因时再引入结构体。

</details>

#### T6.8 W2-b5_types-04（low）四个具名容器组件的 explicit 基类转换构造在全仓没有调用方,X() = default 只是因为写了这两个构造才不得不补上

- 位置：cpp/libs/services/scene/combat/buff/comp/buff_comp.h:24,cpp/libs/services/scene/combat/buff/comp/buff_comp.h:33,cpp/libs/services/scene/combat/skill/comp/skill_comp.h:59,cpp/libs/services/scene/spatial/comp/grid_comp.h:18,docs/design/ecs-component-access-rules.md:95
- 本单口径：若 T6.6 删除了 BuffPendingRemoveBuffs，本步只处理剩下三个类型。

**改法**

四个头文件各自缩成只保留继承构造的写法,删掉 Base 别名、`X() = default;` 和两个 explicit 转换构造。例如:

struct BuffListComp : std::unordered_map\<uint64_t, BuffEntry>

{

    using unordered_map::unordered_map;

};

BuffPendingRemoveBuffs(unordered_set)、SkillContextCompMap、SceneGridListComp 照此处理;如果想保留 Base 别名,也只写 `using Base = ...; using Base::Base;` 两行,不要再写默认构造和转换构造。buff_comp.h、skill_comp.h、grid_comp.h 里只为 std::move 新增的 `#include <utility>` 一并删除。docs/design/ecs-component-access-rules.md:95 的「基类容器转换必须显式」改为「不提供从基类容器到组件类型的转换(继承构造天然排除基类拷贝/移动构造)」。

**回归测试**

可选的编译期契约断言。在 cpp/tests/buff_test/buff_test.cpp 文件作用域加:static_assert(!std::is_constructible_v\<BuffListComp, const std::unordered_map\<uint64_t, BuffEntry>&>); static_assert(!std::is_constructible_v\<BuffPendingRemoveBuffs, std::unordered_set\<uint64_t>>); static_assert(!std::is_constructible_v\<SkillContextCompMap, const std::unordered_map\<uint64_t, SkillContextPtrComp>&>); 再加 static_assert(std::is_default_constructible_v\<BuffListComp>) 等,确认隐式默认构造仍在。在 cpp/tests/aoi_test/aoi_system_test.cpp 对 SceneGridListComp 加同样的断言。修复前 explicit 构造让 is_constructible 为 true,static_assert 编译失败;修复后通过。如果负责人认为为删除死代码专门加断言属于过度,可以不加,改用门禁:rg "explicit (BuffListComp|BuffPendingRemoveBuffs|SkillContextCompMap|SceneGridListComp)\(" cpp 应为零命中。

### T7 actor 属性、战斗快照、玩家读写与 actor 网络号

依赖 T3.2(InitializeOnCreate 已改为 void)。T7.1 与 T3.2 都改 InitializeOnCreate，在其后做。

#### T7.1 W2-b1_runtime-05（medium）MoveSpeedComp 在生产代码中没有任何读者，第 1/5 批却把它定成每个 actor 的必需派生组件，配齐了创建、标脏重算、缺失报错和多组测试

- 位置：cpp/libs/services/scene/actor/attribute/comp/actor_attribute_comp.h:12-23, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:34-71, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:113, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:130-133, cpp/libs/services/scene/actor/attribute/constants/actor_state_attribute_calculator_constants.h:6-9, cpp/libs/services/scene/combat/buff/system/modifier_buff_impl.cpp:13-27, cpp/libs/services/scene/spatial/system/movement.cpp:63, cpp/libs/services/scene/spatial/constants/nav.h:7-9, docs/design/ecs-component-access-rules.md:25, docs/design/ecs-component-access-rules.md:57
- 本单口径：默认选 A：整套删除 MoveSpeedComp。

**改法**

1) actor_attribute_comp.h：删除 12-23 行的注释和 struct MoveSpeedComp。2) actor_attribute_calculator.cpp：删除 34-71 行的注释和 UpdateMoveSpeed；kAttributeConfigs 删除 `{kMoveSpeed, UpdateMoveSpeed},`；InitializeOnCreate 删除 130-131 和 133 行，其余按 W2-b1_runtime-09 收成一行 emplace\<CombatStateFlagsComp>；删除后不再用到的 buff_table.h 和 buff_comp.h include 一并去掉。3) actor_state_attribute_calculator_constants.h：删除 kMoveSpeed 枚举项和 6-8 行注释。4) modifier_buff_impl.cpp：OnBuffStart/OnBuffRemove 删掉 MarkAttributeForUpdate(kMoveSpeed) 分支，函数体留空；IsMovementSpeedBuff 若随之没有调用方，连同 modifier_buff_impl.h:28 的声明一起删除。5) movement.cpp:63-65 的注释改为「移速 buff 目前未接入移动；接入时用独立标量缩放输入方向，绝不要把标量灌进 Velocity」，保留 2026-08 P1 的警示，但不再引用 MoveSpeedComp；nav.h:7-9 删掉「改为 max(..., MoveSpeedComp.moveSpeed\*1.1)」这句。6) docs/design/ecs-component-access-rules.md:25/57：把 MoveSpeedComp 从派生数据示例和 InitializeOnCreate 描述中删掉。7) 测试：player_feature_handler_test.cpp 的 333/351 改用 kCombatState 位并预挂 CombatStateFlagsComp，以此验证脏标记生命周期；351 去掉移速断言，改为断言冻结期间 state_flags 不变；369 删掉 MoveSpeedComp 断言；删除 382 号 MoveSpeedFallsBackToZeroWhenNoBuffListIsLeft；player_feature_persistence_test.cpp:147-148 和 927-928 删掉 MoveSpeedComp 断言；937/1013 按 W2-09 删除。

**回归测试**

不需要。理由：删除的是一个没有读者的组件，可达行为（移动、同步、存盘）在修复前后完全一致，写不出修复前会红的行为测试。替代验证：机械门禁 `git grep -n "MoveSpeedComp\|kMoveSpeed\|UpdateMoveSpeed" -- cpp docs/design` 零命中；改写后的 ActorAttributeCalculatorTest.UpdateRemovesTheDirtyFlagComponentOnceConsumed 和 FrozenPlayerKeepsItsDirtyFlagsUntilTheFreezeEnds（改用 kCombatState）保持绿；buff_test 中带移速 buff 的用例保持绿，证明去掉标脏后 buff 生命周期不受影响。

<details><summary>复核者给出的拍板选项</summary>

选项 A（推荐）：现在整套删除，等移动系统真要按移速缩放输入、或反作弊要用移速上限时，随消费者一起加回。后果：buff 表的 movement_speed_boost/reduction 列继续不生效，这与现状相同；警示注释保留在 movement.cpp。选项 B：保留 MoveSpeedComp，但退回基线形态，即只在 UpdateMoveSpeed 中按需写，不进 InitializeOnCreate，不登记为必需派生组件，删掉缺失报错和专项测试。后果：仍是一个没有读者的组件，只是少了生命周期负担。选项 C：本批顺手接通消费者（移动缩放或 nav 反作弊上限）。后果：任务范围扩大，并按 §12 需要 Java 版同步，不建议在 ECS 收尾里做。

</details>

#### T7.2 W2-b1_runtime-08（low）入场落位 EnsureValidEnterLocation 对持久组件 Transform、默认技能 RegisterPlayer 对持久组件 PlayerSkillListComp 仍用 get_or_emplace,RegisterPlayer 还重复做了一次 SanitizeSkillList;生产中这两处都不会真的建出组件

- 原严重度 medium，复核结论「降级」；合并：W2-cross_sem-04
- 位置：cpp/libs/services/scene/spatial/system/scene_spawn.cpp:101,cpp/libs/services/scene/player/system/player_skill.cpp:40-42

**改法**

scene_spawn.cpp 的 EnsureValidEnterLocation:把第 101 行换成 `auto* transform = tlsEcs.actorRegistry.try_get<Transform>(player); if (transform == nullptr) { LOG_ERROR << "[SceneSpawn] Transform missing (load invariant broken); entry location not validated: player_id=" << GuidForLog(player); return false; }`,函数里其余 `transform.` 改成 `transform->`(102、125、132、137 行)。调用方本来就忽略返回值,不需要改动。player_skill.cpp 的 RegisterPlayer:删掉第 40 行的 `SanitizeSkillList(player);`,第 42 行换成 `auto* skillList = tlsEcs.actorRegistry.try_get<PlayerSkillListComp>(player); if (skillList == nullptr) { LOG_ERROR << "[PlayerSkill] PlayerSkillListComp missing; default skills not registered entity=" << entt::to_integral(player); return; }`,后面的 `skillList.` 和 `AddSkillIfMissing(skillList, ...)` 相应改为 `skillList->` 和 `*skillList`。W2-cross_sem-04 与本条同根,以本条为主条目。

**补充（来自 W2-cross_sem-04）**

见 W2-b1_runtime-08(scene_spawn.cpp:101 改为 try_get,缺失时 LOG_ERROR,不创建)。补充:

① 在 PlayerSceneSystem::HandleEnterScene 的 CheckEnterScene(player_scene.cpp:216-220)之后、离开旧场景(步骤 1)之前加一句 `if (!tlsEcs.actorRegistry.all_of<Transform>(player)) { LOG_ERROR << "HandleEnterScene: persistent Transform missing, " << ActorLog(player); return false; }`,在任何副作用之前 fail-closed。

② EnsureValidEnterLocation 内:`auto* transform = tlsEcs.actorRegistry.try_get<Transform>(player); if (transform == nullptr) { LOG_ERROR << ...; return false; }`,后续把 `transform.` 改成 `transform->`。

③ scene_spawn.h:38-39 注释补一句“Transform 是持久组件,调用方须先确认存在;缺失时只记错误,不创建、不改写,返回 false”。

**回归测试**

(1) 工程 bag_test,cpp/tests/bag_test/scene_spawn_test.cpp 新增 TEST_F(SceneSpawnTest, MissingTransformIsNotCreatedOnEntry):SelectScene(2),不调用 SetPosition,先 ASSERT_FALSE(tlsEcs.actorRegistry.any_of\<Transform>(player)),再 EXPECT_FALSE(SceneSpawnSystem::EnsureValidEnterLocation(player, scene)),最后 EXPECT_EQ(nullptr, tlsEcs.actorRegistry.try_get\<Transform>(player))。修复前 get_or_emplace 会造出 (0,0,0) 的 Transform,并因不在网格上被改写到出生点,返回 true、组件存在,两处断言都会红。(2) 工程 skill_test,cpp/tests/skill_test/skill_test.cpp 新增 TEST(PlayerSkillSystemTest, RegisterPlayerDoesNotCreateMissingSkillList):先 create 一个裸实体,调用 PlayerSkillSystem::RegisterPlayer(player),断言 EXPECT_FALSE(tlsEcs.actorRegistry.any_of\<PlayerSkillListComp>(player)),最后 destroy。修复前 get_or_emplace 会补出技能表,必红。

#### T7.3 W2-b1_runtime-10（low）StoreBattleFreezeForWrite 只是一行透传的 helper；它把三处先确认不存在、再 emplace 的业务创建改成了静默复用的 get_or_emplace，丢掉了 EnTT 对重复创建的断言

- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:141-146, cpp/libs/services/scene/battle/system/player_battle.cpp:1215, cpp/libs/services/scene/battle/system/player_battle.cpp:1550, cpp/libs/services/scene/battle/system/player_battle.cpp:1637, docs/design/ecs-component-access-rules.md:24, docs/design/ecs-component-access-rules.md:55
- 本单口径：规则文档部分并入 T11。

**改法**

1) 删除 player_battle.cpp:141-146 的注释和 StoreBattleFreezeForWrite。2) 1215 改为 `auto& inBattle = tlsEcs.actorRegistry.emplace<InBattleComp>(player);`，1214 的注释保留「上面已确认没有冻结且快照完整」。3) 1550 改为 `tlsEcs.actorRegistry.emplace<InBattleComp>(player, rebuilt);`。4) 1637 改为 `tlsEcs.actorRegistry.emplace<InBattleComp>(player, rebuilt);`。5) docs/design/ecs-component-access-rules.md:24：从按需状态的示例列表里去掉 InBattleComp，或在表下注明「由业务事件创建的状态，可以在多个业务入口各自先确认不存在、再 emplace，不必收口成一个首写函数」；55 行补一句「三处入口各自先确认不存在、再用 emplace」。docs/handoff/ecs-refactor/README.md:114 是历史回执，不改；如需要，可在收尾补充里注明 helper 已删除。

**回归测试**

不需要。理由：三个调用点都已同步确认不存在，换回 emplace 后可达行为完全一致，写不出修复前会红的行为测试；改动只恢复了调试期断言，删掉了一层间接调用。替代验证：player_battle_settlement_test 和既有 PrepareBattle 用例（有冻结时拒绝、无冻结时挂上 PREPARING）保持绿；机械门禁 `git grep -n "StoreBattleFreezeForWrite\|get_or_emplace<InBattleComp>" -- cpp` 零命中。

#### T7.4 W2-cross_sem-05（low）BuildBattleSnapshot 在 all_of 前置校验之后，仍保留 7 处“缺组件就给默认值或跳过”的判空分支，注释也写成了“已确认存在”；结算路径和属性系统里有同样的死分支

- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:925-933, cpp/libs/services/scene/battle/system/player_battle.cpp:945-956, cpp/libs/services/scene/battle/system/player_battle.cpp:966-986, cpp/libs/services/scene/battle/system/player_battle.cpp:999, cpp/libs/services/scene/battle/system/player_battle.cpp:1097, cpp/libs/services/scene/battle/system/player_battle.cpp:1770-1792, cpp/libs/services/scene/player/system/player_attribute.cpp:68-76

**改法**

player_battle.cpp BuildBattleSnapshot，在 all_of 之后取 const auto& registry = tlsEcs.actorRegistry，然后：

- 945-956：改为 const auto& profile = registry.get\<PlayerProfileComp>(player)，再用 profile.name()、profile.appearance_id()、profile.gender() 直接赋值；snapshot.set_class_id(registry.get\<PlayerUint32Comp>(player).class_())；const uint32_t level = registry.get\<LevelComp>(player).level()，再 snapshot.set_level(level > 0 ? level : 1)。保留“名字字段可为空”的说明，删掉“组件已确认存在”这半句。

- 968-986：改为 const auto& derived = registry.get\<DerivedAttributesComp>(player)；maxHealth 和 maxMana 只保留 max_health()==0、max_mana()==0 的数据兜底；拆掉 979 的 if (derived != nullptr) 包裹。

- 999：改为 const auto& skillList = registry.get\<PlayerSkillListComp>(player)，去掉外层 if。

- 1097：改为 const auto& bags = registry.get\<PlayerBagsComp>(player)，去掉外层 if。BuffListComp 是按需组件，那里的 try_get 保留不动。

- 同文件 1770-1792：derived 改为一次 get 取引用；删掉 derived != nullptr 和 derivedForRevive != nullptr 判空，以及“取不到才退回职业初值”的注释。

player_attribute.cpp 68-76：PlayerLevel 改为 const auto level = tlsEcs.actorRegistry.get\<LevelComp>(player).level(); return level > 0 ? level : 1;。PlayerClassId 改为 return tlsEcs.actorRegistry.get\<PlayerUint32Comp>(player).class_();。两个函数上方各写一行“调用方均已过 CheckAttributeInputs”。

**回归测试**

不需要新增修复前会红的用例：这是行为不变的死分支删除，构造不出修复前为红的场景。回归保护靠现有用例：bag_test 的 PlayerBattleSettlementTest.SnapshotRejectsMissingPersistentOrDerivedInputsBeforeChangingOutput 逐个移除 7 个组件，断言返回 kServiceUnavailable、快照不被改写；结算用例 MissingDerivedAttributesRejectsBeforeCurrencyAndCanRetryAfterRecovery 覆盖 1715 的预检；属性面板和重算的现有用例覆盖 CheckAttributeInputs。改完后这三组都应保持绿。

#### T7.5 W2-b1_runtime-11（low）PetSystem::BuildBattleSnapshot 的注释说缺组件时玩家照常进战斗,实际唯一调用方已先 fail-closed;同一个 false 被同时解释成「没有出战宝宝」和「缺持久组件」

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/player/system/player_pet.cpp:931,cpp/libs/services/scene/player/system/player_pet.h:32,cpp/libs/services/scene/player/system/player_pet.h:111

**改法**

只改注释,代码不动。player_pet.cpp:931 改为:「// 缺 PlayerPetComp 是加载故障(FindComp 已记 ERROR)。本函数的 false 分不清『缺组件』和『没有出战宝宝』,所以调用方必须先确认组件存在;唯一调用方 PlayerBattleSystem::BuildBattleSnapshot 已在前置 all_of 中以 kServiceUnavailable 拒绝开战。」player_pet.h:32 的「BuildBattleSnapshot 答 false(不带宝宝)」改为「BuildBattleSnapshot 答 false,调用方须先校验 PlayerPetComp 存在(见 PlayerBattleSystem::BuildBattleSnapshot)」。player_pet.h:111 改为「出战宝宝的战斗快照。没有出战宝宝返回 false;调用前须已确认 PlayerPetComp 存在,缺组件同样返回 false,不能据此当作正常开战」。不要改成 get\<PlayerPetComp>。

**回归测试**

不需要:只改注释,行为不变。现有 PetComponentOwnershipTest.ReadAndRecalculateEntriesCreateNothing 已经守住「缺组件返回 false、不补建」的行为。

#### T7.6 W2-cross_sem-10（low）任务列表读路径在持久组件 MissionsContainerComp 缺失时仍返回 kSuccess，并下发整份“全部未接取”的配置目录；现有测试还把这个行为固化成了契约，与“面板 / 列表缺持久输入返回故障码”的规则冲突

- 位置：cpp/libs/services/scene/player/system/player_feature_snapshot.cpp:187-213, cpp/tests/bag_test/player_feature_snapshot_test.cpp:152-166, docs/design/player-features-ui.md:39
- 本单口径：默认选 A：保留 state_persistent 字段，已加载玩家恒写 true。Java 版按同一口径核对并在 PARITY.md 登记。

**改法**

PlayerMissionReadSystem::BuildList 在 valid 判断之后加：if (container == nullptr) { LOG_ERROR \<\< "[PlayerMission] MissionsContainerComp missing (load path incomplete?) entity=" \<\< entt::to_integral(player); return kServiceUnavailable; }。out 在函数开头已经 Clear，不需要再清一次。然后：

- 删掉第 195 行的 if (container != nullptr) 包裹；

- 第 209 行的 FindScope(container, scope) 改为 container->Get(scope)，FindScope 不再需要判空，可以删除；

- 第 211 行改为 out.set_state_persistent(true)。

PlayerActivityReadSystem::BuildList 在循环之前做同样的检查（缺组件就 LOG_ERROR 并返回 kServiceUnavailable）。

测试和文档跟着改：player_feature_snapshot_test.cpp:152-166 按下面的回归测试改写；player-features-ui.md:39 把“任务返回 state_persistent=false”改为“已加载玩家恒为 true；缺任务存档组件时返回 kServiceUnavailable”。

Java 版要按同一口径核对：只在加载损坏时客户端可见，在 PARITY.md 登记一行。

**回归测试**

工程 bag_test。把 PlayerFeatureSnapshotTest.MissionCatalogueDoesNotAcceptOrInitializePlayerState 改写为 MissionListWithoutLoadedStateFailsClosedWithoutCreatingState：先 LoadMissions()，对没有 MissionsContainerComp 的 player 调 BuildList，断言：

- 返回值等于 kServiceUnavailable；

- out.missions_size() 为 0；

- out.state_persistent() 为 false；

- try_get\<MissionsContainerComp>(player) 仍为 nullptr。

修复前返回的是 kSuccess，并带着整份目录，用例变红。

再新增 MissionCatalogueForLoadedEmptyStateListsAllConfiguredMissions：先 ASSERT_EQ(kSuccess, PlayerMissionSystem::Initialize(player))，再调 BuildList，断言返回 kSuccess、missions_size 等于任务表行数、state_persistent 为 true、每一行都是 NOT_ACCEPTED。这条用例保证新号仍能看到完整目录。

<details><summary>复核者给出的拍板选项</summary>

state_persistent 字段怎么处理：

选项 A：保留字段，已加载玩家恒写 true。零协议变更，Java robot 的 FeaturesScenario 已经断言它为 true。推荐 A。

选项 B：按 AGENTS §4 在 player_mission.proto 里 reserved 3，删掉这个恒真字段，并与 Java 版和客户端同批改契约。收益只是少一个恒真字段，代价是一次客户端可见的协议变更，外加两版同步。

</details>

#### T7.7 W2-b1_runtime-07（low）旧格式快照(只有顶层 bag_data)的背包在派生属性算完后才恢复,DerivedAttributesComp 漏算装备加成,当前 HP 也被压到无装备上限;项目未上线,当前也没有任何代码写出这种快照

- 原严重度 medium，复核结论「降级」
- 位置：tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl:28-38,cpp/libs/services/scene/player/system/player_data_loader.h:30-39,cpp/libs/services/scene/player/system/player_database_loader.cpp:137,cpp/libs/services/scene/player/system/player_database_loader.cpp:170
- 本单口径：默认选 A：保留旧格式兼容，改模板「先并后解」。

**改法**

改模板 tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl 的 PlayerAllDataMessageFieldsUnMarshal,把末尾两段兼容 if 和其中的 bag_marshal::Unmarshal / mission_marshal::Unmarshal 删掉,在 range 之前先归一:

  const bool legacyBag = !message.player_database_data().has_bag_component() && message.has_bag_data();

  const bool legacyQuest = !message.player_database_data().has_mission_component() && message.has_quest_data();

  PlayerAllData upgraded; const PlayerAllData\* source = &message;

  if (legacyBag || legacyQuest) { upgraded = message; auto\* db = upgraded.mutable_player_database_data();

    if (legacyBag) \*db->mutable_bag_component() = message.bag_data();

    if (legacyQuest) \*db->mutable_mission_component() = message.quest_data();

    source = &upgraded; }

然后 range 里的调用改为 {{.HandlerName}}MessageFieldsUnmarshal(player, source->{{.MessageType}}_data());。这样每个实体的背包和任务都只 Unmarshal 一次,并且在 InitializeOnLoad 之前就位。模板头部的 bag_marshal.h / mission_marshal.h 两个 include 只服务于被删的分支,一并删掉。用 pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml 重新生成 player_data_loader.h,不要手改生成头。

**回归测试**

工程 bag_test,文件 cpp/tests/bag_test/player_equip_attribute_test.cpp,新增 TEST_F(PlayerEquipAttributeTest, LegacyTopLevelBagReloadRecomputesTheEquipmentBonus)。前半段照抄 ReloadFromTheDatabaseRecordRecomputesTheEquipmentBonus:WearCombatSet,换上 vitality 衣服,CompletePersistedComps,PlayerAllDataMessageFieldsMarshal(player, saved)。之后执行 saved.mutable_player_database_data()->clear_bag_component(),保留 Marshal 写出的顶层 bag_data,再 ParseFromString(saved.SerializeAsString()) 往返一次,对新实体(先挂 PlayerGuidComp)调用 PlayerAllDataMessageFieldsUnMarshal。断言:ASSERT_FALSE(parsed.player_database_data().has_bag_component());ASSERT_EQ(4u, bags->bags[kEquipment].OccupiedGridCount());ExpectSix(ReadSix(Derived(player)), ReadSix(\*derived));ExpectCombat(expectedCombat, derived->combat());EXPECT_EQ(Base(player).health(), base->health())。修复前背包能恢复,但派生值不含装备、HP 被压低,ExpectSix 与 health 两处必红。

<details><summary>复核者给出的拍板选项</summary>

项目尚未上线,当前也没有任何代码写出只带顶层 bag_data 的快照,所以需要拍板:兼容分支是否还要留。A:保留兼容,按上面的修法先并后解。改动只在模板,不动 proto,现有三条 Legacy/Mixed/AuthoritativeEmpty 用例保持有效。B:直接删掉读侧的两段兼容 if,另立一项清理,一并删除 Marshal 里写顶层 bag_data/quest_data 的镜像,proto 中的这两个字段改为 reserved。这样从根上删掉这一类问题,但要删改 player_feature_persistence_test.cpp 的 LegacyTopLevelRemainsReadableWhenNewDatabaseFieldsAreAbsent、NewDatabaseAndLegacyTopLevelSourcesAreChosenIndependently、AuthoritativeEmptyDatabaseSnapshotWinsOverStaleLegacyTopLevel。万一某个开发环境里还留着 09-11 前的快照,背包会被读成空包再存盘覆盖。推荐 A,作为本轮 ECS 收尾的最小改动;B 等负责人确认各环境都没有旧快照后另做。

</details>

#### T7.8 W1-24（low）66 号 ActorBaseAttributesS2C 只在战斗状态那一路带 entity_id，移动和速度增量的 entity_id 为 0。这是改造前就有的缺口，Java 已修，C++ 待跟；真客户端没有 66 的处理器，ECS 改造也没有让它变坏

- 原严重度 medium，复核结论「降级」
- 位置：tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:34-42, cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:32-40, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:104-109
- 本单口径：默认选 A：现在按模板修法与 Java 对齐。与 T9.3（同一模板）一次重生。客户端没有 66 号的处理器，但 proto 注释属客户端可见契约，Java 侧只需把 PARITY 的「mmorpg 待做」改为已对齐。

**改法**

若决定现在跟齐 Java（见 owner_decision），只改生成源和一个调用点，不手改生成树：

1) attribute_sync.cpp.tmpl：在 range .Fields 循环里，entity_id / player_id 字段不再走脏位分支，只保留 dirtyMask.reset。循环结束后、remove\<DirtyMaskComp> 之前，对这两个字段生成：

   if (hasDirtyField) { if (const auto\* guid = actorRegistry.try_get\<PlayerGuidComp>(entity)) syncMsg.set_\<field>(guid->value); }

   主体标识由此变成“只要发就带”，不再是一个要靠调用方记得置位的脏字段。

2) 删除 actor_attribute_calculator.cpp:105-109 的注释和 SetActorBaseAttributesS2CAttrDirtyBit(..., kEntityIdFieldNumber)，因为它已经多余。

3) proto/scene/player_state_attribute_sync.proto:19 的 entity_id 注释改为：玩家 = player GUID，与 ActorCreateS2C.entity 的 net id 不是同一口径；NPC 暂为 0。

4) 重新生成后，Java PARITY.md:53 的“mmorpg 待做”改成已对齐，并在 PROGRESS 记一条。

副作用：AttributeDelta\*FramesS2C 只有 entity_id 一个字段，改后永远不会发送。生产代码里本来就没有任何地方给它们置脏位，所以行为不变。NPC 的口径不在这次改动范围内。

**回归测试**

工程 aoi_delivery_test，新增用例 AoiDeliveryTest.BaseAttributeSyncCarriesPlayerGuidWhenOnlyTransformIsDirty。

步骤：

1) actor = Spawn(17, 23)，receiver = Spawn(1000, 1000)，取出 actor 的 PlayerGuidComp.value。

2) 只调用 SetActorBaseAttributesS2CAttrDirtyBit(actor, kTransformFieldNumber)，不置 entity_id 位。

3) 调用 ActorBaseAttributesS2CSyncAttributes(actor, ScenePlayerSyncSyncBaseAttributeMessageId, EntityUnorderedSet{receiver})。

断言：

- deliveries.size()==1；

- 解析出的 message.entity_id() 等于该 guid；

- message.has_transform() 为真。

修复前 entity_id 为 0，用例会红。已有用例 BaseAttributeSyncCarriesPlayerGuidAndClearsMask 显式置了 entity_id 位，测不出这个缺口。

<details><summary>复核者给出的拍板选项</summary>

选项 A（推荐）：现在按上面的修法在模板里补齐。改动小，与 Java 已有行为一致，真客户端不消费 66，所以没有兼容风险，还能消掉 PARITY 里一条“mmorpg 待做”。

选项 B：保持现状，继续挂在 PARITY 的“mmorpg 待做”。代价是两版对同一消息的输出继续不一致，robot 断言要兼容两种形态。

另外有一项更大的决定，不在 ECS 范围内：客户端没有 66 的处理器，而 66 每 2 帧向 AOI 内所有人广播 transform 和 velocity，是否下线 66 那一路要和客户端、Java 同批定。NPC 的 entity_id 口径（用 net id 还是保持 0）等到有 NPC 生成入口时再定。

</details>

#### T7.9 W1-25（low）ActorNetIdComp 定义在公共头里、可以随意写，导致 Of、Resolve、Assign 各自再做一遍反查自检；FillActorCreateMessageInfo 与两个调用方重复判 0，契约也没写明

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/actor/actor_net_id.h:8-11, cpp/libs/services/scene/actor/actor_net_id.cpp:39-42, cpp/libs/services/scene/actor/actor_net_id.cpp:57-73, cpp/libs/services/scene/spatial/system/view.cpp:91-98, cpp/libs/services/scene/spatial/system/aoi.cpp:185-190, cpp/libs/services/scene/player/system/player_scene.cpp:294-296
- 本单口径：可选项「把 Install 挪进 Assign」不做。

**改法**

1) 把 struct ActorNetIdComp 从 actor_net_id.h 挪进 actor_net_id.cpp 的匿名命名空间，头文件只保留 Install/Assign/Of/Resolve。

2) 删掉防护代码：

   - Of 改为 `const auto* id = actors.try_get<ActorNetIdComp>(actor); return id ? id->value : 0;`。EnTT 3.13 的 try_get 对无效或 null 句柄走 contains，是安全的。

   - Resolve 删掉 :71-72 的组件回查，改为 `return it == end ? entt::null : it->second;`。索引由 on_destroy 维护，不会留下失效句柄。

   - Assign:39-42 已有组件时直接 `return current->value;`。

3) FillActorCreateMessageInfo 改签名为 (entt::entity entrant, uint64_t netId, ActorCreateS2C&)：去掉没有用到的 observer 参数，删掉函数内的 Of 调用和判 0。aoi.cpp:185-191 与 player_scene.cpp:294-296 本来就先算 Of，改成 `const auto netId = Of(...); if (netId == 0) {LOG; continue/跳过;}`，再把 netId 传进去。这样每个进视野的实体少做一次 Of，也去掉了“半填”的路径。aoi_test/mock_view.cpp 同步改签名。

4) actor_net_id_test 改为只经公开 API 观察：

   - :79 的 remove 改为 destroy 后重建实体；

   - :21/:26/:46 的 any_of 改为断言 Of==0；

   - :47 删除。

可选：把索引改为在 Assign 里首次 ctx().emplace 时一并 connect 信号，Assign 是唯一写入口。这样可以删掉 Install 以及 world.cpp:30 和各测试夹具里的调用。这一项由负责人决定，不是必须做。

**回归测试**

写不出修复前会红的测试。这是纯重构，外部行为不变；组件对外可写这一点在修好后只会表现为编译不过，不是运行期行为。

替代验证：

1) 现有 actor_net_id_test、aoi_delivery_test、aoi_test、skill_test 改成只走公开 API 后全部为绿。

2) grep 门禁：`rg -n "ActorNetIdComp" cpp --glob '!**/actor_net_id.cpp'` 必须为 0 行；`rg -c "Resolve\(" cpp/libs/services/scene/actor/actor_net_id.cpp` 中，Of 和 Assign 的函数体里不再调用 Resolve。

#### T7.10 W1-31（low）actor net id 的唯一性范围（同时在线的节点之间唯一，进程重启后同一 node_id 会重新发同样的号）与 Java 雪花号不同，但没有写进两版的契约文档；F080 的三项客户端确认也没有记录。原文“取值区间已变”的担忧不成立

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/actor/actor_net_id.h:17-19, docs/handoff/ecs-refactor/batch4-external-id.md:529, docs/handoff/ecs-refactor/batch4-external-id.md:625, docs/handoff/ecs-refactor/batch4-external-id.md:629, docs/handoff/ecs-refactor/README.md:123, D:/luyuan/xuanming-server-mmo/docs/reference/mmorpg-client-contract-aoi.md:146, D:/luyuan/xuanming-server-mmo/PARITY.md:177
- 本单口径：**负责人已拍板（2026-10-16，核心拍板 3）：确认客户端在换场景、重连、换节点时清空 actor 表；号源维持现状，只改文档，改法第 3 条（加启动代次）不做。** 改法第 1 条要记录的「负责人对 F080 三项问题的答复」写为：客户端清空 actor 表（负责人 2026-10-16 确认）；另两项（是否只当不透明值使用、是否接受方案 B）负责人没有单独答复，按方案 A 维持现状记录。Java 仓的 mmorpg-client-contract-aoi.md 与 PARITY.md 按 AGENTS §12 同批改。

**改法**

只改文档，不改号源：

1) batch4-external-id.md:529、:625、:629 改写为：C++ 号 = node_id\<\<32|seq，同时在线的节点之间唯一、本进程内不复用；进程重启或 node_id 回收后，同一 node_id 会从 seq=1 重新发号。Java 用 SceneWorld.nextId 雪花号，跨重启也唯一。另记下负责人对 F080 三项问题的答复，若未确认就写“未确认”。

2) Java 仓 docs/reference/mmorpg-client-contract-aoi.md:146 的 C++ 描述改成上面的口径；PARITY.md:177 补一句两版的唯一性范围。

3) 只有当负责人确认客户端在 EnterSceneS2C 或重连时不清空 actor 表，才考虑在 seq 高位加启动代次。在那之前不要加：没有需求依据，属于 YAGNI。

**回归测试**

不需要。这是纯文档和契约记录的改动，不涉及代码行为；号源的进程内不复用、跨节点前缀、耗尽拒绝，已有 actor_net_id_test 的 DifferentNodesAndRegistriesDoNotAcceptEachOthersIdentities 和 ExhaustedSequenceStaysRefusedAndDoesNotWrap 覆盖。

<details><summary>复核者给出的拍板选项</summary>

需要负责人答复 F080 的三项客户端问题，重点是客户端在 EnterSceneS2C、重连或换节点时是否清空 actor 表。

选项 A（推荐）：确认会清空。那么号只需本进程内加在线节点间唯一，维持现状，只补文档。

选项 B：不清空或无法确认。那么在 seq 高位加启动代次，或改用现成的 SnowFlake 号源，让号跨重启也唯一，与 Java 一致。代价是号源改动和对应测试，客户端契约不变。

是否允许本仓的会话直接改 Java 仓的 contract-aoi.md 和 PARITY.md，还是交给 Java 侧的会话，也需要一并确认。

</details>

### T8 节点、单例与网络层：死 API、门面、残留注释与测试

node_utils.cpp、registration_manager.cpp、rpc_client.h、node.cpp、node_connector.cpp、scene_entry_dispatch.cpp、client_message_processor.cpp 都有 gRPC→muduo 迁移或 RPC 通道会话的在途改动：动手前 `git status --porcelain -- <文件>`，在其基础上改。T8.9 等迁移会话提交相关文件后再做。

#### T8.1 W2-b2-01（low）NodeContextManager 的每节点类型「全局实体」三件套随 etcd 迁移失去最后一个调用方，仍连同越界 FATAL、三条死亡测试和失实的头注释保留着

- 原严重度 medium，复核结论「降级」；合并：W2-fit-08
- 位置：cpp/libs/engine/thread_context/node_context_manager.h:11,cpp/libs/engine/thread_context/node_context_manager.h:13,cpp/libs/engine/thread_context/node_context_manager.h:16,cpp/libs/engine/thread_context/node_context_manager.h:24,cpp/libs/engine/thread_context/node_context_manager.h:53,cpp/libs/engine/thread_context/node_context_manager.h:78,cpp/libs/engine/thread_context/node_context_manager.h:92,cpp/tests/routing_identity_test/routing_identity_test.cpp:390,cpp/tests/routing_identity_test/routing_identity_test.cpp:394,cpp/tests/routing_identity_test/routing_identity_test.cpp:405

**改法**

cpp/libs/engine/thread_context/node_context_manager.h：删第 11 行 NodeGlobalEntity 别名；第 13 行注释去掉「and global entities」；删第 16–23 行注释，第 24 行改为 `NodeContextManager() = default;`，保留第 25–26 行的 delete 拷贝；删第 53–72 行（注释、SetGlobalEntity、GetGlobalEntity、GetOrCreateGlobalEntity）；Clear() 里删 `globalEntities_.fill(entt::null);`（第 78 行）；删第 92 行成员 globalEntities_。RequireValidNodeType、FailOutOfRange、GetRegistry、TryGetRegistry 不动，node_context_manager.cpp 不用改。

cpp/tests/routing_identity_test/routing_identity_test.cpp：删第 405–410 行三条 EXPECT_DEATH（SetGlobalEntity / GetGlobalEntity / GetOrCreateGlobalEntity）；第 390 行注释「GetRegistry 与全局实体三件套」改为「GetRegistry」；第 394 行注释删掉「/ globalEntities_」。GetRegistry 和 FindNodeEntityByZoneAndNodeId 的死亡断言保留。

docs/PROGRESS.md 追加一条（不改 8285 旧条目）：说明 NodeContextManager 全局实体 API 已随 etcd 迁移（f0dadf5b0）失去最后一个调用方，已删除。

**补充（来自 W2-fit-08）**

见 W2-b2-01。补充对方可能没写的部分：

① 一并删除：

- node_context_manager.h:13 注释里的「and global entities」。

- :16-23 整段构造函数注释；构造函数改为 NodeContextManager() = default;。

- :53-72 的注释和三个函数。

- :78 的 fill。

- :92 的成员和 :11 的别名。

② routing_identity_test.cpp：

- 删除 :405-410 的三条 EXPECT_DEATH，保留 GetRegistry 和 FindNodeEntityByZoneAndNodeId 的两条死亡断言。

- :390 和 :394 注释里的「全局实体三件套」「globalEntities_」一并删掉。

③ node.cpp:1248 注释删去「与全局实体句柄」。

④ node_context_manager.h 当前没有其他会话的未提交改动，可以直接改。

**回归测试**

删除死 API，没有运行期行为可测。用静态门代替：`git grep -n "GlobalEntity\|globalEntities_" -- cpp/libs/engine/thread_context cpp/tests/routing_identity_test` 期望零命中，修复前命中 node_context_manager.h 的 11/55/60/66/92 行等，即为红。再由 Codex 编译并运行 routing_identity_test（MSBuild /m:1 串行，或 `pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter routing_identity_test`），确认 NodeContextManagerBoundsDeathTest.OutOfRangeNodeTypeIsFatalInEveryBuild 仍对 GetRegistry 与 FindNodeEntityByZoneAndNodeId 生效且通过。

#### T8.2 W2-b2-02（medium）过渡用的空 TRANSFER_ERROR_MESSAGE 宏已无调用方，注释还写着「生成头仍有调用」；任务单 C2 的前置门早已满足，宏却没删

- 合并：W2-cross_sem-06
- 位置：cpp/libs/engine/core/macros/return_define.h:66,cpp/libs/engine/core/macros/return_define.h:67,cpp/libs/engine/core/macros/return_define.h:68

**改法**

删除 cpp/libs/engine/core/macros/return_define.h 第 66–68 行（两行过渡注释和 #define TRANSFER_ERROR_MESSAGE）。其余宏不动，CHECK_PLAYER_REQUEST 上方的规则指针保留。tools/proto_generator/protogen/internal/player_service_response_contract_test.go:97 的禁止串不必改：它在生成器单测层先行拦截，代价为零。负责人要精简的话，可以只删掉其中 "TRANSFER_ERROR_MESSAGE"、"globalRegistry"、"GlobalEntity" 三项（这三个符号在 C++ 里都已不存在，模板一旦产出就编不过），"CopyFrom("、"->mutable_error_message()"、"set_id(" 必须保留。另在 docs/PROGRESS.md 追加一条：第 2 批 C2 已完成。

**补充（来自 W2-cross_sem-06）**

见 W2-b2-02。补充两点：只删 return_define.h:66-68 这三行（两行过渡注释加宏定义），不要顺手删 player_service_response_contract_test.go:97 里的 "TRANSFER_ERROR_MESSAGE" 禁止串，它是用来防止生成器模板退回旧写法的。删完后 git grep -n TRANSFER_ERROR_MESSAGE -- cpp 应该零命中。

**回归测试**

宏本身是空操作，删掉它不改变任何运行期行为，所以不需要运行期用例。用静态门代替：在仓库根目录执行 `git grep -n TRANSFER_ERROR_MESSAGE -- cpp`，期望零命中。修复前命中 return_define.h:68，即为红。生成器侧由现有的 Go 单测 TestPlayerResponseContract_DispatchDoesNotTransferOrCreateErrors 继续守着模板（由 Codex 在 tools/proto_generator/protogen 下跑 go test ./...）。

#### T8.3 W2-b2-03（medium）NodeInfo 只读门面的 atomic 发布和「返回旧值」的 exchange 只为测试存在；SameInstanceOnEveryThread 把契约禁止的跨线程读固化成用例，挑战意见 [low] 未落实

- 位置：cpp/libs/engine/core/network/node_utils.cpp:3,cpp/libs/engine/core/network/node_utils.cpp:16,cpp/libs/engine/core/network/node_utils.cpp:17,cpp/libs/engine/core/network/node_utils.cpp:20,cpp/libs/engine/core/network/node_utils.cpp:27,cpp/libs/engine/core/network/node_utils.h:18,cpp/libs/engine/core/network/node_utils.h:24,cpp/tests/cross_zone_test/cross_zone_test.cpp:2,cpp/tests/cross_zone_test/cross_zone_test.cpp:6123,cpp/tests/cross_zone_test/cross_zone_test.cpp:6138,cpp/tests/cross_zone_test/cross_zone_test.cpp:6147,cpp/tests/cross_zone_test/cross_zone_test.cpp:6216
- 本单口径：默认选 A：删除 SameInstanceOnEveryThread。

**改法**

node_utils.cpp：删第 3 行 #include \<atomic>。第 14–23 行改为 `namespace { const NodeInfo *localNodeInfo = nullptr; }` 和 `void InstallLocalNodeInfo(const NodeInfo *info) { localNodeInfo = info; }`。GetNodeInfo 改为 `return localNodeInfo != nullptr ? *localNodeInfo : NodeInfo::default_instance();`。

node_utils.h：第 24 行签名改为 `void InstallLocalNodeInfo(const NodeInfo *info);`。第 18–22 行注释压成三行：所有者是 Node，构造时登记、析构时注销；未登记时返回只读默认实例，不造状态；只在所属 loop 线程调用，返回引用不越过所有者寿命。node.cpp:414/480 不用改。

cross_zone_test.cpp：删除 TEST(LocalNodeInfoTest, SameInstanceOnEveryThread)（6123–6136）和 TEST(LocalNodeInfoTest, InstallingAnotherOwnerCanRestorePreviousBinding)（6147–6157）；std::thread 只在 6129 用到，第 2 行 #include \<thread> 一并删除。UninstalledIsReadOnlyDefault 改为：`InstallLocalNodeInfo(nullptr);`，保留两条断言，再 `InstallLocalNodeInfo(&TestLocalNodeInfo());`，最后断言 `&GetNodeInfo() == &TestLocalNodeInfo()`。main 改为 `InstallLocalNodeInfo(&TestLocalNodeInfo());` … `RUN_ALL_TESTS()` … `InstallLocalNodeInfo(nullptr);`。

可选：node_allocator.cpp 的读点（99、100、192、193、223、254、319、326）和 registration_manager.cpp:106 本来就依赖 gNode，统一改为 gNode->GetNodeInfo()，自由函数只留给看不到 Node 的模块。registration_manager.cpp 正被其他会话改动，按函数名定位，不按行号。

**回归测试**

这是删除过度设计，可观察行为不变，构造不出「修复前会红」的运行期用例。用静态门代替：`git grep -n "std::atomic<const NodeInfo\|SameInstanceOnEveryThread\|InstallingAnotherOwnerCanRestorePreviousBinding" -- cpp` 期望零命中，修复前有命中即为红。行为由保留的用例守住：Codex 编译并运行 cross_zone_test（MSBuild 串行 /m:1，或 `pwsh -File tools/scripts/run_cpp_tests.ps1 -Build -Filter cross_zone_test`），断言 LocalNodeInfoTest.SurvivesEcsClear 与改写后的 LocalNodeInfoTest.UninstalledIsReadOnlyDefault 都实际执行且通过，其余用例全绿（确认测试宿主身份经 main 安装、监听器每例清零的语义没变）。

<details><summary>复核者给出的拍板选项</summary>

是否保留一条「门面不是 thread_local」的结构性用例。选项 A（推荐）：照挑战意见删掉 SameInstanceOnEveryThread。后果：契约「只在 loop 线程访问」不再被测试反向示范，F008 回归只靠 SurvivesEcsClear。选项 B：保留它，改名为 NotThreadLocalStorage，并在用例注释写明「生产仍禁止跨线程读，此处只验证存储不是 thread_local」。后果：契约与用例之间的张力还在，但 atomic 照样可删，因为 join 已经给出 happens-before。两种选择下 atomic 和返回旧值的 exchange 都应删除。

</details>

#### T8.4 W2-b2-04（low）ecs_context_test 5 条会话索引用例中有 4 条测的是 std::unordered_map、thread_local 或无关 registry，对基线同样全绿；回执和 PROGRESS 里「旧 getter 对照 2/5 失败」的证据对当前源码已不成立。原修复由 globalRegistry 删除在编译期守住，不缺回归保护

- 原严重度 medium，复核结论「降级」
- 位置：cpp/tests/ecs_context_test/ecs_context_test.cpp:36-47, cpp/tests/ecs_context_test/ecs_context_test.cpp:49-61, cpp/tests/ecs_context_test/ecs_context_test.cpp:81-93, cpp/tests/ecs_context_test/ecs_context_test.cpp:95-111, docs/handoff/ecs-refactor/batch2-singletons.md:407, docs/PROGRESS.md:8268
- 本单口径：默认选 A：删除 4 条对基线也全绿的用例。

**改法**

1) ecs_context_test.cpp 删除 EmptyLookupDoesNotCreateComponentPools（36-47）、SurvivesUnrelatedRegistryClear（49-61）、RebindingSessionReplacesOnlyItsPlayer（81-93）、SeparateLoopThreadsOwnSeparateIndexes（95-111）四个 TEST_F。保留 SceneEntityContract 和 ContextClearDropsBothIndexesAndAllowsNewSession，同时删掉随之不再使用的 `#include <iterator>` 和 `#include <thread>`。

2) 交接文档只追加、不改原文：在 batch2-singletons.md §9 末尾追加一条更正，说明 407 行的「2/5 失败」对应 globalRegistry 时期的旧断言，当前用例对基线全绿，会话索引不进 registry 由 globalRegistry 删除在编译期守住；PROGRESS.md 另起一条追加同样的更正，不回改 8268 行。

**回归测试**

不需要新增：删除的是无效用例。现有 ecs_context_test 的 SessionPlayerIndexTest.ContextClearDropsBothIndexesAndAllowsNewSession 就是有效回归，可以做一次变异自检：临时去掉 EcsContext::Clear() 里的 `sessionPlayers.clear();`，该用例的 EXPECT_TRUE(SessionMap().empty()) 变红，恢复后变绿。由 Codex 在忽略目录的对照副本里执行，不改共享工作区。

<details><summary>复核者给出的拍板选项</summary>

选项 A（推荐）：删除 4 条用例，只留 ContextClear 一条。代价是失去「会话索引不得再作为组件挂进 actor/scene registry」的运行期形状守卫，但要重新那样做，必须改写 SessionMap() 的一行返回语句，评审时一眼可见。选项 B：保留 EmptyLookup 和 SurvivesUnrelatedRegistryClear 作为形状守卫，改名并在注释写明「对基线不红，只防将来回退到 registry 组件」；删除 Rebinding 和 SeparateLoopThreads。代价是保留两条对现实现恒真的用例，与负责人「删掉多余」的偏好相悖。

</details>

#### T8.5 W2-b2-05（low）玩家接口契约写着「RpcDoneCallback 延后完成同样遵守」，但玩家 handler 拿不到 done；engine 的 rpc_done_callback.h 反向指向 services 规则，还提到已不存在的全局 tip。挑战意见 [low] 没有落实

- 位置：cpp/libs/services/player/rpc/player_service_interface.h:20, cpp/libs/engine/core/network/rpc_done_callback.h:20-21, docs/design/ecs-component-access-rules.md:69
- 本单口径：规则文档那一句并入 T11。

**改法**

1) player_service_interface.h:20 删掉「RpcDoneCallback 延后完成同样遵守此规则。」这一句，同一行前半句保留。不要动工作区里其他会话改的第 18-19 行。

2) rpc_done_callback.h 删掉第 20-21 行整条 bullet，不换成别的写法：第 46 行 Fail() 的注释已经覆盖「业务失败写 error_message 再 Run」。

3) ecs-component-access-rules.md:69 删掉「，延期应答写本次 `RpcDoneCallback` 的 response」，只留「请求拒绝码直接写本次 response」。节点级延后完成的规则以 muduo-rpc-channel.md §5.2 为唯一出处。

**回归测试**

不需要：只改注释和文档，不改变任何运行期行为。可以用一条 grep 作门禁：`git grep -n "RpcDoneCallback" -- cpp/libs/services/player` 应为零命中，`git grep -n "player_service_interface\|全局 tip" -- cpp/libs/engine/core/network/rpc_done_callback.h` 应为零命中。

#### T8.6 W2-b2-06（low）C4.3 指名的 player-features-ui.md:22 没改，仍教「写 TLS tip 由 TRANSFER 宏转移、直接写 response 会被覆盖」，与现行规则相反；另有 6 处注释或 include 仍按全局 tip、TLS NodeInfo、GlobalEntity 解释现状

- 位置：docs/design/player-features-ui.md:22, cpp/libs/services/scene/player/system/player_landing.h:126, cpp/nodes/scene/handler/rpc/player/player_gm_guard.h:66, cpp/libs/engine/core/network/network_utils.cpp:28-29, cpp/libs/engine/core/network/rpc_channel.cpp:18, cpp/libs/services/scene/spatial/system/aoi.cpp:177-178, cpp/tests/aoi_test/aoi_system_test.cpp:40-41, docs/design/guild-phase2/04-asset-channel.md:521, docs/design/guild-phase2/04-asset-channel.md:1365

**改法**

1) player-features-ui.md:22 把「写入 TLS TipInfoMessage 后由生成层 `TRANSFER_ERROR_MESSAGE` 转移；直接写响应错误会被该宏覆盖。」改为「拒绝码直接写本次应答的 error_message（规则见 cpp/libs/services/player/rpc/player_service_interface.h）。」

2) player_landing.h:126 改为「// 应答类型是 Empty，没有 error_message 字段，结果只经 NotifySyncDigest 传达。」也可以直接删掉，因为 119-125 行已写清推送规则。不要写「所有失败都推」。

3) player_gm_guard.h:66 改为「// true = 本次调用应当被拒绝（调用点把拒绝码写进本次 response 的 error_message 并 return）。」

4) network_utils.cpp:28-29 把「the thread-local zone context (GetZoneId()) lags behind node discovery」改为「this node's zone (GetZoneId(), read from Node::localNodeInfo_, filled in Node::InitRpcServer) is not yet installed」。其余英文说明不动。

5) 删除 rpc_channel.cpp:18 的 `#include "proto/common/base/tip.pb.h"`。

6) aoi.cpp:177-178 两行删除，只留 179-180 行「用函数内 static thread_local…不存在重入覆盖」。aoi_system_test.cpp:40-41 改为一行「// AoiSystem 的 S2C 发送缓冲是 aoi.cpp 内的函数内 static thread_local，测试无需预置。」

7) guild-phase2/04-asset-channel.md 在 521、1365 所在示例上方各加一行「> 历史示例：全局 tip 已于 ECS 第 2 批移除，拒绝码直接写 response 的 error_message。」不改示例本身。

**回归测试**

不需要：只改注释、文档和一个未使用的 include，没有运行期行为变化。门禁 grep：`git grep -n "TLS TipInfoMessage\|SetTip\|全局 tip 会污染\|thread-local zone" -- cpp docs/design/player-features-ui.md` 与 `git grep -n "GlobalEntity\|globalRegistry" -- cpp/libs/services cpp/tests/aoi_test` 应零命中；rpc_channel.cpp 删 include 后由 Codex 正常编译 rpc_controller_test 工程确认。

#### T8.7 W2-b2-07（low）C3 没按挑战意见收尾：用例名和注释仍指向已删除的 GlobalEntity/globalRegistry；末尾换上的 actor/scene any_of 断言对旧实现同样必过，是无效断言

- 位置：cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:183-185, cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:212-216, cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:63

**改法**

1) 删除 183-184 行两行注释。

2) 用例改名为 `TEST_F(AoiDeliveryTest, VisibilityAndLeaveNotificationsCarryCorrectBodies)`。

3) 删除 212-216 行（「发送缓冲不属于 actor 或场景」注释、actorRegistry 的 for 循环加 EXPECT_FALSE、sceneRegistry 的 EXPECT_FALSE）。用例到 ASSERT_EQ(actorDestroyToStationary, 1) 结束。

4) 第 63 行 SetUp 注释整行删除（SetUp 不需要解释自己没做什么）；要保留的话只留 C3 规定的「AOI 的 S2C 发送缓冲是 aoi.cpp 内的 static thread_local」。

前半段的 Spawn、Update、BeforeLeaveSceneHandler 和三组 ASSERT/EXPECT 原样不动。

**回归测试**

不需要：这是测试自身的清理，不修生产代码。保留的断言本身就是有效回归：任一通知路径不再发送，或 ActorDestroyS2C.entity 填错，都会使 ASSERT_EQ(CreatesFor/DestroysFor, 1) 或 EXPECT_EQ(message.entity(), actor_net_id::Of(...)) 变红。Codex 执行 aoi_delivery_test 时用 --gtest_filter=AoiDeliveryTest.VisibilityAndLeaveNotificationsCarryCorrectBodies，确认改名后的用例确实被执行。

#### T8.8 W2-b2-08（low）NodeInfoListComp 唯一使用方是已删除的 ServiceNodeList 单例，现在两个仓库都没有任何手写代码引用它，成了死 proto 消息

- 位置：proto/common/base/common.proto:38
- 本单口径：默认选 A：本仓与 Java 同步副本同批删除。

**改法**

直接删除，不加注释兜底。

1) 从本仓 proto/common/base/common.proto 删掉第 38-41 行的 message NodeInfoListComp 整段，连同前后多出的空行。

2) Java 仓 game-proto/src/main/proto/proto/common/base/common.proto:41 的同一段同批删除（这是 mmorpg proto 的同步副本，§12 要求同批改），并在 PARITY.md 登记一行。

3) 在仓库根目录执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`，重生成 cpp/generated、go/proto、generated/\*\* 下的各语言产物。robot/vendor/proto 是 go mod vendor 出来的副本，在 robot 下重新 vendor 即可，不要手改。

4) 不要手改任何 \*.pb.\* 或 generated 文件。

**回归测试**

不需要新增单元测试：删的是零引用的消息类型，没有运行期行为可测。验证分两步：Codex 重生成后完整编译所有启用模块（C++ MSBuild /m:1、go build ./...、Java mvn -pl game-proto -am compile），任何残留引用都会直接编译失败；另外在两仓执行 `git grep -n NodeInfoListComp -- ':!**/generated/**' ':!*.pb.*' ':!robot/vendor/**' ':!docs/**'`，结果应为空。

<details><summary>复核者给出的拍板选项</summary>

选项 A（推荐）：现在删除。本仓 proto 和 Java 同步副本同批删除，重生成本仓产物，robot 重新 vendor。代价是同级客户端仓里的 C# 生成类会留到下一次获客户端授权的生成时才消失。这个类没有使用方，留着无害；本次未获授权读取客户端仓，所以无法确认客户端手写代码没有引用它，但它属于服务端内部的节点目录结构，按常理客户端不会用到。选项 B：暂不处理。运行期零成本，但带 Comp 后缀的死消息会一直误导读者，也会和 Java 副本一起长期漂移。不建议只加注释保留。

</details>

#### T8.9 W2-fit-07（medium）出站连接 UP 后的节点握手（必需步骤，生产唯一监听者是 Node）仍由 RpcClient 经 tlsEcs.dispatcher 转发；传输对象因此 include 场景 ECS 头，Node 为这一个回调维护 scoped_connection 和析构顺序约束。ecs-fit-overview 定的「构造注入」没有任何会话接手

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/engine/core/network/rpc_client.h:16-17,258, cpp/libs/engine/core/network/rpc_connection_event.h:5, cpp/libs/engine/core/node/system/node/node.cpp:511,1305-1309,1560-1578, cpp/libs/engine/core/node/system/node/node.h:213,241,303-304, cpp/libs/engine/core/node/system/node/node_connector.cpp:142-157
- 本单口径：**等 gRPC→muduo 迁移会话提交 rpc_client.h / node.cpp / node_connector.cpp 后再做**（「拍板」选项 A)。

**改法**

① rpc_client.h

- 删除 :16 的 #include "rpc_connection_event.h" 和 :17 的 #include \<thread_context/ecs_context.h>。

- 新增 using ConnectionObserver = std::function\<void(const muduo::net::TcpConnectionPtr&)>;、私有成员 ConnectionObserver connectionObserver_;，以及 void setConnectionObserver(ConnectionObserver observer) { connectionObserver_ = std::move(observer); }。注释写明它和 registerService 一样必须在 connect() 之前调用。

- :258 改为 if (connectionObserver_) connectionObserver_(conn);。

② node_connector.cpp ConnectToTcpNode：在 client->connect(); 之前加

client->setConnectionObserver([nodeType = nodeInfo.node_type()](const muduo::net::TcpConnectionPtr& conn){ if (gNode == nullptr || !gNode->IsServiceStarted() || !conn->connected()) return; gNode->GetNodeRegistrationManager().TryRegisterNodeSession(nodeType, conn); });

- 只捕获值、不捕获对象，符合 §11.7。

- GetNodeRegistrationManager 和 IsServiceStarted 都是 public；~Node 在释放成员前会把 gNode 置空。

- nodeType 已知，不再遍历全部类型。

③ 删除 Node 侧的接线：

- Node::RegisterEventHandlers：node.h:213 的声明、node.cpp:511 的调用、:1305-1309 的定义。

- Node::OnServerConnected：node.h:241、node.cpp:1560-1578。

- nodeConnectedSubscription_ 成员及其注释（node.h:303-304），以及只为它引入的 sigh.hpp。

- cpp/libs/engine/core/network/rpc_connection_event.h 及其工程登记。

④ 两份测试改用观察者：

- rpc_client_lifecycle_test.cpp：EventCounter::OnEvent 改为接收 const TcpConnectionPtr&，在 make_shared\<RpcClient> 之后、connect 之前调 setConnectionObserver；删掉 SetUp/TearDown 里的 sink connect/disconnect 和 :61 的 include。

- rpc_channel_loopback_test.cpp：DownProbe 照此修改，删掉 :625 和 :752 的 sink 操作。

⑤ 注释和文档：

- registration_manager.cpp:103 的注释改为「重连 UP 经 RpcClient 的连接观察者重新 TryRegisterNodeSession」。该文件有其他会话未提交的改动，只改这一行。

- muduo-rpc-channel.md:378 同步更新。

- 顺带删除 rpc_channel.cpp:20 没用到的 ecs_context.h include。

**回归测试**

工程：rpc_controller_test。

用例：rpc_client_lifecycle_test.cpp 新增 RpcClientLoopback.ConnectionObserverReportsUpAndDownWithoutDispatcher。

步骤：

1. 不在 tlsEcs.dispatcher 上挂任何 sink。

2. 在 loop 线程 make_shared\<RpcClient>，setConnectionObserver 记录 up 和 down 次数；DOWN 时同时记录 client->channel() 是否为空。

3. connect 到夹具的 TcpServer，等第一次 UP。

4. 在 loop 线程对服务端连接 forceClose，等客户端报 DOWN。

断言：

- up==1，down>=1。

- DOWN 回调时 channel() 已为空：钉住「先 OnDisconnect、reset channel，再通知」的顺序，DownProbe 依赖它。

修复前：没有 setConnectionObserver，测试编译即红（API 缺失）。

另加机械门禁：rg -n "dispatcher|ecs_context" cpp/libs/engine/core/network/rpc_client.h 零命中。

<details><summary>复核者给出的拍板选项</summary>

实施归属。rpc_client.h、node.cpp、node_connector.cpp 和 registration_manager.cpp 正在被 gRPC→muduo 迁移会话和 RPC 通道会话改动。

- 选项 A：等迁移会话提交后，作为 ECS 补充任务单的一项交给 Codex，约 8 个文件，不与在途改动冲突。

- 选项 B：并入迁移会话收尾，少一次交接，但会扩大该会话本已未完成的范围。

推荐 A。

</details>

#### T8.10 W1-28（low）gate 节点句柄类型化后残留旧整数逻辑：GetEffectiveNodeEntity 成了多余包装，ResolveSessionTargetNode 里有无意义别名和不可达的重绑分支，ForwardPlayerToScene 日志用“scene_node=”打印实体整数

- 位置：cpp/nodes/gate/handler/rpc/client_message_processor.cpp:136,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:287,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:495,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:788,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:93,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:146,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:152,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:161,cpp/libs/services/gate/session/comp/session_info_comp.h:19,cpp/libs/services/gate/session/comp/session_info_comp.h:127

**改法**

删除优先，不加任何防护：

1. client_message_processor.cpp：删除 static GetEffectiveNodeEntity（:136-155），把其中“已销毁的节点必须等注册事件重新绑定，不能用旧 index 猜测继任节点”这句注释移到 ResolveSessionTargetNode。

2. ResolveSessionTargetNode 的 :287-304 改为：`const auto &registry = tlsNodeContextManager.GetRegistry(nodeType); const auto node = session.GetNodeEntity(nodeType); if (!registry.valid(node)) { LOG_ERROR << "Bound node is invalid. nodeType: " << nodeType << ", session id: " << sessionId << ", stored_entity=" << entt::to_integral(node); session.SetNodeEntity(nodeType, entt::null); return std::nullopt; } return node;`。删除 effectiveNodeEntity、nodeEntity 别名和 :299-302 的重绑分支。

3. HandleTcpNodeMessage :788 改为 `const auto targetNodeEntity = session.GetNodeEntity(handlerMeta.targetNodeType);`，:790 已有的 valid 检查保留，它对 entt::null 返回 false。

4. HandleConnectionDisconnection :495-498：删除 sceneEntityId，改为直接 `const auto sceneEntity = sessionIt->second.GetNodeEntity(SceneNodeService);`。

5. scene_entry_dispatch.cpp ForwardPlayerToScene：参数 sceneNodeEntityId 改名为 sceneEntity，删除 :98 的别名行。:146 和 :152 的 `scene_node=" << entt::to_integral(...)` 改为 `scene_node_id=" << NodeId`，其中 `const auto *info = sceneRegistry.try_get<NodeInfo>(sceneEntity)`，取不到记 0，与 4c-2 断线路径同一写法。

6. ResolveSceneNodeEntity 函数体改为一行：`return NodeUtils::FindNodeEntityByNodeId(SceneNodeService, nodeId);`。

7. session_info_comp.h：成员 entityIds 改名为 nodeEntities，SetNodeEntity 的参数 entityId 改名为 node；同步修改 gate_service_handler.cpp:278 注释里的 entityIds。

**回归测试**

不需要。理由：删除的是不可达分支、别名和多余包装，日志改的只是标签，可观察行为不变，构造不出修复前会红的用例。替代验证（Codex 执行）：编译 nodes/gate 与 routing_identity_test 并运行，现有 SceneRouteEntry.\* 全部保持绿；另外用 git grep 确认 cpp/nodes/gate 下不再出现 `GetEffectiveNodeEntity`、`sceneNodeEntityId`、`scene_node="`。

#### T8.11 W2-b5_types-06（low）gate 把 scene 节点 registry 叫 sceneRegistry、节点句柄叫 sceneEntity,与 tlsEcs.sceneRegistry 及 SceneEntity 类型同名异义(F085 同类漏点);两处类型化后残留的无用别名

- 原严重度 low，复核结论「降级」；合并：W2-fit-09
- 位置：cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:97,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:98,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:392,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:482,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:485
- 本单口径：与 T8.10(W1-28)同一处，一起做。

**改法**

与 W1-28 一并实施。1) scene_entry_dispatch.cpp:ForwardPlayerToScene 中,:97 的局部变量 sceneRegistry 改名为 sceneNodeRegistry,删除 :98 的 `const auto sceneEntity = sceneNodeEntityId;`,后续 :99/:104/:112 直接使用参数(按 W1-28 改名为 sceneNode)。2) scene_entry_dispatch.cpp:OnSceneLinkHandshaken 中,:392 的 sceneRegistry 改名为 sceneNodeRegistry。3) client_message_processor.cpp:HandleConnectionDisconnection 中,删除 :485 的 `const auto sceneEntity = sceneEntityId;`;:482 的 sceneEntityId 改名为 sceneNode,:486-489 直接用它。

**补充（来自 W2-fit-09）**

见 W2-b5_types-06。补充它没有覆盖的部分：

- scene_entry_dispatch.cpp：ForwardPlayerToScene 的参数 sceneNodeEntityId 改名为 sceneNode；删掉 :98 的别名，:99、:104、:112 直接用 sceneNode；:97 和 :392 的 sceneRegistry 改名为 sceneNodeRegistry。

- client_message_processor.cpp：删掉 :485 的别名，把 :482 的 sceneEntityId 改名为 sceneNode，:486-489 直接用它。

- 不要改 SceneEntityComp 的字段名。

- 日志「scene_node=」按 W1-28 处理。

**回归测试**

不需要。纯局部变量改名和删除无用别名,不改变任何行为,也无法构造修复前会红的用例;现有 routing_identity_test、gate 场景入场相关用例覆盖行为不变。

#### T8.12 W1-29（low）“同 index 不同 version 判为换节点”用例是类型契约固定用例，不是修复前会红的回归测试，回执把它称作“回归”是误标

- 原严重度 low，复核结论「降级」
- 位置：cpp/tests/routing_identity_test/routing_identity_test.cpp:643,docs/handoff/ecs-refactor/batch4-external-id.md:691
- 本单口径：默认选 A：保留用例，标注为类型契约固定用例。

**改法**

代码不改，也不加 static_assert 或 grep 门禁。只改两处说明：(1) 在 routing_identity_test.cpp:643 用例上方加一行注释：`// 类型契约固定用例：F033 为纯类型重构，改造前 64 位存储同样判 kChanged；守的是将来把存储收窄成 index/uint32 时 :657 变红，不是修复前红的回归。`。(2) 在 batch4-external-id.md §9 回执之后追加一条更正（不改原文）：“补同index不同version重绑回归”应为“类型契约固定用例，F033 无行为缺陷，无修复前红”，与 §8 对 AnomalyDetector 用例的定性一致。

**回归测试**

不需要新增。理由：F033 没有行为缺陷，不存在修复前会红的用例；现有用例已能固定版本位不丢。可选的变异验证（Codex 执行，验证完即还原，不提交）：把 SessionInfo::SetNodeEntity 临时改成只存 entt::entity{static_cast\<uint32_t>(entt::to_integral(e))}，运行 routing_identity_test，SceneRouteEntry.RecycledNodeIndexWithAnotherVersionCountsAsChanged 应在 :657 变红。

<details><summary>复核者给出的拍板选项</summary>

README §3 要求新增用例“修复前红、修复后绿”，这条用例按定义做不到，需要负责人二选一。A（推荐）：保留，标注为类型契约固定用例。它能在将来有人收窄存储时变红，成本只是一行注释和一条回执更正。B：删除，严格执行 §3。代价是失去对“存储丢 version”的唯一行为级守护，之后只能靠类型系统挡，而类型系统挡不住“仍是 entt::entity 接口、内部改成截断存储”这种回退。

</details>

#### T8.13 W2-fit-06（low）节点表仍是「map + 动态属性袋」式 registry：查找靠线性扫描 view，出站/入站角色靠组件组合区分，握手定时器用通用 TimerTaskComp。这是 ecs-fit-overview 第 5 条中待拍板的存量架构债，不是本次交付缺陷，也没有可达故障

- 原严重度 medium，复核结论「降级」
- 位置：cpp/libs/engine/core/node/system/node/node_util.cpp:52-95,175-210,213-243, cpp/libs/engine/core/node/system/node/node_connector.cpp:87-88, cpp/libs/engine/core/node/system/registration/registration_manager.cpp:95,142,197, cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:403-404
- 本单口径：本单只做文档标注。是否立项 NodeDirectory 见报告「需负责人拍板」。

**改法**

拍板前不改代码。只在 docs/design/ecs-component-access-rules.md 的「实体类型与生命周期边界」表「节点路由句柄」一行加注：节点 registry 是待移出的存量（目标是 NodeDirectory，见 ecs-fit-overview 第 5 条），不得再往节点实体新增组件种类。

拍板选 B 后按目标形态实施：

- NodeRecord 包含 NodeInfo、std::variant\<RpcClientPtr, MuduoGrpcChannelPtr> 出站传输、weak 持有的入站通道、成员 TimerTaskComp handshakeRetry，以及 gate 握手用的 std::weak_ptr\<TcpConnection>。

- 每个 nodeType 一张以 uuid 为键的 map，另建 (zone, node_id) 二级索引。

- NodeUtils::Find\* 改为查 map。

- OnHandshakeCallDone 按 uuid 直接取记录，不再扫 view\<RpcClientPtr>。

- 生成模板 grpc_async_client 的 Send\*(registry&, entity) 签名与迁移会话联合修改。

- 删除 node_util.cpp 里对 view 的线性扫描。

**回归测试**

拍板前没有行为改动，不需要。

若实施 NodeDirectory：在 routing_identity_test 新增 NodeDirectoryTest.LookupAndReRegistration。

步骤：登记两个 zone 中 node_id 相同的节点；再用同一 uuid 重新注册其中一个。

断言：

- 按 (zone, node_id) 和按 uuid 都各自查到正确的记录。

- 重新注册后，旧 RpcClient 的 weak_ptr 已过期。

- 携带旧 weakClient 的握手回包不会改动新记录。

修复前 NodeDirectory 不存在，编译即红。

<details><summary>复核者给出的拍板选项</summary>

对应 ecs-fit-overview §7 第 1 条，三个选项：

- A：不立项。接受节点 registry 为存量，只在文档上标注。后果：线性查找和「靠组件组合区分角色」的可读性问题继续存在，但没有可达故障。

- B：等 gRPC→muduo 迁移提交后，直接做第 2 期替换存储（约 35 个文件，含生成模板），与迁移会话联合实施。收益：节点代码去掉 registry/entity 和线性扫描，gate 的 SessionInfo 改存 NodeRef。

- C：先做第 1 期只读门面（约 13 个文件）。这等于在 registry 上再加一层透传门面，第 2 期还得再改一遍。

推荐 B；没有余力就选 A。不推荐 C。

</details>

### T9 组件类型、注释与 robot 文案

纯收口，不改行为。T9.3 与 T7.8 改同一模板，一次重生。

#### T9.1 W2-b5_types-01（low）读 PlayerGuidComp 的私有 GuidForLog 在 7 个文件里各有一份,还有 4 处守护段内联三元表达式,与 ActorLog 并存:名为日志,实际被用作异步身份栅栏和快照字段;第 4 批要求的「Guid 组件类型变化只改一处」没有达成

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:49,cpp/libs/services/scene/spatial/system/scene_spawn.cpp:38,cpp/libs/services/scene/battle/system/player_battle.cpp:135,cpp/libs/services/scene/player/system/player_equip.cpp:51,cpp/libs/services/scene/player/system/player_pet.cpp:45,cpp/libs/services/scene/player/system/player_attribute.cpp:63,cpp/libs/services/scene/player/system/gm_sanction_inbox_executor.cpp:36,cpp/libs/engine/core/utils/registry/actor_log.h:14,cpp/libs/engine/thread_context/ecs_context.h:42,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:38,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:62,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:77,cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp:30
- 本单口径：默认选 A：只收口读取函数，不统一 0 与 kInvalidGuid 哨兵。gm_sanction_inbox_executor.cpp 属收件箱会话在途改动，协调后再改。

**改法**

1) 在 ecs_context.h:42 的 GetPlayer 旁边加 `[[nodiscard]] Guid GetPlayerId(entt::entity player) const;`。ecs_context.cpp 实现为:`const auto* guid = actorRegistry.try_get<PlayerGuidComp>(player); return guid != nullptr ? guid->value : 0;`,不需要先 valid(),try_get 对陈旧句柄和 null 返回空。2) actor_log.h 的 ActorLog 函数体改为 `return {tlsEcs.GetPlayerId(actor), actor};`,并按第 4 批任务单在注释里写明这是日志标识 actor 的唯一写法。3) 删除 7 份 GuidForLog 定义:player_scene.cpp:47-53、scene_spawn.cpp:38、player_battle.cpp:135、player_equip.cpp:51、player_pet.cpp:45、player_attribute.cpp:63、gm_sanction_inbox_executor.cpp:36(这个文件属于收件箱会话的在途改动,需协调)。所有调用点机械替换为 tlsEcs.GetPlayerId(x),日志文本不变;身份栅栏(player_scene.cpp:65 的 IsSamePlayer,player_battle.cpp:306/443/596/1509/1588/1976/2077/2173)和业务赋值(player_pet.cpp:960、player_battle.cpp:914/1673/2156/2284/2338、player_scene.cpp:398)也照此替换。IsSamePlayer 可简化为 `return playerId != 0 && tlsEcs.GetPlayerId(player) == playerId;`。4) 生成式 handler 守护段内的 4 处三元表达式(s2s_player_scene_handler.cpp:38/62/77、player_scene_handler.cpp:30)换成 tlsEcs.GetPlayerId(player)。5) 哨兵(0 与 kInvalidGuid)这次不动,见 owner_decision。player_message_utils.cpp(cpp/libs/engine/core/network/)发往 gate 的兼容位映射也不动。

**回归测试**

在 cpp/tests/ecs_context_test/ecs_context_test.cpp 新增 TEST(EcsContextTest, GetPlayerIdReadsGuidOrZero),步骤与断言:(a) create 实体并 emplace\<PlayerGuidComp>(e, 42),断言 GetPlayerId(e)==42;(b) 另建一个不带组件的实体,断言 GetPlayerId 返回 0;(c) destroy(e) 后再 create 复用同一 index(version 不同)并挂 PlayerGuidComp{7},断言 GetPlayerId(旧句柄e)==0、GetPlayerId(新句柄)==7;(d) 断言 GetPlayerId(entt::null)==0。修复前 EcsContext 没有 GetPlayerId,编译失败即为红。另加机械门禁:rg -n "GuidForLog" cpp 必须零命中。

<details><summary>复核者给出的拍板选项</summary>

是否统一「无玩家」哨兵(0 与 kInvalidGuid)。A) 本条只收口读取函数,哨兵保持现状(推荐):各调用点内部口径自洽,创建点拒 0,目前不可达,改动面小。B) 统一成 0:需要同时改 player_lifecycle.cpp 里十余处 kInvalidGuid 判定(退出 :2976/:2997/:3028/:3110、存盘 :4228、:3480、:4624、:4708 等)和 player_mission.cpp:31,创建点 InitPlayerFromAllData 也要补拒 kInvalidGuid,并给退出和存盘路径补缺 guid 的用例。这部分触及存盘与退出收尾,应单独立项,不要夹在本条可读性修复里。只改 :2976 与 :4228 会让 :2997/:3028/:3110 在缺 guid 时变成恒真。

</details>

#### T9.2 W2-b5_types-03（low）grid.cpp 新增的注释声称 try_get 前必须先 valid(),与 EnTT 语义和组件访问规则表矛盾;另有几处与 try_get 等价的冗余 valid() 和复述类型的注释

- 原严重度 low，复核结论「降级」
- 位置：cpp/libs/services/scene/spatial/system/grid.cpp:44,cpp/libs/services/scene/player/system/player_lifecycle.cpp:3071,cpp/libs/services/scene/player/system/player_scene.cpp:77,cpp/libs/services/scene/player/system/player_team.cpp:122

**改法**

1) grid.cpp:44 的注释改为:「try_get 对陈旧句柄安全返回空;这里先 valid() 只是为了把『场景已销毁』(错误,打日志)与『尚无格子表』(正常空结果)分开。」valid() 与 LOG_ERROR 保留。2) player_lifecycle.cpp:3072-3073 删除 `&& tlsEcs.sceneRegistry.valid(sceneComp->sceneEntity)`,:3071 注释改为「源场景已销毁时 try_get 返回空,不记 sceneIdAtExit」。该文件有其他会话的在途改动,需协调后再动。3) 删除 player_scene.cpp:77 和 player_team.cpp:122 两行复述类型的注释。4) 可选:aoi.cpp:224、aoi.cpp:312、interest.cpp:55、interest.cpp:72、player_team_aoi.cpp:59 中紧跟 try_get 的 sceneRegistry.valid() 可以删除。aoi.cpp:268 不要动。

**回归测试**

不需要。改动只涉及注释,以及删除与 try_get 语义等价的冗余 valid()。陈旧句柄下 try_get 返回空是 EnTT 库语义,给它写的用例修复前后都会通过,测不出回归。行为不变由现有 aoi_test、scene 退出相关用例覆盖。

#### T9.3 W2-b5_types-05（low）属性同步模板已改成消费后整体 remove DirtyMaskComp,循环里逐字段 dirtyMask.reset 成了死代码,6 个生成文件共 9 处

- 位置：tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:49,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:39,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:49,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:59,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:69,cpp/libs/services/scene/generated/attribute/attributedelta2framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta5framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta10framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta30framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta60framess2c_attribute_sync.cpp

**改法**

(1) 删除 tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl 第 49 行 `dirtyMaskComp->dirtyMask.reset({{ $.CppClass }}::k{{ .CamelFieldName }}FieldNumber);`,其余不动。

(2) 生成产物不能手改。在仓库根目录执行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`(生成器二进制陈旧时先跑 proto-gen-build),重新生成 6 个 \*_attribute_sync.cpp。

(3) 另一个会话正在改 gRPC→muduo 的模板,全量重生会同时改写那批生成文件。最好等那边模板定型后再重生,或者重生后核对:attribute 目录的 diff 只有这 9 行 reset 被删,其他生成文件的变化属于迁移会话的预期。

**回归测试**

不需要。这是删除不可观察的死代码,改前改后行为完全一样,写不出修复前会红的用例。替代验证:重生后由 Codex 跑 aoi_delivery_test(327-357 行,同步后 DirtyMaskComp 被摘除、空壳也摘除)和 bag_test 里 player_feature_persistence_test 的 969-1008 行(标脏后脏位保持不变),两者应保持绿;再 grep 确认 cpp/libs/services/scene/generated/attribute 下 dirtyMask.reset 零命中。

#### T9.4 W2-cross_sem-07（low）robot 冒烟场景的注释和失败文案仍按旧机制写（“拒绝码必须写 TLS tip”“TRANSFER 宏总会物化 error_message”），与第 2 批之后“handler 直接写 response、成功不创建该字段”的规则相反

- 位置：robot/travel_smoke_scenario.go:255-256,robot/travel_smoke_scenario.go:911-912,robot/travel_smoke_wire.go:37-38,robot/channel_smoke_scenario.go:429-432

**改法**

只改注释和文案，不改判定逻辑。(1) travel_smoke_scenario.go:255-256 改为：“这一步同时是 C++ 侧『拒绝码直接写本次应答』（player_service_interface.h）的回归点：handler 漏写 response->error_message 时，客户端看到的是 tip=0 的『受理』。” (2) travel_smoke_scenario.go:911-912 的 fmt.Errorf 文案改为：“TravelToZone{zone=%d} 回包 tip=0（被受理了）：scene 没校验目标 zone，或 handler 没把拒绝码写进 response->error_message”。(3) travel_smoke_wire.go:37-38 保留 id != 0 的判定，理由改为：“契约只保证 id 非零即拒绝；C++ 成功时不创建 error_message，但不要依赖字段是否存在”。(4) channel_smoke_scenario.go:430-432 删掉“scene 每次应答都会把 error_message 物化成一条空消息（…TRANSFER_ERROR_MESSAGE…），于是每一次被受理的切线都会被打成 WARN…”这一段，只保留“gate 信封拒绝时 body 为空，通用 handler 会误打 enter scene accepted；本场景按 error_message.id 自己判”。不转发的代码不动。(5) 改完后在 PROGRESS 另起一条记录（只追加）。Java 版不涉及（只是本仓 Go robot 的注释）。

**回归测试**

不需要：只改注释和错误文案，判定逻辑与行为不变。“handler 直接写拒绝码”这一行为本身已由 travel_smoke 的 T2 步骤（expectTravelRejected(0, …) 要求 tip != 0）在联机时覆盖。

#### T9.5 W1-39（low）3 份设计文档仍描述已删除的 ScenePlayers 反向集合

- 位置：docs/design/scene-creation-architecture.md:344,docs/design/agones-scene-node-high-density.md:23,docs/design/async-load-disconnect-reconnect-race.md:45

**改法**

1) scene-creation-architecture.md:344 改为「3. Emplaces `SceneInfoComp` (config_id, guid). 场景成员不另存反向集合:玩家只挂 `SceneEntityComp`,需要成员时扫描 `view<SceneEntityComp, PlayerGuidComp>`(见 ecs-component-access-rules.md「同步 trigger…场景成员只以 SceneEntityComp 为准」段)。」

2) agones-scene-node-high-density.md:23 改为「`sceneRegistry.create()` + 挂 `SceneInfoComp`」,删掉「/ `ScenePlayers`」。

3) async-load-disconnect-reconnect-race.md:45 改为「-> 给玩家挂 SceneEntityComp 绑定到场景,并加入 AOI 网格」。

不改 ecs-entt-misuse-audit-20261015.md 和 PROGRESS.md 中的历史引用。

**回归测试**

不需要。纯设计文档修正,不涉及代码行为;改完后用 git grep -n ScenePlayers -- docs/design 检查,只应剩 ecs-entt-misuse-audit-20261015.md 中的审计条目。


### T10 调试工具：打印实体的全部组件（负责人点名）

负责人问过「entt 能不能看到某个 entity 的所有组件」。EnTT 3.13.2 没有「实体 → 组件」的列表，但可以遍历全部组件池逐个判断。这里做一个只读函数、一个 GM 命令，并在一处低频出错路径打日志。代价是遍历所有组件池（几百个量级），**不进每帧逻辑**。

**文件**

- 新建 `cpp/libs/engine/core/utils/registry/describe_entity.h`：只放声明和模板实现，engine 层不认识任何业务组件。
- `cpp/libs/services/scene/player/system/player_database_loader.cpp`：出错路径接入。
- `proto/scene/scene_admin.proto`、`cpp/nodes/scene/handler/rpc/scene_admin_handler.cpp`：GM 命令，handler 只改守护段。
- `cpp/tests/ecs_context_test/ecs_context_test.cpp`：测试。
- 新文件登记进对应的 vcxproj、filters 和 CMakeLists。

**1) 只读函数**

```cpp
// describe_entity.h
// 排障用:列出 entity 当前挂着的全部组件。遍历 registry 的每个组件池,代价 O(池数);
// 只在 GM 命令和低频出错路径调用,不进每帧逻辑。陈旧句柄和 entt::null 都安全(contains 按版本比较)。
template <typename Registry>
std::string DescribeEntity(const Registry& registry, typename Registry::entity_type entity)
{
    std::string out = "entity=" + std::to_string(entt::to_integral(entity));
    if (!registry.valid(entity)) {
        return out + " valid=0";
    }
    out += " valid=1 components=[";
    bool first = true;
    for (auto [id, storage] : registry.storage()) {
        if (!storage.contains(entity)) {
            continue;
        }
        std::string_view name = storage.type().name();   // MSVC 下形如 "struct BuffListComp"
        for (std::string_view prefix : {"struct ", "class ", "enum "}) {
            if (name.substr(0, prefix.size()) == prefix) {
                name.remove_prefix(prefix.size());
                break;
            }
        }
        out += first ? "" : ",";
        out += name;
        first = false;
    }
    return out + "]";
}
```

要点：

- `registry.storage()` 只遍历组件池，不含实体池。组件为空时输出空列表就能说明问题，不必再调 `registry.orphan(entity)`。
- 只输出类型名，**不输出组件值**：值里有玩家名、外观等个人信息，出错日志里不要打。需要看值时用下面 GM 命令的 `include_values`。
- SceneRegistry（`basic_registry<SceneEntity>`）同样可用，模板参数自动推导。
- 不要为它新增任何组件或全局表。

**2) GM 命令（只用于看值，鉴权与停机 GM 同一套）**

在 `proto/scene/scene_admin.proto` 的 `service SceneScene` 末尾追加。这是服务端内部服务，没有标客户端协议，不改客户端契约：

```proto
message GmDescribeEntityRequest
{
  string operator = 1;      // 审计用
  string reason = 2;        // 必须等于 player_id 的十进制串:让签名覆盖目标,防止拿一条合法请求换目标重放
  uint64 player_id = 3;
  bool include_values = 4;  // true 时附带持久 protobuf 组件的 ShortDebugString
}

message GmDescribeEntityResponse
{
  string description = 1;
}

// service SceneScene 内追加:
  rpc GmDescribeEntity (GmDescribeEntityRequest) returns (GmDescribeEntityResponse);
```

`SceneSceneHandler::GmDescribeEntity` 守护段：

- 先调 `gate_security::VerifyGmRequestFromEnv("Scene.GmDescribeEntity", selfNodeId, request->operator_(), request->reason())`。失败时的处理与 GmGracefulShutdown 完全一致：截断后打 ERROR，`controller->SetFailed`。再校验 `request->reason() == std::to_string(request->player_id())`，不相等同样拒绝。
- `const auto player = tlsEcs.GetPlayer(request->player_id());` 找不到就写 `description = "player not on this node"` 并返回。
- `description = DescribeEntity(tlsEcs.actorRegistry, player)`。
- `include_values` 为 true 时，再遍历一次 `registry.storage()`。对每个包含该实体的池，按 `storage.type().hash()` 查一张**只定义在这个 handler 文件里**的表 `std::unordered_map<entt::id_type, std::string (*)(const void*)>`。负责人已确认只登记当前 13 个 protobuf 组件：`Transform`、`PlayerUint64Comp`、`PlayerSkillListComp`、`PlayerUint32Comp`、`BaseAttributesComp`、`LevelComp`、`PlayerAttributeComp`、`PlayerPetComp`、`PlayerProfileComp`、`PlayerAssetOpLedgerComp`、`BattleSettlementLedgerComp`、`PlayerInboxLedgerComp`、`CurrencyComp`。值为 `[](const void* p) { return static_cast<const T*>(p)->ShortDebugString(); }`。另外 3 个普通 C++ 组件 `PlayerBagsComp`、`MissionsContainerComp`、`PlayerCurrencyComp` 只由 DescribeEntity 列出类型名，不加值序列化适配。用 `storage.value(entity)` 取 `const void*`，空类型组件返回 nullptr 时跳过。每个值截断到 4 KB，总长截断到 64 KB。
- 日志只记 operator、player_id 和组件数量，不记值。

**3) 出错路径接入**

只接一处低频路径：player_database_loader.cpp 的 PlayerDatabaseMessageFieldsMarshal 在持久组件缺失、拒绝 marshal 时，在现有 `[PlayerSave] ... missing=...` 那行 LOG_ERROR 末尾追加 `<< " " << DescribeEntity(tlsEcs.actorRegistry, player)`。这条路径只有加载不变量被破坏时才会走到（复核确认生产不可达），打出全部组件可以直接看出缺了谁、多了谁。不要接进每帧或每条消息都会走的路径。

**4) 测试**（ecs_context_test，新增 `DescribeEntityTest`）

- `ListsExactlyTheAttachedComponents`：create 一个实体，emplace `PlayerGuidComp{42}` 和测试内定义的空标签 `struct DescribeProbeTag {}`；另建一个实体，挂测试内定义的 `struct DescribeOtherProbe { int v; }`。断言输出以 `entity=` 开头、含 `valid=1`、含 `PlayerGuidComp` 和 `DescribeProbeTag`，不含 `DescribeOtherProbe`。
- `StaleAndNullHandlesAreSafe`：destroy 后用旧句柄调用，断言输出含 `valid=0`、不抛异常、不创建任何池（调用前后 `registry.storage()` 的池数量相同）；`entt::null` 同样断言。
- `WorksForSceneRegistry`：在 `tlsEcs.sceneRegistry` 新建一个场景实体，挂测试内定义的 `struct DescribeSceneProbe { int v; }`，断言输出含 `DescribeSceneProbe`。
- 修复前函数不存在，编译即红。GM 命令不进单测（handler 没有测试工程），由 Codex 本地起 scene 节点后，用一条签名请求冒烟一次。

**5) Java 版**：不实现（Java 不用 EnTT）。ContractSync 会同步新的 message_id，核对 Java 的 `MessageIdRegistryTest` 仍然通过，并在 PARITY.md 登记「C++ 独有的调试命令，Java 不适用」。

### T11 规则文档收口（一次改完 ecs-component-access-rules.md）

`docs/design/ecs-component-access-rules.md` 是 AGENTS §7.5 指定的组件访问权威规则，多条任务都要改它。为避免反复改同一个文件，全部改动在这一步做，按段落定位：

1. **句柄安全表**（W1-35）：拆成两组。
   - 「可直接用于陈旧句柄」：valid、all_of、any_of、try_get、remove、storage.contains；remove 保留 swap-and-pop 的说明。
   - 「必须先 valid()」：emplace、emplace_or_replace、get_or_emplace，理由写「try_emplace 不看版本，Release 下覆盖同槽位活实体的稀疏映射；槽位空闲时把组件挂到死句柄上，Debug 也不报错」；get、patch、replace，理由写「index() 在 Release 下忽略版本，读写同槽位活实体的组件或越界」；erase、storage.erase，理由写「按 index 删除同槽位活实体的组件」；destroy 保留现有说明，并指向带 valid 保护的 DestroyEntity helper，注明 SceneRegistry 没有对应重载、须手写 valid。
   - 第 13 行 get_or_emplace 那一行末尾补「按需写入口调用前须确认 valid(entity)」。
2. **dispatcher 与通知**（W1-23 第 (7) 条、W1-36）：
   - 第 83 行「同步 trigger 可以销毁实体……返回后重验完整句柄」整句删除，改为：「没有消费者不发事件；dispatcher 只做可有可无的一对多通知，必需步骤直接调用；监听者不得销毁或替换实体，不得改 SceneEntityComp、playerList 或会话；trigger 之后不写恢复分支」。
   - 第 59 行改为「默认技能、任务 scope、派生投影由加载链直接完成，不发初始化通知」。
   - 同一段里「销毁兜底与解除 Pin 也清远端观察关系」按 T3.4 的结果改为「离场只清 7 格内的观察者」。
3. **哪些数据进 registry**（W2-fit-05）：在「Overview」和「Three Access Patterns」之间新增一节，不超过 8 行：
   - 进 registry：大量同构实体、被 view 按组件组合批处理的数据，或者需要按实体 O(1) 挂载、随实体销毁回收的数据；
   - 不进：进程单例、请求级数据、连接与网络资源、跨实体共享对象、私有索引；
   - registry 内的单例只允许是该 registry 的派生索引，放 ctx()（现例 ActorNetIdIndex）；
   - 多个 registry 用自定义实体类型区分（现例 SceneEntity）；
   - 现存例外：节点 registry 上的 RpcClientPtr、MuduoGrpcChannelPtr 属存量，迁出见报告 §4.2。
   - 同时把 `docs/design/ecs_zh.md`、`ecs.md`、`ecs_en.md` 第 10 行的末句「单例组件不免除这些所有权约束」改为「registry 内单例只允许是该 registry 的派生索引（放 ctx()）；进程单例、请求级数据和网络资源不进 registry，判据见组件访问规则」，三份同改。
4. **InitializeOnCreate**（W2-b1_runtime-09、W2-b1_runtime-05）：第 57 行删掉「重复调用重算投影，不清零……也不清除……脏标记」，改为「新建实体时调用一次，只挂空的 CombatStateFlagsComp」；第 25、57 行删掉 MoveSpeedComp。
5. **InBattleComp**（W2-b1_runtime-10）：第 24 行的按需状态示例去掉 InBattleComp；第 55 行补「三处业务入口各自先确认不存在、再 emplace」。
6. **BuffPendingRemoveBuffs 与具名容器**（W2-b1_runtime-06、W2-b5_types-04）：T6.6 整套删除时，删掉第 45 行的表项，第 95 行的具名类型列表去掉 BuffPendingRemoveBuffs；第 95 行「基类容器转换必须显式」改为「不提供从基类容器到组件类型的转换（继承构造天然排除基类的拷贝 / 移动构造）」。
7. **延后应答**（W2-b2-05）：第 69 行删掉「，延期应答写本次 `RpcDoneCallback` 的 response」。
8. **gRPC 存量边界**（W1-42）：第 95 行删掉「官方 gRPC Channel/CQ/Stub 等」和「ECS 收尾已补取消、稳定缓冲与非阻塞排空源码；」两段，存量边界只保留 RpcClientPtr 与 MuduoGrpcChannelPtr。
9. **节点 registry**（W2-fit-06）：「实体类型与生命周期边界」表中「节点路由句柄」一行加注：节点 registry 是待移出的存量，不得再往节点实体上新增组件种类。
10. **拒存**（W1-01，负责人已选 B）：第 61 行按 T2.1 改法第 10 条改写。

与上述条目同批改的其他文档：

- `docs/handoff/ecs-refactor/README.md:149`「传输组件寿命」一行改为现状（W1-44）：缺通道时异步失败一次；选点要求挂着 MuduoGrpcChannelPtr（DataService 还要求 kReady）；摘节点先移除通道，~Channel 把在途调用以 UNAVAILABLE 排队，再销毁实体。删掉 CQ、tag、closing 等字样。README 其余部分是历史回执，不改原文。
- `docs/handoff/ecs-refactor/ecs-fit-overview.md:121` 按 T3.9（W1-38）改法第 4 条改。

**验收**：`git grep -n "trigger 可以销毁\|单例组件不免除\|MoveSpeedComp\|BuffPendingRemoveBuffs" -- docs/design` 只剩审计文档中的历史引用；人工核对句柄安全表里每个 API 的归组与 EnTT 3.13.2 源码一致。

## 5. 不在本单的条目

- **不成立**：W1-08。候选事务每次整包复制是文档化、已接受的原子性代价，没有性能数据支撑另开快路径。
- **已被改写**：gRPC→muduo 迁移会话删掉了 CQ、Stub、GrpcEtcdTransport 和 5ms 排空轮询，原问题不再以原形式存在。涉及 W1-41、W1-42（文档部分并入 T11 第 8 条）、W1-43、W1-44（文档部分并入 T11）、W1-46、W2-b2-09。
- **移交迁移会话**（不在 ECS 范围）：
  - W1-45：路由服摘除后，~Channel 已经把在途调用以 UNAVAILABLE 回调，gate lane 的 OnRouterNodeRemoved、routerEntity、nodeRemoved 计数和 ExtractIf 成了重复机制。推荐在迁移删除官方 CQ 回退分支时一并删除（复核记录见报告附录 B）。
  - W1-43：迁移会话重写 grpc_client_lifecycle_test.cpp 时，整条删除两个流式用例，不要改写成新形式。
  - W1-41：LeaseRevoke 维持迁移设计 §13.7 的「尽量送达」。如果观察到停机后仍有流量打到已停实例，再改成带完成回调并等待有界截止。
- **未复核**：W2-b5_types-07（算术类型组件护栏未落地）。它是 low 且低置信度，按交接规则不复核；与 W2-fit-05「不做棘轮脚本」的口径一致，本单不做。
- **审计文档**：W2-cross_sem-11 指出的审计文档过时描述，已由审查方在本轮更新 HEAD 状态时改正。README §5 规定审计文档由审查方更新，Codex 不改。

## 6. 验证顺序（交 Codex，均未执行）

1. **重生成**：T1.10、T5、T7.7、T7.8、T8.8、T9.3、T10 改完后。在仓库根目录执行：
   ```powershell
   pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
   pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml
   ```
   核对守护段内的手写代码一行不丢。本单不删除事件 proto，不需要设 `PROTOGEN_ALLOW_MASS_EVENT_TOMBSTONE`。生成器不会自动删除陈旧产物，按 T5.1 补充部分第 (4) 条手删。
2. **生成器测试**：`cd tools/proto_generator/protogen; go test ./internal/...`，包含 T5.1 新增的两条用例。
3. **C++ 构建**：`msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`，必须串行。EcsContext、Bag 和组件布局都有变化，禁止混链旧 obj / lib。
4. **C++ 测试**：`pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter <工程>`，依次跑 bag_test、cross_zone_test、skill_test、buff_test、aoi_test、aoi_delivery_test、kafka_command_test、missions_test、routing_identity_test、ecs_context_test、rpc_controller_test、currency_test、timer_queue_unit_test。新增用例按各条「回归测试」写的方式确认修复前为红；对照验证放在忽略目录的副本里做，不在共享工作区回退代码。保留 build / test 日志与 JUnit XML，失败时不连续盲目重试。
5. **Java**（T1.10、T8.8、T4.1 与 T7.10 的 PARITY 及契约文档、T7.6 核对、T10 登记）：在 `D:/luyuan/xuanming-server-mmo` 执行 `mvn -pl game-proto -am compile` 与 `mvn -pl game-scene -am test`；ContractSync 之后核对 MessageIdRegistryTest。
6. **本地冒烟**：用 `dev_tools.ps1` 起 scene 和 gate，覆盖进场、换场景、离场、对尸体施法、CreateScene / DestroyScene 的 Agones 计数日志，以及一条签名的 GmDescribeEntity。
7. **机械门禁**：执行各条「回归测试」里列出的 git grep 零命中检查，结果贴进回执。

## 7. 回执

按 [README.md](README.md) §6 的格式交回，另加两项：

- 逐个任务号写「已做」或「未做（原因）」；某项默认决定实施中行不通的，写明卡在哪里，等负责人决定，不自行改走退路；
- 核心拍板 1–4 对应任务（T4.1、T2.1、T7.10、T1.10）的处理结果。

在 `docs/PROGRESS.md` 末尾追加一条（只追加），写明 Java 版状态（AGENTS §12）。审计文档的 HEAD 状态由审查方在拿到运行证据后再更新，Codex 不改。

## 8. Codex 实施暂停回执（2026-10-16，未完成整批）

### 暂停原因与待裁定内容

按负责人「某项行不通就停下回报，不要自行改走退路」执行，全批已暂停，所有子代理已停止。没有回退既有改动，没有 commit、push 或手改生成产物。

T10 的 GM 值表要求「15 个持久 protobuf 组件，清单与 PlayerDatabaseMessageFieldsMarshal 相同」。当前该函数校验 16 类组件，其中只有以下 **13 类 protobuf**：`Transform`、`PlayerUint64Comp`、`PlayerSkillListComp`、`PlayerUint32Comp`、`BaseAttributesComp`、`LevelComp`、`PlayerAttributeComp`、`PlayerPetComp`、`PlayerProfileComp`、`PlayerAssetOpLedgerComp`、`BattleSettlementLedgerComp`、`PlayerInboxLedgerComp`、`CurrencyComp`。

其余 `PlayerBagsComp`、`MissionsContainerComp`、`PlayerCurrencyComp` 为普通 C++ 类型，没有 `ShortDebugString()`；背包、任务由专门 marshal 转为持久消息，货币运行时欠款通过 `SaveToProto` 合入 `CurrencyComp`。因此不能按原清单登记 15 个 protobuf 类型。**尚未自行缩成 13 项，也未为普通组件新增序列化适配。需负责人明确 T10 的值表清单后再恢复。**

当前 T10 只完成只读类型枚举、一个用例、故障日志接入及 proto 源声明；GM handler、另外两个测试与生成尚未做。源码处于部分实施状态，不能据此认为工作区可整批构建。

### 逐任务状态

表中「已做」仅指该项源码改动；除下文实际运行的定向用例外，均未验证。部分实施统一记「未做完」，原样保留，恢复时继续。

| 任务 | 状态与剩余工作 |
|---|---|
| T1.1 | 已做（源码）；承接其他会话已改的扣空断言，补占格/一致性断言，零堆合并测试迁入 ItemStore；未运行，原五红已不适用于接手时的工作区 |
| T1.2 | 已做（源码）；改最终用例名与过时注释，已有预设 GUID 成功语义保留；未运行 |
| T1.3 | 已做（源码）；TearDown 恢复 item 范围 `(1, uint64_t{1} << 54)`；未运行 |
| T1.4 | 已做（源码）；Clone 动态类型 Debug 断言及死亡测试；未确认红绿 |
| T1.5 | 已做（源码）；批量 RestoreItems、还原内部入口和两类包加载调用、旧零章测试；未确认红绿 |
| T1.6 | 已做（源码）；五处日志/Debug 断言和指定函数缩进；未运行 |
| T1.7 | 未做完；CanMintGuid(s) 与主要头注释已删改，item_store.cpp 空堆并堆注释及设计文档发号行仍待改 |
| T1.8 | 未做完；源码及整批溢出临时格测试已写，04-asset-channel 文档三处、RemoveItemsByGuid 残留注释待改；未运行 |
| T1.9 | 未做（暂停，尚未开始） |
| T1.10 | 未做（暂停，item_comp.proto 未删，Java 未同步） |
| T2.1 | 未做完；只追加第1批回执更正及头注释，拒存行为和测试均未改 |
| T2.2 | 未做（暂停，尚未开始） |
| T2.3 | 已做；结算持久化两行注释及 DEBUG 文案修正，无行为改动 |
| T2.4 | 未做完；头文件清单补 DBTask 闸门及当前新增的 SavePlayerForAssetCommit，cpp 重复清单尚未删 |
| T3.1 | 未做（暂停，尚未开始） |
| T3.2 | 未做（暂停，尚未开始） |
| T3.3 | 未做（暂停，尚未开始） |
| T3.4 | 未做（暂停，尚未开始） |
| T3.5 | 未做（暂停，尚未开始） |
| T3.6 | 未做（暂停，尚未开始） |
| T3.7 | 未做（暂停，尚未开始） |
| T3.8 | 未做（暂停，尚未开始） |
| T3.9 | 未做（暂停，尚未开始） |
| T4.1 | 未做（暂停，T3 前置未完成） |
| T4.2 | 未做（暂停，T3 前置未完成） |
| T4.3 | 未做（暂停，T3 前置未完成） |
| T4.4 | 未做（暂停，T3 前置未完成） |
| T5.1 | 未做完；直接 handler 路由、生成器/模板与 Node hook 源码已改，未重生/删陈旧产物/改清单/验证 C++ |
| T5.2 | 已做（测试源码）；加入49号并改上界为 kMaxEventCount；C++ 红绿未运行 |
| T5.3 | 已做（源码）；删除 local_event_cleanup 源码、测试和 pipeline 调用；完整 prototools 验证未运行 |
| T5.4 | 未做（暂停，尚未开始） |
| T5.5 | 未做（暂停，尚未开始） |
| T5.6 | 未做（暂停，尚未开始） |
| T6.1 | 未做（暂停，T3 前置未完成） |
| T6.2 | 未做（暂停，T3 前置未完成） |
| T6.3 | 未做（暂停，T3 前置未完成） |
| T6.4 | 未做（暂停，T3 前置未完成） |
| T6.5 | 未做（暂停，T3 前置未完成） |
| T6.6 | 未做（暂停，T3 前置未完成） |
| T6.7 | 未做（暂停，T3 前置未完成） |
| T6.8 | 未做（暂停，T3 前置未完成） |
| T7.1 | 未做（暂停，T3 前置未完成） |
| T7.2 | 未做（暂停，T3 前置未完成） |
| T7.3 | 未做（暂停，T3 前置未完成） |
| T7.4 | 未做（暂停，T3 前置未完成） |
| T7.5 | 未做（暂停，T3 前置未完成） |
| T7.6 | 未做（暂停，T3 前置未完成，Java 未核对） |
| T7.7 | 未做（暂停，T3 前置未完成） |
| T7.8 | 未做（暂停，T3 前置未完成） |
| T7.9 | 未做（暂停，T3 前置未完成） |
| T7.10 | 未做（暂停，两版契约文档尚未改） |
| T8.1 | 未做（暂停，尚未开始） |
| T8.2 | 未做（暂停，尚未开始） |
| T8.3 | 未做（暂停，尚未开始） |
| T8.4 | 未做（暂停，尚未开始） |
| T8.5 | 未做（暂停，尚未开始） |
| T8.6 | 未做（暂停，尚未开始） |
| T8.7 | 未做（暂停，尚未开始） |
| T8.8 | 未做（暂停，NodeInfoListComp 未删，Java 未同步） |
| T8.9 | 未做（等待迁移会话提交指定文件；接手时 rpc_client.h 仍有未提交改动） |
| T8.10 | 未做（暂停，尚未开始） |
| T8.11 | 未做（暂停，尚未开始） |
| T8.12 | 未做（暂停，尚未开始） |
| T8.13 | 未做（暂停，尚未开始；未新增 NodeDirectory） |
| T9.1 | 未做（暂停，尚未开始） |
| T9.2 | 未做（暂停，尚未开始） |
| T9.3 | 未做完；attribute_sync 模板逐字段 reset 已删，未重生和运行替代验证 |
| T9.4 | 未做（暂停，尚未开始） |
| T9.5 | 未做（暂停，尚未开始） |
| T10 | 未做完（值表清单冲突见上文；只读函数首个用例已有红绿证据） |
| T11 | 未做（暂停，尚未开始） |

核心拍板处理：1 / T4.1、3 / T7.10、4 / T1.10 均未实施；2 / T2.1 仅更正回执和头注释，**拒存退出仍是旧行为**。没有撤销任何已确认拍板。

### 本会话触及文件（含接手时他人的差异，不能视为纯本会话 diff）

```text
cpp/libs/modules/bag/bag_service.{h,cpp}
cpp/libs/modules/bag/bag_system.{h,cpp}
cpp/libs/modules/bag/container_layout.h
cpp/libs/modules/bag/item_store.{h,cpp}
cpp/libs/services/scene/player/system/bag_marshal.cpp
cpp/libs/services/scene/player/system/asset_op_system.cpp
cpp/libs/services/scene/player/system/player_equip.cpp
cpp/libs/services/scene/player/system/player_lifecycle.h
cpp/libs/services/scene/player/system/player_database_loader.cpp
cpp/libs/services/scene/battle/system/player_battle.cpp
cpp/tests/bag_test/bag_java_parity_test.cpp
cpp/tests/bag_test/bag_selection_order_test.cpp
cpp/tests/bag_test/bag_test.cpp
cpp/tests/bag_test/player_battle_settlement_test.cpp
cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp
cpp/tests/ecs_context_test/ecs_context_test.cpp
cpp/libs/engine/core/utils/registry/describe_entity.h（新增）
cpp/libs/engine/core/core.vcxproj
cpp/libs/engine/core/core.vcxproj.filters
cpp/libs/engine/core/CMakeLists.txt
cpp/libs/engine/core/node/system/node/node_entry.h
proto/scene/scene_admin.proto
tools/proto_generator/protogen/cmd/pipeline.go
tools/proto_generator/protogen/internal/generator/cpp/event.go
tools/proto_generator/protogen/internal/generator/cpp/gate_kafka_command_router.go
tools/proto_generator/protogen/internal/generator/cpp/gate_kafka_command_router_test.go
tools/proto_generator/protogen/internal/generator/cpp/scene_kafka_command_router.go（新增）
tools/proto_generator/protogen/internal/generator/cpp/service_register_info.go
tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl
tools/proto_generator/protogen/internal/template/event_handler.cpp.tmpl
tools/proto_generator/protogen/internal/template/event_handler.h.tmpl
tools/proto_generator/protogen/internal/template/gate_kafka_command_router.cpp.tmpl
tools/proto_generator/protogen/internal/template/scene_kafka_command_router.cpp.tmpl（新增）
tools/proto_generator/protogen/internal/template/rpc_event_registry.cpp.tmpl
tools/proto_generator/protogen/internal/template/service_header.tmpl
tools/proto_generator/protogen/internal/template/event_handler_total.cpp.tmpl（删除）
tools/proto_generator/protogen/internal/template/event_handler_total.h.tmpl（删除）
tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go（删除）
tools/proto_generator/protogen/internal/prototools/local_event_cleanup_test.go（删除）
docs/handoff/ecs-refactor/batch1-codex-receipt.md（仅追加更正批注）
docs/handoff/ecs-refactor/batch6-review-fixes.md（本节回执）
docs/PROGRESS.md（末尾追加）
```

### 实际验证与日志

- T10 红：先写 `DescribeEntityTest.ListsExactlyTheAttachedComponents`，用实际 ecs_context_test.vcxproj 构建，退出1；静态门禁解析该测试时报告 `describe_entity.h file not found`（尚无函数实现）。日志 `build/ecs-refactor/batch6/t10-red-build.log`。
- T10 绿：新增实现后，同工程 `/m:1 /nr:false /p:Configuration=Debug /p:Platform=x64` 构建退出0；运行 `build/cpp/tests/ecs_context_test.exe --gtest_filter=DescribeEntityTest.ListsExactlyTheAttachedComponents --gtest_output=xml:build/ecs-refactor/batch6/t10-green.xml`，1通过、0失败。构建、测试日志分别为 `t10-green-build.log`、`t10-green-test.log`。尚无陈旧/空句柄、SceneRegistry 的运行证据。
- T5.1 红：在 protogen 下 `go test ./internal/generator/cpp -run '^TestKafkaIngressDispatchCallsHandlerDirectly$' -count=1`，退出1；仍有 dispatcher.trigger，与直调断言不符。日志 `build/ecs-refactor/batch6-t5-red.log`。
- T5.1 绿：`go test ./internal/generator/cpp -run 'TestKafkaIngressDispatchCallsHandlerDirectly|TestGateKafkaRouter' -count=1`，退出0。日志 `build/ecs-refactor/batch6-t5-green.log`。这是暂停前已在途的测试，非暂停后新增运行。
- 本会话修改的 bag、DescribeEntity 用例及 scene_admin proto 定向 `git diff --check` 退出0；未将其扩大解释为整个共享工作区通过。
- 对照快照：`build/ecs-refactor/batch6-t1-before/` 与 `build/ecs-refactor/batch6-red/t2/`，均在忽略目录。T1.4、T1.5 的红测及 T1.1 变异验证尚未运行。
- T2 测试补丁第一次写 cross_zone_test.cpp 失败且未落盘；只读诊断确认非只读、之后可 ReadWrite 打开，但无法确定首次原因，没有盲重试。生命周期行为代码尚未修改。
- 启动门禁：已读规范、任务/设计、最近20条日志及工作区；无 gh，改用不登录的 GitHub 公开 issues API（含PR）读取，当前 open 列表为空。

### 未执行验证、偏差与恢复前提

§6 的统一重生成、生成器完整测试、game.sln 串行重编、13个 C++ 工程完整测试、Java 构建/测试、节点冒烟和全部机械门禁均未执行，原因是上述清单冲突触发全批暂停。未绕过断言或跳过门禁。只读调试工具的独立红绿使用最小 ecs_context_test 工程，不代表执行过 §6 的完整构建。

恢复后先裁定 T10 值表，再继续未完成步骤。T5 的生成器接口已变而产物尚未重生，需统一生成并清理陈旧 handler/汇总及三份工程清单；battle 空 RegisterNodeEvents 文件也待清理。gate handler 仍用 tlsEcs，模板移除 ecs_context include 后的显式依赖待核对。T5.4–T5.6 文档应引用最终保留的 DispatchSceneKafkaCommand/DispatchGateKafkaCommand。T7.8 可接续修改已释放的 attribute_sync 模板，之后统一重生成。

Java 状态：**待做**。本批未修改 Java 仓库，未运行 ContractSync、Maven 或 MessageIdRegistryTest，未登记完成功能版本；现有 `0.1.0-SNAPSHOT` 不作为本批完成证据。4项拍板和共享契约同批对齐仍待恢复后完成。审计文档 HEAD 状态未改，W1-45/W1-43 未触碰。


## 9. Codex 恢复推进回执（2026-10-16，整批仍未完成）

负责人已裁定 T10 使用当前 13 个 protobuf 组件登记值表，另 3 个普通 C++ 组件仅列类型名；该口径已更新到任务正文并完成对应源实现。§8 是此前因 15/13 清单冲突而暂停的历史记录，不能作为当前逐项状态。

### T1.2 第二项变异的实际检出方式

本批已推进并通过 T3 全组；T3.7–T3.9 的 core/modules/gate/scene 构建均退出 0，skill 45/45、currency 184/184，异常窗口 4 例均已执行，零失败/错误/跳过/禁用。现进入 T4/T6/T7 测试阶段。T1.2 第二项变异要求按淘汰前规划计算铸号数；该输入淘汰前需 1 号、最终需 2 号，按原改法会到达 T1.6 同批要求保留的“预取身份数与规划不一致”Debug 断言。实测修后基线用例 1/1 通过，隔离变异的 modules 与测试工程均编译成功，但指定用例运行时在变异 bag_system.cpp:739 断言终止：原生退出码 3、未超时、无 JUnit；尚未执行到该用例的 EXPECT_EQ 返回值断言。生产断言、Debug 配置和测试断言均未修改。

Codex 曾额外要求变异必须产生 JUnit 断言红，并据此暂停核对；复核任务单原文只要求“变红”，该额外条件不属于负责人要求，现已撤回确认问题并继续 T3.4。本次按表变异确实被现有 Debug 不变量断言检出，按实际出口记录，不伪称产生了 JUnit 断言失败。没有切换 Release、定义 NDEBUG 或绕开断言，没有变更负责人既定方案，也不需要新的负责人裁定。

隔离证据：build/ecs-refactor/batch6/t1-2-mint-diagnostic-20261016T103008997Z/，含 baseline.xml、两个 build 日志/退出码、mutant stdout/stderr/原生退出码、diagnostic.json、proof.json 与冻结的 mutated-bag_system.cpp。输入源码 SHA 前后核对无变化；该诊断没有回退或覆盖共享工作树。

### 当前逐任务状态

下表的“源码已做”不代表整批验收完成。最终生成、整批构建、指定修前红/变异、Java 与冒烟仍须按 §6 原顺序完成。

| 任务 | 当前状态 |
|---|---|
| T1.1 | 源码已做；常规 bag 全量通过，指定变异/修前红仍待执行 |
| T1.2 | 已做并验证：第一项基线 6 绿、变异仅指定 1 红且其余 5 绿；第二项按表变异实测由 Debug 断言提前检出（退出 3、无 JUnit） |
| T1.3 | 源码已做；常规 bag 全量通过 |
| T1.4 | 源码已做；常规 bag 全量通过，指定变异/修前红仍待执行 |
| T1.5 | 源码已做；常规 bag 全量通过，指定变异/修前红仍待执行 |
| T1.6 | 源码已做；常规 bag 全量通过 |
| T1.7 | 源码已做；常规 bag 全量通过 |
| T1.8 | 源码已做；任务指定的失败零改动、整批掉落转临时格和装备扣空替代护栏已在 bag 全量通过，本项不要求修前红 |
| T1.9 | 已做：指定背包文档已同步，另修 API 映射表残留 |
| T1.10 | C++ 源 proto 已删；最终重生成与 Java 同步待做 |
| T2.1 | 源码已做；cross_zone 常规回归通过，T2 隔离九红及在线保护对照尚未执行 |
| T2.2 | 源码已做；cross_zone 常规回归通过，T2 隔离九红及在线保护对照尚未执行 |
| T2.3 | 源码已做；cross_zone 常规回归通过，T2 隔离九红及在线保护对照尚未执行 |
| T2.4 | 源码已做；cross_zone 常规回归通过，T2 隔离九红及在线保护对照尚未执行 |
| T3.1 | 已做并验证：bag 586、cross_zone 232、aoi 78、aoi_delivery 23，全部零失败/跳过 |
| T3.2 | 已做并验证：scene 重编成功，bag 584/584，零失败/跳过 |
| T3.3 | 已做并验证：scene 重编成功，cross_zone 233/233，零失败/跳过 |
| T3.4 | 已做并验证：修前指定用例 2 条断言红；删 Pin/Unpin 与远端扫描后 scene 重编成功，aoi 71/71、aoi_delivery 20/20，零失败/跳过/禁用 |
| T3.5 | 已做并验证：无 sink 击杀回归修前 2 断言红；直接调用后 modules/scene 重编成功，bag 585/585、missions 17/17，零失败/跳过/禁用 |
| T3.6 | 已做并验证：三条修前红共 5 条目标断言；scene 重编成功，bag 586/586、skill 45/45、buff 35/35，零失败/跳过/禁用 |
| T3.7 | 已做并验证：死字段、空事件/API 及死头文件已删，四库重编成功、skill 45/45 |
| T3.8 | 已做；别名与注释已清，T4.2 的两处 AOI 旧注释已随入口回滚删除，机械门禁待最终复核 |
| T3.9 | 已做并验证：上下文唯一写入口已收口、ClearPlayer 已删，skill 45/45、currency 184/184，异常窗口 4 例保留且实际执行 |
| T4.1 | 源码已做；指定三条修前红已确认，两条 cross 修后随 253 全量绿；bag 初始化绿测与 Java PARITY 待步骤 4/5 |
| T4.2 | 已做：计数事件、回滚与死壳已删，当前 scene 库重编成功、cross 全量绿；Agones 日志冒烟待步骤 6 |
| T4.3 | 源码已做；4 处扫描收口、gRPC scene_id=0 拒绝保留，config_id 分叉保留；场景库与 cross 绿，节点最终编译/冒烟待后续 |
| T4.4 | 已做并验证：两函数收入 scene 库，三工程清单已登记；抽取前 cross 234/234、后 253/253，原 234 条全部保留、3 契约全绿，另 16 条来自并行 ReleaseFence 会话 |
| T5.1 | 源码/模板及指定文档已做；最终统一生成、陈旧产物清理与 Kafka C++ 回归待做 |
| T5.2 | 源码/模板及指定文档已做；最终统一生成、陈旧产物清理与 Kafka C++ 回归待做 |
| T5.3 | 源码/模板及指定文档已做；最终统一生成、陈旧产物清理与 Kafka C++ 回归待做 |
| T5.4 | 源码/模板及指定文档已做；最终统一生成、陈旧产物清理与 Kafka C++ 回归待做 |
| T5.5 | 源码/模板及指定文档已做；最终统一生成、陈旧产物清理与 Kafka C++ 回归待做 |
| T5.6 | 源码/模板及指定文档已做；最终统一生成、陈旧产物清理与 Kafka C++ 回归待做 |
| T6.1 | 源码已做；尸体回归修前 2 目标断言红，当前场景库已构建，完整 skill 绿测待正式步骤 4 |
| T6.2 | 源码已做；复核只留 4 个定时器入口，当前场景库已构建，替代护栏完整 skill 绿测待做 |
| T6.3 | 源码已做；无相位技能上下文修前 1 断言红，当前场景库已构建，skill 绿测待做 |
| T6.4 | C++ 源码已做；零冷却修前 2 断言红，C++ 绿测和 Java 唯一写入口同步待做 |
| T6.5 | 源码已做；网络目标只解析 1 次，三个护栏已改走真实 ReleaseSkill，待绿测 |
| T6.6 | 源码已做：pending 整套 API 已删；当前护栏 1 绿、隔离原预标记仅池数 2/1 断言红，待最终 buff 全量 |
| T6.7 | 源码已做；真实技能结束且目标 Buff 存活时 weak 上下文修前 1 断言红，待绿测 |
| T6.8 | 源码已做；三个零调用基类转换构造已删，待整批构建 |
| T7.1 | 源码已做；无读者组件及 hook 已删，源码与权威设计三名零命中，当前场景库已构建，整批门禁待做 |
| T7.2 | 源码已做；缺 Transform 两断言、缺技能存档一断言修前红，待重编绿测 |
| T7.3 | 源码已做；冻结首写 helper 已删，三处恢复显式 emplace，待回归 |
| T7.4 | 源码已做；前置校验后的死判空已删，待快照及属性回归 |
| T7.5 | 已做：宠物快照注释与唯一调用方 fail-closed 语义对齐 |
| T7.6 | 源码已做；缺任务/活动各 2 断言修前红，成功对照均绿，待完整 bag 绿测及 Java 核对 |
| T7.7 | 模板已做；旧顶层背包修前 20 条派生属性/HP/MP 断言红，待统一生成及绿测 |
| T7.8 | 模板与 proto 注释已做；Transform 脏位 GUID 修前 1 断言红，待统一生成及绿测 |
| T7.9 | 源码已做；私有网络号组件及 ActorCreate 接口收口，静态检查通过，待回归 |
| T7.10 | C++ 契约文档已做，号源不改；Java PARITY 与契约同步按 §6 第 5 步待做 |
| T8.1 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.2 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.3 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.4 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.5 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.6 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.7 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.8 | C++ 源契约已删 NodeInfoListComp；最终生成与 Java 同步待做 |
| T8.9 | 源码已做；新用例已确认唯一缺 API 编译红；B 接线和 13 文件静态检查完成。core 重编遇迁移会话同时更新 Node 接口失败，未运行 RPC 绿测；详见 §10，不回退迁移改动 |
| T8.10 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.11 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.12 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T8.13 | 源码或指定文档已做；最终整批构建/机械门禁待做 |
| T9.1 | 源码已做：GetPlayerId 与全部调用点已收口，player_battle 私有 GuidForLog 已随 T3.5 删除；最终整批机械门禁待复核 |
| T9.2 | 指定注释/文档已做；最终机械门禁待做 |
| T9.3 | 模板逐字段 reset 已删；最终统一生成与替代验证待做 |
| T9.4 | 指定注释/文档已做；最终机械门禁待做 |
| T9.5 | 指定注释/文档已做；最终机械门禁待做 |
| T10 | 13 个 protobuf 值表、3 个普通 C++ 仅类型名的源码已完成；最小 ecs_context 六例及生成模板定向红绿已通过；统一生成、签名 GM 冒烟和 Java 登记待做 |
| T11 | 指定当前规则/文档已按实际源码复核收口，7 文档核对、2 处增量修正，静态检查通过；历史回执及审计 HEAD 保留 |

### 本轮实际运行证据与修正

- T3.1：bag 586/586（临时 Redis，零跳过）；cross_zone 232/232；aoi 78/78；aoi_delivery 23/23，均零失败/跳过。证据总表 build/ecs-refactor/batch6/t3-1-gate.json。
- T3.2：scene 重编退出 0，bag 584/584，零失败/跳过；两个仅测试不可达契约的用例按任务删除。证据 build/ecs-refactor/batch6/t3-2-gate.json、cpp-tests/20261016T101427736Z-209f27ed。
- T3.3：scene 重编退出 0，cross_zone 233/233，包含唯一离场出口的双次调用护栏，零失败/跳过。证据 build/ecs-refactor/batch6/t3-3-gate.json、cpp-tests/20261016T102400644Z-0e0504c8。
- T3.4：修前 scene 库不变时，指定 LeaveTouchesOnlyNeighborGridObservers 构建退出 0、测试退出 1，恰有 2 条目标断言失败；实施后 scene 重编退出 0、aoi 71/71、aoi_delivery 20/20，零失败/跳过/禁用。证据 build/ecs-refactor/batch6/t3-4-red-gate.json、t3-4-gate.json 与 cpp-tests/20261016T110051464Z-6f067d60。离场删除全节点扫描，清理范围回到 O(7 格人数)，Pin/Unpin 与 remoteWatchers 在 cpp 全树零命中。
- T1.2 第一项变异：build/ecs-refactor/batch6-red/logs/20261016T104743984Z-ed24aa73，基线 6/6 通过；两个隔离工程编译成功，变异组仅 VectorBatchMintsOnlyWhatTheFinalPlanNeedsWhenSegmentIsUnavailable 失败、其余 5 例通过，零跳过/错误。T1.2 两项已按各自真实出口登记完成。
- T3.5：新无 sink 击杀回归构建退出 0、测试退出 1，任务未完成与物品 0/4 两断言红；改为 MissionFollowUps 收集和玩家任务入口直接驱动后，modules/scene 编译退出 0，bag 585/585、missions 17/17，零失败/跳过/禁用。证据 build/ecs-refactor/batch6/t3-5-red-gate.json、t3-5-gate.json；绿测目录 cpp-tests/20261016T111913468Z-cd81a2d5 与 20261016T112200035Z-e8cb2493。
- T3.6：skill/buff 两条沉默用例修前分别在 states 为空处失败；GM 合法 80→81 后最大生命值仍 4500，派生属性与面板两处比较也失败，合计 3 例、5 条目标断言；三工程构建均退出 0，测试均退出 1，无跳过/错误。直接调用后 scene 重编退出 0，bag 586/586、skill 45/45、buff 35/35，零失败/跳过/禁用。证据 build/ecs-refactor/batch6/t3-6-red-gate.json、t3-6-gate.json。删除回调链已逐项核对：OnBuffStart/OnBuffRemove/OnBuffDestroy 经三个 impl 仅做沉默状态和属性标脏，没有回到 AddOrUpdateBuff；buff_impl 内周期与受击效果的合法 BuffSystem 调用保留，不能将其误报成全文件零命中。
- T3.7–T3.9：core/modules/gate/scene 串行重编均退出 0；skill 45/45、currency 184/184，含全部 4 条 PlayerAnomalyWindowTest 与货币/item 分桶清理用例，零失败/错误/跳过/禁用。证据 build/ecs-refactor/batch6/t3-7-9-gate.json。任务漏列的 currency_test.cpp 第三处 ClearPlayer 改为现有 ClearAll，保留分桶及计数 1→0 断言；不再声称已删除 API 的常量时间清理能力。T3 全组完成后才开始 T4/T6/T7 测试落盘。
- T4/T6/T7 当前源码修前证据：cross 2 例全部红（4 条目标断言）；bag 7 例中预期 5 红、2 条成功对照绿（失败共 27 条目标断言，其中旧格式装备 20 条，HP/MP 为 5200/4520 对 4500/4000）；skill 5 例全部红（7 条目标断言）；aoi_delivery 1 例红（GUID 4294977296 对实际 0）；buff 缺表池数正式基线 1/1 绿。五工程构建均退出 0，测试原生出口分别为 1/1/1/1/0，零错误/跳过/禁用。证据汇总 build/ecs-refactor/batch6/t4-t6-t7-test-first-gate.json，保留各工程完整日志、实际出口与 JUnit。T6.6 另在忽略目录恢复真实旧预标记循环取对照红；当前生产源码继续冻结，未回退共享树。
- T6.6 历史对照现已完成：build/ecs-refactor/batch6-red/t6-pending/logs/20261016T122013365Z-1fa4c3b6，隔离 scene/test 构建均退出 0，当前用例退出 0、原预标记变异退出 1，唯一失败为 componentPoolCount 实际 2、预期 1；全部输入哈希不变，52 主树 TU 与 1 个 shadow 均经静态检查，未超时。首次隔离构建在 logs/20261016T121246190Z-b3ee7275 因漏原目录搜索路径找不到 modifier_buff_impl.h，退出 1，不计行为红；只补隔离工程该目录，并经 MSBuild 元数据核对保留原 32 个目录后才重跑。证据齐全后已解除生产冻结，T4/T6/T7 按文件归属推进。
- 生成前补查发现必要的派生遗漏：CopyProtoToGenDir 仅刷新 db/login，而后续 Robot 阶段仅改 go_package，活 common.proto 因而保留已删的 NodeInfoListComp。在现有复制阶段按 EnableRobotProto 复用同一复制函数补 Robot 输出，不清目录、不手改活产物；新增 TempDir 回归同时保留 db/login 与额外文件护栏。实测修前仅 Robot 字节比较红（1 例，退出 1），11 行最小生产修改后同用例绿（退出 0），原测试哈希未变。证据 build/ecs-refactor/batch6/robot-proto-copy-red-green-gate.json；这属于定向红绿，不能替代 §6 的最终完整生成器测试。
- 精确孤儿盘点保存在 build/ecs-refactor/batch6-mechanical/orphan-output-inventory.json：T1.10 的 8 个旧产物文件及三清单 5 项、T5.1 的 6 个旧 handler 与 13 项现存清单引用待最终生成后核对清理；活 ItemComp 来自 item_base_comp.proto，不能误删。robot/vendor 应通过正规 go mod vendor 刷新，Java 通过 ContractSync 与 clean install 更新；未将活 common.proto 当孤儿整文件删除。
- 构建中修正 T1.5 遗漏的 5 个测试文件共 130 处整份 ItemComp 还原调用，统一使用 RestoreItems，原断言保留。修正整批掉落用例缺少 txlog 测试号段的夹具前置，原流水条数断言保留。cross_zone 两个编译单元的 8 处空句柄比较类型化，3 个既有局部表字段改 string_view，未削弱断言。
- 共享对象冲突与旧 rpc.lib 链接失败均留有独立日志，不计行为红。正式测试 runner 的忽略副本保留原工程/门禁/判定，增加日志、JUnit、实际退出码及独占 IntDir；仅按任务补入原表缺失的 routing_identity_test。构建均使用 /m:1 /nr:false；独占 IntDir 防止 RelativeDir 上跳撞到其他节点的 obj。
- T1.1/T1.2 接手时部分旧语义已经被其他会话修正，不能宣称任务单描述的原始五红已实测。其余隔离变异已准备、尚未运行；本节只确认实际列出的 T1.2 两项与 T3.4 红绿证据，不冒充完整红测矩阵。

核心拍板 1/T4.1 尚未实施；2/T2.1 拒存退出已改为直接丢弃并常规回归通过；3/T7.10 待落实双版文档；4/T1.10 已删除 C++ 源 proto，最终生成与 Java 同批删除待做。W1-45、W1-43 仍由迁移会话处理，审计文档 HEAD 未改。

Java 状态：本批待做。只读核对已确认受管契约仅 common.proto 与 item_comp.proto 两处预期差异，任务列表的 state_persistent 已为 true，Java 无 creators 末端准入。另确认 Java 的零冷却组写入缺陷，已准备同批修前红/修复方案。未执行本批 ContractSync、Maven、MessageIdRegistryTest，也未完成 PARITY 登记；对应本批 Java 版本号：无（现有 0.1.0-SNAPSHOT 不作本批完成证据）。原因是本批源码和统一生成尚未完成，尚未到 §6 第 5 步。没有 commit、push 或 tag。

- T4 抽取前后全量：scene 库均构建退出 0，cross 分别 234/234 与 253/253，零失败/错误/跳过/禁用。逐名核对原 234 条全部保留并绿；新 SceneLifecycleTest 三条全部执行。抽取间并行会话又加了 ReleaseFence 的 16 条，保留其 redis/player_lifecycle 与相关测试改动，不把本次证据说成仅 T4 的隔离差异。证据 build/ecs-refactor/batch6/t4-premerge-gate.json、t4-postmerge-gate.json、t4-cross-comparison.json。首轮检查曾因删 MoveSpeed 依赖后 modifier_buff_impl.cpp 缺 BaseAttributesComp/LevelComp 头而失败；补直接 actor_comp.pb.h 后重新构建通过，首轮日志保留在 t4-premerge-20261016T125117904Z。
- T6.1–T6.8 与 T7.1–T7.10 的 C++ 源码/模板已落盘并冻结；场景库已重编，skill/buff/bag 等整批绿测仍待 §6 原顺序，T7.7/.8 必须等正式生成。静态快照 t6-source-final/manifest.json、batch6-mechanical/t7-source-completion.json。T11 最终当前文档复核证据 batch6-mechanical/t11-final-document-review.json，Java 过时注释留给步骤 5 准确同步。
- T8.9 前置核实修正：迁移 owner 的 docs/handoff/grpc-muduo-batch3-5/README.md §0/§2 明确第 3 批代码已补完并随 0249bee0a、b24a5b772、f0dadf5b0、a105c3efd 提交；实核 rpc_client.h 仅 CQ 措辞注释、node.cpp 仅本批注释、node_connector.cpp 干净，故指定提交门已满足，继续按表实施。此结论不代表第 4 批服务端生成模板冻结；正式统一生成仍按 §2.4 等确认模板定型。此前提交号信息问题已由仓内交接证据补齐，不再等待该问题答复。

## 10. 本轮继续回执（2026-10-16，等待迁移源码与模板定型，整批未完成）

- **源码范围**：本轮完成 T4.1–T4.4、T6.1–T6.8、T7.1–T7.10、T8.9 及 T11 的相应源码/模板/文档收口。逐个任务的当前状态仍以 §9 的 71 行表为准，“源码已做”不等于最终验收完成。T4.4 新增 SceneLifecycle 的两函数及三个契约；T8.9 为 RpcClient 注入单一连接观察者，删除连接事件和 Node 订阅，DOWN 保留 OnDisconnect→channel.reset→observer 顺序。完整增量/快照见 build/ecs-refactor/batch6/t8-observer-source-b/own-delta.patch、t8-9-source-gate.json，T6/T7 证据沿 §9。
- **核心拍板 1–4**：T4.1 已撤 creators 准入，两条 cross 成功回归修后绿，bag 初始化回归与 Java PARITY 待正式步骤 4/5；T2.1 拒存退出的直接销毁源码已落，指定隔离九红待正式测试；T7.10 号源不动、C++ 契约已记，Java 对账待步骤 5；T1.10 源 proto 已删，统一生成、孤儿清理和 Java 同步仍待执行。没有改走退路。
- **已实际验证**：T4 抽取前 cross 234/234，抽取后 253/253，零失败/错误/跳过/禁用，原 234 条逐名保留、新生命周期 3 例全绿，另 16 例来自并行 ReleaseFence 会话。T8.9 新用例修前在实际 MSBuild 静态编译检查中只有一条源码错误——rpc_client_lifecycle_test.cpp:244 不存在 setConnectionObserver；原生 build 退出 1，未执行测试、无 JUnit，按本条明确允许的 API 缺失编译红登记。干净红证据 build/ecs-refactor/batch6/t8-9-api-red-gate.json。
- **必要补漏，不改变既定方案**：首轮 API 红还遇到原死亡测试花括号初始化里的逗号被 EXPECT_DEATH 当参数分隔符，仅改等价圆括号构造，保留死亡断言与 Debug 配置。删除传递包含后，Modifier Buff 和 Node 分别补直接 actor_comp.pb.h、ecs_context.h；不把 ECS 包含加回 RpcClient。相应首次失败日志均保留，没有用跳测、注释断言或旧库伪装通过。
- **本次实际停止点**：T8.9 的第 3 批迁移提交前置已按 owner 交接和实际 commit/diff 核实满足；但重编期间第 4 批迁移又在保存共享 Node 文件。13:22:40 UTC 开始的 core 重编，38 个编译单元静态检查通过后，Node 头在 13:24:49 UTC 更新，MSVC 因新旧 grpc/muduo_grpc 接口交叉写入失败（退出 1）；Node 实现在 13:25:16 UTC 又保存了后续迁移。保留全部在途改动，发现后未继续重试，也未替迁移会话补关机逻辑（W1-43 不在本单）。
- **失败证据**：build/ecs-refactor/batch6/t8-9-green-20261016T132240129Z/core.build.log、core.build.exit；当前源快照及时间/SHA见 t8-migration-write-overlap.json 和其 current_node_snapshot。未运行本轮 RPC 绿测，不声称 core 已构建通过。该次原生编译的错误如下（共 9 条）：

```text
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(302,17): error C2653: “grpc”: 不是类或命名空间名称 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(302,23): error C2065: “Service”: 未声明的标识符 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(302,32): error C2059: 语法错误:“>” [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(303,21): error C2065: “grpc”: 未声明的标识符 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(303,21): error C2653: “grpc”: 不是类或命名空间名称 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(303,27): error C2065: “Server”: 未声明的标识符 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(303,10): error C2923: "std::unique_ptr": "Server" 不是参数 "_Ty" 的有效 模板 类型参数 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(303,10): error C2976: “std::unique_ptr'”: 模板 参数太少 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h(303,35): error C2955: “std::unique_ptr”: 使用 类 模板 需要 模板 参数列表 [D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj]
```

- **尚未执行**：迁移模板定型信息尚未确认，遵守 §2.4，未开始本轮正式 no-unity 生成。因此 §6 后续生成器全量测试、game.sln、13 工程最终顺序验证、剩余变异矩阵、Java 第 5 步及本地冒烟均待做。前置脚本已在忽略目录备妥；生成前须重新拍快照并保留所有共享差异。正式生成必须先重新 proto-gen-build（gRPC 服务端模板编进二进制），不使用旧生成器。
- **Java 版**：本批未改 Java 跟踪源码、未执行 ContractSync/Maven、未登记已完成的 PARITY 行；版本仍 0.1.0-SNAPSHOT，不能据此宣称本批已对齐。ignored java-ready/A-tests-only.patch 与 B-cooldown-and-comments.patch 已静态核对，正式步骤 5 先两条真实施法零冷却红测，再改 PlayerSkillState 唯一写入口；不扩实时伤害/Buff 框架。
- **运行夹具**：没有启动本轮冒烟服务；仅单测使用过的本批 Redis 已核对容器 ID/label 后停止，证据 redis-fixture-stop.json；续测前重建独占 Redis 并刷新 fixture，不能沿用已停止容器。冒烟已准备逐字节固定 dev_tools→cpp_nodes 脚本与最新二进制的隔离运行根，不修改跟踪启动配置或复用演示数据。
- **边界**：未 commit、push、tag；未回退/覆盖他会话改动；未手改生成产物；未修改审计 HEAD。继续所需的是本轮迁移 Node 源码及生成模板定型信息，随后从正式 §6 第 1 步推进。

## 11. 接手合并与兼容方案回执（2026-10-17，整批仍未完成）

- 负责人授权接手原 `origin/main` 合并，并明确不得覆盖、丢弃其他会话改动。原合并的 72 个冲突已拍现场快照；取快照期间外部执行者清除了合并状态并继续提交，当前 `HEAD=360d56ae5`、无 `MERGE_HEAD`。没有冲突标记不代表待合入 `6965aa6a78cba0685e5568c9f9c9cc67c2e119d3` 已融合。本会话没有执行 reset、abort、add、commit 或重新改动用户索引。
- 只在忽略目录用 `git merge-tree` 生成候选树，仍有 72 个冲突，未写入工作树或索引；证据 `build/ecs-refactor/batch6/merge-preview-20261017T043948Z/manifest.json`。先前现场快照的部分 stage 文件受外部状态变化影响，不声称 72 项的三端字节全部拍齐；缺失端以固定 Git 提交恢复供比较，不回写覆盖。
- 负责人确认队伍字段 3、邮件枚举及 tip 的两种含义均已发布，要求先制定兼容方案。完整核对还发现邮件分页、正文、删除回包及 S2C 方法不兼容；待合入源码没有可确认的邮件消息号发号记录。具体方案、推荐的两个契约入口、业务幂等与存量数据边界，以及仍需的发布清单见 [已发布契约兼容方案](../../design/published-contract-compatibility-20261017.md)。尚未选一边改号，也未编造兼容绑定。
- 其他会话继续写入是负责人允许的工作方式；后续按它们最新调用点接续。当前 T8.9 接线与新 muduo API 一致，旧 Node 接口混写已消失。已有共享 MSBuild 先后构建 scene 与 game.sln，本会话两次中间 core 构建都在并发保护阶段退出，未启动第二个 MSBuild，不计编译失败或成功；正式 §6 证据仍待执行。
- 剩余红测已按当前源码重新准备：7 组变异锚点匹配，T2 从最新源复制后只变异任务函数，ReleaseFence、废黜重入与新存盘通道逐字保留；两个 PrepareOnly 退出 0，不等于测试已运行。证据 `build/ecs-refactor/batch6-red/preflight-20261017T042418692456Z/proof.json` 与 `build/ecs-refactor/final-red-execution-matrix.json`。T1.2 已有两组实际证据，未重复跑。
- 合并应保留的三项 gRPC 客户端测试已适配到当前 muduo API，补丁只写入忽略目录，`git apply --check` 退出 0，未 apply/编译；不能把原已删除 CQ 测试整份恢复。见 `build/ecs-refactor/batch6/merge-ready/grpc-client-three-incoming-tests.patch`。Go 12 项冲突逐项比较证据同目录 `go-preservation-review-20261017.json`，不以远端旧池、旧鉴权或旧号段表覆盖当前实现。
- **当前任务口径**：T10 仍为 13 个 protobuf 值表、3 个普通 C++ 仅类型名。T4.1 源码已实施并已有 §10 所列 cross 证据，§9 末尾的“尚未实施”是过时汇总；其最终顺序验证仍待做。§9 的 71 行任务清单保留，不把“源码完成”改称“整批完成”。
- **Java 版**：本批仍待 §6 第 5 步，现有版本 `0.1.0-SNAPSHOT`；两条真实施法零冷却测试与唯一写入口修复补丁已重新静态核对，不覆盖 Java 仓其他会话的 PARITY/来源信息/业务改动。本会话尚未执行本批 ContractSync、Maven、Java 红绿或签名 GM/跨节点冒烟。两个已发布契约也必须在 Java 侧明确版本边界，不能用同步一份契约代替兼容。
- **下一步门禁**：兼容入口方案及真实发布清单确定、相关业务歧义收口后，保留最新共享修改，以新快照从 §6 第 1 步正式 no-unity 生成开始，按原顺序完成全部构建、测试、Java、冒烟与机械门禁。本轮新增跟踪改动仅为兼容设计及进度/任务回执；未手改生成产物，未 commit、push、tag，也未修改审计 HEAD。


## 12. 正式生成与全解构建阻断回执（2026-10-17，整批未完成）

- **实际生成完成**：以当前 Claude 生成器为准执行 proto-gen-build、完整 no-unity proto-gen-run、robot 的 go mod vendor 和 go test ./internal/...，00–09 原生退出均为 0。28 个受守护文件、131 个非空守护段逐字核对保留，消息号/事件号没有新增或改变；T1.10/T5.1 的 14 个精确孤儿文件和 9 个构建清单内 18 条引用已清理，事件源 proto 保留。证据 build/ecs-refactor/batch6/generation-20261017T045318632Z/；该证据不能覆盖生成后新增的协议注释。
- **本轮必要增量**：gate/client_message_processor.cpp 补直接 ecs_context.h 依赖；三条待合入 gRPC 客户端回归已适配当前 muduo API 并在现有测试基础上应用，原三条测试及其断言保留；routing_identity_test 的 Debug|x64 AdditionalOptions 仅加入 /utf-8。三条新 RPC 测试和编码修正尚无本批运行通过证据。未改审计 HEAD。
- **正式全解失败**：执行 game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64 /p:CL_MPCount=1。最早错误为 cpp/tests/bag_test/player_equip_attribute_test.cpp:976、1005 使用未声明的 bag_marshal，静态门禁退出 2，随后 MSB3073；只读定位为缺少直接 bag_marshal.h include，当前 Marshal API 仍合法，不是 RestoreItems 的调用替换。后续 currency_test 又报 PlanItem(DeliveryClass,uint32_t)、IsStuckWarnDue(uint32_t) 的 LNK2019/LNK1120；当前两定义和 scene 工程源项均存在，不能据此虚构漏列源或重复实现。
- **共享变动与停止**：完整输入快照记录 11 个变化文件，其中 10 个是保留的 redis/收件箱/生命周期与测试新修改，另 1 个为本批 routing 编码配置。全解不构成稳定成功证据。确认 PID/父进程/完整命令后仅停止本次 MSBuild 子树，驱动原生退出 1；停止工具因一个子进程已自行退出也返回 1，随后只读复核原树 PID 全无存活，未触碰其他会话进程。完整日志、首错与停止证据在 build/ecs-refactor/batch6/full-cpp-20261017T045823501Z/。
- **停止后的验证边界**：按 AGENTS.md §10.2 保留首错并等待负责人决定，未自动重试、未跳过门禁。第二轮生成、18 个 Go 模块构建、13 个 C++ 测试工程的正式串行验收、余下 7 组隔离变异及 T2 九红、签名 GM/尸体/跨节点/场景计数冒烟和最终机械门禁均未执行。既有 T1/T3/T4/T6 等历史红绿证据沿 §9–§11，不冒充本批最终全绿。§9 原 71 行任务清单保留，当前仅追加真实状态。
- **兼容入口**：负责人已批准两个契约入口各绑定原发布版本并逐步迁移，按最新 Claude 事实接续；当前仍未实施或验证第二套入口，不猜邮件已发布编号，不以一份新协议覆盖两套契约。
- **Java 版**：当前版本 0.1.0-SNAPSHOT、HEAD 0477cc0603efe68791bfd7ee8ae62f6a864e34ac。本批仍未到 §6 第 5 步，未执行本批 ContractSync、Maven、两条真实零冷却红绿、PARITY 完成登记。准备补丁与门禁会保留其现有共享修改；第一轮 C++ 失败使后续步骤硬阻断。未 commit、push、tag，未读改独立客户端。