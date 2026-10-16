# 节点间 muduo RPC:每次调用的回调、延后回包与完整的完成语义(RpcChannel 改造)

> 状态:**批 1–5 已实施,未编译,待 Codex 验证**(2026-10-15,Claude)。用户授权实施到第 5 批,升级口径选「同 zone 停服一次切换」;
> 实施与本文的偏离、未做项与验证步骤见 §18。批 6(玩家回包死链)与 gRPC 每次调用回调见 §19(2026-10-15 续,未编译);批 7 见 §19 后续小节。
> 本文不引用此前任何会话的方案为既定结论;`build/rpc-lambda-tools/`(被 .gitignore 忽略的早先尝试)不是本设计的一部分。
> 事实核对基线:`54c9d775d`(2026-10-15 03:08)。行号以该提交为准,并发会话仍在改 gate 相关文件,动手前须重新核对。

## 0. 一句话

把本仓自写的 `GameChannel` 按 muduo 原版 `RpcChannel` 的结构补完:信封加调用号 `id`,每次调用登记一个 `OutstandingCall`
(带调用方自己的 lambda),服务端拿到一个真正能用的 `done`;再补上原版缺的四样——超时、断连/销毁/关机时让在途调用以失败完成、
把对端错误交给调用方、全部生命周期只用 weak。按 message_id 的全局回包表(`gRpcResponseDispatcher` / rpc_replies)随之退场;
`GateHandler::RoutePlayerMessage` 一族不改造,改为显式拒绝。

## 1. 现状(为什么要改)

节点间 RPC0(`GameChannel` + `GameRpcMessage`,帧格式 `长度 | "RPC0" | payload | adler32`)今天只承载一条边:
gate 出站 `RpcClient` ↔ scene 入站 `RpcServer`,**双向**(gate 把 `GateHandler` 注册在出站 RpcClient 上,scene 经 `RpcSession` 反向调用,
`node_connector.cpp:129-144`)。battle 不说 RPC0(端口给了 `BattleClientEdge`),scene 没有出站 TcpClient(`scene/main.cpp:85-87`)。
所有 channel 操作都已在节点唯一的 EventLoop 上(无 `setThreadNum`;gRPC / Kafka 经 runInLoop / queueInLoop 回 loop),只是从未断言。

缺陷(逐条有代码证据,见附录 A):

1. **没有调用号**:`GameRpcMessage` 只有 `type/request/response/error/message_id`(`proto/common/base/rpc_message.proto:24-31`)。
   回包按 message_id 投给进程级 `gRpcResponseDispatcher`(`game_channel.cpp:97,408`),同一方法并发调用无法区分,调用方上下文只能靠对端回显业务字段找回
   (`scene_response_handler.cpp:60-87` 靠 `session_id`,且不防 session 号回绕串号)。
2. **是否回包由服务端响应内容决定**:Empty 或 0 字节不回(`game_channel.cpp:488-495`)。
3. **服务端只能同步完成**:`done` 恒为 nullptr,CallMethod 一返回就发(`:486`)。A→B→C 只能写成"先回受理、后推送",
   `RoutePlayerMessage` 更是"B 在转发给 C 的同一刻就把镜像应答回给 A"(`gate_service_handler.cpp:149-215`)。
4. **失败全部静默**:RPC_ERROR 只记日志(`:342-379`);未连接 / 锁不住连接 / 序列化失败只打日志(`rpc_client.h:93-115`、`game_channel.cpp:121-131`)。
5. **没有超时**;断连、节点摘除、关机都不完成任何在途调用。
6. **空槽消息号可打崩节点**:message_id 3(`dbTest`)在 `gRpcMethodRegistry` 里 `serviceName == nullptr`(DB 域被跳过,
   `rpc_event_registry.h:12`),`IsValidMessageId` 只查越界(`game_channel.cpp:135-144`),随后用 nullptr 构造 `std::string`
   (`:455-456,:511-513,:356-358`)——任何能连上节点间端口的进程发一帧就能触发 UB。`scene_handler.cpp` 的 `gPlayerService.find(serviceName)` 同类。

真正带业务的回包只有 3 个:gate 的 `OnSceneProcessClientPlayerMessageReply`(回推客户端)、gate 的 `OnSceneNodeHandshakeReply`、
scene 的 `OnGateNodeHandshakeReply`(不可达:scene 从不发 GateNodeHandshake)。其余 rpc_replies 全是空壳。

## 2. 目标与非目标

目标(用户约束):只改 C++,继续用现有生成器(`tools/proto_generator/protogen`,改其模板/谓词属于"使用");channel 操作同一线程;
每次调用独立 lambda 并保留该次上下文;A→B→C 不阻塞 EventLoop;超时 / 错误 / 断连 / 生命周期完整,不重复完成、不误处理迟到回包;
muduo 原版概念沿用原名。

非目标(本轮不做,§14 登记为后续):

- 玩家级 handler 的延后完成:`PlayerService::CallMethod(method, player, req, resp)` 没有 controller / done
  (`player_service_interface.h:15-18`),需改 `player_service_gen.go`,另起设计。本轮 A→B→C 只覆盖**节点级** handler。
- 生成的 gRPC 异步客户端的每次调用回调(今天是按方法的全局 handler + 文件级 deadline)。B 的下游若是 gRPC,本轮只能自建
  "关联号 → RpcDoneCallback"表(§8.4)。
- 跨节点 trace 传播、跨跳传递剩余预算(线上字段)。

## 3. 命名对照(沿用 muduo / muduo-protorpc 原名)

| muduo 原名 | 本仓 | 备注 |
|---|---|---|
| `RpcChannel` | `::RpcChannel`(`GameChannel` 改名;`network/rpc_channel.{h,cpp}`) | 全局命名空间,同 `::RpcController` 先例;**不继承** `google::protobuf::RpcChannel`(§4.6)。`rpc_server.cc` 在 `namespace muduo::net` 里,一律写 `::RpcChannel`;禁止 include `muduo/net/protorpc/RpcChannel.h`(未编译但同名) |
| `CallMethod` | `RpcChannel::CallMethod` | 需要应答的调用 |
| `id` / `id_` | 信封 `id`;`RpcChannel::id_` | 每 channel 单调、从 1 开始、恒非 0 |
| `outstandings_` / `OutstandingCall` | 同名 | 表的实现复用 `InflightIndex`(§4.3) |
| `done`(客户端)/ `ClientDoneCallback` | `ClientDoneCallback` | muduo-protorpc cpp11 名;签名多一个 `RpcStatus`(原版无法表达失败) |
| `done`(服务端)/ `doneCallback` / `RpcDoneCallback` | 生成签名里的 `Closure* done` 改为**真正可用**;`RpcChannel::DoneCallback(...)`(私有,按原 id 回包);`RpcDoneCallback`(延后完成句柄) | 形状偏离:cpp11 的 `RpcDoneCallback` 是 `std::function<void(const Message*)>`,本仓是带 `Run()` 的可拷贝句柄 |
| `onMessage` → `onRpcMessage` | `OnMessage` → `OnRpcMessage`(原 `HandleIncomingMessage` / `HandleRpcMessage`) | |
| `callServiceMethod` | `CallServiceMethod`(原 `ProcessMessage`) | |
| `onDisconnect` | `OnDisconnect` | 原版只有 `//FIXME:`,这里实现 |
| `setConnection` / `setServices` / `services_` | 由构造参数给定(每连接一个 channel,§6.1) | |
| `noreturn`(muduo-protorpc rpc2 方法选项) | `SendRequest`(信封 id=0) | 按**次**决定,不按方法 |
| `Stub` | 生成器产出的方法 tag(§10),不另生成 Stub 类 | 不叫 `<Svc>Stub`:与 protobuf 的 `<Svc>_Stub` 只差一个下划线 |
| `ErrorCode` / `TIMEOUT` … | 线上仍是 `GameErrorCode`;本地完成状态是 `RpcStatus` | 避开 Windows `ERROR` / `NO_ERROR` 宏 |

## 4. 契约

### 4.1 线格式(`proto/common/base/rpc_message.proto`)

```proto
enum GameMessageType {
  REQUEST = 0;
  RESPONSE = 1;
  reserved 2, 3;
  reserved "RPC_CLIENT_REQUEST", "NODE_ROUTE";   // 2026-10 删除:前者从未发送,后者接收端是空函数;不得复用
  RPC_ERROR = 4;                                 // 框架或 handler 拒绝本次调用;回显请求的 id 与 message_id
}
enum GameErrorCode {
  RPC_NO_ERROR = 0; WRONG_PROTO = 1; NO_SERVICE = 2; NO_METHOD = 3; INVALID_REQUEST = 4;
  INVALID_RESPONSE = 5;   // 线上不发;本地状态见 RpcStatusCode
  TIMEOUT = 6;            // 线上不发;本地状态见 RpcStatusCode
  HANDLER_FAILED = 7;     // handler 显式失败(controller->SetFailed / RpcDoneCallback::Fail):handler 已运行
  REPLY_DROPPED = 8;      // handler 接管 done 后未完成即释放:结果未知
}
message GameRpcMessage {
  GameMessageType type = 1; bytes request = 2; bytes response = 3; GameErrorCode error = 4; uint32 message_id = 5;
  uint64 id = 6;          // 调用号(= muduo RpcMessage.id):发起方 channel 内单调递增、从 1 开始;0 = 单向
}
```

- 字段号:1–5 已占,原版的 `fixed64 id = 2` 不能照搬,取 6(AGENTS §4.3 不复用)。类型 uint64 varint:uint32 按 5 万次/s 约 24h 回绕;
  不用 fixed64 是因为本仓不做 balancer_raw 式原地改写 id,varint 小号只占 1–3 字节;id=0 时 proto3 不序列化,单向推送(AOI 广播是最大流量路径)零开销。
- **REQUEST `id != 0`:服务端必须恰好回一帧**——成功回 RESPONSE(同 id、同 message_id;Empty / 0 字节响应也回),失败回 RPC_ERROR(同 id、同 message_id + error)。
- **REQUEST `id == 0`:单向**(调用方声明不要应答,按次决定)。终态:服务端永不回 RESPONSE / RPC_ERROR,只计数。
  过渡期(§11):服务端对 id==0 保持旧规则(响应非 Empty 且非 0 字节才回、回包不带 id),只为服务尚未迁移的调用方;收口批删除。
- RESPONSE / RPC_ERROR:`id != 0` → 查本端 `outstandings_`;查不到 = 迟到 / 未知,丢弃并计数。`message_id` 与登记的不一致 → 以 `kInvalidResponse` 完成(纵深防御),
  解析一律按**登记的** messageId 取响应原型。`id == 0` → 旧对端或旧调用方的回包,过渡期交给旧的按 message_id 分发,收口后丢弃并计数。
- 未知 `type`(含 2、3):计数、限频告警、丢弃,不断连。
- 这是 C++ 节点内部信封,不是客户端契约(client↔gate 用 `ProtobufCodec`;robot 用 TcpCodec;Go 只有生成物;Java `NodeLinkCodec` 注明与 RPC0 不兼容)。
  改 proto 会连带重生成 C++ pb、`go/proto` 与 `robot/vendor` 的 `.pb.go`、`generated/` 下 4 份镜像——生成连带,不是 Go 业务改动。
  同批删除无人引用的陈旧副本 `cpp/libs/engine/core/network/game_rpc.proto`。

### 4.2 线程

`RpcChannel` 的公开入口(`CallMethod` / `SendRequest` / `OnMessage` / `OnDisconnect` / `Close` / `RpcDoneCallback::Run/Fail` / 超时 tick)
一律 `loop_.assertInLoopThread()`(muduo 的实现不受 NDEBUG 影响,Release 下同样 LOG_FATAL)。去掉原版 `MutexLock` / `AtomicInt64`。
**析构函数里不断言、不 LOG_FATAL**(§6.4)。`loop_` 是构造时注入的 `muduo::net::EventLoop&`(引用成员,裸指针门禁不拦;出站取 `*client_.getLoop()`,入站取 `*conn->getLoop()`)。
channel 必须由 shared_ptr 持有(私有构造 + `static Create(...)`,照 `muduo_grpc::Channel`)。

### 4.3 客户端:CallMethod / SendRequest

```cpp
enum class RpcStatusCode : uint8_t {
    kOk,
    kNotSent,          // 本地确定未发出:未连接 / 通道已关闭 / 在途已满 / 消息号不可派发 / 类型与注册表不符 / 序列化失败 —— 确定未执行
    kRemoteRejected,   // 对端 RPC_ERROR:NO_SERVICE / NO_METHOD / INVALID_REQUEST / WRONG_PROTO —— 派发前拒绝,确定未执行
    kRemoteFailed,     // 对端 RPC_ERROR:HANDLER_FAILED —— handler 已运行并显式失败
    kReplyDropped,     // 对端 RPC_ERROR:REPLY_DROPPED —— 结果未知
    kTimeout,          // 结果未知
    kDisconnected,     // 在途期间连接断开 —— 结果未知
    kChannelClosed,    // 通道销毁 / 节点关机 —— 结果未知
    kInvalidResponse,  // 回包解析失败或 message_id 不符 —— 结果未知
    kCount
};
// 平铺名字表 + static_assert(std::size(kRpcStatusCodeNames) == kCount)(AGENTS §11.2,不用 X-macro)

class RpcStatus {
public:
    bool ok() const;
    RpcStatusCode code() const;
    GameErrorCode remoteError() const;     // 仅 kRemote* / kReplyDropped 有意义
    bool DefinitelyNotExecuted() const;    // kNotSent / kRemoteRejected:可安全重试
    bool OutcomeUnknown() const;           // kTimeout / kDisconnected / kChannelClosed / kInvalidResponse / kReplyDropped
};

using ClientDoneCallback = std::function<void(const RpcStatus&, const MessagePtr& response)>;

struct RpcCallOptions { std::chrono::milliseconds timeout{kDefaultRpcCallTimeout}; };   // 必须 > 0;没有"不限时"

// 发起需要应答的调用。done 恰好被调用一次,并且**永不**在 CallMethod 返回之前被调用(本地失败也经 queueInLoop 投递)。
void CallMethod(uint32_t messageId, const ProtobufMessage& request, ClientDoneCallback done, const RpcCallOptions& = {});
template <typename Method, typename Done>   // Method = 生成的方法 tag(§10);Done: void(const RpcStatus&, const std::shared_ptr<typename Method::Response>&)
void CallMethod(const typename Method::Request& request, Done&& done, const RpcCallOptions& = {});

// 单向(id = 0):调用方声明不要应答。返回是否已交给连接(之后不保证送达)。不加 [[nodiscard]]:推送方拿到 false 也无事可做,失败内部计数 + 限频日志。
bool SendRequest(uint32_t messageId, const ProtobufMessage& request);
template <typename Method> bool SendRequest(const typename Method::Request& request);
```

调用方契约(写进头文件注释):

- (a) `response != nullptr` ⇔ `status.ok()`;失败路径不得解引用 response。
- (b) done 只在 loop 线程执行;捕获**值与 weak**(session_id、incarnation、实体 id + 版本、`weak_ptr<RpcClient>`),执行时重新查找、`try_get` 校验。
  不捕获 `RpcChannelPtr`、`TcpConnectionPtr`、controller、request/response 裸指针,不捕获可能先于它析构的对象的 `this`。
- (c) RESPONSE / RPC_ERROR 的 done 在该连接的 I/O 回调栈上**同步**执行(保持同一连接内"回包与后续请求"的处理顺序,今天的 rpc_replies 也是同步);
  在其中**不得同步销毁**本 channel 所属的 RpcClient 或摘节点(与 handler 侧既有规则一致,`rpc_controller.h:36-38`),需要时 queueInLoop。
- (d) 失败语义沿用 gRPC 口径"传输失败 = 结果未知"(`grpc-client-deadline-failure-callback.md` §3.3):`OutcomeUnknown()` 时只释放本地在途状态、提示客户端、重试幂等操作,
  不得当作"对端拒绝"去改权威状态;`DefinitelyNotExecuted()` 可安全重试。RPC 层不自动重试。
- (e) "恰好一次"的唯一例外:EventLoop 已退出后才析构的 channel,在途 done 随容器销毁而不调用(§6.4)。所以不得把资源释放只挂在 done 上;
  也因此 **gRPC sync handler 不得用 future 阻塞等待一次 RPC0 调用的结果**(事故 §7.13 的同形)。
- (f) typed 版本在调用时校验 `Method::Response::descriptor()` 与注册表 `responseProto` 一致,完成时 `static_pointer_cast`;`Done` 必须可拷贝构造(`std::function`)。

**与 `muduo_grpc::Channel` 对齐**:同为"恰好一次 + 永不在调用栈内同步回调 + 本地失败也经 queueInLoop"(`muduo_grpc/channel.h:5-12`),
调用方只需写一条失败路径、没有重入。代价是本地失败多一次闭包分配(重连窗口内每条客户端消息一次),可接受。

`RpcClient` / `RpcSession` 只是薄壳:转发同名 `CallMethod` / `SendRequest`;当前没有 channel(未连接)时 `CallMethod` 经 `client_.getLoop()` 投递 `kNotSent`。
删除 `CallRemoteMethod`(名字暗示有应答,线上却与 `SendRequest` 完全相同,`game_channel.cpp:280-305`)、`RouteMessageToNode`、`SendRouteResponse`。

### 4.4 恰好一次

`outstandings_` 用 `InflightIndex<OutstandingCall>`——把 `gate_lane::InflightIndex`(`router_lane.h:126-214`:unordered_map + 惰性删除的最小堆,
`Extract` 恰好一次、`ExtractExpired(now)` 按截止先后)原样上移到 engine/core(只依赖 STL),gate_lane 以别名沿用,新增 `ExtractAll()`。
不另写第三套在途表(AGENTS §11.5.2;`muduo_grpc::Channel` 的 `calls_ + deadlines_` 可在其作者同意后迁过来,§14)。

```cpp
struct OutstandingCall {
    SteadyTime expiresAt;            // InflightIndex 要求的成员
    uint32_t messageId;
    ClientDoneCallback done;
    tracing::TraceContext trace;     // 发起时的 trace,回调前恢复
};
```

唯一完成出口 `CompleteCall(id, status, response)`:`outstandings_.Extract(id)` 成功 → **之后**才在 `tracing::ScopedSpan(call.trace)` 内调用 `done`。
取不到就什么都不做。不依赖定时器 cancel(Linux 上游 muduo 同批取消不拦截,`third_party/muduo/muduo/net/TimerQueue.cc:191-194`),
不依赖"I/O 先于定时器"的顺序(Windows 是 I/O → timer → pending,Linux 不定)。done 中可重入 `CallMethod`(新 id 不在正在处理的批次里)。

| 完成路径 | 状态 | 在哪执行 |
|---|---|---|
| RESPONSE 命中 | `kOk`;解析失败 / message_id 不符 → `kInvalidResponse` | 同步,`OnRpcMessage` 栈 |
| RPC_ERROR 命中 | 按 error 映射 `kRemoteRejected` / `kRemoteFailed` / `kReplyDropped` | 同步,`OnRpcMessage` 栈 |
| 超时 tick | `kTimeout` | 同步,定时器栈(先把过期前缀整批摘出,再逐个回调) |
| 本地失败(CallMethod 内) | `kNotSent` | `queueInLoop` |
| `OnDisconnect` | `kDisconnected` | 同步 `ExtractAll`,回调整批 `queueInLoop`(避开 DOWN 栈与 `RemoveRpcSessionsBoundTo` 遍历 registry 的栈) |
| `Close()` / `~RpcChannel` | `kChannelClosed` | 同上;析构见 §6.4 |

### 4.5 超时

- 截止时刻 `steady_clock`(AGENTS §11.3),muduo 定时器只负责唤醒。
- 每 channel 一个**惰性 tick**:有在途调用时 `runAfter(0.1s)`,触发后 `SweepTimeouts(now)`,仍有在途且未关闭才续挂;**从不 cancel**。
  tick 只捕获 `weak_ptr<RpcChannel>`,channel 没了 lock 失败即结束。不用"每调用一个 muduo Timer"(每次 new Timer + 两次 std::set 操作,
  且 gate 每条请求类客户端消息都是一次调用)。精度 ±100ms,RPC 超时是秒级;短于 100ms 的超时精度不保证。
- 墙钟回拨会推迟 tick 唤醒(muduo Timestamp 是 gettimeofday)——引擎级残余,与 `RunAtMonotonic` 已登记的同一残余(`player_lifecycle.cpp:371-382`),本轮不解决;
  计 `max_sweep_lateness_ms`(发现过期时刻 − 截止时刻)用以区分 loop 积压与时钟问题。
- 测试接缝:`SweepTimeouts(SteadyTime now)` 公开(loop 线程调用),单测不依赖墙钟:CallMethod 前后取 t0/t1,真实截止必在 `[t0+T, t1+T]`;
  `SweepTimeouts(t0+T-1ns)` 必不过期、`SweepTimeouts(t1+T)` 必过期;用例超时一律设 1h,真实 tick 永不误触发。
- 数值(命名常量,注释写推导):`kDefaultRpcCallTimeout = 10s`(与 gRPC `kDefaultGrpcCallDeadlineMs` 同值);
  `kGateToSceneClientCallTimeout = 12s`,不变式 **I5**:scene 侧下游最大 deadline(10000)+ 1000 ≤ T ≤ 客户端请求超时(15000)− 1000
  (对齐 `client-request-ordering.md` §5 的 I4;登记 I5 由该文档 owner 同意后补);`kNodeHandshakeCallTimeout = 3s`(今天握手按 0.5s 节奏重试)。
  A→B→C:`T(B→C) + B 自身处理 < T(A→B)`,否则 B 的回复到 A 时已过期,只会被计数丢弃(结果未知,与 gRPC 口径一致)。
- 每 channel 在途上限 `kMaxOutstandingCalls = 65536`(与 `muduo_grpc::ChannelOptions::maxOutstandingCalls` 同值),超限 `kNotSent` 并计数。

### 4.6 为什么不继承 `google::protobuf::RpcChannel`、不用生成的 `<Svc>_Stub`

protobuf 的 `CallMethod(method, controller, request, response*, Closure*)` 是"调用方拥有 response 裸指针 + 自删除 Closure",失败只能经 controller;
Stub 按 `MethodDescriptor` 调用,本仓线上用的是 message_id,还得另建反查表;muduo-protorpc 的 cpp11 分支自己也改成了 `std::function`。
同时支持两套会让"恰好一次"的证明翻倍,而生成的 Stub 全仓无人使用(YAGNI)。

## 5. 服务端:默认同步,显式接管 `done`(A→B→C)

### 5.1 派发

`CallServiceMethod` 在栈上构造 `RpcController`(仍是本次派发的 ctx,仍不可存储)和一个栈上的 `DoneClosure`(继承 `::google::protobuf::Closure`),
把 `done` 作为**非空**参数传给 `service->CallMethod`。CallMethod 返回后:若 handler 既没有同步 `done->Run()`、也没有接管(Adopt),框架立即完成。
既有 24 个节点间 handler 不碰 done,"返回即回包"零改动;`GmGracefulShutdown` 依赖的"回包先于 queueInLoop(Shutdown)"时序不变。
同步路径零额外分配(DoneClosure 在栈上,只有 Adopt 才分配共享状态)。

完成规则(`id != 0`):`controller.Failed()` → RPC_ERROR(HANDLER_FAILED);否则 RESPONSE(必回,Empty / 0 字节也回)。修掉"必回之后 `SetFailed` 被忽略 → fail-open"
(`battle_handler.cpp:33-41` 是先例)。`id == 0`:过渡期按旧规则,收口后不回。

派发前的拒绝(`IsDispatchableMessageId`:越界或注册表空槽 → INVALID_REQUEST;无服务 → NO_SERVICE;无方法 → NO_METHOD;解析失败 → INVALID_REQUEST)
都回显 id。修掉 §1 第 6 条的空槽崩溃。

### 5.2 `RpcDoneCallback`

```cpp
class RpcDoneCallback {                      // 可拷贝:内部 shared_ptr<State>,所有副本共享一个完成标记
public:
    static RpcDoneCallback Adopt(::google::protobuf::Closure* done);   // 只能在 handler 同步栈内对本次派发的 done 调用;异类 / nullptr → LOG_FATAL
    template <typename Response> Response& response() const;            // 完成前有效;descriptor 不符 → LOG_FATAL
    void Run() const;                                                    // 回 RESPONSE(muduo doneCallback 语义);第二次起吞掉并计数
    void Fail() const;                                                   // 回 RPC_ERROR(HANDLER_FAILED)
    bool IsCallerConnected() const;                                      // A 已断开时 B 可提前止损
};
```

- State 持 `weak_ptr<RpcChannel>`、id、messageId、request / response 的 `MessagePtr`(保活 handler 拿到的裸指针)、trace、完成标记。
  **绝不持有**强 `TcpConnectionPtr` / 强 `RpcChannelPtr`(§11.7)。每连接一个 channel(§6.1),所以 channel 的 weak 就等价于"请求进来的那条连接":
  重连之后旧 channel 已死,`Run()` lock 失败即丢弃——迟到回复不可能落到新连接上、被对端新 channel 里同号的调用误认领。
- `Run()` / `Fail()` 发包条件:channel.lock() 成功且其连接仍 connected;否则丢弃并计数。`Close()`(关机)之后仍允许发,让 B 能把结果回给 A。
- 最后一个副本析构仍未完成:`id != 0` 时回 RPC_ERROR(REPLY_DROPPED)(A 立即失败而不是等超时);`id == 0` 时什么都不发。
  只在 `EventLoop::getEventLoopOfCurrentThread() == &loop`(只比地址)时发包,否则静默计数——析构可能发生在 `~EventLoop` 丢弃闭包、thread_local 析构等任意栈上。`noexcept`,不 LOG_FATAL。
- Adopt 多次返回同一状态的副本。handler 在 Adopt 之后**同步**失败:先写 `response<T>()` 业务码再 `Run()`,不要直接 return(否则只剩 REPLY_DROPPED,业务码丢失)。
- `CallMethod` 的 done 若按值捕获了 reply,而 CallMethod 本地失败——done 仍会(经 queueInLoop)被调用一次,reply 照常在 lambda 里完成;不存在"done 被就地析构"的路径。
- 延后完成路径直接写本次 `response<T>().mutable_error_message()`，同步路径直接写本次 response。请求级全局 tip 已移除，`Run()` 不再读取或清理其它请求的拒绝状态；生成层对每个非 `Empty` 应答静态约束其 `error_message` 字段类型。
- Defer 不提供次序保证:某条消息一旦接管,同会话后续同步消息会先回包。需要串行时用业务在途闸(`IsSceneChangeBusy` 写法)。
- `RpcController` 契约不变(栈上、不可存储);它的路由 / 会话字段(零读写者)删除。

### 5.3 A→B→C 写法(B = 节点级 handler,守护段内)

```cpp
void SomeHandler::Foo(::google::protobuf::RpcController* controller, const ::FooRequest* request,
                      ::FooResponse* response, ::google::protobuf::Closure* done)
{
    ///<<< BEGIN WRITING YOUR CODE
    if (!Validate(*request)) { response->mutable_error_message()->set_id(kParamError); return; }   // 未接管:框架立即回

    const auto reply = RpcDoneCallback::Adopt(done);                       // 返回后框架不再自动回包
    gateSession->CallMethod<GateBarRpc>(MakeBarRequest(*request),
        [reply, playerId = request->player_id()](const RpcStatus& status, const std::shared_ptr<::BarResponse>& bar)
        {
            auto& out = reply.response<::FooResponse>();
            if (!status.ok()) { out.mutable_error_message()->set_id(kServiceUnavailable); reply.Run(); return; }   // C 失败也必须完成
            if (tlsEcs.GetPlayer(playerId) == entt::null) { out.mutable_error_message()->set_id(kPlayerOffline); reply.Run(); return; }
            /* B 继续自己的业务 */
            reply.Run();                                                    // 回 A;A 的 lambda 随后继续
        },
        RpcCallOptions{.timeout = kFooDownstreamTimeout});                 // 小于 A→B 的超时
    ///<<< END WRITING YOUR CODE
}
```

等待 C 期间 handler 早已返回,EventLoop 照常处理其他消息。今天的拓扑里 RPC0 的 A→B→C 实例是 gate₁ → scene → gate₂(scene 经 gate₂ 的 `RpcSession` 调用);
下游是 gRPC / Redis / 定时器时同样持有 `reply` 完成(gRPC 见 §8.4)。

## 6. 断连、销毁、关机

### 6.1 出站也改成每连接一个 channel

今天出站 RpcClient 终身复用一个 channel、每次 UP 只换连接(`rpc_client.h:26,165`),带来两个问题:旧连接的在途调用要在 DOWN 时手动清;
gate 作为服务端延后回包时会用新连接回旧 id(对端新 channel 的 id 从 1 重来,可能静默错投)。改成与入站 RpcServer 对称:

- UP:`channel_ = RpcChannel::Create(*client_.getLoop(), conn, services_ 的拷贝)`,并 `conn->setMessageCallback(...)` 绑 `weak_ptr<RpcChannel>`,lock 后派发。
  消息回调只在取 channel 时持有 RpcClient,派发期间不强持它 —— 这只是为了不延长 RpcClient 的寿命;在连接 / 消息回调栈上同步销毁 RpcClient
  仍然禁止(muduo 回调期间自己就持着连接的强引用:handleRead 的 shared_from_this 临时量 + Channel 的 tie guard,`~TcpClient` 必然看到 use_count>1、
  跳过 forceClose,重现 `TcpConnection.cc:71` 事故),摘节点一律 queueInLoop(§4.3 契约 (c))。
- DOWN:`channel_->OnDisconnect(); channel_.reset();`。`connected()` = `channel_ != nullptr`。
- 服务表改为 channel 自持拷贝(1–3 项的 `std::map<std::string, Service*>`;Service 对象本身由节点长期持有),消除 `reference_wrapper` 借用
  ——那份借用在"channel 比 RpcClient 多活一会儿"(消息回调 lock 着)时会悬空(评审 R1)。

### 6.2 入站

`RpcServer::onConnection`:UP 时 messageCallback 从裸 `get_pointer(channel)`(`rpc_server.cc:66-67`)改为捕获 `weak_ptr<RpcChannel>`;
DOWN 时先 `OnDisconnect()` 再 `setContext(空)` 与 `RemoveRpcSessionsBoundTo`。DOWN 分支不得访问声明在 `tcpServer` 之后的 RpcServer 成员
(`~TcpServer` 会同步对每条连接执行 `connectDestroyed` 并以裸 this 回调 onConnection)。
`RpcSession` 改为持 `weak_ptr<RpcChannel>`(channel 归连接所有),`IsConnected()` = lock 成功且已连接。

### 6.3 销毁

节点摘除、同 uuid 重注册(`node_connector.cpp:122`)、Finalize 的 `clear<RpcClientPtr>` 都不经过 DOWN 回调(weak lock 失败进不了 onConnection),
由 `~RpcChannel` 兜底:表非空则整批以 `kChannelClosed` 失败(§6.4 的守卫)。

### 6.4 析构与 loop 存活

`~RpcChannel`:表空 → 什么都不做,不碰 loop。表非空 → 仅当 `EventLoop::getEventLoopOfCurrentThread() == &loop_` 时 `ExtractAll` 并 `queueInLoop` 批量失败;
否则(loop 已析构或不在 loop 线程)只计数 + 一行 ERROR,done 随容器销毁——这条路径只在关机收尾遗漏时出现,视为缺陷信号。

### 6.5 节点关机:两段式 Finalize

现状 `FinalizeShutdownInLoop` 同步执行 `tlsEcs.Clear()` 与 `logSystem.stop()` 之后才排两跳 quit(`node.cpp:1167-1236`),
所以任何在它之前或之中 `queueInLoop` 的失败回调(OnDisconnect / ~RpcChannel / muduo_grpc 的 UNAVAILABLE)都会跑在已清空的 ECS 与已停的日志之上;
Finalize 还可能从 drain 定时器进入,同一轮 I/O 阶段排进来的 DOWN 闭包同样落在拆除之后(评审 R1 blocker)。改为:

1. **阶段 0**(`FinalizeShutdownInLoop` 开头):取消各定时器 → `NodeUtils::CloseAllRpcChannels()`(遍历各 node registry 的 `RpcClientPtr` 与 `RpcSession`,
   对 channel `Close()`:置 closed、`ExtractAll`、queueInLoop 批量 `kChannelClosed`)→ `queueInLoop(FinalizeTeardownInLoop)` 后返回。
2. **阶段 1**(`FinalizeTeardownInLoop`):原有的 ReleaseNodeId → etcd → kafka → redis → clear<RpcClientPtr / MuduoGrpcChannelPtr> → tlsEcs.Clear → logSystem.stop → 两跳 quit。

FIFO 保证:阶段 0 之前已入队的 DOWN / 析构闭包、阶段 0 新入队的 Close 闭包,都在 ECS、Kafka、Redis、日志拆除之前执行;失败回调产生的 Kafka / Redis 工作还能被 flush,
B 回 A 的包也还能发出。契约写明:关机期间 done 里再 queueInLoop 的后续工作会落到拆除之后(两跳 quit 仍再排空一层),不要依赖。
建议(需 muduo_grpc 作者同意):把 `clear<MuduoGrpcChannelPtr>` 也挪进阶段 0,它的 UNAVAILABLE 回调有同一问题。
可选:把"在途调用数 + 未完成的 RpcDoneCallback 数为 0"接进 `SetShutdownDrainComplete` 谓词,让 B 在拆除前把 A 的回复发完。

已核实关机入口都在 loop 的安全栈上(admin RPC 经 queueInLoop、Linux 信号走 runEvery、Windows Ctrl 走跨线程 queueInLoop),两段式可行。
阶段 0 到拆除之间 loop 仍在转(另一会话加的 gRPC CQ 排空 `DrainGrpcClientsInLoop` 会把这段拉长):出站 `RpcClient` 在阶段 0 被关闭后,
enableRetry 重连建出的 channel 一出生就是关闭态(`RpcClient::CloseChannel` 置位),新连接上不会再登记在途调用。残余:这段窗口里对已关闭 channel
发起的 `CallMethod` 以 kNotSent 经 queueInLoop 投递,若排在拆除之后执行,回调里只能做 weak / try_get 查找(契约 (b) 本来就要求),不得依赖 ECS 与日志;
Finalize 之后 acceptor 仍在监听,新连入的入站连接会建出未关闭的 channel(今天 scene 侧只经它单向推送)——这是既有问题,只登记。

## 7. 迟到回包与身份

- 迟到回包靠 §4.4 自然落空;id 每连接单调,旧连接的回包落不到新连接上。
- gate 回推客户端时按 `SessionInfo.incarnation`(`session_info_comp.h:85-87`,专为挡"session_id 被复用后的迟到旧回包")复核,而不是 playerId
  (绑定前 playerId 是 kInvalidGuid 会误杀)。复用 `client_request_lane.cpp` 的 `FindLiveSession(sessionId, incarnation)` / `SendEnvelope`(导出到公共头)。
- 目标绑定到具体 `RpcClient` / `RpcSession` 实例,不按 node_id 解析后长期挂起(node_id 可被新实例复用,`FindNodeEntityByNodeId` 线性扫描取第一个)。

## 8. 调用点迁移

### 8.1 gate → scene 客户端消息(`HandleTcpNodeMessage`)

按**内层**方法分流:`gRpcMethodRegistry[innerId].responseProto` 是 Empty(MoveStart / MoveStop / MoveSync 等,`player_movement.proto:106-108`)→ `SendRequest`(id=0):
不登记、不回包,线上与今天逐字节相同(`client-request-ordering.md` O2 的前提不变)。否则 `CallMethod<SceneProcessClientPlayerMessageRpc>`,
lambda 捕获 `{sessionId, incarnation, clientRequestId, innerMessageId}`:

- `ok` 且有 `message_content` → `FindLiveSession` 后原样下发;`ok` 但无内容(scene 早返回)→ 保持今天行为(不下发);
- 失败 → 回带请求 id 的信封 `MessageContent{id, message_id, error_message.id = kServiceUnavailable(1003)}`(与路由 lane、Java 后端失败同形),不再用不带 id 的 `SendTipToClient`;
  同一会话断连批量失败时限频合并。只用已有 tip 1003(fault=1);需要区分"结果未知"时在 `Tip.xlsx` 加行由导表器发号,禁止把 `RpcStatusCode` / `GameErrorCode` 写进 `TipInfoMessage.id`。
- 显式映射函数 `RpcStatus → tip` 一处定义;"调用结果 → 客户端"的落点与 gRPC 的 `gate_rpc_reply_bridge` 放在一起,保持 gate 唯一一份事实源。
- scene 侧仍回填 `session_id` 与 `message_content.id`(后者是客户端协议;前者在过渡期服务尚未迁移的旧 gate,收口后可删)。

量级(推断,须 Codex 微基准确认):请求类消息每次 CallMethod 约 0.4–0.7µs(InflightIndex 节点 + 堆项 + std::function;libstdc++ SBO 仅 16B 会多一次分配),
2–4 万次/s/gate 约占单核 1–3%,小于今天一次往返的开销一个数量级;移动类走单向,零新增。

### 8.2 断线代发 ExitGame、PlayerEnterGameNode

都走 `SendRequest`,线上不变。PlayerEnterGameNode 改用 `CallMethod` 拿"送达确认"可以把 `pending_scene_entry_comp.h:15-17` 登记的 I1 残余变成可观测——
这是进场欠账状态机 owner 的决策,本设计不擅改,列为后续。

### 8.3 握手(`registration_manager.cpp`)

`CallMethod`(`kNodeHandshakeCallTimeout`),lambda 捕获 `{nodeType, entity, weak_ptr<RpcClient>, weak_ptr<TcpConnection>}`,回调时先复核实体上仍是同一个 RpcClient。
成功 → `OnHandshakeReplied` + 成功钩子 `NodeHandshakeManager::SetHandshakeSucceededHook(std::function<void(uint32_t, const TcpConnectionPtr&, const NodeHandshakeResponse&)>)`
(gate/main.cpp 装 `gate_scene_entry::OnSceneLinkHandshaken`,engine/core 不反向依赖 gate)。`kTimeout` / `kRemote*` → 照旧 `RunAfter(0.5)` 重试(补上"应答丢失不重试");
`kDisconnected` / `kChannelClosed` / `kNotSent` 不重试(UP 会经 `OnConnected2TcpServerEvent` 重新握手,否则会握手两次)。lambda 不捕获 this。
顺带修 `OnSceneNodeHandshakeReply` 判空前解引用(`scene_response_handler.cpp:141`)。

### 8.4 B 的下游是 gRPC(本轮的过渡写法)

生成的 gRPC 客户端只有按方法的全局 handler。B 要挂起 `RpcDoneCallback`,只能自建 `关联号 → RpcDoneCallback` 表:成功按响应回显的关联号认领,
失败按 `GrpcCallFailure` 的请求副本认领;下游不回显关联号就不能挂起。后续批次给 `grpc_async_client` 模板加每次调用的 done 与 deadline(§14)。

## 9. 三个指定对象的去向

### 9.1 `handler/rpc`:保留

生成的 `*Handler` 类与守护段就是 muduo 的 Service 实现,保留。签名已带 `Closure* done`(`handler_gen.go:50,96-99`),无需改签名。
改动只有:框架必回、`done` 变得可用(§5)、更正两处守护段里"done 恒为 nullptr / GameChannel::CallMethod / game_channel.cpp:451"的过时注释
(`gate_service_handler.cpp` 与 `scene_admin_handler.cpp` 的 GmGracefulShutdown)以及守护段外的 `gate_security.h:121-122`、`SECURITY.md:47-48`。
handler 头文件模板(整文件生成)加一行类级注释说明"延后应答用 `RpcDoneCallback::Adopt(done)`";cpp 模板不加注释(会干扰守护段定位)。
不把 2f897bd07 的命名守护段套到 muduo handler 上(三个 cpp 的全部守护段都得改写,属于顺手重构)。

### 9.2 `rpc_replies`:muduo 回包整体退场

"按 message_id 的全局回包表"违反每次调用上下文的目标,且同号并发不可区分、注册重复会覆盖。退场清单:

- 迁走:`OnSceneProcessClientPlayerMessageReply` → §8.1 lambda;`OnSceneNodeHandshakeReply` → §8.3 lambda + 钩子。
- 删除:其余空壳(gate 的 `scene_response_handler.*` 余下 10 个、`scene_admin_response_handler.*`;scene 的 `gate_service_response_handler.*` 全部 10 个),
  `gRpcResponseDispatcher` / `MessageResponseDispatcher`,孤儿 `gate/rpc_replies/route_message_response_handler.cpp`(未注册,且以错误类型 `ProtobufDispatcher` extern 声明),
  过期生成物 `scene/rpc_replies/player/player_currency_response_handler.*`,`InitReply` 链接义务(`node.cpp` RegisterHandlers、battle 手写桩、测试桩)。
- **必须同时改生成器**:yaml 的 `enable_rpc_response` 没有读者(`config.go:247-248`),只能改 Go:`handlers.go`(删两项回包配置)、`gen.go`(删两处 `WriteRepliedRegisterFile`)、
  `register_gen.go`、`replied_handler_gen.go`(整删)、`handler.go` / `model.go` 的回包字段、两个 node util 谓词、`generator_filters_test.go`。否则下次生成会补默认守护段并重新注册(`code_parser.go:32-36`)。
- 保留:`scene_manager_response_handler.*` 等 gRPC 应答适配(手写、不走 RPC0);gate 的 `rpc_replies/register_handler.cpp`(生成的空 `InitServiceHandler`)。
  rpc_replies 目录此后只放 gRPC 应答适配。
- 玩家回包死链(`gPlayerServiceReplied` 全仓无读者、`scene/rpc_replies/player/` 5 个类 28 个方法、`InitPlayerServiceReplied` 义务、与 `rpc_event_registry.cpp` 同名不同义的 `XxxImpl` ODR 隐患)单独成批删除。
- 规范:AGENTS §8 与 copilot-instructions.md:213 的 `On<Domain><Method>Reply` 规则改为"节点间 RPC0 的回包在调用点 per-call lambda;rpc_replies 只放 gRPC 应答适配"(改 AGENTS 需用户同意)。

### 9.3 `GateHandler::RoutePlayerMessage` 一族:不改造,显式拒绝;proto 方法暂不删

范围:`Gate.RoutePlayerMessage`(74)、`Scene.RoutePlayerStringMsg`(81)、`Gate.RouteNodeMessage`(41)、`Scene.RouteNodeStringMsg`(78)。理由:

- 全仓无发起端(C++ 只有两个 handler 互相转发;Go / robot 只有生成物;Java 只有 proto 副本与文档);74 / 81 不在 `IsClientMessageId` 里,客户端不可达。
- 它是"源路由(node_list 前向弹栈)+ 逐跳同步镜像应答",应答在下游完成前就回给上游,且无错误字段,正是 A→B→C 的反例;outstanding 模型下每跳只记自己发出的那次调用,回程自然形成,node_list / route_nodes 失去意义。
- gate→gate / scene→scene 分支拓扑不可达;scene 本地命中只打 TRACE 不派发;gate 终点 O(N) 扫会话、无栅栏、把 body 里的 ClientRequest.id 当作 MessageContent.id 下发(可能被客户端当成某次请求的回包)。
- 权威设计已把跨节点玩家消息交给 Kafka 与持久收件箱(`cross_scene_player_messaging.md`、`reliable-player-inbox-20261014.md`)。

做法:四个 handler 守护段改为拒绝(`controller->SetFailed` + 限频 WARN;必回之后调用方得到 `kRemoteFailed`),不再转发、不再往客户端 socket 写 payload;
删除只服务它们的 `ResolveRoutedMessagePriority` / `GetMethodPriority` / `IsImportantPriority`(`scene_handler.cpp` 全局守护段),`RouteMessageToNode` / `SendRouteResponse` /
`HandleNodeRouteMessage` / `HandleClientRequestMessage` / `dispatcher_` / `GetDispatcher`,`RpcController` 的路由与会话字段,`node_message_utils.*` 与 `player_message_utils` 中零调用的封装;
afk.cpp 注释更正。proto 方法上加注释"deprecated:无发起端;删除前须先给 message_id 加墓碑"。

**为什么不现在删 proto 方法**:`InitMessageId` 把空号按 Go map 随机顺序回收给新方法(`service_register_info.go:170-213`),`message_id.txt` 不认墓碑
(event_id 有 `N=reserved:`,message_id 没有;单测还把"复用 ID 1"钉成期望,`service_register_info_test.go:58`)。message_id 是全仓一条线上数轴
(Java ContractSync 原样复制,robot / Unity / 9 个 Go 服务的 message_id.go 都从它生成,Kafka 留存的 MessageContent 也带号)。删方法要等
`client-access-band-routing.md` D52 / D54 的停服重新发号与墓碑发号器,或先给 message_id 补墓碑——须用户拍板项目是否已上线并授权客户端仓重生成。

## 10. 生成器:方法 tag(类型安全的调用面)

`service_id_gen.go` 只为 `CcGenericServices() && !IsPlayerService && !IsClientProtocolService` 的服务(即 Gate、Scene、SceneScene)在各自
`*_service_metadata.h` 追加平铺 tag:

```cpp
struct SceneProcessClientPlayerMessageRpc {
    static constexpr uint32_t kMessageId = SceneProcessClientPlayerMessageMessageId;
    using Request = ::ProcessClientPlayerMessageRequest;
    using Response = ::ProcessClientPlayerMessageResponse;
};
```

- 与 `<KeyName>MessageId` 同一命名族;不按数字特化 `Traits<Id>`(要公共主模板、报错只见数字、号被回收时特化悄悄指向别的方法)。
- 不占用 `<KeyName>Method` 名(那是现有宏,`bag_test/player_feature_handler_test.cpp:117-164` 在用);现有 Method 宏与 Index 常量不动。
- 生成头不 include core,保持纯类型信息;core 的模板入口按 tag 取类型。
- 模板写成 `internal/template/*.tmpl` + `go:embed`(2f897bd07 先例),配 golden 测试:scene.proto 产出 tag,gRPC 文件与玩家服务文件不产出。只新增头文件内容,不改构建清单。
- `registration_manager` 的运行期 `kNodeTypeToMessageId` 表只剩 Scene 一项可达,改为按 nodeType switch 成 typed 调用;无类型 `CallMethod(uint32_t, ...)` 保留为底层入口。

## 11. 兼容与升级(扩 / 缩两版)

gate 与 scene 按 zone 划分(`NodeUtils::IsZoneScopedNodeType`),RPC0 混跑只发生在 zone 内。k8s podip 形态 gate 与 scene 同批滚动、Agones 不等 Fleet,部署工具不保证顺序,
所以不用"先升 scene 再升 gate"的单版本口径,而用两版扩 / 缩:

| 版本 | 内容 | 与上一版混跑 |
|---|---|---|
| R1(扩) | 服务端:回显 id、id≠0 必回、HANDLER_FAILED / REPLY_DROPPED、`done` 可用;核心客户端机制就位但调用方仍发 id=0、旧回包表仍在 | 与 R0 双向兼容(调用方没变;新服务端对 id=0 保持旧规则) |
| R2(切) | 调用方改用 `CallMethod`(id≠0);rpc_replies 与全局回包表退场 | 要求同 zone 对端 ≥ R1。R2×R1 双向可用(R1 已回显 id)。R2×R0 **fail-closed**:握手回包无 id 被丢弃 → 握手超时重试、链路不就绪、gate 不把玩家交给该 scene;日志与 `legacy_id0_reply` 计数可见 |
| R3(缩,下一版本) | 删除服务端 id=0 旧回包分支(id=0 永不回);删 `RpcMethodMeta.handler` 与空 `XxxImpl` | 要求 ≥ R2 |

回滚按相反顺序。若用户选择"同 zone 停服一次切换"(项目未上线,D52 有先例),R1–R3 可合并发布,旧规则分支从不需要。
外部 GM 停机工具(`SceneScene.GmGracefulShutdown`,`SECURITY.md:37-48`,仓库内无发送方)在 R3 之后必须带非 0 id 才能收到应答;`affected_count == 0` 时 R1 起也会回包(改进)。

**双版本(AGENTS §12)**:节点间 RPC0 信封、调用关联、超时、断连语义是服务端内部实现,Java 版 gate↔scene 用 NodeLinkFrame、内部 RPC 用 Dubbo,**不适用**;
"scene 调用失败回带请求 id 的信封 1003"是客户端可见行为,Java 的 scene 路径今天推裸 tip,登记为"待做 / 有意差异"。两行 PARITY 登记需用户同意跨仓写入。

## 12. 可观测性

C++ 节点没有 Prometheus 出口(`dirty_save_stats.h:25-30`),沿用"relaxed 计数 + 30s 汇总行 + stress_summarize 解析"口径:

- `RpcChannelStats`(进程级 relaxed atomic + `Snapshot`,写法照 `exit_persist_stats`):started、ok、remote_rejected、remote_failed、reply_dropped、timeout、disconnected、
  channel_closed、invalid_response、late_reply、legacy_id0_reply、not_sent_{not_connected,closed,overload,invalid}、oneway_sent、oneway_rejected、adopted、reply_run_twice、
  reply_channel_gone、in_flight、max_in_flight、max_sweep_lateness_ms;gate 另有 reply_session_gone。只用低基数维度(可附超时最多的 3 个 message_id),**绝不**用 player_id / session_id。
- 30s 定时器只在计数变化时打一行 `[RpcChannel] summary k=v ...`(注册在 `RegisterTrafficStatsReporter` 旁);格式化函数配单测锁定字段名,作为与 ps1 正则的契约;
  `stress_summarize.ps1` 增加一段(同时 glob `bin\logs\cpp_nodes` 与 `run\logs\cpp_nodes`)。
- 日志:迟到 / 未知 id、本地拒绝、REPLY_DROPPED、重复 Run 走限频器(复用 `FailureLogThrottle`,`infra/messaging/kafka/kafka_producer_policy.h:83-150`,core 已依赖 infra);
  断连每次一行(失败数 + 最老在途时长 + 最多的 message_id);每轮超时扫描一行(限频);id=0 旧回包每进程一次(照 `LogFenceUnfencedOnce`)。
  `rpc_client.h` / `rpc_session.h` / `LockConnection` 的逐条日志改为计数 + 限频;发送前先查 `conn->connected()`,避开 muduo 在已断连接上逐条 WARN。
  `error_reporter::Record` 只在真正打出日志的那一条上调用(它每次加锁 + 两份字符串分配)。

## 13. 分批(每批独立授权;动手前重新核对并发会话改动)

| 批 | 内容 | 行为变化 | 手改文件(估) |
|---|---|---|---|
| 1 清理 | §9.3 的拒绝与死代码删除;`IsDispatchableMessageId` 修空槽崩溃(+ `scene_handler` 的 `gPlayerService.find` 判空);删 `game_rpc.proto` 与 core.vcxproj 三个失效 None 项;过期 `player_currency_response_handler.*`;孤儿 `route_message_response_handler.cpp`;回归测试 | Route 一族不再转发;空槽消息号回 RPC_ERROR 不再崩 | ~20 |
| 2 改名 | `GameChannel` → `RpcChannel`、文件改名、方法改名;core 构建三件套;两个测试;注释(gate_security.h、SECURITY.md、两处守护段);AGENTS §11.7 / copilot-instructions 的引用名(需同意) | 无 | ~14 |
| 3 核心(R1) | rpc_message.proto(+ 8 份生成物由 Codex 生成);`RpcChannel`(CallMethod / SendRequest / 超时 / OnDisconnect / Close / DoneClosure / 必回 / RPC_ERROR 回显);新增 `rpc_status.h`、`rpc_done_callback.h`、`rpc_channel_stats.{h,cpp}`;`InflightIndex` 上移 + gate 别名;`RpcClient` 每连接 channel;`RpcServer` weak 绑定;`RpcSession` weak channel;`node.{h,cpp}` 两段式 Finalize + `CloseAllRpcChannels`;构建三件套;三个新测试 TU + 夹具裸指针修正;文档更正注 | 服务端必回、回显 id;调用方不变 | ~30 |
| 4 生成器 tag | `service_id_gen.go` + tmpl + golden 测试;3 份 metadata 头由 Codex 生成 | 无 | ~5 |
| 5 切换(R2) | §8 全部调用点;gate 回包桥与 `SendEnvelope` / `FindLiveSession` 导出;握手钩子;删 `CallRemoteMethod` 与全局回包表;生成器停产 muduo 回包(§9.2);删生成回包文件;`InitReply` 义务;三节点 vcxproj / filters / CMakeLists;AGENTS §8 / copilot-instructions(需同意);stress_summarize 汇总段 | 调用方切到 id≠0 | ~45(多为删除) |
| 6 玩家回包死链 | §9.2 末条 | 无 | ~30(多为删除) |
| 7 收口(R3) | 删 id=0 旧规则分支;`RpcMethodMeta.handler` / `XxxImpl`;proto 方法删除(待墓碑与用户拍板) | id=0 永不回 | 待定 |

批 1、2、4 互不依赖可先行;3 依赖 2;5 依赖 3、4;6 可在 5 之后任意时间;7 在 R2 全量上线之后。

## 14. 后续(本轮不做,需各自 owner 同意)

- 玩家级 handler 延后完成(`player_service_gen.go` / `PlayerService::CallMethod` 加调用上下文)。
- `grpc_async_client` 模板加每次调用的 done 与 deadline,让 B 的 gRPC 下游也能直接挂 `RpcDoneCallback`。
- `muduo_grpc::Channel` 的在途表迁到同一份 `InflightIndex`;其 `clear<MuduoGrpcChannelPtr>` 挪进关机阶段 0。
- PlayerEnterGameNode 改 `CallMethod` 拿送达确认(I1 残余),由进场欠账 owner 决定。
- `client-request-ordering.md` 登记不变式 I5。
- 跨节点 trace(traceparent)与跨跳剩余预算(届时加 7 号字段:发送时刻剩余预算,接收方从收包单调时刻起算)。
- Finalize 之后 acceptor 仍接新连接的既有问题。

## 15. 测试(批 3 起;放进 `cpp/tests/rpc_controller_test`,新增 TU 登记进其 vcxproj,不写 main、不重复定义 InitReply 等桩)

- **A `inflight_index_test.cpp`(纯逻辑,合成时间)**:Insert 拒 0 与重复;Extract 恰好一次;ExtractExpired 按截止先后、跳过已完成;不同超时混排;ExtractAll 后 Size 为 0。
- **B `rpc_channel_loopback_test.cpp`(真实回环)**:全局 Environment 只调一次 `InitMessageInfo()`;对端装 PeerRecorder(用 RpcCodec 解码记录 type / id / message_id);计数断言比较前后差值。
  正常往返恰好一次且 done 运行时 CallMethod 已返回;同 message_id 并发两次倒序回包各得其所;超时(`SweepTimeouts(t1+T)`)后同 id 回包被丢弃且 late_reply +1;未知 id 与 id=0 回包丢弃计数;
  RPC_ERROR 映射;服务端拒绝回显 id;回包无法解析 → kInvalidResponse;id=0 兼容(用"屏障请求"证明没有回包:同连接 FIFO,下一帧必是屏障的回包,不用时间窗);
  id≠0 打到 Empty handler / handler 早返回也必回;SetFailed → kRemoteFailed;断连时 N 个在途在 OnDisconnect 栈内回调数为 0、PumpLoop 后各 kDisconnected 一次,重连后新 channel 的 id 从 1 重新开始(每连接一个 channel,§6.1;旧连接的回包落不到新连接上);
  销毁最后一个 RpcClientPtr 后 channel 的 weak 立即 expired(定时器无强引用)、各 kChannelClosed 一次、对端再回包不进回调;done 内重入 CallMethod 与销毁 owner(三条完成路径各测);
  本地失败经队列投递、CallMethod 返回前计数为 0;非 loop 线程调用 EXPECT_DEATH;Adopt 后连接引用数不变;RpcDoneCallback:只 Adopt 不 Run → 线上 REPLY_DROPPED 同 id、Run 两次只一帧、
  同步 Run 后框架不再回、channel 销毁后 Run 不崩不出帧、拷贝共享状态兜底只触发一次;loop 析构时被丢弃闭包里的 done / State 析构不碰 loop;Close 的失败回调先于随后排入的标记 functor(对应关机阶段 0)。
- **C `rpc_chain_test.cpp`(A→B→C)**:一个 EventLoopThread、两个 RpcServer(B、C)、两个 RpcClient(A→B、B→C)。C 的 handler Adopt 后把 reply 存进测试存储;
  A.CallMethod → 等 C 被调用 → `RunInLoopAndWait(marker)` 能返回(证明 loop 未阻塞)且 A / B 回调计数为 0 → 在 loop 上 `cReply.Run()` → 等 A 完成;断言事件序列
  `[C.handler, B.cont(kOk), A.done(kOk)]`、A 的响应同时带 C 的载荷与 B 写的字段、A 捕获的上下文不变。变体:B→C 超时 / C 断连时 B 仍 Run 并填业务码;B 忘了 Run 时 A 立即 kReplyDropped。
  拆除顺序:先 reset 两个 RpcClient、PumpLoop、等两端 DOWN,再在 loop 线程销毁两个 RpcServer。所有等待只用 `wait_for(5s)` 作活性上限,不拿时间窗当判据(AGENTS §11.4)。
- 夹具裸成员 `EventLoop* loop_` 同批改为 `std::optional<std::reference_wrapper<EventLoop>>`(裸指针门禁覆盖测试工程,`Directory.Build.targets:50-53`)。
- 微基准 `DISABLED_RpcChannelBench.*`(回环往返 1e5 次 ns/op、表开销),由 Codex 用 `--gtest_also_run_disabled_tests` 在改动前后各跑一次出对比表,Linux 镜像另跑一份。
  按 AGENTS §6,没有对比表不声称"无性能回退";现有压测机器人不移动,测不到逐条消息开销。

## 16. 给 Codex 的验证步骤(每批交付时按批裁剪)

工作目录 `D:\luyuan\xuanming-server-mmo-cpp`,PowerShell,证据放仓库外 `$ev = "$env:TEMP\codex-rpc-channel-<ts>"`。

0. 前置与基线:`git status --short > $ev\status-before.txt`;`Test-Path` 检查 `third_party\boost\boost\any.hpp`、`third_party\grpc\install_vs2026_dbg\lib`、
   googletest 头文件,任一缺失就停下报告(不装工具、不跑 b2,§10.2);`Get-Process MSBuild,cl` 必须为空;`dev_tools.ps1 -Command dev-stop` 释放 bin。
1. 只在改了 proto 的批次:`pwsh -File tools\scripts\dev_tools.ps1 -Command proto-gen-build`;把 `proto_gen.yaml` 的 `enable_unity_client` 改成 false 另存到 `$ev`;
   `proto-gen-run -UseBinary -ConfigPath <副本>`。通过标准:`rpc_message.pb.h` 有 `kIdFieldNumber = 6`;`message_id.txt` 与 `kMaxRpcMethodCount = 253` 不变;
   `git -C ..\mmorpg-client status` 不变;新增 diff 只有本批预期的生成物。随后 robot 下 `go mod vendor`;`$env:GOTOOLCHAIN='local'`,`go\proto` 下 `go build ./...`,robot 下 `go build -mod=vendor ./...`。不要跑 `go\build.bat`(只做 goctl,末尾还有 pause)。
2. 库:严格串行 `/m:1 /p:Configuration=Debug /p:Platform=x64`,按 proto → rpc → proto_helpers → config → infra → session → thread_context → grpc_client → core → table → modules →
   services\battle → services\scene → services\gate,任一失败即停,日志逐个落到 `$ev`。`RpcClient` 全在头文件内联,core、服务库、节点、测试必须在同一份头文件状态下重编。
3. 节点:gate、scene 必编;battle 的 `boost/any.hpp` C1083 是既有阻塞,单列,不算到本改动头上。
4. 只有 vcxproj 的 ClCompile 变了才跑 `py -3 tools\archived\vcxproj2cmake.py`;与基线比对只保留本批相关的 CMakeLists diff,其余如实报告、不回退。
5. 测试:`rpc_controller_test.vcxproj` `/t:Rebuild`;`.\build\cpp\tests\rpc_controller_test.exe --gtest_color=no > $ev\gtest.txt 2>&1`(重定向到文件);
   `--gtest_filter=InflightIndex*:RpcChannel*:RpcChain* --gtest_shuffle --gtest_repeat=50 > $ev\gtest-repeat.txt 2>&1`;再 `run_cpp_tests.ps1 -Filter rpc_controller`。
   通过标准:退出码 0、无 `[  FAILED  ]`、重复 50 次 0 失败。批 4 / 5 另跑 protogen 的 `go test -mod=readonly ./internal/... ./cmd` 与 `go vet`(Go 1.27.2,`GOTOOLCHAIN=local`)。
6. 裸指针检查若打印 SKIP,如实记 SKIP,不当作通过。
7. 冒烟(批 3 起):起服跑 `robot -c etc/travel_smoke.yaml`(会发 MoveStart)与 robot_smoke;gate / scene 日志的 `[RpcChannel] summary` 应 timeout=0、late_reply=0、legacy_id0_reply=0,
   `[RpcChannel]` 总行数 ≤ 运行分钟数 × 2 + 限频突发上限。
8. RED 证明用 `git worktree add` 隔离目录,**不要** `git stash`(共享工作区会收走别的会话的未提交改动)。

## 17. 待用户拍板

1. 授权按 §13 分批实施(多批各自接近或超过 30 个文件)。
2. 升级口径:§11 的扩 / 缩两版,还是同 zone 停服一次切换。
3. 改 `rpc_message.proto`(加 id、两个错误码、枚举 reserved)连带重生成 Go / robot 的 `.pb.go`,是否接受为"只改 C++"的必要代价。
4. 改 AGENTS.md §8 / §11.7 的规则与引用名。
5. RoutePlayerMessage 一族 proto 方法何时删、message_id 是否先做墓碑。
6. 在 Java 仓 `PARITY.md` 登记两行(跨仓写入)。

## 18. 实施记录(2026-10-15,批 1–5,未编译)

### 18.1 与上文设计的偏离

1. **升级口径**:用户选「同 zone 停服一次切换」,所以 §4.1 / §11 的过渡期垫片没有实现:id == 0 的请求服务端永不回包;
   收到 id == 0 的 RESPONSE / RPC_ERROR 计 `legacy_id0_reply` 后丢弃;R1–R3 合并为一次发布。新旧二进制混跑时握手应答无 id 被丢弃 →
   握手 3s 超时重试、链路不就绪、gate 不把玩家交给该 scene(fail-closed),日志可见。
2. **日志限频**:没有复用 `FailureLogThrottle`(它所在头文件 include `rdkafkacpp.h`,不宜拖进 RPC 层),改用 `ShouldLogRpcEvent`
   采样(前 8 条逐条,之后每 1024 条一条),与 `codec.cpp` 的 1/1024 先例同口径;准确数量看 `[RpcChannel] summary`。
3. **全局 tip**:并行的 ECS 整理已移除进程级全局 TipInfoMessage,§5.2 原先「Run() 检查并清理全局 tip」的兜底随之删除(现文以 §5.2 为准)。
4. **关机**:`clear<MuduoGrpcChannelPtr>` 经 muduo_grpc 作者同意挪进阶段 0;另一会话随后在阶段 0 加入官方 gRPC CQ 的关闭与排空
   (`DrainGrpcClientsInLoop`),拆除 `FinalizeTeardownInLoop` 仍经 `queueInLoop` 排在全部失败回调之后,§6.5 的 FIFO 论证不变。
5. **计数**:服务端 handler 显式失败单独计 `handler_failed`,与派发前拒绝 `requests_rejected` 分开。
6. **`SendRequest` 不看 closed**:关机阶段 0 之后单向推送仍可发出(拆除期间给客户端的推送不受影响);关闭只拒新的 `CallMethod`。
7. **PlayerEnterGameNode**:仍是单向;`SendRequest` 返回 false(本地没发出)时按 `kNotConnected` 处理,修掉以前被当成 `kSent` 清掉欠账的假成功。
8. **可观测性收尾**:`stress_summarize.ps1` 新增「RPC0 call outcomes」段,扫 `binogsp_nodes` 与 `runogsp_nodes` 下 gate / scene 日志的最后一行
   `[RpcChannel] summary`;微基准只写了用例,数据待 Codex。

### 18.2 已知行为变化(上线前需知)

- gate 对**内层 Empty** 的客户端消息(移动类)单向发送:scene 对这类消息用 `SendErrorToClient` 填的错误应答(如玩家未载入)不再回到客户端,只在 scene 日志可见。
- 内层非 Empty 的消息,scene 调用失败(12s 超时 / 断连 / 未发出 / 对端拒绝)时客户端收到带请求 id 的 1003;以前没有回执,只能等客户端自己的 15s 超时。
- 握手应答丢失 3s 后重试(以前要等重连);对端自报身份与本端实体不一致时拒绝这次握手。
- 外部 GM 工具调用 `GmGracefulShutdown` 必须带非 0 的调用 id 才能收到应答(`SECURITY.md`)。
- Route 一族四个方法对调用方返回 `kRemoteFailed`(全仓无调用方)。

### 18.3 改动清单

- core network:`rpc_channel.{h,cpp}`(由 `game_channel.*` 改名)、新增 `rpc_status.h`、`rpc_done_callback.h`、`rpc_channel_stats.{h,cpp}`;
  改 `rpc_client.h`、`rpc_session.h`、`rpc_server.{h,cc}`、`rpc_controller.h`、`player_message_utils.{h,cpp}`;
  删 `node_message_utils.{h,cpp}`、`codec/message_response_dispatcher.h`、`game_rpc.proto`。
- core 其它:新增 `utils/data_structures/inflight_index.h`;改 `node/system/registration/registration_manager.{h,cpp}`、`node/system/node/node.{h,cpp}`、
  `node_util.{h,cpp}`、`core.vcxproj` / `.filters` / `CMakeLists.txt`。proto:`proto/common/base/rpc_message.proto`。
- gate:`main.cpp`(握手成功钩子)、`handler/rpc/client_message_processor.cpp`、`client_request_lane.{h,cpp}`(导出 `SendEnvelope` / `FindLiveSession`)、
  `handler/rpc/gate_service_handler.cpp`、`handler/event/scene_entry_dispatch.{h,cpp}`、`gate_security.h`、`SECURITY.md`、删 `rpc_replies/` 6 个文件、
  `gate.vcxproj` / `.filters` / `CMakeLists.txt`;`libs/services/gate/session/comp/router_lane.h`、`pending_scene_entry_comp.h`;`nodes/gate/tests/router_lane_test.cpp`(注释与编译命令)。
- scene:`handler/rpc/scene_handler.cpp`、`scene_admin_handler.cpp`、`main.cpp`、`id_segment_bootstrap.{h,cpp}`(注释)、删 `rpc_replies/` 5 个文件、
  `scene.vcxproj` / `.filters` / `CMakeLists.txt`;`libs/services/scene/player/system/player_scene.cpp`、`afk.cpp`(注释)。
- battle:删 `rpc_replies/register_response_handler.cpp`,`battle.vcxproj` / `.filters` / `CMakeLists.txt`。
- 生成器 `tools/proto_generator/protogen/internal/`:`service_id_gen.go`、`service_id_gen_test.go`(新)、`template/service_metadata.h.tmpl`(新)、`handler_gen.go`、
  `register_gen.go`、`replied_handler_gen.go`;`generator/cpp/` 下 `handlers.go`、`gen.go`、`generator_scene_node_util.go`、`generator_gate_node_util.go`、
  `generator_filters_test.go`、`node_rpc_replies_test.go`(新)。
- 测试 `cpp/tests/rpc_controller_test/`:新增 `inflight_index_test.cpp`、`rpc_channel_loopback_test.cpp`、`rpc_chain_test.cpp`(含 `DISABLED_` 微基准);
  改 `rpc_controller_test.cpp`、`rpc_client_lifecycle_test.cpp`(删 InitReply 桩、按每连接 channel 改写断言)、`hiredis_command_lifecycle_test.cpp`、
  `redis_manager_auth_test.cpp`(夹具裸指针成员改 `optional<reference_wrapper>`)、`rpc_controller_test.vcxproj`(登记三个新 TU)。
- 规范与文档:`AGENTS.md` §8 / §11.7、`.github/copilot-instructions.md`、`docs/design/battle-transport-decision.md` §3.2 更正注、`docs/README.md`、
  `tools/scripts/gen_docs_index.py`;Java 仓 `PARITY.md` 两行。

### 18.4 交给 Codex 的验证顺序(在 §16 的基础上)

1. 生成器源码已改:先 `proto-gen-build`,再 `proto-gen-run -UseBinary -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`(AGENTS §4.1)。
   通过标准:`rpc_message.pb.h` 有 `kIdFieldNumber = 6` 与 `HANDLER_FAILED` / `REPLY_DROPPED`;`scene` / `gate_service` / `scene_admin` 三个 metadata 头末尾共多 24 个
   `<KeyName>Rpc` tag;三个节点级 handler 头各多一行类注释;已删除的节点级回包文件没有被重新生成;`message_id.txt` 不变;再跑一次无新 diff。
2. protogen:`$env:GOTOOLCHAIN='local'`,`gofmt -l ./internal ./cmd` 无输出、`go vet ./internal/... ./cmd`、`go test -mod=readonly ./internal/... ./cmd`。
3. robot 下 `go mod vendor`(刷新 vendor 里的 `rpc_message.pb.go`)。
4. C++ 按 §16 第 2–3 步串行 `/m:1`;battle 的 `boost/any.hpp` C1083 是既有阻塞,单列。
5. `py -3 tools\archived\vcxproj2cmake.py` 核对 core / gate / scene / battle 的 CMakeLists 只有本批的增删。
6. 测试:`rpc_controller_test` Rebuild 后全量与 `--gtest_filter=InflightIndex*:RpcChannel*:RpcChain* --gtest_shuffle --gtest_repeat=50`;
   `routing_identity`、`gate_security`;router_lane 独立 g++ 编译(命令已加 `-I cpp/libs/engine/core`)。
7. 冒烟:`travel_smoke` 与 robot_smoke,gate / scene 日志里握手成功、`[RpcChannel] summary` 的 timeout / late_reply / legacy_id0_reply 为 0、
   `Rejected deprecated RPC` 为 0、客户端请求类消息有回包。
8. 微基准 `DISABLED_RpcChannelBench.*` 改动前后各跑一次出对比表;没有对比表不声称「无性能回退」(AGENTS §6)。

### 18.5 遗留

- 已处理:`docs/design/ecs-entt-misuse-audit-20261015.md` 的 C007 / F093 状态与 `routing_identity_test.cpp` 的注释已更正(entt 审计会话已结束,由本会话代改)。
- ~~GM 停机鉴权被拒时回的是正常 RESPONSE(`affected_count = 0`),是否改为 `SetFailed` 待定。~~ 已改,见 §19.3。
- §14 的后续项:gRPC 每次调用回调已做(§19.2),其余不变。

## 19. 实施记录(续,2026-10-15,用户授权「全部做完」,未编译)

### 19.1 批 6:玩家回包死链

`PlayerServiceReplied` / `gPlayerServiceReplied` 从无读者(`ProcessClientPlayerMessage` 只查 `gPlayerService`),整条删除:

- 删 `cpp/libs/services/player/rpc/player_rpc_response_handler.h`;scene `rpc_replies/player/` 9 个文件;gate、battle 的
  `rpc_replies/player/player_rpc_response_handler.cpp`。四个工程的 vcxproj / filters / CMakeLists 同步去掉。
- `Node::RegisterHandlers` 不再调 `InitPlayerServiceReplied`,节点二进制的链接义务从三个降为两个(`InitPlayerService` / `InitServiceHandler`);
  `rpc_controller_test` 的桩同步删除。
- 生成器:删 `replied_handler_gen.go`、`template/player_service_replied_instance.cpp.tmpl`、`IsSceneNodeReceivedPlayerResponseHandler`、
  `HandlerConfig.IsRepliedHandler`、`MethodInfo.CppRepliedHandler*`、`generateRepliedInstanceData`;`config.go` 去掉只服务于回包链的 10 个字段,
  两份 `etc/proto_gen*.yaml` 去掉对应键(yaml 未知键本就被忽略,`gate_node_replied` 保留:gate 的空 `InitServiceHandler` 仍生成在 `rpc_replies/`)。
- 生成器测试:`TestPlayerReplyChainStillGenerated` 改为 `TestPlayerServiceGeneratesNoReplyChain`(整棵输出树不得出现 `*_response_handler.*` 或
  `PlayerServiceReplied`);删 `TestSceneNodeReceivedPlayerResponseFilter`。
- 生成器从不删除过期文件,所以本批的删除必须随源码一起提交;regen 后不应重新出现上述文件。

### 19.2 gRPC 生成客户端的每次调用回调(§14 第 2 项)

见 `docs/design/grpc-client-deadline-failure-callback.md` §11:每个 unary 方法多一个
`Send<Svc><Method>(registry, entity, req, GrpcDone<Resp>, GrpcCallOptions = {})`,`done` 恰好一次、从不同步、不触发全局处理器;
既有 API 全部保留。模板与 `grpc_call_tag.h` 已随用户快照提交并 regen;`grpc_client_lifecycle_test.cpp` 补 `PerCallDone*` 三条用例。
§8.4「关联号 → RpcDoneCallback」过渡写法从此不再必要:B 的 gRPC 下游直接把 `RpcDoneCallback` 捕获进 `GrpcDone`。

### 19.3 GM 停机鉴权被拒返回失败

`GateHandler::GmGracefulShutdown` 与 `SceneSceneHandler::GmGracefulShutdown` 的拒绝分支调 `controller->SetFailed`,调用方得到
`RPC_ERROR(HANDLER_FAILED)`(`kRemoteFailed`),与「鉴权通过但没有可关的对象」(`RESPONSE`,`affected_count == 0`)可直接区分;
线上只带错误码,拒绝原因只在本地日志。`cpp/nodes/gate/SECURITY.md` 已同步。

## 附录 A:主要事实证据

- 信封与派发:`proto/common/base/rpc_message.proto:4-31`;`game_channel.cpp:97,280-305,342-379,386-409,440,454-495,531-552`。
- 拓扑:`gate/main.cpp:217-219,353-391`;`scene/main.cpp:85-87`;`battle/main.cpp:283-290`;`node_connector.cpp:122,129-144`;`rpc_server.cc:64-81`;`rpc_session.h:16,65`。
- 生命周期:`rpc_client.h:23-30,73-89,159-198`;`node.cpp:1137-1236`;`node_entry.h:157-178`;`muduo_windows/src/muduo/net/EventLoop.cc:100-113,143-153`;
  `TcpServer.cc:39-51`;`TcpConnection.cc:92-110,239-247,334-345`;`TcpClient.cc:73-102`;`ProtobufCodecLite.cc:104-139`。
- 定时器:`third_party/muduo/muduo/net/TimerQueue.cc:180-198`;`muduo_windows/src/muduo/net/TimerQueue.cc:200-221`;`Timestamp.cc:55-61`;`docs/design/muduo-timer-cancellation-hazards.md`。
- 在途表先例:`cpp/libs/services/gate/session/comp/router_lane.h:126-214`;`cpp/libs/engine/muduo_grpc/channel.h:5-12`。
- 生成器:`handler_gen.go:50,96-99,143-145`;`replied_handler_gen.go:145,151-159`;`code_parser.go:32-36`;`service_register_info.go:102-213`;`config.go:247-248`;`service_id_gen.go:23-25`。
- 路由一族:`gate_service_handler.cpp:140-221`;`scene_handler.cpp` RoutePlayerStringMsg 与全局守护段 `:46-94`;`proto/message_id.txt:42,75,79,82`。
- muduo 原版:`third_party/muduo/muduo/net/protorpc/{RpcChannel.h,RpcChannel.cc,RpcServer.cc,rpc.proto,README}`;
  github.com/chenshuo/muduo `examples/protobuf/{rpc,rpcbalancer,resolver}`;github.com/chenshuo/muduo-protorpc(cpp11 分支)。
