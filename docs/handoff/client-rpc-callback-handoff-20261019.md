# 交接:客户端请求自带回调(按请求号认领回包)(2026-10-19)

> 写给换账号后接手的会话。**事实源是设计文档 [client-rpc-callback.md](../design/client-rpc-callback.md)**(契约 §2、服务端 §3、Unity §4、生成器细则 §5、robot §6 / §6.1、残余 §7、验证 §9、复核 §10);本文只讲「做到哪、什么没提交、接下来做什么、哪些要拍板」。
> 写作时间:2026-10-19 约 10:20(America/New_York)。本仓 HEAD `c63b22dca`,客户端仓 HEAD `d6338af1`,Java 仓 HEAD `0e4a613`。同一工作区有多个会话并行,接手前先按 §5 重新核对。

## 1. 用户的原始要求

「客户端也改成 RPC 回调,用上 muduo 的消息 id(就是之前说这个 id 干啥的),每次 request 自己塞回调函数,不用专门的 Handler 了;Handler 是工具生成的,工具生成这些出特别的细节让 Codex 去做,你按最标准的来。」

解读与已定方案:客户端 `ClientRequest.id` / 服务端回显 `MessageContent.id` 就是 muduo `RpcMessage.id` 的同位字段。客户端每次请求分配非 0 请求号并登记自己的回调,回包只按 id 认领、回调恰好一次;单向消息与推送 `id = 0`。生成器改造(用类型化调用桩取代每消息 Handler)只出细则交 Codex 实施(设计 §5),不由 Claude 改生成器源码。

## 2. 已完成的部分与提交状态

全部**未编译、未测试**(AGENTS §10.1,执行交给 Codex)。

| 部分 | 内容 | 提交状态 |
|---|---|---|
| 服务端 gate | 直连模式 gRPC 成功回包回显请求 id 与原 message_id,失败改回带 id 的 1003 信封;`rpc_reply_bridge.{h,cpp}`、`client_session_handler.cpp`、`router_muduo_forward.cpp` | 已随用户快照 `85a457a55` 入库。注意:`client_session_handler.cpp` 现在工作区里另有**别的会话**的未提交改动,与本任务无关 |
| Unity 运行时 | `Net/RpcStatus.cs`(`RpcStatus` / `RpcDone` / `IRpcChannel`)、`Net/OutstandingCalls.cs`、`GameClient.cs` / `BattleDirectLink.cs` 改造、`OutstandingCallsTests`(9 条)、`ActorWorldFoundationTests`、README | 客户端仓 `3e90448a` 已入库 |
| Unity 复核修复 | 设计 §10 的 4 条客户端问题:断线异常安全、直连换局立即失败在途调用、直连回调异常报红、测试 TearDown 亮出被吞断言;`BattleClient.cs` 取消排队注释订正 | **客户端仓工作区未提交**:`Assets/Scripts/Game/{GameClient.cs,Battle/BattleClient.cs}`、`Assets/Scripts/Net/BattleDirectLink.cs`、`Assets/Tests/EditMode/{Net/OutstandingCallsTests.cs,Battle/BattleDirectLinkTests.cs,Battle/DirectRoutingBattleTransportTests.cs}` |
| robot 运行时 | `pkg/client.go` 的 `Call` / `CallSync` / 在途表、`SendRequest` 改发 id 0、`pkg/client_test.go` 14 条、`smoke_rpc_call.go`、friend 迁移 | 已随用户快照 `53361d055` 入库 |
| robot 场景迁移 | guild(含 guild_economy / guild_activity)、team(含 team_follow / team_backfill)、trade 迁到 `smokeCaller`;`pkg/client_test.go` 补 3 行 | **本仓工作区未提交**:`robot/{guild_smoke_scenario.go,team_smoke_scenario.go,team_follow_smoke_scenario.go,trade_smoke_scenario.go,pkg/client_test.go}`。由会话「robot 客户端改为按请求号回调」完成,它在写本文时还有一个**后台复核工作流在跑**,说过跑完会修问题并在 `docs/handoff/` 另写 robot 交接;换账号后该工作流可能中断,接手时先看 `docs/handoff/` 有没有它的 robot 交接,没有就以设计 §6.1 为准。**已写**:[client-rpc-callback-robot-handoff-20261019.md](client-rpc-callback-robot-handoff-20261019.md)(复核结论、逐处待迁清单、迁移陷阱) |
| 文档 | 设计 `client-rpc-callback.md`(新);`client-request-ordering.md` §4.1 补注;`docs/README.md` 登记;PROGRESS 第一条 | 已入库。设计文档的 §3 订正、§4 复核修复行、§6.1(robot 会话写)、§7 R6、§8、§9、§10,以及 PROGRESS 第二条「语义复核与修复」**在工作区未提交** |

## 3. 接下来要做的(按顺序)

1. **Codex 验证现有改动**(设计 §9):① `msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64`;② Unity EditMode 全量(重点 `OutstandingCallsTests`、`ActorWorldFoundationTests.MovementEmptyReplies_…`、`BattleDirectLinkTests`(含新增 `Call_FailsImmediately_WhenNewAssignmentReplacesTheConnection`)、`DirectRoutingBattleTransportTests`);③ robot `go build / vet / test -race ./pkg/...` 与 friend / guild / team / trade 四个冒烟(gate 须是含 §3 修复的新编译版);④ 直连 / 路由两种模式各一次客户端冒烟。
2. **Codex 实施生成器改造**(设计 §5,细则已写全):方法级 option `OptionClientRpcKind`(REQUEST / ONE_WAY / PUSH / SERVER_ONLY,先核对 410001 未占用)并标注 proto;生成期校验;Unity 产物改为每服务一个 `Net/Generated/Rpc/<Service>Rpc.cs`(静态方法、不用扩展方法),删 `Handlers/` 与 `HandlerRegistry.cs`,整目录重写、确定性 `.meta`;配置两份 yaml 同改;Go 单测;重生成后按 §5.6 清单迁移 12 个文件里的 `XxxHandler.MessageId`。robot 生成器(§5.5)可放第二步。跑生成写客户端仓,本任务已获客户端授权。
3. **robot 剩余迁移**(设计 §6.1 迁移进度表):mail / rank 的同形 call → 其余等待帮助函数 → 依赖生成 handler 副作用的 pet / attribute / features / equip 最后迁;登录与重定向重登录的同步握手不迁。
4. 视拍板结果做 §4 里的 Java 对齐与 R1。

## 4. 待用户拍板(未决,别自行决定)

| 项 | 问题 | 建议 |
|---|---|---|
| **R6** Java Empty 应答不回包 | Java gate `ClientDispatcher.replyToClient` 对应答为 Empty 的后端请求(148 CancelQueue,154 / 156 推送占位)tip 为 0 时不回包(Java match-spec §8.1 / M4 有意为之);C++ 两种模式都回。连 Java 时 Unity 非全局取消排队只能 15s 超时、弹「取消排队失败:rpc timeout」(改造前同样如此,不是本次引入) | Java 改为「请求号非 0 就恰好回一包(Empty 回空 body),id 为 0 才不回」;连带改 `MessageRoutesTest`、`MatchBackendForwardingTest`、game-robot `BattleSmokeScenario` / `BattleCrossZoneScenario` / `RobotOptions` 的「148 不回包」断言与说明、Java `architecture.md:109,971` 与 match-spec §8.1 / M4,PARITY 登记一行。用户已看到这个问题,**尚未回答** |
| **R1** scene 派发前早退不回包 | `scene_handler.cpp` 11 处早退,RPC0 照常回 RESPONSE 但无 `message_content`,gate 丢弃,客户端回调只能 15s 超时(Java scene 侧同类) | gate 在「RPC 成功但无 message_content 且内层响应非 Empty」时回带 id 的 1003;客户端可见行为变化,两版同批 |
| PARITY 跨仓写入 | 需在 Java 仓 `PARITY.md` 登记本功能一行,并订正已过时的第 170 行(「ClientForward 不带关联号,Java 待做」) | 用户同意后写;Java 仓 `PARITY.md` 工作区另有别的会话的未提交改动,写时只追加本行 |

## 5. 接手前必做与已知坑

- **重新核对工作区**:`git status` 两个仓(本仓、`D:\luyuan\mmorpg-client`)+ Java 仓;本表「未提交」的文件随时可能被用户整批快照提交,先 `git log -1 -- <文件>` 再下结论。别人的未提交改动混在同一工作区(本仓 gate、多份 design 文档;Java 仓 `PARITY.md`),只认本表列出的文件。
- **不要自己跑构建 / 测试 / 生成**(AGENTS §10.1、用户 2026-10-19 明确分工);给 Codex 的命令照设计 §9。
- **默认生成配置会写客户端仓**:本任务已获客户端授权,可以用默认 `proto_gen.yaml`;其他任务仍用 `proto_gen.no-unity.yaml`。
- **旧字符串错误契约要保留**:`IBattleTransport.Call` 的 onError 字符串(`"rpc timeout"`、`"server tip=N"`、`"link: …"`、`"disconnected"`、`"not connected"`、`"send failed: …"`、`"parse response: …"`)被 8 个业务模块与大量测试按前缀 / 等值判断,唯一翻译点是 `RpcStatus.ToLegacyError`。改文案前先改所有判断处。
- **单向消息改发 `id = 0`** 是客户端可见变化(服务端对 id 无特殊处理,已核对 gate / 路由服 / battle / Java);服务端对单向消息的错误信封也带 id 0,走 message_id 派发。
- **回调异常会被在途表捕获**:测试里在回调中写 `Assert.Fail` 必须配 `onCallbackException` 收集 + `[TearDown]` 断言(Unity 内置 NUnit 3.5 不记录被捕获的断言)。
- 新增的三个 Unity `.meta`(`RpcStatus.cs.meta`、`OutstandingCalls.cs.meta`、`OutstandingCallsTests.cs.meta`)是手写的随机 GUID,已入库,不要重生成。
- 客户端仓 `autocrlf=true`,改过的文件工作区是 LF,git 提示「LF will be replaced by CRLF」无害。
- 额度:并行工作流会打满 5 小时窗口(见记忆「会话额度与 workflow 扇出」),落码类代理按每个约 5% 估算,分批跑。

## 6. 本任务的工作流记录

- 只读摸底 `wf_fcd259cb-0bb`(5 个代理:服务端回显审计、Unity、robot、生成器、Java),结论见设计附录 A。
- 语义复核 `wf_447ef09d-9bf`(3 维度查找 + 每条 2 个对抗复核,5 条发现全部成立,大厅维度无发现),结论见设计 §10。
