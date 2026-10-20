# Codex 任务单：第 2 批：全局实体单例改为所属对象的显式成员（2026-10-15，第 1 版）

> **谁执行**：Codex。总规则、批次顺序、组件三类初始化口径、协调与回执格式见 [README.md](README.md)，本单不重复。
> **本版性质**：设计稿加挑战者意见的原样整理，还没有细化到「改前/改后代码片段」。Claude 的额度恢复后会出第 2 版（补丁级）。**先读 §8 挑战意见**：标为 high/medium 的问题，实施时必须按其中的修法修正设计。凡与 README §2 三类初始化规则冲突的地方，一律以 README 为准。
> **行号**：设计时取自当时的 HEAD，此后第 1 批和其他会话都改过代码，实施前请按函数名重新定位。

## 1. 目标

把挂在 GlobalEntity / GrpcNodeEntity 上的进程级、节点级、请求级单例搬回各自唯一所有者的普通成员。涉及四类：TipInfoMessage 约 60 处读写、NodeInfo 两套 get_or_emplace 访问器、ServiceNodeList 9 处、SessionList/SessionMap。随后删除 globalRegistry、nodeGlobalRegistry、itemRegistry，删除 GlobalEntity()/ErrorEntity()/OperatorEntity()/GrpcNodeEntity()，删除依赖它们的 TRANSFER_ERROR_MESSAGE / CHECK_REQUEST_PRECONDITIONS 宏。请求级 tip 不再做任何暂存，handler 直接写本次 response，这条规则只在一处声明。对外契约不变（tip 码、error_message 字段、帧格式都不变）。顺带消除四个缺陷：F008（Clear() 之后，或换线程时，拿到的是空 NodeInfo/ServiceNodeList）；Empty 应答的 handler 写 tip 会串到同节点下一条请求、下一个玩家；SendErrorToClient 会带出上一次的 tip 参数；InvokePlayerService 每次都拷贝一份恒为空的 tip。EcsContext 改造后只保留真正的 ECS 世界（actorRegistry / sceneRegistry）、dispatcher 和玩家索引。

## 2. 采用的标准做法

原则是 EnTT 惯用法加 AGENTS §11（单一权威来源、显式依赖、信息隐藏）：registry 只装「大量同构实体、按组件组合批量处理」的数据。进程级、节点级、请求级数据各有天然的所有者，放进所有者的普通成员，生命周期和线程模型也跟着所有者走。不用 registry.ctx() 替代：这些数据的作用域不是 registry（NodeInfo 跟 Node 走，tip 跟单次请求走），挂到 ctx 上只是把「全局实体」换个名字，所有者依旧不清。

逐项所有者：
1) NodeInfo → Node::localNodeInfo_。它是普通数据成员，不是 thread_local，也不进 registry。
   - 读：Node::GetNodeInfo() const 返回 const&。
   - 写：只经 Node::MutableNodeInfo()，只在 loop 线程。写入方是 InitRpcServer / InitClientEndpoint / NodeAllocator / gate 人数上报。
   - 自由函数 GetNodeInfo()/GetZoneId()（node_utils，底层模块，不能依赖 Node）改成只读门面，经一个原子「安装指针」读取：Node 构造时安装自己的成员，析构时卸载。未安装时返回 NodeInfo::default_instance()，只读，不造任何状态。
   - 这样做：tlsEcs.Clear() 再也碰不到身份（F008）；任何线程读到的都是同一份；单测进程没有 Node，行为与今天一致（zone=0）；需要指定 zone 的单测自己安装一份。
   - 没选「直接 gNode->GetNodeInfo()」：node_utils 不能依赖 node.obj（rpc_controller_test 已有链接问题），而且所有不构造 Node 的单测会全部空指针崩溃。
   - 没选「未安装即 LOG_FATAL」：无法静态枚举哪些测试二进制会走到 GetZoneId，对 30 多个生产调用点是高风险的行为变化。
2) ServiceNodeList → ServiceDiscoveryManager（Node 成员）持有的 ServiceNodeSnapshot，新建一个 header-only 小类。
   - 按 uuid 的 Upsert/Remove 和带边界检查的 NodesOfType 收口在这一处。原先 AddServiceNode 与 ExecuteNodeRemoval 各写一遍 uuid 匹配，调用方还直接对 std::array 下标取 mutable_node_list()。
   - 写入口只有发现链路（AddServiceNode）和 Node::ExecuteNodeRemoval（RemoveServiceNode）。
3) SessionList → EcsContext::sessionPlayers，普通成员，与 playerList 并列。
   - 它与 playerList 同寿命、同线程、同属「本节点在线玩家索引」，由 EcsContext::Clear() 一并清空，测试语义不变。
   - 访问函数 SessionMap() 保留，签名不变：生产 3 个文件和约 40 行测试零改动，也不碰第 1 批正在改的 UpdateSessionDetail。
4) 请求级 tip：不设暂存，handler 一律直接写 response->mutable_error_message()。
   - response 本身就是请求级暂存：派发方按请求创建，序列化后销毁，生命周期精确。
   - 不按「错误处理模块的请求暂存」做，理由有四：
     - 全仓所有写 tip 的地方都在 handler 函数体里，response 就在作用域内，没有任何系统层写入方；
     - 暂存必然需要一个「搬运」步骤，搬运就是第二个真相源。F003（空 tip 覆盖拒绝码）、Empty 应答串号、延后完成路径污染（rpc_channel.cpp 为此专门加了防御块）都出自这一步；
     - Empty 应答没有 error_message 字段，直接写在编译期就写不进去，把今天靠注释维持的约定（player_scene_handler.cpp 里 GetSyncDigest 的「禁止 SetTip」）变成编译错误；
     - 同步路径与 RpcDoneCallback 延后完成路径共用同一条规则。
   - 两条路径于是合并为一条，规则只写在 PlayerService::CallMethod 的接口注释里（player_service_interface.h，handler 作者必看的地方）。其他地方只放一行指针。
5) 删除 globalRegistry / nodeGlobalRegistry / itemRegistry 与四个知名实体函数。删除后再新造「全局实体挂单例」在类型层面就写不出来。

## 3. 覆盖的审计编号

F004（TipInfoMessage 作为进程级隐式输出邮箱，正确性靠约定）、F008（NodeInfo/ServiceNodeList 靠 get_or_emplace 懒建：Clear 之后或换线程会造出空数据，gate 断线通知丢失）、F018（globalRegistry/nodeGlobalRegistry 只当「类型→单例」容器用）、F019（单例挂在 GlobalEntity/GrpcNodeEntity 上；ErrorEntity/OperatorEntity 是死代码；AOI 部分已由第 1 批处理）、F020（懒建实体加 get_or_emplace 的单例模式；itemRegistry/ErrorEntity/OperatorEntity 闲置）、F021（部分：删除死的 itemRegistry。Bag 私有 registry 属 F024，不在本批）、F092（请求级单例 TipInfoMessage、SessionList 挂在 GlobalEntity 上，每条消息都 get_or_emplace）、F099（部分：SessionList 这个 std 别名不再充当组件；Guid/ScenePlayers 不在本批）、F003/F090 收尾：TRANSFER_ERROR_MESSAGE 整体删除，从根上不再存在覆盖、新发现-1：返回 Empty 的 handler 写全局 tip 时，生成层不搬运，tip 留在 TLS 上，被同节点下一条非 Empty 请求（任意玩家）带走、新发现-2：SendErrorToClient 把全局 tip 整个 CopyFrom 进应答，会带出残留的 parameters、新发现-3：InvokePlayerService 在 CallMethod 之后拷贝的全局 tip 恒为空（TRANSFER 已清），属死代码、新发现-4：NodeInfo 有两套各自 get_or_emplace 的访问器（Node::GetNodeInfo 与 node_utils 的自由函数 GetNodeInfo），违反单一权威来源、新发现-5：rpc_channel.cpp 的 CompleteDeferred 为全局 tip 专设的防御清理块，随全局 tip 消失而失去存在理由

## 4. 实施步骤（预计改动约 56 个文件）

### 4.1 A1 改写 return_define.h 的 tip 宏（TRANSFER 先改成过渡空操作）

**文件**：
- `cpp/libs/engine/core/macros/return_define.h`

按内容定位，行号以 2026-10-15 工作区为准，下同。
1) TRANSFER_ERROR_MESSAGE 改成过渡空操作：`#define TRANSFER_ERROR_MESSAGE(response) do { (void)(response); } while (0)`，上方注释写明「过渡：生成物里还有这一行；proto-gen 重生之后由 C2 删除本宏；拒绝码规则见 rpc/player_service_interface.h」。这样 A 子批不依赖重生时机也能编译，语义也立即正确：tip 已经直接在 response 里。
2) 删除 CHECK_REQUEST_PRECONDITIONS。全仓没有调用点，已用 git grep 确认。
3) 重写 CHECK_PLAYER_REQUEST（它要向调用方注入 return，属于 AGENTS §11.2 允许的第四类宏）。宏体改为：先算 `const auto _err4 = fn(player, request);`，不等于 kSuccess 时先照原样 LOG_WARN（带 __FUNCTION__/__FILE__/__LINE__），再执行 `response->mutable_error_message()->set_id(_err4); return;`。注释写明它依赖调用处作用域里的 player / request / response 三个名字（与 SET_ERROR_IF_FAILURE 同一约定），规则见 player_service_interface.h。
4) 文件里已经没有任何 globalRegistry/GlobalEntity 引用。保留 `#include <thread_context/ecs_context.h>`，ECS_GET_OR_* 仍要用 tlsEcs。

### 4.2 A2 在 PlayerService 接口上写唯一的「拒绝码写哪里」规则

**文件**：
- `cpp/libs/services/player/rpc/player_service_interface.h`

这个头文件是手写的，生成的只有 player_service_interface.cpp。在 `virtual void CallMethod(...)` 上方加接口契约注释，作为全仓唯一的权威表述：
- handler 的业务拒绝码一律直接写本次调用的 `response->mutable_error_message()`；带参数时在同一处 add_parameters。
- response 由派发方按请求创建，即 SceneHandler 的 ProcessClientPlayerMessage / InvokePlayerService / SendMessageToPlayer；序列化后销毁。它就是请求级暂存，进程里没有任何全局或 TLS 的 tip。
- 返回 Empty 的方法没有 error_message 字段，编译期写不进去。这类方法的失败只能由系统层推送：PlayerTipSystem::SendToPlayer，或专用 S2C 通知（例如 NotifySyncDigest 的 UNAVAILABLE）。
- 成功路径不得写 error_message，保持字段缺省。
- 延后完成（RpcDoneCallback）适用同一规则。
- 生成的 CallMethod 只派发、不搬运。

### 4.3 A3 生成器模板去掉 TRANSFER_ERROR_MESSAGE

**文件**：
- `tools/proto_generator/protogen/internal/player_service_gen.go`

1) getPlayerMethodHandlerFunctions 模板里删掉 `{{- if not $method.IsEmptyResponse }}` / `TRANSFER_ERROR_MESSAGE(...)` / `{{- end }}` 这三行。生成出来是：case 块只调 handler，然后 break。不要往模板里加注释行，免得所有生成头产生无关 diff。
2) PlayerMethod 结构体删掉 IsEmptyResponse 字段和它的赋值（原 103 行）。
3) `strings` 包只在那一处用，删掉 import，否则 Go 编译不过。
4) MacroReturnIncludeName（`#include "macros/return_define.h"`）保留：handler 还要用 CHECK_PLAYER_REQUEST 和 ECS_GET_OR_*。
5) 由 Codex 跑 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`，再跑 `-Command proto-gen-run`。预期只有下列 11 个生成头各少一行 TRANSFER：cpp/nodes/scene/handler/rpc/player/ 下的 player_activity / player_attribute / player_bag / player_currency / player_mission / player_movement / player_pet / player_rollback / player_scene / player_skill / s2s_player_scene 这 11 个 _handler.h。不手改任何生成物（AGENTS §3）。

### 4.4 A4 九个玩家 handler 改为直接写 response（只动守护段）

**文件**：
- `cpp/nodes/scene/handler/rpc/player/player_activity_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_mission_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_attribute_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_pet_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_bag_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_currency_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_rollback_handler.cpp`
- `cpp/nodes/scene/handler/rpc/player/player_scene_handler.cpp`

通用改法：`tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).set_id(x)` 和 `SetTip(x)` 一律改成 `response->mutable_error_message()->set_id(x)`。所有改动都在 `///<<< BEGIN/END WRITING YOUR CODE` 守护段内，包括文件头的守护段。已逐个核对：所有写入点所在方法的应答都不是 Empty，都有 TipInfoMessage error_message 字段。
- activity：第 17 行。
- mission：第 17、28、39 行。
- attribute：删掉文件头的 SetTip helper 及其注释（保留 ToStdMap），10 处调用直接写。
- pet：同 attribute，删 helper，10 处调用直接写。
- bag：删 SetTip helper。模板 ReplyInventoryAndEquipment(entt::entity, Response& response) 里的 SetTip(result) 改成 `response.mutable_error_message()->set_id(result)`。第 53、67 行及其余 SetTip 调用直接写。
- currency：RejectGmCurrencyRpc 改成 `template <typename Response> bool RejectGmCurrencyRpc(const char* rpcName, Response& response)`，只在拒绝时写 kFeatureUnavailable。四个调用点改成 `RejectGmCurrencyRpc("GmXxx", *response)`，另外 6 处 set_id 直接写。
- rollback：RejectGmRollbackRpc(const char*) 与 RejectIfFrozen(entt::entity, const char*) 改成以 `Response& response` 为最后一个参数的模板，只在拒绝时写码，不在放行时调 mutable_error_message()，避免平白造出字段存在性。24 个调用点追加 `*response`。文件头英文注释「stamp kInvalidParameter into the global TipInfoMessage」改为写进本次应答。
- scene：删掉文件头守护段里的 SetTip helper 和那段「必须写 TLS」的注释，11 处 SetTip 直接写。TravelToZone 注释「拒绝码写 TLS tip（见文件头 SetTip）」改为「拒绝码直接写应答」。GetSyncDigest 注释改为「应答是 Empty，没有 error_message 字段，所有失败由系统层推 UNAVAILABLE 的 NotifySyncDigest」。
- player_skill_handler.cpp 不用改：ReleaseSkill 已经直接写，CHECK_PLAYER_REQUEST 由 A1 改好。
- handler 里的 `thread_context/ecs_context.h` 不必删。

### 4.5 A5 SceneHandler 两个派发入口去掉全局 tip（不碰第 1 批负责的函数）

**文件**：
- `cpp/nodes/scene/handler/rpc/scene_handler.cpp`

只改 ProcessClientPlayerMessage、InvokePlayerService 和文件头的 WriteRejectTip 注释。CreateScene / UpdateSessionDetail / RoutePlayerStringMsg 归第 1 批，不动。这个文件还有别的会话在改，未提交，动手前重新读一遍，只改下面三块。
1) ProcessClientPlayerMessage 的 GM 闸分支（约 471-491 行）：「把拒绝码搬进应答并清零全局 tip」那段长注释，连同 gmRejectTip 的取用、反射写入、Clear，整块替换成 `WriteRejectTip(*playerResponse, kFeatureUnavailable);`。前面配两行注释：GM 闸不走 CallMethod，拒绝码直接写进应答，与退出闸共用 WriteRejectTip；应答为 Empty 时没有字段，不写。行为完全等价：原逻辑是「写进应答再清零」，现在不存在要清的东西。
2) InvokePlayerService 在 CallMethod 之后的三行删除：`auto &tipInfoMessage = ...; response->mutable_message_content()->mutable_error_message()->CopyFrom(tipInfoMessage); tipInfoMessage.Clear();`。原来拷贝的恒为空 tip，内层 playerResponse.error_message 照常序列化；gate 侧的 OnSceneInvokePlayerServiceReply 是空函数，没有读方。
3) 匿名命名空间里 WriteRejectTip 的注释，删掉「不经进程级全局 TipInfoMessage……GM 闸门那条分支为什么要清全局 tip」两句，改成指向 player_service_interface.h 的规则。

### 4.6 A6 SendErrorToClient 不再读全局 tip

**文件**：
- `cpp/libs/engine/core/network/error_handling_system.h`

1) 删掉 `#include <thread_context/ecs_context.h>`。目前只有 scene_handler.cpp 包含这个头，它自己另有包含链能拿到 tlsEcs。若编译报缺 tlsEcs，就在报错的包含方显式加 `#include "thread_context/ecs_context.h"`，不要把这条依赖放回本头。
2) 第 37-39 行改成：`auto *error = response.mutable_message_content()->mutable_error_message(); error->Clear(); error->set_id(err);`。今天调用时全局 tip 恒为空，所以行为等价，同时去掉了残留 parameters 被带出的潜在泄漏。
3) 删掉函数末尾「Decide whether to clear… // tip->Clear();」两行注释。

### 4.7 A7 RpcChannel 延后完成路径删掉全局 tip 防御块（依赖 muduo-rpc-channel 会话）

**文件**：
- `cpp/libs/engine/core/network/rpc_channel.cpp`
- `cpp/libs/engine/core/network/rpc_done_callback.h`

这两个文件属于正在进行的 muduo RPC 通道会话：rpc_channel.cpp 由 game_channel.cpp 改名而来，rpc_done_callback.h 是未跟踪的新文件。只做下面两处最小改动；该会话尚未落地时本步先跳过，由 C 子批开工前补做。
1) RpcChannel::CompleteDeferred 里从「// 延后完成路径不经过 TRANSFER_ERROR_MESSAGE…」到 `tip->Clear(); }` 的整个 if 块删除。全局 tip 已不存在，这块编译不过，也没有意义。
2) rpc_done_callback.h 契约第 22-23 行（「延后完成路径只能直接写…不得用全局 tip(SetTip / TRANSFER_ERROR_MESSAGE …)」）改成：「与同步路径同一规则：拒绝码写 `response<T>().mutable_error_message()`，见 rpc/player_service_interface.h；进程里没有全局 tip」。

### 4.8 A8 tip 相关单测改为只看 response

**文件**：
- `cpp/tests/skill_test/skill_test.cpp`
- `cpp/tests/bag_test/player_feature_handler_test.cpp`
- `cpp/tests/bag_test/player_equip_test.cpp`

1) skill_test.cpp：删掉 TransferErrorMessageTest 的两个用例（KeepsDirectResponseErrorWhenTlsTipIsEmpty / MovesTlsTipIntoResponseAndClearsIt），换成 PlayerRequestRejectTest。用法：在匿名命名空间写一个参数为 `(entt::entity player, const ReleaseSkillRequest* request, ReleaseSkillResponse* response, uint32_t (*fn)(entt::entity, const ReleaseSkillRequest*))` 的小函数，函数体是 `CHECK_PLAYER_REQUEST(request, fn);`。断言两条：fn 返回 kSkillCooldownNotReady 时，response.error_message().id() 等于该码；fn 返回 kSuccess 时，`!response.has_error_message()`。
2) player_feature_handler_test.cpp：删掉所有 globalRegistry/GlobalEntity/TipInfoMessage 全局引用，包括第 32-33 行的 tip 取用与 Clear、`EXPECT_EQ(0u, tip.id())`、第 53、63 行的 Clear、fixture 的 Tip() helper 及第 93、105 行的 Clear、第 119、126、170 行的 `EXPECT_EQ(0u, Tip().id())`。对 response.error_message() 的断言全部保留，「先拒绝后成功、成功应答 id 为 0」的断言就是不串号的证据。用例名里的 AfterGeneratedTransfer 可改为 IsWrittenToResponse（可选）。
3) player_equip_test.cpp：删掉第 843-846 行的 Tip() helper、第 607、617 行的 Tip().Clear()，以及 17 处 `EXPECT_EQ(0u, Tip().id())` 和它们的 << 说明，其余断言不动。

### 4.9 B1 node_utils：NodeInfo 只读门面与安装指针，删除 ServiceNodeList 别名

**文件**：
- `cpp/libs/engine/core/network/node_utils.h`
- `cpp/libs/engine/core/network/node_utils.cpp`

node_utils.h：
1) 删掉 `using ServiceNodeList = std::array<NodeInfoListComp, eNodeType_ARRAYSIZE>;`，B3 移进 ServiceNodeSnapshot。
2) 签名改为 `const NodeInfo& GetNodeInfo();`。原来返回非 const，改 const 后写入方在编译期就会暴露。`uint32_t GetZoneId();` 不变。
3) 新增 `const NodeInfo* InstallLocalNodeInfo(const NodeInfo* info);`，返回旧值，便于 RAII 还原。
4) 上方注释写清：唯一所有者是 Node::localNodeInfo_；Node 构造时安装、析构时卸载；未安装时返回 NodeInfo::default_instance()，只读、不造状态（F008）；数据不是 thread_local；写只经 Node::MutableNodeInfo()，在 loop 线程。
node_utils.cpp：
1) 匿名命名空间里加 `std::atomic<const NodeInfo*> gLocalNodeInfo{nullptr};`。
2) InstallLocalNodeInfo 实现为 `return gLocalNodeInfo.exchange(info, std::memory_order_acq_rel);`。
3) GetNodeInfo 实现为 load(acquire)，非空返回 *info，否则返回 NodeInfo::default_instance()。
4) 删掉 `#include <thread_context/ecs_context.h>`：本文件只有原第 15 行这一处用到 tlsEcs。

### 4.10 B2 Node 持有 localNodeInfo_，拆分读写访问器

**文件**：
- `cpp/libs/engine/core/node/system/node/node.h`
- `cpp/libs/engine/core/node/system/node/node.cpp`

node.h：
1) 把 `NodeInfo &GetNodeInfo() const;` 换成内联的 `const NodeInfo &GetNodeInfo() const { return localNodeInfo_; }`。
2) 新增 `NodeInfo &MutableNodeInfo();`，注释：只给启动与注册链路及 gate 人数上报，只在 loop 线程调用。
3) 新增数据成员 `NodeInfo localNodeInfo_;`，声明为第一个数据成员（在 eventLoop 之前），保证最先构造、最后析构。注释写明这是本节点身份的唯一所有者，不在任何 registry 里。
4) HasDiscoveredServiceNode 改成 `return serviceDiscoveryManager.ServiceNodes().HasNodesOfType(nodeType);`，B3 的访问器自带越界检查。
5) 第 202-208 行 ClientEndpointRequired 的注释：删掉「读的是 thread_local tlsEcs…会就地 emplace 出一个空 NodeInfo」，改成「NodeInfo 只在 loop 线程写；跨线程读不与写同步，所以 ClientFacing 的实参仍须在 loop 线程取」。
node.cpp：
1) 基础构造函数（gNode = this 所在的那个）函数体开头加 `InstallLocalNodeInfo(&localNodeInfo_);`。删掉第 409 行 `tlsEcs.nodeGlobalRegistry.emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity());`。
2) ~Node 的 `if (gNode == this)` 块里加 `InstallLocalNodeInfo(nullptr);`。
3) 删掉第 481-484 行原 GetNodeInfo 的定义。新增 MutableNodeInfo 的定义：`assert(eventLoop == nullptr || eventLoop->isInLoopThread()); return localNodeInfo_;`。用 Debug 的 assert，不用 muduo 的 assertInLoopThread，避免 Release 下因未发现的调用点直接 abort。
4) 写入点：第 419 行改为 `MutableNodeInfo().set_node_type(nodeType)`；第 520 行（InitRpcServer）与第 643 行（InitClientEndpoint）改为 `NodeInfo &localNodeInfo = MutableNodeInfo();`；第 950 行（StartRpcServer）若只读则改为 `const NodeInfo &`，有写再用 Mutable；第 1594 行 `auto &currentNode` 改为 `const auto &currentNode`。
5) ExecuteNodeRemoval（约 1475-1500 行）只替换「删本地快照」那一段：从 get_or_emplace<ServiceNodeList> 到 removedSnapshotCount 的倒序删除循环，换成 `const int removedSnapshotCount = serviceDiscoveryManager.RemoveServiceNode(stoppedNode.node_type(), stoppedNode.node_uuid());`。下面的 WARN/INFO 日志保持不变。后面销毁节点实体、CQ 那部分归 gRPC 迁移会话（F010），一行都不碰。
6) 健康监控（约 1596 行）改成 `for (const auto &registeredNode : serviceDiscoveryManager.ServiceNodes().NodesOfType(currentNode.node_type()))`。
7) FinalizeShutdownInLoop 里的 `tlsEcs.Clear()` 保持不变：它现在只清 ECS 世界和玩家索引，碰不到身份。

### 4.11 B3 新增 ServiceNodeSnapshot，由 ServiceDiscoveryManager 持有

**文件**：
- `cpp/libs/engine/core/node/system/discovery/service_node_snapshot.h`
- `cpp/libs/engine/core/node/system/discovery/service_discovery_manager.h`
- `cpp/libs/engine/core/node/system/discovery/service_discovery_manager.cpp`

新文件 service_node_snapshot.h，header-only，所以不改 CMake 和 vcxproj 的编译项；如需在 IDE 里看到，可在 engine 的 vcxproj.filters 里登记 ClInclude。include 用 node_utils.h 现在取 eNodeType_ARRAYSIZE / NodeInfo 的同一组头（node/system/node/node_util.h 及其 proto 头）。
```cpp
// 服务发现快照：etcd 中各类型节点的 NodeInfo（本进程视角）。所有者 ServiceDiscoveryManager，寿命同 Node，只在 loop 线程读写。
// 非法 nodeType（>= eNodeType_ARRAYSIZE）按空桶处理：读返回空列表，写返回 kRejected / 0，绝不越界。
// 同一节点按 node_uuid 判定（与 NodeUtils::IsSameNode 同口径：字符串相等）。
class ServiceNodeSnapshot {
public:
  using NodeList = google::protobuf::RepeatedPtrField<NodeInfo>;
  enum class UpsertResult : uint8_t { kAdded, kUpdated, kRejected };
  const NodeList& NodesOfType(uint32_t nodeType) const;   // 越界返回函数内 static 的空列表
  bool HasNodesOfType(uint32_t nodeType) const;
  UpsertResult Upsert(uint32_t nodeType, const NodeInfo& node); // 同 uuid 的第一条用 CopyFrom 覆盖，否则追加
  int Remove(uint32_t nodeType, const std::string& nodeUuid);   // 删掉桶内所有同 uuid 条目（倒序 DeleteSubrange），返回条数
private:
  std::array<NodeList, eNodeType_ARRAYSIZE> byType_;
};
```
service_discovery_manager.h 新增：
- 私有成员 `ServiceNodeSnapshot serviceNodes_;`；
- `const ServiceNodeSnapshot& ServiceNodes() const { return serviceNodes_; }`；
- `int RemoveServiceNode(uint32_t nodeType, const std::string& nodeUuid) { return serviceNodes_.Remove(nodeType, nodeUuid); }`，注释：只给 Node::ExecuteNodeRemoval（etcd DELETE 与宽限期到期）调用。
service_discovery_manager.cpp 的 AddServiceNode（约 88-114 行）：手写的 uuid 查找、覆盖、追加循环换成 `const auto result = serviceNodes_.Upsert(nodeType, discoveredNode);`。日志不变：kAdded 打原来的 INFO「Node added…」加 TRACE detail，kUpdated 打原来的 TRACE。kRejected 走不到，前面已有 eNodeType_IsValid 校验，按防御性写法 return 即可。后续 IsCurrentNode / 白名单 / ConnectToNode 逻辑不动。注意：gRPC 迁移第 2 批（etcd）要做「Range 对账删除本地多出的节点」和「DELETE 按键反查本地快照」，这些将来都应以新方法的形式落在 ServiceNodeSnapshot 上，不要再另起一份快照。

### 4.12 B4 其余 ServiceNodeList 与 NodeInfo 写入点迁移

**文件**：
- `cpp/libs/engine/core/node/system/node/node_allocator.cpp`
- `cpp/libs/engine/core/node/system/node/node_connector.cpp`
- `cpp/libs/engine/core/node/system/registration/registration_manager.cpp`
- `cpp/nodes/gate/main.cpp`

1) node_allocator.cpp：
   - 第 40 行和第 175 行改成 `const auto &existingNodes = gNode->GetServiceDiscoveryManager().ServiceNodes().NodesOfType(<原 nodeType>);`。两处原来都对 mutable_node_list() 只读遍历，nodeList 局部变量随之删除。
   - 写入点改成 `gNode->MutableNodeInfo()`：第 93 行 set_node_id，第 300、320 行 mutable_endpoint()->set_port，第 323-324 行 mutable_grpc_endpoint 的两次 set。
   - 只读的 GetNodeInfo() 调用不动。
2) node_connector.cpp 的 ConnectAllNodes（第 183 行）改成 `const auto &serviceNodes = gNode->GetServiceDiscoveryManager().ServiceNodes();`，循环里用 `serviceNodes.NodesOfType(nodeType)`。
3) registration_manager.cpp 第 90 行（握手请求 lambda tryRegister 内）改成 `const auto &nodesOfType = gNode->GetServiceDiscoveryManager().ServiceNodes().NodesOfType(nodeType);` 再遍历。第 151 行的 OnHandshakeReplied 归第 1 批，不碰。
4) gate/main.cpp 第 390 行改成 `n.MutableNodeInfo().set_player_count(count);`，它在 loop 的定时器里。
5) 全量 grep `GetNodeInfo()`，凡是绑到非 const `NodeInfo&`，或传给接受 `NodeInfo&` 的函数的，按「是否真的写」二选一：真写改 MutableNodeInfo()，否则改 const&。B 阶段已核对过的写入方只有上面这些加上 node.cpp 里的几处。

### 4.13 B5 更新依赖「NodeInfo 是 thread_local」的过时注释

**文件**：
- `cpp/libs/engine/core/node/system/node/client_endpoint.h`
- `cpp/nodes/battle/handler/grpc/battle_node.cpp`
- `cpp/nodes/battle/logic/battle_room_manager.cpp`
- `cpp/nodes/battle/logic/battle_room_manager.h`
- `cpp/nodes/battle/main.cpp`

只改注释，不改行为。位置：client_endpoint.h:105、battle_node.cpp:96、battle_room_manager.cpp:1538、battle_room_manager.h:319、battle/main.cpp:295。把其中「GetNodeInfo() 读 thread_local，gRPC 线程上是空的」统一改成：「NodeInfo 由 Node 持有（非 thread_local），只在 loop 线程写；跨线程读与写不同步，所以仍须在 loop 线程取」。约束本身保持不变。

### 4.14 B6 cross_zone_test：安装测试身份、每例清零，补 F008 与快照回归用例

**文件**：
- `cpp/tests/cross_zone_test/cross_zone_test.cpp`

1) 文件顶部（includes 之后）加一个非匿名的 `NodeInfo& TestLocalNodeInfo() { static NodeInfo info; return info; }`，以及一个 `::testing::EmptyTestEventListener` 子类，OnTestStart 里执行 `TestLocalNodeInfo().Clear()`。
2) main 里 InitGoogleTest 之后执行 `InstallLocalNodeInfo(&TestLocalNodeInfo());`，并 `listeners().Append(new …)`。这样精确复现旧语义（以前每例的 tlsEcs.Clear() 会顺带把 zone 清回 0），且不依赖执行顺序。
3) 第 1628、2001 行的 `GetNodeInfo().set_zone_id(...)` 改成 `TestLocalNodeInfo().set_zone_id(...)`。第 1542、1623、2001 行注释里「tlsEcs.Clear() 会清 globalRegistry，GetZoneId() 回到 0」改成「zone 由每例开始前的监听器清零，与 tlsEcs.Clear() 无关」。第 867 行注释「SessionMap 不随 tlsEcs.Clear() 清」本来就不对，改成「SessionMap 随 tlsEcs.Clear() 一起清；这里显式再清一次只为读者清楚」，或直接删掉这行注释。
4) 新增 LocalNodeInfoTest 三例：
   - SurvivesEcsClear：set_zone_id(9)，执行 tlsEcs.Clear()，期望 GetZoneId()==9。这是 F008 回归。
   - SameInstanceOnEveryThread：用 std::thread 读 GetZoneId()，期望 ==9。旧实现是 thread_local，这里读到的是 0。
   - UninstalledIsReadOnlyDefault：`prev = InstallLocalNodeInfo(nullptr)`，期望 `&GetNodeInfo() == &NodeInfo::default_instance()` 且 GetZoneId()==0，最后用 InstallLocalNodeInfo(prev) 还原。
5) 新增 ServiceNodeSnapshotTest，include "node/system/discovery/service_node_snapshot.h"，测试目录已在 include 路径上：
   - 新 uuid 返回 kAdded；同 uuid 改端口返回 kUpdated，数量不变、端口已更新；不同 uuid 追加。
   - Remove 命中返回 1，未命中返回 0；手工塞两条同 uuid 时 Remove 返回 2。
   - nodeType = eNodeType_ARRAYSIZE 时，NodesOfType 为空、HasNodesOfType 为 false、Upsert 返回 kRejected、Remove 返回 0。
本二进制已链 node_utils 与 common.pb，不需要新建工程。

### 4.15 C1 EcsContext：删除全局 registry 与知名实体，SessionMap 收成普通成员（A、B 合入后）

**文件**：
- `cpp/libs/engine/thread_context/ecs_context.h`
- `cpp/libs/engine/thread_context/ecs_context.cpp`
- `cpp/libs/engine/core/type_alias/player_session_type_alias.h`

前置门：`git grep -n "globalRegistry\|nodeGlobalRegistry\|GlobalEntity()\|GrpcNodeEntity\|ErrorEntity\|OperatorEntity\|itemRegistry" -- cpp tools` 在代码里不能再有命中，注释与 docs 除外。若 rpc_channel.cpp 的防御块还在，先补做 A7；若 aoi_delivery_test 还在用，先做 C3。
ecs_context.h：
- 删掉 globalRegistry、itemRegistry、nodeGlobalRegistry；删掉 GlobalEntity/ErrorEntity/OperatorEntity/GrpcNodeEntity 四个函数和四个私有字段。
- 在 PlayerListMap 旁新增 `using SessionPlayerIndex = std::unordered_map<SessionId, Guid>;`，并在 playerList 旁新增成员 `SessionPlayerIndex sessionPlayers;`。注释：scene 的「gate 会话号 → 玩家号」路由索引；写入方是入场预登记与 UpdateSessionDetail，擦除方是退出收尾和陈旧会话；与 playerList 同寿命、同线程（loop），由 Clear() 一并清；不是组件。
- 结构体顶部注释改成：只放 ECS 世界（actor/scene registry）、dispatcher 和玩家索引；进程、节点、请求级单例归各自所有者（NodeInfo→Node，服务发现快照→ServiceDiscoveryManager，拒绝码→response），不要再在这里造「全局实体」。
ecs_context.cpp：删掉四个 Entity() 的定义；Clear() 里删掉对三个 registry 的 clear 和四个字段的复位，加上 `sessionPlayers.clear();`。
player_session_type_alias.h：删掉 `using SessionList`；`inline SessionPlayerIndex& SessionMap() { return tlsEcs.sessionPlayers; }`，上面一行注释指向 EcsContext::sessionPlayers。保留这个访问函数是为了生产 3 个文件和约 40 行测试零改动，也不碰第 1 批的 UpdateSessionDetail。

### 4.16 C2 删除过渡宏 TRANSFER_ERROR_MESSAGE（proto-gen 重生之后）

**文件**：
- `cpp/libs/engine/core/macros/return_define.h`

前置门：`git grep -n TRANSFER_ERROR_MESSAGE -- cpp tools` 只剩 return_define.h 自身，说明 11 个生成头已经重生。满足后删掉 A1 留下的空操作宏及其过渡注释。不满足时停止并报告，不手改生成物。

### 4.17 C3 删除已无从表达的 AOI 回归用例（第 1 批落地后）

**文件**：
- `cpp/tests/aoi_delivery_test/aoi_delivery_test.cpp`

第 1 批加的 `TEST_F(AoiDeliveryTest, VisibilityNotificationsDoNotAttachComponentsToGlobalEntity)` 引用了 tlsEcs.GlobalEntity()/globalRegistry（约 153-188 行，含上方说明注释），整例删除：globalRegistry 删除后，「往全局实体挂组件」在类型上就写不出来，保证从运行期断言升级为编译期。第 53 行 SetUp 注释里「不应该往 GlobalEntity 上预挂消息组件」改成「AOI 的 S2C 发送缓冲是 aoi.cpp 内的 static thread_local」。这个文件第 1 批正在改，必须等它落地后再动，只改这两处。

### 4.18 C4 文档：规则、审计状态、进度

**文件**：
- `docs/design/ecs-component-access-rules.md`
- `docs/design/ecs-entt-misuse-audit-20261015.md`
- `docs/design/player-features-ui.md`
- `.github/copilot-instructions.md`
- `cpp/AGENTS.md`
- `docs/PROGRESS.md`

1) ecs-component-access-rules.md：
   - 从「Acceptable uses」里删掉 globalRegistry TipInfoMessage 与 nodeGlobalRegistry ServiceNodeList 两条，只动这两行，同一列表第 1 批也可能在改；
   - 新增一节「单例与非实体数据不进 registry」，附所有者表：NodeInfo→Node::localNodeInfo_（gNode->GetNodeInfo()/GetNodeInfo()，写 MutableNodeInfo）；服务发现快照→ServiceDiscoveryManager::ServiceNodes()；会话→玩家索引→tlsEcs.sessionPlayers（SessionMap()）；玩家 RPC 拒绝码→本次 response 的 error_message（规则见 player_service_interface.h）；AOI 发送缓冲→aoi.cpp 函数内 static thread_local（第 1 批）；
   - 写明「不要再造全局实体挂单例组件，也不用 ctx() 代替所有者」。
2) ecs-entt-misuse-audit-20261015.md：只改相关行的「HEAD 状态」列，表格结构不动，第 1 批也在填这一列。F004/F008/F018/F019/F020/F092 填「已修（第 2 批：单例归位）」，F021 填「部分修（itemRegistry 已删；Bag 私有 registry 见 F024）」；同时从「仍未修」小表移除 F008 行。
3) player-features-ui.md 第 22 行把「写入 TLS TipInfoMessage 后由生成层 TRANSFER_ERROR_MESSAGE 转移；直接写响应错误会被该宏覆盖」改成「拒绝码直接写本次应答的 error_message（见 cpp/libs/services/player/rpc/player_service_interface.h）」。cross-zone-scene-travel.md:180 是历史记录，不改。
4) .github/copilot-instructions.md 的 ECS 小节和 cpp/AGENTS.md 的 ECS rules / ANTI-PATTERNS 各加一条：进程、节点、请求级单例不进 registry（附所有者对照），玩家 RPC 拒绝码直接写 response。
5) docs/PROGRESS.md 追加一条，只追加：本批改动、未编译待 Codex 验证、Java 版不适用（无客户端可见契约变化，对应 Java 版本号：无），以及对 robot 里三处过时注释（robot/channel_smoke_scenario.go:431、travel_smoke_scenario.go:253/906、travel_smoke_wire.go:37）的后续清理提示。

## 5. 生成器模板与生成产物

- 模板：tools/proto_generator/protogen/internal/player_service_gen.go。删掉 CallMethod 里的 TRANSFER_ERROR_MESSAGE 三行模板，删掉 IsEmptyResponse 字段及其赋值，删掉不再使用的 strings import。
- 重生产物（只能由 Codex 跑 proto-gen-run 生成，禁止手改）：cpp/nodes/scene/handler/rpc/player/ 下的 player_activity_handler.h、player_attribute_handler.h、player_bag_handler.h、player_currency_handler.h、player_mission_handler.h、player_movement_handler.h、player_pet_handler.h、player_rollback_handler.h、player_scene_handler.h、player_skill_handler.h、s2s_player_scene_handler.h。每个文件各少一行 TRANSFER_ERROR_MESSAGE(...)，其余不变。
- handler .cpp 的守护段由生成器原样保留。A4 的改动全部在守护段内，所以重生不会覆盖。

## 6. 测试

- 生成器（Codex，仓库根目录）：先跑 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`，再跑 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run`。通过标准：git status 里生成物的变化只有 cpp/nodes/scene/handler/rpc/player/ 下 11 个 *_handler.h 各少一行 TRANSFER_ERROR_MESSAGE。出现其他生成物变化时停止，保留 `git diff --stat` 并报告：那是别的会话未重生的模板改动，不属于本批。
- Go 生成器自身：在 tools/proto_generator/protogen 下跑 `go build ./... && go vet ./...`。通过标准：无未使用 import（strings）报错。
- C++ 全量构建（Codex）：`msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`，必须串行 /m:1。顺序是 A、B 各自合入后各构建一次，C 合入后再构建一次。失败时保留首个报错文件与行号：最可能是某处把 GetNodeInfo() 绑到了非 const 引用，或缺少 tlsEcs 包含。
- 单测（Codex）：`pwsh tools/scripts/run_cpp_tests.ps1`，或逐个跑 cross_zone_test、bag_test（含 player_feature_handler_test / player_equip_test）、skill_test、aoi_delivery_test、aoi_test、routing_identity_test、rpc_controller_test、client_endpoint_test。通过标准：全绿，且新增用例都实际执行：LocalNodeInfoTest.SurvivesEcsClear / SameInstanceOnEveryThread / UninstalledIsReadOnlyDefault、ServiceNodeSnapshotTest.*、PlayerRequestRejectTest.*。
- 新增回归用例（B6、A8）：F008 的 SurvivesEcsClear（旧实现 Clear 后 zone 回到 0）；SameInstanceOnEveryThread（旧实现在别的线程读到 0）；UninstalledIsReadOnlyDefault（不造状态）；ServiceNodeSnapshot 的 upsert / remove 与非法 nodeType 不越界；CHECK_PLAYER_REQUEST 把码写进 response，放行时不产生 error_message 字段。
- 既有用例的语义保持：player_feature_handler_test 与 player_equip_test 里「先拒绝、再成功，成功应答 error_message().id()==0」的断言全部保留，这就是不跨请求串号的证据。
- 静态门（C 合入后）：`git grep -n "globalRegistry\|nodeGlobalRegistry\|GlobalEntity()\|GrpcNodeEntity\|ErrorEntity\|OperatorEntity\|TRANSFER_ERROR_MESSAGE\|CHECK_REQUEST_PRECONDITIONS\|ServiceNodeList" -- cpp tools` 在代码里零命中，允许 docs 和历史注释。
- 本地冒烟（可选，Codex，按 dev_tools 的 dev-start）：起 1 gate + 1 scene，确认三件事：etcd 注册后日志里 node_id / 端口正常、gate 能发现 scene（DependencyGate ready）、客户端 GM 指令在 prod 模式下仍收到 kFeatureUnavailable。

## 7. 并行拆分、风险与拍板项

**并行拆分**：分三个子批，文件互不重叠。A 与 B 可以并行，C 必须在 A、B 都合入之后执行。

A「请求级 tip」（18 个手写文件 + 11 个重生头）：return_define.h、player_service_interface.h、player_service_gen.go、error_handling_system.h、scene_handler.cpp（只改 ProcessClientPlayerMessage 的 GM 分支、InvokePlayerService 收尾、WriteRejectTip 注释）、8 个玩家 handler 的 .cpp（activity / mission / attribute / pet / bag / currency / rollback / scene）、rpc_channel.cpp、rpc_done_callback.h、skill_test.cpp、player_feature_handler_test.cpp、player_equip_test.cpp。A 把 TRANSFER 改成过渡空操作，所以不依赖重生时机也能编译。

B「节点身份与服务发现」（17 个文件）：node_utils.h/.cpp、node.h/.cpp、node_allocator.cpp、node_connector.cpp、registration_manager.cpp（只改第 90 行所在的 lambda）、service_discovery_manager.h/.cpp、新文件 service_node_snapshot.h、gate/main.cpp、client_endpoint.h 与 4 个 battle 文件（只改注释）、cross_zone_test.cpp。

C「收尾删除与文档」（10 个文件，串行）：ecs_context.h/.cpp、player_session_type_alias.h、return_define.h（只删过渡宏，此时 A 已合入，不构成并行冲突）、aoi_delivery_test.cpp、6 个文档。C 的每一步都有前置 grep 门。

与其他会话的边界：
- 第 1 批：本方案不碰它独占的 CreateScene / UpdateSessionDetail / RoutePlayerStringMsg / registration_manager:151 / AOI / buff / skill / player_battle。C3 等第 1 批落地后再动它写的 aoi_delivery_test。
- gRPC 迁移会话：B2 只替换 ExecuteNodeRemoval 里「删快照」那一段，不碰其后的实体与 CQ 销毁（F010）。
- muduo RPC 通道会话：A7 等它的 rpc_channel.cpp / rpc_done_callback.h 落地后再做最小删改。

**风险**：
- 规模超过 AGENTS §10.2 的 30 文件门：全批约 56 个文件，其中 11 个是重生头、6 个是文档、5 个只改注释。实施前需要用户明确授权（见 decisions_needed），或只先批准其中一个子批。
- 工作区有多个会话同时改动。scene_handler.cpp、node.cpp、aoi_delivery_test.cpp、rpc_channel.cpp（刚由 game_channel.cpp 改名）、两份 ECS 文档都有别人未提交的改动。实施代理动手前必须重新读文件，只改本规格点名的代码块，不覆盖、不回退他人改动（见 memory「同一工作区多会话并行」）。
- proto-gen 是全量重生，可能把别的会话已改模板、尚未重生的产物一起卷进来（例如 battle handler 模板、gRPC 迁移模板）。Codex 发现计划外的生成物 diff 时要停止并报告，不要一并提交。A 的过渡空宏保证在重生之前也能编译。
- rpc_channel.cpp 里专门针对全局 tip 的防御块属于 muduo RPC 通道会话。若该会话迟迟不落地，C1 删 globalRegistry 时会编译失败。所以 C1 开工前必须过 grep 门。
- 可观察行为的细微变化有三处，都无外部读方：(a) InvokePlayerService 外层 message_content.error_message 从「存在但为空」变成「不存在」，gate 的 OnSceneInvokePlayerServiceReply 是空函数，没有读方；(b) SendErrorToClient 不再带出残留的 tip parameters；(c) Empty 应答的 handler 再也不可能串号到下一条请求，这一条是缺陷修复。tip 码与 error_message 字段语义对客户端不变。
- NodeInfo 不再是 thread_local。以前从非 loop 线程误读时得到的是「静默为空」，现在得到真实数据，但与 loop 线程的运行期写（gate 的 player_count）不同步。规则仍是只在 loop 线程读写：写入口 MutableNodeInfo 加了 Debug assert，读入口不加，避免 Release 下因未发现的调用点 abort。battle 的 gRPC 线程按现有注释本就不读 NodeInfo。
- Node 未构造或已析构时（例如 thread_local 析构期），GetNodeInfo() 返回只读的默认实例，与今天「拿到空 NodeInfo」一致，但不再造状态。代价是这类误用仍是静默的，这是为了不让无 Node 的单测和关机路径崩溃而做的有意取舍，已写进接口注释。
- 测试隔离：zone 不再随 tlsEcs.Clear() 归零。cross_zone_test 改用每例开始前的监听器清零；其他测试二进制从不设置 NodeInfo，读到的都是默认实例，行为不变。若别的测试将来要设 zone，必须同样安装并清理。
- AOI 的 F075 回归用例被删除：在类型层面已无法往全局实体挂组件，保证从运行期断言升级为编译期。这个用例由第 1 批编写，需要在交付说明里告知第 1 批。
- 这是纯静态设计，没有编译、没有测试。所有「等价」结论都来自 git grep 与阅读，需以 Codex 的构建与单测结果为准（AGENTS §10.1）。

**需负责人拍板**（未拍板前不要实施相关步骤，其余步骤照常进行）：
- 本批约 56 个文件（其中 11 个重生头、6 个文档、5 个只改注释），超过 AGENTS §10.2 的「30+ 文件需停下等授权」门槛。用户说过「不怕大改」，但仍需明确确认按 A/B/C 三个子批落地：A 与 B 可并行，C 在两者合入后执行。也可以只先批准某个子批。
- 与另两个并行会话的文件交界需要用户点头或协调。一是 muduo RPC 通道会话的 rpc_channel.cpp / rpc_done_callback.h：本批要删其中为全局 tip 设的防御块，并改一行契约注释。二是第 1 批写的 aoi_delivery_test 回归用例：本批要删除它，因为 globalRegistry 删除后该用例无从表达。请确认由本批在对方落地后做这两处最小删改，还是交回对方会话处理。

**Java 版（AGENTS §12）**：不适用，Java 版不需要对应改动。Java 版（D:/luyuan/xuanming-server-mmo）不用 entt；本批是 C++ 服务端内部重构，客户端可见契约不变：tip 码、各应答的 error_message 字段、帧格式、配置表都没有变化。唯一的外显差异是「Empty 应答 handler 的 tip 串到下一条请求」这个 C++ 独有缺陷被消除，Java 版没有这种全局 tip 机制，无需对齐。交付时在 docs/PROGRESS.md 条目里注明「对应 Java 版本号：无（C++ 内部重构，无对外契约变化）」；是否在 Java 仓库 PARITY.md 登记一行备注由用户决定，本批不改 Java 仓库。

## 8. 挑战者意见（实施时必须处理）

结论：**needs_changes**

- **[medium]** 删掉 TRANSFER_ERROR_MESSAGE（A3/C2）后，会失去一个现在靠编译期保证的不变量：每个非 Empty 的玩家 RPC 应答都带 `TipInfoMessage error_message` 字段。今天生成的 CallMethod 对每个非 Empty 方法都会展开 `_resp->mutable_error_message()` 和 `= std::move(TipInfoMessage)`，新增的应答类型缺这个字段就编译不过。生成器里没有任何对 error_message 的校验（tools/proto_generator 下 grep 结果为零），测试里也没有描述符扫描。而退出闸和 GM 闸走的 WriteRejectTip（scene_handler.cpp:136-152、481）是按字段名反射写入，找不到字段时静默 return false。这样以后新增一个不带 error_message 的非 Empty 应答，拒绝码会悄悄丢掉，客户端看到的是「成功」，正是 scene_handler.cpp:474-476 注释里警告的同一类缺陷。
  - 修法：把这条约束换一种机械方式继续守住，三选一：(a) 生成的 CallMethod 每个非 Empty 的 case 生成一行 `static_assert(std::is_same_v<std::remove_pointer_t<decltype(std::declval<Resp&>().mutable_error_message())>, ::TipInfoMessage>)`。不搬运，也不碰运行期；(b) 新增单测，遍历 gPlayerService 全部方法的响应描述符，断言非 Empty 的应答都有 TipInfoMessage 类型的 error_message；(c) 让生成器在生成期校验。同时把 WriteRejectTip 注释里「与生成层同一个 error_message 字段名契约」改成指向新的守卫。
- **[medium]** A8 要改的两个测试文件，第 1 批正在改，还没提交：skill_test.cpp 在 HEAD 之上多了 129 行（F051 技能相位用例），player_feature_handler_test.cpp 多了 246 行（F034/F035/F074 的 actor 组件用例，见 git diff）。parallel_split 只说 A/B/C 之间互不重叠，与第 1 批的边界清单里没有这两个文件。另外 A8 给的 player_feature_handler_test 行号（32-33、53、63、93、105、119、126、170）已经和工作区对不上，实际在 50-51、71、81、111、123、127、145、152 一带，说明规格写完之后文件又被别人改过。
  - 修法：把 skill_test.cpp 和 player_feature_handler_test.cpp 加进「与第 1 批的交界」：A8 等第 1 批落地后再动，或者交给第 1 批会话顺手做。一律按内容定位，不用行号。A8 的其余部分（player_equip_test.cpp）不受影响。
- **[low]** C3 要整例删除 AoiDeliveryTest.VisibilityNotificationsDoNotAttachComponentsToGlobalEntity。但这个用例的前半段（aoi_delivery_test.cpp:156-181）还在断言三条通知路径真的发出了消息，并解析 BeforeLeaveSceneHandler 路径上 ActorDestroyS2C 的消息体，检查 entity。这个文件里其他用例都不覆盖离场时的 ActorDestroy 消息体。整例删掉会丢这部分功能覆盖。
  - 修法：只删 183-187 行那四条 GlobalEntity/globalRegistry 断言和上方的 F075 说明，用例改名（例如 VisibilityAndLeaveNotificationsCarryCorrectBodies），前半段的投递和消息体断言保留。
- **[low]** registration_manager.cpp 的定位用的是 HEAD 行号（规格写「第 90 行」「第 151 行 OnHandshakeReplied 归第 1 批」），但工作区这个文件已经被 muduo RPC 通道会话改写（+133/-96）。OnHandshakeReplied 已经不存在，换成了 OnHandshakeCallDone/OnHandshakeSucceeded，后者已改用 TryGetRegistry。B4 要改的 ServiceNodeList 读取现在在 OnNodeHandshake 的 tryRegister lambda 第 191 行，紧挨着第 189 行的 `tlsNodeContextManager.GetRegistry(nodeType)`。这里的 nodeType 来自对端，正是第 1 批「GetRegistry 加 Release 边界检查」（F093/C007）同一类问题，两处改动只隔一行。gate/main.cpp 写 player_count 的位置也已经从 390 行漂到 405 行，node.cpp 各处都偏了 2 到 24 行（例如 1594 行在工作区是 1618 行）。
  - 修法：B4 改成按内容定位（「OnNodeHandshake 里 tryRegister lambda 中的 ServiceNodeList 读取」）。动手前和第 1 批确认它会不会改同一个 lambda 里的 GetRegistry；会改就由一方顺带完成这两行。把规格开头「行号以工作区为准」的说法改正，或者统一删掉行号。
- **[low]** A3/C2 的通过标准（proto-gen-run 之后只有 11 个生成头各少一行）在当前工作区一定会失败。proto/common/base/rpc_message.proto 已被 muduo RPC 通道会话修改（10-15 04:09），而受跟踪的 cpp/generated/proto/common/base/rpc_message.pb.h 还是 10-14 的旧版本，没有重生。第 1 批也改了 player_data_loader.cpp.tmpl/.h.tmpl。全量重生会把这些产物一起带出来。另外测试清单只有 go build/vet，没有跑 protogen 模块里现有的 Go 单测。
  - 修法：在 C2 的前置门里明确写上：先等 muduo RPC 通道会话和第 1 批各自完成重生，或得到对方许可，再执行 proto-gen-run。也可以接受由对方的模板或 proto 改动产生的 diff，但要逐一列出。测试清单加一条：在 tools/proto_generator/protogen 下执行 `go test ./...`。
- **[low]** A2 在 PlayerService::CallMethod 的契约里写了「延后完成（RpcDoneCallback）适用同一规则」，A7 又把 rpc_done_callback.h 的条款改成指向 player_service_interface.h。但 PlayerService::CallMethod 的签名里没有 done，玩家 handler 根本无法 Adopt。RpcDoneCallback 只用于节点级的 google::protobuf::Service（rpc_channel.cpp:922），全仓 cpp 下目前也没有任何 Adopt 调用方。让 engine/core 的头文件指向 libs/services/player 的规则，层次是反的。而且「规则只在一处声明」本来就要求不要在这里再写一遍。
  - 修法：player_service_interface.h 的契约里不提 RpcDoneCallback。rpc_done_callback.h 里关于全局 tip 的那条直接删掉：全局 tip 已经不存在，不需要再留指针。如果确实要写，就写成与类型无关的一句「拒绝码写 response<T>() 自己的字段」。
- **[low]** C4.1 已经过时。工作区里的 docs/design/ecs-component-access-rules.md 已被第 1 批重写（「组件的三种初始化方式」一节），规格要删的「Acceptable uses」里 globalRegistry TipInfoMessage 和 nodeGlobalRegistry ServiceNodeList 两行，在工作区已经不存在了（HEAD 版在第 30-31 行）。
  - 修法：C4.1 改成：在第 1 批落地后的版本上新增「单例与非实体数据不进 registry」一节，附所有者表。不再写「删两行」。
- **[low]** B6 新增的 LocalNodeInfoTest.SameInstanceOnEveryThread 用例，在测试里把「从非 loop 线程读 NodeInfo」固化成了正确行为。可 B1 和 B2 的接口注释写的是「只在 loop 线程读写，跨线程读与写不同步」，battle 那几处注释（B5）也仍然要求在 loop 线程取。用例和契约自相矛盾，后来的人可能据此在 gRPC 线程读 NodeInfo。
  - 修法：删掉这个用例，或改成只断言「未安装时返回只读默认实例、不造状态」。F008 的回归保留 SurvivesEcsClear 就够了。
- **[low]** 规格里有几处计数与实际不符，实施方如果按数核对会困惑。rollback 的 RejectGmRollbackRpc/RejectIfFrozen 调用点是 12+6=18 个，不是 24 个。attribute 和 pet 各是 9 处 SetTip 调用，不是 10 处。player_equip_test 里 `EXPECT_EQ(0u, Tip().id())` 是 16 处，不是 17 处。rpc_done_callback.h 要改的条款在第 20-21 行，不是 22-23 行。另外 gate 的 OnSceneInvokePlayerServiceReply 所在文件 cpp/nodes/gate/rpc_replies/scene_response_handler.cpp，在工作区已经被删除（git status 显示 D）。「没有读方」这个结论依旧成立，但证据要更新。
  - 修法：计数改成实际值，或者改成「全部调用点」，不写具体数字。A5 里的证据改为「工作区已无任何 InvokePlayerService 的发送方和应答处理」。

**遗漏的调用点**：
- cpp/libs/services/scene/player/system/player_landing.h:126 注释写的是「应答类型是 Empty：本函数不写 TipInfoMessage(全局 tip 会污染…)」，改造后这个理由就过时了（不在第 1 批的文件清单内）
- docs/design/muduo-rpc-channel.md:259 延后完成路径「不得用全局 tip(SetTip / TRANSFER_ERROR_MESSAGE…)」是现行设计规则，C4 没有列入
- docs/design/landing-recovery-sync-digest.md:182 写着「返回 Empty 的 handler 里禁止 SetTip(全局 tip 会污染…)」
- docs/design/equipment-attributes.md:476 与 docs/design/equipment-enhance-inlay.md:503 把 handler 套路描述为「失败 SetTip」，A4 删掉 SetTip helper 之后就失效了
- cpp/libs/engine/core/node/system/etcd/etcd_service.cpp:257、cpp/libs/engine/core/node/system/node/node.cpp:1599、cpp/libs/engine/core/node/system/node/node_allocator.h:21 的注释仍然称「ServiceNodeList」快照，B 改名为 ServiceNodeSnapshot 后会对不上
- cpp/tests/aoi_test/aoi_system_test.cpp:38 的注释提到「globalRegistry」
- cpp/nodes/scene/handler/rpc/scene_handler.cpp:133 与 :477 的注释写「与生成层同一个 error_message 字段名契约」，生成层不再引用 error_message 之后这个契约就没有守卫了（见 problems 第 1 条）
- cpp/tests/bag_test/player_feature_handler_test.cpp 与 cpp/tests/skill_test/skill_test.cpp 正在被第 1 批修改（未提交），应列入与第 1 批的交界
- cpp/libs/engine/core/node/system/registration/registration_manager.cpp:189 的 `GetRegistry(nodeType)` 用的是对端传来的 node_type，与 B4 要改的第 191 行在同一个 lambda 里，需要与第 1 批的 F093/C007 修复协调

## 9. 回执

按 [README.md](README.md) §6 的格式交回。另外逐条说明 §8 每个 high/medium 问题是怎么处理的。

### 2026-10-15 Codex：S 子批，会话索引归位

**范围**：本会话获授权开始修改后，先完成独立的会话索引迁移。NodeInfo、服务发现与 tip 迁移未实施，不将本子批计为第 2 批全部完成。

- `ecs_context.h/.cpp`：普通成员 `sessionPlayers` 持有会话路由索引，与 `playerList` 同线程、同清理边界；删除全仓无调用的 `itemRegistry`、`ErrorEntity()`、`OperatorEntity()` 及对应字段。保留 actor 0 号句柄退役、dispatcher 清理与其他会话的既有修改。
- `player_session_type_alias.h`：保留 `SessionMap()` 调用接口及实际 map 类型，直接返回上下文成员；生产调用点无需修改。独立清理 globalRegistry 不再清掉会话路由；完整 `EcsContext::Clear()` 仍清掉该索引。
- 新增独立 `ecs_context_test` 工程及 5 条行为用例，并登记到 `run_cpp_tests.ps1`。工程直接编译生产 `ecs_context.cpp` 和现有 gtest 源码，不链接旧业务静态库，不增加依赖、不改生成产物。
- 审计号：F092 / F099 的 SessionList 部分、F020 的两个死实体入口、F021 的死 itemRegistry 部分。NodeInfo / tip / Bag 相关部分仍待后续处理；审计总表 HEAD 状态按原规则留给整批审查方更新。

**与原方案不同**：C1 中可独立验证的会话索引及死入口先行；GlobalEntity / GrpcNodeEntity 和两份仍有使用者的全局 registry 保留。其余跨会话文件与 gRPC 收尾均未修改。§8 关于 tip 生成器不变量、NodeInfo 并发读取和传输会话交界的问题尚未进入实施，不标为已解决。

**实际运行证据**：

- 用本次断言和真实上下文实现，只在忽略目录将 getter 替换为 HEAD 中原有实现作为对照：5 条中 2 条失败，分别为查询创建组件、全局组件池清理丢索引；另 3 条通过。没有回退共享工作区。初次测试夹具编译遇到 gtest 对 `entt::null` 的转换冲突，已改为实体值的布尔比较；原错误日志保留。
- 当前工程：`MSBuild.exe cpp/tests/ecs_context_test/ecs_context_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64` 退出 0；实际运行 **5/5 通过**，退出 0。
- 统一入口：`pwsh -NoProfile -File tools/scripts/run_cpp_tests.ps1 -Build -Filter ecs_context_test` 显示构建 OK、5/5 全过，退出 0。运行使用执行环境内现有 PowerShell，没有安装或修改工具。
- 日志与 XML：`build/ecs-context-session-index/` 的 `before-build.log`、`before-tests.log/xml`、`after-build.log`、`after-tests.log/xml`、`runner.log`；初次夹具编译错误为 `initial-harness-build-error.log`。

**边界**：EcsContext 的布局改变，生产节点及包含该头的静态库必须统一重编，不能混用旧库。当前工作区缺少同步后的 RPC / 落地摘要生成契约，本次没有触碰其他会话的生成任务，因此没有生产节点整编、bag/cross_zone/skill 整批、联机或压测通过证据。工程附带的裸指针检查器输出 SKIP，不能视为静态检查通过。下一个完整单例迁移批次预计超过 30 个文件，仍须遵守 AGENTS.md §10.2 的范围门禁。

**Java**：0.1.0-SNAPSHOT，无新增版本或业务实现。本次仅调整 C++ EnTT 存储机制，无客户端契约变化；Java 不使用 EnTT，在 PARITY.md 登记不适用，不宣称其他既有玩法差异已经对齐。未 commit、未 push。

### 2026-10-15 Codex：第 2 批源码收尾（生成与运行验收暂缓）

NodeInfo 已移为 Node 成员，服务发现改由 ServiceDiscoveryManager 的 ServiceNodeSnapshot 持有；请求级 tip 直接写本次 response，延后应答同样遵守本次所有权。SessionMap 保留普通成员索引，所有调用迁走后删除 EcsContext globalRegistry/nodeGlobalRegistry 及两 getter。静态约束非精确 Empty 应答必须提供 TipInfoMessage error_message，不以字符串包含 Empty 判断。

新增/适配节点身份跨 Clear、默认实例、安装恢复、跨线程同一地址、快照去重/移除/边界与请求拒绝回归。atomic 仅发布身份地址，不承诺内容并发和引用寿命；跨线程用例在 owner 固定且无并发写时读取。生成层不再转移全局 tip，旧生成头的空 TRANSFER 宏暂留，统一重生成后删除。

与 S 子批证据区别：当前 EcsContext 布局和测试已继续变化，先前 5/5 不代表本版本通过。未执行本批最终生成、构建和测试；完整回执、静态证据、Java 0.1.0-SNAPSHOT 的不适用边界及恢复验证顺序见 [总回执 §7](README.md)。

### 2026-10-16 第6批更正（保留以上历史回执）

“旧 getter 对照 2/5 失败”对应 globalRegistry 时期的旧断言，当前四条会话索引形状用例对基线同样通过，已删除。会话索引不进 registry 由 globalRegistry 删除在编译期守住；保留 ContextClearDropsBothIndexesAndAllowsNewSession 验证清理。NodeInfo 门面删除 atomic 与两个越界于线程契约的用例，空 TRANSFER_ERROR_MESSAGE 宏已删除；完整运行证据见第6批最终回执。
