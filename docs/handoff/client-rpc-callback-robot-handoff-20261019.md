# 交接:robot 请求自带回调(按 MessageContent.id 认领回包)(2026-10-19)

> 写给换账号后接手 **robot 部分**的会话。整个功能(gate / Unity / 生成器 / 待拍板项)的总交接见 [client-rpc-callback-handoff-20261019.md](client-rpc-callback-handoff-20261019.md)。事实源是设计文档 [client-rpc-callback.md](../design/client-rpc-callback.md):robot 部分在 §6 与 §6.1,验证步骤在 §9 第 5 步。
> 本文只讲 robot:做到哪、哪些没提交、怎么验、接着迁什么、迁的时候会踩什么坑。
> 写作时间 2026-10-19 10:50(America/New_York),本仓 HEAD `c63b22dca`。同一工作区有多个会话并行,接手前先按 §7 重新核对。

## 1. 现状

全部**未编译、未测试**(AGENTS §10.1,执行交给 Codex)。

| 部分 | 文件 | 提交状态 |
|---|---|---|
| 运行时 `Call` / `CallSync` / `LateReplies`、在途表、`SendRequest` 改发 id 0、收包按 id 认领、`Close` / `SwapConn` 让在途调用失败 | `robot/pkg/client.go`;`robot/pkg/redirect.go`(只改注释) | 已入库。是用户的批量快照 `53361d055` 带进去的,不是本会话提交的 |
| 单测 14 条 | `robot/pkg/client_test.go` | 初版在 `53361d055`;工作区另有 +3 行未提交(并发用例等 Close goroutine 退出) |
| 四个场景共用的 `smokeCaller` | `robot/smoke_rpc_call.go`(新) | `53361d055` |
| friend 迁移 | `robot/friend_smoke_scenario.go` | `53361d055` |
| guild / team / trade 迁移;team_follow 构造改为注入 `caller` | `robot/{guild_smoke_scenario,team_smoke_scenario,trade_smoke_scenario,team_follow_smoke_scenario}.go` | **工作区未提交** |
| 复核确认的 5 处陈旧注释订正(只改注释,§3) | `robot/{guild_smoke_scenario,team_follow_smoke_scenario,guild_economy_smoke,team_backfill_smoke_scenario,mail_smoke_scenario,ordering_smoke_groups}.go` | **工作区未提交** |
| 文档 | 设计 §6 标题、§6.1 实施记录、§9 第 5 步;`docs/PROGRESS.md` 条目「Claude:robot 请求自带回调」;本文 | 设计初版和 PROGRESS 第一条已入库;以上改动在工作区 |

本功能在工作区未提交的 robot 文件共 9 个(都在 `robot/` 下)。提交时只认这 9 个,外加设计文档 `docs/design/client-rpc-callback.md`、本文,以及 `docs/README.md`、`docs/PROGRESS.md` 里本功能那几行;其余改动是别的会话的:

- `guild_smoke_scenario.go`
- `team_smoke_scenario.go`
- `trade_smoke_scenario.go`
- `team_follow_smoke_scenario.go`
- `team_backfill_smoke_scenario.go`
- `guild_economy_smoke.go`
- `mail_smoke_scenario.go`
- `ordering_smoke_groups.go`
- `pkg/client_test.go`

**HEAD 不是坏的**:HEAD 的 `SendRequest` 已改发 id 0,但 guild / team / trade 在 HEAD 上还是旧的按消息号轮询,照样能按 message_id 收到回包。工作区迁移即使暂不提交,也不会让 HEAD 编不过或跑不通。

## 2. 验证(给 Codex,按顺序;任何一步失败保留输出并停下,不要连续重试)

工作目录 `robot/`,不需要额外环境变量。

1. `go build -mod=vendor ./...`
2. `go vet -mod=vendor ./...`
3. `go test -mod=vendor -race ./pkg/...`
   通过标准:全绿,无 `DATA RACE`。重点看 `TestCallDoneExactlyOnceUnderConcurrentOutcomes`(10s 内完成)和其余 13 条 `TestCall*` / `TestSendRequest*` / `TestSwapConn*` / `TestReply*` / `TestClose*`。
4. `go test -mod=vendor ./...`:确认主包既有单测(`battle_direct_conn_test.go`、`pkg/redirect_test.go` 等)不受影响。
5. 冒烟:在 `robot/` 下执行 `.\robot.exe -c etc/friend_smoke.yaml`、`etc/guild_smoke.yaml`、`etc/team_smoke.yaml`、`etc/trade_smoke.yaml`;`etc/team_follow_smoke.yaml` 可选加跑,因为本次改了它的构造。
   - 前置条件:各 yaml 文件头列出的服务就绪(两区 gate 以 `GATE_CLIENT_RPC_ROUTER=1` 启动等)。直连模式下 gate 必须是含 `85a457a55`(直连回包回显请求号)的新编译版,否则回包不带请求号,调用只能超时。
   - 通过标准:各自输出 `*_SMOKE_OK`、退出 0,日志里 `reply without request id dropped` 和 `[rpc] late reply dropped` 都为 0。
   - 失败时保留 robot 输出中的 `*_SMOKE_FAIL step=… reason=…` 行,以及同窗口的 gate、路由服日志。

失败的快速定位:

- **出现 `reply without request id dropped`**:服务端没回显请求号。先查 gate 是否为新编译版。
- **friend 第 6 步 / guild 第 9 步拿不到信封拒绝**:看路由服是否把上游 PermissionDenied 翻成了 1003 并带回 id。
- **trade 第 2 步**:期望的就是超时,即 gate 丢弃 TradeAdmin 消息号。

## 3. 静态复核结论(工作流 `wf_5cd420ca-79e`,只读,不代替编译)

做法:两个维度查找(`pkg` 运行时与单测;`main` 包的 smokeCaller 与场景迁移),每条发现再派一个复核者尝试驳倒。另有一路盘点剩余迁移点(§5),一路核对功能整体状态。

- **pkg 维度:0 条成立,2 条被驳倒。**
  - 「SwapConn 不看 Close 的终态」:与 redirect 竞争时会留下新连接。但这是改造前就有的行为,本次没有变坏;终态只约定 `Call` 一律 NotSent,这一点仍然成立。
  - 「并发用例对 CallSync 只数返回次数」:各完成路径共用 `finishCall` / `completeAll`,320 个 `Call` 已经覆盖了恰好一次。
  - 查过且干净的:标识符与导入、muduo API、双 `%w`、vet 各分析器、gofmt 对齐、锁顺序(只有 `callMu → connMu`)、恰好一次、回调不在锁内、单测确定性,以及本机监听不 Accept 时 muduo 能连上且 Close 及时返回。
- **main 维度:5 条成立,全是陈旧注释**(迁移后仍写「按消息号认领」),已订正,见 §1。

## 4. 运行时契约速记(继续迁移时必须遵守)

- **`Call` 只用于恰好回一个包的方法。** scene TCP 路径上响应恰好是 `Empty` 的方法是单向的,gate 不回包,用 `Call` 必然超时:`MoveStart` / `MoveStop` / `MoveSync`、`SceneInfoC2S`、`GetSyncDigest`。这类保留 `SendRequest` 加推送等待。gRPC 路径(路由服 / 直连)的 Empty 响应照常回包,可以用 `Call`。
- **带请求号的回包不进 onMessage,生成的 handler 也就不会跑。** 凡是等「生成 handler 写进 Player 的状态」的(面板 / 列表序号、FeatureResponse、货币 ready 通道、chatInbox、技能就绪),迁到 `Call` 后都必须改成直接解析 `Call` 的结果,或者在 done 里手动调 handler,否则只会超时。
- **done 跑在 RecvLoop 或定时器 goroutine 上,不得阻塞。** `CallSync` 禁止在 RecvLoop goroutine 上调用,包括消息处理器、done 回调和重定向重登录(`pkg/redirect.go` 的 `ReloginFunc`)。
- **断线:** `Close` 是终态,在途调用以 `ErrRPCDisconnected` 失败,之后的 `Call` 一律 `ErrRPCNotSent`。`SwapConn` 只让旧连接上的调用失败。vendored muduo 被对端断开后会静默重拨、不报错,这种情况只能等超时。
- **场景 onMessage 的写法(照 friend / guild / team / trade):**
  - 请求消息号上 id = 0 的消息交给 `smokeDropUncorrelatedReply` 丢弃,不交 `handler.MessageBodyHandler`:信封拒绝会被解成全零响应,看上去像受理。
  - 收到 `SendTipToClient` 时调 `caller.onGateTipMessage(msg)`,让 gate 推 kServiceUnavailable 时快速失败。
  - 回包的 `MsgRecv` 计数已移进 `smokeCaller`。
- **`smokeCaller` 同一时刻只服务一次等待。** 这与「一个机器人只由一个 goroutine 驱动」的约定一致。要在同一连接上流水线发多条(ordering 冒烟),直接用 `gc.Call` 各自登记回调。
- **业务幂等键不能拿信封 id 代替**:一次操作一个键,所有重发共用(设计附录 A 末条)。

## 5. 剩余迁移清单(按设计 §6 第 4 步的顺序;行号取自 2026-10-19 工作区)

### A. 同形 call(下一步做,可直接换成 `smokeCaller`)

| 位置 | 说明 |
|---|---|
| `robot/mail_smoke_scenario.go:731` `mailSmokeBot.call` | 有 `awaiting` 兜底和 `gateTips` 快速失败。兜底只挡住两次调用之间到达的迟到回包;上一次超时请求的回包若在下一次同号调用期间到达,仍会被误认。第 7 步对推送号 `NotifyMailChanged` 本身发请求,迁后靠 id 区分。onMessage 认领 8 个邮件号,迁移时保留推送分支,其余改 `smokeDropUncorrelatedReply`。上层 `rpc` / `expect` 经它走 |
| `robot/rank_smoke_scenario.go:439` `rankSmokeBot.call` | 只处理 GetRankPage,`awaiting` 的局限同 mail。没有 SendTipToClient 快速失败,`honourSpacing` 控制间隔 |
| `robot/sync_smoke_scenario.go:767` `syncSmokeBot.friendRpc` | 与 friend 迁移前的 call 同形,但基于留底 records,没有 `awaiting`。调用方 `syncSmokePreclean`、`rejectPendingFrom`、`pendingRequestIDsFrom` |

### B. 其余等待帮助函数(按消息号从留底里认领,都没有迟到防护)

- channel:`channel_smoke_scenario.go:560` `requestEnterScene`(EnterScene 同步应答);mystery_merchant `mystery_merchant_smoke_scenario.go:90` `merchantCall`(经 `channelSmokeBot.send`)。
- travel:`travel_smoke_scenario.go:885` `expectTravelRejected`、`:680` `travelTo`。travelTo 正常路径回包可有可无,旧连接会被关。
- sync:`sync_smoke_scenario.go:891` `travelTo`,与 travel 同形。
- ordering:`ordering_smoke_scenario.go:297` `waitReplies`,以及调用它的 `:346` `pipelineJoinThenCancel`、`:377` `queueState`、`:397` `cancelWithTicket`;`ordering_smoke_groups.go:693` `pipeline`、`:717` `rpcTip`。流水线场景迁移时用多个并发 `gc.Call`,各自按 id 认领。O7 组在 RecvLoop 已启动的连接上连发 Login → EnterGame,不是同步握手,也可以迁。
- team:`team_smoke_scenario.go:663` `send` 配合 `:949` `teamSmokeWaitPush`,用于 JoinQueue / CancelQueue / GetQueueStatus;`:808` `startSoloBattle`;`team_backfill_smoke_scenario.go:228`、`:364` `backfillCancelQueue`、`:378` `backfillExpectQueueState`。换场景(`:768` `switchScene`)等的是 NotifyEnterScene 推送,可以不迁。

### C. 依赖生成 handler 副作用的等待(最后迁,按 §4 第二条改写)

- attribute:`attribute_smoke_scenario.go:518` `send`、`:442` `callTolerant`、`:530` `waitSuggestion`。
- pet:`pet_smoke_scenario.go:506` `send`、`:453` `callTolerant`、`:485` `callAttributePanel`、`:517` `waitSuggestion`。
- equip:`equip_smoke_scenario.go:632` `call`、`:756` `attributePanel`。equip_forge、equip_forge_topup、asset_arrival_prepare 都复用它们。
- features:`features_smoke_scenario.go:417-423` `call` / `callVia`;`features_battle_smoke.go:174` `callDirect`(直连)。
- 货币:`currency_crash_window_scenario.go:51` `readGoldBalance`、`:78` `gmAddGold`。attribute、pet、equip_forge、travel、currency 多处调用;读不出余额时分不清是余额为 0 还是被拒。
- chat:`chat_smoke_scenario.go:383` `send`、`:404` `sendChat`、`:423` `pullHistory`,都等 chatInbox。
- channel:`channel_smoke_scenario.go:490` `requestSceneInfo`。SceneInfoC2S 是单向,等的是推送,保留。
- battle 大厅侧:`battle_smoke_scenario.go:313`(JoinQueue)、`:182`(WatchBattle);`battle_smoke_cross_zone_scenario.go:244`(rank 冒烟也复用)。回包只打日志,等 WaitBattleStart / WaitBattleAssigned。
- 压测主循环与登录测试:`main.go:541`(ListSkills → WaitSkillsReady);`login_test_scenarios.go:1123` `prepareBehaviorClient`、`:1154` `testSkillCast`、`:1197` `testSceneSwitch`。

### D. 不迁:同步握手,跑在 RecvLoop 之前或之上,只能 `SendRequest + RecvOne`

- `login.go:138` `sendAndRecvTimeout`
- `main.go:880` `sendAndRecvLocal`,以及 `:83` 注册的重定向重登录
- `features_smoke_scenario.go:373` `featureAuthCall`
- `travel_smoke_scenario.go:503` `travelSmokeStrictRelogin`
- `pkg/redirect.go:184` `followRedirectLocked`
- `login_test_scenarios.go` 的 `:434` / `:646` / `:704` / `:894` / `:931`(异常登录用例,故意不读回包或用 RecvOne 数回包)

### E. 不必迁:发出即走,或单向

移动上报、直连 `SetAutoBattle`(`team_smoke_scenario.go:874`、`battle_smoke_scenario.go:357`、`battle_smoke_cross_zone_scenario.go:278`、`guild_activity_smoke.go:735`)、LeaveGame / Disconnect 收尾、`data_stress.go`、`logic/ai/robot_ai.go` 的 AI 循环、`login.go:268` 已弃用的 TCP RefreshToken。

### 迁移陷阱(迁之前先看)

- `battle_direct_conn.go:181` 补拉 GetBattleState 的回包由 `record()` 计入 `stateReplies`(`:222`),`battle_smoke_scenario.go:378` 与 `battle_smoke_cross_zone_scenario.go:300` 断言它 ≥ 1。改成 `Call` 后回包不经 onMessage,这两处断言会失败,要一起改。
- travel 的 `replyRejection`(`travel_smoke_scenario.go:627` 起)依赖回包既进留底又走通用分发。
- `login.go:268` 的 RefreshToken 靠生成 handler `ClientPlayerLoginRefreshTokenHandler` 写回新 token。若迁到 `Call`,要在 done 里自己写。
- 设计 §6.1 的进度表把 chat、currency_crash_window、battle_smoke 大厅侧、channel 的 requestSceneInfo 归为「其余等待帮助函数」。按它们实际等待的对象看,属于 C 类,以本文为准;§6.1 表已指向本文。

## 6. 顺带发现的既有缺陷(本轮范围外,未修)

- `main.go:879` `sendAndRecvLocal` 不检查信封 `error_message`,空 body 会被当成成功(`login.go` 的 `sendAndRecvTimeout` 有这道检查)。
- `login.go:138` `sendAndRecvTimeout` 与 `login_test_scenarios.go:683` `testDuplicateLoginRequest` 超时后留下阻塞在 `RecvOne` 的 goroutine,之后会和 RecvLoop 抢 `Recv`。
- §5 B 类的留底认领、mail / rank 的 `awaiting` 兜底,都可能把上一次超时请求的迟到回包当成本次结果。迁到 `Call` 即修。

## 7. 接手前必做与已知坑

- **重新核对工作区**:`git status --short robot docs` 加 `git log -1 -- <文件>`。用户会定期整批快照提交(「补充…」「合并…」),本表「未提交」的文件随时可能已入库:`53361d055` 就带走了本功能前半部分,`fc3ff045e` 合入了 mail / rank / team_follow / team_backfill 等新场景。
- **不要自己跑构建 / 测试 / 冒烟**(AGENTS §10.1),命令照 §2 交给 Codex。
- PROGRESS 里 Codex 的对账条目已注意到「其他工作正在修改 robot 的 guild/team/trade 冒烟源码」。Codex 准入前应审实际 diff,而不是只看描述。
- 提交只在用户发话时做,且只提交 §1 列出的文件。
- `smokeCaller` 本身没有单测,靠四个冒烟验证。
- 记录:运行时实现与迁移在会话「robot 客户端改为按请求号回调」完成;只读复核工作流 `wf_5cd420ca-79e`;上一级总交接见文首链接。
