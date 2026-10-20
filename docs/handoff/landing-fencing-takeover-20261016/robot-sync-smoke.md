<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**summary**: sync-smoke（批次 F，robot/sync_smoke_scenario.go 等文件均已在 de4960bee 入库，工作区里这些文件没有在途改动）对设计 §10 第 6 步的四个环节逐条都有对应：登录后取摘要对应 S1；好友申请只让对方的 FRIEND_REQUEST 恰好 +1、对方再取只变这一个域对应 S3；短重连后 g 不变对应 S4，而且 S4 比设计更严，g 和各域计数都要求不变。S2 的播种与基线是必要补充：新号没有 hash，不播种的话 S3 的「g 不变」就没有意义。S5（跨 zone）可选，S6 是设计之外的加项。逐条核对后，没有发现断言失败后仍打印 SYNC_SMOKE_OK 的路径：每个断言失败都会走 fail()，即 os.Exit(1)；只有走到末尾才打 OK，并经 main 的 return 以退出码 0 结束。253 的请求间隔按 1.1 秒节流，跨重连仍然保留，S6 探针也走同一节流。253 和 43 都不在 generated/tables/messagelimiter.json 里（共 81 行），所以实际生效的是 MessageLimiter 的默认档：3 次，窗口 1 秒，但按整秒淘汰，实际是「当前整秒加上一整秒」合计 3 次。1.1 秒的间隔保证每整秒最多 1 次，缓冲区里最多 2 条，不会被限流。账号 robot_9911 / 9912 与其它冒烟账号及压测 account_fmt 的范围都不冲突。实现者自述的三处偏差都不影响冒烟有效性，建议接受，但要改文档口径；跨 zone 腿建议在双 zone 环境里补跑一次。另外发现 6 个 low / nit 级问题，都不会造成假绿：预清理吞错，加上 friend 对已有 pending 申请的「同代次重放」，会让 S3 的失败归因到错误的方向；S6 可以补几条与时序无关的硬断言；共享 helper 里 VerifyGateToken 没有超时，gate 不可达时会永久挂起；重连后旧 RecvLoop goroutine 永久阻塞，属于泄漏，但进程寿命很短、影响可忽略；253 收到信封 1003 时，诊断文案错指成 friend 路由服；限流档位与短重连时序的文档描述不够精确。本次复核只做静态阅读，未编译、未运行。
## findings
  - [0]
    **id**: F1
    **severity**: low
    **title**: 预清理吞错，加上 friend 对已有 pending 申请的同代次重放：S3 失败时归因错误，残留不能自愈
    **location**: robot/sync_smoke_scenario.go:744-764（syncSmokePreclean）、768-775（rejectPendingFrom 失败只打 WARN）、277-280（S3-add-friend 的提示文案）、298-303（HEAD）；go/friend/internal/data/friend_repo.go:419-427（HEAD）
    **detail**: 场景：上一轮停在 S3 之后，B→A 还有一条 PENDING 申请。本轮 S2 预清理里 A 的 GetPendingRequests 超时，或被信封拒绝（1003 / 1008），friendRpc 返回的 err 只记一行 WARN，残留的 pending 没被拒掉。到 S3，B 调 AddFriend(A)：friend_repo.CreateRequest 发现同方向已有 pending 且 request_id≠0，就原样返回同一个 request_id，Changed=false，并不报 ErrRequestAlreadySent。于是 AddFriend 回 tip=0、request_id≠0，却不 bump。S3 会在 5 秒预算内重取约 4 次，最后以「S3-only-friend-request ... did not change」失败。这不是假绿，但 S3-add-friend 的提示说的是「残留的 pending 会导致申请被拒」，与实际行为（受理并重放）相反；失败原因只写在 S3-only 那条信息里，而且只在 DEBUG 日志里才看得到预清理失败了。这类失败每轮都会复现，直到人工清理为止。
    **suggested_fix**: 1）syncSmokePreclean 改为返回 error。传输层失败（friendRpc 的 err：信封拒绝、超时、gate 推 1003）一律按 fail("S2-preclean") 处理，业务 tip 仍然不断言。2）rejectPendingFrom 改为返回 (rejected int, residualIDs []uint64, err error)：拒完后再调一次 GetPendingRequests，确认没有来自 B 且仍为 PENDING 的申请，否则让 S2 失败。S7 继续尽力而为、忽略 err。3）S3 在受理后加一条：如果 added.GetRequestId() 属于预清理时见过的残留 pending id，就直接 fail("S3-add-friend")，原因写「friend 对已有 pending 做同代次重放、不 bump，预清理没有清掉残留申请」。4）把 277-280 行的提示改成「已是好友或被拉黑会被拒；已有 pending 会被受理但不 bump，见 S3-only 步骤」。
  - [1]
    **id**: F2
    **severity**: low
    **title**: S6 探针只记录不断言：方向可以接受，但漏掉了几条与时序无关的硬判据
    **location**: robot/sync_smoke_scenario.go:382-397（S6 判定）、1012-1044（syncSmokeClassifyProbe）；robot/sync_smoke_scenario_test.go:315-358（HEAD）
    **detail**: 设计 §10 第 6 步不包含 S6，§4.2 的「载入中丢弃」由代码证实：scene_handler.cpp 在途 241-259 / HEAD 340-354 在实体未建好时静默 return；gate 对应答为 Empty 的 253 是单向发送（client_message_processor.cpp:833-839），scene 不会回包。实体是否已经销毁、gate 是否已绑定 scene，都取决于时序，所以 dropped / replied_ok / replied_unavailable / envelope_rejected(1003) 都是合法结局，只记录是合理的。但下面三种结局与时序无关，现在也都放行了：（a）outcome=undecodable：254 解不开就是契约破坏，无论时机如何都应失败；（b）replied_ok 时摘要内容从未校验，scene 回了一份错误的摘要（比如 g 变了）也会通过，而紧接着的 d6 只验了下一份；（c）envelope_rejected 且 tip=1008（kRateLimitExceeded），说明冒烟自己的节流坏了，应当失败，不应只是一条 OBSERVE。另外，S6 的 kicked 失败信息把 KickPlayer 归因于探针，实际上也可能来自交接 §3.3 a 的登录替换竞态（login 只按 session_id 踢线）。
    **suggested_fix**: 保留只记录的总体口径，补三条硬断言：1）probe.outcome == undecodable → fail("S6-probe")；2）syncSmokeProbe 增加一个字段，保存已解析的 *scene.SyncDigestS2C。outcome==replied_ok 时，先过 syncSmokeParseDigest，再与 last 做 syncSmokeCheckUnchanged，不一致就失败。注意：加了指针字段后结构体不再可比较，测试里的 got != c.want 要改成逐字段比较；3）envelope_rejected 且 envelopeTip == tipSyncRateLimited → fail，原因写「节流失效」。kicked 的失败文案改成「重登后收到 KickPlayer（可能是探针，也可能是 login 按 session_id 踢线的竞态，见 handoff §3.3 a），对照 gate / login 日志」。在 sync_smoke_scenario_test.go 里为这三条各补一个用例，包括 replied_ok 时内容不一致的反例。
  - [2]
    **id**: F3
    **severity**: low
    **title**: connectAndVerify → VerifyGateToken 无超时：gate 不可达时冒烟永久挂起，不会输出 SYNC_SMOKE_FAIL
    **location**: robot/pkg/client.go:207-237（VerifyGateToken 阻塞在 c.Recv() 上，没有截止时间）；robot/login_test_scenarios.go:259-273；robot/vendor/github.com/luyuancpp/muduoclient/muduo/connection.go:56-104（拨号失败后每秒重拨、永不放弃）、Recv() 是无超时的 channel 读；调用点 robot/sync_smoke_scenario.go:422、510（全部 HEAD）
    **detail**: 场景：S4 或 S6 已经执行 old.Close()，这时 gate 进程崩溃或端口不通。pkg.NewGameClient 立即返回，因为 muduo 是异步拨号。connectAndVerify 等 200ms 后发 ClientTokenVerifyRequest，消息进了 outgoing 缓冲，但连接一直建不起来。VerifyGateToken 的 c.Recv() 读的是 incoming channel，永远不会关闭，于是主 goroutine 永久阻塞，进程既不打 FAIL，也不退出。其它步骤都有界：sendAndRecv 15 秒、waitSceneReady 15 秒、friendRpc / waitDigest 10 秒、travelTo 60 秒。只有这一处无界。它不会造成假绿，但 CI 或 Codex 只会看到挂起。
    **suggested_fix**: 不改共享 helper，因为别的场景依赖它。在 sync_smoke_scenario.go 加一个本地包装 syncSmokeConnect(host, port, account, payload, sig)：在 goroutine 里调用 connectAndVerify，结果写进带缓冲的 channel；主流程用 select 等结果，超时时间用 syncSmokeSceneReadyTimeout（15s）。超时就返回 error，并另起一个 goroutine，等 connectAndVerify 迟到返回后对拿到的 gc 调 Close。syncSmokeLogin（第 422 行）和 reconnect（第 510 行）都改用这个包装。
  - [3]
    **id**: F4
    **severity**: nit
    **title**: 重连后旧连接的 RecvLoop goroutine 永久阻塞（泄漏）
    **location**: robot/sync_smoke_scenario.go:447、502-503、531；robot/vendor/github.com/luyuancpp/muduoclient/muduo/connection.go 的 Close()（只关 outgoing，不关 incoming）；robot/pkg/client.go:109-117 的注释已说明这一点（全部 HEAD）
    **detail**: reconnect 在主 goroutine 上执行 old.Close()，旧 gc 的 RecvLoop 正阻塞在 <-incoming 上，而 incoming 永不关闭，所以这个 goroutine 永不退出。它会一直持有旧 gc、旧 Connection 和 bot.onMessage 闭包。每轮泄漏 2 个（S4、S6），失败路径上 syncSmokeLogin 第 453 行也一样。冒烟进程寿命很短，结束就退出，实际影响可以忽略。理论上的风险是：如果旧 incoming 里还有没消费的消息，旧 RecvLoop 会用 PlayerList.Get(homePlayer) 把它分发给新的 Player（MessageBodyHandler 按 client.PlayerId 查表）。但 Close 之后 readLoop 就停了，残余消息会在 ≥250ms 的重连窗口内被瞬间消费掉，不会被新请求的 mark 误认领。
    **suggested_fix**: 接受，不改。如果要更干净：在 reconnect 的注释里写明「旧 RecvLoop 会泄漏一个 goroutine，原因见 pkg/client.go SwapConn 注释」。真要根治得改 vendored muduo，让 Close 时关闭 incoming，但这超出本批范围，也会影响其它场景，不建议。
  - [4]
    **id**: F5
    **severity**: nit
    **title**: 253 收到信封 1003 时，诊断文案错指成 friend 路由服
    **location**: robot/sync_smoke_scenario.go:655-660（waitDigest）、1088-1095（syncSmokeTipHint）（HEAD）；依据为 cpp/nodes/gate/handler/rpc/client_message_processor.cpp:788-811（文件有在途改动，但这几行与 HEAD 行号一致）
    **detail**: 253 走 gate 的 HandleTcpNodeMessage。会话还没有绑定 scene 节点（Scene not ready），或者 RpcClient 还没挂上时，gate 回带请求 id 的信封 1003。253 是单向发送，不会出现调用失败的 1003。但 syncSmokeTipHint(1003) 输出的是「the router has no friend instance, or a friend-side storage fault」，waitDigest 的主文案也只列了「gate 不认识消息号 / 被限流」，漏掉了「会话尚未路由到 scene」这一种。此外，1008 在路由服模式下还表示 lane 满（见 login.go 中 sendAndRecvTimeout 的注释），提示里只写了限流。这只影响排障，不影响判定。
    **suggested_fix**: syncSmokeTipHint 改为接收消息号：syncSmokeTipHint(messageId, tip uint32)。对 253 与 1003，提示「gate 上该会话尚未绑定 scene 节点（Scene not ready）或 scene RpcClient 未挂上，看 gate 的 [TCP Node] WARN」；对 friend 消息保持现有文案；1008 的提示补上「或路由服 lane 满」。waitDigest 的主文案补充这一原因。
  - [5]
    **id**: F6
    **severity**: nit
    **title**: 短重连的实际时序与 yaml / 文件头描述不一致（自述偏差 1）：不影响有效性，只需改口径
    **location**: robot/etc/sync_smoke.yaml:8；robot/sync_smoke_scenario.go:14（文件头）与 96-97（常量注释已写明 200ms）、479-482；robot/login_test_scenarios.go:259-273（HEAD）
    **detail**: 实际时序：old.Close() 后等 50ms，NewGameClient 异步拨号，再固定等 200ms，然后经过一次 VerifyGateToken 往返，才发 Login，所以 Login 比 Close 晚 ≥250ms 加 RTT。有效性不受影响，原因有三：1）gate 在 TCP 断开时会立即给 scene 发 ScenePlayerExitGame（client_message_processor.cpp:492-517），无论隔 50ms 还是 250ms，都不存在「复用旧实体」的窗口；2）摘要在 Redis 里，与实体的生死无关；3）设计 §10 第 6 步只要求「短重连后 g 不变」，S4 还额外要求各域计数不变，比设计更严。唯一的差别是对「旧会话退出与新登录竞态」的压测强度弱了一些，但那属于进场门禁的范围，不是本冒烟的目标。
    **suggested_fix**: 接受。把 yaml 第 8 行和文件头第 14 行改成「直接断开，50ms 后新建连接，固定再等 200ms 验票后 Login（Close 后约 ≥250ms 加 RTT）」，交接 §3.2 同步更新。如果确实想要真正的 50ms：在本场景加一个本地拨号函数（pkg.NewGameClient + gc.Account = ... + gc.VerifyGateToken，不 sleep；muduo 的 outgoing 有缓冲，连上之后 writeLoop 会把消息补发出去，所以不需要等），并套上 F3 的超时包装。不要改共享的 connectAndVerify。
  - [6]
    **id**: F7
    **severity**: nit
    **title**: 默认限流档的语义描述不精确，「档位同 SceneInfoC2S」在当前表里等于不配
    **location**: cpp/libs/engine/core/message_limiter/message_limiter.{h,cpp}（HEAD）；robot/sync_smoke_scenario.go:37-39、89；docs/design/landing-recovery-sync-digest.md §4.1（「每秒约 3 次」，该文件有在途改动，只动了状态行）
    **detail**: SessionInfo::messageLimiter 按会话（即每条 TCP 连接）、按消息号分开计数，用的是默认构造（3 次，窗口 1 秒）。253 和 43 都不在 messagelimiter.json 里，FindByIdSilent 返回空，因此生效的是默认档。时间用 NowSeconds，是整秒墙钟。淘汰条件是 now - ts > 1，所以一个时间戳在「当前整秒和下一整秒」内都算数，实际是「当前与上一整秒合计最多 3 次」（真实时间 1 到 2 秒），持续速率约 1.5 次/秒，并不是「每秒约 3 次」。相邻 253 至少隔 1.1 秒，就保证每个整秒最多 1 次，判定时缓冲区最多 2 条，永远不会被拒；同一机器人的其它消息号各有各的桶，互不影响。冒烟的节奏是安全的，而且跨重连、包括 S6 探针都保持着。设计要求 253「与 SceneInfoC2S 同档」，但 43 本身也没有配行，照做只是追加一行同样的默认值，不改变任何行为。
    **suggested_fix**: 不需要改代码。建议：1）把设计 §4.1 和场景注释改成「同一消息号在当前与上一整秒内合计最多 3 次」；2）是否给 253 追加限流行由用户拍板（交接 §3.5 已列出）。如果要追加，不能写「同 43」，要给出明确的 max_requests 和 time_window。无论追加哪一档，只要 max_requests ≥ 2、time_window ≤ 1，冒烟的 1.1 秒节奏都不用改。
  - [7]
    **id**: F8
    **severity**: nit
    **title**: 跨 zone 腿默认关闭（自述偏差 3）：可以接受，但验收时应补跑一次
    **location**: robot/etc/sync_smoke.yaml:62；robot/config/config.go:533-563；robot/sync_smoke_scenario.go:329-360（HEAD）
    **detail**: 设计 §10 第 6 步没有要求跨 zone。默认的 dev-start 是单 zone，双 zone 要用 dev-start-zones -Zones 1,2（见 travel_smoke.yaml 前置 1），所以默认关闭是合理的。S5 实际验证的是 CZ-2 契约（各 zone 的 zone_redis 物理上共享）在摘要键上同样成立，也就是访客区 scene 的 HGETALL 能读到同一个 g。交接 §4 第 8 步已经要求回归 travel-smoke，到时双 zone 环境本来就会起，顺带跑一遍的成本很低。
    **suggested_fix**: 接受默认关闭。在交接 §4 第 8 步和设计 §10 第 6 步加一句：「双 zone 环境（dev-start-zones 1,2）起来后，再以 sync_smoke.cross_zone=true、visit_zone=2 跑一次。前提是 robot_9911 首次是在 zone_id 建的角。」
**fix_plan**: 以下改动都可选，都是 low / nit 级，不改也不影响 SYNC_SMOKE_OK 的正确性。全部只动 robot/，不碰共享 helper 和 vendor。
1. robot/sync_smoke_scenario.go
   - syncSmokePreclean(subject, peer) 改成返回 (residual []uint64, err error)。RemoveFriend 和 B 的 Unblock 只要 friendRpc 返回 err（传输层失败）就返回错误，业务 tip 照旧不断言。调用点（第 239 行）出错就 fail("S2-preclean", ...)。
   - rejectPendingFrom(from) 改成返回 (rejected int, residualIDs []uint64, err error)。GetPendingRequests 失败时返回 err；逐条拒完后再拉一次 GetPendingRequests，把来自 from 且仍为 PENDING 的 request_id 放进 residualIDs。预清理里 residualIDs 非空就失败；S7（第 400 行）忽略 err 和 residual，只打日志。
   - S3（第 281 行之后）：added.GetRequestId() 如果属于预清理第一次 GetPendingRequests 看到的、来自 B 的 pending id，就 fail("S3-add-friend", "friend 对已有 pending 做同代次重放(Changed=false)、不 bump;预清理未清掉残留申请")。第 277-280 行的提示文案改成正确的语义。
   - syncSmokeProbe 增加字段 digest *scene.SyncDigestS2C；syncSmokeClassifyProbe 在 replied_ok / replied_unavailable 时填上它。S6（第 386 行之前）补三条：outcome==undecodable → fail；outcome==envelope_rejected 且 envelopeTip==tipSyncRateLimited → fail("S6-probe","节流失效")；outcome==replied_ok → 先 syncSmokeParseDigest，再 syncSmokeCheckUnchanged(last, 探针摘要)，不一致就 fail。kicked 的失败文案补上「或 login 按 session_id 踢线的竞态(handoff §3.3 a)」。
   - 新增 syncSmokeConnect(host string, port int, account string, payload, sig []byte) (*pkg.GameClient, error)：在 goroutine 里调用 connectAndVerify，主流程用 select 等结果，超时 syncSmokeSceneReadyTimeout；超时返回 error，并在后台等迟到的结果再 Close。第 422 行和第 510 行改用它。
   - syncSmokeTipHint 改成 syncSmokeTipHint(messageId, tip uint32)：对 253 与 1003 输出「gate 会话尚未绑定 scene(Scene not ready)/RpcClient 未挂上」；1008 的提示补上「或路由服 lane 满」。waitDigest 第 655-660 行的主文案同步修改。
   - 文件头第 14 行改为实际时序：「Close 后 50ms 新建连接，再固定等 200ms 验票后 Login，约 ≥250ms 加 RTT」。
2. robot/sync_smoke_scenario_test.go
   - TestSyncSmokeClassifyProbe：结构体不再可比较，改成逐字段比较（outcome、envelopeTip、replyBeforeEnterScene、kicked，以及 digest 是否非空和其 generation）。
   - 新增纯函数测试：把 S6 的新判定抽成纯函数 syncSmokeJudgeProbe(probe, last) error，覆盖四种情况：undecodable 判失败；1008 判失败；replied_ok 但内容不一致判失败；dropped / 1003 / replied_ok 且一致时放行。
3. robot/etc/sync_smoke.yaml
   - 第 8 行改为实际时序；在用法段补一句：双 zone 环境（dev-start-zones 1,2）下再以 cross_zone=true 跑一次。
4. 文档（如果要动；docs/design/landing-recovery-sync-digest.md 有他人的在途改动，只追加，不覆盖）：§4.1 的「每秒约 3 次」改成「同一消息号在当前与上一整秒内合计最多 3 次」；§10 第 6 步补跨 zone 那一遍；交接 §3.2 记下本复核的结论。
5. 验证（由用户或 Codex 执行，Claude 不跑）：在 robot/ 下执行 go test -mod=vendor -run 'TestSyncSmoke' ./... 和 go build -mod=vendor；联机执行 .\robot.exe -c etc/sync_smoke.yaml，期望输出 SYNC_SMOKE_OK 且退出码 0。双 zone 环境再跑一遍 cross_zone=true。保留整份冒烟日志，包括 OBSERVE 行。
## verified_ok
  - [0]
    与设计 §10 第 6 步逐条对应：「登录取摘要」对应 S1（sync_smoke_scenario.go:205-236，要求 status=OK 且契约形状合法）；「好友申请让对方某域 +1、对方再取只变该域」对应 S3（270-312：B AddFriend 必须受理；syncSmokeCheckSingleBump 要求 g 不变、只有 FRIEND_REQUEST 变且恰好 +1；按客户端 §4.3 规则算出的待拉取域恰好是 {FRIEND_REQUEST}）；「短重连后 g 不变」对应 S4（314-327，syncSmokeCheckUnchanged 要求 g 与所有域计数都不变，比设计严）。没有缺失的步骤。
  - [1]
    S2 播种是必要的：新号没有 hash（g=0），不先播种的话 S3 的第一次 bump 会建 hash 并写入新 g，「g 不变」就验不了。friend 的 Unblock 无条件给调用方 bump FRIEND_BLOCK（go/friend/internal/logic/friend_logic.go:621-651，HEAD），AddFriend 只在 change.Changed 时给接收方 bump 一次 FRIEND_REQUEST（:340-345），而且 bump 在 RPC 回包之前同步完成。这些都与 S2、S3 的前提吻合；会给 A bump 的预清理操作（RemoveFriend、Reject）都排在 S2 基线之前。
  - [2]
    254 只有一个发送点，即 player_landing.cpp:64 的 PushSyncDigest（player_scene_handler.cpp:268 是服务端 NotifySyncDigest 的转发，没有调用方），所以「mark 之后的第一条 254 就是本次请求的回包」这一假设成立。
  - [3]
    §4.2 的「载入中丢弃」有代码证据：gate 对应答为 Empty 的 253 走单向 SendRequest（client_message_processor.cpp:833-839），会话未绑定 scene 时回信封 1003（:788-798）；scene 在实体未建好时静默 return（scene_handler.cpp 在途 241-259 / HEAD 340-354）。S6 的几种合法结局分类与此一致。
  - [4]
    253 的请求间隔：sendDigestRequest 先按 lastDigestAt 等满 1.1 秒，send 再按 lastRequestAt 等满 1.1 秒，节奏跨重连保留，S6 探针也走同一路径（sync_smoke_scenario.go:622-646）。253 和 43 都不在 generated/tables/messagelimiter.json（共 81 行）中，生效的是 MessageLimiter 的默认构造（3 次，窗口 1 秒，按会话、按消息号计数，见 session_info_comp.h:49）。1.1 秒间隔下判定时缓冲区最多 2 条，不会触发 kRateLimitExceeded。友方 RPC 的各消息号也按同一节奏发送，S3 的重取同样间隔 ≥1.1 秒。
  - [5]
    账号：robot_9911 / 9912 在 robot/*.go 和 robot/etc/*.yaml 里只出现在 sync-smoke。其它冒烟用的是 9001-9006、9101-9104、92xx、93xx、94xx、9501（资产通道预留）、9601、97xx、9801、9901；压测配置 account_fmt=robot_%04d 的最大 robot_count 是 1000，其余配置用 stressbot_ / z1bot_ / smoke5k_ 等不同前缀，都不冲突。
  - [6]
    不存在误报 SYNC_SMOKE_OK 的路径：fail() 统一 os.Exit(1)（195-200）；fetchDigest 要求 status=OK 且契约合法；friendRpc 把信封拒绝、gate 推送的 1003、超时一律当 error；S2 要求 g≠0，挡住了「scene 读错 Redis、永远返回空摘要」导致 S4 / S6 平凡通过的情形；OK 行只在 S1-S6 全部通过后打印（403-405），之后 main.go:248-250（HEAD）return，退出码 0。zap 用的是 Development 配置，Info 级别会输出 OK 行。
  - [7]
    tip 判定：tipSyncRateLimited 与 tipSyncServiceUnavailable 取自导表生成的枚举（1008 / 1003），与 C++ common_error_tip.pb.h:69、74 一致，没有手写数字；业务 tip 读的是响应体 error_message.id，信封 tip 读的是 MessageContent.error_message，两者没有混用。
  - [8]
    两个 handler 桩：生成器在文件已存在时跳过（tools/proto_generator/protogen/internal/generator/go/robot_handler.go:71-78），手写桩不会被覆盖；Empty 应答替换成请求类型的签名一致；分发表注册在 robot/logic/handler/message_body_handler.go:126-127（HEAD）。253 / 254 常量在 robot/generated/pb/game/message_id.go:256-257（HEAD），与 proto/message_id.txt 一致；vendored 的 player_scene.pb.go 已含 SyncDomain 和 SyncDigestS2C。
  - [9]
    main.go 中 sync-smoke 的分发 hunk 已在 de4960bee 入库（main.go:244-251）；main.go 当前的在途 diff 属于别的会话（assign-gate 排队抖动），与本冒烟无关。config.go 的 SyncSmokeConfig.validate 在 cross_zone=true 时强制 zone_id 和 visit_zone 都非 0 且不相同。
  - [10]
    连接与资源：失败路径都会经 cleanup 关闭两个会话；S1 先收齐两个登录结果再判错，避免一方失败时漏收尾；跨 zone 腿的严格重登只在本模式独占的进程里覆盖全局重登函数，并且在 RecvLoop 启动之前注册。
**notes**: 1）本次复核只读，没有编译，也没有运行任何测试或冒烟；所有结论来自静态阅读，运行期证据要等 Codex 或用户按 handoff §4 第 6、8 步执行后才能补上。2）引用的 robot/、go/friend、message_limiter、player_landing.cpp、messagelimiter.json 都是 HEAD 已提交、没有在途改动的文件。cpp/nodes/gate/handler/rpc/client_message_processor.cpp 与 cpp/nodes/scene/handler/rpc/scene_handler.cpp 有别的会话的在途改动：前者引用的行号与 HEAD 一致；后者标了在途和 HEAD 两套行号。3）三处自述偏差的结论：短重连约 250ms，接受，只改文案（F6）；S6 只记录，接受，但建议补三条与时序无关的硬断言（F2）；跨 zone 默认关，接受，但建议在双 zone 环境补跑一次（F8）。4）是否给 253 追加 MessageLimiter 行要由用户拍板（handoff §3.5）。按当前默认档，冒烟间隔是安全的；设计中「同 SceneInfoC2S」等于不配，因为 43 也没有行。5）没有核对客户端仓（../mmorpg-client）是否有人用 robot_9911 / 9912 登录，本任务也没有读客户端的授权。6）sendAndRecvTimeout 在超时后会留下一个仍在读连接的 goroutine，属于共享 helper 的既有行为；本场景超时之后立即 fail 退出，不影响判定，所以没有列为 finding。7）没有创建任何文件，也没有用 git show 重定向生成临时副本。
