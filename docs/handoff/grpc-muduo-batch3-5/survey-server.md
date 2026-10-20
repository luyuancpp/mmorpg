<!-- 只读调查结果(2026-10-15,基线 HEAD 526780b4e 加当时工作区的未提交改动;第 3 批前半已落地,行号可能已漂移)。原样保留作交接附录,见同目录 README.md。 -->

# AREA server
## summary
【现状一句话】只有 scene 和 battle 两类 C++ 节点起 gRPC 服务端，都用官方 grpc C++ 的同步服务端（sync server）。gRPC poller 线程先跑「投递前段」（会阻塞调 Agones 分配许可），再 runInLoop 把业务投到 muduo EventLoop，然后在 promise/future 上阻塞等结果。关机时临时起一条线程跑 grpc::Server::Shutdown(now+2s)，跑完再 queueInLoop 回到 loop，接上 Node 的两段式收尾。所有方法都是 unary（单次请求单次应答），调用方全是 Go，不带自定义 metadata。本次只做静态阅读，没有编译或运行。基线是 HEAD 526780b4e 加工作区里未提交的改动。

【现状图】
```
Go scene_manager ─┐ grpc.NewClient(insecure)：无拦截器 / 无 keepalive / 无压缩 / 无自定义 metadata
Go match ─────────┼─h2c─► scene  grpc_endpoint = POD_IP:(TCP+30000)  scene_node.SceneNodeGrpc（8 个 unary）
Go guild/trade ───┘       （资产 3 个 RPC 经 shared/scenenode ConnCache 发出）
Go match ─────────h2c──► battle grpc_endpoint                       BattleNode（5 个 unary，无 package）
                           │ grpc::ServerBuilder：NUM_CQS=1，MIN_POLLERS=1，MAX_POLLERS=8（GRPC_SERVER_MAX_POLLERS）
                           │ poller 线程：投递前段（scene CreateScene / battle CreateBattle：准入闸 + Agones
                           │   AcquireAllocationPermitBlocking，最多阻塞约 3s）→ 非 OK 直接返回 UNAVAILABLE
                           │ loop_.runInLoop([&]{ in_loop 段（复核）；Handle*()（同步）；promise.set_value })
                           │ future.get() 阻塞 poller → post_dispatch 段 → 返回 grpc::Status
                           ▼
                     muduo EventLoop（ECS 单线程；Windows 上 wakeup 是空操作，靠 5ms CQ 定时器唤醒）
Agones sidecar ◄── REST/libcurl（lifecycle worker 线程，只在 Linux）── GameServerLifecycle（scene / battle 共用）
关机：ShutdownInLoop → StopConsumers → beforeShutdown hook → 业务 drain 看门狗（15s）
      + ShutdownGrpcServer（临时线程 server->Shutdown(now+2s) → queueInLoop：join、reset、shutdownGrpcDrainComplete_）
      → MaybeFinalizeShutdownInLoop（gRPC 排空完 且 业务排空完或超时）→ FinalizeShutdownInLoop（ReleaseNodeId、关通道）
      → DrainGrpcClientsInLoop → FinalizeTeardownInLoop（Kafka / Redis / 清池 / ECS / 日志）→ 两跳 queueInLoop 再 quit
```

【第 4 批服务端要实现的能力】（以下是从现状推出来的需求，不是已有代码）
1. 方法与路由：13 个 unary 方法按 :path 精确路由（/scene_node.SceneNodeGrpc/*、/BattleNode/*）。未知路径回 trailers-only 的 UNIMPLEMENTED(12)。不需要任何流式方法。方法表由生成器输出，只依赖 .pb.h，不依赖第 5 批要删的 *.grpc.pb.h。
2. 请求处理：
   - 只支持 h2c prior-knowledge（不走 Upgrade）。
   - 校验 POST 和 application/grpc。
   - 不声明压缩；收到 grpc-encoding 或压缩标志 1 就拒绝。
   - 接收上限 4MiB，超了回 RESOURCE_EXHAUSTED。
   - unary 必须恰好一条消息。
   - protobuf 解析失败回 INTERNAL（推测与官方行为一致）。
3. 元数据：解析时对 -bin 键解掉一层 base64，头部总长上限 16KiB。现有 handler 一个 metadata 都不读，可以先只做到「收下不报错」，不回写自定义 metadata。
4. 截止时间：
   - 用 ParseGrpcTimeout 解析 grpc-timeout；没有这个头就视为无截止（scene_manager 的后台循环就不带）。
   - 在投递业务之前已经到期：不进业务，直接回 DEADLINE_EXCEEDED。
   - 业务已经开始：不中途打断，保持「结果未知」的语义；计时用 steady_clock。
5. 取消：对端 RST 或断连时把调用标为已取消。还没投递的不执行；已经执行的照常完成，应答丢弃并计数；每个调用恰好完成一次。
6. 应答与状态码：
   - 成功：200 + content-type + 一条消息 + trailers（grpc-status，grpc-message 用百分号编码）；非 OK 用 trailers-only。
   - 两条现有的 UNAVAILABLE 文案逐字保留：scene 的 "agones allocate not confirmed"，battle 的 "battle_not_allocatable"（后者是跨语言契约）。
   - 其余业务错误照旧：status OK，错误写在应答字段里。
7. 流控：窗口交给 nghttp2；输出缓冲设高水位，超过就暂停从 nghttp2 取帧；SETTINGS 与客户端侧对称；防 PING / SETTINGS / rapid-reset 洪泛（推测要配 nghttp2 选项）。
8. 并发：
   - 派发和完成都在节点 EventLoop 上，不创建线程。
   - 用显式的在途上限替代今天「8 个 poller」这层隐式限流，要和 Go 侧 reconcile.go 的 Workers 设计依据对齐。
   - 每次读事件的处理量要有上限，不能挤压逻辑帧。
9. 异步完成句柄：仿照 RpcDoneCallback::Adopt 做一个可拷贝的完成句柄。
   - 默认「handler 返回即完成」。
   - 句柄只持有 weak（AGENTS §11.7）。
   - 最后一个副本析构时还没完成：回失败并计数。
   - 连接已断：丢弃应答并计数。
10. Agones 许可：新增回调式的异步许可，结果经 queueInLoop 回到 loop，带等待上限。
    - Stop() 要把所有等待者以「拒绝」回调掉。
    - 保留 allocateWaiters_ 的晚到收口语义。
    - 许可要活到这次调用完成。
    - battle 在许可回到 loop 之后要再复核一次准入闸。
11. 关机：在 loop 内完成以下步骤，删掉关机线程和 ~Node 里的 join。
    - 停止接新连接：muduo 没有停止监听的接口，要么改 Acceptor，要么接入后立刻 GOAWAY 再关。
    - 两段 GOAWAY；之后来的新流回 REFUSED_STREAM。
    - 在 kGrpcDrainTimeout 内等在途调用完成，包括还在等许可的调用；到期的以失败完成。
    - 关掉连接，置 shutdownGrpcDrainComplete_，再走 MaybeFinalize。
    - TcpServer 在两跳 quit 之前销毁。
12. 运维：
    - K8s tcpSocket 就绪探针会来连，不发 preface 就断开，要静默处理。
    - 低基数指标：按方法、状态码、在途数。
    - 回退开关，启动时读一次；同一端口同一时刻只能有一种实现在监听。
    - GRPC_SERVER_MAX_POLLERS 退役。

【关键文件】
- cpp/libs/engine/core/node/system/node/node.{h,cpp}
- tools/proto_generator/protogen/internal/template/grpc_service_handler.{h,cpp}.tmpl
- tools/proto_generator/protogen/internal/grpc_handler_gen.go
- cpp/nodes/scene/handler/grpc/scene_node_service.{h,cpp}
- cpp/nodes/battle/handler/grpc/battle_node.{h,cpp}
- cpp/libs/engine/infra/agones/agones_gameserver_lifecycle.{h,cpp}
- cpp/libs/engine/muduo_grpc/*
- docs/design/grpc-muduo-transport.md §10
## facts
### 起 gRPC server 的节点
只有 scene 和 battle 注册了 gRPC 服务。
- scene：SceneRuntimeContext 持有 SceneNodeGrpcImpl（main.cpp:45,56），main.cpp:120 调 node.RegisterGrpcService(&context->grpcService)；context 先于 Node 构造（:94 对比 :102），所以析构晚于 Node。
- battle：main.cpp:268-269 注册 BattleRuntimeContext::battleNodeService；RunSimpleNodeMainWithOwnedContext 里 context 同样先于 Node 构造（node_entry.h:184-187）。
- gate 不注册：grpcServices_ 为空时 StartGrpcServer 只做发布（node.cpp:748-755）；k8s_deploy.ps1 的注释也写明 gate 的 RpcPort+30000 上没有监听。
- 非生成代码里 grpc::ServerBuilder 只出现在 node.cpp，以及测试 grpc_client_lifecycle_test.cpp:317/380/480（那里是给官方客户端用例当假服务端）。
files: cpp/nodes/scene/main.cpp, cpp/nodes/battle/main.cpp, cpp/libs/engine/core/node/system/node/node_entry.h, cpp/libs/engine/core/node/system/node/node.cpp, cpp/tests/rpc_controller_test/grpc_client_lifecycle_test.cpp, tools/scripts/k8s_deploy.ps1
### 注册、端口与启动时序
- RegisterGrpcService 只把指针 push 进 grpcServices_（node.cpp:736-744），必须在 StartRpcServer 之前调用。
- 只有 GetGrpcServices() 非空，node_allocator 才派生 gRPC 端口，规则是 TCP 端口 + 30000（kGrpcPortOffset，node_allocator.cpp:182,306-323）。派生端口先用 IsLocalPortAvailable 试 bind（INADDR_ANY + SO_REUSEADDR，:112-137），不可用则本轮 fail-closed、不发布。
- 端口区间：非 gate 的 TCP 是 20000-35535，对应 gRPC 50000-65535（:174-182,265-275）。
- 启动链：etcd 分配键 CAS 成功 → StartRpcServer（etcd_service.cpp:479-490）→ StartGrpcServer（node.cpp:1010）→ BuildAndStart 之后才 PublishDiscoveryAfterGrpcReady（node.cpp:819；etcd_service.h:58-64），避免对端在端口打开前拨号。
- 重注册模式下 gRPC 不重启，发现键立即发布（etcd_service.cpp:474-477,511-515）。
files: cpp/libs/engine/core/node/system/node/node.cpp, cpp/libs/engine/core/node/system/node/node_allocator.cpp, cpp/libs/engine/core/node/system/etcd/etcd_service.cpp, cpp/libs/engine/core/node/system/etcd/etcd_service.h
### 监听地址与 server 配置
- 监听地址：AddListeningPort(grpc_endpoint.ip():port, InsecureServerCredentials)（node.cpp:767-770）。ip 取自 endpoint.ip（node_allocator.cpp:319），即 ResolveNodeIp：POD_IP > NODE_IP > localip()（node.cpp:288-310）。
- 对比：RPC0 的 TcpServer bind 的是 0.0.0.0 或 BIND_IP（node.cpp:965-974），两者 bind 策略不同。
- sync server 配置：NUM_CQS=1、MIN_POLLERS=1、MAX_POLLERS 默认 8，可被 GRPC_SERVER_MAX_POLLERS 覆盖（node.cpp:772-792）。部署脚本会注入这个变量（k8s_deploy.ps1:3330,3927；lib/k8s_client_entry.ps1:619,693；deploy/k8s/README.md:327）。
- 没有设置：消息大小上限、keepalive、MAX_CONCURRENT_STREAMS、ResourceQuota；也没有健康检查服务或反射服务（全仓 grep 无 EnableDefaultHealthCheckService / reflection）。
- BuildAndStart 失败直接 LOG_FATAL（:799-804）；刻意不起线程调 Wait()（:806-811）。
files: cpp/libs/engine/core/node/system/node/node.cpp, cpp/libs/engine/core/node/system/node/node_allocator.cpp, tools/scripts/k8s_deploy.ps1, tools/scripts/lib/k8s_client_entry.ps1, deploy/k8s/README.md
### 注册的 service 与方法（全部 unary）
- scene：package scene_node，服务 SceneNodeGrpc（scene_node_service.proto:2,13-37）。方法有 CreateScene、DestroyScene、ReleasePlayer、PrepareBattle、CancelBattlePrepare、AssetDebit、AssetAbortDebit、AssetCredit；线上路径是 /scene_node.SceneNodeGrpc/<M>（go/proto/scene_manager/scene_node_service_grpc.pb.go:25-32）。
- battle：没有 package，服务 BattleNode（battle_node.proto:1-12,84-90）。方法有 CreateBattle、DestroyBattle、AddObserver、RemoveObserver、IssueBattleTicket；路径是 /BattleNode/<M>（go/proto/battle/battle_node_grpc.pb.go:23-27）。
- 两份 Go 服务描述里的 Streams 都为空（scene_node_service_grpc.pb.go:422、battle_node_grpc.pb.go:272），没有 server streaming，也没有 bidi。
- C++ 侧没有任何非生成代码调用 SendSceneNodeGrpc* / SendBattleNode*，所以调用方全部是 Go。
files: proto/scene_manager/scene_node_service.proto, proto/battle/battle_node.proto, go/proto/scene_manager/scene_node_service_grpc.pb.go, go/proto/battle/battle_node_grpc.pb.go
### 生成器路由
- handlers.go:41-62 有两条 IsGrpcHandler=true 的配置。
- 筛选谓词：IsSceneNodeGrpcHandler 要求文件名含 scene_node、非 cc_generic_services、HasGrpcService；IsBattleNodeGrpcHandler 要求 battle 域、文件 battle_node、服务名 BattleNode（generator_scene_node_util.go:57-88）。
- 输出目录：scene_node_grpc 指向 {{scene_node_dir}}handler/grpc/，battle_node_grpc 指向 nodes/battle/handler/grpc/（proto_gen.yaml:191-192，no-unity 配置同值）。
- 单文件入口在 grpc_handler_target.go:73-125：先完整渲染两份，再写文件。
files: tools/proto_generator/protogen/internal/generator/cpp/handlers.go, tools/proto_generator/protogen/internal/generator/cpp/generator_scene_node_util.go, tools/proto_generator/protogen/internal/generator/cpp/grpc_handler_target.go, tools/proto_generator/protogen/etc/proto_gen.yaml
### 模板的同步阻塞模型
- 头文件模板：类声明为 class XImpl final : public <pkg>X::Service，继承 grpc_cpp_plugin 生成的 Service 基类（h.tmpl:14）。方法签名是 grpc::Status M(grpc::ServerContext*, const Req*, Resp*) override（h.tmpl:20-22）。业务委托是 static void HandleM(const Req*[, Resp*])，没有 done 或回调参数，只能同步完成（h.tmpl:25-26）。
- cpp 模板（cpp.tmpl:19-41）的执行顺序：
  1. context 形参被注释掉；
  2. 投递前段在 gRPC 线程上运行；
  3. 构造 std::promise<grpc::Status>；
  4. loop_.runInLoop([&]{ 设 dispatchStatus=OK；跑 in_loop 段；ok 时调 HandleM；promise.set_value })；
  5. future.get() 阻塞 poller 线程；
  6. 跑 post_dispatch 段，然后 return status。
- [&] 捕获的是栈变量，生命期靠 future.get() 兜住。
- include 由 grpcHandlerIncludeName 额外追加 <future>（grpc_handler_gen.go:196-200）。
files: tools/proto_generator/protogen/internal/template/grpc_service_handler.h.tmpl, tools/proto_generator/protogen/internal/template/grpc_service_handler.cpp.tmpl, tools/proto_generator/protogen/internal/grpc_handler_gen.go
### 守护段解析机制（迁移的关键依赖）
- 命名段：头文件只有 grpc.header.includes / public / private 三个（grpc_handler_gen.go:74-78）；cpp 每个方法有 grpc.<M>.in_loop 和 grpc.<M>.post_dispatch（:118-123）。
- 出现未识别的命名段：头文件返回 error，cpp 直接 panic，拒绝生成（:124-127；grpc_code_sections.go:77-93）。
- Handle* 体按 key 'XImpl::HandleM(' 读取（:171-173）。
- 投递前段按 key 'XImpl::M(grpc::ServerContext' 读取（:177-179），依赖旧签名的字面量；找不到时补一个默认空段（:141-144）。
- 历史教训：投递前段曾因写在守护段外被重生成吞掉（commit 6c4021ae5；scene_node_service.cpp:272 注释；grpc_handler_gen.go:28-30）。
- 相关测试：grpc_handler_gen_test.go:17-39、grpc_handler_preservation_test.go:15/57/157/203/225、generator/cpp/grpc_handler_routing_test.go。
files: tools/proto_generator/protogen/internal/grpc_handler_gen.go, tools/proto_generator/protogen/internal/grpc_code_sections.go, tools/proto_generator/protogen/internal/grpc_handler_gen_test.go, tools/proto_generator/protogen/internal/grpc_handler_preservation_test.go, tools/proto_generator/protogen/internal/generator/cpp/grpc_handler_routing_test.go
### scene 守护段里的手写代码
- 头文件 scene_node_service.h：三个命名段都是空的。
- Handle* 体：
  - CreateScene：按 scene_id 去重；建实体并 trigger OnSceneCreated；监听者删掉组件时补发 OnSceneDestroyed 回滚并 destroy，不回 scene_info（cpp:22-87）。
  - DestroyScene：幂等；场景里还有居民时 BeginSceneDrain 改派、保留实体（:92-146）。
  - ReleasePlayer：玩家不在本节点就幂等 OK；IsHandoffRequested 时跳过；否则 HandleExitGameNode(kReleasedByTransfer)（:151-205）。
  - PrepareBattle / CancelBattlePrepare：委托 PlayerBattleSystem（:211-223）。
  - 资产三件：委托 PlayerAssetOpSystem（:229-254）。
- 投递前段只有 CreateScene 有内容：AcquireAllocationPermitBlocking，失败回 UNAVAILABLE "agones allocate not confirmed"，createPermit 活到函数返回（:261-281）。
- 所有 in_loop / post_dispatch 段都是空的。
- 与 muduo 路径的差异：muduo 的 SceneHandler::CreateScene 拒绝 config_id 或 scene_id 为 0（scene_handler.cpp:688-693），gRPC 版不做这项校验；Go 发送前只拦 sceneId==0（scene_node_client.go:102-104）。
files: cpp/nodes/scene/handler/grpc/scene_node_service.h, cpp/nodes/scene/handler/grpc/scene_node_service.cpp, cpp/nodes/scene/handler/rpc/scene_handler.cpp, go/scene_manager/internal/logic/scene_node_client.go
### battle 守护段里的手写代码
- 头文件：includes 段引入 battle_admission_gate.h；public 段有 OpenAdmission / CloseAdmission；private 段有 AdmissionGate admission_（battle_node.h:7-9,21-31,62-64）。
- cpp 首段：定义 kBattleNotAllocatableMessage="battle_not_allocatable" 和 NotAllocatable()，后者返回 UNAVAILABLE（battle_node.cpp:3-37）。
- Handle* 全部委托 BattleRoomManager（:44-85）。
- CreateBattle 投递前段（:91-132）：
  1. 准入闸 Phase()!=kOpen → 拒绝；
  2. AcquireAllocationPermitBlocking 失败 → 同一个 UNAVAILABLE；
  3. 把 admission 引用绑出来给 loop 内用。
- in_loop 段复核准入闸（:140-149）；post_dispatch 段只打日志（:157-163）。
- 准入闸是无锁 atomic，三态 kNotStarted → kOpen → kClosed；Open / Close 只在 loop 上调（battle_admission_gate.h:18-21,55-74）。SetAfterStart 最后才开闸（main.cpp:324-329），SetBeforeShutdown 第一步关闸（main.cpp:345-348）。
files: cpp/nodes/battle/handler/grpc/battle_node.h, cpp/nodes/battle/handler/grpc/battle_node.cpp, cpp/nodes/battle/battle_admission_gate.h, cpp/nodes/battle/main.cpp
### 业务处理全部同步
现有 13 个 Handle* 都在一次 loop 任务里同步返回，不等 Redis / Kafka 的应答。比如资产操作在 loop 内调 response.set_durable(PersistAndProbeDurable(...))（asset_op_system.cpp:261,303,451,596,831-841）。proto 注释写明资产 RPC 的 status 恒为 OK、结局放在 response.outcome（scene_node_service.proto:31-36）。目前没有任何 handler 需要延后完成；需要延后完成的场景属于第 4 批新增的能力。
files: cpp/libs/services/scene/player/system/asset_op_system.cpp, proto/scene_manager/scene_node_service.proto
### 线程与唤醒
- poller 线程走 runInLoop → queueInLoop → wakeup 把任务交给 loop。
- Windows 侧（两份含 Windows 实现的 muduo 树）：EventLoop::wakeup 只在 __linux__ 下写 eventfd，Windows 上是空操作（cpp/libs/engine/muduo_windows/src/muduo/net/EventLoop.cc:258-268；third_party/muduo/muduo/net/EventLoop.cc:249-258）。Windows 的 kPollTimeMs 是 100，Linux 是 10000（EventLoop.cc:32,36）。
- 所以 Windows 上跨线程投递要等 EtcdService 的 5ms CQ 轮询定时器把 loop 唤醒（etcd_service.cpp:35-39）。第 3 批计划删掉这个定时器。
files: cpp/libs/engine/muduo_windows/src/muduo/net/EventLoop.cc, third_party/muduo/muduo/net/EventLoop.cc, third_party/muduo-linux/muduo/net/EventLoop.cc, cpp/libs/engine/core/node/system/etcd/etcd_service.cpp
### metadata（两侧都不使用）
- C++ 服务端从不读 ServerContext（模板把 context 注释掉），也不回写初始或尾随 metadata。资产 RPC 的认证因此放在请求体 AssetOpRequest.auth，头文件注释写明了原因（asset_op_auth.h:9-13）。
- Go 调用方都是裸 grpc.NewClient(insecure)，没有拦截器、keepalive 或压缩：scene_manager（scene_node_client.go:39-41）、match（internal/discovery/node_watcher.go:359）、guild/trade 经 shared/scenenode（conn.go:48）。
- 调用点都不附加 outgoing metadata。相关目录里出现 metadata 包的只有 match_service.go:361 和 guild/trade 的 session.go，读的都是它们自己的入站 metadata。
files: cpp/libs/services/scene/player/system/asset_op_auth.h, go/scene_manager/internal/logic/scene_node_client.go, go/match/internal/discovery/node_watcher.go, go/shared/scenenode/conn.go
### Go 调用方与截止时间
scene_manager → scene：
- CreateScene 在 createscenelogic.go:297、enterscenelogic.go:1271 用 l.ctx，即 zrpc 请求 ctx（服务端 Timeout 8000，scene_manager_service.yaml:14）；world_init.go:301/322 用 5s（:51）。
- DestroyScene 在 world_autoscale.go:416 用 safego.Loop 传下来的 ctx，源头是 context.WithCancel(context.Background())（scene_manager_service.go:76,172；safego.go:122-141），没有截止。推测：grpc-go 在 ctx 无截止时不发 grpc-timeout。
- ReleasePlayer：每次 1s，失败按 1s / 3s 退避再试 2 次（scene_node_client.go:333-375）。
- instance_lifecycle.go / orphan_cleanup.go / world_rebalance.go 等其余调用点的 ctx 没有逐一核对。

match → scene / battle：
- PrepareBattle 3s、CancelBattlePrepare 3s（gather.go:27-29,447-450,555-557）。
- CreateBattle 5s、DestroyBattle 3s（:594-597,619-622）。
- AddObserver 取请求 ctx 与 3s 中较早者（spectate.go:37,333-339）；RemoveObserver 3s（:38,348-350）。
- IssueBattleTicket 3s（requestbattleticketlogic.go:22,35-37）。

guild / trade → scene 资产：DefaultCallTimeout 800ms（go/shared/assetop/caller.go:25,190-208）。
files: go/scene_manager/internal/logic/createscenelogic.go, go/scene_manager/internal/logic/enterscenelogic.go, go/scene_manager/internal/logic/world_init.go, go/scene_manager/internal/logic/world_autoscale.go, go/scene_manager/scene_manager_service.go, go/scene_manager/etc/scene_manager_service.yaml, go/shared/safego/safego.go, go/match/internal/logic/gather.go, go/match/internal/logic/spectate.go, go/match/internal/logic/requestbattleticketlogic.go, go/shared/assetop/caller.go
### 截止与取消在 C++ 侧不生效；Go 按「结果未知」处理
- C++ handler 不看 deadline 也不看 IsCancelled。客户端超时后，已经投进 loop 的任务照样执行完。
- Go 侧已经按「结果未知」处理：
  - spectate.go:356-363 把 DeadlineExceeded / Canceled / Unavailable 统一当作「可能已落地」，注释原话是「C++ battle 同步处理、不一定看 grpc-timeout」。
  - gather.go:567-591 只有 Unavailable 且消息精确等于 battle_not_allocatable 时才认定没有副作用；其余失败一律发 DestroyBattle 兜底。
files: go/match/internal/logic/spectate.go, go/match/internal/logic/gather.go, tools/proto_generator/protogen/internal/template/grpc_service_handler.cpp.tmpl
### 状态码契约
handler 自己返回的非 OK 只有两处，都是 UNAVAILABLE：
- scene CreateScene："agones allocate not confirmed"（scene_node_service.cpp:279）。
- battle CreateBattle："battle_not_allocatable"（battle_node.cpp:15-20，用在 :105、:122、:147）。这条是跨语言字符串契约，Go 侧对应 go/match/internal/logic/gather.go:65 和 :584-585，两边必须同改。

其余业务错误一律 status OK，错误写在应答的 tip / outcome / error_message 字段里。UNIMPLEMENTED、RESOURCE_EXHAUSTED 等目前由官方运行时隐式给出。
files: cpp/nodes/scene/handler/grpc/scene_node_service.cpp, cpp/nodes/battle/handler/grpc/battle_node.cpp, go/match/internal/logic/gather.go
### Go 侧依赖「poller 限流」的设计假设
- go/shared/assetop/reconcile.go:256-268 按「每条在途资产 RPC 占住 scene 一条 sync poller，上限默认 8」来定 Workers 上限，并引用 GRPC_SERVER_MAX_POLLERS。
- poller 默认值 2 → 8 的来历：node.cpp:772-779 注释说 Round 14 压测时 ReleasePlayer 撞到 500ms 悬崖；scene_node_client.go:315-321 记录的是同一次压测。
- 推测：今天 8 个 poller 并发 runInLoop，同一连接上的请求进入 loop 的顺序没有保证。
files: go/shared/assetop/reconcile.go, cpp/libs/engine/core/node/system/node/node.cpp, go/scene_manager/internal/logic/scene_node_client.go
### Agones 生命周期（不是 gRPC）
- 传输是 REST over libcurl：读 AGONES_SDK_HTTP_PORT，接口有 /ready、/health、/allocate、/shutdown、GET /gameserver（agones_rest_client.cpp:48,206-231）。只有 Linux 且定义 MMORPG_AGONES_CURL 时才有 curl 传输层（agones_rest_client.h:63,88-90）；Windows 退化成 Disabled。
- GameServerLifecycle 是进程级单例，HTTP 在专属 worker 线程上跑（lifecycle.cpp:110）。
- 启动位置：scene 在 SetAfterStart 里 Start / StartDisabled（main.cpp:329-347）；battle 在 StartAgonesLifecycle（main.cpp:67-89,317），并额外开 1s 一次的 EventLoop 心跳（:321-322）和排空标签（:84-88）。
- 许可 API 只有两种：
  - Blocking：调用线程有界等待 allocateWaitTimeout，默认 3s（lifecycle.h:89-95；cpp:139-214）。
  - NonBlocking：踢一次 allocate 后立即返回 false（cpp:221-261）。muduo RPC 的 SceneHandler::CreateScene 用的就是它，失败即拒绝（scene_handler.cpp:695-710）。
- 没有回调式的异步 API。
- 头文件规定 EventLoop 上不得调 Blocking（lifecycle.h:39-45）。
- AllocationPermit 删了拷贝构造和移动赋值，只保留移动构造（:118-136）。
- Stop() 会 join worker；Disabled 状态下 worker 不存在，Stop 直接返回、状态不变（cpp:113-132）。
files: cpp/libs/engine/infra/agones/agones_gameserver_lifecycle.h, cpp/libs/engine/infra/agones/agones_gameserver_lifecycle.cpp, cpp/libs/engine/infra/agones/agones_rest_client.h, cpp/libs/engine/infra/agones/agones_rest_client.cpp, cpp/nodes/scene/main.cpp, cpp/nodes/battle/main.cpp, cpp/nodes/scene/handler/rpc/scene_handler.cpp
### 关机：gRPC 与两段式收尾的交互
1. ShutdownInLoop（node.cpp:1119-1145）：StopConsumers → beforeShutdownFn_ → StartShutdownDrainWatchdog（业务预算 15s，100ms 轮询，:1147-1165）→ ShutdownGrpcServer → MaybeFinalizeShutdownInLoop。
2. ShutdownGrpcServer（:822-877）：在 loop 上断言后起 grpcShutdownThread_，线程里跑 server->Shutdown(system_clock::now()+kGrpcDrainTimeout(2s,:54))。返回后 queueInLoop 回 loop：耗时达到预算就记 WARN，然后 join、grpcServer_.reset()、置 shutdownGrpcDrainComplete_、调 MaybeFinalize。不能在 loop 上同步 Shutdown 的理由写在 :834-843：handler 正卡在 future.get()，会与 loop 互等死锁；而 grace 到期的 cancel 也弹不出卡住的 handler。
3. MaybeFinalizeShutdownInLoop（:1167-1195）：要求 gRPC 已排空，且业务已排空或超时；业务完成状态不锁存，每次现场重查。
4. FinalizeShutdownInLoop（:1197-1230）：ReleaseNodeId（撤 etcd 租约）、serviceDiscoveryManager.Shutdown、取消定时器、CloseAllRpcChannels、BeginShutdownGrpcNode（CQ）、清 MuduoGrpcChannelPtr，然后进 DrainGrpcClientsInLoop（:1266-1280）。
5. FinalizeTeardownInLoop（:1282-1352）：Kafka / Redis Shutdown → 再轮询一次客户端 → tlsNodeContextManager.Clear → tlsEcs.Clear → logSystem.stop → 两跳 queueInLoop 后 quit，并在锁内置 shutdownComplete_。
6. ~Node（:438-486）：loop 线程上若尚未完成就重跑 loop 兜底；之后等完成条件，再 join grpcShutdownThread_。
7. 相关注释：RequestShutdown 的注释说明 RPC handler 不能同步等 drain，否则与 Server::Shutdown 互等（node.h:186-188）；Windows 控制台 handler 调 gNode->Shutdown()（node_entry.h:138-153）。
files: cpp/libs/engine/core/node/system/node/node.cpp, cpp/libs/engine/core/node/system/node/node.h, cpp/libs/engine/core/node/system/node/node_entry.h, docs/design/scene-node-threading-model.md
### 关机窗口里仍会有入站与发现
- 顺序：beforeShutdown hook 先于 ShutdownGrpcServer（node.cpp:1134-1143）。所以 hook 跑完之后，已经排进 loop 或此刻新到的 RPC 仍会执行。
- scene 没有准入闸。Disabled 形态下 Stop() 不改状态，许可恒放行。scene 的 drain 谓词每次重新扫描在线玩家，用来兜这个窗口（scene main.cpp:202-291）。
- battle 靠 CloseAdmission 加 loop 内复核兜底（battle main.cpp:332-353；battle_node.cpp:140-149）。
- etcd 发现键要到 FinalizeShutdownInLoop 里的 ReleaseNodeId 才撤（node.cpp:1202）。也就是说，在 gRPC drain 和业务 drain 期间，Go 端仍能发现并拨号本节点。
files: cpp/libs/engine/core/node/system/node/node.cpp, cpp/nodes/scene/main.cpp, cpp/nodes/battle/main.cpp, cpp/nodes/battle/handler/grpc/battle_node.cpp
### K8s 探针连 gRPC 端口
scene（Deployment 模式）与 battle 的 readinessProbe 都用 tcpSocket 探 gRPC 端口（RpcPort+30000），周期 5s；battle 的 startupProbe 探的是 TCP 端口（k8s_deploy.ps1:3360-3364,3386-3430）。注释写明 C++ 没有注册 grpc.health.v1。Agones Fleet 不加 K8s 探针。新服务端会被周期性地「连上又断开、不发 HTTP/2 preface」。
files: tools/scripts/k8s_deploy.ps1
### muduo TcpServer 没有「停止监听」的能力
- Acceptor 只有构造、setNewConnectionCallback、listen()、listening()（cpp/libs/engine/muduo_windows/src/muduo/net/Acceptor.h:33-56；third_party/muduo-linux/muduo/net/Acceptor.h 同样）。
- TcpServer 没有 stop 方法。~TcpServer 断言在 loop 线程，并对每条连接 runInLoop(connectDestroyed)（third_party/muduo/muduo/net/TcpServer.cc:39-51）。
- 仓库内有三份 muduo 源码树：cpp/libs/engine/muduo_windows/src（muduo.vcxproj 编译）、third_party/muduo-linux（Linux CMake）、third_party/muduo。
files: cpp/libs/engine/muduo_windows/src/muduo/net/Acceptor.h, third_party/muduo-linux/muduo/net/Acceptor.h, third_party/muduo/muduo/net/TcpServer.cc, cpp/libs/engine/muduo_windows/muduo.vcxproj
### muduo_grpc 现有可复用件
- 目前只有客户端：channel、client_session、http2_transport（CMakeLists.txt:33-41；muduo_grpc.vcxproj:22-37 里没有服务端源文件）。
- 服务端能直接用的工具：
  - status.h：状态码、RST / HTTP 状态映射、ParseGrpcStatusValue；
  - metadata.h：-bin 宽松解码、grpc-message 百分号编码；
  - grpc_timeout.h：ParseGrpcTimeout，注释写明「服务端方向(第 4 批)」；
  - message_framing.h：MessageDeframer，可配上限。
- 测试替身 test_h2_peer 本身就是一个内存里的 nghttp2 服务端会话（cpp/tests/muduo_grpc_test/test_h2_peer.h:1-20），可作参考。
- 构建：scene / battle 已链 muduo_grpc 和 nghttp2（scene/CMakeLists.txt:150-162；battle/CMakeLists.txt:91-103；scene.vcxproj:119；battle.vcxproj:140），同时仍链 gRPC::grpc++ 等官方库。
files: cpp/libs/engine/muduo_grpc/CMakeLists.txt, cpp/libs/engine/muduo_grpc/muduo_grpc.vcxproj, cpp/libs/engine/muduo_grpc/status.h, cpp/libs/engine/muduo_grpc/metadata.h, cpp/libs/engine/muduo_grpc/grpc_timeout.h, cpp/libs/engine/muduo_grpc/message_framing.h, cpp/tests/muduo_grpc_test/test_h2_peer.h, cpp/nodes/scene/CMakeLists.txt, cpp/nodes/battle/CMakeLists.txt
### 可对照的异步完成先例（RPC0）
节点间 RPC0 服务端已经有「默认同步，显式接管 done」的模型。RpcDoneCallback::Adopt 返回可拷贝句柄，提供 Run / Fail / response<T>；内部只持 weak_ptr<RpcChannel>；最后一个副本析构仍未完成时回 REPLY_DROPPED（docs/design/muduo-rpc-channel.md §5.1-5.3）。gRPC 服务端的异步完成句柄可以照这个形状做。
files: docs/design/muduo-rpc-channel.md
### 工作区与批次前置
- 以下文件有未提交修改（git status / git diff）：node.cpp、node.h、node_util.{h,cpp}、scene main.cpp、grpc-muduo-transport.md。例如 node.cpp 正在把 eventLoop 从指针改成引用（RequireEventLoop），scene main.cpp 加了收件箱 tick。
- docs/PROGRESS.md 只有第 1、2 批条目（:8397、:8665），没有第 3 批。设计 §10 规定第 4 批以第 3 批为前置。
- 设计 §12 结论：本迁移只改 C++ 内部传输，对 Java 版不适用。
files: cpp/libs/engine/core/node/system/node/node.cpp, cpp/libs/engine/core/node/system/node/node.h, cpp/nodes/scene/main.cpp, docs/PROGRESS.md, docs/design/grpc-muduo-transport.md
## changeSurface
- [template] tools/proto_generator/protogen/internal/template/grpc_service_handler.h.tmpl: 去掉对 <pkg>X::Service 和 grpc::ServerContext / grpc::Status 的依赖。改为 loop 内的异步调用签名（带完成句柄），并生成方法表：路径、请求和应答类型。
- [template] tools/proto_generator/protogen/internal/template/grpc_service_handler.cpp.tmpl: 删掉 promise / future / runInLoop 阻塞那套。投递前段改为在 loop 内运行，可以异步等待许可；默认同步完成，接管后可以延后完成；不再 include <future>。
- [generator-source] tools/proto_generator/protogen/internal/grpc_handler_gen.go: 投递前段的 key 'XImpl::M(grpc::ServerContext' 和 grpcHandlerIncludeName（<future>）都依赖旧签名。要写旧段到新段的迁移，否则手写的 Agones 许可和准入闸代码会丢；方法数据结构也要加路径等字段。
- [generator-source] tools/proto_generator/protogen/internal/grpc_code_sections.go: 未识别的命名段会被拒绝生成（panic）。新增或改名段（比如异步许可之后的复核段）时必须同步更新允许列表和旧名兼容。
- [generator-source] tools/proto_generator/protogen/internal/generator/cpp/handlers.go: 如果要额外输出服务注册或方法表文件，要在这里加 HandlerConfig；同时要保证 IsGrpcHandler 的两条路由不变。
- [generator-source] tools/proto_generator/protogen/internal/generator/cpp/grpc_handler_target.go: 单文件生成入口要跟着新增的输出走。
- [generator-source] tools/proto_generator/protogen/internal/grpc_handler_gen_test.go: 现有用例写死了 'grpc::ServerContext* /*context*/' 签名，要换成新签名，并加旧格式迁移用例。
- [generator-source] tools/proto_generator/protogen/internal/grpc_handler_preservation_test.go: battle / scene 守护段保留和旧格式兼容的用例（:157 写死了旧签名）要更新。
- [generator-source] tools/proto_generator/protogen/internal/generator/cpp/grpc_handler_routing_test.go: 如果输出文件有变化，路由用例要同步。
- [handwritten] cpp/libs/engine/muduo_grpc/server_session.{h,cpp}（新）: 纯协议层的 nghttp2 服务端会话：不碰 I/O，负责请求头校验、拆包、trailers / trailers-only、RST、GOAWAY（两段）、SETTINGS 与上限；参考 test_h2_peer。
- [handwritten] cpp/libs/engine/muduo_grpc/server.{h,cpp}（新）: muduo TcpServer、连接表、方法表派发、在途上限、截止与取消、loop 内 GOAWAY 排空；连接只存 weak，回调只绑 weak（AGENTS §11.7）。
- [handwritten] cpp/libs/engine/muduo_grpc/server_call.{h,cpp}（新）: 可拷贝的完成句柄：Finish / response / IsCancelled，只持 weak，保证恰好完成一次；最后一个副本析构仍未完成时回失败。
- [build] cpp/libs/engine/muduo_grpc/CMakeLists.txt、muduo_grpc.vcxproj、muduo_grpc.vcxproj.filters: 加入服务端源文件。
- [handwritten] cpp/tests/muduo_grpc_test/*（server_session_test、server_test、go_interop_test 扩展、vcxproj / filters）: 服务端单测（内存里对接 ClientSession）、muduo 回环测试、真 Go 客户端互通测试；测试仍然不链 grpc。
- [handwritten] cpp/libs/engine/core/node/system/node/node.h: RegisterGrpcService(grpc::Service*)、grpcServer_、grpcShutdownThread_、#include <grpcpp/grpcpp.h> 要换成新的服务注册接口和 muduo 服务端成员。这个文件有并行会话的未提交改动。
- [handwritten] cpp/libs/engine/core/node/system/node/node.cpp: StartGrpcServer 改用 muduo（监听地址策略、去掉 poller 配置）；ShutdownGrpcServer 改为 loop 内 GOAWAY 排空；删 kGrpcDrainTimeout 的线程用法和 ~Node 里的 join；FinalizeTeardownInLoop 在两跳 quit 之前销毁服务端。有并行会话的未提交改动。
- [handwritten] cpp/libs/engine/core/node/system/node/node_allocator.cpp: GetGrpcServices() 的类型跟着变；端口派生规则不变。
- [handwritten] cpp/libs/engine/infra/agones/agones_gameserver_lifecycle.{h,cpp}: 新增回调式异步许可：结果经 queueInLoop 回 loop，带超时，Stop 时以拒绝回调全部等待者，保留 allocateWaiters_ 收口语义，许可对象要能在回调里传递；同时更新「gRPC 线程用 Blocking」的契约注释。
- [handwritten] cpp/tests/agones_lifecycle_test/agones_lifecycle_test.cpp: 补异步许可的用例：Disabled 立即放行、Allocated、超时、排空、Stop 拒绝等待者、晚到分配收口。
- [generated-output] cpp/nodes/scene/handler/grpc/scene_node_service.{h,cpp}: 生成骨架，重生成后签名会变。守护段里的手写代码要人工迁移：CreateScene 投递前段的阻塞许可改为异步许可；Handle* 体保持不变。
- [generated-output] cpp/nodes/battle/handler/grpc/battle_node.{h,cpp}: 生成骨架。手写的准入闸、许可、loop 内复核要改为「异步许可回到 loop 后复核准入闸」；battle_not_allocatable 的文案和状态码必须保持。
- [handwritten] cpp/nodes/scene/main.cpp: 服务注册调用与注释（gRPC drain 的说法）要跟着改；有并行会话的未提交改动。
- [handwritten] cpp/nodes/battle/main.cpp: 服务注册以及停机顺序注释（「框架随后关 gRPC」）要更新。
- [handwritten] cpp/nodes/battle/battle_admission_gate.h: 注释里写的是 gRPC 线程读闸、Node::StartGrpcServer；线程模型变了要更新，逻辑可以不动。
- [handwritten] cpp/nodes/scene/handler/rpc/scene_handler.cpp: :695-702 的注释说 SceneManager 走 gRPC 路径用的是阻塞许可，改后会过时。
- [handwritten] cpp/libs/services/scene/player/system/asset_op_auth.h: 注释引用了 grpc_handler_gen.go:125 和 node.cpp:623 InsecureServerCredentials，迁移后要更新（只改注释）。
- [third-party] cpp/libs/engine/muduo_windows/src/muduo/net/{Acceptor,TcpServer}.{h,cc}、third_party/muduo-linux/muduo/net/{Acceptor,TcpServer}.{h,cc}、third_party/muduo/muduo/net/{Acceptor,TcpServer}.{h,cc}: 只有在选择「停止监听」方案时才要改：给三份树统一补 stopListening，并考虑 Windows wakeup 空操作的问题。如果选择「接入后立即 GOAWAY 再关」则不用动。
- [generated-output] cpp/generated/proto/scene_manager/scene_node_service.grpc.pb.{h,cc}、cpp/generated/proto/battle/battle_node.grpc.pb.{h,cc}: 第 4 批之后服务端不再引用它们；真正删除和停用 grpc_cpp_plugin 在第 5 批。不手改。
- [build] tools/scripts/k8s_deploy.ps1、tools/scripts/lib/k8s_client_entry.ps1、deploy/k8s/README.md: GRPC_SERVER_MAX_POLLERS 注入退役或改成新的在途上限变量；tcpSocket 探针的注释也要同步。
- [doc] go/shared/assetop/reconcile.go: Workers 上限的理由是「poller 上限 8」，迁移后要按新的在途上限重写注释（是否调参另行决定）。
- [doc] docs/design/grpc-muduo-transport.md: 新增第 4 批的服务端设计章节：线上契约（服务端方向）、关机、回退开关、验证步骤。
- [doc] docs/design/scene-grpc-server-design.md、docs/design/scene-node-threading-model.md、docs/design/thread-count-monitoring*.md、docs/design/battle-transport-decision.md: sync poller、promise/future、关机线程这些描述会过时。
- [doc] .github/copilot-instructions.md、cpp/AGENTS.md: GRPC_SERVER_MAX_POLLERS 的说明，以及 cpp/AGENTS.md:68「gRPC 线程用 AcquireAllocationPermitBlocking」的约定要改。
- [doc] docs/PROGRESS.md、docs/README.md: 追加第 4 批条目（未编译、待 Codex 验证）；新文档登记索引。
## risks
- 守护段丢失：投递前段的查找 key 写死了 '(grpc::ServerContext' 签名（grpc_handler_gen.go:177-179），未识别的命名段会直接 panic（:124-127）。如果改签名时不先写迁移，scene 的 Agones 许可和 battle 的准入闸 + 许可这些手写代码，要么被补成空段静默丢掉，要么生成直接失败。commit 6c4021ae5 就出过同类事故。
- 投递前段挪进 loop 后不能再调 AcquireAllocationPermitBlocking（agones_gameserver_lifecycle.h:39-45），否则最多阻塞 3s 卡死整个逻辑帧。必须先有回调式的异步许可；AllocationPermit 又不可拷贝（:118-136），装进 std::function 要专门处理。
- battle 的准入语义容易被打破。今天的顺序是「闸 → 许可 → loop 内复核闸」；异步许可回到 loop 时，停机可能已经关闸并作废了全部房间。必须在许可回调里、建房之前再复核一次，否则房间会随进程退出无声丢失（battle_admission_gate.h:10-12）。
- 并发模型改变：今天最多 8 个 poller 隐式限流，Go 侧 reconcile.go:256-268 也按这个口径定 Workers。改成 loop 内派发后，没有在途上限的话，一阵突发请求可能挤压 20FPS 的 World::Update，或者让单个 Go 副本压垮一个 scene 节点。
- 关机停接新连接：muduo TcpServer / Acceptor 没有停止监听的接口；而 etcd 发现键直到 FinalizeShutdownInLoop 的 ReleaseNodeId 才撤（node.cpp:1202）。drain 期间 Go 会继续拨号和发请求。没有「接入即 GOAWAY」或停监听，排空可能永远收不拢，或者新请求在拆除阶段才失败。
- Windows 上三份 muduo 树的 EventLoop::wakeup 都是空操作，poll 超时 100ms。第 3 批删掉 5ms CQ 定时器以后，凡是跨线程 queueInLoop（Agones worker 回调、控制台信号的 Shutdown、Kafka 消费线程）都可能被延迟到 100ms。Windows 上 Agones 是 Disabled，许可路径不受影响，但其余路径要先修 wakeup。
- K8s tcpSocket 探针每 5s 连一次 gRPC 端口又不发 preface。如果服务端把这当作协议错误打 ERROR 或做重处理，会刷日志。
- 截止与取消的语义：今天服务端完全忽略截止，Go 侧已按「结果未知」处理。新实现如果在业务执行中途按截止或取消打断（而不是只在投递前拦截），会制造半完成状态；资产路径的 durable 和 outcome 语义尤其不能被破坏。
- bind 地址的策略：官方服务端 bind 的是注册 IP（POD_IP），RPC0 bind 的是 0.0.0.0 或 BIND_IP。新实现如果顺手改成 0.0.0.0，会改变暴露面（scene 的资产 RPC 只靠请求体签名防护，asset_op_auth.h:9-13）。
- 回退开关：同一个 gRPC 端口同一时刻只能有一种实现在监听。如果模板只生成新形态，回退到官方实现时就没有可用的 grpc::Service 适配；保留双形态又会让模板和守护段更复杂。
- 生命周期（AGENTS §11.7）：异步完成句柄、等待许可的回调、截止定时器只能持 weak；连接断开或节点关机后 Finish 必须安全丢弃。nghttp2 回调里不得调用业务，也不得销毁对象。
- 工作区冲突：node.cpp、node.h、node_util.*、scene main.cpp、grpc-muduo-transport.md 都有其他会话未提交的改动（node.cpp 正在把 eventLoop 指针改成引用）。第 4 批改的正是 node.{h,cpp} 的服务端和关机部分，容易冲突。
- 批次前置：设计 §10 规定第 4 批以第 3 批为前置，但 PROGRESS 里还没有第 3 批的条目；第 1、2 批也还没完全验证。
- 推测：grpc C++ 在 Linux 默认开 SO_REUSEPORT，muduo TcpServer 默认 kNoReusePort。切换实现后，同机两个进程意外 bind 同一个 gRPC 端口时的表现会变（更严格），会影响 node_allocator 的重试路径。
- 推测：HTTP/2 服务端暴露在集群内，需要配好 nghttp2 的 rapid-reset、SETTINGS / PING 洪泛防护以及头部上限；官方 grpc 默认带了一部分这类保护，新实现必须显式配置。
## openQuestions
- 回退开关的形态：要不要加一个与第 1、2 批同形的启动开关（比如 NODE_GRPC_SERVER_TRANSPORT=grpc|muduo_h2）？如果要，生成器要不要同时输出官方 grpc::Service 适配和 muduo 派发两种形态，还是一刀切？
- 在途上限取多少、超限时怎么处理：回 RESOURCE_EXHAUSTED、回 UNAVAILABLE，还是排队？要不要宣告 SETTINGS_MAX_CONCURRENT_STREAMS（grpc-go 会按它在客户端排队、等到截止）？Go 侧 reconcile.go 的 Workers 要不要跟着调？
- 停止接新连接的方式：改三份 muduo 树给 Acceptor 补 stopListening，还是接入后立即 GOAWAY 再关？drain 期间要不要提前撤 etcd 发现键或标记下线，让 Go 不再派新请求？
- kGrpcDrainTimeout 继续用 2s 吗？到期后对还在等 Agones 许可或业务未完成的调用，统一回什么状态码（CANCELLED / UNAVAILABLE），要不要发 RST？
- 截止时间到了但业务已经开始的调用：照常回 DEADLINE_EXCEEDED trailers，还是只发 RST_STREAM(CANCEL) 并丢弃应答？
- 这一批是否就要支持延后完成（Handle* 接管完成句柄去等 Redis ACK 等）？还是只把外层包装改为 loop 内同步完成，Handle* 签名保持不变？
- 异步许可的接口放在哪一层：GameServerLifecycle 直接接 EventLoop，还是传入一个投递函数？scene 的 muduo RPC 路径（SceneHandler::CreateScene 用 NonBlocking、失败即拒绝）要不要一并改成异步？
- gRPC 端口的 bind 地址：沿用注册 IP，还是与 RPC0 统一成 0.0.0.0 / BIND_IP？
- scene 在停机窗口没有准入闸（Disabled 下许可恒放行，hook 之后还会执行新到的 CreateScene / PrepareBattle / AssetCredit），要不要借这一批补上？
- 要不要向 handler 暴露只读的入站 metadata（今天没有任何调用方用，资产认证在请求体里）？
- gRPC 的 HandleCreateScene 不校验 config_id / scene_id 为 0，muduo 路径会拒绝。迁移时是保持原样，还是补齐？

