# ECS / entt 滥用改造：Codex 任务单总索引（2026-10-15）

> **谁执行**：Codex 实施全部代码改动、编译、测试。Claude 只出任务单、做审查，不改代码、不编译、不测试（AGENTS.md §10.1）。
> **结果交给谁**：每批完成后，按本文 §6 的回执格式填好，交负责人贴回 Claude 的对话。Claude 对照任务单审查 diff，审出的问题写成补充任务单再交回 Codex。
> **来源**：审计结论与全部 110 条编号（F001–F100、C001–C010）见 [ecs-entt-misuse-audit-20261015.md](../../design/ecs-entt-misuse-audit-20261015.md)；组件初始化规则见 [ecs-component-access-rules.md](../../design/ecs-component-access-rules.md)。本目录各任务单沿用这些编号与批次号，不重新编号。
> **负责人授权**：负责人 2026-10-15 明确表示「不怕大改」，并要求「全部让 Codex 去改，要特别细」。
> **最新执行口径**：负责人已授权“全部做完，2 到5批”，并明确“先完成源码，暂缓生成器构建、编译和测试”。第 2–5 批源码与回归用例已实施，当前生成产物尚未统一更新，**不能据此宣称当前工作区可构建或本轮测试通过**。范围、偏差与剩余验收统一见文末 §7。之前 S 子批的 5/5 和一部分生成器定向测试属于先前版本证据，不覆盖本次最终源码。

---

## 0. 一页看懂

- **问题**：项目里很多不该放进 ECS 的东西被塞进了 registry，包括单例、请求级临时数据、背包内部索引和网络资源；实体句柄被当成对外 ID，下发给客户端、走 Kafka、截成 uint32；组件该由谁、在什么时候创建也没有约定。
- **改法**：按批次逐步收口，每批小而完整、可以独立验证。
- **全局口径**：组件按三类初始化，负责人已确认，见 §2。任何一批都不得违反。

## 1. 批次与状态

| 批次 | 任务单 | 内容 | 状态 |
|---|---|---|---|
| 第 1 批 | [batch1-verify.md](batch1-verify.md) | 存盘与快照只读化、空间/AOI、actor 属性、战斗 buff 与技能、货币与结算账本、场景入口与节点 registry 边界 | **Codex 已继续修正三类初始化与错误传播**;[实施回执](batch1-codex-receipt.md)。依负责人指令未编译 / 测试,跨会话项仍待闭环 |
| 第 2 批 | [batch2-singletons.md](batch2-singletons.md) | 单例回到明确所有者，请求 tip 直接写 response，删除无调用的全局 registry | 源码完成；重生成、整批构建/测试与审查待执行 |
| 第 3 批 | [batch3-bag.md](batch3-bag.md) | 背包普通字典、确定的实例顺序、删除物品句柄 API | 源码完成；18 条新增用例未运行 |
| 第 4 批 | [batch4-external-id.md](batch4-external-id.md) | Kafka 显式白名单、内部结构体事件、节点句柄类型、日志、ActorNetIds | 源码完成；默认兼容方案 A；统一生成及运行验收待执行 |
| 第 5 批 | [batch5-lifecycle-types.md](batch5-lifecycle-types.md) | 生命周期、可搬移定时器、PlayerGuidComp、SceneEntity、死代码清理 | 源码完成；已补通知重入防护；运行验收待执行 |
| 总评 | [ecs-fit-overview.md](ecs-fit-overview.md) | 逐个子系统判断用 ECS 是否合适，以及第 2–5 批之外还该做的结构性改动 | 参考 |

**顺序**：第 1 批 → 第 2 批 → 第 3 批 → 第 4 批 → 第 5 批。
- 第 3 批与第 2 批的文件不重叠。负责人同意后可以并行，但每批的回执分开交。
- 第 4 批的 Kafka 白名单要先盘点 Go 与 Java 两版的事件生产者。
- 第 5 批把 `Guid` 这类基础类型组件改名，改动面最大，放在最后。

负责人本次已授权连续完成第 2–5 批源码；各批回执分开记录，完整运行验收与后续审查仍须完成。

## 2. 全局口径：组件的三种初始化方式（负责人 2026-10-15 确认）

| 类别 | 例子 | 何时创建 | 读的时候拿不到 |
|---|---|---|---|
| **持久数据** | 货币、资产与结算账本、等级、属性、宠物、背包、任务 | **只在登录加载时**（`*MessageFieldsUnmarshal` / `InitPlayerFromAllData`） | 报错并拒绝（`LOG_ERROR` + fail-closed），**绝不补默认值** |
| **按需状态**（延迟初始化） | 冷却表、技能上下文、AOI 列表、施法定时器、属性脏标记 | **该功能唯一的写入口在第一次写时创建**，入口加注释「此处延迟创建」；结束时可以 `remove` | 当作「空 / 未激活」，属于正常分支，不是错误 |
| **派生运行时数据** | 由属性算出来的值 | 实体创建时算好 | `LOG_ERROR` + 跳过本次处理 |

硬规则：
1. 读路径（查询、存盘、快照、面板、日志、每 tick 的只读计算）只用 `try_get` / `all_of` / `any_of`，不改变组件集合。
2. `get_or_emplace` 只出现在创建点，以及按需组件的唯一写入口。
3. 不要把按需组件的写入口改成 `try_get`，否则再也没人创建它。
4. 校验前置：先确认需要的组件齐全，再做副作用。

## 3. 通用实施规则（每批都适用）

- **生成产物不手改**（AGENTS.md §3）：`cpp/generated/**`、`cpp/libs/services/scene/generated/**` 只能改模板 `tools/proto_generator/protogen/internal/template/**`，再重新生成：
  ```powershell
  pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
  pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml
  ```
  生成式 handler 文件只改 `///<<< BEGIN WRITING YOUR CODE` 守护段内。重新生成后，核对守护段里的手写代码一行不丢。
- **proto 改动**（AGENTS.md §4）：字段编号上线后不复用。只在开发期新增/删除字段时，按 §4 的流程重新生成并完整编译。uint32 → uint64 同为 varint 只表示编码类型相同,旧接收端仍可能截断高位;任务单须逐个列出受影响的读写点、混合版本窗口与部署顺序,不能据此宣称滚动兼容。
- **tip 错误码**只能在 `data/tip/Tip.xlsx` 加行后由导表器生成，不准手写数字。
- **编译**必须串行：`msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`（并发会报假的 C1041 / LNK1104）。
- **测试**：`pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter <工程名>`。新增用例必须做到「修复前红、修复后绿」，确定、可重复，不依赖墙钟和执行顺序。
- **MSVC 捕获 `entt::entity`** 要用 init-capture（`[entity = entity]`）。
- 注释、日志说明一律中文，写「为什么」。不夹带无关重构和格式化。

## 4. 与其他会话的协调（同一工作区多人并行）

开始改一个文件前，先运行 `git status --porcelain -- <文件>` 和 `git log -3 --oneline -- <文件>`。有别人的未提交改动就在其基础上改，**不回退、不覆盖**。

| 会话 | 正在改的文件 | 与本改造的关系 |
|---|---|---|
| C++ RPC 异步回调链式调用设计 | `registration_manager.{h,cpp}`（已整段重写）、`node.{h,cpp}`（`FinalizeShutdownInLoop` 拆两段、`RegisterHandlers`）、`core/network/` 下的 `game_channel.*`（改名 `rpc_channel.*`）、`rpc_client.h`、`rpc_server.*`、`rpc_session.h`、`rpc_controller.h`、`node_message_utils.*`（整删）、`player_message_utils.*`（删零调用封装）、gate `client_message_processor.cpp` 的 `HandleTcpNodeMessage` 与断线 `ExitGame` 两段、`scene_handler.cpp` 与 `gate_service_handler.cpp` 的 `Route*` 函数（改为显式拒绝） | 本改造碰到这些文件时，先确认它已落地并重读。F007 已随它删除，C007 的握手同类已在它的重写里解决 |
| C++ gRPC 迁移至 muduo HTTP/2 | gRPC 客户端模板、`node_connector.cpp`、`node.cpp` 的通道清理、CQ 轮询 | F010、F100、F006、F082、F011、F012/F022 的剩余部分、C008 **已移交给它**，作为迁移验收条件（见 `docs/design/grpc-muduo-transport.md` §10.1）。本改造**不改**这些点 |

## 5. 交付与文档

- 每批改完，在 `docs/PROGRESS.md` **末尾追加**一条（只追加，不改旧条目），写明：改动范围、修了哪些审计编号、编译和测试结果、Java 版状态（AGENTS.md §12）。
- 审计文档的「HEAD 状态」由 Claude 在审查通过后统一更新，Codex 不改。
- 不 commit、不 push。是否提交由负责人决定。

## 6. 回执格式（每批完成后交回）

```text
批次: 第 N 批
改动文件: <git diff --stat 的输出,或逐个列出>
审计编号:
  F0xx  已修 | 推迟(原因) | 不成立(依据)
  ...
与任务单不一致之处:
  - <位置>: <实际做法> / <原因,例如任务单漏了调用点、代码已被别的会话改过>
编译: <命令> -> <通过/失败;失败时贴前 30 行错误>
测试: <命令> -> <通过数/失败数;失败用例名 + 关键断言输出>
新增测试: <用例名> -> 修复前是否确认为红(怎么确认的)
未执行的验证: <列出,并说明原因>
```

## 7. 2026-10-15 第 2–5 批源码合并回执（Codex）

### 授权、状态和证据边界

负责人授权全部第 2–5 批及其 30+ 文件范围，随后明确先完成源码、暂缓生成器构建、编译和测试。三个本聊天子代理与“细化 ECS 重构任务单”聊天按文件归属协作，后者完成 L1 后明确释放文件，依次集成 PlayerGuidComp、ActorNetIds 和 SceneEntity。未回退共享工作区的其他 RPC/gRPC/玩法改动，未 commit / push / tag，未读取或改动独立客户端。

本轮交付是源码与测试源码。没有执行最终生成、C++/Go/Java 构建、单元/集成/联机或压测，没有本轮红/绿证据，也没有性能提升结论。S 子批先前的独立 5/5，及白名单最初版本的 `go test ./internal/generator/cpp ./internal/prototools/option` 退出 0（日志 `build/ecs-refactor/event-ingress-generator-test.log`），均早于后续接口/生成器修改，不能覆盖最终状态。

### 分批结果与关键偏差

| 批次 | 最终源码与测试范围 | 任务单挑战的实际处理 |
|---|---|---|
| 2 | Node 独占身份、发现管理器独占 ServiceNodeSnapshot；SessionMap 普通成员；handler 直接写本次拒绝码；去掉全局 tip 和 EcsContext 两个全局 registry/getter | 只读身份门面未安装返回默认实例，跨线程测试只验证同一地址且无并发写，不承诺 protobuf 线程安全；非精确 `::Empty` 应答由生成头 static_assert 保证 error_message 类型；保留过渡空 TRANSFER 宏直到旧生成调用重生 |
| 3 | ItemStore 单一 `Guid -> ItemComp` 字典；删 EntityOf/GetItemByGuid/GetItemByPos 实体接口；补堆/合并/FIFO显式实例序、扣除预计算槽位 rank、批量配置升序；18 条新增用例 | 淘汰后重新规划，计划只存 Guid；零堆不当幸存者；不改变 already-compact 早退；未把局部有序误报为所有动态包写盘全局稳定 |
| 4 | 3 scene + 9 gate 显式入口；普通 C++ 内部事件；删除 11 源 proto/22 旧 handler；gate typed 节点句柄和完整版本位；ActorLog；ActorNetIds | bool `OptionKafkaIngress` 为唯一标注，入口由目录决定，避免 enum/生成器双份常量；两个派发函数与空载荷回退共用生成白名单；零白名单仍重写拒绝路由；旧产物清理和三份工程清单由生成器执行，预检失败不删文件且可恢复；网络号用 node32+seq32，耗尽饱和拒绝，避免跨节点从 1 撞号 |
| 5 | scoped_connection；TimerTaskComp 内部共享 State/弱回调、移动转移；Anomaly 玩家组件；显式 AOI 离场/Pin-Unpin；ScenePlayers和死crowd/acceleration清理；直接默认技能初始化；PlayerGuidComp、具名容器、SceneEntity | 只保留施法者技能上下文；远端/缺空间状态/已毁句柄也清旁路关系；解除 Pin 先取消其隐身豁免再判断；空接收者保留属性脏位、有接收者消费摘除；EntityVector广播stub防重复链接；通知后复核句柄与玩家索引归属，初始化/退出/废黜不误改替身 |

第 5 批另外将 InBattleComp 的三个合法创建/恢复入口收口为同一首写 helper，保留权威锁、账本校验和副作用顺序。SceneEntity 与 entt::entity 不隐式互转，场景创建、排空、传送、AOI、s2s查找及主测试一并迁移；gate 的 scene 节点池仍是节点 registry，保持 entt::entity 正确。

### 静态证据和明确保留项

各 owner 对自己范围做 diff 检查，并交叉只读复核背包、Timer/Anomaly、白名单和 SceneEntity。最终静态汇总见 PROGRESS；XML 解析只证明工程格式，不能代替编译。旧 `cpp/tests/scene_test` 已由正式 runner 排除，仍引用历史 API，本轮没有假装修好该废弃工程。

- C004 依任务单只记录业务决策：PlayerItemBlockList 没有生产写入口，本轮不新增 GM/持久化功能。
- C006 的 AOI 缺策略/压力/容量组件采用既定默认值，Npc 标签保留给未来生成入口。
- 当时保留的 actor 必需派生初始化 sink 依赖已在后续补充中移除：创建流程直接调用 `ActorAttributeCalculatorSystem::InitializeOnCreate`，`InitializeActorCompsEvent` 仅通知，详见 [第 5 批补充回执](batch5-lifecycle-types.md#10-actor-必需派生初始化补充回执2026-10-15)。
- 内部 SkillContext/Buff 尚以 uint64 本地句柄和 0 表示空，保留 EcsContext 的 0 号 actor 退役兼容；不再用于对外 actor 编号。ActorNetIds 是本进程运行期身份，不承诺进程重启后永久唯一，不落库。
- gRPC CQ/Stub、RpcClientPtr/MuduoGrpcChannelPtr 等资源组件的既有例外继续交 `grpc-muduo-transport.md §10.1` 的迁移会话，本批未改传输销毁协议。
- 审计文档 HEAD 状态仍由审查方更新，未把源码落盘等同于全仓 110 条全部关闭。

### 恢复验证时的顺序（当前未执行）

1. 先确认共享工作区的 RPC/落地摘要契约源已定型。在仓根运行 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`，随后仅对本进程设置 `PROTOGEN_ALLOW_MASS_EVENT_TOMBSTONE=1`，以 `proto_gen.no-unity.yaml` 运行 `proto-gen-run`。该配置禁用独立客户端写入。核对事件号只转墓碑、已有活号不变，新 option/白名单和属性同步已生成，陈旧事件产物/汇总已清；确认守护段手写代码保留后删除空 TRANSFER 宏。当前不要用旧二进制或旧生成树宣称验证。
2. EcsContext、Bag 和组件布局均变化，统一重编受影响库和节点，禁止混链旧 obj/lib。MSBuild 使用现有 VS 18 Enterprise，`/m:1 /nr:false /p:Configuration=Debug /p:Platform=x64`，串行构建。
3. 生成器定向测试至少覆盖 internal、internal/generator/cpp、internal/prototools/option 和 internal/prototools；C++ 按统一 runner 分别执行 ecs_context_test、kafka_command_test、bag_test、missions_test、cross_zone_test、routing_identity_test、aoi_test、aoi_delivery_test、skill_test、buff_test、currency_test、timer_queue_unit_test、rpc_controller_test。以 runner 实际工程名为准，保留 build/test log 与 JUnit XML，失败不连续盲重试。
4. 构建和单测后再做本地节点起停、进场/换场景/离场、技能目标回传和场景排空冒烟。压测是独立授权与口径，不以本次静态工作宣称性能变化。

### Java 状态

现有版本 0.1.0-SNAPSHOT，无新增实现版本。Java 不用 EnTT，节点/场景使用领域对象，SceneWorld 已用独立 nextId 给 actor 发不透明号；本轮无需复制 C++ 存储机制。背包采用入包序/guid 比较器，本次与其正常操作顺序对齐；已有发号原子性、零堆、旧档 seq=0 补章和存档排列差异仍保留，不宣称全部两版背包一致。客户端字段编号/类型/消息号未改，proto 仅补 actor 语义注释；Java ContractSync 与两版生成/构建/测试均未执行。已在 Java PARITY.md 登记本轮范围与待验收项。
