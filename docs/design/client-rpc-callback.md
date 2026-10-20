# 客户端请求自带回调:按请求号认领回包,生成类型化调用桩取代每消息 Handler

> 状态:**运行时与服务端前提已实施,未编译、未测试,待 Codex 验证**(2026-10-19,Claude)。生成器改造(§5)只出细则,由 Codex 实施;robot 运行时(§6)已由 Claude 实施(§6.1),同样未编译、未测试。
> 任务来源:用户 2026-10-19「客户端也改成 RPC 回调,用上 muduo 的消息 id,每次 request 自己塞回调函数,不用专门的 Handler;Handler 是工具生成的,生成部分出细节给 Codex,你按最标准的来」。
> 「muduo 的消息 id」即 muduo `RpcMessage.id`:每次调用一个调用号,对端原样回显,调用方按它找回自己的回调。节点间 RPC0 已按此改造(`muduo-rpc-channel.md`);客户端信封早就有同位字段 `ClientRequest.id` / `MessageContent.id`(`proto/common/base/message.proto:11-17,133-140`),本次让两端都把它用成唯一的关联依据。
> 事实核对基线:本仓 `1fc103dce`,客户端仓 `09c6ccea`,Java 仓 `4054006`。只读摸底工作流 `wf_fcd259cb-0bb`(5 个视角),结论与证据见附录 A。

## 0. 一句话

客户端每次请求分配非 0 的 `ClientRequest.id` 并登记自己的回调;回包只按 `MessageContent.id` 认领,恰好一次完成(回包 / 信封错误 / 超时 / 断线 / 本地未发出);`id = 0` 只用于推送与单向消息。服务端补上最后一处不回显请求号的路径(gate 直连模式的 gRPC 回包与失败)。每消息一个的生成 Handler 由「每个服务一个类型化调用桩类」取代(§5,Codex)。

## 1. 改造前的问题

| # | 问题 | 证据 |
|---|---|---|
| P1 | Unity 大厅的回包两级匹配:先按 id,id 为 0 时按 message_id 先进先出。超时后迟到的回包会被错配给下一次同号请求;帮会 / 组队 / 聚宝斋 / 商城 / 邮件 / 排行 / 好友 / 社交 8 个模块因此把「错误不明」一律隔离到重连 | 客户端 `GameClient.cs`(改前)42-54、1683-1721;`GuildClient.cs:1739-1741` 注释直指 FIFO |
| P2 | FIFO 兜底存在的唯一原因已过时:注释说「gate 的 SetIfEmptyHandler 只填 serialized_message + message_id」,该函数已在 `8b9592de3` 删除;今天只剩 gate **直连模式**的 gRPC 成功回包(`rpc_reply_bridge.cpp` 包一层时不填 id)与失败(推 23 号 tip)不回显请求号 | 服务端 `rpc_reply_bridge.cpp`(改前)92-95、134;`client-request-ordering.md` §4.1 订正注 |
| P3 | 超时靠每个调用一条协程每帧轮询 15s 截止;协程宿主一停,调用既不超时也不回调;N 个在途调用每帧重入 Tick N 次 | 客户端 `GameClient.cs`(改前)1633-1645 |
| P4 | 断线 / 换连接只清表、不回调,失败要等各协程下一帧自己发现 | 同上 2118-2119、1504-1505 |
| P5 | 大厅与战斗直连两套待回表,断线是否回调、迟到回包去哪、超时由谁驱动三点都不一致 | `BattleDirectLink.cs`(改前)103-136、430-444、839-845 |
| P6 | 「伪 RPC」:RefreshToken 单向发出、回包靠 `OnNotify(RefreshToken)` 接;ReleaseSkill 有回包却单向发,回包进 `[unhandled]` | `GameClient.cs`(改前)1798-1808、2053、1412 |
| P7 | 生成的 139 个 Unity Handler 与 `HandlerRegistry` 从未被注册,全是空壳;但 `XxxHandler.MessageId` 被业务当常量用(Scripts 20 处、Tests 7 个文件) | `GameClient.cs:1763-1767` 注释;§5.6 清单 |
| P8 | Go robot 完全不读 `MessageContent.id`,全部按 message_id 认领;friend 以外的 `call` 帮助函数会把上一次超时请求的迟到回包当成本次结果 | `robot/guild_smoke_scenario.go:840-922`、`team_smoke_scenario.go:691`、`trade_smoke_scenario.go:612` |

## 2. 契约(两端共同遵守)

### 2.1 线上关联规则

| 方向 | `id` | 含义 |
|---|---|---|
| C2S 请求-响应 | 非 0,连接内单调,不复用 | 服务端恰好回一个 `MessageContent`,`id` 原样回显 |
| C2S 单向消息 | 0 | 成功不回包;错误信封同样 `id = 0`,与推送一样按 message_id 派发 |
| S2C 回包 | = 请求的 id | 成功:`serialized_message` 是响应体;失败:`error_message` 带 tip(信封错误) |
| S2C 推送 | 0 | 按 message_id 派发 |

服务端的业务拒绝有两种形状,客户端都要处理:信封错误(`MessageContent.error_message`,gate / 路由服 / battle 拒绝)与响应体里的错误字段(scene / Go 服务自己写在响应 message 的 `error_message`)。前者是 `RpcStatusCode.ServerTip`,后者是 `Ok` + 业务判断。

### 2.2 客户端调用面(Unity:`Assets/Scripts/Net/RpcStatus.cs`、`OutstandingCalls.cs`)

- `IRpcChannel`:`CallMethod<TResp>(messageId, request, parser, RpcDone<TResp> done)`、`SendOneWay(messageId, request)`、`RegisterNotify(messageId, handler)`。大厅 `GameClient` 与战斗直连 `BattleDirectLink` 都实现它;生成的调用桩只依赖它(§5)。
- `RpcStatus`:`Ok / NotSent / Disconnected / Timeout / ServerTip(TipId) / ParseError`;`OutcomeUnknown` = Timeout 或 Disconnected(请求可能已执行,有副作用的请求复用业务幂等键重发)。
- `OutstandingCalls`:一条连接的在途表。请求号从 1 起、跳过 0;`TryComplete` 只按 id 认领;`ExpireDue(now)` 由所有者 Tick 驱动;`FailAll(status)` 用于断线 / 换连接 / 关闭;每项先摘除再回调,回调里重入安全,回调异常交给注入的处理器、不打断同批其余调用。
- **恰好一次**:每个 `CallMethod` 的 done 只调一次。本地未发出(未连接 / 直连未就绪 / 发送异常)在返回前同步以 `NotSent` 调用;其余结局在之后的收包派发或 Tick 里调用,都在主线程。
- **迟到回包**:`id ≠ 0` 却认领不到 → 计数、记日志、丢弃,**不得**再交给同号的推送处理器。
- **旧字符串错误契约**:`IBattleTransport.Call` 与 16 个实现、约 60 个调用点、大量测试按字符串判错(`"rpc timeout"`、`"server tip=N"`、`"link: …"`、`"disconnected"`)。本轮保留它们,唯一翻译点是 `RpcStatus.ToLegacyError(transportPrefix)`;协程版 `GameClient.Call` 与 `BattleDirectLink.Call` 都改成 `CallMethod` 的薄适配。业务模块迁到 `RpcStatus` 是后续工作(§7)。

### 2.3 超时

大厅 15s(`GameClient.RpcTimeoutSec`,不变式 I4:≥ gate 侧路由 deadline 8000 + lane 等待 5000 + 1000,`client-request-ordering.md` §5.2);直连 `BattleDirectLink.CallTimeoutSeconds`。精度 = 一帧。

## 3. 服务端前提:每条请求回包都回显请求号

| 路径 | 回显 id | 备注 |
|---|---|---|
| gate 本地拒绝(体积 / 限流 / GM 闸 / battle 拒绝 / 未登录 / 解析失败 / 找不到节点 / scene 未就绪) | 是 | `client_session_handler.cpp` 的 `SendRequestErrorEnvelope`、`gate_lane::SendEnvelope` |
| 路由模式(lane):成功、拒绝、失败、本地截止 | 是 | `forwardlogic.go:187-191,226-232`;`client_request_lane.cpp` |
| **直连模式 gRPC 成功 / 失败** | **本次改为是** | 见下 |
| gate → scene RPC0:成功、失败 1003、业务拒绝 | 是 | `client_session_handler.cpp:856`、`scene_handler.cpp:327-328` |
| battle 直连 | 是 | `battle_client_edge.cpp:436,491` |
| 白名单外的 message_id / 未验签 | 不回(有意:防放大 / 直接断连) | `client-request-ordering.md` §4.1 |
| scene 派发前早退(11 处) | **不回**,客户端以 Timeout 收场 | §7 残余 R1 |

**本次服务端改动**(`cpp/nodes/gate/handler/rpc/`):

- `rpc_reply_bridge.h/.cpp`:新增 `DirectClientCall{requestId, messageId}`;`DeliverReply` 与 `NotifyCallFailure` 增加 `const DirectClientCall *directCall` 参数。直连成功:包一层时 `set_id(requestId)`、`set_message_id(messageId)`(不再按响应类型反查——多个方法共用同一响应类型时反查只能得到其中一个号);直连失败:`gate_lane::SendEnvelope(session, requestId, messageId, kServiceUnavailable)`,与 lane 的失败回执同形。路由路径传 `nullptr`,行为不变。
- `client_session_handler.cpp`:`HandleGrpcNodeMessage` 的每次调用 lambda 捕获 `DirectClientCall{request->id(), request->message_id()}`,交给 `ForwardDirectGrpcCallResult`。
- `router_muduo_forward.cpp`:三处调用补 `nullptr`。

`client-request-ordering.md` §4.1 订正注里「直连失败桥拿不到客户端请求 id」的前提已在 2026-10-17 改为每次调用回调后不成立,本次据此收口;该文档追加了补注。

**Java 版**(AGENTS §12):gate 各路径(login 应答 / 失败、其它后端、lane 满、排队超时、闸门、转发 scene)都已回显 id(`ClientDispatcher.java:557,569,630-665,798-817,1471-1477`),battle 直连同样(`BattleFrames.java:41-56`),回显这一点无需改动。Java 的静默丢弃(gate 不跟踪已转给 scene 的请求、scene 侧会话不符等)与本仓 R1 同性质:客户端以 Timeout 收场。PARITY 登记待用户同意跨仓写入后补一行(§8)。

> 2026-10-19 复核订正(`wf_447ef09d-9bf`):「Java 无需改动」只对「回显 id」成立。Java gate 对**应答类型为 Empty** 的后端方法(match 的 148 CancelQueue,以及 154 / 156 两个推送占位)在 tip 为 0 时**不回包**(`ClientDispatcher.java:645-652` `replyToClient`,依据是 Java match-spec §8.1 / M4,`MessageRoutesTest`、`MatchBackendForwardingTest` 与 game-robot 的 `BattleSmokeScenario` 第 3 步都钉住了这一点);C++ 两种模式都回(路由服 `forwardlogic.go:186-191` 无条件回,直连经本次改动回带 id 的空包)。于是连 Java 服务端时 Unity 非全局取消排队(`BattleClient.cs` `_net.Call(CancelQueue, …, Empty.Parser, …)`)的回调只能 15s 超时、弹「取消排队失败:rpc timeout」——这不是本次改造引入的(改前客户端同样等不到回包),但它违反 §2.1「请求-响应恰好一个回包」,也是两版客户端可见行为的分歧。见 §7 R6,待用户拍板。

## 4. Unity 客户端改动(已实施,客户端仓,未提交)

| 文件 | 改动 |
|---|---|
| `Assets/Scripts/Net/RpcStatus.cs`(新) | `RpcStatusCode`、`RpcStatus`、`RpcDone<TResp>`、`IRpcChannel` |
| `Assets/Scripts/Net/OutstandingCalls.cs`(新) | 在途表 + `Typed(parser, done)` 解析适配 |
| `Assets/Scripts/Game/GameClient.cs` | 实现 `IRpcChannel`;`_pending` / `_pendingIdsByMsg` / `_seq` 换成 `OutstandingCalls`;删 FIFO 兜底;`CallMethod`(可指定超时)+ 协程 `Call` 适配;Tick 扫超时;`AdoptGate` 先置空 `_gate` 再 `FailAll(Disconnected)`;`DisconnectInternal` 在状态复位与断线通知之后 `FailAll`;单向消息发 `id = 0`;迟到回包记 `[rpc] late reply dropped` 后丢弃;RefreshToken / ReleaseSkill 改为请求自带回调,删掉 RefreshToken 的推送处理器 |
| `Assets/Scripts/Net/BattleDirectLink.cs` | 实现 `IRpcChannel`;待回表换成 `OutstandingCalls`;`CallMethod` + 旧 `Call` 适配(错误串不变,`link: ` 前缀);单向发 `id = 0` |
| `Assets/Tests/EditMode/Net/OutstandingCallsTests.cs`(新) | 9 条:请求号、按 id 不按 FIFO、id 0 不认领、信封错误、超时后回包为迟到、FailAll 重入、回调异常隔离、Remove、类型化解析 |
| `Assets/Tests/EditMode/Tianyong/ActorWorldFoundationTests.cs` | 移动类回执改 `id = 0`;新增「迟到回包不派发给同号推送处理器」断言 |
| 复核修复(2026-10-19,§10) | `GameClient.DisconnectInternal`:拆连接抛异常(`catch` 里先 `FailAll` 再 `throw`)或断线订阅者抛异常(通知外包 `try/finally`)时,在途调用仍立刻以 `"disconnected"` 收场;`BattleDirectLink`:`OpenConnection` / `RestartWithReissue` 拆旧连接后立即 `FailPending("connection_replaced")`,`Finish` 兜底 `FailPending(reason)`;新增构造参数 `onCallbackException`,`GameClient` 接到 `LogError`(两条连接的回调异常都报红);测试:`OutstandingCallsTests` 与两个战斗测试类加 `[TearDown]` 断言回调里没有被吞掉的异常,新增 `BattleDirectLinkTests.Call_FailsImmediately_WhenNewAssignmentReplacesTheConnection` |

行为变化(上线前需知,项目未上线,`project-not-launched`):

1. 单向消息(移动、SceneInfoC2S、GetSyncDigest、战斗直连单向)改发 `id = 0`;服务端对 id 无特殊处理(已核对 gate / 路由服 / battle,Java 只透传)。
2. 断线时在途调用在 `DisconnectInternal` 末尾**同步**以 `"disconnected"` 失败(改前是各协程下一帧发现);顺序仍是「先断线通知、后调用失败」。换连接(`AdoptGate`)时旧连接的在途调用同样以 `"disconnected"` 失败,`ZoneTravelClient` 据此判传送已交接的逻辑不变。
3. 回调抛出的异常被捕获并记错误级日志(大厅 `[rpc] callback threw`、直连 `[battle-direct] callback threw`,都走 `LogError`),不再沿收包派发上抛中断同批后续帧。**改前** onResponse 抛异常会被同一个 try 转成 `onError("parse response: …")`,等于同一次调用先成功、又报失败;改后不再补调 onError。
4. ReleaseSkill 改为请求-响应:被拒时记 `[skill] ReleaseSkill … rejected: tip=N`;未连接时记一行 NotSent(改前静默)。
5. 战斗直连换局(新分配包 / 对另一局补签)时,旧连接上的在途调用立即以 `"link: connection_replaced"` 失败(改前要等 15s 超时,或被新局之后的断线顶着失败)。上层按 `TransportErrorPrefix` 判传输层失败,这一类判断不变;`SpectateClient` 的 StopWatchBattle 失败提示不看局号,换局瞬间仍会弹一次「退出观战失败」,只是从 15s 后提前到当场。

## 5. 生成器改造细则(交给 Codex 实施)

目标:**不再生成每消息一个的 Handler**;每个客户端协议服务生成一个「类型化调用桩」静态类,请求-响应方法带 `RpcDone<TResp>` 回调,单向方法只发,推送方法生成类型化注册函数。生成器源码在 `tools/proto_generator/protogen/`(Go)。客户端仓的写入受 `enable_unity_client` 控制,**跑生成前须获客户端任务授权**(本任务已授权)。

### 5.1 方法形态必须显式声明(proto option)

现状无法机械区分请求 / 单向 / 推送(附录 A 生成器报告:按 `Notify` 前缀只是碰巧分对;按响应为 Empty 会把 8 个上行误判成推送、漏掉 `NotifyMailChanged`;gRPC 路径的 Empty 响应照样回包,例如 `CancelQueue`)。新增方法级 option,显式优于约定:

```proto
// proto/db/proto_option.proto,紧挨现有 MethodOptions 扩展(第 71 行起,OptionMessagePriority = 410000)
// 客户端协议方法的调用形态(docs/design/client-rpc-callback.md §5.1)。只对 is_client_protocol_service 的服务有意义。
enum ClientRpcKind {
  CLIENT_RPC_REQUEST     = 0;  // 客户端发起,恰好一个回包(默认)
  CLIENT_RPC_ONE_WAY     = 1;  // 客户端发起,成功不回包,以 id = 0 发送
  CLIENT_RPC_PUSH        = 2;  // 服务端推送,客户端不发起;载荷为方法输入类型
  CLIENT_RPC_SERVER_ONLY = 3;  // 服务端内部或 gate 代发,客户端不得发起
}
extend google.protobuf.MethodOptions {
  ClientRpcKind OptionClientRpcKind = 410001;  // 先核对 410001 未被占用
}
```

在 proto 上标注(以 Codex 核对后的全集为准,下列来自摸底报告):

- `CLIENT_RPC_PUSH`:全部 `Notify*` 方法(31 个)+ `SceneClientPlayerCommon` 的 `SendTipToClient` / `KickPlayer` / `RedirectToGate`。
- `CLIENT_RPC_ONE_WAY`:`SceneMovementClientPlayer.MoveStart / MoveStop / MoveSync`、`SceneSceneClientPlayer.SceneInfoC2S / GetSyncDigest`。判据 = 走 scene TCP 路径且响应类型**恰好**是 `Empty`(gate 按 `descriptor == ::Empty` 精确判定单向,`client_session_handler.cpp:859-866`)。
- `CLIENT_RPC_SERVER_ONLY`:`ClientPlayerLogin.Disconnect`(gate 代发)、`GuildService.UpdateGuildScore`(服务端专用)。
- 其余(含 `LeaveGame`、`CancelQueue`、7 个 `Gm*`)保持默认 `REQUEST`。

### 5.2 生成期校验(任一不满足即生成失败,打印服务.方法与原因)

1. `Notify` 前缀 ⇔ `PUSH`,唯一例外是 `SceneClientPlayerCommon` 的三个推送。
2. `ONE_WAY` 的响应类型必须恰好是 `Empty`(全名比较,**不要**用现有 `isEmptyResponse` 的子串匹配——它把 `LoginEmptyResponse` 也当 Empty),且目标协议是 scene TCP;反过来,scene TCP 路径上响应恰好是 `Empty` 的方法必须标 `ONE_WAY`(否则 gate 不回包,回调只能超时)。
3. `PUSH` 的输出类型必须是 `Empty` 或与输入类型相同(`NotifyMailChanged(MailChangedEvent) returns (MailChangedEvent)`,`go/mail/internal/notify/notify.go:81-85` 以输入类型填 `serialized_message`)。
4. 客户端协议服务的判定只看 `is_client_protocol_service` option,**删掉**按服务名含 `ClientPlayer` / `GamePlayer` 的过滤(它漏掉 `GuildService` 27 + `MatchService` 13 个方法;`SceneRollbackClientPlayer` 名字含 ClientPlayer 却没标 option,不应生成)。Unity 与 robot 两个生成器同改。
5. message id 为 `math.MaxUint64`(未分配)时报错,不再静默跳过;`0` 是合法号(`0=KVCompact`),不能当「未分配」。

### 5.3 Unity 产物

- **删除**:模板 `internal/template/unity_client_handler.cs.tmpl`、`unity_client_handler_registry.cs.tmpl`;客户端仓 `Assets/Scripts/Net/Generated/Handlers/`(整个目录与 `.meta`)与 `Assets/Scripts/Net/Generated/HandlerRegistry.cs(.meta)`。
- **新增**模板 `internal/template/unity_client_rpc.cs.tmpl`,每个客户端协议服务一个文件 `Assets/Scripts/Net/Generated/Rpc/<Service>Rpc.cs`,形如:

```csharp
// <auto-generated>
//   Generated by protogen from proto/friend/friend.proto. DO NOT EDIT.
// </auto-generated>
using System;
using Google.Protobuf;
using MmorpgClient.Net;

namespace MmorpgClient.Net.Generated
{
    /// <summary>ClientPlayerFriend(proto/friend/friend.proto)的客户端调用面。</summary>
    public static class ClientPlayerFriendRpc
    {
        // REQUEST:请求自带回调,done 恰好一次(IRpcChannel 契约)。
        public const uint AddFriendMessageId = 57;
        /// <summary>{proto 方法前导注释,原样搬运;没有则省略}</summary>
        public static void AddFriend(IRpcChannel channel, Friendpb.AddFriendRequest request,
                                     RpcDone<Friendpb.AddFriendResponse> done)
            => channel.CallMethod(AddFriendMessageId, request, Friendpb.AddFriendResponse.Parser, done);

        // ONE_WAY:只发,id = 0。
        public const uint MoveStartMessageId = 134;
        public static void MoveStart(IRpcChannel channel, MoveStartC2S request)
            => channel.SendOneWay(MoveStartMessageId, request);

        // PUSH:类型化注册;坏包交给 onMalformed(默认丢弃),不抛进收包派发。同号覆盖语义不变。
        public const uint NotifyFriendEventMessageId = 63;
        public static void OnNotifyFriendEvent(IRpcChannel channel, Action<Friendpb.FriendEvent> handler,
                                               Action<InvalidProtocolBufferException> onMalformed = null)
            => channel.RegisterNotify(NotifyFriendEventMessageId, mc =>
            {
                Friendpb.FriendEvent message;
                try { message = Friendpb.FriendEvent.Parser.ParseFrom(mc.SerializedMessage); }
                catch (InvalidProtocolBufferException ex) { onMalformed?.Invoke(ex); return; }
                handler(message);
            });

        // SERVER_ONLY:不生成任何成员。
    }
}
```

  要点:成员**不用扩展方法**(多个服务有同名方法,同命名空间里的扩展方法会二义);号、类型名为示意,以生成时为准;C# 类型名沿用 `resolveCSharpType`;`ONE_WAY` 的参数是请求类型;`PUSH` 的载荷是**输入类型**;注释从 descriptor 的 source info 取前导注释(描述符已带 `--include_source_info`,需新写提取函数),取不到就不写 `<summary>`。
- **目录整写**:每次生成先算出本次全部输出文件,写完后删除 `Rpc/` 下不在本次清单里的 `*.cs` 及其 `.meta`(方法 / 服务删除后不留旧桩)。
- **`.meta` 确定化**:为每个生成的 `.cs` 同时写 `.meta`,GUID = `md5("protogen:" + 相对 Assets 的路径)` 的 32 位十六进制,内容与仓内脚本 `.meta` 同格式(参照客户端仓 `Assets/Scripts/Net/RpcStatus.cs.meta`)。避免每次生成 GUID 漂移、也避免 Unity 首次打开时才生成导致的无关 diff。
- **配置**:`etc/proto_gen.yaml` 与 `etc/proto_gen.no-unity.yaml` 两份同改:删 `unity_handler_dir`、`unity_handler_registry_file`,加 `unity_rpc_dir: "{{unity_client_dir}}Assets/Scripts/Net/Generated/Rpc/"`;`internal/config/config.go` 对应字段同改;`enable_unity_client` 语义不变。
- **`MessageIds.cs` 本轮不动**(客户端仓 `tools/gen_messageids.ps1` 按手写白名单生成,业务大量引用)。退役它是后续工作,前提是业务全部改用 `<Service>Rpc.<Method>MessageId`。

### 5.4 生成器单测(新增,Go)

`internal/generator/unity/unity_client_rpc_test.go`(或同包现有测试文件),用内存构造的 `MethodInfo` / descriptor 夹具:

1. 三种形态各一个方法 → 输出文本含对应成员签名,`SERVER_ONLY` 不出现。
2. §5.2 的每条校验各一个反例 → 返回错误,错误信息含服务.方法名。
3. 目录整写:预置一个已不存在的服务文件 + `.meta` → 生成后被删除。
4. `.meta` GUID 对同一路径两次生成一致。
5. 服务名不含 `ClientPlayer` 但标了 option 的服务(如 `GuildService`)被生成;名字含 `ClientPlayer` 但没标 option 的不生成。

### 5.5 robot 生成器(随 §6 一起做,可放第二步)

`internal/generator/go/robot_handler.go`、`robot_case.go`:

- 保留**推送**的类型化 handler 与注册表(`robot/logic/handler/message_body_handler.go` 只注册 `PUSH`,以及 §6 迁移完成前仍依赖副作用的旧请求 handler——见 §6 第 4 步)。
- 新增 `robot/generated/rpc/<snake_service>.go`(整写、删旧):每个 `REQUEST` 方法生成
  `func <Method>(c *pkg.GameClient, req *<ReqType>, timeout time.Duration, done func(*<RespType>, error))` 与
  `func <Method>Sync(ctx context.Context, c *pkg.GameClient, req *<ReqType>) (*<RespType>, error)`;`ONE_WAY` 生成只发函数。
- 已存在即跳过的单方法桩(`robot_handler.go:68-78`)不再为 `REQUEST` 方法新建;已有且带副作用的 58 个手写 handler 不删,随场景迁移逐个退役。
- 同样改用 option 判定(§5.2 第 4 条),修掉取 `service.Methods[0]` 前不判空(`robot_case.go:27`)。

### 5.6 迁移 `XxxHandler.MessageId` 引用(Codex,生成之后、编译之前)

删 Handler 后以下引用会断,逐个改为 `<Service>Rpc.<Method>MessageId`(或已有的 `MessageIds.X`,二选一,同一文件内统一):

| 文件 | 处数 |
|---|---|
| `Assets/Scripts/App/DevTeamInvitationDriver.cs` | 4 |
| `Assets/Scripts/Game/PlayerFeatures/PlayerFeaturesClient.cs` | 7 |
| `Assets/Scripts/Game/Social/FriendClient.cs` | 5 |
| `Assets/Scripts/Game/Social/SocialClient.cs` | 2 |
| `Assets/Scripts/Game/Team/TeamInvitationDirectory.cs` | 2 |
| `Assets/Tests/EditMode/Battle/EquipClientTests.cs` | 2 |
| `Assets/Tests/EditMode/Battle/EquipForgeClientTests.cs` | 5 |
| `Assets/Tests/EditMode/Battle/EquipWindowTests.cs` | 7 |
| `Assets/Tests/EditMode/Battle/PlayerFeaturesClientTests.cs` | 11 |
| `Assets/Tests/EditMode/Net/LandingSyncClientTests.cs` | 1 |
| `Assets/Tests/EditMode/Social/FriendRequestTests.cs` | 6 |
| `Assets/Tests/EditMode/Social/SocialTests.cs` | 3 |

核对命令:`rg -n "Handler\.MessageId|HandlerRegistry" Assets --glob '*.cs' --glob '!Assets/Scripts/Proto/Generated/**'` 结果为空。注释里提到 `HandlerRegistry` 的三处(`GameClient.cs` 重定向处理器注释、`LandingSyncClient.cs:175`、`SceneChannelClient.cs:239`)改为说明「推送注册以 OnNotify / `<Service>Rpc.On<Method>` 为准」。

## 6. robot 运行时改造细则(第 1–3、5、6 条已实施;第 4 步 A / B 类已迁、C 类待迁,见 §6.1;未编译)

`robot/pkg/client.go`:

1. `SendRequest` 改发 `Id = 0`(不关联):回包仍按 message_id 交给现有 handler,旧场景零改动。服务端对 id 无特殊处理。
2. 新增在途表(`sync.Mutex` 保护,`map[uint64]*outstanding`,请求号原子自增、跳过 0)与
   `Call(messageId uint32, body proto.Message, timeout time.Duration, done func(*base.MessageContent, error))`:登记后发送,`time.AfterFunc(timeout)` 到期以 `ErrRPCTimeout` 完成;done 恰好一次(摘表成功者才回调)。
   错误值:`ErrRPCNotSent`、`ErrRPCTimeout`、`ErrRPCDisconnected`、`*ServerTipError{Tip uint32}`(信封错误)。
3. `RecvLoop`:`mc.Id != 0` → 认领并回调,认领不到计 late 并丢弃(**不**交给 handler);`mc.Id == 0` → 现有 `onMessage`。`Close` / `SwapConn` → 全部在途以 `ErrRPCDisconnected` 完成。done 可能跑在 RecvLoop 或定时器 goroutine,**不得阻塞**;另给 `CallSync(ctx, ...)`(阻塞调用方 goroutine 等结果),注释写明禁止在 RecvLoop goroutine 上调用(`pkg/redirect.go:114-143` 的重登录就在 RecvLoop 上,改它要先挪出去)。
4. 迁移顺序:先 friend / guild / team / trade 四个 `call` 帮助函数(guild / team / trade 没有迟到过滤,是现存缺陷)→ 其余等待帮助函数 → 依赖生成 handler 副作用的状态序号等待(pet / attribute / features / equip)最后迁。每迁一个场景,跑对应冒烟。
5. vendored muduo 不 close `incoming`、`Send` 吞错(`vendor/.../connection.go:251-266`、`client.go:31-36`):断线只能靠 `Close`/`SwapConn` 与超时兜底,不要写依赖 `Recv` 报错的分支。
6. 单测:`pkg/client_test.go` 增加按 id 认领、乱序回包、超时后迟到回包丢弃、Close 失败全部、done 恰好一次(`-race`)。

### 6.1 实施记录(2026-10-19,Claude,未编译、未测试)

**运行时**(`robot/pkg/client.go`):

- `SendRequest` 改发 `Id = 0`,旧场景照常按 message_id 收回包。
- 新增 `Call(messageId, body, timeout, done)` / `CallSync(ctx, messageId, body, timeout)` / `LateReplies()`,错误值 `ErrRPCNotSent` / `ErrRPCTimeout` / `ErrRPCDisconnected` / `*ServerTipError{Tip}`。done 的形状:成功 `(reply, nil)`;信封错误 `(reply, *ServerTipError)`,reply 是那条信封;其余 `(nil, err)`。
- 在途表 `calls map[uint64]*outstandingCall` 由 `callMu` 保护;请求号由 `seq` 发放,从 1 起、跳过 0;每项记下发出它的 muduo 连接。超时用 `time.AfterFunc`。摘表成功者才回调,所以回包、定时器、断线、ctx 取消并发时 done 仍恰好一次;回调都在放锁之后调用。
- 本地未发出(没有连接、已 Close、序列化失败、`timeout <= 0`)在 `Call` 返回前同步以 `ErrRPCNotSent` 完成。`done == nil` 是编程错误,直接 panic。
- 收包统一走 `deliver`:`mc.Id ≠ 0` 认领;认领不到的计 `lateReplies`,记 `[rpc] late reply dropped` 后丢弃,不交给 onMessage。`mc.Id = 0` 交给 onMessage。`RecvLoop`、暂存补投(`replayDeferredMessages`)都经它派发;同步握手用的 `RecvOne` 读到 `id ≠ 0` 时就地认领,然后继续读。
- `Close`:先让全部在途调用以 `ErrRPCDisconnected` 失败,并置终态,之后的 `Call` 一律 NotSent;再关连接。之所以先失败调用,是因为 muduo 的 `Close` 黑洞地址下可能阻塞到拨号 OS 超时。`SwapConn`:换上新连接后,只让旧连接上的在途调用失败,再关旧连接。startCall 在 `callMu` 内取连接快照并登记,所以换连接前后没有漏网的调用。
- `CallSync` 的注释写明禁止在 RecvLoop goroutine 上调用;`pkg/redirect.go` 的 `ReloginFunc` 注释同步写明重登录只能用 `SendRequest + RecvOne`。
- 测试替身注入点 `transmit` / `afterFunc` 是未导出字段,nil 时走生产实现。

**单测**(`robot/pkg/client_test.go`,14 条新用例):SendRequest 发 id 0;同号两调用按 id 认领;跨号乱序回包;信封错误;id 0 同号消息交 onMessage 且不认领;超时后迟到回包丢弃并计数;回包后定时器回调竞争不重复回调;Close 按请求序失败全部、之后 NotSent、关闭前请求的回包算迟到;SwapConn 只失败旧连接上的调用;无连接 / 零超时 NotSent;CallSync 成功;CallSync 以 ctx 取消原因完成且迟到回包丢弃;ctx 已结束不发送;400 次调用在真实定时器、重复回包、ctx 取消与 Close 并发下每个 done 恰好一次(配合 `-race`)。定时器用手动替身,只有并发用例用真实定时器,而且其断言不依赖先后。muduo 连接接本机监听端口,只为让 `Close` / `SwapConn` 走真实路径。

**场景迁移(第 4 步首批)**:新增 `robot/smoke_rpc_call.go`。其中 `smokeCaller.call` 取代 friend / guild / team / trade 四份相同的「按消息号认领 + 50ms 轮询」循环,返回形状不变:`(envelopeTip, err)`,超时仍包裹各场景自己的哨兵(`errFriendSmokeTimeout` 等)。gate 推 `SendTipToClient(kServiceUnavailable)` 时,经 `context.WithCancelCause` 立刻中止当前等待,报错文案不变。onMessage 中:

- 拦下请求消息号且 `id = 0` 的消息(`smokeDropUncorrelatedReply` 记一行后丢弃)。正常回包带请求号,不会走到这里;出现这种消息说明服务端没回显请求号,例如直连模式下的 gate 不含 §3 的修复。
- 删掉各 bot 的 `replies` / `gateTips` 字段,friend 删掉 `awaiting`。friend 第 6 步对推送号本身发请求,现在靠 id 区分回包与推送。
- 请求回包不再经过 onMessage,`MsgRecv` 改由 `smokeCaller` 在收到回包时计数。

`team_follow_smoke_scenario.go` 构造 `teamSmokeBot` 的地方改为注入 `caller`。它的留底只消费推送,不受影响。

**迁移进度**:

| 状态 | 场景 / 帮助函数 |
|---|---|
| 已迁(Call,10-19) | friend / guild(含 guild_economy、guild_activity 经同一 `call`)/ team(含 team_follow、team_backfill 复用的 `teamSmokeBot.call`)/ trade |
| 已迁(Call,10-20 第一批) | A 类:mail、rank、sync 的 `friendRpc`。B 类:travel 与 sync 的 `travelTo`、travel 的 `expectTravelRejected` 与金币读写、channel 的 `requestEnterScene`、mystery_merchant 的 `merchantCall`、ordering 的单发与流水线等待(`callAll`)、team 的排队请求(`matchCall`:JoinQueue / CancelQueue / GetQueueStatus,含 team_backfill)与 `startSoloBattle` |
| 待迁:依赖生成 handler 副作用(C 类) | attribute / pet / equip(含 equip_forge、equip_forge_topup、asset_arrival_prepare)/ features(含 features_battle_smoke 直连)/ currency_crash_window 的 `readGoldBalance`、`gmAddGold` / chat / battle 大厅侧与 battle_direct_conn 的 GetBattleState 补拉 / main.go 与 login_test_scenarios 的 ListSkills、技能、换场景。分组与做法见 robot 交接 §8 |
| 不迁 | 登录与重定向重登录的同步握手(`login.go`、`login_test_scenarios.go`、`main.go`、`pkg/redirect.go`):跑在 RecvLoop 前或 RecvLoop 上,只能 `SendRequest + RecvOne`。scene TCP 上响应恰好是 Empty 的单向方法(Move*、SceneInfoC2S、GetSyncDigest)与发出即走的请求保留 `SendRequest` |

逐处待迁清单(带行号)、按等待对象的重新归类(chat / currency_crash_window / battle_smoke 大厅侧 / channel requestSceneInfo 实属「依赖生成 handler 副作用」)与迁移陷阱见 [robot 交接](../handoff/client-rpc-callback-robot-handoff-20261019.md) §5;以那里为准。

**10-20 续做(第一批,工作流 `wf_26396b1d-800`,sync / travel 补评审 `wf_0f932c90-afc`;未编译)**:

- 运行时补充 `CallSyncThen(ctx, messageId, body, timeout, onReply)`(会话「Client RPC callback handoff [53a2d1]」所加):本次请求的回包(成功或信封错误)先在派发它的 goroutine(RecvLoop,或同步握手里的 RecvOne)上调 `onReply`,再唤醒调用方;超时、断线、ctx 先结束、本地未发出都不调;`CallSync` 等于 `CallSyncThen(..., nil)`。新增单测 3 条 `TestCallSyncThen*`。用途是 C 类:回包的副作用(交给生成 handler 写 Player 状态)与迁移前同一 goroutine、同一收包顺序,调用方返回时状态已写好。
- `smokeCaller` 补充:`callWithin`(这一次的超时预算单独指定);`callDispatch`(基于 `CallSyncThen`,成功回包在 RecvLoop 上交给 dispatch 后才返回;信封错误**不交** dispatch、照常作为 envelopeTip 返回;response 可为 nil)。`stats` 可为 nil;`timeoutErr` 为 nil 时包裹 `pkg.ErrRPCTimeout`。
- 迁移的三种形状:同形 call 直接换 `smokeCaller`;回包可有可无的传送(`travelTo`)用 `gc.Call` 异步登记、done 只把结局写进带锁的结构,原轮询循环照旧等 msg 124 → 严格重登 → NotifyEnterScene,`ErrRPCDisconnected`(124 之后 `SwapConn`)按正常路径处理;流水线(ordering)用多个 `gc.Call` 各自登记,结果按请求号对应。所有改动场景的 onMessage 把请求消息号上 `id = 0` 的消息交 `smokeDropUncorrelatedReply` 丢弃,推送照旧留底。
- 评审修复:mail 第 7 步的反向断言(「MailAdmin 号应被 gate 丢弃 → 超时」)加了「无请求号回包」计数,未回显请求号的信封不再被当成 gate 丢弃;ordering 的回包超时由 8s 提到 15s(满足 §2.3 不变式 I4:流水线里第二条的定时器从发出起算,而 lane 要等第一条完成才转发);其余是陈旧注释。
- 已随用户快照 `c35e1133d`(10-20 03:08)入库;评审的修复在其后,见 robot 交接 §1。

**未覆盖**:`smokeCaller` 没有单测,靠四个冒烟验证。muduo 被对端断开后会静默重拨,在途调用只能以超时收场(第 5 条)。

## 7. 残余与后续

- **R1 scene 派发前早退不回包**(`scene_handler.cpp` 11 处 `return`,RPC0 照常回 RESPONSE 但无 `message_content`,gate 丢弃,`client_session_handler.cpp:95-98`):客户端以 15s Timeout 收场。建议 gate 在「RPC 成功但无 message_content 且内层响应非 Empty」时回带 id 的 1003;两版同批(Java scene 侧同类静默丢弃 `ClientRequestHandler.java:195-242`)。客户端可见行为变化,需用户拍板。
- **R2 业务模块仍用旧字符串错误**:8 个模块因 FIFO 错配而「错误不明即隔离到重连」的保守策略,在 id-only 认领后可以放宽为「仅 `OutcomeUnknown` 才隔离」。逐模块改、逐模块测,不在本轮。
- **R3 `IBattleTransport` 与 `IRpcChannel` 并存**:16 个实现与约 60 个调用点未迁,`ToLegacyError` 是唯一桥。迁完后删 `Call(onResponse, onError)` 与 `ParseCallLevelTip` 一类字符串解析。
- **R4 推送号进了 gate 上行白名单**(`service_register_info.go:537,555-557` 的 `IsClientMessageId` 不按方向):§5.1 的 option 落地后,生成白名单时排除 `PUSH` 与 `SERVER_ONLY`(`Disconnect` 由 gate 合成,不经白名单),把方向约束收到服务端。改 gate 白名单是服务端行为变化,与 §5 分开做。
- **R5 `MessageIds.cs` 手写白名单**:见 §5.3 末条。
- **R6 Java gate 对 Empty 应答的后端请求成功不回包**(§3 复核订正):建议 Java `replyToClient` 改为「`request.getId() != 0` 就恰好回一个包(Empty 回空 body),id 为 0 才不回」,与 C++ 与 §2.1 一致;连带改 `MessageRoutesTest`、`MatchBackendForwardingTest`、game-robot `BattleSmokeScenario` / `BattleCrossZoneScenario` / `RobotOptions` 里「148 不回包」的断言与说明,以及 Java `architecture.md:109,971` 与 match-spec §8.1 / M4,PARITY 登记一行。客户端 `BattleClient.SendGlobalCancellation` 的注释已改为只描述 Java 现状。属于 Java 侧客户端可见契约变更,待用户拍板。

## 8. 双版本与对账

- 客户端契约变化只有两条:单向消息改发 `id = 0`;回包按 id 认领(两版服务端都已回显,无需改)。回显这一点 Java 版服务端**已满足**,本仓服务端补齐直连模式一处;「Empty 应答也恰好回一个包」Java 尚不满足,见 §7 R6。
- PARITY:建议在 Java 仓 `PARITY.md` 登记一行「客户端请求号关联:C++ gate 直连模式回包 / 失败回显 id(本次)↔ Java gate 已满足(`ClientDispatcher` 全路径回显)」,跨仓写入待用户同意;另 `PARITY.md:170`(「ClientForward 不带关联号,Java 待做」)已过时,`node_link.proto:120` 早有 `request_id`,一并订正。

## 9. 给 Codex 的验证步骤

按顺序;任何一步失败保留输出并停下。

1. **服务端 gate**(本仓根目录):`msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`(必须串行 `/m:1`)。期望 gate 工程编过;改动只在 `cpp/nodes/gate/handler/rpc/{rpc_reply_bridge.h,rpc_reply_bridge.cpp,client_session_handler.cpp,router_muduo_forward.cpp}`。
2. **Unity EditMode**(客户端仓 `D:\luyuan\mmorpg-client`,Unity 6000.6.0f1):
   `Unity.exe -batchmode -nographics -projectPath D:\luyuan\mmorpg-client -runTests -testPlatform EditMode -testResults <scratch>\editmode.xml -logFile <scratch>\editmode.log`
   通过标准:全部 EditMode 通过,重点 `MmorpgClient.Tests.EditMode.Net.OutstandingCallsTests`(9 条,TearDown 断言回调无异常)、`ActorWorldFoundationTests.MovementEmptyReplies_AreConsumedWithoutFourHertzUnhandledLogs`、`BattleDirectLinkTests`(含新增 `Call_FailsImmediately_WhenNewAssignmentReplacesTheConnection`)、`DirectRoutingBattleTransportTests`(错误串不变;两类都在 TearDown 断言回调无异常)。失败保留 xml 与 log 中 `error CS` / 失败用例段落。
3. **生成器(§5)**:实施后在 `tools/proto_generator/protogen` 下 `go test ./...`;再 `pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build`、`pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run`(默认配置,写客户端仓;本任务已获客户端授权)。期望:客户端仓 `Net/Generated/Handlers/` 与 `HandlerRegistry.cs` 消失,`Net/Generated/Rpc/*.cs` + `.meta` 出现;本仓除 proto option 标注带来的生成物外无意外 diff;再跑一次生成为零 diff。之后按 §5.6 迁移引用,重跑第 2 步。
4. **冒烟**(直连模式与路由模式各一次;直连 = gate 进程不设 `GATE_CLIENT_RPC_ROUTER`):`pwsh -File tools/run_crosszone_pair.ps1`(客户端仓)走完登录 → 建角 → 进场 → 传送;客户端日志里 `[rpc] late reply dropped` 为 0、没有 `rpc timeout`;直连模式下 Login / CreatePlayer / EnterGame 回包的 `MessageContent.id` 非 0(gate TRACE 或客户端日志可见)。
5. robot(§6,已实施):**10-20 起完整命令、冒烟清单与通过标准以 [robot 交接](../handoff/client-rpc-callback-robot-handoff-20261019.md) §2 为准**(新增 `TestCallSyncThen*` 与第一批迁移的冒烟)。以下是 10-19 首批的原文,保留备查。在 `robot/` 下依次执行 `go build -mod=vendor ./...`、`go vet -mod=vendor ./...`、`go test -mod=vendor -race ./pkg/...`,再执行 `go test -mod=vendor ./...`,确认主包既有单测不受影响。通过标准:全绿;`-race` 无 DATA RACE;`TestCallDoneExactlyOnceUnderConcurrentOutcomes` 不超时。随后跑四个冒烟。前置条件见各自 yaml 文件头与设计文档,包括两区 gate 以 `GATE_CLIENT_RPC_ROUTER=1` 启动、对应服务就绪。gate 须是含 §3 修复的新编译版,否则直连模式的回包不带请求号。在 `robot/` 下执行:`.\robot.exe -c etc/friend_smoke.yaml`、`etc/guild_smoke.yaml`、`etc/team_smoke.yaml`、`etc/trade_smoke.yaml`;改了构造的 `etc/team_follow_smoke.yaml` 可选加跑。通过标准:各自输出 `*_SMOKE_OK` 并退出 0;日志里 `reply without request id dropped` 与 `[rpc] late reply dropped` 都为 0(trade 第 2 步的超时是 gate 丢弃请求,本就不会有回包)。失败时保留 robot 输出中的 `*_SMOKE_FAIL step=… reason=…` 行,以及同窗口的 gate、路由服日志。

## 10. 语义复核记录(2026-10-19,`wf_447ef09d-9bf`)

3 个维度查找(大厅 GameClient / 共享在途表与战斗直连 / gate 直连回显与 Java 对照),每条发现 2 个复核者尝试驳倒;共 5 条,0 条被驳倒。大厅维度没有发现问题。

| # | 发现 | 级别 | 处理 |
|---|---|---|---|
| 1 | 直连换局 / 对另一局补签只拆连接、不让在途调用失败,旧调用等 15s 或被新局的断线顶着失败(改造前即如此,本次统一没收口) | minor | 已修(§4 复核修复行、行为变化 5);`Finish` 一处复核者认为实际不可达,仍加兜底 |
| 2 | `DisconnectInternal` 的 `FailAll` 不在异常安全位置:拆连接或断线订阅者抛异常时,在途调用拖到 15s 变成 `"rpc timeout"`(业务把它当另一种失败) | minor | 已修(`catch` + `try/finally`,不重排原有拆连接代码) |
| 3 | 直连在途表的回调异常只记 info 日志,与大厅的 `LogError` 不一致;改前 onResponse 异常会变成玩家可见的 onError | minor | 已修(`onCallbackException` 接 `LogError`);行为变化 3 写明不再补调 onError |
| 4 | 回调里的 `Assert.Fail` 被在途表捕获,Unity 内置 NUnit 3.5 不记录被捕获的断言,「恰好一次」类护栏形同虚设 | minor | 已修(三个测试类 `[TearDown]` 断言回调异常列表为空) |
| 5 | Java gate 对 Empty 应答的后端请求成功不回包,连 Java 时 CancelQueue 回调只能超时;§3「Java 无需改动」不成立 | major(复核者之一建议 minor~major) | 文档订正(§3、§7 R6、§8);客户端注释订正;Java 改动待用户拍板 |

## 附录 A:摸底结论摘要(`wf_fcd259cb-0bb`)

- 服务端回显审计:除直连模式 gRPC 成功 / 失败(本次已改)与 scene 早退(R1)外,所有请求回包都回显 id;推送一律 `id = 0`,没有发现非 0 的推送。证据见 §3 表。
- Unity:大厅两级匹配 + 协程超时 + 断线不回调;直连已是 id-only + Tick 超时 + 断线回调;生成 Handler 从未注册;`*Handler.MessageId` 被当常量用(§5.6)。
- robot:不读 id;47 处 `SendRequest`、16 处 `RecvLoop`、约 20 个等待帮助函数;vendored muduo 断线不可感知。
- 生成器:Unity / robot 两个生成器都按服务名过滤、都不清旧文件、都无单测;方法级只有 `OptionMessagePriority`,无方向 option。
- Java:全路径回显;PARITY 两行过时;兄弟工作树 `wt-guild-client-request-id` 是 Java 仓的帮会经济业务幂等键(`client_request_id`)快照,与信封 id 是两个概念,无冲突。**业务幂等键必须保持「一次操作一个键、所有重发共用」**,不能拿信封 id 代替。
