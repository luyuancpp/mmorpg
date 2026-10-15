# C++ gRPC 传输改由 muduo 驱动(nghttp2 + 自有 gRPC 调用语义)

- 日期:2026-10-14
- 状态:决策已定;第 1 批代码已落(**未编译、未测试,待 Codex 验证**,步骤见 §11)
- 范围:C++ 节点(gate / scene / battle)作为 gRPC 客户端与服务端的传输层。Go / Java 服务与线上协议不变
- 授权:负责人 2026-10-14 拍板「分 5 批推进、本次做第 1 批」「引入 nghttp2 v1.70.0 子模块」「etcd 走原生 gRPC 接口」
- 关联:[client-rpc-router.md](client-rpc-router.md)(第 1 批接入点)、
  [grpc-client-deadline-failure-callback.md](grpc-client-deadline-failure-callback.md)(deadline 与失败回调契约)、
  [scene-grpc-server-design.md](scene-grpc-server-design.md)(第 4 批替换对象)、
  [thread-count-monitoring.md](thread-count-monitoring.md)(现有 gRPC 线程构成)、
  AGENTS.md §11.7(连接与回调生命周期)

## 0. 一句话

**线上仍然是标准 gRPC(HTTP/2 h2c)**,Go 服务一行不改;C++ 侧把官方 gRPC C++ 运行时(EventEngine 线程池、
sync server poller、CompletionQueue 与 5ms 轮询)换成:muduo `TcpClient` / `TcpServer` 收发字节 →
nghttp2 做 HTTP/2 编解码 → 自有模块 `muduo_grpc` 实现 gRPC 调用语义 → 官方 Protobuf 序列化。
网络、定时器、完成回调全部在节点自己的 EventLoop 上执行,新模块**不创建任何线程、线程池或 CQ**。

## 1. 目标与非目标

目标:

1. C++ 节点的 gRPC 出站、入站都由节点 EventLoop 驱动;业务回调与今天一样只在 loop 线程上执行。
2. 线上字节与 Go(grpc-go v1.79.3 + go-zero)完全兼容:请求头、5 字节消息前缀、trailers、状态码、
   `-bin` 头、`grpc-timeout`、GOAWAY / PING 行为。
3. 迁移期新旧实现并存,按开关在启动时选定;**一次调用只走一种实现、只发一次**。
4. 最终移除官方 gRPC C++ 的运行时、`grpc_cpp_plugin` 产物与 28 个工程的 grpc 链接项。

非目标:

- 不改 `.proto`、不改 Go / Java 服务、不改客户端契约(AGENTS §12:Java 版不受影响,见 §12)。
- 不做 TLS(现网 C++ 与 Go 之间、与 etcd 之间都是明文 h2c;muduo 本身也没有 TLS)。
- 不做压缩、不做 retry policy / service config / 负载均衡策略(现网 C++ 一样都没开,见 §4)。

## 2. 选型

| 方案 | 结论 | 理由 |
|---|---|---|
| **nghttp2 核心库 + 自写 gRPC 语义(采用)** | ✅ | nghttp2 是 C 语言 HTTP/2 编解码的事实标准(curl、Apache httpd mod_http2、Node.js 内置),设计上**不做 I/O**:`nghttp2_session_mem_recv2` 吃字节、`nghttp2_session_mem_send2` 吐字节,天然由外部 EventLoop 驱动。gRPC over HTTP/2 的规则只有一份公开规范([PROTOCOL-HTTP2](https://grpc.github.io/grpc/core/md_doc__p_r_o_t_o_c_o_l-_h_t_t_p2.html)),Envoy 的 gRPC 客户端就是这个组合。MIT 许可。 |
| 官方 gRPC + 自定义 EventEngine | ❌ | EventEngine 仍是 `experimental` 接口、随版本变;要实现 Endpoint / Listener / DNS / 定时器整套语义;运行时、CQ 与构建依赖都还在,与「移除官方运行时」的目标相反。 |
| Boost.Beast | ❌ | 最新稳定版仍无 HTTP/2。 |
| cpp-httplib | ❌ | HTTP/1.1、阻塞 I/O,不能进 EventLoop,也连不了 gRPC。 |
| brpc 的 h2/gRPC 协议 | ❌ | 自带 bthread 调度,与 muduo 单 loop 模型冲突。 |
| Thrift / 自定义 TCP | ❌ | 改协议,Go 侧要跟着改,违背「线上协议不变」。 |
| etcd 走 HTTP/JSON 网关 | ❌(负责人 10-14 定) | 要另写流式 HTTP/1.1 客户端与 int64 / bytes 的 JSON 适配,等于多维护一套协议;原生 gRPC 接口与其他调用共用同一套传输。 |

依赖:`third_party/nghttp2` 子模块,钉 **v1.70.0**(2026-07-29 发布;annotated tag 对象 `b69da4c2656b…`,指向提交
`85e300c79fb6…`,与 GitHub API 核对一致;v1.68.1 修过 CVE-2026-27135,本版本已含)。只编静态核心库
(`ENABLE_LIB_ONLY=ON`、`BUILD_SHARED_LIBS=OFF`、`BUILD_STATIC_LIBS=ON`),不编 nghttp / nghttpx / h2load,
不引入 mruby、neverbleed 等嵌套子模块。构建方式与 grpc / hiredis / yaml-cpp 一致(上游 CMake),见 §9。

## 3. 分层

```
业务(gate SendViaRouter / 将来的生成客户端与服务端 handler)
        │  Channel::StartUnaryCall(method, 请求字节, metadata, 截止时间, 完成回调)
        ▼
muduo_grpc::Channel ── 连接状态机 / 排队 / 截止时间 / 透明重试 / 恰好一次完成
        │ 持有 1 个 READY 连接 + 若干 draining 连接
        ▼
muduo_grpc::Http2Transport ── 一条 TCP 连接:muduo TcpClient(按值持有)+ 握手超时 + 存活探测
        │ 字节进出
        ▼
muduo_grpc::ClientSession ── 纯协议引擎(无 I/O、无定时器):nghttp2 client session、请求头、
        │                       5 字节前缀、headers / trailers / RST / GOAWAY → gRPC Status
        ▼
nghttp2(HTTP/2 帧、HPACK、流控、SETTINGS、PING ACK)
```

- `ClientSession` 不碰 socket 与定时器,单测里与一个内存中的 nghttp2 服务端会话对接,完全确定性;
  `Http2Transport` 只是把它接到 muduo 上的薄适配器。
- 公共头文件不包含 `nghttp2.h`;使用方(gate)只需 `muduo_grpc` 的包含目录与两个 lib。
- 工具层:`status`(状态码与 HTTP / RST_STREAM 映射)、`metadata`(键校验、`-bin` 的 base64、`grpc-message`
  百分号编码)、`grpc_timeout`、`message_framing`(5 字节前缀的拼装与增量拆包)。

## 4. 线上契约(必须逐字节保持)

全部来自 2026-10-14 对现网 C++(grpc 1.83.0)与 Go(grpc-go v1.79.3、go-zero v1.9.2 / v1.10.0)源码的核对。

### 4.1 请求

- h2c prior-knowledge:TCP 建好立即发 24 字节 magic + SETTINGS(nghttp2 自动发 magic)。不走 HTTP/1.1 Upgrade。
- 头部顺序:`:method POST`、`:scheme http`、`:path /<pkg>.<Service>/<Method>`(无 package 时 `/<Service>/<Method>`,
  如 `/BattleNode/CreateBattle`)、`:authority <ip:port>`、`te: trailers`、`grpc-timeout`、
  `content-type: application/grpc`、`user-agent`、自定义 metadata。键一律小写,不得带 `connection` 头。
- 不发 `grpc-encoding` / `grpc-accept-encoding`:Go 只在请求带了已注册编码时才压缩响应,不发就永远不压缩。
- `grpc-timeout`:≤8 位十进制 + 单位(H/M/S/m/u/n),取能放下的最细单位并向上取整(与 grpc-go `encodeDuration` 一致,
  例:800ms → `800000u`,10s → `10000000u`,12s → `12000000u`)。超过 8 位 Go 会以 400 + Internal 拒绝整个请求。
- 消息:1 字节压缩标志(恒 0)+ 4 字节大端长度 + protobuf;空消息也有 5 字节前缀。请求上限默认 4 MiB(Go 服务端
  `maxReceiveMessageSize` 默认值),超出本地直接 `RESOURCE_EXHAUSTED`,不发出。

### 4.2 metadata 的两层 base64(不要「优化」)

```
SessionDetails 原始字节 raw
 → 应用层值 B = base64_std_padded(raw)          // 生成客户端 grpc_async_client.cpp.tmpl:155 的 Base64Encode(boost beast)
 → 线上头值   = base64_std_unpadded(B)            // 因为键以 -bin 结尾;官方 grpc C++ 与 grpc-go 发送都不带 padding
 → Go 应用层拿到 B → 业务 base64.StdEncoding 解出 raw
回程:路由服 grpc.SetHeader(B)(初始 metadata,不是 trailer)→ 线上 RawStd(B) → 传输层解一层得 B → gate 再解一层得 raw
```

- 传输层对 `-bin` 键**只加 / 解一层**;解码同时接受有无 padding,并按逗号拆分重复值(规范要求)。
- 应用层值仍由调用方(gate)按旧客户端的方式编码,传输层不知道 SessionDetails 是什么。
- 非 `-bin` 键的值必须是可打印 ASCII(0x20–0x7E),否则本地以 `INTERNAL` 失败。
- 自定义键不得以 `grpc-` 开头、不得是伪头;路由服按 `x-` 前缀整体透传,新实现**不得私自添加 `x-` 键**。

### 4.3 响应与状态

- 正常:HEADERS(`:status 200`、`content-type: application/grpc…`,可能带初始 metadata)→ 恰好一条消息 →
  trailers(END_STREAM,`grpc-status`、`grpc-message`)。trailers 之后 Go 可能补一个 RST_STREAM(NO_ERROR),不是失败。
- trailers-only:单个 HEADERS 带 END_STREAM,含 `:status`、`content-type`、`grpc-status`、`grpc-message`;此时初始 metadata 为空。
- **HTTP 200 或 protobuf 能解析都不代表成功**,只认完整的 gRPC 完成状态:
  - `content-type` 不是 `application/grpc` 开头(后接结束、`+` 或 `;`)或 `:status != 200`:按 HTTP 状态合成
    (400→INTERNAL、401→UNAUTHENTICATED、403→PERMISSION_DENIED、404→UNIMPLEMENTED、429/502/503/504→UNAVAILABLE、其余 UNKNOWN)。
  - 缺 `grpc-status`:UNKNOWN;值不是 0–16 的十进制:UNKNOWN。
  - 状态 OK 但消息数 ≠ 1,或有残留半条消息:INTERNAL(与 grpc-go「cardinality violation」一致)。
  - 无 trailers 就结束流:INTERNAL。
  - RST_STREAM:REFUSED_STREAM→UNAVAILABLE(**未被处理,可透明重试**)、CANCEL→CANCELLED、
    ENHANCE_YOUR_CALM→RESOURCE_EXHAUSTED、INADEQUATE_SECURITY→PERMISSION_DENIED、其余→INTERNAL。
  - 压缩标志为 1:INTERNAL(我们没声明任何编码,出现即对端违约)。
  - 响应超过接收上限(默认 4 MiB,与 grpc C++ 默认一致):RST_STREAM(CANCEL) + RESOURCE_EXHAUSTED。
- `grpc-message` 按规范百分号解码,解码永不失败(坏序列原样保留)。
- 状态码数值与 `grpc::StatusCode` 完全一致(日志里按整数打印,两边要能对照)。
- 本地截止时间到期:RST_STREAM(CANCEL) + DEADLINE_EXCEEDED。**超时不等于对端没执行**(go-zero 超时拦截器先返回、
  handler 继续跑),所以上层一律按「结果未知」处理,与 grpc_call_tag.h 的约定相同。

### 4.4 PING:必须服从 grpc-go 服务端默认的 keepalive 强制策略

Go 侧没有任何服务配置 `KeepaliveEnforcementPolicy`,即 grpc-go 默认:有活跃流时两次 PING 间隔 < 5min 记一次 strike,
无活跃流时 < 2h 记一次;超过 2 次 strike 即 `GOAWAY(ENHANCE_YOUR_CALM, "too_many_pings")`;连接上第一个 PING 不记;
服务端写过 HEADERS / DATA 之后到达的下一个 PING 清零 strike。现有 C++ 通道的 30s、空闲也 ping 的设置
已在日志里触发过 `too_many_pings`,新实现不照搬。

新实现的规则(对 Go 服务端**永远不会产生 strike**):

- 默认不做周期 keepalive(`keepalive_time = 0`)。
- 存活探测只在「某个调用的截止时间到期、而这条连接自该调用发出以来**一个帧都没收到**」时发一次 PING,且只在下列之一成立时发:
  这条连接还没发过 PING;或在上一次 PING 的 ACK **之后**收到过 HEADERS / DATA(说明服务端在处理我们上个 PING 之后写过数据,
  按 grpc-go 逻辑下一个 PING 清零)。
- 探测在 `keepalive_timeout`(默认 20s,与 grpc-go 服务端的 ACK 超时一致)内收不到 ACK:判连接已死,关连接、
  在途调用以 UNAVAILABLE 结束、通道重连。
- 对端的所有 PING(含 BDP ping、GOAWAY 后的 ping)由 nghttp2 自动 ACK。
- etcd(第 2 批)的长流需要周期 keepalive:那时按通道配置打开(etcd 默认 `--grpc-keepalive-min-time=5s`),不影响 Go 服务通道。

### 4.5 GOAWAY

- go-zero 服务端 `MaxConnectionIdle = 5min`,空闲连接会周期性收到两段式 GOAWAY(先 `last_stream_id = 2^31-1`
  再发真实值)。收到即把该连接标为 draining:不再开新流,新调用触发新连接(通道回到 IDLE→CONNECTING)。
- id 大于 `last_stream_id` 的流由 nghttp2 以 REFUSED_STREAM 关闭(`nghttp2_session.c` `session_close_stream_on_goaway`),
  按「未被处理」处理;HEADERS 因 GOAWAY 没发出去的请求同样以 REFUSED_STREAM 关闭(`nghttp2_session.c` 2885–2900)。
- 已接收的流照常等完;draining 连接上没有流之后由通道在 `queueInLoop` 里释放。

### 4.6 透明重试(只针对可证明未被处理的请求)

与官方 gRPC C++ / grpc-go 的 transparent retry 一致:流以 REFUSED_STREAM 关闭(含 GOAWAY 未处理、HEADERS 未发出)
且截止时间未到时,**在新实现内部**重发一次。其余任何失败(超时、断线、RST CANCEL、非 OK 状态)一律不重发,
更**不会**改走旧实现重发 —— 服务端可能已经执行。

### 4.7 并发与 SETTINGS

- Go 服务端不宣告 `MAX_CONCURRENT_STREAMS`(即不限),每个流一个 goroutine,背压只能由 C++ 自己做:
  通道在途上限(排队 + 活跃)默认 65536,超出本地以 RESOURCE_EXHAUSTED 失败。
- 本端 SETTINGS:`ENABLE_PUSH=0`、`INITIAL_WINDOW_SIZE=1 MiB`、`MAX_HEADER_LIST_SIZE=16 KiB`(与 grpc C++ 硬上限一致)
  ,连接级接收窗口放大到 16 MiB。收到的头部块超过 16 KiB:RST_STREAM + RESOURCE_EXHAUSTED。
- stream id 用尽(nghttp2 返回 `STREAM_ID_NOT_AVAILABLE`)时该连接转 draining,调用改走新连接。

## 5. 连接状态机与退避

对外暴露与 gRPC 相同的五态,供以后替换 `GetState`(`node_utils.cpp` 的 SceneManager / DataService 选点):

| 状态 | 含义 | 新调用 |
|---|---|---|
| IDLE | 没有可用连接 | 入队并开始连接 |
| CONNECTING | 正在建 TCP + 等服务端 SETTINGS | 入队(不是 wait_for_ready:各自截止时间到期即失败) |
| READY | 收到服务端首个 SETTINGS | 立即发出 |
| TRANSIENT_FAILURE | 本次尝试失败,退避中 | 立即 UNAVAILABLE(fail-fast,与现网未开 wait_for_ready 一致) |
| SHUTDOWN | 通道已拆 | 立即 UNAVAILABLE |

- READY 以「收到服务端 SETTINGS」为准,而不是 TCP 可写:Windows 的 `win_connect` 有假阳性(见 `rpc_client.h` 注释)。
- 一次连接尝试的上限 `connect_timeout` 默认 20s(gRPC MIN_CONNECT_TIMEOUT)。超时 → TRANSIENT_FAILURE,
  排队中的调用以 UNAVAILABLE 结束,按退避重连:初始 1s、倍数 1.6、抖动 ±20%、上限 120s;收到 SETTINGS 即重置
  ([connection-backoff](https://github.com/grpc/grpc/blob/master/doc/connection-backoff.md))。
- READY 连接断开(非 GOAWAY):在途流以 UNAVAILABLE 结束(REFUSED 的走 §4.6),通道回 IDLE,下次调用重连 —— 与
  pick_first 在 READY 断开后回 IDLE 的行为一致。
- **已知差异**:muduo `Connector` 没有「连接失败」回调(上游 `TcpClient.cc` 自己标了 `FIXME setConnectFailedCallback`),
  被拒绝的连接在我们看来一直是 CONNECTING,直到 `connect_timeout`;官方 gRPC 会立即进 TRANSIENT_FAILURE。
  后果只是失败来得晚(调用按自己的截止时间失败,gate 照样回 1003),不影响正确性。第 3 批要用连接状态做选点前,
  在三份 muduo 树里补这个回调(见 §10)。

## 6. 线程、生命周期与关机(AGENTS §11.7)

- 所有 API 只能在通道所属 EventLoop 线程调用(断言);新模块不创建线程。
- 完成回调**恰好一次**,且**永不在 `StartUnaryCall` 内同步执行**:一律 `queueInLoop` 投递。成功、失败、截止、取消、
  断线、通道析构之间的竞争由每个调用自己的 completed 标志收口。`id_segment_bootstrap.cpp:160-163` 这类「先发再登记」
  的调用方依赖这一点。
- 连接所有权只在 muduo:`Http2Transport` 按值持有 `TcpClient`,只存 `weak_ptr<TcpConnection>`;绑进 `TcpClient`
  回调与定时器的一律是 `weak_ptr<自己>`。拆连接 = 显式 `forceClose` 后直接销毁,不在连接自己的回调栈上释放最后一个引用
  (通道在 `queueInLoop` 里释放 draining / 失败的连接)。
- nghttp2 回调里不调用业务回调、不销毁任何对象(只记状态),避免重入 nghttp2。
- 截止时间用 `steady_clock`;muduo 定时器是墙钟,只作唤醒提示,到点用 steady_clock 复核,未到期就重新 arm。
- 节点摘除:通道是节点实体上的组件(`MuduoGrpcChannelPtr`),`ExecuteNodeRemoval` 的 `DestroyEntity` 析构它;
  析构把在途调用以 UNAVAILABLE 投递失败回调(gate 据此回客户端 1003 —— 旧路径这里是静默丢失)。
- 关机:关机分两段(2026-10-15 由「C++ RPC 异步回调链式调用设计」会话改成两段式,见 muduo-rpc-channel.md)。
  **阶段 0**(`Node::FinalizeShutdownInLoop`)取消定时器、关闭 RPC0 通道,并清掉所有 registry 上的 `MuduoGrpcChannelPtr`,
  然后 `queueInLoop(FinalizeTeardownInLoop)`。按 FIFO,`~Channel` 排进队列的 UNAVAILABLE 失败回调先于拆除段执行,
  执行时 ECS 与日志都还活着。拆除段(ReleaseNodeId …… `tlsEcs.Clear()` …… 两跳 quit)覆盖 `~TcpClient` 排进的
  `forceCloseInLoop` / `connectDestroyed`,理由与清 `RpcClientPtr` 相同(`~TcpClient` 要用 loop)。

## 7. 限额汇总

| 项 | 默认 | 依据 |
|---|---|---|
| 请求消息 | 4 MiB | grpc-go 服务端接收上限 |
| 响应消息 | 4 MiB | grpc C++ 默认接收上限 |
| 接收头部块 | 16 KiB | grpc C++ 元数据硬上限 |
| 通道在途调用(排队 + 活跃) | 65536 | 本地背压 |
| 输出缓冲 | 1 MiB 以上暂停从 nghttp2 取帧,写完回调后继续 | 防止慢对端把内存拖满 |
| 每次读事件 | muduo 一次 readv(≤ 可写空间 + 64 KiB),nghttp2 同步解完 | 单轮 CPU 有界 |

## 8. 第 1 批:Gate → client_rpc_router

- 只覆盖路由模式(`GATE_CLIENT_RPC_ROUTER=1`)下 gate 的唯一 gRPC 出站 `ClientRpcRouter.Forward`:业务请求与断线通知
  共用 `SendViaRouter` 这一个接缝(`client_message_processor.cpp`)。
- 开关 `GATE_ROUTER_TRANSPORT`,启动时读一次:`grpc`(默认,旧实现)| `muduo_h2`(新实现)。不认识的值按 `grpc`
  并 WARN。它与 `GATE_CLIENT_RPC_ROUTER`(业务拓扑:直连还是经路由服)是两回事;直连模式下这个开关不生效。
- `muduo_h2` 时 gate 在 `ClientRpcRouterNodeService` 的节点 registry 上下文里装一个通道工厂,`NodeConnector::ConnectToGrpcNode`
  看到工厂就建 `muduo_grpc::Channel`,**不再创建官方 `grpc::Channel`、Stub 与 CompletionQueue**。
  同一进程内其余 gRPC(etcd)仍走官方运行时 —— **不能据第 1 批宣称 gate 已经没有 gRPC 线程**。
- 回包 / 失败对客户端的可见行为与旧实现逐条相同(抽成 `rpc_reply_bridge` 供两条路径共用):
  成功按响应初始 metadata 里回显的 `x-session-detail-bin` 找会话、`MessageContent` 原样下发;
  失败按发出的 SessionDetails 找会话、回 `kServiceUnavailable`(1003);会话已不在只留采样日志;不重试。
- deadline 照旧取 `grpc_call_deadline::Get(ClientRpcRouterNodeService)`(配置 8000ms),线上编码成 `grpc-timeout`。
- 回退:改回 `GATE_ROUTER_TRANSPORT=grpc` 后滚动重启 gate。切换前先让该 gate 收敛在途调用(滚动重启本来就会断开客户端)。

## 9. 构建

- Windows:`tools/scripts/third_party/build_nghttp2.ps1` 用上游 CMake(Debug、`/MDd`、只编静态库、优先 Ninja)装到
  `third_party/nghttp2/install_vs2026_dbg`(子模块内,`.gitmodules` 为它设了 `ignore = dirty`,与 grpc 的安装目录同做法),
  产物 `lib/nghttp2.lib` 与 `include/nghttp2/{nghttp2.h,nghttp2ver.h}`。脚本先核对子模块提交号。
  使用 `nghttp2.h` 的翻译单元定义 `NGHTTP2_STATICLIB`(只有 `muduo_grpc` 库与它的测试)。
- 新库 `cpp/libs/engine/muduo_grpc/muduo_grpc.vcxproj`:只依赖 muduo 头(及 muduo 头间接要的 boost)与 nghttp2,
  **不依赖 protobuf / grpc / core / entt**(收发的都是字节,序列化由调用方做);`/utf-8`、`/WX`、`/MDd`。
  gate 链接 `muduo_grpc.lib` 与 `nghttp2.lib`(库目录加 `third_party/nghttp2/install_vs2026_dbg/lib/`);game.sln 里 gate 依赖它。
- 测试 `cpp/tests/muduo_grpc_test`:**不链接 grpc / grpc++ / core / session / proto**,只链 muduo、nghttp2。
- Linux:`setup_dependencies.sh` 加 nghttp2 段(核对同一提交 / 版本,源码 stamp 判定是否重编)、`Dockerfile.cpp` deps 阶段
  COPY 子模块、CI 子模块清单(不递归拉 nghttp2 的嵌套子模块)、`vcxproj2cmake.py` 的 `EXTERNAL_LIBS` 与工程清单、
  `build_linux.sh` 的 `LIB_PROJECTS`。运行镜像里 libcurl 依赖系统自带的旧版 `libnghttp2.so`,所以节点链接时加
  `-Wl,--exclude-libs,libnghttp2.a`,不把静态链入的 1.70 符号导出,避免 libcurl 运行期混用两个版本。

## 10. 分批计划

| 批 | 内容 | 前置 |
|---|---|---|
| **1(本批)** | nghttp2 依赖;`muduo_grpc` 客户端 unary;不链 grpc 的单测与对真实 Go 路由服的互通用例;gate 路由链路;开关默认旧实现 | — |
| 2 | 客户端双向流;etcd(KV / Txn / Lease / Watch / KeepAlive)迁到新传输:流断重建、按 revision 续 watch、compact 处理、关机 LeaseRevoke 先 flush;修 Windows `EventLoop::wakeup()` 空操作(删 5ms CQ 轮询的前提) | 1 验证通过 |
| 3 | 生成器:客户端模板改为生成传输无关的上下文 / 状态类型,scene→scene_manager / data_service 等全部出站迁移;`GetState` 选点改用新状态机;muduo 三份树补连接失败回调;删除 CQ 轮询 | 2 |
| 4 | 服务端:muduo `TcpServer` + nghttp2 server session;scene / battle handler 模板改为 loop 内异步完成;Agones 许可改异步;关机改 GOAWAY 排空,删关机线程 | 3 |
| 5 | 删除官方 gRPC:proto-gen 停用 `grpc_cpp_plugin`、删 `*.grpc.pb.cc`、28 个工程去掉 grpc 链接项、Linux 改独立构建 protobuf 35.1 / abseil、文档同步 | 4 |

每批完成后交 Codex 编译与验证,通过再开下一批。

### 10.1 来自 entt 审计的验收条件

2026-10-15 并行会话「代码中的 entt 使用问题」移交,原文见 [ecs-entt-misuse-audit-20261015.md](ecs-entt-misuse-audit-20261015.md)
「仍未修」表与「修复时的注意事项」。旧模板不在那边修,这里把它们列为迁移的验收条件:

| 审计号 | 问题 | 新传输的要求 | 落在哪批 | 第 1 批状态 |
|---|---|---|---|---|
| F010 | 节点摘除直接 destroy 挂着 CQ 的实体,在途调用永不回调、调用对象不释放 | 摘除时先让在途调用以失败结束(走失败回调),再释放 | 1(muduo 通道)/ 3(删 CQ) | muduo 通道已满足:`~Channel` 把在途调用以 UNAVAILABLE 投递失败回调(§6);官方路径仍是旧行为,第 3 批随 CQ 一起删 |
| F100 / F006 / F082 / F011 | 生成的 Send* 对节点实体无守卫 `get<CompletionQueue>` / `get<StubPtr>`;选点只按 `view<NodeInfo>`,混进 TCP 握手建的、缺这两个组件的实体 | 新模板对通道组件 `try_get`,缺失走失败回调;选点改按通道组件的 view | 3 | gate 路由路径已对 `MuduoGrpcChannelPtr` 用 `try_get`;缺组件时仍落到官方 Send*(旧行为),第 3 批改模板时一并收口 |
| F012 / F022 | `tlsNodeContextManager` 从不 Clear;RunNodeMain 兜底路径与 CQ / stub 未处理 | 关机在两跳排空前清掉所有传输组件;兜底路径同样处理 | 1(部分)/ 2 / 3 | `FinalizeShutdownInLoop` 已清 `MuduoGrpcChannelPtr`;兜底路径与 CQ / stub 待第 2、3 批 |
| C008 | 流式客户端把 registry 里可移动的 protobuf 响应组件地址交给异步 Read(swap-and-pop 后悬空) | 流的响应缓冲放进不可移动的通道 / 调用对象,不作为 entt 组件 | 2(etcd 双向流) | 不涉及(第 1 批只有 unary;调用状态存在 Channel 内部的节点式容器里,地址稳定) |

### 10.2 第 2 批(etcd)的验收条件:P0-D「etcd 双向流断开后不重建」

2026-10-15 并行会话「全局单例扩容交接」移交,不在旧 CQ 模板上修。证据与测试设计见
[global-singletons-p0-findings-20261014.md](../handoff/global-singletons-p0-findings-20261014.md)「## P0-D」及其「原报告:修复设计」。
以下结论均为静态分析(基线 `2f897bd07`),**未实测**:

- 生成的 `HandleCompletedQueueMessage` 遇 `ok=false` 只打 `RPC failed` 就 return:tag 不分发不回收、跳过本轮其余事件。
  Watch / LeaseKeepAlive 在任何一次断连后永久失效,约每 181s 本地判失租重注册一轮;推断从第 2 轮起本节点的键会先被 etcd
  删 1~2s 再写回,watch 正常的 gate 会立刻摘掉它。
- 每 2s 的全前缀 Range 只当 PUT 处理,补不回删除:流断期间下线的对端永久留成幽灵节点。
- DELETE 处理依赖 `prev_kv`,开 MVCC 压缩后可能拿不到;`ScheduleWatchReconnect` 只处理 canceled,不处理 `compact_revision`。
- 生成物 FINISH 分支是 `cq.Shutdown()`,真走到会关掉整条 etcd 完成队列。

第 2 批必须满足:

1. 流断(RST / GOAWAY / 靠 HTTP/2 keepalive 发现的静默断链)后按退避重建:0.2s 起、封顶 5s、±20% 抖动、稳定 10s 清零、**不设次数上限**。
   最后一项是对 AGENTS §11.3「重试有次数上限」的有意偏离:etcd 连接是节点身份的生命线,放弃重连等于主动失租;
   与 etcd clientv3 的做法一致。代价是 etcd 长时间不可用时持续重连(每 5s 一次,带抖动,不形成风暴)。
2. KeepAlive 流重建后立刻发一拍,不等 1s 定时器。
3. Watch 流重建后逐前缀做「Range → 对账删除本地多出的节点 → 从 R+1 watch」;快照不完整(`more=true`、值解析失败、键不属前缀)一律不删。
4. 处理 `compact_revision` 取消(重新 Range 后续 watch);DELETE 处理不依赖 `prev_kv`(按键反查本地快照)。
5. 不再有任何会关掉整条完成队列 / 整条连接的流结束分支;流结束只影响该流。
6. 验收口径:删除 / 重启一个 etcd 成员后**无节点 fence 且零重注册**(node-id-overhaul-plan 原口径只有前半句)。
7. 硬约束(移交方同时写进其 PROGRESS 与交接):任何 etcd 滚动(钉镜像版本、加压缩参数)都要等本项验证通过后再做。

回退开关:第 2 批沿用第 1 批的做法,新实现走开关、验证通过前默认仍是旧 CQ 版;**回退到旧实现时上述缺陷依然存在**,
运维手册与启动日志都要写明,回退只用于新实现自身出问题的场景。

## 11. 第 1 批验证(交 Codex,未执行)

见 `docs/PROGRESS.md` 2026-10-14 本批条目的「交 Codex」一节:依赖构建 → 单测(不链 grpc、线程检查)→
对真实 Go 路由服的互通用例 → gate 两种传输分别跑登录 / 进场 / 断线通知 → 新旧实现**分别**用隔离输入,
不对同一有副作用请求双发。

## 12. 双版本(AGENTS §12)

本改动只换 C++ 服务端内部传输,线上协议、客户端帧格式、proto、tip 码、配置表都不变,不属于两版共享的客户端契约;
Java 版用 Java gRPC,**不适用**。PARITY.md 不登记功能行。
