# 交接:C++ gRPC 迁到 muduo 驱动·第 3–5 批(2026-10-15,Claude)

负责人 10-15:「不用管验证,全部做完」,随后「把交接给我,我让别的账号去做」。本文件是给接手会话的唯一入口。

- **总设计**:[grpc-muduo-transport.md](../../design/grpc-muduo-transport.md)。§0–§9 线上契约与分层,§10 分批计划,§13 第 2 批,**§14 第 3 批的决定**(本次写的,先读)。
- **现状调查附录**(本目录,只读调查原样保留,基线 HEAD 526780b4e,第 3 批前半落地后部分行号已漂移):
  - [survey-client-gen.md](survey-client-gen.md) —— 生成客户端全链路、模板对 grpc 类型的逐点依赖、改动面;
  - [survey-call-sites.md](survey-call-sites.md) —— 全部出站调用点、全局处理器安装点、各调用点的迁移做法;
  - [survey-server.md](survey-server.md) —— scene / battle 的 gRPC 服务端、Agones 许可、关机;**第 4 批需求清单 12 条**;
  - [survey-build.md](survey-build.md) —— 官方 gRPC 在构建里的全部依赖(protobuf / abseil / BoringSSL / gtest / zlib / protoc 都从 grpc 树来);
  - [survey-muduo.md](survey-muduo.md) —— 三份 muduo 树、连接失败回调、Windows wakeup、墙钟定时器。
- **硬规矩**(AGENTS.md):中文;不手改 `generated/`(`cpp/generated/grpc_client/grpc_call_tag.h` 是文件里自注明的手写例外);
  Claude 不编译 / 不测试(负责人自己编译,见记忆「编译由用户自己做」),交付写明「未编译」;不 push;同一工作区多会话并行,动手前重核文件归属。

## 0. 2026-10-16 接手更新(先读这节)

- **第 3 批代码已补完**(仍未编译):调用方签名、gate 收口、选点状态机、旧 CQ 测试、muduo 连接失败回调、Windows 唤醒、
  测试工程链接项。逐项落地记录见设计 §14.3;下文 §2、§3 是转交时的状态,保留作历史。生成产物已于 10-15 22:27 按新模板
  重生成(不是本交接链上的会话跑的)。
- **负责人澄清目标**(10-15,原话):「我不是说删除 grpc 所有代码,grpc 库还是可以用,但是不要启动 grpc 任何线程,明白了吗?
  我需要的是线程数量可控」。因此:
  - 第 4 批(服务端改 muduo + nghttp2)照做,它与第 3 批一起消除全部 gRPC 线程;11 个待定问题负责人选「按最标准做法由接手方定」。
  - **第 5 批改口径**:不换依赖、不删 grpc 子模块、不删 `*.grpc.pb.*`。改为运行期守卫(采样时核对 `grpc_is_initialized()`、
    第一次发现即 ERROR)、回归用例(走完生成客户端后 gRPC 运行时仍未初始化)与清理失效的线程配置 / 文档。
  - 负责人同一轮对 protobuf / abseil 改独立子模块、gtest / protoc / zlib 来源选了推荐项,但那是基于「删 grpc」的前提,
    **只有将来真要去掉 grpc 依赖时才适用**,现在不动依赖。
- **第 4 批设计已定稿**:设计 §15(11 个待定问题的裁决、API、线上契约、守护段迁移、Agones 异步许可、关机、验证步骤)。
- **后续(负责人 10-16 要求出清单,交 Codex 执行)**:删除 gRPC 生成客户端的全局应答处理器、只留每次调用回调,
  逐文件改动清单见 [global-handler-removal.md](global-handler-removal.md)(分 A–D 四批授权;gate 直连模式默认走「类型擦除回调」,
  要先删直连模式需负责人豁免 D61 前置)。与第 4 批无关,不要混做;前置是第 3 批编译通过。
- **regen 新规则**(RpcChannel 批 7 会话 10-16 通知,详见 muduo-rpc-channel.md §20):`proto/message_id.txt` 有
  `N=reserved:<KeyName>` 墓碑行。regen 前**必须先** `proto-gen-build`(旧 exe 会把墓碑号当空号回收并丢掉墓碑行;dev_tools
  已加防护);从 worktree 合入只带 proto 源码与手写代码,不带 message_id.txt 与生成物,合入后确认 message_id.txt 没删行、
  reserved 行没变回活行。

## 1. 进度一览

| 批 | 内容 | 状态 |
|---|---|---|
| 1 | nghttp2 + `muduo_grpc` 客户端 unary + gate 路由链路 | 完成;Codex 续编时已随 21 个工程构建通过、通道用例 22/22 |
| 2 | 双向流、周期 keepalive、etcd 迁 muduo_h2(流断重建、按前缀对账) | 完成,三轮评审修复;etcd 评审修复后的那部分**未编译** |
| 3 | 生成客户端全部走 muduo_grpc、删 CQ 与 5ms 轮询、选点改新状态机、muduo 连接失败回调 / Windows wakeup | 10-16 代码补完,**未编译**(设计 §14.3) |
| 4 | 服务端:muduo `Acceptor` 自管连接 + nghttp2 server session,scene / battle handler 改 loop 内派发,Agones 许可改异步,关机 GOAWAY 排空 | 10-16 设计定稿(设计 §15)并分六组落码,**未编译、未重生成**(§4) |
| 5 | ~~删官方 gRPC~~ → 10-16 改为无 gRPC 线程守卫 + 清理(§0) | 未开始 |

## 2. 第 3 批已完成(都未编译、未重生成)

已随负责人快照提交:`0249bee0a`、`b24a5b772`、`f0dadf5b0`、`a105c3efd`(后者含 `InstallFromEnv` 兼容改写、三个节点 main 与 `cpp/nodes/_template` 的注释)。
未提交的只有文档:本交接目录、设计 §14 的状态行、docs/README 索引、PROGRESS 条目。

- **调用类型**(`cpp/generated/grpc_client/grpc_call_tag.h`,手写):不含任何 grpc 类型。新增 `GrpcMetadata`、`kGrpcStatus*` 常量、`FindGrpcMetadata`。`GrpcCallOutcome` 增加 `initialMetadata` / `trailingMetadata` / `FindInitialMetadata`。`GrpcReplyContext = GrpcCallOutcome`(全局应答处理器的新参数)。`GrpcCallFailure` 改为 `{messageId, method, const GrpcCallOutcome& outcome, sentMetadata}` 加 `code()` / `message()`。删掉 `GrpcTag`。
- **模板**(`tools/proto_generator/protogen/internal/template/`):
  - `grpc_async_client.{h,cpp}.tmpl` 重写:
    - 每方法生成 `k<Svc><Method>GrpcPath`(`/<proto package>.<Service>/<Method>`,无 package 时 `/<Service>/<Method>`)。
    - 全局处理器签名 `void(const GrpcReplyContext&, const Resp&)`。
    - 所有 Send 汇到 `Start<Svc><Method>Call`:先从实体取 `MuduoGrpcChannelPtr`,再序列化,metadata 应用层 `Base64Encode`,steady 截止,最后调 `StartUnaryCall`。应答解析失败报 INTERNAL;缺通道 / 序列化失败经 `QueueGrpcLocalCompletion` 异步报 UNAVAILABLE / INTERNAL。
    - 调用状态放在 `shared_ptr`,不进 ECS。
    - 流式方法(含纯服务端流)只保留三个旧 Send 重载,调用即记 ERROR,供 gRpcMethodRegistry 链接。
    - 不再 include `.grpc.pb.h`(服务元数据头已带 `.pb.h`)。
  - `grpc_init_total.{h,cpp}.tmpl`:只剩 `Set[IfEmpty][Failed]Handler` 广播与 `SetGrpcCallDeadline`。`InitGrpcNode` / `HandleCompletedQueueMessage` / `BeginShutdownGrpcNode` / `PollGrpcNodeShutdown` 删除。
- **生成器**:`grpc_process.go` 删掉 `GrpcIncludeHeadName` / `GrpcCompleteQueueName` 字段。`grpc_process_test.go` 改成断言「无 CQ / 无 grpcpp / 用 GrpcReplyContext」,新增 `TestGrpcAsyncClientTemplatesUseMuduoChannelAndRejectStreaming`。只改模板不需重编 proto-gen;`grpc_process.go` 改了,要先 `proto-gen-build`。
- **运行期接缝**:
  - `cpp/libs/engine/core/network/grpc_client_runtime.h` 重写,提供 `FindGrpcChannel`、`HasUsableGrpcClient`(挂着通道且非 SHUTDOWN)、`QueueGrpcLocalCompletion`、`GrpcClientLocalCompletionsDrained`(thread_local 计数)。
  - `muduo_grpc_channel_slot.h` 删掉 `MuduoGrpcChannelFactory`,只剩 `MuduoGrpcChannelPtr`。
- **建连**:`NodeConnector::ConnectToGrpcNode` 对所有 gRPC 节点直接 `muduo_grpc::Channel::Create(*gNode->GetLoop(), ip, port)`(只认 IPv4 字面量),建好即 `GetState(true)` 预连。
- **Node**:
  - 关机阶段 0 只清 `MuduoGrpcChannelPtr`,然后 `queueInLoop(FinalizeTeardownInLoop)`。
  - 拆除开头若 `GrpcClientLocalCompletionsDrained()` 为假就让一轮,再进来。
  - `PollGrpcClientsInLoop` / `DrainGrpcClientsInLoop` / `CompleteNodeRemoval` / `grpcHandlerTimer` / `grpcClientDrainTimer_` / `GrpcChannelCache` 全删。
  - 摘节点改为:先 `remove<MuduoGrpcChannelPtr>`,再触发 `OnNodeRemoveEvent`,再直接 `DestroyEntity`。
- **etcd**:
  - `GrpcEtcdTransport` 与 `grpc_channel_cache.h` 已删(core 工程登记已清)。
  - `etcd_transport::Create` 默认建 `MuduoEtcdTransport`,`RebuildsStreams()` 删除。
  - EtcdService 删掉旧路径:`OnLegacyRange` / `OnWatchResponse` / `ScheduleWatchReconnect` / `StartWatchingPrefixes` / `streamAware_` / `revision` / 5ms CQ 轮询 / `SetEtcdHandler` 清空。
  - `NODE_ETCD_TRANSPORT=grpc` 只打 WARN。

## 3. 第 3 批剩余工作(按顺序;**做完前工作区编不过**)

当前状态:模板已改,**生成产物还是旧的**。旧产物引用已删除的 `GrpcTag` / `GrpcClientRuntime` / `ClientContext` / CQ,调用方也没改。所以 3.1 的重生成与 3.2–3.5 的调用方改动要一起完成,才可能编过。

### 3.1 重生成
仓库根目录执行:先 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`(`grpc_process.go` 改过),再 `... -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`。

重生成后核对:
- `cpp/generated/grpc_client/**` 里不再有 `CompletionQueue` / `ClientContext` / `grpcpp` / `.grpc.pb.h`;
- `grpc_init_client.{h,cpp}` 只剩处理器广播与 deadline。

接着跑 `go test ./internal/... ./cmd`(在 `tools/proto_generator/protogen`)。

### 3.2 全局处理器调用方签名迁移(`ClientContext` → `GrpcReplyContext`,`failure.status` → `failure.outcome`)
- `cpp/nodes/gate/main.cpp`(约 :359-376):
  - `SetIfEmptyHandler` 的 lambda 改为 `(const GrpcReplyContext& ctx, const Message& reply)`;
  - `SetIfEmptyFailedHandler` 里的 `failure.status.error_code()` / `error_message()` 改为 `failure.code()` / `failure.message()`。
- `cpp/libs/engine/session/system/session.{h,cpp}`:`GetSessionDetailsByClientContext(const grpc::ClientContext&)` 改成接 `const GrpcReplyContext&`,内部用 `ctx.FindInitialMetadata(kSessionBinMetaKey)` 后交 `ParseSessionDetailsMetadataValue`。值的含义不变:`-bin` 已解线上一层,应用层仍是 base64 文本。session 工程要能 include `grpc_client/grpc_call_tag.h`。
- `cpp/nodes/scene/id_segment_bootstrap.cpp`:
  - `OnResponse(ctx, …)` 里的 `ctx.GetServerInitialMetadata()` 改为 `ctx.FindInitialMetadata("x-idseg-seq")`;回显值是应用层 Base64 后的文本,与改前同义,比对逻辑照旧。
  - `OnFailure` 改用 `failure.code()` / `failure.message()`;
  - 删 `#include <grpcpp/grpcpp.h>`;
  - `PickReadyDataServiceNode` 见 3.3。
- `cpp/nodes/scene/rpc_replies/scene_manager_response_handler.cpp`:两处处理器签名与 `DescribeGrpcFailure`。注意 worktree `blissful-morse-969d44` 正在改这个文件(CreateScene 改每次调用回调),先协调。
- `cpp/nodes/scene/rpc_replies/player_inbox_response_handler.cpp` 与 `cpp/libs/services/scene/player/system/player_inbox_system.cpp`:另一会话(玩家收件箱)的文件。签名同上;`kGrpcUnimplemented` 改用 `kGrpcStatusUnimplemented`;`PickReadyPlayerInboxNode` 见 3.3。动手前先与该会话确认。
- 测试:`cpp/tests/rpc_controller_test/grpc_client_lifecycle_test.cpp` 整个建在 CQ / Stub / `grpc::ServerBuilder` 上,已失去被测对象。删掉它,并从 `rpc_controller_test.vcxproj` / filters / CMake 移除。其中「每次调用回调」三条(PerCallDone*)的语义,改到第 4 批有 muduo 服务端后用回环重写。这个文件由「C++ RPC 异步回调链式调用设计」会话补过用例,通知它。
- `cpp/tests/cross_zone_test/cross_zone_test.cpp` 约 :1855:`InitGrpcNode` + Begin / Poll 改为挂一个不连的 `MuduoGrpcChannelPtr` 或直接删掉该段;worktree `blissful-morse` 也在改它。

### 3.3 选点改新状态机
- `cpp/libs/engine/core/network/node_utils.{h,cpp}`:
  - `Candidate.channelState` 由 `std::optional<grpc_connectivity_state>` 改为 `std::optional<muduo_grpc::ConnectivityState>`,去掉 `<grpc/impl/connectivity_state.h>`;
  - `GetSceneManagerEntity` 改为 `FindGrpcChannel(...)->GetState(true)`;
  - `ClassifyCandidate` 映射五态;
  - :52-64 的注释依赖官方 pick_first 的「30 分钟回 IDLE」,按 muduo_grpc 的状态机重写(§5)。
- `cpp/tests/routing_identity_test/scene_manager_selector_test.cpp`:`GRPC_CHANNEL_*` 换成 `muduo_grpc::ConnectivityState::k*`。
- `PickReadyDataServiceNode`(id_segment_bootstrap.cpp:91-108)与 `PickReadyPlayerInboxNode`(player_inbox_system.cpp:63-80):`view<NodeInfo, MuduoGrpcChannelPtr>`,只认 `GetState(true) == kReady`。

### 3.4 gate 收口
- `cpp/nodes/gate/handler/rpc/router_muduo_forward.{h,cpp}`:`InstallChannelFactory` 引用了已删的 `MuduoGrpcChannelFactory`,**必须删**(node_connector 已统一建通道)。`SendForward` 可以保留,也可以改用生成的 `SendClientRpcRouterForward(…, GrpcDone, GrpcCallOptions{metadata})`,择一。`AllChannelsIdle` 保留给关机排空判定。
- `cpp/nodes/gate/main.cpp`:
  - 删 `GATE_ROUTER_TRANSPORT` 分支与 `InstallChannelFactory` 调用(约 :274-299);
  - 「等 beforeShutdown 关掉的会话走完断线处理、路由通道空闲再放行 Finalize」的排空判定,改为路由模式下**一律安装**(以前只在 muduo_h2 下);
  - `cpp/nodes/gate/gate_router_transport.h` 与 `gate_security_test.cpp` 的 `GateRouterTransport.*` 用例:改为「grpc 值打 WARN 仍走 muduo_h2」,或连同开关删除。
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp`:另一会话「Route 水平扩展消息乱序问题」持有,先协调。
  - `SendRawViaRouter` 的官方分支,以及对 `grpc::CompletionQueue` / `ClientRpcRouterStubPtr` 的 `all_of` 守卫,都要删;
  - `PickRandomNode` 的候选条件简化成 `HasUsableGrpcClient`;
  - 约 :1240 那段「CompletionQueue 随后析构……F29」的注释已过时。
  - 直连模式的 `Login.Disconnect` 与通用 sender 不用改,生成层已换传输。

### 3.5 muduo:连接失败回调 + Windows wakeup(细节见 survey-muduo.md)
- **两份 Windows 头必须逐字同步**:使用方 include `third_party/muduo/muduo/net/*.h`,muduo.lib 却用 `cpp/libs/engine/muduo_windows/src/muduo/net/*.h` 编译。`TcpClient.h` 只加 out-of-line 方法,不加数据成员,否则就是静默的 ABI 错配。
- 连接失败回调:
  - `Connector.{h,cc}` 加 `setConnectFailedCallback`,在 `retry()` 排下一次之前回调;致命错误 / default 分支也回调。
  - `TcpClient.{h,cc}` 把回调转给 Connector,替换 :68 的 FIXME。
  - Windows `Channel.cc:90-101` 在 POLLHUP 时提前 return,可能吞掉 Connector 的错误回调,需先实测 WSAPoll 对拒连报什么事件。
  - Linux 走 `tools/archived/muduo_linux_overlay/muduo/net/{Connector,TcpClient}.{h,cc}`,需新增 overlay 文件(上游拷贝加 LOCAL DELTA),`setup_dependencies.sh` 会自动拾取。
  - 使用方:`cpp/libs/engine/muduo_grpc/http2_transport.cpp` 在 `Connect()` 里用 weak_ptr 绑定回调,失败即 `CloseWithError(UNAVAILABLE)` 并停掉 Connector 的内部重试,Channel 随即进 TRANSIENT_FAILURE。
  - 补用例:connectTimeout 设长、目标端口不监听,断言 1s 内失败。
- Windows wakeup:`muduo_windows/src/muduo/net/EventLoop.cc` 的 `createEventfd` / `wakeup` / `handleRead`,配合 `CrossPlatformAdapterFunction.{h,cc}`,改成自连接的回环 UDP socket(单 fd,EventLoop.h 布局不变)。5ms 轮询删掉后,不修这一处,Windows 上跨线程 `queueInLoop` 最坏要等 100ms。Linux 不受影响。
- 可选(§6 列为第 3 批,但不是删 CQ 的前提):muduo TimerQueue 改单调时钟(Windows 树加 Linux overlay)。

### 3.6 构建
core 与 grpc_client 从此依赖 muduo_grpc + nghttp2:
- **Windows**:所有链 core / grpc_client 的工程(3 个节点加约 25 个测试工程,清单见 survey-build.md)的 Debug|x64 要加 `muduo_grpc.lib;nghttp2.lib`,以及 `third_party/nghttp2/install_vs2026_dbg/lib/`。
- **Linux**:CMake 加 `-lmuduo_grpc`(组内)与 `-Wl,--exclude-libs,libnghttp2.a nghttp2`。
- 可与第 5 批去 grpc 链接项**合并成一次扫荡**。

### 3.7 文档
- `docs/design/grpc-client-deadline-failure-callback.md`:§11 的「未 regen」,以及 :316「两种传输都在 grpc::ChannelInterface 之下」,都与事实不符,要改;同时写明 `GrpcCallFailure` 的新形状。
- 设计 §14 补一节「落地记录」。
- PROGRESS 追加条目,注明每一项是否已编译。

## 4. 第 4 批:服务端(以设计 §15 为准;2026-10-16 已落码,未编译)

**唯一依据是 [grpc-muduo-transport.md](../../design/grpc-muduo-transport.md) §15**(决定、结构、API、线上契约、恰好一次、生成代码形状、
守护段迁移、Agones 异步许可、关机、运维、验证、分组)。本节转交时写的要点已被 §15 取代,更正如下:
- 方法是 **15 个**,不是 13 个:scene 9 个(多了 `AssetAckReceipt`)、battle 6 个(多了 `QueryBattleCreation`),见 §15.2 #1。
- 接入不用 `TcpServer`,改用 muduo `Acceptor` + 自管 `TcpConnection`,`muduo_grpc::Server` 是连接的唯一强持有者(§15.4.1)。
- survey-server.md 末尾的 11 个待定问题已按「最标准做法」裁决(§15.3):不设回退开关;每连接 MCS=256、全节点在途 1024、超限回
  `RESOURCE_EXHAUSTED`;停止监听 = 经 `queueInLoop` 销毁 Acceptor;排空 2s + GOAWAY/PING 1s + 宽限 1s;bind 注册的 IPv4。
- 非空手写段是 5 段(含 battle `DestroyBattle` 的 `FAILED_PRECONDITION "placement_operation_mismatch"`),三条手写文案逐字保留;
  scene CreateScene 停机期间新增 `UNAVAILABLE "scene_node_shutting_down"`(§15.3 Q9)。
- survey-server.md 作为只读调查原样保留,行号与上述几点以 §15 为准。

**落地状态**(按 §15.15 分组并行落码,每组一轮语义评审;**全部未编译、未运行、未重生成**):

| 组 | 内容 | 状态 |
|---|---|---|
| G1 | muduo_grpc 服务端库:`address`、`method_table`、`server_session`、`server_call.h`、`server`,`message_framing.cpp` 改 1 行;muduo_grpc_test 新用例(`server_session_test` / `server_test` / `server_interop_test`、`test_h2_client`);Go 互通用例 `go/shared/grpcinterop` | 已落 |
| G2 | `GameServerLifecycle::AcquireAllocationPermitAsync`(另加只读 `PendingPermitCount`)与 agones_lifecycle_test 新用例 | 已落 |
| G3 | 生成器模板、`grpc_handler_gen.go`、`grpc_code_sections.go` 与测试:旧的非空无名 / `in_loop` / `post_dispatch` 段一律拒绝生成,且在写 .h 之前预检 | 已落;go test 未跑 |
| G4 | `core/network/grpc_unary_call.h`;Node 接线与 loop 内关机(删 `grpcShutdownThread_` 与 `~Node` 的 join,`GetGrpcServices` 换成 `HasGrpcServices`,`GRPC_SERVER_MAX_POLLERS` 只打一次 WARN);scene / battle main 注册;rpc_controller_test 回环用例 `grpc_client_loopback_test` | 已落 |
| G5 | scene / battle handler 手写段按 §15.8.4 手工迁进命名守护段 `grpc.<M>.guard`;`battle_admission_gate.h`、`scene_handler.cpp`、`asset_op_auth.h` 的注释 | 已手改;§15.14 第 2、3 步的定向重生成未跑 |
| G6 | 部署脚本 `GRPC_SERVER_MAX_POLLERS` 退役(`k8s_deploy.ps1` 参数默认 0、大于 0 只 Write-Warning,Deployment / Fleet / 客户端入口生成器都不再注入;`lib/k8s_client_entry.ps1` 参数保留、取值忽略;tcpSocket 探针注释补「不发 preface 静默处理、排空期间探针失败属预期」);Go `Workers` 与「poller」理由注释(`assetop` 的 reconcile / config_test / reconcile_test / caller / caller_test,guild 的 `config.go` 与 `etc/guild.yaml`,数值不动);`.github/copilot-instructions.md` 两节改写、`cpp/AGENTS.md` 许可约定、`deploy/k8s/README.md`;设计文档同步(`scene-node-threading-model`、`scene-grpc-server-design` 抬头注明被 §15 取代、`battle-transport-decision`、`agones-scene-node-high-density`、`k8s-client-entry`、`team-system`、`turn-based-battle-server`、`turn-battle-gap-closure`、`guild-phase2/04-asset-channel`、`guild-phase2/92-handoff`),以及 `docs/ops/incident-gate-tcpconnection-dtor-assert-2026-09-13.md`、`docs/handoff/friend-handoff-20260920.md` 里仍会被照做的旧断言 / 旧恢复步骤(就地加 2026-10-16 更正,原文保留) | 已落 |

各组相对 §15 草稿的偏离(如 `Observer::OnRequestRejected`、`MethodTable::size()` / `Paths()`、`HasGrpcServices`、listening /
draining 日志改由 `Server` 打(Node 只打 drained 行)、帧级协议错误改由 `DrainOutput` 报告等)由主会话汇总补记进设计 §15。

仍未做 / 待定:
- **验证**:按设计 §15.14 交 Codex(生成器单测 → 迁移前拒绝验证 → 定向重生成零 diff → Windows / Linux 构建 → C++ 单测 → Go 互通 →
  冒烟与线程核对 → 关机 → 探针)。前提是第 3 批编译通过。没有运行结果之前不得宣称「线程归零」。
- AGENTS.md §11.7 参考实现列表补 `muduo_grpc::Server` / `ServerCall`:需负责人同意(§15.4.1),实施组不改。
- `docs/PROGRESS.md` 条目与设计 §15 偏离补记:主会话负责。`docs/design/thread-count-monitoring*.md`:归第 5 批。
- 不在 G6 范围的过时 C++ 注释(2026-10-16 复核时仍在;**同日已由第 4、5 批一致性检查组逐处局部改准**,只改注释、不动代码;
  同批顺带改了 `client_endpoint.h:107`、`player_battle.h:24` 的 runInLoop 说法,`asset_op_auth.h:27` / `asset_op_auth.cpp:82` /
  `asset_op_system.h:13` 的资产入口计数(含 AckReceipt 共 4 个),以及 `agones_lifecycle_test.cpp:717` 的 NonBlocking 用途说明):
  - `cpp/nodes/battle/logic/battle_room_manager.h:34`(「runInLoop + promise 投递进来」)、`:188-189`(「gRPC drain 期间已排进 loop 的
    CreateBattle 仍会执行」,语义仍成立,措辞宜改为「gRPC 排空期间已收完的 CreateBattle 仍会派发」);`battle_room_manager.cpp:1711`
    (「Handle* 经 runInLoop 投递」);battle `main.cpp:11-13`(「runInLoop + promise 投递到 loop 线程」);
  - `cpp/nodes/scene/handler/rpc/scene_admin_handler.cpp:126-127`(守护段内「gRPC Server::Shutdown 等 handler 退出……会闭环」);
  - `cpp/libs/services/scene/player/system/asset_op_system.h:13`(「gRPC 包装已 runInLoop 投递」)与 `:20-21`(「future.get() 之后没有守护段」);
    `asset_op_auth.cpp:82`(runInLoop)与 `:122-123`、`cpp/tests/currency_test/asset_op_auth_test.cpp:235`(「InsecureServerCredentials()」,
    现为明文 h2c,语义不变);
  - `cpp/tests/agones_lifecycle_test/agones_lifecycle_test.cpp:408`(「gRPC 线程通过 gate 后」)。
  - `grpc_client_local_reject_test.cpp` 头注释已由他人改为指向 `grpc_client_loopback_test.cpp`,划掉。
- 另一会话在途的 `docs/handoff/landing-fencing-takeover-20261016/c-release-player.md:24` 仍写「在 gRPC 线程上 runInLoop … future.get()」,
  未跟踪、归该会话,G6 没动,由该会话或主会话同步。
- 只作历史保留、未加更正的旧记录:本目录 `survey-*.md`(只读调查原样保留)、`docs/handoff/global-singletons-p0-findings-20261014.md:761`
  与 `docs/handoff/ecs-refactor/review-report-20261015.md:1128`(当时代码的快照,结论不受影响)、`docs/design/team-system.md`
  里其余已执行完的旧断言与执行记录(只在仍可能被照做的 §I.6 第 2 步加了一条更正)。

## 5. 第 5 批(原计划:删官方 gRPC;10-16 改口径为运行期守卫,已落地,见 §0 与设计 §15.16。下文是原计划的待决清单,仅在将来真要去掉 grpc 依赖时适用)

grpc 子模块目前还是这些依赖的唯一来源:protobuf 35.1、abseil 20250512.1、BoringSSL(gate / scene / battle 的 HMAC)、zlib(Windows)、gtest / gmock、protoc.exe、zlibd.dll。删 grpc 之前,这些要逐个换成独立依赖。以下几项**需要负责人决定**:
- SSL 的来源;
- gtest 的来源;
- protoc.exe 自己编还是下载(下载要授权);
- Windows zlib;
- protobuf / abseil 安装目录的约定。

另两处注意:
- Linux 三个节点的 CMakeLists 由 `tools/archived/vcxproj2cmake.py` 生成,且已经和生成器漂移;改 CMake 必须同时改生成器。
- `common/constants/etcd_grpc.pb.cc` 是普通 pb,**不能删**。

## 6. 协作现状(交接时)

- 「C++ RPC 异步回调链式调用设计」会话:落地了每次调用回调(`GrpcDone` / `GrpcCallOptions`);第 3 批以它为唯一出口。它在 `grpc_client_lifecycle_test.cpp` 补过 PerCallDone* 三条,第 3 批要删这个文件,先告诉它。
- 「Route 水平扩展消息乱序问题」会话:持有 gate `client_message_processor.cpp`;守卫与 `clear<MuduoGrpcChannelPtr>` 的约定见 PROGRESS。
- 「玩家收件箱」会话:`player_inbox_*` 的文件(scene 侧应答适配、系统、选点)。
- worktree `.claude/worktrees/blissful-morse-969d44`:未提交地改着 `player_scene.*`、`player_lifecycle.*`、`scene_manager_response_handler.cpp`、`cross_zone_test.cpp`(CreateScene 改每次调用回调)。

## 7. 风险备忘

- 对端 `grpc_endpoint.ip` 必须是 IPv4 字面量,否则通道建不起来、节点进不了候选。Go 侧 POD_IP 推测满足,未核实。
- 停机 / 摘节点时在途调用一律 UNAVAILABLE(官方路径是 CANCELLED)。摘节点时完成回调在实体销毁之后才执行,回调不得捕获实体。
- 官方通道「空闲也 ping」「30 分钟回 IDLE」消失,改为截止触发探测(§4.4)。连接失败回调落地前,拒连会在 20s 内一直报 CONNECTING。
- 官方路径的 LeaseRevoke 在阶段 0 紧接着被 TryCancel 的问题,随官方 etcd 传输删除而消失;新传输下只是尽量送达(§13.7)。
- etcd 滚动(钉镜像版本、加压缩参数)仍要等第 2 批的故障注入验证通过(§10.2 第 7 条)。

## 8. 验证(第 3 批做完后,交负责人)

1. 重生成(3.1),生成器单测通过。
2. `msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64` 退出 0(MSBuild 一律串行)。
3. `pwsh tools/scripts/run_cpp_tests.ps1 -Filter muduo_grpc` 全绿;routing_identity_test 的 scene_manager_selector 用例全绿。
4. 冒烟:etcd + Go 服务 + gate(路由模式)+ scene,登录 / 进场 / 换图 / 号段领取 / 断线通知。日志里不得再有 `CompletionQueue`、`gRPC client config: ResourceQuota`。
5. 关机:Ctrl+C gate 与 scene,无 assert;login 侧会话转为离线。
6. Windows 下观察 loop 延迟(wakeup 修复后):跨线程投递应远小于 100ms。
