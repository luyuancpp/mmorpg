# ECS 改造代码审查报告（2026-10-15～16）

> **审查对象**：Codex 交付的第 1–5 批 ECS / entt 改造和补充收尾，即 `git diff 6281e4d27`（已随快照提交进 HEAD 的部分，加上工作区未提交的部分）。
> **方法**：两波共 11 个方向查找，93 条初步发现中 92 条交给对抗式复核者（要求亲自读代码、尽力推翻，拿不出代码证据就判不成立），high 再加一位独立复核者。全程只读，没有编译、没有运行测试；编译由负责人自己做，按约定不判断能否编过。
> **基准**：AGENTS.md §4、§7、§11（含 §11.6、§11.7）；[组件访问规则](../../design/ecs-component-access-rules.md) 的三类初始化口径；EnTT 3.13.2 源码语义；ECS 适用判据（大量同构实体、按组件组合批处理才进 registry；dispatcher 只做可有可无的一对多通知，必需步骤直接调用）。
> **交付**：本报告；给 Codex 的补充任务单 [batch6-review-fixes.md](batch6-review-fixes.md)；[审计文档](../../design/ecs-entt-misuse-audit-20261015.md) 的 HEAD 状态已按本轮结果更新。原始发现与审查规则见 [审查交接](review-handoff-20261015.md)。

## 1. 一页结论

- **改造方向是对的，主体已经落实。** 审计 110 条里，源码层面 90 条已修、7 条部分修、5 条因其他会话删改了代码而不再存在、8 条原审计已推翻，没有原样未修的（见 §6）。全局单例、请求级 tip、背包私有 registry、实体整数下发客户端、Kafka 全量派发这几个根源问题都已收口。
- **复核后成立**：high 1 条、medium 20 条、low 53 条（合并重复之后）。
  - 唯一的 high 是 W1-40：私有场景 creators 准入只在 C++ 末端生效，把三条合法流程拒掉，还让 scene_manager 的状态与节点不一致。
  - medium 中，可达缺陷 2 条：W2-b1_runtime-01（对尸体施法会中止整次施法）、W1-33（批量退出变成 O(N²) 阻塞主循环）。已经必红或无效的测试 3 条：W1-05、W1-06、W1-17。其余是违反 ECS 判据或任务单要求没落实。
- **主线问题**：本该删掉的事件、组件和死代码被保留下来，再写防护兜住。这就是负责人说的「不可读」，详见 §2。补充任务单一律给出「删掉」的直接写法，不再加一层防护。
- **4 项核心拍板已由负责人于 2026-10-16 决定，均按审查推荐**（§4.1）。另有 24 项默认决定，负责人已确认全部维持（任务单 §2.2）；任务单于 2026-10-16 交 Codex。
- **运行证据：没有。** 所有「已修」都是源码静态结论，要等负责人编译、Codex 跑完测试后才能关闭审计条目。

## 2. 主线：该删的没删，反而加防护

按表现形式分六类，括号里是对应条目。

1. **dispatcher 承担必需步骤，而且只有一个监听者。** 涉及：
   - Kafka 入口的 13 个必需命令（W1-16）；
   - 沉默状态和 GM 调级后的重算（W1-22）；
   - 任务链与战斗击杀进度（W2-cross_sem-02）；
   - Agones 场景计数（W2-cross_sem-01）；
   - 出站连接握手（W2-fit-07）。

   共同后果有三：没装订阅的宿主里会静默失效，返回值却报成功；测试只能把 nodes 层文件编进来，或手工照抄组合根的接线；engine、modules 层因此反向依赖组合根。生产路径目前都在对外服务之前注册齐全，所以定 medium，不定 high。直接写法：调用方直接调用唯一的消费者。
2. **零监听或空监听的事件，再为它们写重入复核。**
   - W1-23 列出的 6 个事件在生产里没有监听者，或者监听者是空的。为防御「监听者在通知里销毁或替换玩家」这种假想情况，代码里写了 EntryOwnershipGuard（9 个字段的快照）、canContinueInitialization，以及约 40 处通知后提前 return，还配了约 14 个只能靠人造监听者走到的测试。
   - 这些防护分支一旦走到，会留下比不发通知更坏的状态：旧实体泄漏、在线实体没有场景。
   - 规则文档第 83 行还把「通知后重验」写成了通用做法（W1-23、W2-fit-05），后来的人会照着写。
   - 同类：CreateScene 为不存在的监听者写回滚（W1-36、W2-cross_sem-01）；Buff 为任意监听者的重入写防御（W1-22）；只活在一次同步调用里的待删集合被做成 registry 组件（W2-b1_runtime-06）。
3. **失去调用方的东西不删，反而加守卫。**
   - NodeContextManager 的全局实体三件套，加了 FATAL 守卫和死亡测试（W2-b2-01）；
   - 空的 TRANSFER 宏（W2-b2-02）；
   - 死 proto NodeInfoListComp（W2-b2-08）；
   - 没有读者的 MoveSpeedComp 配齐了全套生命周期（W2-b1_runtime-05）；
   - 没有调用方的 Pin/Unpin，让每次离场都全量扫描（W1-33、W1-37）；
   - 具名容器零调用的 explicit 转换构造（W2-b5_types-04）；
   - 逐字段 reset 的死代码（W2-b5_types-05）；
   - 生成器为非入口事件生成 handler 空壳（W2-cross_sem-03）；
   - 一次性迁移逻辑被固化成生成器每轮都跑的步骤（W1-18）；
   - 事件结构体里的死字段（W1-27）；
   - gate 里不可达的重绑分支（W1-28）。
4. **前置校验之后仍保留「缺了就给默认值」的死分支，或者背靠背重复校验。**
   - BuildBattleSnapshot（W2-cross_sem-05）；
   - 一次施法 6+N 次 CanContinueSkillCast（W2-b1_runtime-04）；
   - InitializeOnCreate 的可重入契约（W2-b1_runtime-09）；
   - 交接预检整份 marshal（W1-02）；
   - AddItems 已经原子化，调用方仍保留补差分支（W1-10）；
   - ActorNetIdComp 公开可写，于是到处反查自检（W1-25）；
   - 只为测试服务的原子安装指针门面（W2-b2-03）。
5. **「为了安全」保留实体，结果更糟。** 拒存退出一律保留实体（W1-01）：拒存原因都是永久的，保留只会让退出收不了尾、重连复用僵尸实体。
6. **测试证明不了修复。**
   - 对基线同样全绿的用例被当成「修复前红」的证据（W2-b2-04、W2-b2-07、W1-29）；
   - 新语义落地后旧用例必红，却没跟着改（W1-05、W1-06）；
   - 上界写死导致漏测（W1-17）。

**成因**：任务单要求「不得回退」「保守兼容」的地方，Codex 倾向于保留旧结构再加守卫。也有几处是任务单本身就这样写的（W1-16 的 trigger、W1-33 的全量扫描、W2-b2-03 的 atomic），这几处是审查方的责任，补充任务单一并纠正。

## 3. 按批次的问题清单（复核后成立）

严重度为复核后的结果，「原 x」表示复核改了严重度；「任务」指向补充任务单的任务号。完整的位置与推理见附录 B，修法与回归测试见任务单。

### 第 1 批：存盘与快照

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W1-01 | low（原 medium） | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:3099` | 退出流程遇到拒存一律保留实体:拒存原因全是永久的,保留永远存不了盘,只会让退出收不了尾、重连复用僵尸并扩大丢失量;但当前生产代码到不了拒存分支,属防御分支的处置口径问题,原修法(r… | T2.1 |
| W1-02 | low | `cpp/libs/services/scene/player/system/player_lifecycle.cpp:4743-4749` | 三处交接预检各构造一份整份 PlayerAllData 后丢弃。StartTravelHandoff 的预检拒绝没有调用点日志；BeginTravelHandoff 的预检保护不了… | T2.2 |
| W1-03 | low | `cpp/libs/services/scene/battle/system/player_battle.cpp:401-407` | 结算 RequestSettlementPersist 中 PersistFn 返回 false 的注释列了一个在这里走不到的来源（交接/退出跳过），又漏了实际可达的拒存；原修法把… | T2.3 |
| W1-04 | low | `cpp/libs/services/scene/player/system/player_lifecycle.h:1438-1446` | SavePlayerToRedisImpl 头注释仍写「只有三个调用方」传 allowSkipWhenPersisted=false，实际 5 处，与 cpp 内清单不一致 | T2.4 |

### 第 1 批：运行时三类初始化（技能、Buff、属性、战斗）

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W2-b1_runtime-01 | medium（原 high） | `cpp/libs/services/scene/combat/skill/system/skill.cpp:790-843` | 落点时目标已死,被 CalculateSkillDamage 的 false 当成数据缺失,整次施法被中止。对尸体施法、或目标在前摇期间被打死时,会跳过效果和后摇;这与改造前、与致… | T6.1 |
| W2-b1_runtime-04 | medium | `cpp/libs/services/scene/combat/skill/system/skill.cpp:372-384` | 一次普通技能施放要做 6+N 次 CanContinueSkillCast,外加 2+N 次 HasRequiredTargetComps(1/2 号技能有 8 个效果,即 14+… | T6.2 |
| W2-b1_runtime-05 | medium | `cpp/libs/services/scene/actor/attribute/comp/actor_attribute_comp.h:12-23` | MoveSpeedComp 在生产代码中没有任何读者，第 1/5 批却把它定成每个 actor 的必需派生组件，配齐了创建、标脏重算、缺失报错和多组测试 | T7.1 |
| W2-b1_runtime-06 | medium | `cpp/libs/services/scene/combat/buff/system/buff.cpp:277-296` | BuffPendingRemoveBuffs 实际上只是 RemoveBuff(set) 一次同步调用内的「正在删除」标记，却作为 registry 组件反复 emplace/re… | T6.6 |
| W2-b1_runtime-02 | low（原 medium） | `cpp/libs/services/scene/combat/skill/system/skill.cpp:251-269` | 没有前摇相位的技能类型(开关/激活/普攻/被动)施放后,SkillContext 永不回收。目前没有玩家拥有这类技能,属于潜伏缺陷;改造前就存在,不是第 1 批引入的 | T6.3 |
| W2-b1_runtime-03 | low（原 medium） | `cpp/libs/services/scene/combat/skill/system/skill.cpp:186-199` | cooldown_id=0(无冷却)的技能也会创建冷却表并写入 0 号条目,之后每次施放都会在 CheckCooldown 里打一条 LOG_ERROR。问题在改造前就有,第 1 … | T6.4 |
| W2-b1_runtime-07 | low（原 medium） | `tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl:28-38` | 旧格式快照(只有顶层 bag_data)的背包在派生属性算完后才恢复,DerivedAttributesComp 漏算装备加成,当前 HP 也被压到无装备上限;项目未上线,当前也没… | T7.7 |
| W2-b1_runtime-08 | low（原 medium） | `cpp/libs/services/scene/spatial/system/scene_spawn.cpp:101` | 入场落位 EnsureValidEnterLocation 对持久组件 Transform、默认技能 RegisterPlayer 对持久组件 PlayerSkillListCom… | T7.2 |
| W2-b1_runtime-09 | low | `cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.h:15-19` | InitializeOnCreate 只在新建的玩家实体上调用一次，却写成了可重入的契约：投影已有来源、保留脏位、返回走不到的 bool，还配了两条人造场景测试 | T3.2 |
| W2-b1_runtime-10 | low | `cpp/libs/services/scene/battle/system/player_battle.cpp:141-146` | StoreBattleFreezeForWrite 只是一行透传的 helper；它把三处先确认不存在、再 emplace 的业务创建改成了静默复用的 get_or_emplace… | T7.3 |
| W2-b1_runtime-11 | low | `cpp/libs/services/scene/player/system/player_pet.cpp:931` | PetSystem::BuildBattleSnapshot 的注释说缺组件时玩家照常进战斗,实际唯一调用方已先 fail-closed;同一个 false 被同时解释成「没有出战… | T7.5 |

### 第 2 批：单例与请求所有权

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W2-b2-02 | medium | `cpp/libs/engine/core/macros/return_define.h:66` | 过渡用的空 TRANSFER_ERROR_MESSAGE 宏已无调用方，注释还写着「生成头仍有调用」；任务单 C2 的前置门早已满足，宏却没删 | T8.2 |
| W2-b2-03 | medium | `cpp/libs/engine/core/network/node_utils.cpp:3` | NodeInfo 只读门面的 atomic 发布和「返回旧值」的 exchange 只为测试存在；SameInstanceOnEveryThread 把契约禁止的跨线程读固化成用例… | T8.3 |
| W2-b2-01 | low（原 medium） | `cpp/libs/engine/thread_context/node_context_manager.h:11` | NodeContextManager 的每节点类型「全局实体」三件套随 etcd 迁移失去最后一个调用方，仍连同越界 FATAL、三条死亡测试和失实的头注释保留着 | T8.1 |
| W2-b2-04 | low（原 medium） | `cpp/tests/ecs_context_test/ecs_context_test.cpp:36-47` | ecs_context_test 5 条会话索引用例中有 4 条测的是 std::unordered_map、thread_local 或无关 registry，对基线同样全绿；回… | T8.4 |
| W2-b2-05 | low | `cpp/libs/services/player/rpc/player_service_interface.h:20` | 玩家接口契约写着「RpcDoneCallback 延后完成同样遵守」，但玩家 handler 拿不到 done；engine 的 rpc_done_callback.h 反向指向 … | T8.5 |
| W2-b2-06 | low | `docs/design/player-features-ui.md:22` | C4.3 指名的 player-features-ui.md:22 没改，仍教「写 TLS tip 由 TRANSFER 宏转移、直接写 response 会被覆盖」，与现行规则相… | T8.6 |
| W2-b2-07 | low | `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:183-185` | C3 没按挑战意见收尾：用例名和注释仍指向已删除的 GlobalEntity/globalRegistry；末尾换上的 actor/scene any_of 断言对旧实现同样必过，… | T8.7 |
| W2-b2-08 | low | `proto/common/base/common.proto:38` | NodeInfoListComp 唯一使用方是已删除的 ServiceNodeList 单例，现在两个仓库都没有任何手写代码引用它，成了死 proto 消息 | T8.8 |

### 第 3 批：背包

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W1-05 | medium | `cpp/tests/bag_test/bag_selection_order_test.cpp:149` | bag_selection_order_test 有 5 例没跟上「扣空即释放 / 还原跳过零堆」的新语义，当前必红，其中 2 例已构造不出它要测的空堆 | T1.1 |
| W1-06 | medium | `cpp/tests/bag_test/bag_test.cpp:4063-4098` | VectorBatchNeverEvictsWhenSegmentIsUnavailable 的 ② ③ 仍按旧闸门「号源不可用不许腾位」断言，与「先规划到不动点、按最终规划铸号」… | T1.2 |
| W1-12 | medium（原 low） | `docs/design/bag-instance-layout-split.md:293-304` | batch3 §4.7 任务单与挑战意见要求的背包设计文档更新大半没落实：没有 §14 规则表，§8「全部已知差异」没登记本批客户端可见的变化，bag-rule-policy-la… | T1.9 |
| W1-07 | low（原 medium） | `cpp/libs/modules/bag/container_layout.h:144` | FlatLayout 是唯一非 final 的具体布局，今后它的派生类漏写 Clone 会在首次入包后被切片；目前所有派生类都已覆盖，属于潜在的维护陷阱 | T1.4 |
| W1-09 | low | `cpp/libs/modules/bag/item_store.h:121-123` | 实例层和桥层的注释仍写着「空堆保留占格、等后续并堆回填 / 由整理统一回收」，与「扣空当场两层成对释放、加载跳过零堆」的现行实现相反；CanMintGuid(s) 已无生产调用方，… | T1.7 |
| W1-10 | low | `cpp/libs/services/scene/battle/system/player_battle.cpp:794-797` | 调用方仍按旧口径写：「入包失败可能已改包」「扣空留僵尸堆」。beforeCounts 补差、items 部分发放分支、mutated 出参、扣除后的 ReclaimEmptySta… | T1.8 |
| W1-11 | low | `proto/common/component/item_comp.proto:4-11` | C004 只删了 C++ 的 PlayerItemBlockList，proto 的 PlayerItemBlockComp（注释还引用不存在的 Bag::IsItemBlocke… | T1.10 |
| W1-13 | low | `cpp/libs/services/scene/player/system/bag_marshal.cpp:204-211` | 旧档零章补盖依赖调用方「先对全包 Raise 再逐条 Insert」两步协议：bag_marshal 写了两遍，Bag 只暴露半个不变量 | T1.5 |
| W1-14 | low | `cpp/libs/modules/bag/bag_system.cpp:676-679` | AddItems 有五处内部不变量失败复用表数据错误码 kInvalidTableData，其中三处没有专门日志；同文件多处函数内 tab 与空格缩进混排 | T1.6 |
| W1-15 | low | `cpp/tests/bag_test/bag_java_parity_test.cpp:124` | BagJavaParityTest::TearDown 把 item 号段恢复成 bag_instance_data 的 [900000,1000000) 而不是基线 [1,2^5… | T1.3 |

### 第 4 批：Kafka 入口白名单

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W1-16 | medium | `tools/proto_generator/protogen/internal/generator/cpp/service_register_info.go:363-384` | Kafka 入口的 13 个必需命令（4 个 scene、9 个 gate，外加 gate 信封回退）仍经 tlsEcs.dispatcher.trigger 转发；没有监听者时静… | T5.1 |
| W1-17 | medium | `cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:13` | 白名单单测把上界写死为 49，没有覆盖已进入白名单的 49 号 PlayerInboxNotifyEvent，也拦不住以后把新事件号（≥49）误标进白名单 | T5.2 |
| W1-18 | medium | `tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:84-87` | 一次性的事件迁移逻辑被固化成生成器每轮必跑的步骤：用正则改写三份手工维护的构建清单，并以 attribute_sync_option 为锚点硬编码补登 event_ingress_… | T5.3 |
| W1-19 | low | `cpp/nodes/scene/handler/event/battle_event_handler.cpp:21` | 已删除的 DispatchProtoEvent 和 SceneEventHandler 仍被注释与规范引用，任务单 4.4 第 6 条的零命中门禁没有达成 | T5.4 |
| W1-20 | low | `cpp/nodes/_template/main.simple.cpp.example:14` | 两份节点模板和模板 README 没有跟上新 hook 契约：只声明 KafkaCommandType 而缺 DispatchKafkaCommand，照抄就会撞上 static_… | T5.5 |
| W1-21 | low | `docs/design/control-plane-topic-partitioning-20260908.md:312` | 控制面 topic 文档里的 scene 白名单数量和生成状态都已过时；事故文档 F001/F002 没有按任务单 4.5 第 2 条改为指向新方案 | T5.6 |

### 第 4 批：进程内事件、句柄与 ActorNetIds

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W1-22 | medium | `cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:85` | 沉默状态和 GM 调级后的重算都是只有一个消费者的必需步骤，现在仍经 dispatcher 转发：不装订阅的宿主里会静默失效，skill_test 只能手工复刻接线，Buff 系统… | T3.6 |
| W1-23 | medium | `cpp/libs/services/scene/player/system/player_scene.cpp:227-228` | 生产中零监听或只有空监听的 6 个事件仍在 trigger，还配了通知后重入复核和人造重入测试；复核分支一旦走到会留下半成品状态 | T3.1 |
| W1-24 | low（原 medium） | `tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:34-42` | 66 号 ActorBaseAttributesS2C 只在战斗状态那一路带 entity_id，移动和速度增量的 entity_id 为 0。这是改造前就有的缺口，Java 已修… | T7.8 |
| W1-25 | low | `cpp/libs/services/scene/actor/actor_net_id.h:8-11` | ActorNetIdComp 定义在公共头里、可以随意写，导致 Of、Resolve、Assign 各自再做一遍反查自检；FillActorCreateMessageInfo 与两… | T7.9 |
| W1-26 | low | `cpp/libs/services/scene/combat/skill/system/skill.cpp:110-112` | 同一个不可信的 target_id 在一次 ReleaseSkill 里被反查最多 6 次；uint64 版的 ResolveSkillTarget 没有按任务单删掉，Valida… | T6.5 |
| W1-27 | low | `cpp/libs/engine/core/node/system/node/node_events.h:10` | OnNodeRemoveEvent::sceneNodeType 是无人写、无人读的死字段；SkillExecutedEvent 的 skillId/skillTableId 从不… | T3.7 |
| W1-28 | low | `cpp/nodes/gate/handler/rpc/client_message_processor.cpp:136` | gate 节点句柄类型化后残留旧整数逻辑：GetEffectiveNodeEntity 成了多余包装，ResolveSessionTargetNode 里有无意义别名和不可达的重绑… | T8.10 |
| W1-29 | low | `cpp/tests/routing_identity_test/routing_identity_test.cpp:643` | “同 index 不同 version 判为换节点”用例是类型契约固定用例，不是修复前会红的回归测试，回执把它称作“回归”是误标 | T8.12 |
| W1-30 | low | `cpp/libs/modules/mission/system/mission.cpp:345` | 事件结构体化后仍有注释描述旧机制：mission.cpp 的“截成 uint32”注释和多余别名、CreateScene 回滚注释里的“AOI 格子表”、属性同步注释里的“Guid… | T3.8 |
| W1-31 | low | `cpp/libs/services/scene/actor/actor_net_id.h:17-19` | actor net id 的唯一性范围（同时在线的节点之间唯一，进程重启后同一 node_id 会重新发同样的号）与 Java 雪花号不同，但没有写进两版的契约文档；F080 的三… | T7.10 |

### 第 5 批：生命周期

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W1-33 | medium | `cpp/libs/services/scene/spatial/system/aoi.cpp:235-246` | 每次离场都全量扫描全节点 AoiListComp，疏散、排空、停机这几条批量退出路径因此变成 O(N²) 的主循环同步阻塞，而这次扫描只兜住了生产中没有调用方的远端 Pin | T3.4 |
| W1-34 | medium | `cpp/libs/services/scene/player/system/player_scene.cpp:248-271` | 玩家离场序列（AOI 清理→摘 SceneEntityComp 与 Hex）在 HandleEnterScene 和 DetachFromScene 各写了一份，同类分叉以前造成过… | T3.3 |
| W1-35 | medium | `docs/design/ecs-component-access-rules.md:85-91` | 组件访问规则的句柄安全表漏写 emplace/emplace_or_replace/get_or_emplace/patch/replace/erase/storage.erase… | T11 |
| W1-36 | low | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:749-775` | 场景创建和销毁通知之后的复核与回滚，是为不存在的监听者写的防护，并且与规则文档的「trigger 后重验」互相矛盾；应改用监听者契约收口，删掉 CreateScene 的回滚，而不… | T4.2（作废，以 W2-cross_sem-01 为准） |
| W1-37 | low | `cpp/libs/services/scene/spatial/system/aoi.cpp:249-251` | Pin 有 AoiSystem::PinAoiEntity（纯透传）和公开的 InterestSystem::PinAoiEntity 两个入口，10 处测试与三份设计文档仍指向 … | T3.4（并入 W1-33） |
| W1-38 | low | `cpp/libs/services/scene/combat/skill/system/skill.cpp:64` | 第 5 批后有 4 处注释或文档与实现不符(skill.cpp、player_lifecycle.cpp、anomaly_detector.h、ecs-fit-overview.m… | T3.9 |
| W1-39 | low | `docs/design/scene-creation-architecture.md:344` | 3 份设计文档仍描述已删除的 ScenePlayers 反向集合 | T9.5 |

### 第 5 批：组件类型

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W2-b5_types-01 | low | `cpp/libs/services/scene/player/system/player_scene.cpp:49` | 读 PlayerGuidComp 的私有 GuidForLog 在 7 个文件里各有一份,还有 4 处守护段内联三元表达式,与 ActorLog 并存:名为日志,实际被用作异步身份… | T9.1 |
| W2-b5_types-02 | low | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:498` | 已有 PlayerSceneSystem::FindSceneById,但两个入口仍各手写 2 份 scene_id 全表扫描(共 4 份);原修法漏了 gRPC 建场入口没有 s… | T4.3 |
| W2-b5_types-03 | low | `cpp/libs/services/scene/spatial/system/grid.cpp:44` | grid.cpp 新增的注释声称 try_get 前必须先 valid(),与 EnTT 语义和组件访问规则表矛盾;另有几处与 try_get 等价的冗余 valid() 和复述类… | T9.2 |
| W2-b5_types-04 | low | `cpp/libs/services/scene/combat/buff/comp/buff_comp.h:24` | 四个具名容器组件的 explicit 基类转换构造在全仓没有调用方,X() = default 只是因为写了这两个构造才不得不补上 | T6.8 |
| W2-b5_types-05 | low | `tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:49` | 属性同步模板已改成消费后整体 remove DirtyMaskComp,循环里逐字段 dirtyMask.reset 成了死代码,6 个生成文件共 9 处 | T9.3 |
| W2-b5_types-06 | low | `cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:97` | gate 把 scene 节点 registry 叫 sceneRegistry、节点句柄叫 sceneEntity,与 tlsEcs.sceneRegistry 及 SceneE… | T8.11 |

### 补充收尾：场景权限与 gRPC 资源寿命

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W1-40 | high | `cpp/libs/services/scene/player/system/player_scene.cpp:196` | Codex 把废弃 scene_test 里的 creators 断言做成了生产准入，而且只在 C++ 节点末端生效。非创建者按号加入镜像、组队跟进镜像、开启去重后复用镜像这三条合… | T4.1 |

### 跨批：调用点语义与残留误用

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W2-cross_sem-01 | medium | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:533` | Agones 单元计数这个必需步骤仍经 dispatcher 一对一转发(唯一监听者只调 OnUnitCreated/OnUnitDestroyed),两个 CreateScene… | T4.2 |
| W2-cross_sem-02 | medium | `cpp/libs/modules/mission/system/mission.cpp:343-414` | 任务链的自动发奖、自动接后续、“完成任务 X”推进和战斗击杀进度都是必需步骤，现在仍经 dispatcher 转给 nodes 层的唯一监听者：services/modules 层… | T3.5 |
| W2-cross_sem-03 | low | `tools/proto_generator/protogen/internal/generator/cpp/event.go:206` | 白名单落地后，生成器仍为非 Kafka 入口的事件生成 handler 并在 gate/scene 注册 dispatcher 订阅，这些订阅永远不会触发 | T5.1（并入 W1-16） |
| W2-cross_sem-05 | low | `cpp/libs/services/scene/battle/system/player_battle.cpp:925-933` | BuildBattleSnapshot 在 all_of 前置校验之后，仍保留 7 处“缺组件就给默认值或跳过”的判空分支，注释也写成了“已确认存在”；结算路径和属性系统里有同样的… | T7.4 |
| W2-cross_sem-07 | low | `robot/travel_smoke_scenario.go:255-256` | robot 冒烟场景的注释和失败文案仍按旧机制写（“拒绝码必须写 TLS tip”“TRANSFER 宏总会物化 error_message”），与第 2 批之后“handler … | T9.4 |
| W2-cross_sem-10 | low | `cpp/libs/services/scene/player/system/player_feature_snapshot.cpp:187-213` | 任务列表读路径在持久组件 MissionsContainerComp 缺失时仍返回 kSuccess，并下发整份“全部未接取”的配置目录；现有测试还把这个行为固化成了契约，与“面板… | T7.6 |
| W2-cross_sem-11 | low | `docs/design/ecs-entt-misuse-audit-20261015.md:70` | 审计文档作为“修 entt 前先看这里”的入口，仍把 F044/F050 写成“现在 actor 初始化时挂上”，注意事项还在教已被删除的 CQ/Stub 排空修法，并把 etcd… | 审计文档（本轮已改） |

### 跨批：ECS 适用性与可读性

| 编号 | 严重度 | 位置 | 复核后的问题 | 任务 |
|---|---|---|---|---|
| W2-fit-07 | medium | `cpp/libs/engine/core/network/rpc_client.h:16-17` | 出站连接 UP 后的节点握手（必需步骤，生产唯一监听者是 Node）仍由 RpcClient 经 tlsEcs.dispatcher 转发；传输对象因此 include 场景 EC… | T8.9 |
| W2-fit-02 | low（原 medium） | `cpp/nodes/scene/handler/rpc/scene_handler.cpp:471-476` | 场景创建和销毁的业务逻辑在 muduo 与 gRPC 两个入口各写一份（去重、字段组装、排空、销毁，第 1 批又各加一份回滚），只靠「Keep in sync」注释同步，前置校验已… | T4.4 |
| W2-fit-04 | low（原 medium） | `cpp/libs/services/scene/combat/buff/comp/buff_comp.h:12-18` | 目标身上的 BuffEntry 仍存着施法者 SkillContextCompMap 里同一个 shared_ptr，但 buff 侧只读它的 caster()，这个值 buffP… | T6.7 |
| W2-fit-06 | low（原 medium） | `cpp/libs/engine/core/node/system/node/node_util.cpp:52-95` | 节点表仍是「map + 动态属性袋」式 registry：查找靠线性扫描 view，出站/入站角色靠组件组合区分，握手定时器用通用 TimerTaskComp。这是 ecs-fit… | T8.13 |


## 4. 需负责人拍板

### 4.1 核心拍板（负责人 2026-10-16 已决定，均按推荐）

**1. 私有场景 creators 准入（W1-40，high）**

- **现状**：Codex 把废弃 scene_test 里的断言做成了生产准入，而且只在 C++ 场景节点末端生效。
  - 三条合法流程都会收到 3022：非创建者按号加入镜像、组队跟进队长所在的镜像、开启去重后复用镜像。
  - 拒绝发生在 scene_manager 已经提交 location、人数和 owner_epoch 之后，两边状态随之不一致；跨节点按号加入时，玩家在所有节点上都没有实体。
  - Java 版不做准入（PARITY 第 128 行），两版已经分叉，且没有登记。3022 在 Tip.xlsx 里也没有文案。
- **选项 A（推荐，两位复核者一致）**：撤掉 C++ 末端准入，回到两版同基线。只改 C++ 约 6 个文件；Go、Java 都不改代码，Java PARITY 改为「已决：不做准入」。代价：拿到镜像号的同 zone 玩家都能进，这与线上基线和 Java 版一致。
- **选项 B**：准入挪到 scene_manager 选场景之前，两版统一实现。要改 Go、C++、Java、robot 和设计文档，还要定组队跟随的放行规则，属于新的玩法规则。即使选 B，C++ 节点末端的检查也必须删掉，否则两边判定不一致时照样出现状态分裂。
- **负责人决定（2026-10-16）：选 A。** 实施见任务单 T4.1。

**2. 拒存退出的处置（W1-01，复核降为 low）**

当前生产代码走不到拒存分支：持久组件只在加载时创建，而且没有任何删除路径。所以这一项只决定「将来加载不变量万一被破坏时」采用哪种失败形态。

- **选项 A（Codex 现状）**：保留实体。代价：拒存原因都是永久的，实体永远存不了盘；退出收不了尾，停机只能靠看门狗；30 秒内重连会复活这个僵尸实体，丢失量随在线时长无界增长；资产指令永不定案，战斗锁不销账。规则文档第 61 行的这条口径已经被另一个会话照搬。
- **选项 B（推荐）**：调用 DestroyDeposedPlayer 不存盘丢弃，不写 A1′、不写任何交接标记、不发改派。I0、I1、A1′ 都成立；丢失量等于上次完好落盘之后的增量，与宕机相同；玩家重登后资产按账本重投、结算由登录钩子补应用，都能收敛。
- 原审查给的 B′（releaseMarkSuppressed + FinishExitAfterPersist）不可取：改派会写交接标记，违反 I1。
- **负责人决定（2026-10-16）：选 B。** 实施见任务单 T2.1，规则文档第 61 行随 T11 一起改。

**3. ActorNetIds 的客户端确认（F080 / W1-31，low）**

需要负责人答复 F080 的三项客户端问题，重点是：客户端在 EnterSceneS2C、重连、换节点时是否清空 actor 表。

- **A（推荐）**：确认会清空。号源维持现状（同时在线的节点之间唯一、本进程内不复用），只补两版文档。
- **B**：不清空或无法确认。号源加启动代次，或者改用 SnowFlake，让号跨重启也唯一，与 Java 一致。
- 另需确认：本仓的会话能否直接改 Java 仓的 mmorpg-client-contract-aoi.md 和 PARITY.md，还是交给 Java 侧的会话。
- 复核更正：原先担心的「取值从小于 2^32 变成大于等于 2^32」不成立。改造前 0 号槽位退役之后，客户端就已经收到过大于等于 2^32 的值。
- **负责人决定（2026-10-16）：选 A。** 负责人确认客户端在换场景、重连、换节点时清空 actor 表；号源不动，只补两版文档（任务单 T7.10）。另两项（是否只当不透明值使用、是否接受方案 B）没有单独答复，按 A 维持现状记录；Java 仓文档由 Codex 按 AGENTS §12 同批改。

**4. C004 之后 proto PlayerItemBlockComp 是否删除（W1-11，low）**

- **A（推荐）**：整份删除 `proto/common/component/item_comp.proto`，Java 同批删除并登记 PARITY。这条消息从未被引用或持久化，删除没有兼容风险。
- **B**：保留消息，注释改为「未使用」。这违反 YAGNI，后来的人会以为功能还半存在。
- **负责人决定（2026-10-16）：选 A。** 实施见任务单 T1.10。

### 4.2 另需负责人知悉

- **默认决定**：任务单 §2.2 列了 24 项按推荐执行的决定，负责人 2026-10-16 确认全部维持。其中改动面较大或触及客户端可见行为的有：
  - T5.1：Kafka 入口改为直接调用，生成器约 8–12 个文件；
  - T4.2、T4.4：删除场景事件，并收进单一函数；
  - T6.1：对尸体施法的语义；
  - T7.8：66 号消息带 entity_id。
- **不在本单、需单独立项**：
  - **NodeDirectory**：节点表移出 registry（W2-fit-06，即 ecs-fit-overview 的 S6）。推荐 B：等 gRPC→muduo 迁移提交后，与迁移会话联合实施第 2 期（约 35 个文件）。没有余力就选 A：不立项，只在文档标注。不推荐只做第 1 期的只读门面，那等于在 registry 上再加一层透传，第 2 期还得重改。
  - **S8b**：CurrencyComp.debts 有两份事实源，修正属于客户端可见变化（ecs-fit-overview §7 第 2 条），仍待拍板。
  - **长期目标**：gate 不再持有 ECS 世界，engine 层不再读 actorRegistry。前置条件是 T5.1 与 T8.9 落地。

### 4.3 有争议

- **复核者之间没有结论分歧**：W1-40 两位复核者都判 high；第二波唯一的 high（W2-b1_runtime-01）首审和独立二审都降为 medium。
- **修法冲突 7 处**：不同条目之间的修法有 7 处互相冲突，主审的取舍写在任务单 §2.3。负责人不同意的话，直接改那一节即可。

## 5. ECS 该用 / 不该用，以及落实情况

写进规范的版本见任务单 T11 第 3 条。

| 子系统 | 结论 | 现状 |
|---|---|---|
| 场景里的 actor（玩家，将来的 NPC、怪物） | **该用**：World::Update 每帧有 6 个系统按组件组合批处理 | 保留。唯一创建点是 InitPlayerFromAllData，派生投影已改为直接初始化 |
| actor 的按需状态（冷却、技能上下文、BuffList、AoiList、施法相位、属性脏标记、异常窗口、InBattle、战斗状态集合） | **该用**：「组件存在」就表示「功能激活」，view 只遍历激活的实体，实体销毁时自动回收 | 三类口径已落实。残留：无冷却的技能也建冷却表（W2-b1_runtime-03）；BuffPendingRemoveBuffs 实际是请求级数据（W2-b1_runtime-06） |
| 玩家流程态标签（Frozen、TravelHandoff、SceneChangeInFlight 等） | **该用**：纯标签，配合 view 扫描 | 保留 |
| 玩家冷数据（货币、背包、任务、宠物、属性、账本） | **勉强可用**：当作「实体 → 聚合」的 O(1) 表；只在加载时创建，缺了就拒绝 | 已落实；CurrencyComp.debts 仍是两份（S8b 待拍板） |
| registry 自己的派生索引（ActorNetIdIndex） | **该用**：放在 ctx()，由 on_destroy 维护 | 机制正确；组件应改为私有（W1-25） |
| 场景实例（sceneRegistry） | **勉强可用**：数量少，没有每帧批处理，价值只在「销毁时连带回收」 | SceneEntity 强类型已做；创建、销毁仍在两个入口各写一份，Agones 计数仍经 dispatcher（T4.2–T4.4） |
| 节点与连接（NodeContextManager） | **不该用**：几十个异构对象，registry 只是「map + 属性袋」，查找全靠线性扫描 | 没做（S6，需拍板）；还残留一套无人调用的全局实体 API（T8.1） |
| 进程单例与请求级数据（NodeInfo、ServiceNodeList、SessionList、TipInfoMessage、AOI 下行缓冲） | **不该用** | 已全部移出；NodeInfo 多了一层只为测试服务的 atomic 门面（T8.3） |
| 背包物品实例 | **不该用** | 已改为 ItemStore 字典，正确 |
| 跨实体共享对象（施法上下文） | **不该用**：一次施法只归施法者所有 | 目标侧镜像已删；目标身上的 BuffEntry 仍持有施法者的 shared_ptr（T6.7） |
| dispatcher | 只做可有可无的一对多通知，没有消费者就不发事件；必需步骤直接调用或构造注入 | 当前「不可读」的主要来源，见 §2 第 1、2 类（T3、T4.2、T5.1、T8.9） |
| gate 进程 | **不需要 ECS 世界**：只把 tlsEcs.dispatcher 当事件总线用 | 没做；前置条件是 T5.1 与 T8.9 |
| battle 节点 | 回合制，不用 entt | 正确 |
| engine 层 | 不应认识 actorRegistry | player_message_utils、actor_log、ECS_GET_OR 宏仍在读它（长期目标） |

**一句话**：ECS 真正的用武之地只有「scene 进程里的 actor 及其按需状态」，再加上「场景实体销毁时连带回收」这一点。其余的要么已经移出（单例、请求级数据、背包），要么还待移出（节点表、gate 的 ECS 世界、经 dispatcher 走的必需步骤、跨实体共享的施法上下文）。

**ecs-fit-overview 各步骤的落实情况**。这份总评在 README 里标为「参考」，S0–S9 没有派给任何一批，所以「没做」不算 Codex 漏做；列出来是为了看清还剩什么。

| 步骤 | 状态 | 说明 |
|---|---|---|
| S0：规范写明哪些数据进 registry | 没做 | 规则文档第 83 行反而把通知后重验写成了通用做法；由 T11 第 2、3 条补上 |
| S1：ECS 用法棘轮脚本 | 没做 | 原方案的 R2 与现行「按需写入口用 get_or_emplace」冲突；本单默认不做（W2-fit-05） |
| S7：删 engine 层读 actorRegistry 的死函数 | 做了 | node_message_utils 已删 |
| S9：删死 ECS 类型和孤儿测试 | 做了 | 漏了 NodeContextManager 的全局实体 API（T8.1） |
| S5：任务链改为显式驱动 | 没做 | 由 T3.5 完成 |
| S8a：AnomalyDetector 侧表改为玩家组件 | 做了 | ClearPlayer 已无生产调用方（T3.9） |
| S3：必需初始化改为直接调用 | 部分做了 | 直接调用已做；三个零监听通知仍在（T3.1） |
| S8b：货币 debts 单一事实源 | 没做 | 待拍板 |
| S4：施法实例只归施法者 | 部分做了 | 目标镜像已删；BuffEntry 仍共享指针（T6.7） |
| S2a：场景目录 | 部分做了 | ScenePlayers 已删（比原方案更简单）；单一入口没做（T4.4，不建索引） |
| S2b：SceneEntity 强类型 | 做了 | gate 里有同名异义的命名冲突（T8.11） |
| S6：节点目录 | 没做 | 需拍板（§4.2） |

## 6. 审计 110 条的 HEAD 状态摘要

已写回 [审计文档](../../design/ecs-entt-misuse-audit-20261015.md) 附录表的「HEAD 状态」列。这些是源码层面的结论，运行证据待负责人编译与测试。

- **已修 90 条**。「已修」不等于没有问题：其中多条的修法本身留下了本报告的条目，单元格里已注明审查编号。
- **部分修 7 条**：F014、F043、F052、F085、F095、F099、C006。
- **不再存在 5 条**：C007、F007、F010、F093、C008。相关代码已被 RPC 通道重构或 gRPC→muduo 迁移删除。
- **原审计已推翻 8 条**：在当前代码中依然不成立。

## 7. 复核方法与覆盖

- **查找**：
  - 第一波 6 个方向（存盘、背包、Kafka、事件与句柄、生命周期、补充收尾），46 条；
  - 第二波 5 个方向（运行时三类初始化、单例、组件类型、调用点语义、ECS 适用性与可读性），47 条。
- **复核**：
  - 按文件分组，每组不超过 5 条，交给对抗式复核者；
  - W1-01 做了专项复核，核对 I0、I1、A1′ 不变量和重登后的收敛；
  - W1-40 由两位复核者独立复核，一位看业务流程可达性，一位看两版契约（读了 Java 仓 PARITY.md 和实现）；
  - 第二波唯一的 high 另加一位独立复核者；
  - low 且低置信度的 1 条（W2-b5_types-07）按交接规则不复核。
- **复核结论分布**：第一波 46 条：成立 22、降级 16、不成立 1、重复 1、已被改写 6；第二波 47 条：成立 17、降级 19、重复 9、已被改写 1、未复核 1。「降级」含严重度下调、上调或描述与修法更正。
- **只读核对**：所有代理都没有使用 Write 或 Edit；%TEMP%、/tmp、会话草稿目录里没有仓库文件副本。个别 Grep 输出过长，被工具框架自动保存到会话目录，不在仓库内。
- **时间差**：第一波发现写于 2026-10-15 的工作区，复核按 2026-10-16 的当前代码。这期间 gRPC→muduo 迁移删掉了 CQ 和 Stub，所以 W1-41～W1-46 判为「已被改写」。
- **未覆盖**：
  - 编译可行性（按约定不判断）；
  - 运行期证据；
  - 客户端仓库（未获授权）；
  - Java 版只在 W1-40、W1-24、W1-31 等条目里按需核对，没有全面审查。

## 8. Java 版（AGENTS §12）

本轮只做审查和出任务单，对应 Java 版本号：无。涉及两版的条目：

- **W1-40**（负责人已选方向 A）：Java 不改代码，PARITY 第 128 行改为「已决：不做准入」，第 182 行补「C++ 曾加 3022 准入，已撤回」。
- **W1-11**（负责人已选 A）、**W2-b2-08**：在 Java 的同步副本 game-proto 里同批删除对应 proto，并登记 PARITY。
- **W1-24**：Java 已经带 entity_id；C++ 跟齐后，PARITY 第 53 行改为已对齐。
- **W1-31**（负责人已选 A，号源不动）：在 Java 仓 mmorpg-client-contract-aoi.md 第 146 行和 PARITY 第 177 行补上两版各自的唯一性范围。
- **W2-cross_sem-10**：Java 按同一口径核对任务列表缺组件时的返回码，并登记 PARITY。
- **T10 GmDescribeEntity**：C++ 独有，Java 不适用；ContractSync 之后核对 MessageIdRegistryTest。

## 附录 A：不成立、已被改写、重复与未复核

| 编号 | 复核结论 | 说明 | 去向 |
|---|---|---|---|
| W1-08 | 不成立 | 每次入包都复制候选仓库和布局，这是文档化并已被接受的原子性代价；没有性能数据，不应为它另开一条快路径 | 无 |
| W1-32 | 重复 | 与 W1-23 同根：为没有生产订阅者的通知保留重入防护，防护分支自身留下半成品状态（与 W1-23 同根，已并入） | T3.1（并入 W1-23） |
| W1-41 | 已被改写 | 原问题（F010 在 LeaseRevoke 后立即 TryCancel 全部在途调用）已随 CQ 和 GrpcEtcdTransport 一起删除；现在 MuduoEtcdTransport 对 LeaseRevoke… | 无 |
| W1-42 | 已被改写 | GrpcClientRuntimePtr、CQ 组件、in_place_delete 流式调用组件和逐回调 try_get 都已删除；节点实体上只剩一个 MuduoGrpcChannelPtr，调用状态随回调走，不进 E… | 无 |
| W1-43 | 已被改写 | 被测对象(注册表里的流式调用组件及其 reply 缓冲)已随 muduo 迁移删除,swap-and-pop 步骤和整组流测试一起作废 | 无 |
| W1-44 | 已被改写 | DataService 选点改为 view\<NodeInfo, MuduoGrpcChannelPtr> 并要求 kReady;摘节点先同步移除通道组件,不再有「关闭中」窗口 | 无 |
| W1-45 | 已被改写 | 路由服摘除时 ~Channel 把在途调用以 UNAVAILABLE 回调,OnRouterNodeRemoved 已经多余;lane 注释仍写「永远不会再有回调」,lateDropped 会把每个被摘调用再计一次 | 移交迁移会话 |
| W1-46 | 已被改写 | 停机和摘节点的 5ms gRPC 排空轮询已删除；剩下的 FinalizeTeardownInLoop 本地完成让轮靠 FIFO 有界收敛，当前没有任何回调会同步重发，不构成可达缺陷 | 无 |
| W2-b2-09 | 已被改写 | SceneManager 选点「底线」注释已被迁移会话改写，现在写「只在没有挂着未关闭通道的实例时返回 null」，与新实现一致 | 无 |
| W2-b5_types-07 | 未复核 | B1 可选的算术类型组件护栏未落地,也没有记录取舍,F032/F099 没有任何机械防复发手段（low 且低置信度，按交接 §5 不复核） | 不入本单 |
| W2-cross_sem-04 | 重复 | 与 W2-b1_runtime-08 同根：入场落位对持久组件 Transform 用 get_or_emplace,违反“持久数据绝不延迟创建”;生产上到不了补建,属规则违例 | T7.2（并入 W2-b1_runtime-08） |
| W2-cross_sem-06 | 重复 | 与 W2-b2-02 同根：空操作宏 TRANSFER_ERROR_MESSAGE 已经没有任何调用方，但第 2 批 C2 规定的删除没做，过渡注释“生成头仍有调用”也已失实 | T8.2（并入 W2-b2-02） |
| W2-cross_sem-08 | 重复 | 与 W2-b5_types-02 同根：已有 FindSceneById,两个场景入口的 CreateScene 去重和 DestroyScene 仍各手写一份 scene_id 全表扫描(F095 未收口);s2s 两… | T4.3（并入 W2-b5_types-02） |
| W2-cross_sem-09 | 重复 | 与 W2-b1_runtime-09 同根：InitializeOnCreate 的 bool 返回值到不了 false,4137 的整条 if(含 canContinueInitialization)都是死分支,失败侧… | T3.2（并入 W2-b1_runtime-09） |
| W2-fit-01 | 重复 | 与 W2-cross_sem-02 同根：任务链的三个必需步骤（自动发奖、接续任务、推进「完成任务 X」条件）和战斗结算的击杀进度，仍由 libs/modules 的 MissionSystem 经 dispatcher… | T3.5（并入 W2-cross_sem-02） |
| W2-fit-03 | 重复 | 与 W2-cross_sem-01 同根：场景的 Agones 单元计数是必需记账，却经 OnSceneCreated / OnSceneDestroyed 由唯一监听者转发。两个 CreateScene 入口还为一个并… | T4.2（并入 W2-cross_sem-01） |
| W2-fit-05 | 重复 | 与 W1-23 同根：组件访问规则缺少「哪些数据进 registry」判据，ecs\*.md 里「单例组件」的说法含糊；:83「通知后重验」和缺失的 dispatcher 规则已并入 W1-23；S0/S1 只是标为「参考… | T3.1（并入 W1-23） |
| W2-fit-08 | 重复 | 与 W2-b2-01 同根：NodeContextManager 的全局实体三件套（Set/Get/GetOrCreateGlobalEntity）及 globalEntities_ 自 f0dadf5b0 删除 Grp… | T8.1（并入 W2-b2-01） |
| W2-fit-09 | 重复 | 与 W2-b5_types-06 同根：gate 把 scene 节点 registry 叫 sceneRegistry、把节点句柄叫 sceneEntity，与 scene 进程的 SceneRegistry / Sc… | T8.11（并入 W2-b5_types-06） |

## 附录 B：逐条复核记录

复核者的原文（只做了标点和转义整理），包含位置和推理。修法与回归测试见任务单的对应条目；W1-40 有两份独立复核，W2-b1_runtime-01 附有独立二审。

#### W1-01

- 原发现（medium）：拒存退出一律保留实体，没有有界出口；重连会复用这个永远存不了盘的实体
- 复核（W1-01）：**降级 / low**，退出流程遇到拒存一律保留实体:拒存原因全是永久的,保留永远存不了盘,只会让退出收不了尾、重连复用僵尸并扩大丢失量;但当前生产代码到不了拒存分支,属防御分支的处置口径问题,原修法(releaseMarkSuppressed + FinishExitAfterPersist)还会经改派写出交接标记
- 位置：cpp/libs/services/scene/player/system/player_lifecycle.cpp:3099, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3009, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2236, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3338, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3408, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3438, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4487, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2648, docs/design/ecs-component-access-rules.md:61, cpp/tests/cross_zone_test/cross_zone_test.cpp:1183, cpp/tests/cross_zone_test/db_task_dispatch_test.cpp:577

【现状核对】按函数名重新定位,保留实体的拒存分支现在在:HandleExitGameNode 首次退出 3098-3104、耗尽重踢 3008-3014;HandlePlayerAsyncSaved 收敛比对 marshal 失败 2236-2244(原文写 2223-2231,那段现在是“意图缺失”分支);FinishExitOrAwaitDbTaskAck 的 kResave 拒存 3338-3345;OnDbTaskDispatchSettled 3408-3414 与 3438-3443;HandlePlayerSaveRejected 旧 epoch 补存 4487-4492。其中 M4 强制写 3117、HandlePlayerAsyncSaved 重存 2282、OnDbTaskDispatchSettled 3438 这三处,同一调用栈里刚刚 marshal 成功过,拒存实际到不了(只剩 lastSaveSeq==UINT64_MAX 这一理论来源)。

【1 不变量】cross-zone-scene-travel.md:902-905 的定义是:I0 = SET 未发出就不可能被放行;I1 = SET 已发出则 Redis ⊇ 冻结内存。§12.6.3 第二步(507-512)规定 A1′ 只在退出收敛后由 FinishExitAfterPersist 写,标记含义是“ms 时刻状态已落盘、本节点不再持有”(1066 行)。退出优先(1654 行):摘交接组件和冻结组件;标记写过就按原文撤回;不写 A1′。“丢弃实体、不写 A1′、不写任何标记”这种做法不发新的 SET,I0 成立。若此前已发过 SET,那是在 I1 成立时发的(冻结内存已在 Redis),所以 I1 仍成立;A1′ 没写,也就没有宣称收敛。三者都满足。原审查给的修法是先 releaseMarkSuppressed=true 再 FinishExitAfterPersist,它只压住了 A1′。FinishExitAfterPersist 在 3268 行先调 DispatchEmergencyRelocate,后者在 3643-3647 / 3728 行会按 owner_epoch 条件写 "E:now" 交接标记并发出改派。拒存时 Redis 并不包含当前内存,这一步违反 I1 的字面契约,也违反规则文档第 61 行的“不得发送交接标记”。所以原修法自相矛盾,应改用不写任何标记的丢弃路径。

【2 重登收敛】资产:durable 只按落盘快照判(asset_op_system.cpp:208-274)。Go 的 Decide 对未 durable 的结局只会 AwaitDurable 重排(decide.go:65-68),不会定案。离线时 tryPersistedLedger(reconcile.go:981-1012)读盘上账本,读到“未见”时这一行保持未决(I7),等玩家上线再投递。重登后账本从盘上加载,里面没有这个 seq,scene 重新应用并存盘,durable 后 Go 定案,能收敛。结算:销账唯一入口是 AckSettlementPending(player_battle.cpp:440-482),没有活实体时 pending 不动,留给登录钩子。ApplyPendingSettlement 按重载后的账本判 alreadyApplied,结果是补应用一次,也能收敛。丢弃实体后的丢失量,等于上次完好落盘之后的增量,与宕机相同,有上界。保留实体时这两条链路都卡住:Go 每 500ms 重查一次 durable,永远拿不到;战斗锁一直不销账,玩家开不了下一场。

【3 保留实体在各收尾口的实际后果】(a) 退出收不了尾。UnregisterPlayer 一直挂着,IsSaveInFlight 恒为真(6297)。重踢 ShouldRekickExhaustedExit 要求 resaveRounds>=上限(player_exit_intent.h:227-230),拒存时轮次是 0,不会触发;DB 闸门重踢要求 rounds>=3,而拒存发生在第 1 轮,也不会触发。(b) 停机时 main.cpp:216/233 的谓词一直把它算作在途存盘,只能等 Node 的 15s 看门狗。疏散和排空的票据消费不掉。(c) 玩家在 30s 租约内重连:HandleEnterGameNode 2538-2542 → EnterScene → 2648 CancelExitOnReconnect 复活实体。DiscardDeposedEntityOnReentry 只在 epoch 至少 +2 时才丢弃。复活后每次周期存盘、资产存盘、结算存盘都被拒,丢失量随在线时长无界增长。(d) 交接已发起时,SavePlayerToRedisImpl 在 4245 行先做完整性校验,4262 行才判交接跳过,于是交接中的实体也被保留并冻结,直到 EnterScene 应答或 70s 冻结上限。若走 B4 解冻,实体会留成退出中的僵尸。按 I1,这时直接丢弃不会丢任何数据。测试 IssuedHandoffCannotHideMissingPersistedComponent(cross_zone_test.cpp:1254)把这个行为固定住了。(e) Codex 回执“不一致之处 1”的理由是“销毁会丢失内存状态”。但这份内存状态永远存不进盘:持久组件只在加载时创建,缺了不会再回来;身份缺失和序号耗尽同样是永久的。保留实体只是把丢失往后推,而且越推越大。这条错误口径已经写进规则文档 ecs-component-access-rules.md:61,另一会话的 DB 闸门实现(godb-pipeline-impl-20261015/batch-reports.md:202)就是照这条写的。

【4 生产能否触发:决定降级】kRejected 只有四个来源。① 实体无效:各调用点都先检查了 valid。② PlayerGuidComp 缺失或为 0:InitPlayerFromAllData 在 4124 行必定挂上;HandleExitGameNode 在 3090 行对 kInvalidGuid 已提前返回。③ marshal 缺持久组件:所有玩家实体都经过 InitPlayerFromAllData → PlayerAllDataMessageFieldsUnMarshal,无条件 emplace 全部 16 个持久组件(player_database_loader.cpp:113-168;bag_marshal / mission_marshal 的 Unmarshal 用 get_or_emplace,也一定会挂上)。全仓非测试代码 grep 不到对这 16 个组件或 PlayerGuidComp 的 remove / erase / clear / storage 操作。④ lastSaveSeq==UINT64_MAX:要盘上数据本身坏成最大值才会出现。测试只能靠手动 remove\<...> 才进得了这些分支。所以今天这是不可达的防御分支,问题只是“万一将来有 bug 破坏加载不变量,选哪种失败形态”。保留实体在这个选择上严格劣于丢弃,应当修;但运行期影响为零,严重度从 medium 降为 low。原文的位置清单和修法也一并更正,见 fix。

#### W1-02

- 原发现（low）：三处用整份 MarshalPlayerForSave 只为拿一个「组件齐不齐」的布尔值，紧接着又整份 Marshal 一次；提交点的 kRejected 分支因此成了死代码
- 复核（W1-save）：**降级 / low**，三处交接预检各构造一份整份 PlayerAllData 后丢弃。StartTravelHandoff 的预检拒绝没有调用点日志；BeginTravelHandoff 的预检保护不了任何东西。提交点 kRejected 分支是负责人要求保留的复核，不是缺陷
- 位置：cpp/libs/services/scene/player/system/player_lifecycle.cpp:4743-4749, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4791-4796, cpp/libs/services/scene/player/system/player_lifecycle.cpp:5270-5277, cpp/libs/services/scene/player/system/player_lifecycle.cpp:5703-5711, cpp/tests/cross_zone_test/cross_zone_test.cpp:1891, cpp/tests/cross_zone_test/cross_zone_test.cpp:1913

成立的部分：三处（4745、5272、5705）都构造一份完整的 PlayerAllData 再丢掉，只用返回值；变量名 verifiedState 让人以为后面会用它。StartTravelHandoff 的预检（4746）与提交点 4791 之间只摘 PlayerSceneChangeInFlightComp、挂交接组件和冻结组件、计数、挂定时器。整个仓库只有 ActorNetIdComp 连了 on_destroy（actor_net_id.cpp:29），所以 4792 的 kRejected→Abort 在当前代码里确实走不到。

需要更正的三点：

(1) storage-patch-notes.md:118 是负责人的明确要求：「预检…拒绝返回现有故障 tip。保存提交点仍复核，预检不能代替保存结果」。代码 4743-4744 的注释也写明了这一点。所以 kRejected 分支是有意保留的提交点复核，不能算缺陷，原修法也是保留它。

(2) 「误导排障」说重了。[PlayerSave] 那行 ERROR 带 player_id 和 missing 清单，内容如实。BeginTravelHandoff 失败时 Abort 会打 WARN 写明原因，B5 有自己的 ERROR。只有 StartTravelHandoff 的预检拒绝后静默回 tip，缺一行「是交接请求被拒」的上下文。

(3) 原修法是新增并导出 HasAllPersistedComps 谓词。声明要进 player_data_loader.h，它由 player_data_loader.h.tmpl 生成，所以要改模板再重生；还要同时改手工维护的 player_database_loader.cpp 和 player_database_1_loader.cpp，Marshal 内部也得改成两遍查组件。这些代价只换来未经测量的性能：交接是每次传送一次，不在每帧路径上，§11.3 禁止无数据的优化。代码只会变多，不取。

另外，BeginTravelHandoff 的预检（5270-5277）是 Codex 自己加的，负责人的规格里没有，而且它保护不了任何东西。能进 BeginTravelHandoff 的路径只有两条：StartTravelHandoff 的 kAlreadyPersisted（同一调用栈里刚 marshal 成功），以及冻结期间那笔 marshal 成功的存盘落地后的回调。标记宣称的是「Redis ⊇ 冻结内存」，Redis 里那份是完整的，事后内存缺组件推翻不了这个结论。保留这道检查，只会把这种不可能状态变成「解冻、留下一个此后每次存盘都被拒的实体」（W1-01 那类僵尸）；删掉它，目标节点会加载 Redis 里那份完整存档，结果反而更好。B5 那处预检决定的是「采纳并解冻」还是「保持冻结、等冻结上限不存盘销毁」，有分支意义，保留。

#### W1-03

- 原发现（low）：结算 PersistFn 返回 false 的注释和日志仍写「两种来源」，漏了拒存
- 复核（W1-save）：**降级 / low**，结算 RequestSettlementPersist 中 PersistFn 返回 false 的注释列了一个在这里走不到的来源（交接/退出跳过），又漏了实际可达的拒存；原修法把「交接已发起」写进新注释同样不准
- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:401-407

「漏了拒存」成立。本批让 SavePlayerToRedisImpl 在缺 Guid（4222-4229）或缺持久组件（4240-4253）时返回 kRejected；另外存盘序号耗尽（4366-4370）也返回 kRejected。经 SavePlayerToRedis 的 bool facade（4209）都变成 false。

原发现没看到的一点：395 行的 IsSettlementApplicable（378-382）已经挡掉了所有带 PlayerTravelHandoffComp 或 UnregisterPlayer 的实体，所以这里根本拿不到 kHandoffAlreadyRequested（它要求 requestedAtMs!=0，前提是挂着交接组件）；SavePlayerToRedisImpl 本身也从不因退出而跳过写盘（基线 6281e4d27 时就不跳）。因此注释的第二个来源在这一处是错的。原修法建议写「已落盘相等、交接已发起、或…拒存」，会把一个走不到的来源写进新注释。

可参照同形状的 player_inbox_system.cpp:408，它的写法是对的：「盘上已是这一份，也可能是被拒存」。行为本身没错，durable 只按快照字节判，所以仍是 low。

#### W1-04

- 原发现（low）：SavePlayerToRedisImpl 头注释仍写「只有三个调用方」传 allowSkipWhenPersisted=false
- 复核（W1-save）：**成立 / low**，SavePlayerToRedisImpl 头注释仍写「只有三个调用方」传 allowSkipWhenPersisted=false，实际 5 处，与 cpp 内清单不一致
- 位置：cpp/libs/services/scene/player/system/player_lifecycle.h:1438-1446, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4298-4301

grep 结果显示传 /\*allowSkipWhenPersisted=\*/false 的共 5 处：3116（M4）、3333（FinishExitOrAwaitDbTaskAck）、3433（OnDbTaskDispatchSettled）、4810（交接快路径补写）、5726（B5 采纳）。基线 6281e4d27 时确实只有 3 处（3039、4366、5259）。DBTask 回执闸门是 0f6690604 的 Kafka 存盘投递改动加的，那次只更新了 cpp 4298-4301 的清单，没改头文件。本批又改写了头文件这段注释的前两行（加了「持久组件缺失仍拒绝」），但计数没跟着改。结果头文件和实现各说一套，同一条知识有两个来源且已经漂移。原修法里「顺手给 SaveResult 各值补注释」不必做：1427-1428 已说明 kRejected 不是任何落盘凭据，1076-1079 已说明 kQueued 只表示排入写入，再补就是第三份。

#### W1-05

- 原发现（medium）：新增选择顺序用例没有跟上「扣空即释放 / 加载跳过零堆」，至少 5 例必红或已测不到原规则
- 复核（W1-bag-tests）：**成立 / medium**，bag_selection_order_test 有 5 例没跟上「扣空即释放 / 还原跳过零堆」的新语义，当前必红，其中 2 例已构造不出它要测的空堆
- 位置：cpp/tests/bag_test/bag_selection_order_test.cpp:149,cpp/tests/bag_test/bag_selection_order_test.cpp:214,cpp/tests/bag_test/bag_selection_order_test.cpp:218-230,cpp/tests/bag_test/bag_selection_order_test.cpp:249-264,cpp/libs/modules/bag/bag_system.cpp:352-355,cpp/libs/modules/bag/bag_system.cpp:1368-1371

按当前代码逐例走了一遍，结论都成立。工作区里这两个文件没有未提交改动，测试文件仍是原样。(1) PlaceDrainFixture 的布局是 B 在槽 0、C 在槽 1、A 在槽 2，每堆 5 个。扣 7 个时按槽位序先扣 B 的 5 个，B 归零后 DrainOneConfig 第 352-355 行当场 DestroyItem。于是 ExpectLowestSlotsDrained 第 149 行调用 StackSize(B) 时 GetItemCompByGuid 返回 nullptr，第 76 行的 EXPECT_NE 失败。ClampedDrainTakesLowestSlotFirst 和 ExactDrainTakesLowestSlotFirst 共用这个辅助函数，两例都红。(2) DrainWithoutReceiptUsesTheSameSlotOrder 第 214 行同理。(3) DrainSkipsEmptyStackWithoutReceipt 里，InsertItemForRestore 遇到 size==0 在第 1368 行直接 return，A 根本没进包，第 228 行失败。这个用例已经造不出它声称要测的空堆。(4) MergeNeverPicksAnEmptyStackAsSurvivor 里 A 同样没进包，只有 B(3, seq2) 和 C(4, seq3) 参与 MergePartialStacks：B 变成 7，C 变成 0，CollectEmptyInstances 只收回 C。destroyed.size()==1，第 259 行的 ASSERT 失败。其余 13 例逐一核对过，不受影响（MergeKeeps… 只退役由合并产生的空堆，仍然通过）。batch3-bag.md 第 335 行回执写的是「原有 18 项选择顺序测试仍保留」，第 320/333 行承认未运行，与现状吻合：没有红绿证据，5 例已红。修法上，Bag 层现在已经不可能持有零堆：AddItems 第 637 行拒收 size==0，PutInstance 要求 size==1，还原时跳过，两条扣减路径当场销毁。因此 (3) 在 Bag 层没有可测对象，ItemStore 层的同一条规则已由 ItemStoreTest.DrainRankIsReadOncePerNonemptyCandidate 覆盖（kGuidD 的 size 为 0，断言 rankReads.count==0 且 drained.size()==2），按「删掉多余的」原则直接删除。(4) 的规则只在 ItemStore 层还成立：ItemStore::Insert 仍接受零堆，IsMergeCandidate 也仍有 size>0 判断。下沉为 ItemStoreTest 后，batch3 §8 第 240 行计划的红证据「IsMergeCandidate 去掉 size>0 时应 FAIL」继续有效。另外两点不并入本条：item_store.h:145-150 与 bag_system.cpp:517-522 的注释仍说「DrainStacks 刻意保留空堆」「RemoveItemByPos 扣到 0 的僵尸由整理回收」，已与实现不符；item_store.cpp:285-288 的 take==0 分支已被第 268 行的 size>0 过滤和第 279 行的 count==0 退出完全挡住，属于不可达的重复防护。可另列 low 项删除。

#### W1-06

- 原发现（medium）：旧用例 VectorBatchNeverEvictsWhenSegmentIsUnavailable ② 与新的「先规划到不动点再发号」语义冲突，会红
- 复核（W1-bag-tests）：**成立 / medium**，VectorBatchNeverEvictsWhenSegmentIsUnavailable 的 ② ③ 仍按旧闸门「号源不可用不许腾位」断言，与「先规划到不动点、按最终规划铸号」的新契约冲突，当前必红
- 位置：cpp/tests/bag_test/bag_test.cpp:4063-4098,cpp/tests/bag_test/bag_test.cpp:4033,cpp/tests/bag_test/bag_test.cpp:1957,cpp/libs/modules/bag/bag_system.cpp:668-692

按当前代码走 ②：包是 Temporary(2)，940001 在槽 0，940002 在槽 1，已满。preassigned 是 kNonStack1，size 1，item_id 为 940003。第 659-665 行把它记入 preassigned={940003}，stackUnits[kNonStack1]=1。PlanEviction 的结论是现状放不下，不是具名槽，带回执，freeCells=0。不动点第一轮按 EvictOldestFirst 选出 [940001]，第二轮排除后需求仍为 1，满足 shortfall\<=1，收敛：victims=[940001]，newInstanceCount=1。第 676 行 1\<1 不成立，第 683 行循环 0 次，根本不调用 MintGuid。候选仓库删掉 940001、插入 940003，候选布局试放成功后提交，返回 kSuccess，evicted=[940001]。所以第 4085、4087、4089 行三处失败（第 4088 行 OccupiedGridCount 仍为 2，能通过）。③ 再次提交 940003 时，第 661 行 store_.Contains 命中，返回 kBagDeleteItemAlreadyHasGuid。evicted 里还留着 ② 的 940001，占用格数为 2，因此第 4093、4094、4095 行也失败。新行为本身是对的：旧闸门要防的是「淘汰后重新规划才发现要铸号」，现在 PlanEviction 先算到不动点，铸号数取自最终规划，并且在任何修改之前完成。那条风险已由 BagJavaParityTest.PostEvictionMintFailurePreservesOldItemsLayoutAndReceipts 钉住。不需要铸号的批次在号源不可用时照常按 FIFO 腾位，既不丢东西也有回执，符合临时格的语义。问题只在这个旧用例和它的注释没有同步。顺带：第 4033 行「铸号预检都在 ReserveBatch 之前」和第 1957 行「(ReserveBatch)」里的 ReserveBatch 已被删除（batch3 §10 第 331 行），注释过时。用例名「NeverEvicts」在 ② 改写后也不再准确。与 W1-05 同属「旧用例没跟上新语义」，但涉及的代码和根因不同，不算重复。

#### W1-07

- 原发现（medium）：提交时用 Clone 出的对象整体替换 layout_，派生类漏覆盖 Clone 时会被永久切片且没有任何检查
- 复核（W1-bag-core）：**降级 / low**，FlatLayout 是唯一非 final 的具体布局，今后它的派生类漏写 Clone 会在首次入包后被切片；目前所有派生类都已覆盖，属于潜在的维护陷阱
- 位置：cpp/libs/modules/bag/container_layout.h:144,cpp/libs/modules/bag/container_layout.h:150-153,cpp/libs/modules/bag/bag_system.cpp:753,cpp/libs/modules/bag/bag_system.cpp:772-773

机制属实：AddItems 在提交时用 Clone 出的副本整体替换 layout_。只要哪个 FlatLayout 子类没覆盖 Clone，第一次成功入包后布局就会被切成 FlatLayout，HasSlotSemantics 等行为静默丢失。但这个问题现在走不到：(1) 生产代码只构造 FlatLayout 和 FixedSlotLayout（bag_system.h:164/181/193/208/653），FixedSlotLayout 和 GridLayout 都是 final 且自带 Clone；两个测试布局 LyingLayout（bag_test.cpp:2468-2472）和 FailSecondPlacementLayout（bag_java_parity_test.cpp:78-85）也都覆盖了 Clone。(2) BagJavaParityTest.FormalLayoutClonesKeepGeometryAnchorsAndIndependentOccupancy（bag_java_parity_test.cpp:246-265）对三种正式布局逐一比较 Capacity、Slots、HasSlotSemantics、FreeCells，删掉 FixedSlotLayout::Clone 这条用例就会红。所以剩下的风险只有一种：今后有人新写一个 FlatLayout 子类又忘了 Clone，这是潜在的维护陷阱，不是可达缺陷。原文说的「布局对象身份改变」，bag_system.h:217-218 已写明 Layout 引用只在只读阶段有效；生产上唯一的调用 player_equip.cpp:971-973 在写操作之前用完即弃，没有跨写持有。结论：medium 偏高，降为 low。原修法要在 AddItems 里加 typeid 运行期分支，或改成在原布局上重放，都属于再加一层防护或增加代码量，不符合优先删除的偏好。

#### W1-08

- 原发现（low）：候选事务每次入包都整包复制，包括最常见的纯合堆；可加一条无新实例的快路径
- 复核（W1-bag-core）：**不成立 / none**，每次入包都复制候选仓库和布局，这是文档化并已被接受的原子性代价；没有性能数据，不应为它另开一条快路径
- 位置：cpp/libs/modules/bag/bag_system.cpp:695,cpp/libs/modules/bag/bag_system.cpp:753,cpp/libs/modules/bag/bag_system.h:217-218,cpp/libs/modules/bag/bag_system.h:286-288

事实描述都对：纯合堆也会复制整个仓库并 Clone 布局，ItemComp\* 借用也会随之失效。但这不是缺陷。(1) 代价和借用契约都写进了公开接口注释（bag_system.h:217-218、286-288），batch3-bag.md §10 也把它记为负责人接受的设计。(2) 调用方都已适配：player_equip.cpp:1466-1467 按 guid 重新获取；入包调用点只有 player_battle、asset_op、player_mission、player_equip 四处，没有一处跨入包持有借用。(3) 默认容量 10 格，入包发生在结算、领奖、资产操作、装备和宝石操作时，不在每 tick 热路径上，也没有任何压测或 profiling 数据显示它是瓶颈。AGENTS §11.3 要求性能优化由数据驱动。原文建议的快路径会多出第二条提交路径，written 回执顺序、AssertLayerConsistency、evicted 输出都得和候选路径保持一致，违背 DRY 和负责人优先删除的偏好，反而增加分叉风险。

#### W1-09

- 原发现（low）：实例层注释仍按「空堆保留占格、等待回填 / 整理回收」描述，与扣空即释放的实现相反；CanMintGuids 已无生产调用方，注释却仍称必须调用
- 复核（W1-bag-docs）：**成立 / low**，实例层和桥层的注释仍写着「空堆保留占格、等后续并堆回填 / 由整理统一回收」，与「扣空当场两层成对释放、加载跳过零堆」的现行实现相反；CanMintGuid(s) 已无生产调用方，注释却仍说铸号型写入必须先调用它
- 位置：cpp/libs/modules/bag/item_store.h:121-123,cpp/libs/modules/bag/item_store.h:145-150,cpp/libs/modules/bag/item_store.h:162-163,cpp/libs/modules/bag/item_store.h:177-183,cpp/libs/modules/bag/item_store.cpp:437-438,cpp/libs/modules/bag/item_store.cpp:469-474,cpp/libs/modules/bag/bag_system.cpp:517-522,cpp/libs/modules/bag/bag_system.h:87,cpp/libs/modules/bag/bag_service.h:212,docs/design/bag-instance-layout-split.md:114

想推翻它，只能证明 Bag 层仍会出现持久的零堆。结论是不会：DrainOneConfig(bag_system.cpp:352-355) 和 RemoveItemByPos(400-404) 扣到 0 就当场 DestroyItem，InsertItemForRestore(1367-1371) 加载时跳过零堆，AddItems 入口(637)拒绝 size==0，全仓生产代码里 set_size 只出现在 bag 模块内部。唯一的零堆来源是 MergePartialStacks 清空的组员，它们会在同一次 MergeAndCompact 里立刻被 CollectEmptyInstances 回收。item_store.h:113 的「实例本身仍在(可能 size==0)」在 ItemStore 这一层仍然成立，所以不算错。但 121-123 的「槽位留着、后续并堆填回去」、147-150 的两条来路加「由整理统一回收」、163 的「并堆刻意允许往空堆回填」，以及 bag_system.cpp:517-522、bag_system.h:87、bag_service.h:212 里的「RemoveItems 抽光留下的空堆 / RemoveItemByPos 扣到 0 的僵尸」，描述的都是已经不存在的状态。CanMintGuids 已没有生产调用方：AddItems 在改动任何状态之前直接循环 MintGuid，失败就整批拒绝，契约也明写「允许烧号」(bag_service.h:53)。asset_op 的号段预检直接调用 tlsGuidSegmentRegistry…Available()，同样不走 CanMintGuids。item_store.h:177-183、item_store.cpp:437 和 471-474 仍宣称它是必需的前置步骤，与代码不符。只影响可读性，没有运行期后果，严重度 low 合适。

#### W1-10

- 原发现（low）：调用方注释与防御分支仍按旧的「入包失败可能已改包 / 空堆留格」写，与新的原子契约矛盾
- 复核（W1-bag-docs）：**成立 / low**，调用方仍按旧口径写：「入包失败可能已改包」「扣空留僵尸堆」。beforeCounts 补差、items 部分发放分支、mutated 出参、扣除后的 ReclaimEmptyStacks/MergeAndCompact 在原子提交和扣空即释放之后都已不可达或不再需要，属于该删没删的防护
- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:794-797,cpp/libs/services/scene/battle/system/player_battle.cpp:818-856,cpp/libs/services/scene/battle/system/player_battle.cpp:1101-1104,cpp/libs/services/scene/player/system/asset_op_system.cpp:503-542,cpp/libs/modules/bag/bag_service.cpp:142-149,cpp/libs/modules/bag/bag_service.cpp:178-181,cpp/libs/modules/bag/bag_service.h:56-63,cpp/libs/services/scene/player/system/player_equip.cpp:440-454,cpp/libs/services/scene/player/system/player_equip.cpp:1271-1275,cpp/libs/services/scene/player/system/player_equip.cpp:1385-1386

推翻路径是找到一条「BagService::AddItems 失败但包已变」的路径，没有找到。Bag::AddItems(bag_system.cpp:604-790) 的所有拒绝都发生在 `store_ = std::move(candidate); layout_ = std::move(candidateLayout);` 之前，淘汰、合堆、插入都只作用于候选副本；BagService 的 ItemCountMap 重载只在 RETURN_ON_ERROR 之后才把 mutated 置为 true。由此：player_battle 里 landed 恒为 0，remainder 恒等于 gained，beforeCounts 补差与 854 行的 else 分支不可达；asset_op 的 `else`（mutated 为真且 error 不为 0）不可达，而 507 行的注释其实已经改成新口径，分支却留着；mutated 出参恒等于「error==kSuccess 且请求非空」，信息完全冗余。扣除方面，DrainOneConfig/RemoveItemByPos 扣到 0 已当场 DestroyItem，所以 player_battle:794-797 与 player_equip 的 ReclaimEmptyStacks 原本要解决的「僵尸堆占格」已不存在。它们现在只剩顺手合并零头的副作用，还会为被合并的实例多写一批退役流水；1101-1104 的 size()==0 跳过同样不可达。这些都不会造成运行期错误：资产不重复、不丢失，PlayerEquipTest.EnhanceReclaimsTheMaterialStackItEmptied 等断言删掉回收调用后照样成立。所以定 low，不升。这正是负责人点名的通病「保留本该删的东西再加防护」，应该删，而不是像原修法的选项一那样再加「LOG_ERROR 不可达兜底」。原修法只改 player_equip 的注释不够直接，应连 ReclaimEmptyStacks 一起删。

#### W1-11

- 原发现（low）：C004 只删了 C++ 类型，单人物品封禁的 proto 消息和设计文档仍宣称该功能存在
- 复核（W1-bag-docs）：**成立 / low**，C004 只删了 C++ 的 PlayerItemBlockList，proto 的 PlayerItemBlockComp（注释还引用不存在的 Bag::IsItemBlocked）和 exploit_loss_prevention / bag-service-srp-refactor / 04-asset-channel 文档仍把单人物品封禁和 blockList 参数写成现状
- 位置：proto/common/component/item_comp.proto:4-11,docs/design/exploit_loss_prevention.md:25,docs/design/exploit_loss_prevention.md:28,docs/design/exploit_loss_prevention.md:136,docs/design/exploit_loss_prevention.md:169,docs/design/exploit_loss_prevention_en.md:28,docs/design/exploit_loss_prevention_zh.md:28,docs/design/bag-service-srp-refactor.md:6-33,docs/design/bag-service-srp-refactor_en.md:6-33,docs/design/bag-service-srp-refactor_zh.md:6-33,docs/design/guild-phase2/04-asset-channel.md:483

推翻路径一是证明 proto 仍有用途。git grep 显示 PlayerItemBlockComp 只出现在 proto 源、各生成树（cpp/generated/proto、go/proto、generated/proto/\*、robot/vendor）和文档里：没有任何 .proto import item_comp.proto，没有 message 字段引用它，没有存盘或 DB 列，C++/Go 业务代码零引用；Bag::IsItemBlocked 已不存在。整个 item_comp.proto 只装这一条 message。路径二是证明文档已经标注。git diff 6281e4d27 对 exploit_loss_prevention\*.md 和 bag-service-srp-refactor\*.md 没有任何改动；04-asset-channel.md 只在文末追加了与本条无关的段落，483 行签名仍带 blockList。只有 ecs-component-access-rules.md:97 和审计文档记录了删除决定。所以问题成立：照文档排障或接功能的人会以为「单人封禁存在且下线后仍生效」。不过这条消息从未被序列化，没有协议兼容或数据风险，严重度维持 low。补充一处原发现漏掉的地方：exploit_loss_prevention.md:25 的「Bag::BlockItem」行同样过时（那是更早的 SRP 重构遗留，可同批修）；136 行的 IsCurrencyBlocked 仍然存在（asset_op_system.cpp 在用），只需删掉 IsItemBlocked。

#### W1-12

- 原发现（low）：任务单与挑战要求的背包设计文档更新大半未做（§8 行为差异、回滚分析、策略分层文档仍是旧现状）
- 复核（W1-bag-docs）：**降级 / medium**，batch3 §4.7 任务单与挑战意见要求的背包设计文档更新大半没落实：没有 §14 规则表，§8「全部已知差异」没登记本批客户端可见的变化，bag-rule-policy-layering.md 一字未改、仍把已删除的 ReserveOrEvict/ReserveForBatchAdd 写成现行形状
- 位置：docs/design/bag-instance-layout-split.md:293-304,docs/design/bag-instance-layout-split.md:271,docs/design/bag-instance-layout-split.md:114,docs/design/bag-instance-layout-split.md:609,docs/design/bag-rule-policy-layering.md:417-437,docs/design/bag-rule-policy-layering.md:449-466,docs/design/bag-rule-policy-layering.md:555,docs/design/bag-rule-policy-layering.md:567,docs/design/bag-rollback-feasibility-analysis.md:121-149

推翻路径一是证明文档已更新。git diff 6281e4d27 只显示 bag-instance-layout-split.md 改了 14 行（§3.1、§3.3、§4 表、§6 不变量 4、§11.2），确实没有 §14；§8 仍是 6 行拆分期差异，标题却声称「全部已知差异」。bag-rule-policy-layering.md 零改动：432 行的「悬空的 entity」和 567 行 P2 的「registry_.valid() 防御」都是 batch3 §4.7 点名要改的，也没改。更要紧的是，§6.2(3)「落码后的实际形状」表和 §6.3、555 行把 ReserveOrEvict/ReserveForBatchAdd 以及「fence 下连腾位也不做」写成现行机制，而 §10 回执明确说这些旁路已经删除，代码也查不到，实际机制已换成 Bag::AddItems 候选提交。bag-instance-layout-split.md:271 的不变量 5「任何失败都发生在第一次写入之前」也与「写入候选、失败丢弃副本」的新形状不符；114 行仍列出 CanMintGuid（见 W1-09）。路径二是这些要求可能本来就被豁免。batch3 §4.7 明确列出 §14 规则表和 bag-rule-policy-layering 两处修改，§8 挑战 low（293-294 行）明确要求 §8 登记加 rollback 顶部注明，§9/§10 回执既没声明完成也没声明放弃。唯一能部分推翻的是 rollback 文档：它顶部已有 2026-09-20 的状态说明，写明「v1 的 §1–§7 调研只作为当时代码事实的记录」，所以 121/144 行的 itemRegistry_ 已被整体标成历史，再加一行属于锦上添花，误导风险很低。严重度更正：按审查基准，「任务单或挑战意见要求未落实」属于 medium；而且 bag-rule-policy-layering.md 是排障时的主要参考，现在把已删除的机制写成现状，有实际误导后果。原判 low 偏低，改为 medium（这里是上调；verdict 选「降级」，是因为基准规定严重度需更正时用这一项）。

#### W1-13

- 原发现（low）：旧档补章靠调用方「先 Raise 再 Insert」两步协议，规则泄漏到 marshal 且写了两遍
- 复核（W1-bag-core）：**成立 / low**，旧档零章补盖依赖调用方「先对全包 Raise 再逐条 Insert」两步协议：bag_marshal 写了两遍，Bag 只暴露半个不变量
- 位置：cpp/libs/services/scene/player/system/bag_marshal.cpp:204-211,cpp/libs/services/scene/player/system/bag_marshal.cpp:249-255,cpp/libs/modules/bag/bag_system.h:505-512,cpp/libs/modules/bag/bag_system.h:526-528,cpp/libs/modules/bag/bag_system.cpp:1360-1363,cpp/libs/modules/bag/item_store.cpp:51-58

代码与描述一致。「零章必须排在全包最大既有序号之后」本是实例层的不变量，现在却由 bag_marshal 在固定包和动态包两个循环里各写一遍。前置条件只写在 RaiseAcquireSeqForRestore 的声明上（h:527），InsertItemForRestore 两个重载的注释（h:505-506、514-526）都没提「调用前必须先 Raise」。新的还原入口只要漏掉 Raise，重放 [A(seq0), B(seq1)] 就会得到 A=1、B=1 的重复序号。AcquiredBefore 会按 item_id 决胜（item_store.cpp:87-91），所以顺序仍是确定的，不会崩，但 FIFO 淘汰和并堆幸存者会与 Java 口径漂移，且没有任何报错，定级 low 合适。batch3-bag.md §8 的挑战意见 (b) 本来就要求在 Bag/ItemStore 内部收口，Codex 却把它落在了 marshal 里。现有用例 LegacyZeroSequencesFollowEveryExistingStampInTheirOwnBag 只守得住 marshal 这条路径。原修法 FinishRestore 要给 Bag 加一张待补章列表，ItemStore::Insert 也要支持「暂不盖章」，比下面的批量入口更复杂。

#### W1-14

- 原发现（low）：AddItems 一个函数串起 9 步，内部不变量失败一律报 kInvalidTableData，掩盖真实原因
- 复核（W1-bag-core）：**降级 / low**，AddItems 有五处内部不变量失败复用表数据错误码 kInvalidTableData，其中三处没有专门日志；同文件多处函数内 tab 与空格缩进混排
- 位置：cpp/libs/modules/bag/bag_system.cpp:676-679,cpp/libs/modules/bag/bag_system.cpp:699-703,cpp/libs/modules/bag/bag_system.cpp:725-730,cpp/libs/modules/bag/bag_system.cpp:747-750,cpp/libs/modules/bag/bag_system.cpp:754-759,cpp/libs/modules/bag/bag_system.cpp:401-404,cpp/libs/modules/bag/bag_system.cpp:903-923,cpp/libs/modules/bag/bag_system.cpp:1367-1371

属实的部分：Bag::AddItems(vector)（621-784，约 164 行）有五处计划与提交不一致的内部不变量失败都返回 kInvalidTableData，与 645 行真正的表数据错误（max_stack_size==0）同码，排障时会把人引去查配表。缩进混排也属实：RemoveItemByPos 401-404 用空格、其余用 tab；PlanEviction 903-923 用空格、924 起用 tab；InsertItemForRestore 1367-1371 用空格、其余用 tab。需要更正的部分有三点。(1) 「掩盖真实原因」说重了：PrintStackAndReturnError 会打印错误码和调用栈，688、727、756 还各有专门的 LOG_ERROR，只有 678、702、749 三处没有专门日志。asset_op 对任何未改动的失败都回 RETRY（530-533），内部错误回 RETRY 本身就是对的，没有行为后果，只影响诊断。(2) 原修法要求「改用一个现有的内部错误码」，但 bag 域（6000-6014）和 common 域都没有这样的码，kEquipInternalError 属于 equip 域，不能跨域借用。新增码需要改 Tip.xlsx，这是客户端可见契约，按 §12 两版要同批改，要负责人拍板。(3) 这五处检查都不该删：678 删掉后 size_t 下溢会导致 reserve 异常；702 守的是可插拔的 IEvictionPolicy 契约（策略返回了不在包里的 guid）；729 删掉会越界访问 minted；749 发现的是规划高估，可能意味着多淘汰了玩家物品；754 守的是 IContainerLayout::Clone 的非空契约。它们是 §11.3 要求的「内部不变量显式失败」，不属于多余防护。拆成三个私有函数只是可选的可读性改进，不是必需项。

#### W1-15

- 原发现（low）：BagJavaParityTest 的 TearDown 把号段恢复到非基线区间，与同工程其他夹具不一致
- 复核（W1-bag-tests）：**降级 / low**，BagJavaParityTest::TearDown 把 item 号段恢复成 bag_instance_data 的 [900000,1000000) 而不是基线 [1,2^54)，默认顺序下被后续夹具掩盖，乱序或换序运行时会与硬编码 guid 撞号
- 位置：cpp/tests/bag_test/bag_java_parity_test.cpp:124

核心问题成立：第 124 行恢复的区间正是 bag_instance_data_test 的 SetUp 号段（第 64-65 行），是复制粘贴错误。其余夹具（instance_data、selection_order、equip、equip_attribute、feature_handler、mission）的 TearDown 都恢复为基线 [1, 2^54)，bag_selection_order_test 文件头也承诺「每例恢复基线，不依赖执行顺序」。但原文对影响面的描述需要更正。按 vcxproj 的 ClCompile 顺序，bag_java_parity_test.cpp 之后紧接 player_equip_test.cpp：PlayerEquipTest 的 SetUp（第 626 行）会重新装号段，TearDown（第 650 行）恢复基线，再往后的 equip_attribute 也一样。所以默认链接顺序下，player_battle_settlement、feature_persistence 等不装号段的用例拿到的仍是基线，原文「按链接顺序……于是取决于执行顺序」不成立。--gtest_filter 也保持注册顺序，bag_test.cpp 里的用例总排在 parity 之前。只有在 --gtest_shuffle 或其他非默认顺序下，依赖基线又会铸号的用例才可能落进这 10 万个号的窗口。例如 AcquireSeqTest.FifoFollowsSequenceNotGuid 预设了 900001/900002 并铸号：窗口内此前若已铸过 1-2 个号，就会撞号，AddItems 第 686 行判 Contains 后返回失败，表现为偶发红。此外 950001-950003、910001/910002 等硬编码 guid 也在这个区间内。所以这是测试卫生和顺序独立性问题，不影响生产，维持 low。

#### W1-16

- 原发现（medium）：Kafka 必需命令仍经 entt dispatcher 路由，kDispatched 不代表已处理，无监听者时静默吞掉
- 复核（W1-kafka）：**降级 / medium**，Kafka 入口的 13 个必需命令（4 个 scene、9 个 gate，外加 gate 信封回退）仍经 tlsEcs.dispatcher.trigger 转发；没有监听者时静默丢弃，却照样返回 kDispatched。这是任务单 4.4 定下的写法，生产路径目前到不了丢消息的情形
- 位置：tools/proto_generator/protogen/internal/generator/cpp/service_register_info.go:363-384,tools/proto_generator/protogen/internal/template/rpc_event_registry.cpp.tmpl:53-75,tools/proto_generator/protogen/internal/template/service_header.tmpl:37-39,tools/proto_generator/protogen/internal/template/gate_kafka_command_router.cpp.tmpl:43,tools/proto_generator/protogen/internal/template/event_handler.cpp.tmpl:8,cpp/nodes/scene/handler/event/scene_kafka_command_router.cpp:19-24,cpp/generated/rpc/service_metadata/rpc_event_registry.cpp:2101-2140

事实部分成立。(1) 当前生成的 DispatchSceneCommandEvent（42/45/48/49）和 DispatchGateCommandEvent（9 个 gate 事件）每个 case 都是解码后调 tlsEcs.dispatcher.trigger，再返回 kDispatched。gate 的信封回退（gate_kafka_command_router.cpp.tmpl:43）同样只 trigger。(2) EnTT 3.13.2 的 sigh::publish 只遍历 calls，没有监听者就什么都不做。scene 路由（scene_kafka_command_router.cpp:20-24）和 gate 路由对 kDispatched 都不打日志，所以“没处理却报成功”在结构上成立。(3) 全仓 grep：这 13 个事件各只有一个 sink（battle_event_handler.cpp:9-10、team_event_handler.cpp:9、player_inbox_event_handler.cpp:9、gate_event_handler.cpp:28-37），也没有任何进程内 trigger，它们只能从 Kafka 进来，确实是单消费者的必需命令。按 ecs-fit-overview.md 第 (6) 条，这类命令应当直接调用。

需要更正的描述有三点。(a) 这个写法不是 Codex 偏离任务：batch4-external-id.md:154（4a-4）明确规定每个 case“成功则 tlsEcs.dispatcher.trigger 后 return kDispatched”，ecs-fit-overview.md 第 9 项也只写了“Kafka 入口归第 4 批白名单”。所以这是任务单层面留下的结构问题，应写进补充任务单，不应记成 Codex 未落实。(b) 生产路径目前不会丢消息：node_entry.h:85 的 RegisterNodeEvents 先于 :90 的 SetKafkaHandlers；全仓没有任何地方调用 EventHandler::UnRegister；关机时 node.cpp 的 FinalizeTeardownInLoop 先 kafkaManager.Shutdown() 再 tlsEcs.Clear()，而 dispatcher.clear() 只丢弃排队中的事件，不断开 sink。所以这是潜在缺陷加违规，按“潜在缺陷、违规且有后果”维持 medium。(c) 原修法有问题。首选方案把节点实现的 HandleSceneCommand 接到 rpc_event_registry.cpp 里，但这个文件属于各节点共享的 rpc 库（cpp/generated/rpc/CMakeLists.txt:36、rpc.vcxproj:80）。共享库不应反向依赖 scene/gate 节点的 handler，派发 switch 应当移到节点本地的生成文件里。“最小改法”在 trigger 前判 sink().empty() 并新增 kNoHandler，等于再加一层防护，正是负责人认为的通病，不应采用。另外，gate 的长期目标是不持有 ECS 世界（ecs-fit-overview.md“长期目标形态”），gate 命令经 tlsEcs.dispatcher 转发也和这个目标冲突。

#### W1-17

- 原发现（medium）：白名单单测把上界写死为 49，漏测已进白名单的 49 号 PlayerInboxNotifyEvent 和 [49,kMaxEventCount) 区间
- 复核（W1-kafka）：**成立 / medium**，白名单单测把上界写死为 49，没有覆盖已进入白名单的 49 号 PlayerInboxNotifyEvent，也拦不住以后把新事件号（≥49）误标进白名单
- 位置：cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:13,cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:25,cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp:32

当前代码证实了这一条。生成的 IsSceneCommandEvent(49) 为 true，kMaxEventCount 已是 50，但两个循环只跑到 48，kSceneEvents 里也没有 49。测试照样是绿的，起不到注释所说“固定已发布事件号，防止墓碑或内部事件重新进入白名单”的作用：49 号是否放行没有被钉住，以后新增的事件号（≥49）被误标进白名单，这个测试也不会红。根因是上界写成了字面量。第 4 批编写时 kMaxEventCount 仍为 49（batch4-external-id.md:349），写死的 49 在当时等于 kMaxEventCount；玩家收件箱会话新增 49 号事件后，测试没有跟上。任务单 4.5 要求的“≥kMaxEventCount 的号”只用 UINT32_MAX 测了一个点，这一点也属实。属于“测试无效”，按标准为 medium。原修法正确，而且是最直接的做法。

#### W1-18

- 原发现（medium）：一次性迁移逻辑被固化为生成器每轮必跑的步骤：正则改写三份手工维护的工程清单，并硬编码 event_ingress_option 和 attribute_sync_option 锚点
- 复核（W1-kafka）：**成立 / medium**，一次性的事件迁移逻辑被固化成生成器每轮必跑的步骤：用正则改写三份手工维护的构建清单，并以 attribute_sync_option 为锚点硬编码补登 event_ingress_option。迁移早已完成，这段逻辑现在只剩新增 Fatal 的副作用
- 位置：tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:84-87,tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:90,tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:141,tools/proto_generator/protogen/internal/prototools/local_event_cleanup.go:157-192,tools/proto_generator/protogen/cmd/pipeline.go:46,tools/proto_generator/protogen/internal/prototools/local_event_cleanup_test.go:83-134

核实结果如下。(1) 生成器中只有这一个文件会读写 cpp/generated/proto 的 CMakeLists、vcxproj 和 filters（在 protogen 里 grep vcxproj / CMakeLists，非测试代码只命中本文件）。仓库原来的做法是手工登记：AGENTS §4.4 要求表的 pb 手工加入工程清单，任务单 4a-1 第 3 步也要求照 attribute_sync_option 的先例手工登记。(2) 迁移已经完成：proto/common/event 只剩 4 个 proto；cpp/generated/proto/common/event、go/proto/common/event、service_metadata/common_event_\*、db/login/robot 的 staging 目录都只剩对应产物；三份清单都已登记 event_ingress_option。所以 :157-192 现在每轮都走 len(newEntries)==1 的 continue 分支，没有实际作用，只剩失败面。例如有人在 CMakeLists 里给 option 加一行 .pb.h，:188 会匹配到 2 条，2 != 1，于是 Fatal；vcxproj 里的条目带了 Filter 以外的子元素时，正则不匹配，代码会照锚点再插一行，随后 optionRefs 计数不符，同样 Fatal。(3) 这段清理只覆盖 common/event 一个目录，生成器其他目录仍不删陈旧产物（batch4:353/620 写明“生成器不会删除陈旧产物”）。清单因此有了两套事实源：手工维护加本函数的正则。挑战意见 §8（batch4:658-659）的低级别项只要求补删 staging 副本，或让生成器在复制前清空目标目录，并没有要求正则改写清单。Codex 这样做，大概是因为它不能手改生成树、只能借用户重跑生成来落地迁移（README §7 回执写了“未手改生成树”），但迁移落地后没有撤掉。这违背 KISS / YAGNI 和“与仓库一致”，属于明显过度设计，medium 成立。原修法方向正确；按“优先删除”的偏好，应把整步删掉，而不只是删 addIngressOption。

#### W1-19

- 原发现（low）：任务单要求的 DispatchProtoEvent 零命中门禁未达成；已删 handler 仍被文档引用
- 复核（W1-kafka-docs）：**成立 / low**，已删除的 DispatchProtoEvent 和 SceneEventHandler 仍被注释与规范引用，任务单 4.4 第 6 条的零命中门禁没有达成
- 位置：cpp/nodes/scene/handler/event/battle_event_handler.cpp:21, cpp/nodes/scene/handler/event/team_event_handler.cpp:19, cpp/nodes/scene/handler/event/player_inbox_event_handler.cpp:19, cpp/libs/services/scene/player/system/player_team.cpp:38, go/login/internal/svc/gate_command.go:106, tools/proto_generator/protogen/internal/generator/cpp/event_id.go:455, cpp/AGENTS.md:69, docs/design/team-system.md:1603, docs/design/team-system.md:1932, docs/design/scene-enter-session-fence.md:126, docs/design/agones-scene-node-high-density.md:135

按当前工作区重新 grep。DispatchProtoEvent 已经从生成模板（rpc_event_registry.cpp.tmpl、service_header.tmpl）、生成产物以及 node_kafka_command_handler.h 的 DefaultKafkaCommandDispatch 中删除，Kafka 入口改为 DispatchSceneCommandEvent 和 DispatchGateCommandEvent，两者先查 IsScene/GateCommandEvent 白名单。但 cpp、go、tools 下仍有 6 处注释写着旧名字。原发现列了 5 处，这次多出 player_inbox_event_handler.cpp:19。这一处来自另一个会话新增的收件箱 handler，是照抄过时注释的结果，说明过时注释已经在被复制传播。event_id.go:455 的说法也不对了：墓碑号 43、44 不在白名单里，现在由 Dispatch\*CommandEvent 返回 kNotAllowed，scene 和 gate 都打 LOG_ERROR，不再是“default 分支静默丢弃”。cpp/AGENTS.md:69 要求“只在 SceneEventHandler::OnSceneCreatedHandler 计数”，但仓库里已经没有 SceneEventHandler 这个类，真实计数点是 scene_local_events.cpp:127/133 的自由函数，174/175 处接线。docs/design/agones-scene-node-high-density.md:135-136 还指向已删除的 scene_event_handler.cpp，原发现漏了这一处。以上都只是注释和文档，运行期行为正确，所以维持 low。docs/PROGRESS.md 只能追加，审计文档和 batch4 任务单是历史记录，这三处不在修改范围内。

#### W1-20

- 原发现（low）：节点模板仍只声明 KafkaCommandType，照抄会撞上新加的 static_assert
- 复核（W1-kafka-docs）：**成立 / low**，两份节点模板和模板 README 没有跟上新 hook 契约：只声明 KafkaCommandType 而缺 DispatchKafkaCommand，照抄就会撞上 static_assert，也看不出该接哪个白名单
- 位置：cpp/nodes/_template/main.simple.cpp.example:14, cpp/nodes/_template/main.with_context.cpp.example:19, cpp/nodes/_template/README.md:32, cpp/libs/engine/core/node/system/node/node_entry.h:87

git diff 6281e4d27 显示，本批在 node_entry.h 加了 static_assert，要求存在 THooks::DispatchKafkaCommand，同时删掉了兜底的 DefaultKafkaCommandDispatch 和无参 RegisterKafkaCommandHandler\<CommandT>(node)。两份 .example 在这段时间的改动只有另一个会话加的 etcd_transport::InstallFromEnv()，hook 结构没动。真实节点不受影响：scene/main.cpp:77-78 和 gate/main.cpp:176-177 都已经补上 DispatchKafkaCommand，battle 也说明了不声明 KafkaCommandType。所以这只是新建节点的指引过时，维持 low。原修法有两处要补：(1) 模板 README 的 THooks Convention 一节写的是 struct KafkaHandler{CommandType; Dispatch} 和 EventHandlerType，node_entry.h 都不认识，这是更早就过时的第三种写法，本批改契约时也应一并改正；(2) 原修法给模板注释写的是“必须调用生成的白名单派发 Dispatch\*CommandEvent”，但生成器 event_ingress.go 只有 eventIngressScene 和 eventIngressGate 两种入口，入口由 proto 文件位置决定，新节点类型没有现成的生成派发函数可调。注释必须写清楚新入口要先在生成器里登记，否则新建节点的人只能自己 trigger，正好绕开白名单。

#### W1-21

- 原发现（low）：白名单规则文档与现状不符；事故文档 F001 未按任务单改为指向新方案
- 复核（W1-kafka-docs）：**成立 / low**，控制面 topic 文档里的 scene 白名单数量和生成状态都已过时；事故文档 F001/F002 没有按任务单 4.5 第 2 条改为指向新方案
- 位置：docs/design/control-plane-topic-partitioning-20260908.md:312, docs/design/control-plane-topic-partitioning-20260908.md:314, docs/ops/incident-scene-ecs-gameplay-20261014.md:37, docs/ops/incident-scene-ecs-gameplay-20261014.md:83, docs/ops/incident-scene-ecs-gameplay-20261014.md:130

当前生成的 kafka_command_event_whitelist.h 已经提交（526780b4e、de4960bee）。scene 白名单是 42、45、48、49 四个事件，其中 49 PlayerInboxNotifyEvent 是收件箱会话加的，加的时候没同步这份文档。:312 仍写 3 个，:314 仍写“生成产物尚待执行”，两处都与现状不符。gate 的 9 个事件和生成头文件一致，这部分没错。batch4 任务单 4.5 第 2 条明确要求“事故文档里 F001 的待办改为指向本方案”，但事故文档 :37 仍是“未处理”，:83 链接的 DefaultKafkaCommandDispatch 和 DispatchProtoEvent 也都已删除，同一个入口在两份文档里状态互相矛盾。审计文档第 5 行明确写了“HEAD 状态等待独立审查后统一更新”，属于有意推迟，不在本条范围内。原修法不直接标关闭，只写“源码已收口、运行验收待做”，这符合事故文档自己“不能因生产者尚未盘清而把入口记为安全”的口径。附带发现一个低风险缺口，建议另记一条 low：cpp/tests/kafka_command_test/kafka_command_whitelist_test.cpp 的两个循环上界硬写成 49，kSceneEvents 仍是 {42,45,48}。kMaxEventCount 已经变成 50，事件 49 被加进白名单后测试也不会红，快照测试对 49 及以后的号码完全不覆盖。

#### W1-22

- 原发现（medium）：沉默状态、升级重算这类必需步骤仍经 dispatcher 一对一转发，没有订阅就静默失效
- 复核（W1-dispatch-required）：**降级 / medium**，沉默状态和 GM 调级后的重算都是只有一个消费者的必需步骤，现在仍经 dispatcher 转发：不装订阅的宿主里会静默失效，skill_test 只能手工复刻接线，Buff 系统还为任意监听者的重入写了防御代码和专项测试
- 位置：cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:85,cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:94,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:65,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:139,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:171,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:176,cpp/libs/services/scene/player/system/player_attribute.cpp:1142,cpp/libs/services/scene/actor/actor_events.h:8,cpp/libs/services/scene/player/player_events.h:7,cpp/tests/skill_test/skill_test.cpp:209,cpp/tests/buff_test/buff_test.cpp:686,cpp/libs/services/scene/combat/buff/system/buff.cpp:232

核心问题成立，严重度 medium 不变，但原描述和修法要更正几处。

【成立的部分】

(1) AddSilenceState/RemoveSilenceState 只发事件。唯一的生产订阅是 scene_local_events.cpp:139-147 的薄转发，原样调用 CombatStateSystem::AddCombatState/RemoveCombatState。buff_impl 和 combat_state 同在 libs/services/scene 库，skill.cpp 已经直接 include combat_state.h 调用 ValidateSkillUsage，因此没有任何分层上的理由要经过 dispatcher。

(2) GmSetLevel（player_attribute.cpp:1123-1148）改了存档用的 LevelComp 后，派生属性重算、面板推送、宠物重算和推列表，都交给组合根 handler（scene_local_events.cpp:65-81）。这个 handler 回头调用的正是 PlayerAttributeSystem 自己。GmSetLevel 是唯一的 trigger 点，唯一订阅来自 main.cpp:108 的 RegisterAll。

(3) 生产环境装了 RegisterAll，所以线上目前行为正确，不到 high。但这违反了“dispatcher 只做可有可无的一对多通知”这条判据，而且有实际后果，按 medium 计。

【需要更正的描述】

(a) 原文说“仓库里没有任何测试覆盖沉默生效”“skill_test 只装 RegisterMission 或什么都不装”，这两句都不对。skill_test.cpp:209/226 的 SkillPrecheckPolicyTest 手工 connect 了 CombatStateAddedEvent→AddCombatState，:270-275 断言沉默已写进 CombatStateCollectionComp。这反倒是本问题最直接的证据：测试宿主必须照抄组合根的接线，漏抄就静默失效。

(b) 原文漏了一个更大的代价。buff_test.cpp:686-745 的三个用例（ChildStartCallbackRemovesOwner_DoesNotLeaveChildOrArmTimer、OwnerRemovalCallback_DoesNotConsumeIndependentBuff、ResetCallback_DoesNotConsumeIndependentBuff）就是借这两个事件注入任意监听者来模拟重入的。BuffSystem 里相应的防御，包括 buff.cpp:232-240“启动回调可以同步重入 / 启动回调已移除本条”、:489-509 OnBuffStart 每路重查、:278“先标记整批再触发移除回调”，前提都是这些事件可能有任意监听者。已核对：改成直接调用后，Start/Remove/Destroy 钩子链里没有任何一处能回到 BuffSystem。BuffImpl 只经 AddCombatState 走到 MarkAttributeForUpdate（只标脏位）；ModifierBuffImplSystem 也只调 MarkAttributeForUpdate；MotionModifier 全是空函数。所以这些防御会随之失去存在理由。

【需要更正的修法】

原修法要新增 PlayerAttributeSystem::OnLevelChanged。现在只有 GmSetLevel 一个调用方，按 YAGNI 直接内联进 GmSetLevel 更直接，等经验系统接入出现第二个调用方时再抽函数。

【与其他条目的关系】

与 W1-23 不同根：W1-23 讲的是零监听或空监听的事件；本条讲的是唯一消费者承担必需步骤的事件。

#### W1-23

- 原发现（medium）：保留了多个生产环境零监听或监听为空操作的事件，并为它们写了大段重入复核
- 复核（W1-events-reentry）：**成立 / medium**，生产中零监听或只有空监听的 6 个事件仍在 trigger，还配了通知后重入复核和人造重入测试；复核分支一旦走到会留下半成品状态
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:227-228,252-270, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2909-2927, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4095-4171,4176-4180,4199, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2793-2797, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2886-2899, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3089-3093, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4538-4552, cpp/libs/modules/mission/system/mission.cpp:107-110, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:52-63,83-125,170-186, docs/design/ecs-component-access-rules.md:59,83

逐项核对了当前代码。(1) 零监听：在 cpp/ 下 grep，BeforeLeaveScene、InitializeActorCompsEvent、OnAcceptedMissionEvent 在生产代码里没有任何 sink，只有 cross_zone_test 和 player_feature_persistence_test 订阅。RegisterPlayerEvent 和 InitializePlayerCompsEvent 的生产 handler（scene_local_events.cpp:52-63、83-94）只做 valid 检查。PlayerLoginEvent 的 handler（96-125）只打一行日志，各分支都写着“待接”。对比基线 6281e4d27：这些事件原来都挂着真实 handler（SceneEventHandler 调 AOI 和 crowd，RegisterPlayer 调 PlayerSkillSystem::RegisterPlayer，InitializePlayerComps 调 PlayerMissionSystem::Initialize）。Codex 把 handler 正文改成直接调用后，trigger 留了下来，所以这些事件变成了零监听。OnAcceptedMissionEvent 在基线时 handler 就是空的。同一轮里 Codex 已经把同类的零监听事件 BeKillEvent、InterruptCurrentStateEvent 等删掉了（HEAD 中 grep 无结果），处理口径前后不一致。(2) 防护的代价：batch5 §10、§11 的回执明确写了“原三种通知后的实体版本和 playerList 归属校验均保留”。于是有了 EntryOwnershipGuard（9 个字段的快照）、canContinueInitialization（25 行）、HandleEnterScene 在通知后的复核和 CopyFrom 重取、DetachFromScene、HandleExitGameNode、DestroyDeposedPlayer 和 DestroyPlayer 里的提前 return 与“替身”判断，还有约 14 个只靠人造监听者才能走到的测试。ecs-component-access-rules.md:83 还把“trigger 可以销毁实体……返回后重验”写成了规则，等于把错误模式固化下来。(3) 复核分支本身就有问题（W1-32 的 a/b/c 已在当前代码中核实，并入本条）：HandleExitGameNode 在 3054 挂上 UnregisterPlayer 之后，3090-3093 在“旧实体仍有效、索引已指向替身”时直接 return，旧实体从此泄漏，IsEmergencyRelocateDrained 依赖的 view\<Player>().size()==0（3582）永远不成立，而测试 cross_zone_test.cpp:4352 把这个泄漏当作期望结果断言；canContinueInitialization 在“实体仍有效、仍在索引里但快照变了”时不擦索引就返回 null，persistence_test:1131 断言这个没有场景的实体留在 playerList 里；HandleEnterScene 在 259 行先解绑旧场景，到 261-268 才复核，复核失败时玩家不属于任何场景，cross_zone_test:5548 断言了这个状态。(4) 第 5 批挑战意见 §8 的 low 项 B2(6)（batch5-lifecycle-types.md:462-463）要求“通知后不再 re-fetch、不提前 return，守卫放在任何修改之前”，当前 261-270 两条都违反了。(5) 原描述需要更正一处：EnterScene 里 canContinueEntry 的大部分调用跟在真实业务调用之后（ResolveTravelOutcome、PlayerTeamSystem、PlayerBattleSystem、PlayerLandingSystem），不全是为“不存在的监听者”写的。只有 2797（PlayerLoginEvent 之后）这一处纯粹是通知复核。这一处不影响结论。生产路径走不到这些分支，所以不升到 high；违反 KISS/YAGNI、ecs-fit 判据 (6) 和挑战意见，且代价可见，定 medium 成立。

#### W1-24

- 原发现（medium）：66 号 ActorBaseAttributesS2C 只有标了 entity_id 脏位时才带主体标识，移动和速度增量的 entity_id 恒为 0，客户端无法归属（存量问题）
- 复核（W1-netids）：**降级 / low**，66 号 ActorBaseAttributesS2C 只在战斗状态那一路带 entity_id，移动和速度增量的 entity_id 为 0。这是改造前就有的缺口，Java 已修，C++ 待跟；真客户端没有 66 的处理器，ECS 改造也没有让它变坏
- 位置：tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:34-42, cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:32-40, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:104-109

现象核对属实。生成的序列化器只在 kEntityIdFieldNumber 被置位的那一帧写 entity_id，而生产代码中只有 ResetCombatStateFlags 置这个位。InternalBroadcast 的信封只有 message_id 和序列化后的消息体（player_message_utils.cpp:88-156），不带主体。NPC 没有 PlayerGuidComp，任何一帧都拿不到 entity_id。

但原严重度和定性需要更正，理由有四条：

1) 改造前就是这样。6281e4d27 的模板用 try_get\<uint64_t>，置位规则完全相同，第 4 批只把组件换成 PlayerGuidComp，语义没有变化。

2) 真客户端不消费 66。C++ 仓 team-system.md:2245 与 Java PARITY.md:53/148 都写明客户端没有 66 的 proto 类和处理器；远端移动已经改由 133 ActorMoveS2C 广播，它带 net id（movement.cpp:163-182）。所以“客户端无法归属”今天没有可见后果。

3) C++ robot 不读 66。Java robot 的 MoveAssertions.ownerOf 已把 entity_id==0 显式归为 UNATTRIBUTED。

4) 这是已登记的双版本差异。Java PARITY.md:53 写的是“有意差异（mmorpg 待做）：每条都带 entity_id = player_id”，Java 已经修了。

因此这不是 ECS 改造引入或恶化的问题，也不是契约破坏，只是一个已登记、C++ 待跟的存量对齐项。对 ECS 交付说明“66 号 entity_id 仍为玩家 GUID”的批评只算文档措辞不全，按 low 处理。

#### W1-25

- 原发现（low）：ActorNetIds 接缝不够收敛：组件公开可写、Of 在 AOI 热路径上反复做反查自检、调用方和被调方重复判 0 且被调方静默半填消息
- 复核（W1-netids）：**降级 / low**，ActorNetIdComp 定义在公共头里、可以随意写，导致 Of、Resolve、Assign 各自再做一遍反查自检；FillActorCreateMessageInfo 与两个调用方重复判 0，契约也没写明
- 位置：cpp/libs/services/scene/actor/actor_net_id.h:8-11, cpp/libs/services/scene/actor/actor_net_id.cpp:39-42, cpp/libs/services/scene/actor/actor_net_id.cpp:57-73, cpp/libs/services/scene/spatial/system/view.cpp:91-98, cpp/libs/services/scene/spatial/system/aoi.cpp:185-190, cpp/libs/services/scene/player/system/player_scene.cpp:294-296

主体成立。

- 组件公开且可写，生产代码中只有 Assign 写它；只有测试直接 remove 或 any_of 这个组件（actor_net_id_test.cpp:21/26/46/47/79）。

- 有了 on_destroy\<ActorNetIdComp> 维护（EnTT 3.13 的 sigh_mixin::pop/pop_all 先 publish 再擦除，mixin.hpp:44-75），索引与组件只会在有人绕过 Assign 时不一致，比如 emplace_or_replace 或 patch。Of 里的 Resolve 回查、Resolve 里的 id->value==netId、Assign:39-42 的一致性分支，三处都只是在兜“组件公开”这一个口子。这正是“保留本该删的，再加防护兜住”的写法。

- FillActorCreateMessageInfo 遇到 0 就提前返回。这时调用方已经 add_actor_list()，会留下 entity=0 的空项。现在两个调用方都先判了 Of!=0，所以不可达，但契约是隐式的。另外 observer 参数在函数体里没有使用。

需要更正的地方：

1) 调用方是两处（aoi.cpp:186、player_scene.cpp:294），原文写的是三处。

2) “AOI 热路径上的查找次数”没有 profiling 数据。按 AGENTS §11.3 不能拿它当理由，只能算可读性和信息隐藏的问题。

3) 原文的可选建议“把索引改成 EcsContext 的具名成员”与任务单 4d-1 第 6 条的分层决定冲突。EcsContext 在 cpp/libs/engine，不认识 scene 组件，所以这条建议应当撤回。

结论：严重度维持 low，描述和修法按上面更正。

#### W1-26

- 原发现（low）：客户端 target_id 在一次 ReleaseSkill 中被反查最多 6 次，任务单要求的“入口解析一次后传 entt::entity”只做了一半
- 复核（W1-netids）：**成立 / low**，同一个不可信的 target_id 在一次 ReleaseSkill 里被反查最多 6 次；uint64 版的 ResolveSkillTarget 没有按任务单删掉，ValidateTarget 还手写了一遍与 ValidateSkillTargetEntity 重复的检查
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:110-112, cpp/libs/services/scene/combat/skill/system/skill.cpp:201-249, cpp/libs/services/scene/combat/skill/system/skill.cpp:251-269, cpp/libs/services/scene/combat/skill/system/skill.cpp:507-534, cpp/libs/services/scene/combat/skill/system/skill.cpp:633-634

逐处核对，一次 ReleaseSkill 的解析位置是：

- ValidateTarget :507 直接 Resolve，:529 再 ResolveSkillTarget；

- LookAtTargetPosition :204，仅在没有 position 时；

- BroadcastSkillUsedMessage :633；

- CreateSkillContext :218；

- ApplySkillHitEffectIfValid :243。

合计最多 6 次。任务单 4d-2 第 3 条要求“ResolveSkillTarget 改为接收 entt::entity，客户端路径先 Resolve 再做校验”。实现新增了实体版 ValidateSkillTargetEntity，却保留了 uint64 包装，并在 5 处重复使用，只做了一半。

反证核对：ValidateTarget 之后的 CheckCasting、CheckRecovery、CheckChannel 可能调用 HandleSkillFinish（只删施法者的上下文）、SendSkillInterruptedMessage 和 CheckState，都不会改动目标实体，所以几次解析的结果目前一致，不是正确性问题。因此维持 low，归为接缝不清和可读性问题。

补充一个修法时要注意的点：ValidateTarget 现在的顺序是 无效/类型 → InBattle（返回 kSkillInvalidTarget）→ 场景/冻结（返回 kSkillInvalidTargetId）。合并成一次校验后，如果目标“在战斗中且不在同场景”，返回码会从 kSkillInvalidTarget 变成 kSkillInvalidTargetId。两者都是拒绝，现有测试也没有覆盖这个组合，但交付说明里要写明。

#### W1-27

- 原发现（low）：进程内事件结构体里留有无人写、无人读的字段，以及调用空函数的“事件”
- 复核（W1-dispatch-required）：**成立 / low**，OnNodeRemoveEvent::sceneNodeType 是无人写、无人读的死字段；SkillExecutedEvent 的 skillId/skillTableId 从不赋值，唯一消费者 BuffSystem::OnSkillExecuted 是空 TODO
- 位置：cpp/libs/engine/core/node/system/node/node_events.h:10,cpp/libs/services/scene/combat/combat_events.h:6,cpp/libs/services/scene/combat/skill/system/skill.cpp:19,cpp/libs/services/scene/combat/skill/system/skill.cpp:932,cpp/libs/services/scene/combat/buff/system/buff.cpp:588,cpp/libs/services/scene/combat/buff/system/buff.h:7,cpp/libs/services/scene/combat/buff/system/buff.h:83,cpp/libs/services/scene/scene.vcxproj:329,cpp/libs/services/scene/scene.vcxproj.filters:161

在当前代码中逐项核实，都成立：

- sceneNodeType：在 cpp 全仓 grep，只有 node_events.h:10 的定义；唯一构造点 node.cpp:1540-1542 只写了 node/nodeType，唯一消费者 RpcClientSessionHandler::OnNodeRemoveEventHandler（client_message_processor.cpp:1238）只读 node/nodeType。

- SkillExecutedEvent：只有 skill.cpp:932 一个构造点，没有写 skillId/skillTableId；唯一消费者 OnSkillExecuted 是空函数，没有任何测试引用它。另外 :934 用 entt::entity{skillContext->target()} 重新转了一次句柄，而同一作用域里已经有校验过的 targetEntity，属于多余转换；整体删除后这个问题也随之消失。

补充出处：这两个死字段是第 4 批任务单 batch4-external-id.md §4.10（:307、:328）照旧 proto 写下来的，不是 Codex 擅自加的。但任务单同节也写了“没有生产者的事件直接删除（YAGNI）”的口径，空消费者加半填结构体与这一口径不符。

严重度维持 low：没有运行期影响，只是可读性问题，将来接入时会埋下静默读到 0 的陷阱。不与 W1-23 重复：SkillExecutedEvent 不经过 dispatcher，W1-23 讲的是零监听的 dispatcher 事件。

#### W1-28

- 原发现（low）：gate 节点句柄类型化后残留旧整数逻辑：冗余别名和不可达的重绑分支，“scene_node=”日志打的是实体整数
- 复核（W1-gate-ids）：**成立 / low**，gate 节点句柄类型化后残留旧整数逻辑：GetEffectiveNodeEntity 成了多余包装，ResolveSessionTargetNode 里有无意义别名和不可达的重绑分支，ForwardPlayerToScene 日志用“scene_node=”打印实体整数
- 位置：cpp/nodes/gate/handler/rpc/client_message_processor.cpp:136,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:287,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:495,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:788,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:93,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:146,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:152,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:161,cpp/libs/services/gate/session/comp/session_info_comp.h:19,cpp/libs/services/gate/session/comp/session_info_comp.h:127

按当前工作区逐项核对，问题都还在。(1) GetEffectiveNodeEntity 只可能返回仍然有效的 stored 句柄或 entt::null。EnTT 3.13.2 的 registry::valid 走 sparse_set::contains，sparse_ptr(null) 取不到页，valid(entt::null) 恒为 false，所以 null 在 :290 已经返回。走到 :299 时 effectiveNodeEntity 必然等于 session.GetNodeEntity(nodeType)，重绑分支不可达。:289 的 nodeEntity 是原来 entt::entity{effectiveEntityInt} 转换被机械替换后剩下的别名。基线 6281e4d27 的 GetEffectiveNodeId 也只返回 stored 值或 kInvalidEntityId，这个分支在改造前就不可达；类型化改造照搬了下来，没有趁机删掉。(2) 更进一步：两处调用方（:288 和 :788）拿到结果后都会再做一次 registry.valid，所以 GetEffectiveNodeEntity 本身就是多余包装，相当于在同一个 registry 上把 valid 判断做了两遍。(3) 同样的别名残留还有 client_message_processor.cpp:495/498（sceneEntityId 被别名成 sceneEntity）和 scene_entry_dispatch.cpp:98。ResolveSceneNodeEntity（:161-169）原来的整数转换去掉后，函数体只剩 `if (!x) return nullopt; return *x;`，等价于直接返回 FindNodeEntityByNodeId 的结果。(4) ForwardPlayerToScene 在 :146（采样 WARN）和 :152（DEBUG）用“scene_node=”标签打印 64 位实体整数。基线就是这样，没有截断，标签也不是 scene_node_id，所以比 F084 轻；但 4c 的口径是实体整数不再冒充节点标识，4c-2 在断线路径已经改为通过 try_get\<NodeInfo> 取 node_id，这两处属于同类漏改。(5) SessionInfo 成员仍叫 entityIds，参数仍叫 entityId，gate_service_handler.cpp:278 的注释也引用 entityIds。行为全部正确，只是可读性问题，所以维持 low。

#### W1-29

- 原发现（low）：新增的“同 index 不同 version 判为换节点”用例在修复前就会通过，测不出任何回归
- 复核（W1-gate-ids）：**降级 / low**，“同 index 不同 version 判为换节点”用例是类型契约固定用例，不是修复前会红的回归测试，回执把它称作“回归”是误标
- 位置：cpp/tests/routing_identity_test/routing_identity_test.cpp:643,docs/handoff/ecs-refactor/batch4-external-id.md:691

前半句成立。基线里 SessionInfo 存的就是完整 64 位 to_integral，ResolveSceneNodeEntity 也返回 to_integral，CompareSceneNode 按 uint64 比较，同 index 不同 version 本来就判为 kChanged。审计表把 F033 定性为“裸 uint64 存句柄”的类型安全问题，没有任何行为缺陷，所以不可能有修复前会红的用例。batch4 §9 回执写“补同index不同version重绑回归”属于误标。根因在任务单本身：§4.9 第 5 条和 §6 要求补这个用例，§8 挑战意见只点了 AnomalyDetector 的同类问题，漏了这一条。后半句“测不出任何回归”不成立。如果将来有人把存储收窄成 uint32 或只存 index（例如 Set 时存 static_cast\<uint32_t>(to_integral(e))、Get 时返回 entt::entity{stored}），:656 的 kChanged 仍会通过，因为比较的是截断后的 stored 和完整的 replacement；但 :657 的 EXPECT_EQ(replacement, GetNodeEntity) 会红，因为截断后取回的是 entity{0}，replacement 是 1\<\<32。所以它能守住将来丢 version 的回归，属于有效的固定用例。原修法建议的 static_assert(decltype(GetNodeEntity)==entt::entity) 和 grep 门禁都是多余的防护层：用例把 entt::entity 直接传给 SetNodeEntity 和 RebindSceneNode，接口一旦退回整数形参，enum class 不会隐式转换，用例本身就编不过，类型已经被钉住。按负责人“不再加一层防护”的偏好，应当去掉这部分修法。

#### W1-30

- 原发现（low）：事件结构体化后多处注释或文档仍描述旧机制
- 复核（W1-dispatch-required）：**降级 / low**，事件结构体化后仍有注释描述旧机制：mission.cpp 的“截成 uint32”注释和多余别名、CreateScene 回滚注释里的“AOI 格子表”、属性同步注释里的“Guid(uint64_t)组件”；回滚分支本身在生产中也已不可达
- 位置：cpp/libs/modules/mission/system/mission.cpp:345,cpp/nodes/scene/handler/rpc/scene_handler.cpp:754,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:62,cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:107

保留的 4 项都已在当前代码中逐一核实：

- mission.cpp：OnMissionCompletion 的参数已经是 entt::entity，不存在截断；playerEntityValue 只是 :373/:388/:394 用到的别名。

- CreateScene 回滚注释：OnSceneCreated 的唯一生产监听者是 scene_local_events.cpp:127 的 Agones 计数；AOI 侧已不再订阅，grid.cpp:42 写明格子表在首次入格时创建。注释里的“AOI 格子表”已经过时。

- actor_attribute_calculator.cpp:107：生成的序列化器读的是 PlayerGuidComp。

需要更正的有两处：

(1) 原条目里有两项与别的条目重复，从本条剔除：cpp/AGENTS.md:69 指向已删的 SceneEventHandler，已由 W1-19 覆盖，修法相同；player_state_attribute_sync.proto 的 entity_id 注释已写进 W1-24 的修法，而且客户端可见，须按 §12 两版同批改。

(2) 回滚注释过时的根源比原文说的更深。scene_handler.cpp:754-771 和 scene_node_service.cpp:62-78 的“trigger 后重查 SceneInfoComp + 补发 OnSceneDestroyed 回滚”是为审计 F096“监听者可能删场景”加的防御。现在唯一的监听者 GameServerLifecycle::OnUnitCreated 不碰任何 registry，tests 里也没有任何 sink\<OnSceneCreated>，所以这整段分支在生产中不可达。只删掉注释里的四个字是治标；按负责人“优先删除”的口径，应把分支一起删掉，与 W1-23 属于同一类。

删掉重复项后仍有多处过时注释和一段死防御，严重度维持 low。

#### W1-31

- 原发现（low）：客户端可见 actor 号的取值区间和重启语义已变，但 F080 的三项客户端确认没有书面记录，与 Java 版语义也不完全一致
- 复核（W1-netids）：**降级 / low**，actor net id 的唯一性范围（同时在线的节点之间唯一，进程重启后同一 node_id 会重新发同样的号）与 Java 雪花号不同，但没有写进两版的契约文档；F080 的三项客户端确认也没有记录。原文“取值区间已变”的担忧不成立
- 位置：cpp/libs/services/scene/actor/actor_net_id.h:17-19, docs/handoff/ecs-refactor/batch4-external-id.md:529, docs/handoff/ecs-refactor/batch4-external-id.md:625, docs/handoff/ecs-refactor/batch4-external-id.md:629, docs/handoff/ecs-refactor/README.md:123, D:/luyuan/xuanming-server-mmo/docs/reference/mmorpg-client-contract-aoi.md:146, D:/luyuan/xuanming-server-mmo/PARITY.md:177

更正之一：原文担心“取值从小于 2^32 变成恒大于等于 2^32”，不成立。改造前的 EcsContext 构造函数（6281e4d27 也一样）就会先把 0 号槽位 create 再 destroy，让它的 version 变成 1。之后第一个复用这个槽位的 actor，整数值就是 1\<\<32；此后任何被回收复用的槽位，version 都不小于 1。所以客户端早已收到过不小于 2^32 的 entity，取值区间不是新风险。

更正之二：重启语义并非完全没写。README.md:123 已注明“不承诺进程重启后永久唯一”。而且新方案比改造前严格更好：改造前 to_integral 在同一进程内、跨节点都会撞号，现在同时在线的节点之间也不会撞。

确实还存在的问题：

1) F080 的三项客户端确认（是否把号当不透明值使用、换场景或换节点时是否清空 actor 表、是否接受方案 B）在 README §7、§8 和 PROGRESS:8299 里都没有记录，只写了“默认兼容 A”。

2) 任务单 §8 的挑战意见要求把 batch4 的 decisions_needed 改写成“同时在线节点间唯一，node_id 回收后可能重复”，并更正与 Java 的对比。:529、:625、:629 仍是原文，仍写着“语义与 Java 现状一致”。

3) 4d-3 要求更新 Java 仓的 mmorpg-client-contract-aoi.md:146，但那里仍写着“C++ 是 entt::to_integral，会复用”，已经过时。PARITY.md:177 只写“Actor不透明身份语义已有”，没有写两版唯一性范围的差异，这违反 AGENTS §12。

是否真会出问题取决于客户端在重连或换节点时是否清空 actor 表。AGENTS §9 不允许读客户端仓，所以无法证实有运行期缺陷，只能算文档和契约记录缺口，维持 low。

#### W1-32

- 原发现（medium）：为没有生产订阅者的生命周期通知保留了大量重入防护，防护分支会制造半成品状态
- 复核（W1-events-reentry）：**重复 / medium**，为没有生产订阅者的通知保留重入防护，防护分支自身留下半成品状态（与 W1-23 同根，已并入）
- 位置：cpp/libs/services/scene/player/system/player_lifecycle.cpp:3089-3093, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4545-4552, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4095-4121, cpp/libs/services/scene/player/system/player_scene.cpp:259-270, cpp/tests/cross_zone_test/cross_zone_test.cpp:4352, cpp/tests/bag_test/player_feature_persistence_test.cpp:1131

根因和修法与 W1-23 相同：保留零监听或空监听的通知，再为它们写通知后复核。删掉事件以后，这些复核和它们的半成品分支会一起消失，所以判为重复。三条子论点都已在当前代码核实，并已并入 W1-23。(a) HandleExitGameNode 在 3054 挂上 UnregisterPlayer 之后，3090-3093 提前 return；DestroyDeposedPlayer 的 4547-4552 也一样。旧实体带着 Player 一直留在 registry 里，IsEmergencyRelocateDrained（3582）因此无法收敛，测试 4352 把这个泄漏当作期望结果。(b) canContinueInitialization（4097-4121）在“实体仍有效、仍在索引里但快照变了”时不擦索引就返回 false，InitPlayerFromAllData 返回 null，留下一个没有场景的实体，persistence_test:1131 断言了这个状态。原文说“之后同号登录会被 already exists 拒绝”，这句不准确：HandleEnterGameNode 在 2486 会先找到已有实体，走复用路径，不会进 InitPlayerFromAllData；实际后果是这个半成品实体会被复用。(c) HandleEnterScene 在 259 解绑之后，到 261-264 才复核，失败时玩家没有任何场景。第 5 批挑战意见 §8 的 B2(6)（batch5-lifecycle-types.md:462-463）没有落实，这一点也已写进 W1-23。生产路径走不到这些分支，medium 合适。

#### W1-33

- 原发现（medium）：每次离场都全量扫描所有 AoiListComp，批量退出路径因此变成 O(N²) 同步阻塞
- 复核（W1-aoi-rules）：**成立 / medium**，每次离场都全量扫描全节点 AoiListComp，疏散、排空、停机这几条批量退出路径因此变成 O(N²) 的主循环同步阻塞，而这次扫描只兜住了生产中没有调用方的远端 Pin
- 位置：cpp/libs/services/scene/spatial/system/aoi.cpp:235-246, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2912, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3089, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3506, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3525-3528, cpp/libs/services/scene/player/system/player_lifecycle.cpp:3552-3558, cpp/nodes/scene/main.cpp:191-197, cpp/libs/services/scene/player/system/player_scene.cpp:254

按当前代码逐项核对，没能推翻。(1) 基线 6281e4d27 的 BeforeLeaveSceneHandler 只处理离场者当前格及 6 个邻格（BroadcastEntityLeave）。全量扫描是第 5 批 B4(1) 新增的，任务单 batch5-lifecycle-types.md:278 明确要求这样写，所以这是任务单的设计缺陷，Codex 照做了。(2) 调用链是同步的：BeginEmergencyRelocateAll（3525）、BeginSceneDrain（3552）、停机 exitAllPlayers（main.cpp:191）都在一次循环里对每个玩家调用 HandleExitGameNode→DetachFromScene→BeforeLeaveSceneHandler。离场者只做 entries.clear()，不 remove AoiListComp；实体要等存盘回调才销毁，所以批量循环期间 view\<AoiListComp> 的大小不会缩小，总代价是 O(N×M)，M≈N。(3) 收益只有远端 Pin。GridId 与 hex 一一对应（grid.cpp:20-38），邻格关系对称，空间条目只会存在于互为邻格的实体之间；BroadcastEntityLeave 会对 7 格内所有观察者 RemoveAoiEntity，连 7 格内的 kPinned 也一并清掉。扫描额外能清到的只有两类：7 格之外或其他场景的 Pin，以及缺 Hex、缺场景、缺格子表时的残留。Pin/Unpin 在生产代码里没有调用方，Grep 只命中 tests。缺失分支在生产中也到不了：Hex 与 SceneGridListComp 在 UpdateGridState 里同时创建；场景只在 DestroyScene 中、且在 BeginSceneDrain 同步摘掉住户之后才销毁；被 ResetEntityVisibility 摘掉 Hex 的实体，远端只剩 Pin 条目。(4) 任务单风险项（batch5:428）要求在 PROGRESS 里写明 O(AOI 参与者) 的复杂度，PROGRESS 8302 的第 5 批条目没有写。另外，正常的单次离场和换场景（player_scene.cpp:254）也要付 O(M)。原发现的严重度和描述都正确。

#### W1-34

- 原发现（medium）：离场序列在 HandleEnterScene 和 DetachFromScene 各写一份，正是这类分叉以前造成过 Hex 泄漏
- 复核（W1-events-reentry）：**成立 / medium**，玩家离场序列（AOI 清理→摘 SceneEntityComp 与 Hex）在 HandleEnterScene 和 DetachFromScene 各写了一份，同类分叉以前造成过 Hex 泄漏
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:248-271, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2902-2928, cpp/libs/services/scene/player/system/player_lifecycle.h:1450-1452

当前代码里仍然有两份几乎相同的离场序列（grep remove\<SceneEntityComp 只命中这两处）。“AOI 必须在 Hex/SceneEntityComp 还在时读旧格子，然后二者成对摘除”是一条会一起变化的业务不变量，现在却分写在两个类里，解释它的 6 行注释只在其中一份。基线 6281e4d27 时两份已经分叉过两次：退出路径漏摘 Hex，玩家从此不可见；退出路径漏摘 ScenePlayers。DetachFromScene 的注释里有这段记录，但注释引用的“player_scene.cpp:194”已经过时。第 5 批又把两份各改了一遍（AOI 改为直接调用）。这属于 AGENTS §11.2 DRY 所说的“会共同变化的重复”，是有历史后果的潜在缺陷，定 medium。目前两份行为一致，没有现存的运行期错误，所以不升级。依赖方向：player_lifecycle.cpp 已经依赖 PlayerSceneSystem，而 player_scene.cpp 没有 include player_lifecycle.h。统一出口应放在 PlayerSceneSystem，反方向会形成循环依赖。这条应和 W1-23 一起落地，W1-23 删除 trigger 之后，统一出口只剩两句。

#### W1-35

- 原发现（medium）：句柄安全表漏写 emplace/get_or_emplace/patch/replace/erase 必须先 valid，任务单 B9 的要求只做了一半
- 复核（W1-aoi-rules）：**成立 / medium**，组件访问规则的句柄安全表漏写 emplace/emplace_or_replace/get_or_emplace/patch/replace/erase/storage.erase 必须先 valid，任务单 B9 §4.13(1) 和挑战意见都只落实了一半
- 位置：docs/design/ecs-component-access-rules.md:85-91, docs/design/ecs-component-access-rules.md:13

文档现状与原发现一致：emplace 一行只说明不会搬移；get_or_emplace、emplace_or_replace、patch、replace、erase、storage.erase 都没有出现；只有 destroy 写了先 valid。任务单 batch5:369 要求写明「emplace\*/get_or_emplace/get/patch/replace/erase 必须先 valid()」，挑战意见 batch5:467 要求把 destroy 和 storage.erase 与这些操作并列，Codex 回执只补了 destroy。按严重度标准，「任务单或挑战意见要求未落实」属于 medium。按 EnTT 3.13.2 源码核对后，需要更正原发现的一处细节：registry::emplace（registry.hpp:587）没有 ENTT_ASSERT(valid)，唯一的检查是 sparse_set::try_emplace（sparse_set.hpp:349）里的 ENTT_ASSERT(elem == null, "Slot not available")。因此当同槽位的活实体还没有该组件时，对陈旧句柄 emplace 在 Debug 下也不会报错：组件直接挂到死句柄上，view\<T> 会遍历到死实体，槽位复用后再 emplace 就会覆盖稀疏映射。get_or_emplace 和 emplace_or_replace 的 contains 按版本比较，对陈旧句柄返回 false，随后走同一条 emplace 路径（registry.hpp:637-643、900-905）。patch、replace 和 get 经 storage::patch/get→index()（storage.hpp:681-686、sparse_set.hpp:748-749），Release 下 index() 忽略版本，读写的是同槽位活实体的组件，或者越界。erase 经 to_iterator→index()，删除的是活实体的组件（sparse_set.hpp:855-858）。remove 是 contains && erase（sparse_set.hpp:885-887），对陈旧句柄安全。这处更正只会让结论更严重，不影响原发现成立。

#### W1-36

- 原发现（low）：DestroyScene 和 CreateScene 回滚都在 trigger 之后直接 destroy 场景，没有重新 valid
- 复核（W1-aoi-rules）：**降级 / low**，场景创建和销毁通知之后的复核与回滚，是为不存在的监听者写的防护，并且与规则文档的「trigger 后重验」互相矛盾；应改用监听者契约收口，删掉 CreateScene 的回滚，而不是在 4 处再补 valid
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:749-775, cpp/nodes/scene/handler/rpc/scene_handler.cpp:825-831, cpp/nodes/scene/handler/grpc/scene_node_service.cpp:56-80, cpp/nodes/scene/handler/grpc/scene_node_service.cpp:139-143, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:127-137, cpp/libs/modules/scene/scene_events.h:6-7, docs/design/ecs-component-access-rules.md:83

问题存在，但原发现对缺陷的定性和修法都需要更正。(1) 生产路径到不了：OnSceneCreated/OnSceneDestroyed 只有 scene_local_events.cpp:174-175 连接的 Agones 计数处理器，全仓 sceneRegistry.destroy 只出现在这 4 个 handler 里，监听者不可能销毁或重建场景。(2) DestroyScene 中「trigger 之后直接 destroy」在基线 6281e4d27 就存在。ECS 改动新增的是 CreateScene 的通知后 try_get 复核加回滚分支（两个 handler 各一份；原发现漏了 scene_node_service.cpp:71-77，实际是 4 处，不是 3 处）。它原本要解决 F096，即 sceneInfo 引用跨 trigger 使用；最直接的解法是在 trigger 之前把回包拷好，不需要复核和回滚。回滚分支本身只针对「监听者删掉 SceneInfoComp」这种假想情况，注释里说监听者包括「AOI 格子表」也不对：SceneGridListComp 在 UpdateGridState 里按需创建，不是监听者。(3) 原修法是 trigger 之后再补 valid，并为 SceneRegistry 新增 DestroyEntity 重载。这属于再加一层防护，与负责人「dispatcher 只做可有可无的通知、以监听者契约代替通知后复核」的方向相反，也与 W1-32 给出的契约修法不一致。真正的毛病是规则文档第 83 行把「监听者可销毁实体」当成合法前提，迫使代码到处复核。因此保持 low，改定为过度防护与规则矛盾，修法改为删除代码加写明契约。

#### W1-37

- 原发现（low）：AoiSystem::PinAoiEntity 只是透传，InterestSystem::PinAoiEntity 仍是公开接口，Pin/Unpin 实际没有成对收口
- 复核（W1-aoi-rules）：**成立 / low**，Pin 有 AoiSystem::PinAoiEntity（纯透传）和公开的 InterestSystem::PinAoiEntity 两个入口，10 处测试与三份设计文档仍指向 InterestSystem，Pin/Unpin 成对只靠注释约束
- 位置：cpp/libs/services/scene/spatial/system/aoi.cpp:249-251, cpp/libs/services/scene/spatial/system/aoi.h:16-19, cpp/libs/services/scene/spatial/system/interest.h:20-22, cpp/libs/services/scene/spatial/system/interest.cpp:150-153, cpp/tests/aoi_test/interest_system_mock.cpp:94-97, cpp/tests/aoi_test/aoi_system_test.cpp:440, cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:107, docs/design/aoi_priority_design.md:100-108

需要先更正原发现的一处说法：挑战意见 batch5:470-471 给了两个选项，「把 Pin 也移到 AoiSystem」或「加一个对应的 AoiSystem::PinAoiEntity 包装」，Codex 选了后者，字面上没有违背挑战意见。不过问题本身属实：包装层不做任何隔离或转换；InterestSystem::PinAoiEntity 仍然公开，测试里有 10 处直接调用它（aoi_system_test.cpp:440/441/483/592/663/711/1130，aoi_delivery_test.cpp:107/158/172），不参与编译的 mock（interest_system_mock.cpp:94）里还有一份同名实现。三份 aoi_priority_design\*.md 仍写着 InterestSystem::PinAoiEntity/UnpinAoiEntity，后者已经删除，「降级为 kNormal」的语义也已过时；任务单 B4(5) 要求更新这三份文档，没有落实。同一能力有两个入口，文档还把读者指向不存在的 API，可读性受损。生产中没有调用方，所以没有运行期后果，保持 low。

#### W1-38

- 原发现（low）：多处注释在第 5 批改造后已与实现不符
- 复核（W1-b5-docs）：**降级 / low**，第 5 批后有 4 处注释或文档与实现不符(skill.cpp、player_lifecycle.cpp、anomaly_detector.h、ecs-fit-overview.md);gate/main.cpp 那处并没有过时
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:64,cpp/libs/services/scene/player/system/player_lifecycle.cpp:4543,cpp/libs/modules/transaction_log/anomaly_detector.h:80,docs/handoff/ecs-refactor/ecs-fit-overview.md:121

逐条核对当前代码,共 5 个子项,其中 1 项被推翻,4 项成立。

(1) 推翻 gate/main.cpp:148:旧成员 aliveToken 已删,但现在 TimerTaskComp 用 shared_ptr\<State> 持有状态,闭包只捕获 weak_ptr\<State>,开火时 lock 失败就返回,析构时还会 Cancel。这本身就是 AGENTS §11.7 所说的弱持存活令牌。注释说的是机制,不是旧成员名,与实现一致。原发现把它列为首要位置,不对。

(2) 成立 skill.cpp:64:目标侧镜像已删除,StoreSkillContextForWrite 全仓只有 AddSkillContext 一个调用方,只传 caster。「施法者与目标共用」已不成立。

(3) 成立 player_lifecycle.cpp:4543-4545:DetachFromScene 现在直接调用 AoiSystem::BeforeLeaveSceneHandler。该函数以及它调用的 RemoveEntityFromGrid、BroadcastEntityLeave、NotifyEntityVisibilityChanges 都不读 PlayerFrozenComp。随后 dispatcher.trigger(BeforeLeaveScene) 在生产中没有任何监听者(git grep 找不到 sink\<BeforeLeaveScene>),RemovePlayerSession 和 DestroyPlayer 也不读这两个组件。「让事件看到普通离场实体、绕开 Frozen 闸」这条理由已不存在。

(4) 成立 anomaly_detector.h:80:窗口改成玩家组件后会随实体销毁;进程重启时内存本来就会清空。ClearPlayer、ClearAll、GetCurrencyGainCount 在生产代码中都没有调用方,只有测试在用。「player logout 时清理」既不需要,也没人调用,注释会误导读者在登出路径里补一次多余的调用。

(5) 成立 ecs-fit-overview.md:121:A2 之后 timer_task_comp.h:20 已写明 in_place_delete「移动正确性已不依赖此标记」;B8 之后 BuffListComp、BuffPendingRemoveBuffs、SkillContextCompMap、SceneGridListComp 都继承了 std 容器,且基类转换是 explicit。权威规范 ecs-component-access-rules.md:93、95 已经更新,只有这份任务单还停在第 1 版。它是交接文档,文首说明与 README 冲突时以 README 为准,影响有限。

这些都只是注释或文档与实现不符,不影响运行期,low 合适。按「优先删除」,skill.cpp 那处与其改注释,不如删掉只剩一个调用方的透传 helper;anomaly 那处删掉无调用方的 ClearPlayer。

#### W1-39

- 原发现（low）：挑战意见点名的 3 份设计文档仍在描述已删除的 ScenePlayers
- 复核（W1-b5-docs）：**成立 / low**，3 份设计文档仍描述已删除的 ScenePlayers 反向集合
- 位置：docs/design/scene-creation-architecture.md:344,docs/design/agones-scene-node-high-density.md:23,docs/design/async-load-disconnect-reconnect-race.md:45

cpp/ 下已经完全没有 ScenePlayers(B2 删除了 scene_node_comp.h 和全部 emplace)。BeginSceneDrain 现在扫描 view\<SceneEntityComp, PlayerGuidComp>,场景成员的事实源只剩 actor 上的 SceneEntityComp,权威规范 ecs-component-access-rules.md:83 也已写明。batch5-lifecycle-types.md:483「遗漏的调用点」点名的正是这三处,却没有同步修改。其余命中都是历史记录,不需要改:ecs-entt-misuse-audit-20261015.md 是审计条目原文,PROGRESS.md 是只追加的流水账。三处文档会让读者以为创建场景时还要挂反向集合、进场时还要往里加人,与当前实现矛盾,违反 AGENTS §5「没写文档 = 没说过」的维护要求。只是文档问题,不影响运行期,low 合适。

#### W1-40

- 原发现（high）：新增的 creators 准入只在 C++ 节点末端生效：「按场景号加入镜像」、组队跟进镜像、镜像去重复用都会被拒，且与 Java 版对账口径相反
- 复核（W1-40-flows）：**成立 / high**，Codex 把废弃 scene_test 里的 creators 断言做成了生产准入，而且只在 C++ 节点末端生效。非创建者按号加入镜像、组队跟进镜像、开启去重后复用镜像这三条合法流程都会被拒，scene_manager 已提交的 location 和人数随之与节点状态不一致，C++ 和 Java 两版对客户端的行为也分叉了
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:196,cpp/libs/services/scene/player/system/player_scene.h:42,cpp/libs/services/scene/player/system/player_lifecycle.cpp:2484,cpp/libs/services/scene/player/system/player_lifecycle.cpp:2636,cpp/libs/services/scene/player/system/player_lifecycle.cpp:2725,cpp/libs/services/scene/player/system/player_lifecycle.cpp:4079,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:36,cpp/nodes/scene/handler/rpc/scene_handler.cpp:166,cpp/tests/cross_zone_test/cross_zone_test.cpp:5482,cpp/tests/bag_test/player_feature_persistence_test.cpp:80

逐条核对的结论：三条合法流程在当前代码里都可达，也都会被拒。

(a) 按号加入已有镜像。客户端 EnterSceneC2S 只要 scene_id>0 就会走 player_scene_handler.cpp:96-162，最后进 RequestSceneChange。注释 :72-76 写明「joining an existing mirror by id」是支持的流程。Go 侧 resolveScene(enterscenelogic.go:1204-1276) 对 scene_id!=0 的请求直接按 scene:{id}:node 查，不区分镜像，也不知道 creators。client_channel_pick 只对大世界分线生效(:448)。Java 仓 PARITY.md 第 128 行的两版同基线口径写的是「creators 不做准入，任何人可按号加入(同节点同步换，别的节点走 5.2 交出)」。Java game-robot 的 CrossNodeScenario.java:54/:537 和 RobotOptions.java:503 都把「C/B 按号加入 A 的镜像，creators 仍是 {A:true}」当作必须成功的客户端契约来测。

(b) 组队跟进镜像。创建者自动进场后会走 player_lifecycle.cpp:2779 的 PlayerTeamSystem::OnEnteredScene，接着 kFollowLeaderAndFanout(player_team.cpp:342-354)，再对本节点每个队员调 CheckFollowLeader。队长的 location 节点就是本节点(:444)，于是以 scene_id=镜像号 发 EnterScene(:490-506)。路由回到本节点后，HandleEnterGameNode 的预检对 creators={队长} 判 3022，并给队员推一条他根本没请求过的 tip。PARITY 里「组队同节点跟进镜像」也是基线行为。

(c) 镜像去重复用。MirrorDedupBySource 默认 false，但它是正式配置项(scene_manager_service.yaml:122)。打开后，去重分支回显的是新调用者的 CreatorIds(createscenelogic.go:208-212)，logic_test.go:3092-3150 的 TestCreateScene_EchoesCreatorIds 明确要求「so the C++ caller drives EnterScene for them」。C++ 端那个场景的 creators 仍只有首个创建者，所以自动进场(scene_manager_response_handler.cpp:115-180)每次都会被拒。这条只在开关打开时成立，单看属于 medium，并入本条。

状态不一致也核实了：scene_manager 在发路由前已经提交了人数(enterscenelogic.go:679 AtomicIncrPlayerCountIfSceneExists)和 location(:734 placePlayerLocation，跨节点时还会铸 epoch)。C++ 拒绝后只发 tip(scene_handler.cpp:166-169 直接 return)，不回告 scene_manager。同节点时的后果：实体仍在源场景，Redis location 却指向镜像；源场景人数少算 1，镜像人数虚高 1，镜像因此不会被空闲回收(instance_lifecycle.go:107)，要等玩家下次换图才恢复。跨节点按号加入时更严重：源节点在交接成功应答后已经释放实体，目标节点预检直接拒绝，玩家连着 gate 却在哪个节点上都没有实体，只能重登。没有资产损失，因为交接前已存盘。

根因：基线 6281e4d27 的生产代码里 creators 只写不读。旧的 SceneCommon::CheckPlayerEnterScene 早在 centre 删除时就没了，scene_test 不在 game.sln 里，是死工程。负责人给的任务只是「迁移旧 scene_test 的有效权限断言」，Codex 却为了让这条失效断言成立而新增了生产准入(README §8 场景权限一行、batch5 §第 526 行回执)。PARITY 里 B5「删掉过时单测或接上 creators 准入(3022)」原本是挂起待定的可选项，等于 Codex 替负责人做了决定，Java 也没有同批改。这违反 AGENTS §12 和 §11.3 的向后兼容要求。

对原发现的更正：行号已更新；严重度维持 high。跨节点那条接近 blocker，但客户端会收到 3022，不算静默丢弃，也没有存档损坏，所以不升级。原修法 B 里「C++ 末端检查作为纵深防御保留」不对：去重合并 creators 后，C++ 端那份会过期；末端拒绝又无法回告 scene_manager，留着就是在复制本条的不一致，属于两份真相(DRY)。即使选 B，也应删掉 C++ 端检查。

- 复核（W1-40-parity）：**成立 / high**，ECS 收尾在 C++ 场景节点末端新增了 creators 准入（3022），把基线和 Java 版都放行的三类合法流程改成拒绝：非创建者按号加入镜像、组队跟进队长所在镜像、镜像去重复用。而且拒绝发生在 scene_manager 已经提交 location、人数和 epoch 之后，两版行为分叉，PARITY 也没有登记
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:191-199, cpp/libs/services/scene/player/system/player_scene.h:42-46, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2486-2494, cpp/libs/services/scene/player/system/player_lifecycle.cpp:2634-2640, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4077-4083, cpp/libs/services/scene/player/system/player_team.cpp:490-506, cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp:72-82, go/scene_manager/internal/logic/enterscenelogic.go:679-760, go/scene_manager/internal/logic/createscenelogic.go:202-212, cpp/tests/cross_zone_test/cross_zone_test.cpp:5418-5551, cpp/tests/bag_test/player_feature_persistence_test.cpp:79-90, cpp/tests/bag_test/player_feature_persistence_test.cpp:1037-1060

我按推翻的思路逐项核对，没有推翻。

(0) 是不是新增：基线 6281e4d27 的生产代码只在 scene_node_service.cpp 和 scene_handler.cpp 里写 creators，没有任何读取。唯一读取方是 cpp/tests/scene_test/scene_test.cpp:925-943，它引用早已不存在的 SceneCommon，不在 game.sln 里，是一份死单测。ecs-fit-overview.md:270 对它的任务只是「删除整个 cpp/tests/scene_test 目录」。Codex 在第 5 批 §11 和 README §8 里写的是「迁移有效权限断言」，实际做法是把死单测的断言变成了生产准入。110 条审计 F001–F100/C001–C010 里没有任何一条要求这项准入，所以这是超出范围、客户端可见的行为变更。

(1) 所有入场都会走到这道检查：gate 的 scene_entry_dispatch.cpp 只发 PlayerEnterGameNode，handler 是 scene_handler.cpp:166 → HandleEnterGameNode。它的 2486-2494 行对已有实体和新载入两种情况都调 CheckEnterScene，拒绝时 SendTipToPendingSession(3022) 并返回 kRejected。之后的 EnterScene（2636）和 InitPlayerFromAllData（4079）又各查一次。s2s SceneScenePlayerHandler::EnterScene 在仓内找不到发送方，不影响结论。

(2) 三条合法流程都能走到：(a) 客户端 63 带 scene_id>0：player_scene_handler.cpp:72-76 明文写着这是「joining an existing mirror by id」，它不做任何 creators 预检，直接经 RequestSceneChange 发给 scene_manager。同节点时，路由回本节点后被 3022 拒掉。跨节点时，源节点按 18 起交接，存盘后经 ResolveTravelOutcome 销毁源实体；目标节点 HandleEnterGameNode 在 existingPlayer==null 分支用 CheckEnterScene(playerId) 拒绝，不发 AsyncLoad，结果玩家在所有节点上都没有实体。(b) PlayerTeamSystem::CheckFollowLeader（player_team.cpp:490-506）用队长 location 的 scene_id（可以是镜像）代发 EnterScene，playerRequested=false。HandleSceneChangeEnterSceneReply 的约定是跟随被拒「只记日志、不回 tip」，但现在拒绝发生在节点入口，SendTipToPendingSession 会把 3022 推给队员，这和跟随的约定（team-system DV-6）也冲突。(c) createscenelogic.go:202-212 去重命中时不调 C++ CreateScene，直接回显新调用者的 CreatorIds。scene_manager_response_handler.cpp:115-150 随后替新创建者代发 EnterScene，而节点上那个场景的 creators 只有首个创建者，必然被拒。MirrorDedupBySource 默认是 false（scene_manager_service.yaml:122），所以这一条目前是潜伏缺陷。

(3) Redis 与节点状态不一致已经确认：enterscenelogic.go 先 AtomicIncrPlayerCountIfSceneExists 给目标场景加人数（679），再 placePlayerLocation 写 location，必要时铸 epoch（734），再给旧场景减人数（757-759），最后推 RoutePlayer（763）。节点到最后才拒绝时，C++ 侧没有任何补偿：RefuseInheritedEntry 只擦会话、推 tip。于是 location 指向镜像，镜像多算 1 人，旧场景少算 1 人，而同节点的玩家实际还在旧场景。镜像带着这个幽灵人数，空闲回收用的「人数为 0 才删」条件不成立。跨节点时 location 记的是 E+1 epoch 加目标节点，目标节点从未持有这个玩家，也就不会为 E+1 写交接标记。此后重登如果选线落到别的节点，可能被 18 换手门挡住（这是按代码推断，未运行）。

(4) 两版已经分叉（§12）：Java 的 SceneWorld.resolveSwitchTarget 对 scene_id≠0 只看场景存在和是否 draining；Scene.java:176 注明「不做准入，同基线」。MirrorSceneTest.镜像里有旁人时_换入者收到含旁人的47 和 TeamFollowServiceTest.队长进镜像_本节点队员跟进同一个镜像 这两条正好钉住了非创建者加入和跟进镜像。Java 的 dungeon-mirror-spec.md:57/472/1055/1067 把 3022 记为「两版运行期都不产生（死码）」，N7 creators 准入写的是「需两版同改」。C++ 的 PROGRESS:8339 和 Java 的 PARITY:182 却都写着「客户端契约未变」，PARITY:128 仍是「不做准入」，这次变更完全没有登记。另外 3022 在 Tip.xlsx 里没有文案：generated/tables/tip_text.json 里没有 3022 这个键，客户端拿到的提示是空文案。

对原修法的更正：① 原修法只点了 cross_zone_test。bag_test/player_feature_persistence_test.cpp 也有两处靠 creators 制造拒绝（RevokeEntryDuringInitialization:79-90，以及 InitRejectsMissingOrRestrictedDestination... 的 restricted 分支 1037-1060），需要改写，不能只删。② 方向 B 里「C++ 节点末端检查只作为纵深防御保留」的说法不对：只要节点和 scene_manager 的判定有分歧（例如去重合并不同步），节点末端拒绝就会原样复现 location、人数和 epoch 不一致。准入只能有 scene_manager 这一个决策点，节点末端最多打日志。③ 去重命中的路径根本不调 C++ CreateScene，所谓「经 C++ CreateScene 幂等分支合并 creator_ids」需要新增一次调用，或者干脆让 creators 只放在 Redis。

#### W1-41

- 原发现（medium）：停机时同一调用栈内先发 LeaseRevoke，紧接着就 TryCancel 所有在途调用，优雅注销基本退化成等租约过期
- 复核（W1-grpc-shutdown）：**已被改写 / none**，原问题（F010 在 LeaseRevoke 后立即 TryCancel 全部在途调用）已随 CQ 和 GrpcEtcdTransport 一起删除；现在 MuduoEtcdTransport 对 LeaseRevoke 只做「尽量送达」，这是迁移设计 §13.7 记录在案的取舍，与 ECS 改动无关
- 位置：cpp/libs/engine/core/node/system/node/node.cpp:1191-1196,cpp/libs/engine/core/node/system/node/node.cpp:1350-1353,cpp/libs/engine/core/node/system/etcd/etcd_service.cpp:607-629,cpp/libs/engine/core/node/system/etcd/muduo_etcd_transport.cpp:178-216,cpp/libs/engine/core/node/system/etcd/muduo_etcd_transport.cpp:243-268,docs/design/grpc-muduo-transport.md:411-417

原发现依赖三样东西：node.cpp 停机循环里的 BeginShutdownGrpcNode、grpc_init_total 模板里对 runtime->calls 的 TryCancel、默认的 GrpcEtcdTransport。在当前工作区里这三样都已删除：node.cpp 中找不到 BeginShutdownGrpcNode；grpc_init_total.cpp.tmpl 只剩处理器 setter 和 SetGrpcCallDeadline；grpc-muduo-transport.md:500-501 写明 InitGrpcNode、BeginShutdownGrpcNode、PollGrpcNodeShutdown、GrpcClientRuntime、GrpcEtcdTransport 一并删除。所以 ECS 第 1 批 F010 引入的「本地 TryCancel 刚发出的 revoke」这一退化已不存在。

现在的流程：ReleaseNodeId 走 EtcdHelper::RevokeLeaseAndCleanup，调到 MuduoEtcdTransport::LeaseRevoke，再到 Core::Unary 的 channel_->StartUnaryCall；同一个栈里紧接着 EtcdService::Shutdown 调 Core::Shutdown，执行 channel_.reset()。~Channel 对在途调用只做本地 UNAVAILABLE 排队，不发 RST_STREAM；Http2Transport::Shutdown 调 forceClose，丢掉的只是 muduo 用户态输出缓冲。如果提交时通道是 READY、输出缓冲为空，字节已经同步写进内核，fd 在 ~TcpConnection 时关闭，FIN 排在数据之后。服务端在 EOF 时取消 handler ctx，只要在 Propose 入队之前取消，这次 revoke 就会丢失。不过基线里进程退出同样会关 socket，属于同一类风险。剩余的「不等应答、不排空」是迁移会话在 §13.7 写明的有意设计，不是 ECS 改动造成的，按任务口径不再作为 ECS 问题报告。原修法要求的「Etcd 实体延迟 BeginShutdownGrpcNode」已经没有可以改的对象。

#### W1-42

- 原发现（medium）：gRPC 客户端寿命状态继续塞进节点 registry，又新增 GrpcClientRuntimePtr 组件，靠 in_place_delete 和每次回调后重取组件来维持正确性，难读难维护
- 复核（W1-grpc-shutdown）：**已被改写 / none**，GrpcClientRuntimePtr、CQ 组件、in_place_delete 流式调用组件和逐回调 try_get 都已删除；节点实体上只剩一个 MuduoGrpcChannelPtr，调用状态随回调走，不进 ECS
- 位置：cpp/libs/engine/core/network/grpc_client_runtime.h:1-36,tools/proto_generator/protogen/internal/template/grpc_async_client.cpp.tmpl:59-150,cpp/libs/engine/core/node/system/node/muduo_grpc_channel_slot.h:20,docs/design/ecs-component-access-rules.md:95

原发现批评的几样东西都已不存在：registry.emplace\<GrpcClientRuntimePtr> 和 emplace\<grpc::CompletionQueue>、流式调用对象作为 in_place_delete 组件、ReadBorrow 析构时重取组件、轮询时逐组件 try_get。在 cpp/libs、cpp/nodes 和模板目录里搜 GrpcClientRuntimePtr 和 gRPC 相关的 in_place_delete 都没有结果；剩下的 in_place_delete 只在 timer_task_comp.h 和 skill_comp.h，与 gRPC 无关。grpc_init_total.cpp.tmpl 只剩处理器 setter；流式方法不再生成客户端（etcd 由 MuduoEtcdTransport 手写）。

现在每次调用的状态（请求副本、metadata、done）放在 shared_ptr 里随完成回调走；本地完成计数是 thread_local，不在 registry 里。节点实体上只剩一个 MuduoGrpcChannelPtr，正好是原修法「节点实体上最多挂一个组件」的形态，也和存量的 RpcClientPtr 同构，组件访问规则第 95 行把它列为存量边界。Send 只做一次 FindGrpcChannel，之后不再碰 registry。

仍残留的几处都属于迁移未完成，不在本次审查范围：client_message_processor.cpp:225 还在判断 grpc::CompletionQueue 和 ClientRpcRouterStubPtr；grpc_client_lifecycle_test.cpp 还在用 GrpcClientRuntimePtr、BeginShutdownGrpcNode 等已删除的接口。另外，ecs-component-access-rules.md:95 里「官方 gRPC Channel/CQ/Stub 等已有组件」「ECS 收尾已补取消、稳定缓冲与非阻塞排空源码」两句描述的代码已经不存在，属于文档过时，应在迁移收尾时一并清理。

#### W1-43

- 原发现（low）：C008 用例里的「swap-and-pop」步骤在修复前后都不可能触发移动，测不出回归
- 复核（W1-grpc-misc）：**已被改写 / none**，被测对象(注册表里的流式调用组件及其 reply 缓冲)已随 muduo 迁移删除,swap-and-pop 步骤和整组流测试一起作废
- 位置：cpp/tests/rpc_controller_test/grpc_client_lifecycle_test.cpp:314-436, cpp/generated/grpc_client/etcd/etcd_grpc_client.h:111-116, cpp/libs/engine/core/network/grpc_client_runtime.h:5-6, cpp/libs/engine/core/node/system/etcd/muduo_etcd_transport.h:21-46

原结论在旧代码上成立:other 建在节点实体之后,destroy 时只弹池尾,不会搬动节点实体的组件;修复后 reply 已在调用对象里,这几行实际什么也没测。但现在被测对象已经没了。流式客户端不再生成(etcd_grpc_client.h:111-116 只留拒绝用的签名),etcd 的 Watch/LeaseKeepAlive 改由 MuduoEtcdTransport 手写,状态放在 shared_ptr\<Core> 里,不进 registry。AsyncWatchWatchGrpcClient、AsyncWatchWatchHandler、GrpcClientRuntimePtr、BeginShutdownGrpcNode、PollGrpcNodeShutdown 在生产代码和模板里都搜不到,只剩这个测试文件、cross_zone_test.cpp 和 grpc_process_test.go 还在引用。所以「同类型组件 swap-and-pop 挪动读缓冲」这个风险已经不存在,原问题也没有以别的形式留下来。测试文件仍是旧文本、引用了已删除的类型,这属于迁移没做完(编不过),按约束不另行报告。

#### W1-44

- 原发现（low）：DataService 选点没有跳过关闭中的节点，与 README「选点跳过无传输及关闭中的节点」不一致
- 复核（W1-grpc-misc）：**已被改写 / none**，DataService 选点改为 view\<NodeInfo, MuduoGrpcChannelPtr> 并要求 kReady;摘节点先同步移除通道组件,不再有「关闭中」窗口
- 位置：cpp/nodes/scene/id_segment_bootstrap.cpp:92-103, cpp/libs/engine/core/node/system/node/node.cpp:1540-1551, cpp/libs/engine/core/node/system/node/node.cpp:1208-1215

原发现依赖旧机制:BeginShutdownGrpcNode 置 closing=true 之后,CQ 还要排空几轮,这期间 std::shared_ptr\<grpc::Channel> 仍可能是 READY。迁移把这套机制删了。现在 ExecuteNodeRemoval 在同一个 queueInLoop 闭包里依次做三件事:remove\<MuduoGrpcChannelPtr>、trigger、DestroyEntity。关机时 FinalizeShutdownInLoop 阶段 0 直接 clear\<MuduoGrpcChannelPtr>。BeginShutdownGrpcNode/PollGrpcNodeShutdown 在生产代码里已不存在,也就没有「挂着通道、但已在关闭」的中间态。PickReadyDataServiceNode 的视图要求 MuduoGrpcChannelPtr 存在,还要求 GetState==kReady,比 HasUsableGrpcClient 的 !=kShutdown 更严,正在摘除或已关闭的节点都选不中。这个选点和 player_inbox_system.cpp:68 写法一致,不需要再加 HasUsableGrpcClient。问题不以任何形式残留。只剩 docs/handoff/ecs-refactor/README.md:149 的「选点跳过无传输及关闭中的节点…CQ/stub…处理全部 tag」描述的是旧 CQ 机制,文字过时。

#### W1-45

- 原发现（low）：F010 修复后，路由服摘除时在途调用会以 CANCELLED 回调，gate lane 的注释和计数口径已过时
- 复核（W1-grpc-misc）：**已被改写 / low**，路由服摘除时 ~Channel 把在途调用以 UNAVAILABLE 回调,OnRouterNodeRemoved 已经多余;lane 注释仍写「永远不会再有回调」,lateDropped 会把每个被摘调用再计一次
- 位置：cpp/nodes/gate/handler/rpc/client_request_lane.h:47-48, cpp/nodes/gate/handler/rpc/client_request_lane.h:87-90, cpp/nodes/gate/handler/rpc/client_request_lane.cpp:187-195, cpp/nodes/gate/handler/rpc/client_request_lane.cpp:322-349, cpp/nodes/gate/handler/rpc/client_request_lane.cpp:357, cpp/nodes/gate/handler/rpc/client_message_processor.cpp:1240-1245, docs/design/client-request-ordering.md:212, docs/design/client-request-ordering.md:230

原发现写的机制已经过时:BeginShutdownGrpcNode 取消、在途调用以 CANCELLED 回调、node.cpp:1601。但同一个问题以新形式仍然存在。

现在 MuduoGrpcChannelPtr 只由组件强持有:router_muduo_forward.cpp 只借引用,FindGrpcChannel 的拷贝是临时的,通道内部回调只绑 weak_from_this。所以 ExecuteNodeRemoval 的 remove\<MuduoGrpcChannelPtr> 会立即析构通道,~Channel 把每个在途 Forward 调用的 done 以 UNAVAILABLE 排进下一轮。

摘除那一轮的顺序:先排入 UNAVAILABLE 失败闭包;trigger 时 OnRouterNodeRemoved 同步摘出在途项,再把 finish 排在这些失败闭包之后;最后 DestroyEntity。

下一轮:失败闭包经 NotifyCallFailure 进入 OnRouterCallFailed,Claim 取不到条目,lateDropped 加一后返回 true;随后 finish 回 1003。客户端回执仍然恰好一次,行为正确。但有两处不对:

(1) lane.h:47-48、87,lane.cpp:357,client_message_processor.cpp:1240-1241,以及设计文档 client-request-ordering.md:212、230 都写着「在途调用永远不会再有回调 / CQ 轮询 / F29」,和现状相反。

(2) 每次路由服摘除,lateDropped 会按在途数多计一次。Claim 注释把「已因节点摘除处理」列为 lateDropped 的来源,所以不算违反定义,但运维看不出这是正常摘除还是真有迟到完成。

更关键的是 OnRouterNodeRemoved 的存在前提「摘除后没有回调」已经不成立。~Channel 的 UNAVAILABLE 走 OnRouterCallFailed 本来就会 Claim、FinishWithFailure、回 1003、Pump。失败闭包在 DestroyEntity 之后的下一轮才执行,而且 PickRandomNode(client_message_processor.cpp:112-113)要求实体挂着通道,所以 Pump 不会再选中被摘节点。OnRouterNodeRemoved 只是复制了这条路径,这个必需步骤还是挂在 dispatcher 上的。只改注释、单独计数属于「保留多余的东西再加防护」。按偏好应删除。严重度维持 low:文档、注释和指标口径有问题,没有业务错误。

#### W1-46

- 原发现（low）：gRPC 排空轮询没有时间预算，也没有长时间未排空的日志
- 复核（W1-grpc-shutdown）：**已被改写 / none**，停机和摘节点的 5ms gRPC 排空轮询已删除；剩下的 FinalizeTeardownInLoop 本地完成让轮靠 FIFO 有界收敛，当前没有任何回调会同步重发，不构成可达缺陷
- 位置：cpp/libs/engine/core/node/system/node/node.cpp:1222-1241,cpp/libs/engine/core/node/system/node/node.cpp:1499-1556,cpp/libs/engine/core/network/grpc_client_runtime.h:38-77

原发现指向两处每 5ms 无限重排的轮询，现在都已删除：一处是 DrainGrpcClientsInLoop / PollGrpcClientsInLoop 配合 grpcClientDrainTimer_，另一处是 CompleteNodeRemoval。node.cpp 中已找不到这些符号，设计 §14.1 第 500-503 行也写明删除。摘节点改为先 remove\<MuduoGrpcChannelPtr>、再 trigger、再直接 DestroyEntity。停机时阶段 0 执行 clear\<MuduoGrpcChannelPtr>，~Channel 把所有在途调用立即以 UNAVAILABLE 排队，所以原发现的第二个触发条件「取消后迟迟不完成」已无从发生。

剩下唯一的等待是 FinalizeTeardownInLoop 在 localCompletions 不为 0 时让一轮。由于 queueInLoop 是 FIFO，让轮之前排进去的本地完成都会先执行完，只有当这些完成回调又同步发起一次本地失败的 Send 时，才会再让一轮。我逐个核对了已安装的失败处理：

- 号段：OnFailure 调 OnTransportFailure，再走 OnFetchFailed 和 ScheduleRetry，用退避定时器重试（guid_segment_client.cpp:330-341），不同步重发。

- 收件箱：PlayerInboxSystem::OnFetchFailed / OnAckFailed。

- scene_manager：EnterScene 走 DispatchEnterSceneTransportFailure；CreateScene 只给创建者发 tip。

- gate lane：FinishWithFailure 后 Pump 到 SendRawViaRouter，阶段 0 之后已经没有通道，返回 nullopt，不会 Send。

这些路径都不会同步重发，链长有界，让轮会在有限轮内结束，不存在「无声无限轮询」的可达场景。为这种理论上的回环再加预算和日志，正是负责人反对的「加防护兜住」，不应加。

#### W2-b1_runtime-01

- 原发现（high）：落点时目标已死被当成「缺数据」中止整次施法:效果和后摇都跳过
- 复核（b1_runtime-g2）：**降级 / medium**，落点时目标已死,被 CalculateSkillDamage 的 false 当成数据缺失,整次施法被中止。对尸体施法、或目标在前摇期间被打死时,会跳过效果和后摇;这与改造前、与致命一击路径、与 Java 版都不一致
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:790-843,cpp/libs/services/scene/combat/skill/system/skill.cpp:926-929,cpp/libs/services/scene/combat/skill/system/skill.cpp:873-878,cpp/libs/services/scene/combat/skill/system/skill.cpp:99-108

机制属实。CalculateSkillDamage 有 5 条 false 分支,在 926 处都能被排除,只剩目标已死:

- valid(802)和 PlayerFrozenComp(813)已由 901 的 ValidateSkillTargetEntity 排除;

- 上下文和施法者 LevelComp 已由 892 的 CanContinueSkillCast 保证;

- 技能表行来自已校验的请求。

所以 926 的 Abort 实际只会被「目标已死」触发,bool 原本表示「不变量被破坏」,这层语义被污染了。中止的后果:AbortSkillCast 删掉上下文,376 返回;TriggerSkillEffect、OnSkillExecuted(目前是空 TODO,见 W1-27)、HandleSkillRecovery 都不执行。基线中 CalculateSkillDamage 是 void,死目标零伤害,整条链照常走完。batch1-codex-receipt.md:19 写的「缺失按技能实例收尾」也只针对缺组件。

需要更正两点:

(1) 主要入口不只是「前摇期间被打死」,更常见的是直接对尸体施法。实时场景没有死亡状态,也不移除 0 血实体(复活只发生在加载和回合战结算);ValidateTarget / ValidateSkillTargetEntity 都不看血量;robot_ai 从可见实体里随机选目标。

(2) 实际后果有限,定为 medium 而非 high。现表只有 1 号技能的 recovery_time>0(1 秒),跳过后摇的效果就是:施法者对尸体放 1 号技能后,可以立刻接 2/13 号非立即技能,本该返回 kSkillUnInterruptible。被跳过的 buff 本来就落在尸体上,没有资产或存档损坏。

两版也已分叉:Java 版 SkillService.settle 不论目标状态,都会走 CASTING→RECOVERY。原修法第 1 步要在 HandleSkillSpell 新加一个 IsTargetDead 分支,其实多余:DealDamage:876 已经让死目标零伤害,删掉 CalculateSkillDamage 的三段即可。

- 独立二审：**降级 / medium**

这条发现成立，生产路径能走到，但后果有限，从 high 降为 medium。

1) 语义确实被混在一起。CalculateSkillDamage 只有一个调用方，就是 skill.cpp:926。走到这里之前，它的几个 false 分支都已被前面的检查排除：

- 892 行 CanContinueSkillCast → CheckRequiredSkillComps(115-133) 已保证施法者有 LevelComp 和上下文；

- 901 行 ValidateSkillTargetEntity(99-108) 已排除目标无效和冻结；

- 901 到 926 之间没有回调，状态不会变。

因此 802-805(valid)、813-819(PlayerFrozenComp)、828-834(LevelComp) 在唯一调用点上都是死代码。926 实际能走到的 false 只剩 821-824「目标已死」这一个业务分支(另有查表失败)。828 行注释写的是「缺了是不变量被破坏,返回失败让整条落点链路停止」，可见作者把 false 定义为不变量失败，死目标却也走这条返回路径。

2) 可达性。

- ValidateTarget(507-536) 不拒绝死目标，ReleaseSkill 可以直接对尸体施法；

- HandleTargetDeath(862-871) 只调 buff 钩子，不销毁实体也不复活。玩家要等重新加载或战斗结算(player_battle.cpp:1791)才回血；

- 所以尸体一直可选为目标。直接对尸体放普通或引导技能，或者前摇期间目标被别人打死，都会走到 926 → AbortSkillCast(144) → HandleSkillFinish 删上下文。

- 之后 HandleGeneralSkillSpell:376 的 CanContinueSkillCast 找不到上下文，返回 false。TriggerSkillEffect 和 HandleSkillRecovery 都不执行，OnSkillExecuted(932-935) 也跳过；

- 引导技能在 HandleChannelSkillSpell:452 同样提前结束，不挂引导定时器，也没有后摇。

- 基线 6281e4d27 里 CalculateSkillDamage 是 void：死目标只是零伤害，DealDamage:876 自己早退，后续效果和后摇照常执行。所以这是 Codex 改动引入的回归。致命一击(落点时目标还活着)会正常进入后摇，与死目标分支结果不一致。

3) 为什么降级。

- 不崩溃，不损坏资产或存档；

- OnSkillExecuted 目前是空 TODO(buff.cpp:588)；

- 跳过的效果是给尸体加 buff，本身就没有意义，eBuffType 里也没有复活类 buff；

- 唯一有实际意义的后果是这次施法本来就打空了，施法者却免掉了后摇，CheckRecovery 拦不住下一个非立即技能，节奏规则不一致。这一点收益很小，构不成实质性利用。

按「潜在缺陷、违规且有后果」定为 medium 更贴切。另外 skill_test.cpp 里没有死目标用例，修复要补回归测试。

#### W2-b1_runtime-02

- 原发现（medium）：没有前摇相位的技能每次施放都留下一条永不回收的 SkillContext
- 复核（b1_runtime-g2）：**降级 / low**，没有前摇相位的技能类型(开关/激活/普攻/被动)施放后,SkillContext 永不回收。目前没有玩家拥有这类技能,属于潜伏缺陷;改造前就存在,不是第 1 批引入的
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:251-269,cpp/libs/services/scene/combat/skill/system/skill.cpp:646-663,cpp/tests/skill_test/skill_test.cpp:884-896

机制属实:上下文只有 HandleSkillFinish 会删。非 General、非 Channel 的技能不挂任何相位,所以既没有定时器收尾,也没有相位可打断,上下文会留到实体销毁。原文说「SetupCastingTimer_NoCastPhaseSkill_DoesNotStartPhase 只断言没挂相位组件」,这一点也属实。

两点需要更正:

(1) 原文说「批前上下文根本没入表、第 1 批之后泄漏才生效」,这不成立。基线 6281e4d27 的 InitializeActorCompsEventHandler 已为每个玩家 get_or_emplace\<SkillContextCompMap>,审计 F044/F050 在基线已标为「已修(actor 初始化挂组件)」,基线的 AddSkillContext 也确实写了表。当时无相位技能的走法是:基线 SetupCastingTimer 挂一个没有定时器的 CastingTimerComp,下一次 CheckCasting 发现定时器不在跑,只 remove 组件,不调 HandleSkillFinish。上下文同样泄漏,所以这是存量问题。

(2) 可达性:handler 用 PlayerSkillSystem::HasSkill 校验拥有关系;RegisterPlayer 把所有职业技能并给玩家,并集是 {1,2,13},skill_type 都是 [1],即 1\<\<1 = kGeneralSkill,都有前摇相位。目前生产路径到不了,只有改配表把 4..12 号发给玩家才会触发。

这类技能本身也没有任何执行逻辑。Java 版 SkillService.java:127 只在 def.general()||def.channel() 时建 Cast,两版行为已经分叉。性质是潜伏缺陷,且属存量,与 W1-07 / W1-01 同口径降为 low。原修法「SetupCastingTimer 返回 bool,失败再 HandleSkillFinish」是先建后删,不如直接不建。

#### W2-b1_runtime-03

- 原发现（medium）：cooldown_id=0(无冷却)的技能也建冷却表并写 0 号条目,之后每次施放都在热路径打 LOG_ERROR
- 复核（b1_runtime-g2）：**降级 / low**，cooldown_id=0(无冷却)的技能也会创建冷却表并写入 0 号条目,之后每次施放都会在 CheckCooldown 里打一条 LOG_ERROR。问题在改造前就有,第 1 批没有让它变坏;新问题只是这种写法违反了组件访问规则「真正启动冷却时才创建」
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:186-199,cpp/libs/services/scene/combat/skill/system/skill.cpp:599-601,cpp/libs/engine/core/time/system/time_cooldown.h:15,cpp/tests/skill_test/skill_test.cpp:239,cpp/tests/skill_test/skill_test.cpp:257

机制属实:skill.json 中 2..12 号的 cooldown_id 都是 0,cooldown.json 只有 id 1..9。StartCooldown 不判 0,会写入 key=0。下一次 CheckCooldown 会找到这个条目,调用链是 IsInCooldown → GetStartTime≠0 → Remaining → LookupCooldownOrReturn(0) 查不到行 → TableLookupLogMissing 打 LOG_ERROR,最后返回 0,判定「不在冷却」。结果正确,但每次都打 ERROR。可达性:RegisterPlayer 把所有职业的技能并给每个玩家,并集是 {1,2,13},2 号技能人人都有;robot_ai.castSkill 从已拥有技能里随机挑,压测中大约三分之一的施放会打这条 ERROR。

原文说「批前组件从不存在、第 1 批改成首写后才出现」,这一点不成立。基线 6281e4d27 的 ActorEventHandler::InitializeActorCompsEventHandler 已在 actor 初始化时 get_or_emplace\<CooldownTimeListComp>,并经 event_handler.cpp:23 注册生效;基线的 StartCooldown 拿得到组件,同样写 key=0;基线 time_cooldown.h 的 Remaining 和查表宏也与现在一字不差。所以 ERROR 刷屏是存量问题,第 1 批没有让它变坏。

与 ECS 相关的只剩一点:ecs-component-access-rules.md:41 写明 CooldownTimeListComp 在「真正启动冷却时」创建,而 0 号技能并没有启动冷却,现在却会建表。这与 W1-24 的情形相同(改造前就有的缺口,ECS 改造没让它变坏),按同一口径降为 low。修起来成本极低,建议本批顺手修。

原修法同时改 CheckCooldown,这一步多余:StartCooldown 修好后不会再有 0 号条目,冷却表又不入库,不存在旧条目。原修法还漏了一点:修好后 SkillPrecheckPolicyTest 释放的 2 号技能不再建冷却表,ExpectRejectionPreservesOldCast 的 get\<CooldownTimeListComp> 会在 EnTT 里对缺失组件断言失败。

#### W2-b1_runtime-04

- 原发现（medium）：CanContinueSkillCast 在同一同步调用链里背靠背重复校验多达 6+N 次,另有预检/提交两份相同检查
- 复核（b1_runtime-g2）：**降级 / medium**，一次普通技能施放要做 6+N 次 CanContinueSkillCast,外加 2+N 次 HasRequiredTargetComps(1/2 号技能有 8 个效果,即 14+10 次);CheckState 还重复了 348 行的 ValidateSkillUsage。只有定时器回调入口需要复核。描述成立,但原修法需更正:TriggerSkillEffect 不需要返回 bool,388 的复核应移到 HandleChannelFinish,还要删掉一条随之失效的测试
- 位置：cpp/libs/services/scene/combat/skill/system/skill.cpp:372-384,cpp/libs/services/scene/combat/skill/system/skill.cpp:387-388,cpp/libs/services/scene/combat/skill/system/skill.cpp:430-452,cpp/libs/services/scene/combat/skill/system/skill.cpp:472-483,cpp/libs/services/scene/combat/skill/system/skill.cpp:694-726,cpp/libs/services/scene/combat/skill/system/skill.cpp:891-892,cpp/libs/services/scene/combat/skill/system/skill.cpp:314-318,cpp/tests/skill_test/skill_test.cpp:743-759

计数属实。普通技能、N 个效果时,CanContinueSkillCast 的调用点依次是 373、892、376、695、N×719、382、388,共 6+N 次;HasRequiredTargetComps 是 918、711、N×720。每次 CanContinueSkillCast 都要做上下文查表、CheckRequiredSkillComps(all_of 加网络号反查)、冻结检查和 InBattle 检查。

这些检查之间夹着的代码不可能让结论改变:

- DealDamage 的 buff 钩子:OnBeforeGiveDamage → BuffImplSystem;OnAfterTakeDamage 只重置脱战加成;OnBeforeTakeDamage、OnAfterGiveDamage、OnBeforeDead、OnAfterDead、OnKill、OnSkillExecuted 全是空 TODO。

- AddOrUpdateBuff:只动 buff 列表,以及沉默的 CombatStateAdded/Removed 事件,注释明说回调不销毁实体。

它们都不会摘 BaseAttributes/Level/Derived/网络号,不会加 PlayerFrozenComp 或 InBattleComp,也不会删上下文。

真正有意义的只有定时器回调入口:373、431、468,以及引导结束进后摇的那一处。376、382、452 只是因为 HandleSkillSpell 和 TriggerSkillEffect 返回 void,被借来当「是否已中止」的信号。CheckState:315 与 348 重复,两者之间只有 CheckTimerPhase 的打断(HandleSkillFinish、EndAllSkillPhases、广播),都不改 CombatStateCollectionComp。349 与 316 是预检加提交,两处都需要保留。这正是负责人说的「再加防护兜住」,算明显过度设计,medium 成立。

原文小误:「452→892」实际是 431→892 背靠背,452 是中止信号。原修法有三处要更正:

(1) 删掉 695、711、719-723 后,TriggerSkillEffect 已经没有任何中止路径,不需要改成返回 bool;

(2) 把复核留在 HandleSkillRecovery:388,普通路径仍会背靠背多查一次,应移到真正的定时器回调 HandleChannelFinish;

(3) DirectEffectDispatchMissingRequiredDataEndsCastWithoutApplyingBuffs 直接调用 TriggerSkillEffect,测的正是要删的入口复核,修法必须同时处理它。

本条与 W1-26(ReleaseSkill 内 target_id 反查最多 6 次)同属一族,但代码和修法都不同,不判重复。

#### W2-b1_runtime-05

- 原发现（medium）：MoveSpeedComp 全仓零读者,却被定成每个 actor 的必需派生组件(创建、重算、缺失报错、测试一应俱全)
- 复核（b1_runtime-g1）：**成立 / medium**，MoveSpeedComp 在生产代码中没有任何读者，第 1/5 批却把它定成每个 actor 的必需派生组件，配齐了创建、标脏重算、缺失报错和多组测试
- 位置：cpp/libs/services/scene/actor/attribute/comp/actor_attribute_comp.h:12-23, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:34-71, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:113, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:130-133, cpp/libs/services/scene/actor/attribute/constants/actor_state_attribute_calculator_constants.h:6-9, cpp/libs/services/scene/combat/buff/system/modifier_buff_impl.cpp:13-27, cpp/libs/services/scene/spatial/system/movement.cpp:63, cpp/libs/services/scene/spatial/constants/nav.h:7-9, docs/design/ecs-component-access-rules.md:25, docs/design/ecs-component-access-rules.md:57

推翻失败。全仓非测试代码对 MoveSpeedComp 只有写入，没有读取：UpdateMoveSpeed 写值，InitializeOnCreate 创建，modifier_buff_impl.cpp:15/25 的 OnBuffStart/OnBuffRemove 标 kMoveSpeed 脏位。MovementSystem::Update 只按 view\<Transform, Velocity> 积分；player_movement_handler 的速度上限用常量 kMaxTrustedClientSpeed，nav.h:9 只在注释里写着「接入后应改为 MoveSpeedComp.moveSpeed\*1.1」。基线 6281e4d27 里，MoveSpeedComp 只是 UpdateMoveSpeed 中 get_or_emplace 出来的写侧组件。ECS 第 1/5 批把它升格为「actor 必需派生组件」：InitializeOnCreate 中创建并立即重算；重算时缺失就 LOG_ERROR 后跳过；ecs-component-access-rules.md:25/57 登记为必需派生数据；player_feature_handler_test.cpp:333/351/369/382 和 player_feature_persistence_test.cpp:147-148/922-928/937/1013 都写了专门断言。这是在为不存在的消费者维护整套生命周期，违反 §11.2 YAGNI，也正是负责人说的「保留本该删的东西」。按评级口径，「明显过度设计」定 medium。原发现的证据与修法都对，只需补齐遗漏的删除点：枚举、两处注释、规则文档、更多测试。另外核对过：kMoveSpeed 只用于运行期的 AttributeDirtyFlagsComp 位图，不入库、不进 proto；删除后后续枚举值会前移，但没有外部依赖。移速没有 S2C 通道（actor_attribute_calculator.cpp:41-42），删除它不影响客户端契约，也不涉及 Java 版对齐。

#### W2-b1_runtime-06

- 原发现（medium）：BuffPendingRemoveBuffs 实际只是 RemoveBuff(set) 一次同步调用内的重入标记,却作为 registry 组件反复 emplace/remove
- 复核（b1_runtime-g1）：**成立 / medium**，BuffPendingRemoveBuffs 实际上只是 RemoveBuff(set) 一次同步调用内的「正在删除」标记，却作为 registry 组件反复 emplace/remove；它的「帧末移除」API 只有测试在用，表行缺失时还会每帧重试并刷错误日志
- 位置：cpp/libs/services/scene/combat/buff/system/buff.cpp:277-296, cpp/libs/services/scene/combat/buff/system/buff.cpp:309-343, cpp/libs/services/scene/combat/buff/system/buff.cpp:372-377, cpp/libs/services/scene/combat/buff/system/buff.cpp:428-431, cpp/libs/services/scene/combat/buff/system/buff.cpp:795, cpp/libs/services/scene/combat/buff/system/buff.h:42-46, cpp/libs/services/scene/combat/buff/comp/buff_comp.h:29-36, docs/design/ecs-component-access-rules.md:45, docs/design/ecs-component-access-rules.md:95

推翻失败。git grep 显示 MarkBuffForRemoval 的生产调用方只有 RemoveBuff(set):284。这个调用是 ab0f27907（ECS 整理快照）新加的；基线 6281e4d27 中 MarkBuffForRemoval 没有调用方，RemoveBuff(set) 也只是逐条调用 RemoveBuff。RemoveBuff(set) 第一轮标记，第二轮对每个仍在列表里的 id 调用 OnBuffExpire。所有 buff 删除都经过 OnBuffExpire:371 的 erase，它在 372-377 同步地把待删条目移除，集合空了就 remove 组件。BuffListComp 只在帧末 Update:802 摘除，不会在这次调用中途消失。因此在生产中，这个组件只存活于一次同步调用之内，属于请求级数据，按 ECS 适用判据不应进 registry。它唯一的读者 HandleExistingBuff:428-431 的作用，是在删除回调重入 AddOrUpdateBuff 时，防止新 buff 叠到正在删除的同表条目上（buff_test 737/742 的两条重入用例守的就是这个）。帧末的 RemovePendingBuffs 虽然每帧对每个带 buff 的实体执行一次 try_get，但只有在待删集合跨帧残留时才做事，而残留只有一种可达来源：OnBuffExpire 在 LookupBuffOrReturnVoid 处因表行缺失提前返回（例如热更后删了行）。此时 Update 每帧调用 RemovePendingBuffs → RemoveBuff(set) → 重新 MarkBuffForRemoval → OnBuffExpire 再次失败，每帧打印一条 TableLookupLogMissing，并反复 emplace/remove 组件。329-332 的注释「MarkBuffForRemoval 全仓无调用方，这个集合永远是空的」描述的是旧状态，已经与现行代码不符。综合来看，这是 ECS 适用性违规，带有轻微后果（表行缺失时刷日志、无谓的组件增删和每帧查询），而且 MarkBuffForRemoval/RemovePendingBuffs 是只服务于测试的 API，定 medium 合适。原修法方向正确：把标记做成 BuffEntry 上的字段，与条目同生同灭，最直接，也不需要任何清理路径。

#### W2-b1_runtime-07

- 原发现（medium）：旧快照的背包在派生属性算完之后才恢复,DerivedAttributesComp 漏算装备加成
- 复核（b1_runtime-g3）：**降级 / low**，旧格式快照(只有顶层 bag_data)的背包在派生属性算完后才恢复,DerivedAttributesComp 漏算装备加成,当前 HP 也被压到无装备上限;项目未上线,当前也没有任何代码写出这种快照
- 位置：tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl:28-38,cpp/libs/services/scene/player/system/player_data_loader.h:30-39,cpp/libs/services/scene/player/system/player_database_loader.cpp:137,cpp/libs/services/scene/player/system/player_database_loader.cpp:170

机制属实。PlayerDatabaseMessageFieldsUnmarshal 先用缺省的空 bag_component 建出空背包(137),InitializeOnLoad 在 170 行建 DerivedAttributesComp 并调用 Recalculate。Recalculate 经 CollectBonus 从空装备栏算加成,写出不含装备的上限、combat 块和 BaseAttributesComp.speed;旧上限为 0,所以 kLoad 分支用 RescaleCurrent 把当前 HP/MP 压到这个偏低的上限。之后 player_data_loader.h:35 才用 bag_data 再 Unmarshal 一次,生产代码里没有任何加载后的重算(Recalculate 只在换装和升级时调用)。BuildBattleSnapshot 和面板读的都是缓存的 DerivedAttributesComp,所以本次会话整段的战斗和面板都缺装备加成。降级理由有三。(1) 可达性:这个分支是 441097343(2026-09-11)加的兼容分支。现行 PlayerAllDataMessageFieldsMarshal 每次都同时写 bag_component 和 bag_data;Go 侧(go/ 下 grep 不到对 BagData 的赋值)和 snapshot 也都不会写出「只有顶层 bag_data」的快照,只有 09-11 之前的二进制会产出。PROGRESS 2026-09-15 记有「项目未上线,没有老号」,§6.2 每轮压测前还要清空 Redis/MySQL。(2) 后果:不碰资产,只影响一次会话的派生值和当前 HP,重新存盘后会写回 bag_component,下次登录自愈。(3) 这是基线 6281e4d27 之前就有的老问题(当时的顺序完全相同),不是 ECS 改动引入的,只是与「派生数据在创建时由全部输入算好」的口径冲突。原修法方向正确,但要改正一处:模板对 Entries 是泛型 range,不能只针对 player_database 写一份特例,应在 range 之前把旧数据并进一份 PlayerAllData 副本。

#### W2-b1_runtime-08

- 原发现（medium）：入场写路径仍对持久组件 Transform 用 get_or_emplace(同类还有 RegisterPlayer 对 PlayerSkillListComp)
- 复核（b1_runtime-g3）：**降级 / low**，入场落位 EnsureValidEnterLocation 对持久组件 Transform、默认技能 RegisterPlayer 对持久组件 PlayerSkillListComp 仍用 get_or_emplace,RegisterPlayer 还重复做了一次 SanitizeSkillList;生产中这两处都不会真的建出组件
- 位置：cpp/libs/services/scene/spatial/system/scene_spawn.cpp:101,cpp/libs/services/scene/player/system/player_skill.cpp:40-42

规则违例属实。按 ecs-component-access-rules.md 硬规则 2/4,get_or_emplace 只允许出现在创建点和按需状态的写入口,持久组件绝不延迟创建。对 15 个持久组件 grep 全仓的 get_or_emplace,只剩这两处在创建点之外:bag_marshal、mission_marshal、PlayerMissionSystem::Initialize 三处都位于 Unmarshal 创建链内,不算违例。这两处在基线就是 get_or_emplace,是第 1 批清理 F059 时漏掉的。降级理由:实际不可达。所有玩家实体都只经 InitPlayerFromAllData 创建,PlayerDatabaseMessageFieldsUnmarshal 无条件 emplace 了 Transform 和 PlayerSkillListComp,全仓没有任何 remove/erase\<Transform>。EnsureValidEnterLocation 的唯一生产调用方是 player_scene.cpp:281;RegisterPlayer 的唯一调用方是 InitPlayerFromAllData:4151,紧跟在 Unmarshal 之后。所以 get_or_emplace 在生产中只会取到已有组件。即使不变量真被破坏,后果也只是坐标落到出生点、默认技能被补上,存盘时还有其余 13 个持久组件各自的拒存兜着,算不上 medium 的「有后果」。RegisterPlayer 开头的 SanitizeSkillList 是多余的第二遍:Unmarshal 刚做过,中间没有任何代码改过技能表。

#### W2-b1_runtime-09

- 原发现（low）：InitializeOnCreate 为不存在的场景设计成「可重入、投影已有状态、保留脏位」并返回不可达的 bool
- 复核（b1_runtime-g1）：**成立 / low**，InitializeOnCreate 只在新建的玩家实体上调用一次，却写成了可重入的契约：投影已有来源、保留脏位、返回走不到的 bool，还配了两条人造场景测试
- 位置：cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.h:15-19, cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:119-136, cpp/libs/services/scene/player/system/player_lifecycle.cpp:4135-4140, cpp/tests/bag_test/player_feature_persistence_test.cpp:937, cpp/tests/bag_test/player_feature_persistence_test.cpp:1013, docs/design/ecs-component-access-rules.md:57

现状核对：git grep 显示 InitializeOnCreate 的生产调用点只有 player_lifecycle.cpp:4137 一处，全仓也只有 player_lifecycle.cpp:4084 这一个 actor 创建点。实体在 4084 刚 create，随后 emplace 进 playerList，接着执行 Unmarshal。Unmarshal 只 emplace 持久组件（player_database_loader.cpp:113-168 中有 Transform、BaseAttributesComp、LevelComp 等），没有 BuffListComp、CombatStateCollectionComp、SkillContextCompMap。Unmarshal 链路（player_data_loader.h、两个 loader、bag/mission marshal、PlayerAttributeSystem::InitializeOnLoad、PetSystem::InitializeOnLoad）里也没有 dispatcher.trigger，player_attribute.cpp 唯一的 trigger 在 GmSetLevel 里。由此可得三点：(1) 走到 4137 时，canContinueInitialization() 的三个条件（valid、GetPlayer==player、notificationOwnership 为空）恒为真；(2) InitializeOnCreate 只在 !valid 时返回 false，这个分支走不到；(3) 新实体上不存在 Buff 或战斗状态来源，CombatStateFlagsComp 和 MoveSpeedComp 也都不存在，所以「重复调用、投影已有来源、不清已有脏位」在生产中没有对应场景。937 号测试只能手工 emplace BuffListComp、CombatStateCollectionComp、SkillContextCompMap 并调用两次，才造得出这个场景；1013 号测试专测 null 和过期句柄返回 false，同样只服务于这条走不到的分支。原文「万一走到只收拾一半」成立：InitializeOnCreate 返回 false 时直接 return entt::null，既不清 playerList，也不调用 RefuseInheritedEntry。但这条路径走不到，所以定 low，不必升级。canContinueInitialization 在 4137 这一处的删除已经包含在 W1-23 的修法里（W1-23 点名了「4137-4156 中的 canContinueInitialization 调用」）。本条独有的部分是 InitializeOnCreate 的可重入契约、bool 返回值、两条人造测试和规则文档里的对应描述，与 W1-23 不同根，所以判「成立」。W2-cross_sem-09 与本条同根，本条编号更小，作为主条目。

#### W2-b1_runtime-10

- 原发现（low）：StoreBattleFreezeForWrite 是一行透传,还把三处已确认不存在的 emplace 换成静默复用的 get_or_emplace
- 复核（b1_runtime-g1）：**成立 / low**，StoreBattleFreezeForWrite 只是一行透传的 helper；它把三处先确认不存在、再 emplace 的业务创建改成了静默复用的 get_or_emplace，丢掉了 EnTT 对重复创建的断言
- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:141-146, cpp/libs/services/scene/battle/system/player_battle.cpp:1215, cpp/libs/services/scene/battle/system/player_battle.cpp:1550, cpp/libs/services/scene/battle/system/player_battle.cpp:1637, docs/design/ecs-component-access-rules.md:24, docs/design/ecs-component-access-rules.md:55

推翻失败。三个调用点都在同一函数体内、同步地先确认了 InBattleComp 不存在：1215 前有 1179 的 try_get 存在即拒绝；1550 前有 1515 的 try_get 存在即放弃；1637 前有 1592 的 any_of 存在即返回。检查与创建之间只有 BuildBattleSnapshot、FindSettlementLedger、BuildInBattleFromCtx 这类只读或纯计算的调用，全仓也没有其他生产代码 emplace InBattleComp（git grep 只命中这个 helper 和测试）。所以这里用 get_or_emplace 不会改变可达行为，但有两处代价：一是把基线 emplace 在调试版对「已存在」的断言（EnTT sparse_set.hpp:341/349 的 "Slot not available"）换成了静默复用，将来有人漏掉前置检查时，旧冻结会被 CopyFrom 覆盖，或者在 PrepareBattle 路径上只被部分改写，而且不会报错；二是多了一层没有附加逻辑的间接调用。它的来源是规则文档 ecs-component-access-rules.md:24 的「首写集中在一个命名函数里」被机械套到了 InBattleComp 上。InBattleComp 不是首写时才懒建的容器，而是三个业务事件各自创建的状态；规则文档 55 行本来就这样描述它。没有可达缺陷，定 low 合适。

#### W2-b1_runtime-11

- 原发现（low）：PetSystem::BuildBattleSnapshot 的契约写的是「缺组件照常进战斗」,与唯一调用方的 fail-closed 前置检查相反
- 复核（b1_runtime-g3）：**降级 / low**，PetSystem::BuildBattleSnapshot 的注释说缺组件时玩家照常进战斗,实际唯一调用方已先 fail-closed;同一个 false 被同时解释成「没有出战宝宝」和「缺持久组件」
- 位置：cpp/libs/services/scene/player/system/player_pet.cpp:931,cpp/libs/services/scene/player/system/player_pet.h:32,cpp/libs/services/scene/player/system/player_pet.h:111

问题属实,而且在 ECS 改动范围内:基线中这里还是 EnsureComp 补建;第 1 批改成了 FindComp,并写了「玩家本人照常进战斗」;ab0f27907 在 PlayerBattleSystem::BuildBattleSnapshot 加了 all_of 前置拒绝,却没回头改这段注释。现在注释描述的是调用方并不存在的行为,头文件 32/111 行也把 false 解释成「不带宝宝/不是错误」。原修法的第二个选项不能用:把 FindComp 分支换成 get\<PlayerPetComp>,公开接口遇到缺组件就从「返回 false」变成 EnTT 断言或 UB。现有用例 PetComponentOwnershipTest.ReadAndRecalculateEntriesCreateNothing(player_feature_handler_test.cpp:516)正是对没有 PlayerPetComp 的实体调用 BuildBattleSnapshot 并期望 false,换成 get 后会直接崩。所以严重度维持 low,修法只保留改注释。与 W2-cross_sem-05(调用方 player_battle.cpp 前置校验后仍留着缺组件默认值分支)相邻,但函数和修法都不同,不判重复,可以放在同一次提交里改。

#### W2-b2-01

- 原发现（medium）：节点 registry 里的「全局实体」设施仍在：GetOrCreateGlobalEntity/GetGlobalEntity/SetGlobalEntity 生产零调用，等于换个名字的 GrpcNodeEntity
- 复核（b2-g1）：**降级 / low**，NodeContextManager 的每节点类型「全局实体」三件套随 etcd 迁移失去最后一个调用方，仍连同越界 FATAL、三条死亡测试和失实的头注释保留着
- 位置：cpp/libs/engine/thread_context/node_context_manager.h:11,cpp/libs/engine/thread_context/node_context_manager.h:13,cpp/libs/engine/thread_context/node_context_manager.h:16,cpp/libs/engine/thread_context/node_context_manager.h:24,cpp/libs/engine/thread_context/node_context_manager.h:53,cpp/libs/engine/thread_context/node_context_manager.h:78,cpp/libs/engine/thread_context/node_context_manager.h:92,cpp/tests/routing_identity_test/routing_identity_test.cpp:390,cpp/tests/routing_identity_test/routing_identity_test.cpp:394,cpp/tests/routing_identity_test/routing_identity_test.cpp:405

事实部分成立。在整个工作区（不含 docs）用 rg 搜 GetOrCreateGlobalEntity、GetGlobalEntity(、SetGlobalEntity(，只命中头文件自身和 routing_identity_test.cpp:405–410 的三条 EXPECT_DEATH。基线 6281e4d27 的唯一调用方是 etcd_helper.cpp 与 etcd_manager.cpp（GetGlobalEntity(EtcdNodeService)），由 f0dadf5b0 随 GrpcEtcdTransport 一起删除。头注释第 16–23 行讲的「etcd 全部 gRPC 客户端状态 emplace 在它上面」和「EcsContext 对各 globalEntity_ 逐一初始化」，两件事都已不存在。

严重度和描述需要更正。(1) 第 2 批任务单删除的范围是 EcsContext 的 globalRegistry/nodeGlobalRegistry 和 GlobalEntity()/GrpcNodeEntity()/ErrorEntity()/OperatorEntity()，没有要求删 NodeContextManager 这一套。PROGRESS.md:8285 写「NodeContextManager/etcd 那套合法 API 保留」，在当时 etcd 仍在用的前提下是对的。这套 API 是被迁移会话的 f0dadf5b0 新变成孤儿的，不是第 2 批漏做。(2) 它没有运行期后果。死 API 只是留着让人「再往全局实体挂单例」的门，按第一波同类先例（W1-27 死字段、W1-19 已删符号仍被引用，均判 low）定为 low。与 W2-fit-08 同根，本条编号字典序较小，作为主条目。

#### W2-b2-02

- 原发现（medium）：过渡用的空 TRANSFER_ERROR_MESSAGE 宏零调用，注释却写「生成头仍有调用」，C2 没做
- 复核（b2-g1）：**成立 / medium**，过渡用的空 TRANSFER_ERROR_MESSAGE 宏已无调用方，注释还写着「生成头仍有调用」；任务单 C2 的前置门早已满足，宏却没删
- 位置：cpp/libs/engine/core/macros/return_define.h:66,cpp/libs/engine/core/macros/return_define.h:67,cpp/libs/engine/core/macros/return_define.h:68

我试着推翻它，没推翻。(1) 在当前工作区用 rg 搜 cpp/ 与 tools/，TRANSFER_ERROR_MESSAGE 只命中两处：本宏定义，以及 player_service_response_contract_test.go:97 的禁止串。cpp/nodes/scene/handler/rpc/player/ 下的 14 个生成头一处都没有，非 Empty 方法改成了 static_assert 守卫。模板 player_service_gen.go 也不再产出它。(2) batch2-singletons.md §4.16 给 C2 定的前置门是「git grep 只剩 return_define.h 自身」，现在已经满足。Codex 回执原话是「旧生成头的空 TRANSFER 宏暂留，统一重生成后删除」，重生已经发生，删除这一步没做。(3) 第 66 行注释「生成头仍有调用」现在是错的。按严重度标准，「任务单要求未落实」归 medium，所以维持 medium。运行期没有影响：它是空操作，也没有调用方。删除还有一个附带好处：模板以后如果误把这一行带回来，C++ 会直接编不过，不会再被一个空宏悄悄吞掉。与 W2-cross_sem-06 同根，本条编号字典序较小，作为主条目。

#### W2-b2-03

- 原发现（medium）：身份门面：atomic 加返回旧值的 exchange 只服务测试，SameInstanceOnEveryThread 等用例把契约禁止的跨线程读和换主固化下来
- 复核（b2-g1）：**成立 / medium**，NodeInfo 只读门面的 atomic 发布和「返回旧值」的 exchange 只为测试存在；SameInstanceOnEveryThread 把契约禁止的跨线程读固化成用例，挑战意见 [low] 未落实
- 位置：cpp/libs/engine/core/network/node_utils.cpp:3,cpp/libs/engine/core/network/node_utils.cpp:16,cpp/libs/engine/core/network/node_utils.cpp:17,cpp/libs/engine/core/network/node_utils.cpp:20,cpp/libs/engine/core/network/node_utils.cpp:27,cpp/libs/engine/core/network/node_utils.h:18,cpp/libs/engine/core/network/node_utils.h:24,cpp/tests/cross_zone_test/cross_zone_test.cpp:2,cpp/tests/cross_zone_test/cross_zone_test.cpp:6123,cpp/tests/cross_zone_test/cross_zone_test.cpp:6138,cpp/tests/cross_zone_test/cross_zone_test.cpp:6147,cpp/tests/cross_zone_test/cross_zone_test.cpp:6216

我尝试从三个方向推翻，都没成功。

(1) 找真实的跨线程读者。Node::AsyncOutput 用的是另一个 gNodeAtomic，不读 NodeInfo。grpcShutdownThread_ 只调 server->Shutdown。node.cpp 里的 GetNodeInfo() 解析到的是成员函数。battle 的 gRPC 线程按注释（battle_node.cpp:131、battle_room_manager.h:338、client_endpoint.h:105）明令不得读。没有找到合法的生产跨线程读者。

(2) 看安装时机。Node 构造时（node.cpp:414）安装；会读身份的工作线程都在构造之后启动，std::thread 的创建本身就建立 happens-before，普通指针已经足够。卸载在 shutdownComplete_ 之后，此时 localNodeInfo_ 也即将析构，atomic 保护不了引用寿命，注释（node_utils.cpp:16、node_utils.h:22）自己也承认这一点。

(3) 看 exchange 的旧值有谁用。生产两处调用都丢弃返回值，第二个 Node 会在 node.cpp:408 LOG_FATAL，不存在「换主再还原」。旧值只在 cross_zone_test 的 6140/6143/6151/6155/6219 行被用到。

需要更正一处归属：atomic 和「返回旧值，便于 RAII 还原」是任务单 batch2 §4.9 B1 第 1–3 条明文规定的，Codex 照单实现，算任务单自身的过度设计，不是 Codex 自加。但挑战意见 [low] 已明确要求删掉 SameInstanceOnEveryThread 或改写它，Codex 两样都没做，只补了一行注释，回执反而把它列为新增回归。用例在另一线程读身份并断言成功，与 node_utils.h:20「访问限所属 loop 线程」正面冲突。F008 中「换线程拿到空值」那一半在现行契约下已不是合法场景，F008 的回归由 SurvivesEcsClear 覆盖就够了。按「挑战意见未落实 / 明显过度设计」归 medium。原发现第 (4) 点也属实：node_allocator.cpp 写走 gNode->MutableNodeInfo()（90、296、316、319、320），读走自由函数 GetNodeInfo()（99-100、192-193、223、254、319、326），可读性层面，可选修。

#### W2-b2-04

- 原发现（medium）：ecs_context_test 的会话索引用例对基线实现也全绿，回执里「2 条修复前为红」的证据对当前代码不成立
- 复核（b2-g2）：**降级 / low**，ecs_context_test 5 条会话索引用例中有 4 条测的是 std::unordered_map、thread_local 或无关 registry，对基线同样全绿；回执和 PROGRESS 里「旧 getter 对照 2/5 失败」的证据对当前源码已不成立。原修复由 globalRegistry 删除在编译期守住，不缺回归保护
- 位置：cpp/tests/ecs_context_test/ecs_context_test.cpp:36-47, cpp/tests/ecs_context_test/ecs_context_test.cpp:49-61, cpp/tests/ecs_context_test/ecs_context_test.cpp:81-93, cpp/tests/ecs_context_test/ecs_context_test.cpp:95-111, docs/handoff/ecs-refactor/batch2-singletons.md:407, docs/PROGRESS.md:8268

事实部分成立。基线 SessionMap() 只碰 globalRegistry。当前用例 1 数的是 actorRegistry 和 sceneRegistry 的池，用例 2 清的也是这两个 registry，对旧 getter 必然通过。用例 4 测的是 std::unordered_map 的语义，用例 5 测的是 thread_local 的语义，对任何实现都通过。所以回执 batch2-singletons.md:407 和 PROGRESS.md:8268 写的「旧 getter 对照 2/5 失败」只对 globalRegistry 还存在时的旧版断言成立。globalRegistry 删除后断言被改指到无关 registry，那份红证据不再对应当前源码。降级的理由：F092/F099 的原缺陷是 SessionList 作为组件挂在 globalRegistry 的全局实体上，这个形状随 EcsContext::globalRegistry 成员删除已在编译期无法表达（batch2 §4.17 对 AOI 用例用的是同一个理由），修复并不缺保护。剩下的问题只是 4 条无效用例在充数，以及交接文档里的证据过时，不是「修复缺回归测试」的 medium。第 3 条 ContextClearDropsBothIndexesAndAllowsNewSession 有效：删掉 EcsContext::Clear() 里的 sessionPlayers.clear() 它就会红。SceneEntityContract 用例不在本条范围。

#### W2-b2-05

- 原发现（low）：玩家接口契约仍写「RpcDoneCallback 延后完成同样遵守」，engine 的 rpc_done_callback.h 反向指向 services 规则，挑战意见 [low] 没落实
- 复核（b2-g2）：**成立 / low**，玩家接口契约写着「RpcDoneCallback 延后完成同样遵守」，但玩家 handler 拿不到 done；engine 的 rpc_done_callback.h 反向指向 services 规则，还提到已不存在的全局 tip。挑战意见 [low] 没有落实
- 位置：cpp/libs/services/player/rpc/player_service_interface.h:20, cpp/libs/engine/core/network/rpc_done_callback.h:20-21, docs/design/ecs-component-access-rules.md:69

想推翻它，但推不翻。PlayerService::CallMethod 的签名里没有 Closure\* done，玩家 handler 没法 Adopt。全仓生产代码里 RpcDoneCallback::Adopt 只有 rpc_channel.cpp 自己的实现和 gate 注释提到，玩家路径上没有调用方。docs/design/player-handler-deferred-reply.md 第 3 行写着「设计已定，暂不实施」，第 46 行写「player 库不认识 RpcDoneCallback（依赖倒置）」，计划的类型是 PlayerDeferredReply\<R>，所以 player_service_interface.h 这句点错了类型。batch2-singletons.md:370-371 的挑战意见 [low] 明确要求删掉这两处，Codex 回执没有处理，当前代码原样保留。rpc_done_callback.h 是 engine/core 头文件，却引用 libs/services/player 的规则，层次是反的。第 21 行「进程里没有全局 tip」是给一个已删除的东西留的说明。另外，同一头文件第 46 行 Fail() 的注释已经写了「业务失败优先写响应里的 error_message 再 Run」，第 20 行这条整体重复。需要更正的一处：ecs-component-access-rules.md:69 这句对节点级 RPC 来说本身是对的（muduo-rpc-channel.md:260 也这样写），说它「把玩家 RPC 混进来」言重了。但这条规则在 ECS 规则文档里属于第二份副本，删掉不丢信息。整体仍是注释层面的问题，维持 low。player_service_interface.h 工作区有其他会话的未提交改动（批 7 退役 InvokePlayerService），该句的行号从 19 漂到了 20。

#### W2-b2-06

- 原发现（low）：多处文档和注释仍按全局 tip、TLS NodeInfo 描述，C4.3 指名的 player-features-ui.md 没改
- 复核（b2-g2）：**成立 / low**，C4.3 指名的 player-features-ui.md:22 没改，仍教「写 TLS tip 由 TRANSFER 宏转移、直接写 response 会被覆盖」，与现行规则相反；另有 6 处注释或 include 仍按全局 tip、TLS NodeInfo、GlobalEntity 解释现状
- 位置：docs/design/player-features-ui.md:22, cpp/libs/services/scene/player/system/player_landing.h:126, cpp/nodes/scene/handler/rpc/player/player_gm_guard.h:66, cpp/libs/engine/core/network/network_utils.cpp:28-29, cpp/libs/engine/core/network/rpc_channel.cpp:18, cpp/libs/services/scene/spatial/system/aoi.cpp:177-178, cpp/tests/aoi_test/aoi_system_test.cpp:40-41, docs/design/guild-phase2/04-asset-channel.md:521, docs/design/guild-phase2/04-asset-channel.md:1365

逐处核对，都成立。(1) batch2 §4.18 C4 第 3) 条明确要求改 player-features-ui.md:22，当前原样未改，内容与 player_service_interface.h 的现行规则正好相反。照做会去找已不存在的 TLS tip 和 TRANSFER 宏，所幸在编译期暴露，所以维持 low。(2) player_landing.h:126 是挑战意见「遗漏的调用点」第一条，仍拿「全局 tip 会污染」当理由。(3) player_gm_guard.h:66 让调用点「负责 SetTip」，但 SetTip 在 cpp 下已零定义，各调用点（如 player_attribute_handler.cpp:171-174）实际写的是 response。(4) 基线 GetNodeInfo() 是 tlsEcs.globalRegistry.get_or_emplace，确实是 thread-local。当前 node_utils.cpp:17-34 改为进程级 atomic 发布 Node::localNodeInfo_，zone_id 在 Node::InitRpcServer（node.cpp:555）写入，所以 network_utils.cpp 的「thread-local zone context」已过时。(5) rpc_channel.cpp:18 的 tip.pb.h 由 ab0f27907 引入，基线 game_channel.cpp 没有这个 include，当前文件也没有任何 tip 用法，是为已删的 CompleteDeferred 防御块留下的残余。(6) aoi.cpp:177-178 和 aoi_system_test.cpp:40-41 用已删的 GlobalEntity/globalRegistry 解释现状。(7) guild-phase2/04-asset-channel.md 的两处示例属于历史设计。aoi_delivery_test.cpp:63 的同类注释已并入 W2-b2-07。W2-cross_sem-07（robot 冒烟注释）根因相近但文件不同，不判重复。原修法给 player_landing.h:126 的替换文字「所有失败都推 UNAVAILABLE」与上一行第 125 行「除实体无效 / 玩家已不在本节点外」的例外矛盾，需要更正。

#### W2-b2-07

- 原发现（low）：C3 没按挑战意见收尾：用例名和注释仍指向已删的 GlobalEntity，末尾断言改成了对旧实现也必过的 actor/scene 检查
- 复核（b2-g2）：**成立 / low**，C3 没按挑战意见收尾：用例名和注释仍指向已删除的 GlobalEntity/globalRegistry；末尾换上的 actor/scene any_of 断言对旧实现同样必过，是无效断言
- 位置：cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:183-185, cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:212-216, cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp:63

想推翻它，但推不翻。基线 aoi.cpp 的三处发送缓冲都挂在 globalRegistry 的 GlobalEntity 上，从来没有往 actorRegistry 或 sceneRegistry 挂过，所以 213-216 行的 EXPECT_FALSE 对修复前的实现必然通过，拦不住它声称要拦的回归。当前 aoi.cpp:183/199/372 已改为函数内 static thread_local，globalRegistry 也已从 EcsContext 删除。原问题现在在类型上写不出来，这正是 batch2 §4.17 C3 要删除该断言的理由。挑战意见（batch2-singletons.md:364-365）要求删掉 GlobalEntity 断言和 F075 说明、给用例改名、保留前半段的投递和消息体断言。实际做法是把断言换成对旧实现同样必过的检查，用例名和 183-184 行注释也继续讲已删除的概念。第 63 行 SetUp 注释同样多出「不需要(也不应该)往 GlobalEntity 上预挂消息组件」。前半段 195-210 行（CreatesFor/DestroysFor 与 ActorDestroyS2C 消息体解析）有效，应当保留：它是文件内唯一覆盖离场 ActorDestroy 消息体的断言。

#### W2-b2-08

- 原发现（low）：NodeInfoListComp 只为 ServiceNodeList 单例而存在，单例删除后成了死 proto 消息
- 复核（b2-g3）：**成立 / low**，NodeInfoListComp 唯一使用方是已删除的 ServiceNodeList 单例，现在两个仓库都没有任何手写代码引用它，成了死 proto 消息
- 位置：proto/common/base/common.proto:38

想推翻这条，但没推翻成。(1) 在 6281e4d27 上执行 `git grep NodeInfoListComp`，排除 \*.pb.\* 和 generated/\*\* 后，只剩 node_utils.h:8 的 ServiceNodeList 别名，说明它确实只为这个单例存在。(2) 当前工作区用 rg 查同一范围（另外排除 build/\*\*、vendor/\*\*），只命中 proto 定义本身和两份 handoff 文档。cpp、go、java、robot 的手写代码都是零引用。proto/ 里也没有任何消息拿它当字段类型。protogen 生成器没有按 "Comp" 后缀做特殊处理（例如自动注册组件），所以也不存在隐式使用方。(3) 节点快照已经改成 ServiceNodeSnapshot 里的 RepeatedPtrField\<NodeInfo>。(4) Java 仓 D:\luyuan\xuanming-server-mmo 只在同步副本 game-proto/src/main/proto/proto/common/base/common.proto:41 里定义了它，target/、build/ 以外的手写代码都没引用。(5) 它没有被序列化、落库或放进 Any，删掉对线上数据没有影响，也不涉及 §4.3 的字段号复用。batch2-singletons.md §4.9 只要求删 ServiceNodeList 别名，没要求删这条消息，所以算单例拆除漏下的残余，不算违反任务单。它的危害只在可读性：名字带 Comp，读的人会以为它还是 ECS 组件，容易被重新放回 registry。low 的严重度合适。原修法里「暂不删就加注释」的兜底属于负责人反对的「留着再加防护」，应该去掉，只保留删除。

#### W2-b2-09

- 原发现（low）：SceneManager 选点头注释仍写「只在注册表为空时返回 null、不新增 null 窗口」，与过滤无传输实体后可返回 null 的新实现矛盾
- 复核（b2-g1）：**已被改写 / none**，SceneManager 选点「底线」注释已被迁移会话改写，现在写「只在没有挂着未关闭通道的实例时返回 null」，与新实现一致
- 位置：cpp/libs/engine/core/network/node_utils.cpp:45,cpp/libs/engine/core/network/node_utils.cpp:46,cpp/libs/engine/core/network/node_utils.cpp:47

原发现引用的原文「注册表里只要有一个 SceneManager 实体就一定挑出一个……只在注册表为空时返回 null」，已被 gRPC→muduo 迁移会话在工作区（未提交，git diff HEAD 可见）改写成现在第 45–47 行的版本。新版把「底线」限定在「挂着未关闭通道的实例」上，与 node_utils.h:29–32 和 GetSceneManagerEntity 中 HasUsableGrpcClient 的过滤一致。括号里的前提也核对过：ConnectToGrpcNode 先 emplace MuduoGrpcChannelPtr、再 emplace NodeInfo，端点为空或建通道失败都会 destroy 实体。入站握手在 registration_manager.cpp:221 拒绝纯 gRPC 类型。所以生产上每个已发现的 SceneManager 实例都带通道，「不新增 null 窗口」在这个前提下成立。唯一的细微差别：ExecuteNodeRemoval（node.cpp:1543–1546）先摘通道、再 trigger 摘除通知，通知期间正在摘除的那个实例不会被选中。这是注释写明的有意取舍，而且旧实现挑中它也只会以 UNAVAILABLE 失败，不构成矛盾。原问题不再以任何形式存在。顺带一提但不另立条目：③b「无通道」这一级和 Candidate::channelState 的空值语义，按第 75 行注释只服务纯选择器用例，生产不可达，属于另一类 YAGNI 问题，不在本条范围。

#### W2-b5_types-01

- 原发现（low）：读 PlayerGuidComp 的逻辑在 6 个文件里各有一份私有 GuidForLog,名为日志却被当身份校验和业务字段用,失效哨兵 0 与 kInvalidGuid 各处不一
- 复核（b5_types-g1）：**降级 / low**，读 PlayerGuidComp 的私有 GuidForLog 在 7 个文件里各有一份,还有 4 处守护段内联三元表达式,与 ActorLog 并存:名为日志,实际被用作异步身份栅栏和快照字段;第 4 批要求的「Guid 组件类型变化只改一处」没有达成
- 位置：cpp/libs/services/scene/player/system/player_scene.cpp:49,cpp/libs/services/scene/spatial/system/scene_spawn.cpp:38,cpp/libs/services/scene/battle/system/player_battle.cpp:135,cpp/libs/services/scene/player/system/player_equip.cpp:51,cpp/libs/services/scene/player/system/player_pet.cpp:45,cpp/libs/services/scene/player/system/player_attribute.cpp:63,cpp/libs/services/scene/player/system/gm_sanction_inbox_executor.cpp:36,cpp/libs/engine/core/utils/registry/actor_log.h:14,cpp/libs/engine/thread_context/ecs_context.h:42,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:38,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:62,cpp/nodes/scene/handler/rpc/player/s2s_player_scene_handler.cpp:77,cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp:30

问题存在,但描述需要更正。(1) 这不是本批引入的:基线 6281e4d27 就已有 6 份 GuidForLog(当时读 try_get\<Guid>),身份栅栏用法(player_scene IsSamePlayer、player_battle 8 处)和 player_pet 写快照 owner_player_id 也都是基线原样。B1 只是机械地换成 PlayerGuidComp。(2) ActorLog 不是「同批新增」,是第 4 批 4c-1 加的。该任务单(batch4-external-id.md:47、:231)明确写了「Guid 组件类型将来改动(F032 所在批次)时只需改这一处」「注释写明这是日志里标识 actor 的唯一写法」。第 5 批改 Guid 时实际改了 6 份 GuidForLog 和 4 处守护段内联,actor_log.h 的注释也没写「唯一写法」,任务单意图没有落实。(3) 现在是 7 份:收件箱会话在 gm_sanction_inbox_executor.cpp:36 又复制了一份,说明这个模式还在继续扩散,DRY 的后果是真实的。(4) 目前没有运行期错误:创建点 InitPlayerFromAllData(player_lifecycle.cpp:4061)拒掉 0;IsSamePlayer 先拒 0;battle 栅栏比对的 playerId 也取自同一函数;try_get 对陈旧句柄和 null 都安全返回空。所以严重度维持 low。(5) 原修法里统一哨兵这一步不成立,不能照做:player_lifecycle.cpp:2976 的 exitingPlayerId 回落值 kInvalidGuid 被 :2997、:3028、:3110 拿来与 kInvalidGuid 比较。只把 :2976 改成 0,这几处判断在缺 guid 时都会变成恒真,会拿 0 去查 HasUnsettledPlayerSave 等。该文件里还有 :103、:850、:2958、:3172、:3480、:4624、:4708 等十余处以 kInvalidGuid 表示「无玩家」;而创建点只拒 0、不拒 kInvalidGuid。哨兵是基线就有的口径分歧,涉及退出和存盘路径,不应夹在这条可读性修复里。(6) 原修法把 PlayerIdOf 放进 actor_log.h 不合适,那是日志头。ecs_context.h:42 已经有反向的 GetPlayer(Guid),而且已 include player_guid_comp.h,正向查询放在它旁边最自然。

#### W2-b5_types-02

- 原发现（low）：已有 PlayerSceneSystem::FindSceneById,scene_id→SceneEntity 的全表扫描仍在两个入口手写 4 份(F095 未收口)
- 复核（b5_types-g2）：**降级 / low**，已有 PlayerSceneSystem::FindSceneById,但两个入口仍各手写 2 份 scene_id 全表扫描(共 4 份);原修法漏了 gRPC 建场入口没有 scene_id==0 校验,直接替换会改变 0 号去重语义
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:498,cpp/nodes/scene/handler/rpc/scene_handler.cpp:583,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:25,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:96,cpp/libs/services/scene/player/system/player_scene.cpp:182

问题本身成立。FindSceneById 是这批改动新加的(基线 6281e4d27 的 player_scene.cpp 里没有),s2s 入场和 player_lifecycle 的三处已经改成调用它。但 SceneHandler::CreateScene/DestroyScene(当前 498、583 行)和 SceneNodeGrpcImpl::HandleCreateScene/HandleDestroyScene(当前 25、96 行)这 4 份扫描是基线原样留下的,git diff 里只把 entt::entity 改成了 SceneEntity。cross_sem-08 与本条同根。fit-02 范围更大:它要的是场景目录,并把两套创建/销毁合成一份。

严重度维持 low:4 份扫描当前行为一致,不构成可达缺陷,属 DRY/可读性问题。

需要更正的是原修法。它写「入口原有的 0 校验保留即可」,只对 muduo RPC 入口成立(scene_handler.cpp:472 校验了 config_id 和 scene_id 都非 0)。gRPC 路径的外层 CreateScene(scene_node_service.cpp:265)只拿 Agones permit,HandleCreateScene 本身也没有 0 校验。现在 scene_id==0 的请求第一次会建一个 scene_id=0 的场景,之后靠手写扫描去重到它。去重块改成 FindSceneById 以后,0 会恒返回 null,每次请求都会新建一个 scene_id=0 的实体。改法必须给 gRPC 入口补上与 RPC 入口一致的 0 拒绝。DestroyScene 两个入口改用 FindSceneById 后,0 号按「找不到」处理,比现状更好,不需要另外处理。

#### W2-b5_types-03

- 原发现（low）：SceneEntity 类型化后新增的注释声称 try_get 前必须先 valid(),与 EnTT 语义和组件访问规则表矛盾;多处冗余 valid 与复述类型的注释
- 复核（b5_types-g1）：**降级 / low**，grid.cpp 新增的注释声称 try_get 前必须先 valid(),与 EnTT 语义和组件访问规则表矛盾;另有几处与 try_get 等价的冗余 valid() 和复述类型的注释
- 位置：cpp/libs/services/scene/spatial/system/grid.cpp:44,cpp/libs/services/scene/player/system/player_lifecycle.cpp:3071,cpp/libs/services/scene/player/system/player_scene.cpp:77,cpp/libs/services/scene/player/system/player_team.cpp:122

核心问题成立:grid.cpp:44 这条注释是本批新增的(git diff 6281e4d27),把「必须先 valid 再 try_get」写成了规则。按 EnTT 3.13.2 的 registry::try_get → sparse_set::contains,陈旧句柄和 null 都安全返回 nullptr(SceneRegistry 就是 basic_registry\<SceneEntity>,语义相同)。ecs-component-access-rules.md:88 也写明 try_get 由完整句柄检查组件归属,注释与规则表矛盾,会诱导后人到处补冗余检查。FindSceneGridList 里的 valid() 本身有用,它负责区分「场景已销毁(打 ERROR)」与「缺格子表(正常空)」,只是注释写错了理由。需要更正的部分:(1) player_lifecycle.cpp:3071 不是「自相矛盾」。基线这里的注释错写成「对已销毁场景 try_get 是 UB」,Codex 本批把注释改正了,只是没删已经多余的 valid()。删掉它行为等价(陈旧句柄下 try_get\<SceneInfoComp> 同样为空),属于冗余,不是矛盾。(2) player_scene.cpp:77 和 player_team.cpp:122 的注释在基线就有(:70/:121),不是类型化引入的;SceneEntity 类型化之后它们只是变得多余。(3) 原修法的可选清单把 aoi.cpp:267/268 也列为冗余,这不成立。那里的 valid() 是「双方仍在同一个活场景」判定的一部分,后面接的是可见性计算,不是 try_get,删掉会改变语义。aoi.cpp:224/312、interest.cpp:55/72、player_team_aoi.cpp:59 后面紧跟 try_get、且不单独打日志,删掉才是等价的。

#### W2-b5_types-04

- 原发现（low）：四个具名容器组件的 explicit 基类转换构造全仓零调用,只是为它们才不得不写 X() = default
- 复核（b5_types-g1）：**成立 / low**，四个具名容器组件的 explicit 基类转换构造在全仓没有调用方,X() = default 只是因为写了这两个构造才不得不补上
- 位置：cpp/libs/services/scene/combat/buff/comp/buff_comp.h:24,cpp/libs/services/scene/combat/buff/comp/buff_comp.h:33,cpp/libs/services/scene/combat/skill/comp/skill_comp.h:59,cpp/libs/services/scene/spatial/comp/grid_comp.h:18,docs/design/ecs-component-access-rules.md:95

复核确认问题成立。git diff 6281e4d27 显示,这四个类型本批从 using 别名改成了派生 struct。任务单 batch5-lifecycle-types.md:313 要求手写 X(const Base&)/X(Base&&),理由是「继承构造不能从基类对象构造」;挑战意见 :464-465 给出两个选项:标 explicit,或者删掉只留 using Base::Base。Codex 选了前者。全仓搜索结果:没有任何代码从基类容器构造这几个组件。所有 emplace/get_or_emplace\<这四种> 都不带实参(buff.cpp:212、:322,skill.cpp:70,aoi.cpp:72,以及各测试),也没有按值构造或 static_cast。这两个构造在 YAGNI 意义上就是死代码。X() = default 是因为声明了这两个构造、隐式默认构造被抑制,才不得不补写;为了 std::move 还新加了 #include \<utility>。删掉以后行为不变:inherited constructor 规则会把基类的拷贝/移动构造从候选集里排除(对 D 而言,C 与 P 引用相关,P 是 D 的基类);同时有继承构造的类不是聚合体。所以基类对象既不能隐式、也不能显式转换成组件类型,挑战意见担心的临时拷贝(TimerTaskComp 拷贝丢定时器)依然不可能发生。另外,继承构造不算用户声明的构造,隐式默认构造会保留。规则文档 :95 的「基类容器转换必须显式」是在为这两个死构造背书,需要一起改。相关条目:W2-b1_runtime-06 如果把 BuffPendingRemoveBuffs 移出 registry,这个类型会整体消失,两条修复需要协调。

#### W2-b5_types-05

- 原发现（low）：模板改成“消费后整体 remove DirtyMaskComp”后,逐字段 dirtyMask.reset 成了死代码,6 个生成文件都带着
- 复核（b5_types-g2）：**成立 / low**，属性同步模板已改成消费后整体 remove DirtyMaskComp,循环里逐字段 dirtyMask.reset 成了死代码,6 个生成文件共 9 处
- 位置：tools/proto_generator/protogen/internal/template/attribute_sync.cpp.tmpl:49,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:39,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:49,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:59,cpp/libs/services/scene/generated/attribute/actorbaseattributess2c_attribute_sync.cpp:69,cpp/libs/services/scene/generated/attribute/attributedelta2framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta5framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta10framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta30framess2c_attribute_sync.cpp,cpp/libs/services/scene/generated/attribute/attributedelta60framess2c_attribute_sync.cpp

成立。git diff 6281e4d27 显示,本批在模板第 54-56 行新增了无条件的 actorRegistry.remove\<DirtyMaskComp>(entity),同时删掉了入口的 dirtyMask.none() 早退;逐位 reset 是基线就有的写法,原样留了下来。

从拿到 dirtyMaskComp 指针到 remove 之间,只有对其他组件的 try_get(不发信号、不改组件池)、protobuf 的 set_/CopyFrom,以及纯局部的 hasDirtyField 赋值。中间没有提前 return,也没有任何代码再读 dirtyMask。所以逐位 reset 没有任何可观察效果,确实是死代码。

生成产物里 actorbaseattributess2c 有 4 处,其余 5 个文件各 1 处,共 9 处,与「6 个生成文件」的描述一致。现有 aoi_delivery_test 327-357 行已经固定了「消费后整体摘除、空壳也摘」的契约,删掉 reset 不会改变这些用例的结果。

#### W2-b5_types-06

- 原发现（low）：gate 把场景节点 registry 叫 sceneRegistry、节点句柄叫 sceneEntity,与新类型 SceneRegistry/SceneEntity 同名异义(F085 同类漏点)
- 复核（b5_types-g1）：**降级 / low**，gate 把 scene 节点 registry 叫 sceneRegistry、节点句柄叫 sceneEntity,与 tlsEcs.sceneRegistry 及 SceneEntity 类型同名异义(F085 同类漏点);两处类型化后残留的无用别名
- 位置：cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:97,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:98,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:392,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:482,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:485

问题存在。F085 的修复理由是避免与 tlsEcs.sceneRegistry 混淆,第 4 批 4c-2 只点名了 client_message_processor.cpp。4c-4 同批改过 scene_entry_dispatch.cpp,却留下两处 `auto &sceneRegistry = ...GetRegistry(SceneNodeService)`,全仓只剩这两处。B3 之后 SceneRegistry/SceneEntity 专指场景实例,这里同名表示节点 registry 和节点句柄,确实容易误读。需要更正:(1) 第 98 行的无用别名不是本批(b5)改的,是第 4 批 4c-4 把 `const entt::entity sceneEntity{sceneNodeEntityId};` 改成了 `const auto sceneEntity = sceneNodeEntityId;`。(2) 原发现漏了 client_message_processor.cpp:485 一模一样的无用别名 `const auto sceneEntity = sceneEntityId;`,那里同样把节点句柄叫 sceneEntity。(3) 与 W1-28 部分重叠:W1-28 已要求把 ForwardPlayerToScene 的参数 sceneNodeEntityId 改名为 sceneNode,并修 scene_node= 日志,但它没覆盖局部 registry 变量名,也没覆盖这两处别名。所以本条不判重复,与 W1-28 一起实施。W2-fit-09 与本条同根,以本条为主条目。

#### W2-b5_types-07

- 原发现（low）：B1 可选的算术类型组件护栏未落地,也没有记录取舍,F032/F099 没有任何机械防复发手段
- 未复核（low 且低置信度）。原发现：任务单把护栏列为可选,并且写了撤掉的条件:编译时发现 EnTT 内部会实例化算术类型。Codex 没有编译,也就没有出现撤掉的理由,却直接没做,§9/§10/§11 回执里也没有说明。AGENTS §11 要求偏离要记录原因。现在仓库里没有 check_ecs_usage 一类的脚本,唯一的防线只是 ecs-component-access-rules.md:95 的一句文字规则,下一个人写 emplace\<uint64_t> 照样能编过,并且和其他 uint64 组件共用一个池。EnTT 3.13.2 的 component_traits(component.hpp:43)是官方的定制点。这是编译期约束,不属于运行期兜底。

#### W2-cross_sem-01

- 原发现（medium）：Agones 单元计数这个必需步骤仍经 dispatcher 一对一转发,还为此在两个入口各留了一套“监听者删了场景”的回滚分支
- 复核（cross_sem-g2）：**成立 / medium**，Agones 单元计数这个必需步骤仍经 dispatcher 一对一转发(唯一监听者只调 OnUnitCreated/OnUnitDestroyed),两个 CreateScene 入口还为不存在的“监听者删场景”各留一套不可达的回滚
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:533,cpp/nodes/scene/handler/rpc/scene_handler.cpp:609,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:56,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:139,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:127,cpp/libs/modules/scene/scene_events.h:6,cpp/AGENTS.md:69,docs/design/agones-scene-node-high-density.md:135

逐点核实成立。① 全仓 sink\<OnSceneCreated>/sink\<OnSceneDestroyed> 只在 scene_local_events.cpp:174-175 连接一次,cpp/tests 里没有任何引用;AoiSystem::OnSceneCreatedHandler 已不存在,事件只剩一对一转发。② 唯一的监听者只调 GameServerLifecycle(内部加 mutex 维护 activeUnits_ 集合),不碰 sceneRegistry,因此 scene_handler.cpp:537-558 和 scene_node_service.cpp:60-79 的“trigger 后重取 SceneInfoComp、丢了就补发 OnSceneDestroyed 并 destroy”是不可达的死分支;注释里的“AOI 格子表”已经过时。③ 计数是必需记账:漏掉 OnUnitCreated 时,createPermit 析构会走 --pendingPermits_ → RequestReadyIfIdleLocked,带着活场景回到 Ready,Agones 可能重新分配或缩容这个进程。生产 scene/main.cpp:108 在服务对外之前已调用 RegisterAll,所以目前没有丢,属于“违规且有后果”的潜在缺陷,medium 合适,与 W1-16/W1-22 同口径。④ battle 的计数是直接挂钩子(battle/main.cpp:291-293),scene 这边并不需要事件。与已有条目的关系:W1-36 只处理了通知后的复核和回滚,结论是“改用监听者契约收口、删掉 CreateScene 回滚”,保留了事件;本条的根因是事件本身不该承担必需步骤,方向不同,所以单独成立,并会让 W1-36 的修法失去对象。W1-30 已包含“AOI 格子表”注释,W1-19 已包含 cpp/AGENTS.md:69 对已删 SceneEventHandler 的引用。W2-fit-03 与本条同根,按字典序应判为本条的重复。

#### W2-cross_sem-02

- 原发现（medium）：任务链自动发奖、自动接后续、完成条件和战斗击杀进度这些必需步骤仍靠 dispatcher 的唯一监听者完成;漏注册时静默丢失,战斗结算还会把这一局永久记成“已应用”
- 复核（cross_sem-g1）：**降级 / medium**，任务链的自动发奖、自动接后续、“完成任务 X”推进和战斗击杀进度都是必需步骤，现在仍经 dispatcher 转给 nodes 层的唯一监听者：services/modules 层离开 scene 组合根就不完整，测试只能把 nodes 文件编进来，监听入口缺持久组件时还会静默返回
- 位置：cpp/libs/modules/mission/system/mission.cpp:343-414, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:24-50, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:79-80, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:153-165, cpp/libs/services/scene/battle/system/player_battle.cpp:1834-1844, cpp/libs/services/scene/player/system/player_mission.cpp:137-184, cpp/tests/missions_test/missions_test.cpp:33, cpp/tests/bag_test/player_mission_system_test.cpp:87

核心结论成立，但有两处描述需要更正，原修法也需要修正。

成立的部分：OnMissionCompletion 收集到的三类待办，只靠 dispatcher 交给 scene_local_events.cpp 里的唯一监听者（RegisterMission 是这三个 sink 的唯一注册点），而且都是业务必需步骤，不符合“dispatcher 只做可有可无的一对多通知”的判据。由此带来三点实际后果：

1. 依赖方向反了。测 libs/modules 的 missions_test 和测 services 的 bag_test 都要把 nodes/scene/handler/local_event/scene_local_events.cpp 编进去（它还会连带拉进 agones、属性和宠物系统），否则任务链不生效。

2. player_battle_settlement_test 只能挂一个 ConditionEvent 观察 sink 来验证击杀，并借这个 sink 人造重入。

3. 升级这一路（scene_local_events.cpp:79-80 → ConditionEventHandler）没有任何预检，MissionsContainerComp 或 kPlayerMission scope 缺失时会静默 return，不记 ERROR，违反“持久数据缺失要 fail-closed”的规则。

需要更正的描述：

- “漏注册时静默丢失、战斗把这一局永久记成已应用”在生产路径上走不到。scene/main.cpp:108 在任何流量进来之前就执行了 RegisterAll；ApplySettlementToEntity 在任何副作用之前已经用 IsSettlementApplicable 挡住了冻结、交接和退出（1702-1708），又用 all_of\<MissionsContainerComp> 做了预检（1714-1723），所以监听者里的冻结和缺组件早退从战斗路径到不了。只有不装订阅的宿主（测试、今后的新宿主）才会出现原文描述的丢失。

- 这个模式在基线就有（当时是生成的 mission_event_handler.cpp），第 4 批只是把 proto 事件改成了结构体。S5 写在 ecs-fit-overview.md 里，而 README 第 26 行把该文档标为“参考”，没有分配给任何批次，所以这不算任务单未落实，而是“违反 ECS 判据并且有后果”，按口径定为 medium，与同模式的 W1-22 一致。

原修法有三处需要修正：

① OnAcceptedMissionEvent 零监听的问题已经在 W1-23 里，本条不重复处理。

② 不能删除 AcceptMissionEvent 和 ConditionEvent 结构体。它们还是 MissionSystem::AcceptMission、CheckMissionAcceptance、HandleConditionEvent 以及 PlayerMissionSystem::Accept 的参数类型，真正只作事件用的只有 OnMissionAwardEvent。

③ 不要加深度计数。CheckAccept（player_mission.cpp:101-102）会拒绝已接取和已完成的任务，而完成位在派发待办之前就已置上，所以每个任务对同一玩家至多完成一次，链长天然以任务数为上界。ecs-fit-overview §8 的挑战意见也已经指出深度计数在经过 Accept 时会归零、根本不起作用。

与 W1-22 的关系：同一模式里的升级重算部分在 W1-22，但 W1-22 的修法写的是“保留 ConditionEvent(kConditionLevelUp) 作为一对多通知”。ConditionEvent 实际只有一个必需的监听者，这句与本条修法冲突，需要负责人拍板，见 owner_decision。

#### W2-cross_sem-03

- 原发现（low）：白名单落地后,生成器仍为非 Kafka 入口的事件生成并注册 dispatcher handler 空壳,这些订阅永远不会触发
- 复核（cross_sem-g3）：**成立 / low**，白名单落地后，生成器仍为非 Kafka 入口的事件生成 handler 并在 gate/scene 注册 dispatcher 订阅，这些订阅永远不会触发
- 位置：tools/proto_generator/protogen/internal/generator/cpp/event.go:206,tools/proto_generator/protogen/internal/generator/cpp/event.go:233,tools/proto_generator/protogen/internal/generator/cpp/event.go:246,tools/proto_generator/protogen/internal/generator/cpp/event.go:321,tools/proto_generator/protogen/internal/generator/cpp/event.go:336,tools/proto_generator/protogen/internal/generator/cpp/event.go:344-352,cpp/nodes/gate/handler/event/gate_event_handler.cpp:30,cpp/nodes/gate/handler/event/gate_event_handler.cpp:133-137,cpp/nodes/gate/handler/event/match_event_handler.cpp:8-9,cpp/nodes/gate/handler/event/player_event_handler.cpp:8,cpp/nodes/gate/handler/event/event_handler.cpp:12-13,cpp/nodes/scene/handler/event/player_migration_event_handler.cpp:8-9,cpp/nodes/scene/handler/event/event_handler.cpp:14

尝试推翻未果。(1) 进程内没有触发点：在 cpp/libs 和 cpp/nodes 下 grep PlayerDisconnectedEvent、PlayerLifecycleCommand、BattleResultEvent、PlayerMigration(Ack)Event，除 handler 文件本身外，只有 battle 节点把 BattleResultEvent 发往 match-results topic，没有任何 dispatcher.trigger。(2) Kafka 入口只按 OptionKafkaIngress 白名单派发（scene 3+1、gate 9），上述事件都不在名单内。所以这 6 个 sink 订阅永远不触发；match_event_handler 和 player_event_handler 两个文件里一个入口事件都没有，整类是死代码。(3) 根因在生成器：generateEventHandlerFiles 对文件内每个 message 都生成 handler，没有用 ReadAllProtoEvents 已经算好的 ProtoEventInfo.Ingress 过滤；GenerateEventHandlersTemplate 也按文件无条件写 Register。(4) 需要更正的一点：第 4 批任务单 §381 明确要求“event_handler.cpp 汇总只剩 battle、team、player_migration 三个类”，§98-101 也只决定“不标注”这些事件，没有要求生成器过滤，也没有要求删除 player_migration_event.proto。所以这不是 Codex 没落实任务单，而是白名单落地后留下的死订阅，读代码的人会以为这些事件能到达。只影响可读性，没有错误行为，维持 low。(5) 与 W1-16 的关系：W1-16 修法第 4 步“Register 列表跳过 Ingress != None 的事件”，如果单独执行，留下的恰好就是本条这批死订阅。两条要一起改。

#### W2-cross_sem-04

- 原发现（low）：入场落位对持久组件 Transform 用 get_or_emplace,违反“持久数据绝不延迟创建”
- 复核（cross_sem-g2）：**重复 / low**，入场落位对持久组件 Transform 用 get_or_emplace,违反“持久数据绝不延迟创建”;生产上到不了补建,属规则违例
- 位置：cpp/libs/services/scene/spatial/system/scene_spawn.cpp:101,cpp/libs/services/scene/spatial/system/scene_spawn.h:38,cpp/libs/services/scene/player/system/player_scene.cpp:216,cpp/libs/services/scene/player/system/player_scene.cpp:281

规则违例成立。batch1-verify.md:15 把 Transform 列为 15 个持久组件之一;全仓 emplace\<Transform> 只有 player_database_loader.cpp:113 一处;按 ecs-component-access-rules.md 硬规则 4,持久组件不得延迟创建。实际后果到不了:EnsureValidEnterLocation 的唯一生产调用点是 HandleEnterScene(player_scene.cpp:281),而 HandleEnterScene 的两个调用方(player_lifecycle.cpp:2729、s2s_player_scene_handler.cpp:46)处理的都是经过 InitPlayerFromAllData→Unmarshal 的玩家,Transform 一定存在,get_or_emplace 从不真正建组件。所以定 low,不定 medium,b1_runtime-08 的 medium 也应按 low 计。根因与 W2-b1_runtime-08 相同(任务已列为已知重叠),按编号字典序 b1_runtime-08 为主条目。补充一个对方修法没考虑的点:EnsureValidEnterLocation 的 bool 契约是“位置是否被改写”(scene_spawn.h:38),不是成功或失败;HandleEnterScene 还忽略这个返回值。只在函数内 try_get 后 return false,玩家会在没有 Transform 的情况下继续完成入场(绑定 SceneEntityComp、下发 EnterSceneS2C/ActorCreate),这违反硬规则 5“校验前置”。

#### W2-cross_sem-05

- 原发现（low）：BuildBattleSnapshot 新加了 all_of 前置校验,后面却原样保留“缺组件就给默认值”的分支,注释也改成了“已确认存在”
- 复核（cross_sem-g1）：**成立 / low**，BuildBattleSnapshot 在 all_of 前置校验之后，仍保留 7 处“缺组件就给默认值或跳过”的判空分支，注释也写成了“已确认存在”；结算路径和属性系统里有同样的死分支
- 位置：cpp/libs/services/scene/battle/system/player_battle.cpp:925-933, cpp/libs/services/scene/battle/system/player_battle.cpp:945-956, cpp/libs/services/scene/battle/system/player_battle.cpp:966-986, cpp/libs/services/scene/battle/system/player_battle.cpp:999, cpp/libs/services/scene/battle/system/player_battle.cpp:1097, cpp/libs/services/scene/battle/system/player_battle.cpp:1770-1792, cpp/libs/services/scene/player/system/player_attribute.cpp:68-76

成立。git diff 6281e4d27 显示，all_of 预检和“组件已在前置校验中确认存在”这两段注释都是 Codex 加的，但原来的 nullptr 三元式和 if(try_get) 包裹原样留着。我核对了预检和这些读取点之间的调用：ResolveGateRoute（884-903）只读 PlayerSessionSnapshotComp 和 gate registry，PetSystem::BuildBattleSnapshot 也只读，期间没有任何结构性改动，所以这些分支确实走不到。它们留在这里会让读者以为缺组件还能拼出快照，与 925-926 的注释正好相反。

同类位置还有两处：

- 同一文件 ApplySettlementToEntity 的 1770、1777、1790：1715 已经用 all_of\<DerivedAttributesComp> 预检过，而这三处读取发生在宠物、物品和任务副作用之前，所以 derived 判空同样是死分支。

- player_attribute.cpp 的 PlayerLevel 和 PlayerClassId：它们只在匿名命名空间里使用，调用方是 TotalPoints、DimensionValue、ResetCostFor、Recalculate、BuildPanel、Allocate、Reset、AutoAllocate，这些路径全部先经过 CheckAttributeInputs（或 CheckWritable）。我已逐个核对第 679、696、806、892、934、976 行，所以“缺 LevelComp 返回 1”这条分支也走不到。

整体是可读性问题，没有运行期错误，定为 low。

#### W2-cross_sem-06

- 原发现（low）：空的 TRANSFER_ERROR_MESSAGE 过渡宏已无任何调用方,该删没删,注释还说“生成头仍有调用”
- 复核（cross_sem-g1）：**重复 / medium**，空操作宏 TRANSFER_ERROR_MESSAGE 已经没有任何调用方，但第 2 批 C2 规定的删除没做，过渡注释“生成头仍有调用”也已失实
- 位置：cpp/libs/engine/core/macros/return_define.h:66-68

问题成立。我在 cpp/ 和 tools/ 下把工作区与已跟踪文件都查了一遍，宏只剩定义本身，11 个生成的 player_\*_handler.h 已经重生，不再调用它。batch2-singletons.md §4.16 给 C2 设的前置门是“git grep 只剩 return_define.h 自身”，这个条件现在已经满足，但删除没做；README §7 恢复步骤 1 也把“删除空 TRANSFER 宏”列为待办。按严重度口径，“任务单要求未落实”属于 medium。这个宏也不在 AGENTS §11.2 允许的四类宏之内。它与 W2-b2-02 同根，且 b2-02 编号字典序更小，所以本条判为重复。docs/design/player-features-ui.md:22、muduo-rpc-channel.md:259 和 robot 注释里仍在讲旧规则，这些分别由 W2-b2-06 和 W2-cross_sem-07 覆盖，不在本条范围内。

#### W2-cross_sem-07

- 原发现（low）：robot 冒烟场景的注释和失败信息仍在教旧规则“拒绝码必须写 TLS tip”,与第 2 批后“handler 直接写 response”相反
- 复核（cross_sem-g3）：**成立 / low**，robot 冒烟场景的注释和失败文案仍按旧机制写（“拒绝码必须写 TLS tip”“TRANSFER 宏总会物化 error_message”），与第 2 批之后“handler 直接写 response、成功不创建该字段”的规则相反
- 位置：robot/travel_smoke_scenario.go:255-256,robot/travel_smoke_scenario.go:911-912,robot/travel_smoke_wire.go:37-38,robot/channel_smoke_scenario.go:429-432

尝试推翻未果。第 2 批之后，TLS/全局 tip 已删除（cpp 下已无 SetTip 的定义和调用，只剩 player_gm_guard.h:66 的一处陈旧注释，归 W2-b2-06）。TRANSFER_ERROR_MESSAGE 是空操作，生成头也不再调用（git grep 只剩 return_define.h 自己）。player_service_interface.h 规定 handler 直接写 response，成功时不创建 error_message；TravelToZone 也确实这样写。所以 travel_smoke 的注释和失败文案方向正好相反：真出现 tip=0 时，它会让排障的人去找一个已不存在的 TLS tip 机制，并认为“直接写 response”是错的。channel_smoke 里“每次应答都物化空 error_message”的前提也已不成立；不转发 63 号应答还有另一个仍然成立的理由，即 gate 信封拒绝时 body 为空，通用 handler 会误打 “enter scene accepted”，所以代码行为不用改。另外，batch2-singletons.md §304 要求在第 2 批的 PROGRESS 条目里留下这三处 robot 注释的后续清理提示，但 PROGRESS.md:8282 的第 2 批条目里没有。与 W2-b2-06 同属第 2 批遗留的陈旧注释，但那条的修法清单不含 robot（Go 工程），两者没有重叠，所以单列。只影响注释和诊断文案，维持 low。

#### W2-cross_sem-08

- 原发现（low）：新增了 PlayerSceneSystem::FindSceneById,但 scene_id 全表扫描仍在 4 个场景入口各手写一份(F095 未收口);s2s 入场还把 CheckEnterScene 做了两遍
- 复核（cross_sem-g2）：**重复 / low**，已有 FindSceneById,两个场景入口的 CreateScene 去重和 DestroyScene 仍各手写一份 scene_id 全表扫描(F095 未收口);s2s 两次 CheckEnterScene 不算缺陷
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:497,cpp/nodes/scene/handler/rpc/scene_handler.cpp:582,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:24,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:95,cpp/libs/services/scene/player/system/player_scene.cpp:182

DRY 问题成立:git grep 的结果与描述一致,生产代码里有 1 个具名函数,外加 4 份手写扫描。根因与 W2-b5_types-02 相同(任务已列为已知重叠),并与 W2-fit-02(场景目录、单一创建入口)相近;按字典序 b5_types-02 为主条目。s2s 那个子论点不成立:SceneScenePlayerHandler::EnterScene 先做 CheckEnterScene,是为了拿到具体的 tip 码,因为 HandleEnterScene 只返回 bool。HandleEnterScene 内部的 CheckEnterScene 是它自身契约的守卫,还要服务 player_lifecycle.cpp:2729 这第二个调用方,符合 AGENTS §11.3“预检 + 提交点复核”的写法。两次检查之间只有 EntryOwnershipGuard 的构造和读取,没有任何通知或写操作,而 CheckEnterScene 是纯函数,所以第二次不可能在第一次通过后失败,“两次失败回的 tip 不一样”在任何输入下都观察不到。b5_types-02 的修法写“入口原有的 0 校验保留即可”,但 gRPC 的 HandleCreateScene 根本没有 scene_id==0 校验,只有 muduo 的 SceneHandler::CreateScene:472-477 有。HandleCreateScene 换成 FindSceneById 之后(它对 0 返回 null),同一个 scene_id=0 重复创建会从“返回已有场景”变成“再建一个”。

#### W2-cross_sem-09

- 原发现（low）：InitializeOnCreate 返回 [[nodiscard]] bool 只是为了拒绝无效句柄;唯一的生产调用点刚确认过有效,而失败分支既不清 playerList 也不撤会话
- 复核（cross_sem-g2）：**重复 / low**，InitializeOnCreate 的 bool 返回值到不了 false,4137 的整条 if(含 canContinueInitialization)都是死分支,失败侧也没有撤销清理
- 位置：cpp/libs/services/scene/player/system/player_lifecycle.cpp:4137,cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.cpp:119,cpp/libs/services/scene/actor/attribute/system/actor_attribute_calculator.h:18,cpp/tests/bag_test/player_feature_persistence_test.cpp:925,cpp/tests/bag_test/player_feature_persistence_test.cpp:1013

按当前工作区核对,问题成立。4094 发布到 playerList 之后到 4137 之间只有 emplace 和 Unmarshal。player_database_loader.cpp、player_database_1_loader.cpp 里没有任何 dispatcher/trigger;全仓的 EnTT 信号只有 actor_net_id.cpp:29 的 on_destroy\<ActorNetIdComp>,没有 on_construct 挂在玩家组件上。此时 notificationOwnership 为空、实体有效、GetPlayer(playerId)==player,所以 canContinueInitialization() 恒为真。InitializeOnCreate 只在 !valid 时返回 false,这一侧同样到不了。万一走到,return entt::null 不会摘 playerList,也不调 RefuseInheritedEntry,只收拾了一半。不过分支不可达,定 low 合适。W2-b1_runtime-09 已经覆盖了同一根因:返回值不可达、4137 的 canContinueInitialization() 恒真、失败侧不清理(其证据与推理逐条对应)。按编号字典序 b1_runtime-09 \< cross_sem-09,本条判重复。本条唯一多出来的是测试侧的善后,见 fix。

#### W2-cross_sem-10

- 原发现（low）：任务列表读路径在持久组件 MissionsContainerComp 缺失时仍回成功,并按配置表下发一整份默认状态,与规则“面板 / 列表缺持久输入返回故障码”冲突
- 复核（cross_sem-g1）：**成立 / low**，任务列表读路径在持久组件 MissionsContainerComp 缺失时仍返回 kSuccess，并下发整份“全部未接取”的配置目录；现有测试还把这个行为固化成了契约，与“面板 / 列表缺持久输入返回故障码”的规则冲突
- 位置：cpp/libs/services/scene/player/system/player_feature_snapshot.cpp:187-213, cpp/tests/bag_test/player_feature_snapshot_test.cpp:152-166, docs/design/player-features-ui.md:39

成立。MissionsContainerComp 是持久组件：玩家加载时 player_database_loader.cpp:138 会无条件调用 mission_marshal::Unmarshal，后者先 get_or_emplace 这个组件，结尾再调 PlayerMissionSystem::Initialize，保证 kPlayerMission scope 存在。所以对已加载的玩家来说，组件缺失只能说明加载已经坏了。此时 BuildList 仍返回 kSuccess，下发整份配置目录且全部是 NOT_ACCEPTED，只多一个 state_persistent=false；而 Accept 和 ClaimReward 会经 CheckPlayer（player_mission.cpp:33）返回 kServiceUnavailable，读和写的结论前后不一致。第 1 批已经把宠物列表和属性面板改成缺组件返回 kServiceUnavailable，batch1-verify.md:398 称 player_feature_snapshot.cpp 早已是同一口径，但对任务列表并不成立；git diff 6281e4d27 显示这个文件自基线以来没有改过。

另外两点：

- 现有用例 MissionCatalogueDoesNotAcceptOrInitializePlayerState 正好把旧行为固定成了契约，player-features-ui.md:39 的“任务返回 state_persistent=false”也停留在任务尚未持久化的时期，两者都要跟着改。

- 同文件的 PlayerActivityReadSystem::BuildList（216-243）也会经 CheckAccept 读取任务状态，缺组件时同样返回成功，只把 can_participate 置成 false 并给出泛化原因，属于同类遗漏。

这个问题只在加载损坏时才会触发，后果是误导性的展示，不会写坏数据，所以定为 low。

#### W2-cross_sem-11

- 原发现（low）：审计文档的“已修”和“注意事项”与最终代码矛盾:F044/F050 写的是“actor 初始化时挂上”,而现行规则正好相反
- 复核（cross_sem-g3）：**降级 / low**，审计文档作为“修 entt 前先看这里”的入口，仍把 F044/F050 写成“现在 actor 初始化时挂上”，注意事项还在教已被删除的 CQ/Stub 排空修法，并把 etcd 写成定时器轮询 CQ
- 位置：docs/design/ecs-entt-misuse-audit-20261015.md:70,docs/design/ecs-entt-misuse-audit-20261015.md:133,docs/design/ecs-entt-misuse-audit-20261015.md:80,docs/design/ecs-entt-misuse-audit-20261015.md:90,docs/design/ecs-entt-misuse-audit-20261015.md:55-58

问题存在，但原描述要更正。(1) 第 70 行在 ebccaf04d 时是准确的历史记录。git grep 显示，当时 actor_event_handler.cpp:32-33 的 InitializeActorComps 确实预挂了这两个组件。第 1 批之后改为按需创建：skill_comp.h:16-17、skill.cpp 中 StoreSkillContextForWrite 与 StartCooldown 的 get_or_emplace，规则文档“第 1 批核对后的具体创建入口”表也明确“不在 actor 创建时预挂”，player_feature_persistence_test.cpp:134 的 ExpectLazyActorStateAbsent 守着这一点。(2) 原发现说这几行“不是状态未更新，而是给出相反修法”，说法过头。第 70 行和第 133 行恰恰就是状态没更新：文档第 5 行已声明 HEAD 状态待审查后统一更新，review-handoff §113 第 3 步也把它列为本轮交付物。它们在“已修”表里，没人会照着去“修”。不过文档自称“修 entt 相关问题前先看这里”，又用了“现在”这个词，读者会把预挂当作现行做法。(3) 真正会误导人去动手的是第 80 行：注意事项要求 TryCancel、Shutdown CQ 并排空，再把响应缓冲挪进 Async\*GrpcClient。CQ、Stub 和流式调用组件已经随 gRPC→muduo 第 3 批删除，§8.1 中 W1-41/42/43 已判为“已被改写”。第 90 行“etcd 的 CQ 由定时器轮询”也已不成立：GrpcEtcdTransport 已删除，现在走 MuduoEtcdTransport。第 55-58 行“仍未修”表里的 F010/F100/F012/F022 也还按 CQ/Stub 描述。纯文档问题，没有运行期后果，维持 low。

#### W2-fit-01

- 原发现（medium）：任务链（自动发奖、接续任务、推进完成任务条件）仍经 dispatcher 一对一转发，S5 没做；modules 层借事件反调 services 层，模块单测要编译 nodes 文件才能跑通
- 复核（fit-g1）：**重复 / medium**，任务链的三个必需步骤（自动发奖、接续任务、推进「完成任务 X」条件）和战斗结算的击杀进度，仍由 libs/modules 的 MissionSystem 经 dispatcher 交给 nodes/scene 里的唯一监听者完成：漏注册时静默丢失，modules 层反向调用 services 层，任务和背包单测工程因此要编译 nodes/scene 的源文件
- 位置：cpp/libs/modules/mission/system/mission.cpp:343-414, cpp/nodes/scene/handler/local_event/scene_local_events.cpp:23-49,79-80,153-165, cpp/libs/services/scene/battle/system/player_battle.cpp:1833-1844, cpp/tests/missions_test/missions.vcxproj:160, cpp/tests/bag_test/bag_test.vcxproj:185

与 W2-cross_sem-02 同根，都是任务链和击杀进度这些必需步骤经 dispatcher 交给唯一监听者。按编号字典序，W2-cross_sem-02 为主条目。第一波的 W1-22（沉默状态和 GM 重算）、W1-23（零监听事件）都不覆盖这条任务链；其中零监听的 OnAcceptedMissionEvent（mission.cpp:107-110）归 W1-23。

独立复核结论：

- 三个事件的生产 sink 只有 scene_local_events.cpp 里 RegisterMission 那一组，经 scene/main.cpp:108 的 RegisterAll 注册。MissionFollowUps、ApplyFollowUps、kMaxMissionFollowUpDepth 全仓零命中。

- missions_test.cpp:33 和 player_mission_system_test.cpp:87/90 必须手工调用 RegisterMission，并在 vcxproj 中编译 nodes 层的源文件，分层倒置属实。

- player_battle.cpp:1846-1849 的注释为「中途派发的任务事件等若销毁了别的实体」重取账本指针，也是这条 dispatcher 链带来的防御。

- 生产路径注册齐全，到不了丢失的情形，与 W1-16、W1-22 同口径定为 medium。

需要更正原文的一点：ecs-fit-overview 是「参考」文档，S5 不在任何 Codex 批次的任务单里，所以「S5 没做」不算任务单未落实。问题本身按审查基准「必需步骤直接调用」成立。

#### W2-fit-02

- 原发现（medium）：S2a 场景目录没做：scene_id→场景实体仍是 5 份全表扫描，其中 FindSceneById 是本次新加的一份；创建与销毁仍在两个入口各写一份，注释要求「手工保持同步」
- 复核（fit-g2）：**降级 / low**，场景创建和销毁的业务逻辑在 muduo 与 gRPC 两个入口各写一份（去重、字段组装、排空、销毁，第 1 批又各加一份回滚），只靠「Keep in sync」注释同步，前置校验已经出现分叉。scene_id 扫描部分见 W2-b5_types-02
- 位置：cpp/nodes/scene/handler/rpc/scene_handler.cpp:471-476,cpp/nodes/scene/handler/rpc/scene_handler.cpp:496-559,cpp/nodes/scene/handler/rpc/scene_handler.cpp:569-616,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:19-87,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:90-144,cpp/libs/services/scene/player/system/player_scene.cpp:182-189

成立的部分：场景实体只在这两个入口 create / destroy。两份逻辑逐字重复，靠注释维持同步。第 1 批又在两边各加了约 20 行回滚，重复量翻倍。分叉已经出现：muduo 入口校验 config_id 和 scene_id 非零（:471），SceneManager 实际走的 gRPC 入口没有这条校验。好在 Go 侧 scene_node_client.go:102 在上游拒绝了 scene_id=0，所以目前没有可达缺陷。

降级与更正：

(1)「S2a 没做」不是任务单违例。S2a 属于 ecs-fit-overview，README:26 把这份总评标为「参考」，不在第 2–5 批之内。F095 在审计里本来就是 low。

(2)「FindSceneById 是本次新加的第 5 份扫描」不对。6281e4d27 时已有 5 份扫描：player_lifecycle.cpp:2640、scene_node_service.cpp 两处、scene_handler.cpp 两处。s2s 入场当时还在用 entt::to_entity(scene_id)（F091）。Codex 把 player_lifecycle 那份收进 FindSceneById，并让 s2s 改用它，扫描份数没有增加，还修掉了 F091。

(3)「入场热路径」：基线里入场同样要扫描。场景数是每节点几十到几百的量级，没有 profiling 数据。按 AGENTS §11.3，不应为此加 ctx 索引。索引还要靠 on_destroy 信号保持同步，等于第二份事实源加一层防护，正是负责人要避免的写法。

(4) 两个入口手写的 4 份扫描、没复用 FindSceneById，与 W2-b5_types-02 同根，归那一条。回滚块与 W2-cross_sem-01、W1-36 同根。

本条独有、其他条目都没覆盖的，只有「两个传输入口各写一份场景生命周期业务」这一点（审计 N6）。它属于 DRY 和可维护性问题，存在潜在分叉，目前没有可达缺陷，定为 low。

#### W2-fit-03

- 原发现（medium）：场景的 Agones 单元计数是必需记账，却经 OnSceneCreated / OnSceneDestroyed 一对一转发；两个 CreateScene 入口为「监听者删掉 SceneInfoComp」写了回滚，而这种监听者并不存在，注释还说监听者包括早已不监听的 AOI 格子表
- 复核（fit-g2）：**重复 / medium**，场景的 Agones 单元计数是必需记账，却经 OnSceneCreated / OnSceneDestroyed 由唯一监听者转发。两个 CreateScene 入口还为一个并不存在的「删掉 SceneInfoComp 的监听者」各留了一段生产不可达的回滚
- 位置：cpp/nodes/scene/handler/local_event/scene_local_events.cpp:127-137,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:174-175,cpp/nodes/scene/handler/local_event/scene_local_events.cpp:187-188,cpp/nodes/scene/handler/rpc/scene_handler.cpp:533-559,cpp/nodes/scene/handler/rpc/scene_handler.cpp:610-612,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:56-80,cpp/nodes/scene/handler/grpc/scene_node_service.cpp:139-141,cpp/libs/modules/scene/scene_events.h:6-7,go/scene_manager/internal/logic/scene_node_client.go:119-120

事实成立：

- 全仓只有 scene_local_events.cpp:174-175 连接这两个 sink，函数体只做 Agones 计数。

- AOI 格子表在 aoi.cpp:72 按需创建，不订阅场景创建，所以「AOI 格子表」的注释已过时。

- 计数是必需步骤：漏记时，allocation permit 结束会因零单元回到 Ready（agones_gameserver_lifecycle.cpp:296-298），有场景的进程会被当作空闲。

- 回滚分支只有在监听者删掉 SceneInfoComp 时才会走到，生产中不存在这样的监听者，属于不可达代码。

- 场景实体只在这两个入口 create / destroy（全仓 grep 确认）。

本条与 W2-cross_sem-01 是同一条（编排方已标注），按字典序 cross_sem-01 为主。回滚部分另与第一波 W1-36、「AOI 格子表」注释与 W1-30 同根。

更正：

(1)「同一个计数器收两种口径的 key」不成立。scene 与 battle 是两个可执行程序（cpp/nodes/scene/main.cpp、cpp/nodes/battle/main.cpp），各有自己的 GameServerLifecycle 单例，计数器并不共用。实体整数含 version，在一个进程内对每个存活场景唯一，作为 key 没有正确性问题；改用 scene_id 只是可选的日志友好化。

(2) W1-36 的修法是「保留事件、用监听者契约收口」，与本条「删除事件、直接调用」冲突。按 dispatcher 只做可选通知的判据和负责人「直接调用优先」的偏好，应以本条为准。

(3) Go 侧 scene_node_client.go:119-120 的注释「C++ 创建监听者可能回滚实体并留下空应答」是为这段回滚写的，删回滚后要同步改注释；那个确认检查本身可以保留。

#### W2-fit-04

- 原发现（medium）：S4 只做了一半：目标侧镜像删了，但目标身上的 BuffEntry 仍持有施法者 SkillContextCompMap 里同一个 shared_ptr，跨实体共享所有权仍在；BuffSource 值类型没做
- 复核（fit-g2）：**降级 / low**，目标身上的 BuffEntry 仍存着施法者 SkillContextCompMap 里同一个 shared_ptr，但 buff 侧只读它的 caster()，这个值 buffPb.caster 里已经有。问题是跨实体共享所有权和多余的 API 耦合，没有行为后果
- 位置：cpp/libs/services/scene/combat/buff/comp/buff_comp.h:12-18,cpp/libs/services/scene/combat/buff/system/buff.cpp:75-81,cpp/libs/services/scene/combat/buff/system/buff.cpp:214-227,cpp/libs/services/scene/combat/buff/system/buff.cpp:406-416,cpp/libs/services/scene/combat/buff/system/buff.cpp:677-689,cpp/libs/services/scene/combat/buff/system/buff_impl.cpp:131-134,cpp/libs/services/scene/combat/buff/system/buff.h:32,cpp/libs/services/scene/combat/skill/system/skill.cpp:724

事实成立，三处都核对过。第一，TriggerSkillEffect 把施法者表里的 shared_ptr 交给目标的 AddOrUpdateBuff，buff.cpp:227 把它存进目标的 BuffListComp。ApplyNextBasicAttackBonus 还会把它继续传给另一个实体身上的 target_sub_buff。第二，全仓对 BuffEntry.skillContext 和 abilityContext 的读取最终都只取 caster()：ProcessedCasterOf、set_caster、:414 的有无判断，以及子 buff 继承（再传进 AddOrUpdateBuff）。三个 OnBuffRefresh 都是空函数，modifier 和 motion 那两份全仓零调用。第三，AddTargetSkillContext / RemoveTargetSkillContext 零命中，镜像确实删了。

降级理由：

(1) S4 只出现在 ecs-fit-overview。README:26 把这份总评的状态标为「参考」，属于「第 2–5 批之外还该做的结构性改动」。真正派给 Codex 的是 batch5 §4.10 B5，只要求删除目标镜像，已完成。所以这不是「任务单没落实」。

(2) 没有行为后果。context 的 caster 在 skill.cpp:215 创建后不再改，buff 读到的值与 buffPb.caster 相同。后果只是一个小 proto 的寿命被延长到 buff 移除为止（无限时长 buff 要到实体销毁），属于设计异味和 ECS 适用判据违例（跨实体共享对象），够不上 medium 的「有后果」。

(3) 原推理有两处错误。其一，修复后 ecs_context.h:17-23 的 0 号实体退役仍然必需：原修法里 BuffSource::HasCaster() 自己就依赖它（挑战意见原话「依据 F048：0 号句柄已退役」），SkillContextComp.target 也仍是本地整数。其二，SkillContextPtrComp 别名还是 SkillContextCompMap 的值类型，skill.cpp 里也到处在用，buff 不是它存在的「唯一原因」。

(4) 原修法的 BuffSource 带 skillId / skillTableId 两个字段，全仓没有读者，违反 YAGNI。按负责人「优先删除」的偏好，直接传 uint64 caster 就够了。

附带发现：buff.cpp:66-72 ProcessedCasterOf 的注释说子 buff 链路「没有技能上下文」，实际上子 buff 继承了父的上下文，注释与实现不符。

#### W2-fit-05

- 原发现（medium）：S0 / S1 都没做：规范里没有「哪些数据进 registry」和「dispatcher 只做可选通知」，反而把「通知后重验句柄与 playerList 归属」写成了通用做法；ECS 用法棘轮脚本不存在
- 复核（fit-g3）：**重复 / low**，组件访问规则缺少「哪些数据进 registry」判据，ecs\*.md 里「单例组件」的说法含糊；:83「通知后重验」和缺失的 dispatcher 规则已并入 W1-23；S0/S1 只是标为「参考」、从未派发的设计稿，不算 Codex 漏做
- 位置：docs/design/ecs-component-access-rules.md:83, docs/design/ecs-component-access-rules.md:3-7, docs/design/ecs_zh.md:10, docs/design/ecs.md:10, docs/design/ecs_en.md:10, docs/design/ecs-entt-misuse-audit-20261015.md, docs/handoff/ecs-refactor/README.md:26

先核事实，都成立：① 在 docs/design、.github/copilot-instructions.md、cpp/AGENTS.md 里 grep「可有可无 / 必需步骤直接调用 / 哪些数据进 / 不进 registry / 没有消费者」，ECS 相关零命中；② 规则文档 :83 仍把「trigger 返回后重验完整句柄和 playerList 归属」写成通用做法；③ tools/scripts 下没有 check_ecs_usage.ps1，也没有 ecs_usage_baseline.json；④ 审计文档没有「适用性总评」，也没有 N1–N7。

但定性需要更正，理由有三。

(1)「S0/S1 都没做」不能算作任务单未落实。README §1 把 ecs-fit-overview 的状态标为「参考」；overview 开头写明它是设计稿，等 Claude 出第 2 版补丁级任务单；各批任务单和 Codex 回执（README §7、§8）都没有把 S0/S1 派给 Codex。overview §8 的 [high] 挑战意见还指出 S0/S1 依据的是旧版规范：S1 的 R2 把所有 get_or_emplace 计为违规，和现行「按需组件写入口用 get_or_emplace」直接冲突，原样落地本身就是错的。所以原严重度依据「任务单要求未落实」不成立。

(2) 真正有后果的部分已被 W1-23 覆盖：:83 把错误模式固化下来，又缺少「dispatcher 只做可有可无的通知」这条规则。W1-23 复核结论的修法第 (7) 条已经要求把 :83 改为「没有消费者不发事件；dispatcher 只做可有可无的一对多通知；监听者不得销毁或替换实体，不得改 SceneEntityComp、playerList 或会话；trigger 之后不写恢复分支」，:59 一并改写。W1-32 已并入 W1-23，W1-36 也点名了这处矛盾。两者同根，判重复。补一处来源：「trigger 之后要用就重新 try_get」最早出自第 5 批任务单 batch5-lifecycle-types.md:19。Codex 把它扩大成「重验完整句柄和 playerList 归属」，而且违背了同一任务单 §8 low 项「通知后不再 re-fetch、不提前 return」的要求。

(3) W1-23 没覆盖的只剩三项，都是文档或规划层面，不影响运行期，定 low：组件访问规则缺「哪些数据进 registry」判据；ecs\*.md 末句「单例组件不免除这些所有权约束」说法含糊，没说明 registry 内只允许哪种单例（实际代码里唯一的 ctx 单例是 ActorNetIdIndex，属于 registry 自己的派生索引，符合判据 (3)，并没有和第 2 批的结论相反，只是措辞没跟上）；审计文档缺适用性总评和 N1–N7。棘轮脚本做不做，属于负责人拍板。

#### W2-fit-06

- 原发现（medium）：节点表仍是 registry（S6 第 1 期、第 2 期都没做）：网络资源、握手定时器和 gate 握手状态都挂在节点实体上，所有查找都是对 view 的线性扫描，角色靠组件组合区分
- 复核（fit-g1）：**降级 / low**，节点表仍是「map + 动态属性袋」式 registry：查找靠线性扫描 view，出站/入站角色靠组件组合区分，握手定时器用通用 TimerTaskComp。这是 ecs-fit-overview 第 5 条中待拍板的存量架构债，不是本次交付缺陷，也没有可达故障
- 位置：cpp/libs/engine/core/node/system/node/node_util.cpp:52-95,175-210,213-243, cpp/libs/engine/core/node/system/node/node_connector.cpp:87-88, cpp/libs/engine/core/node/system/registration/registration_manager.cpp:95,142,197, cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:403-404

证据按函数名重新定位后属实，node_directory.\* 全仓不存在。但严重度和定性要更正：

(1) 这不是交付缺陷。README 把 ecs-fit-overview 标为「参考」，第 2–5 批的任务单都不含 S6。S6 第 1 期的前置条件是拍板 decisions 第 1 条和 muduo RPC 第 5 批，第 2 期归迁移会话。原发现自己也承认这不算 Codex 违规。

(2) 没有可达缺陷：

- 节点只有几十个，线性扫描没有任何性能数据；按 §11.3，性能问题要靠数据驱动。

- 节点实体上的 TimerTaskComp 全仓只有一个写入点（:95）和一个移除点（:197），「第二个模块挂定时器会撞池」只是假设。

- gate 的 SceneLinkHandshakeComp 只存 weak_ptr，符合 §11.7。

- gRPC 资源已经收敛为单个 MuduoGrpcChannelPtr，W1-42 已据此判「已被改写」。

(3) 原修法里「拍板前最低限度」的做法，是把 TimerTaskComp 包成具名的 NodeHandshakeRetryComp。这是为假想冲突再加一层类型，与「优先删、不加防护」的修法偏好相悖，不建议做。

因此降为 low，保留为需要负责人拍板的存量项。注意：registration_manager.cpp 和 scene_entry_dispatch.cpp 上有其他会话未提交的改动，行号以当前工作区为准。

#### W2-fit-07

- 原发现（medium）：传输层的连接回调仍经 ECS dispatcher 转给 Node（唯一监听者），engine 网络层因此依赖场景的线程局部 ECS 上下文；总评说要改成构造注入，后来没有任何会话接手
- 复核（fit-g1）：**降级 / medium**，出站连接 UP 后的节点握手（必需步骤，生产唯一监听者是 Node）仍由 RpcClient 经 tlsEcs.dispatcher 转发；传输对象因此 include 场景 ECS 头，Node 为这一个回调维护 scoped_connection 和析构顺序约束。ecs-fit-overview 定的「构造注入」没有任何会话接手
- 位置：cpp/libs/engine/core/network/rpc_client.h:16-17,258, cpp/libs/engine/core/network/rpc_connection_event.h:5, cpp/libs/engine/core/node/system/node/node.cpp:511,1305-1309,1560-1578, cpp/libs/engine/core/node/system/node/node.h:213,241,303-304, cpp/libs/engine/core/node/system/node/node_connector.cpp:142-157

成立的部分：rpc_client.h:258 是这个事件唯一的 trigger 点。生产代码里唯一的 sink 在 node.cpp:1308，由 Node::Initialize 第 511 行在 InitEtcdService 之前注册。OnServerConnected 只处理 UP，并对全部 nodeType 调 TryRegisterNodeSession，这正是握手这个必需步骤。按 EnTT 源码，sigh.hpp:316 的 operator=(connection) 只替换不断开，所以先 release 再赋值是对的。ecs-fit-overview 第 9 条写的是「传输回调改为构造注入，随 muduo RPC 第 3 批一起做」；batch5 §4.1 A1 只把它改成了 scoped_connection，还为此加了 node.h:303 的成员顺序约束；muduo-rpc-channel.md:378 至今仍把该事件写成现行机制，确实没有会话接手。这是「必需步骤经 dispatcher 交给唯一监听者」，与 W1-16、W1-22 同口径，维持 medium。

需要更正的地方：

(1) 「没注册 sink 的宿主会静默地永远不握手」在生产上到不了。RpcClient 的唯一生产构造点是 NodeConnector::ConnectToTcpNode，它直接用 gNode；而 Node::Initialize 总是在发现节点之前就注册了 sink。实际后果在分层和可读性上：传输对象 include ecs_context.h；Node 为一个回调维护订阅成员和析构顺序注释；两份 RPC 测试要往 tlsEcs.dispatcher 挂探针，见 rpc_client_lifecycle_test.cpp:105-154 和 rpc_channel_loopback_test.cpp:567-625、752。

(2) 「engine 网络层因此依赖 tlsEcs」只部分成立。player_message_utils.cpp 仍读 tlsEcs.actorRegistry，属于 S7 和长期目标；rpc_channel.cpp:20 只是多了一个没用到的 include。「gate 保留 EcsContext 的原因之一」这个理由偏弱：gate 还用 dispatcher 承接 Kafka 命令和 OnNodeRemoveEvent。

(3) 原修法不可照做：

- OnServerConnected 是 protected（node.h:241），NodeConnector 调不到。

- 原回归测试要经 NodeConnector 建连，但 ConnectToTcpNode 直接解引用 gNode（GetLoop、GetNodeReplyService），测试里没有 Node 根本跑不起来；而观察者里的 if (gNode) 会让测试修复后也不变绿。

- 删除 rpc_connection_event.h 会让上述两份测试的探针失效，原修法没提。

#### W2-fit-08

- 原发现（low）：节点 registry 的「全局实体」三件套已无生产调用方，仍被保留，还加了 Release 下的 FATAL 守卫和三条死亡测试；头部注释仍说 etcd 的 gRPC 状态挂在这个实体上
- 复核（fit-g1）：**重复 / low**，NodeContextManager 的全局实体三件套（Set/Get/GetOrCreateGlobalEntity）及 globalEntities_ 自 f0dadf5b0 删除 GrpcEtcdTransport 后已无生产调用方，却仍带着 FATAL 守卫和三条死亡测试；头部注释仍说 etcd 的 gRPC 状态挂在它上面
- 位置：cpp/libs/engine/thread_context/node_context_manager.h:11,13,16-24,53-72,78,92, cpp/tests/routing_identity_test/routing_identity_test.cpp:390-410, cpp/libs/engine/core/node/system/node/node.cpp:1248

与 W2-b2-01 同根，都是节点 registry 的全局实体设施零调用、该删没删。按编号字典序，W2-b2-01 为主条目。

独立复核结论：在当前工作区 git grep 里，GetGlobalEntity、GetOrCreateGlobalEntity、SetGlobalEntity 只剩定义和 routing_identity_test:405-410 的死亡测试；etcd 目录（MuduoEtcdTransport、etcd_service）已不碰任何 registry。

需要更正原文的归因。按提交历史，de4960bee、0249bee0a、b24a5b772 里 grpc_etcd_transport.cpp 仍有 5 处调用，最后一个生产调用方是在 f0dadf5b0 随 GrpcEtcdTransport 一起被迁移会话删掉的。所以第 1 批加 RequireValidNodeType 守卫和死亡测试、第 2 批把它当「合法 API 保留」时，它确实还有生产调用方，当时不构成「保留本该删的东西再加防护」。它是迁移之后才变成死代码的。

没有可达缺陷：节点 registry 的 view\<NodeInfo> 不会看到无组件的全局实体。风险只在于有人顺手再用它挂节点级单例，所以严重度是 low；主条目 W2-b2-01 标的 medium 也应降为 low。

#### W2-fit-09

- 原发现（low）：场景句柄分型之后，gate 仍把 scene 节点的 registry 叫 sceneRegistry、把节点实体叫 sceneEntity，和 scene 进程里的 SceneRegistry / SceneEntity 同名不同义；SceneEntityComp 的字段仍叫 sceneEntity
- 复核（fit-g2）：**重复 / low**，gate 把 scene 节点 registry 叫 sceneRegistry、把节点句柄叫 sceneEntity，与 scene 进程的 SceneRegistry / SceneEntity 同名不同义。节点句柄带类型后还留下两处无意义别名
- 位置：cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:93-112,cpp/nodes/gate/handler/event/scene_entry_dispatch.cpp:392-407,cpp/nodes/gate/handler/rpc/client_message_processor.cpp:482-489

现象成立：gate 里的 sceneRegistry / sceneEntity 指的是 scene 节点的 registry 和节点实体，与 scene 进程里的 SceneRegistry / SceneEntity 同名不同义。这与 W2-b5_types-06 同根，按编号字典序 b5_types-06 为主条目。

原文有三处需要更正：

(1) 任务单 4c-2 的文件清单只有 client_message_processor.cpp。batch5 §4.12(5) 还明确写了「gate 里名叫 sceneRegistry 的局部变量是节点 registry,不受影响」。所以 scene_entry_dispatch.cpp 不算「漏改」，这个名字在 6281e4d27 就已存在（当时在 :95）。Codex 实际造成的问题是：参数改成 entt::entity 之后，原来的类型转换 `const entt::entity sceneEntity{sceneNodeEntityId}` 退化成无意义别名（:98，client_message_processor.cpp:485 同样如此）。这与 W1-28 指出的「ResolveSessionTargetNode 无意义别名」是同类残留。

(2) 把 SceneEntityComp::sceneEntity 改名为 scene 没有依据。这个字段只在 scene 进程使用，类型就是 SceneEntity，名字与含义一致，不存在同名不同义。字段名是 batch5 §4.12(2) 明确指定的，S2b 也只是总评里的参考项。

(3) :147 和 :153 日志里的「scene_node=」打印实体整数，已由 W1-28 覆盖。

