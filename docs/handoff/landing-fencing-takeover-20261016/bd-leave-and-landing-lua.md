<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**item**: 进场链余项 b（迟到的 LeaveScene 删掉新登录的 location）与 d（scene_manager 的落点 Lua 不认会话），合并调研与设计
**current_behavior**: 【版本说明】下文 Go 文件和 proto 的行号对 HEAD（a105c3efd）与工作区都成立：git diff HEAD 显示 go/scene_manager/internal、go/player_locator/internal、proto/scene_manager/storage.proto、scene_manager_service.proto 都没有未提交改动。C++ 的 player_lifecycle.cpp/.h 和 scene_manager_response_handler.cpp 有其他会话的在途改动，涉及存盘、资产、gRPC 回调签名，与本题无关。C++ 行号按工作区计；player_lifecycle.h 比 HEAD 多 2 行，例如 kSmErrHandoffPending 在 HEAD 是 :1098，工作区是 :1100。

━━ 一、MarkOffline → LeaveScene 调用链（问题 b）━━
1. gate 发出 LeaveGame，进入 login。go/login/internal/logic/clientplayerlogin/leavegamelogic.go:26-55。:35-36 的注释写明「LeaveGame 不拿玩家锁，会与 EnterGame 的后台链并发」。:51-53 对每个玩家调 deletePlayerSession，经 session_lifecycle.go:46-54 到 sessionmanager.DeleteSession（pkg/sessionmanager/session_manager.go:157-174）：先 GetSession，session_id 相同时带 ExpectedSessionId 和 ExpectedSessionVersion 调 MarkOffline。
2. player_locator 的 MarkOffline 在 go/player_locator/internal/logic/markofflinelogic.go:39-100。:48 GET player:session:{id}；:65 比对 session_id 和 version；:80 调 deleteSessionIfUnchanged（session_cas.go:262-289，Lua 在 :75-89），按整字节 CAS 删除会话键、旧键 player:location:{id}（keys.go:15-17，生产不写）和租约簿记。:98 随即同步调 notifySceneManagerLeaveOrEnqueue（:105-140）。
3. notifySceneManagerLeave 在 leasemonitor.go:286-315。:287-294 中 session.SceneId 恒为 0（见 :317-318 与 login entergamelogic.go:672-673 的注释），所以由 resolveSceneManagerSceneID（:319-332）即时 GET player:{id}:location，取其中的 SceneId。:304-307 发出的 LeaveSceneRequest 只有 PlayerId 和 SceneId，不带会话身份。
4. RPC 失败时走 enqueueOfflineCleanup（session_cas.go:203-225）。守卫是「会话键已重新出现就不入队」。
5. scene_manager 的 LeaveScene 在 go/scene_manager/internal/logic/leavescenelogic.go:116-159：:117 GET location 原文，:128 解码，:137 只比 SceneId，:143 调 deletePlayerLocationIfUnchanged（:63-83）。Lua 在 :33-49：原文仍等于 ARGV[1] 才 DEL，同一段脚本里给场景人数 instance:{sid}:player_count 和节点人数各 DECR 一次，并钳到 0。:110-115 的注释自认挡不住两类：同落点重连不写 location；同一场景的新落点在 GET 之前已写入。
6. 屏障现状：新登录 SetSession 的「missing」模式（session_cas.go:29-57）只在 :38-42 看到 processing claim、token 或 payload 时返回 2（cleanup pending）。MarkOffline 的同步路径不登记任何 claim，所以从删会话到 LeaveScene 完成之间没有屏障。租约到期路径（leasemonitor.go:121-216，commitLeaseExpiry 在 session_cas.go:129-142、375-392）在 ack 之前一直持有 processing claim，可以挡住新登录。

━━ 二、EnterScene 落点、location 与人数（问题 d）━━
1. login 发起：entergamelogic.go:729-857 applyLoadedPlayerSession。:745 持锁铸代次，:754 persist（SetSession 或 Reconnect，:900-955），:775 Bind，:789-798 复查栅栏，:807 调 EnterScene。请求由 buildEnterSceneRequest（:976-988）构造，带 SessionId(3) 和 LoginGeneration(12)。
2. scene 发起：一共 5 个构造点，都经 SendCorrelatedEnterScene（player_lifecycle.cpp:468-480）发出，全部只 set_session_id，不填 login_generation。
   - 客户端换图：player_scene_handler.cpp:141-157
   - 镜像自动进场：scene_manager_response_handler.cpp:161-174
   - 队伍跟随：player_team.cpp:490-503
   - 以上三处经 RequestSceneChange（player_lifecycle.cpp:4905-4925）
   - 疏散改派：player_lifecycle.cpp:3848-3882，用票据中的 sessionId
   - 交接重发：player_lifecycle.cpp:5449-5473
3. scene_manager 的 EnterScene 在 enterscenelogic.go:127-796：
   - 去重：:199-278
   - :289 getPlayerLocationWithRaw（changesceneutil.go:46-61）读 location；:296 读 owner_epoch
   - :313-319 Gone 过滤、:328-338 Dead 过滤会把 currentLoc 和 currentLocRaw 清空
   - 跨区分支 :379-438：guard 在 :425，handleCrossZoneRedirect 在 :1047-1117（:1062 写等待落点，:1080-1082 扣旧场景人数）
   - :475 resolveSceneForEnter，自动选线时预占 +1
   - :521-525 判定 samePlacement、samePhysicalNode、crossNodeHandoff
   - 换手门 :533-544；归属 zone :568-582；补种 :623-647；guard :649-653
   - :658-674 samePlacement：退回预占（:660-662）→ 发路由（:668）→ 返回成功，location 一个字节都不写
   - :677-699 预占；:723-728 ReleasePlayer
   - :734 placePlayerLocation（changesceneutil.go:128-187）。:138-146 每次从零构造 PlayerLocation，只有 scene/node/update_time/zone/epoch/pending 这几项。:153-155 EVAL 的 KEYS 为 owner_epoch、location、handoff，ARGV 为观察到的 epoch、新原文、标记。
   - Lua 在 owner_epoch.go：铸造版 :103-122，不铸造版 :130-140。两者都只做 epoch CAS，既不比 location 原文，也不认会话。
   - 失败时 :737 退还预占；:757-759（6b）旧场景 -1；:763-781 推路由（routePlayerToGate :1339-1391，:1357 原样带 in.LoginGeneration）；推路由失败走回滚（:1128-1144；owner_epoch.go:432-524；Lua :209-242）。
4. 人数：instance:{sid}:player_count 与 node:zone:{z}:{n}:player_count。Incr/Decr 在 instance_lifecycle.go:355-375。副本按 player_count>0 保活，否则空闲超时就销毁（instance_lifecycle.go:106-117）。

━━ 三、location 键的结构与全部读写方 ━━
- 键：player:{player_id}:location，无 TTL。值是 protobuf storage.PlayerLocation（proto/scene_manager/storage.proto:17-43）：scene_id=1、node_id=2、update_time=3、zone_id=4、owner_epoch=5、pending_scene_conf_id=6、rollback_receipt=7。没有任何会话身份字段。:41 注释写「GO-3 的新字段从 8 起取」。
- 与 owner_epoch、handoff、player:session、login_generation 同在核心 Redis 实例（shared-redis-split-by-purpose.md:129、:135、:180）。
- 写方（只有这些）：
  - scene_manager 落点 Lua：owner_epoch.go:103-122、:130-140；经 changesceneutil.go:128-187；调用点 enterscenelogic.go:734、:1062；UpdatePlayerLocation（changesceneutil.go:69-76）只有测试夹具调用
  - 回滚 Lua：owner_epoch.go:209-242
  - LeaveScene 删除 Lua：leavescenelogic.go:33-49
  - DeletePlayerLocation：changesceneutil.go:202-206，只有测试调用
  - 离线合服工具：tools/merge_zone/scene_hot_state.go:637（pipe.Del），解码器 :157-230 手写，会跳过未知字段
- 读方：
  - scene_manager：changesceneutil.go:42、:46-61；leavescenelogic.go:117
  - player_locator：leasemonitor.go:319-332（键名见 keys.go:25-27）
  - login：player_class_backfill.go:91-95（Lua 只看在不在）
  - go/shared/scenenode/locator.go:37-41、:149-175，被 trade、guild、player_inbox（notify.go:118-125）使用
  - go/match/internal/playercontract/playercontract.go:49-51、:98-105
  - C++ scene：player_ownership_comp.h:64-67；改派核实 relocate_confirm.h:146-148 与 player_lifecycle.cpp:1755-1764；ResolveTravelOutcome 的 kLuaJudgeTravelOutcome（player_lifecycle.h:745-749，player_lifecycle.cpp:5575-5591），是唯一读 rollback_receipt 的地方；队伍跟随 player_team.cpp:390、:432-433、:512；解析入口 player_lifecycle.cpp:80
  - C++ battle：battle_room_manager.cpp:229-235、:2059
- C++ 对 location 只读不写。
**problem_confirmed**: True
**problem_analysis**: 两项都按代码静态确认成立。目前没有指标能统计发生频率（未核实）。

━━ b：迟到的 LeaveScene 删掉新登录的 location ━━
设 A 为旧会话（sA, gA），B 为新登录（sB, gB > gA），A 在世界频道 S1、节点 X 上。
1. A 发 LeaveGame → login → MarkOffline。markofflinelogic.go:80 删掉 A 的会话。
2. LeaveGame 不拿玩家锁，客户端可以立刻再发 EnterGame（例如返回选角后马上进游戏，或同一连接、另一台设备）。login 的 GetSession 读到 nil，判为 FirstLogin，SetSession 走「missing」模式；同步路径没有 claim，所以直接成功（session_cas.go:38-42 不拦）。
3. B 的 EnterScene（ZoneId=本 zone，SceneId=0）选出最优频道。

接下来有四种变体：
- b1，同落点：选出的仍是 S1、节点 X，命中 samePlacement（enterscenelogic.go:658-674）。退还预占后发路由，location 原样不动。随后 A 的 LeaveScene 到达（leavescenelogic.go:117），原文未变、SceneId 相等，Lua 删掉 location，S1 和 X 的人数各 -1。
- b2（交接文档没写到的更宽一种）：B 已把 location 写到 S2。player_locator 的 resolveSceneManagerSceneID（leasemonitor.go:319-332）是现读 location，读到的就是 B 的 S2，于是 LeaveScene(S2) 在 scene_manager 端 SceneId 相等、原文就是 B 的，B 的新落点被删，S2 人数 -1。从 markofflinelogic.go:80 删会话起，到 leasemonitor.go:320 GET 之前，B 的任何一次落点都会被删。
- b3：B 落到同一个 scene_id、但节点换了；只要落在 player_locator 读之后、scene_manager GET 之前，同样被删。
- b4：LeaveScene 的 RPC 在客户端超时，但服务端仍在执行（go-zero 的 Redis 调用不受 ctx 约束）。迟到的执行发生在新登录之后时，效果同 b1/b2。租约到期路径的 processing claim 屏障也拦不住这一种，因为 ack 发生在重试成功之后。

后果：
- B 在 X 上，但 location 没了。
- 人数少 1：副本可能被 destroy-on-empty 带人销毁（instance_lifecycle.go:106-117）；世界频道会被误判为空，影响扩缩容和选线。
- B 下一次跨节点换图时 currentLoc 为 nil：不过换手门、不发 ReleasePlayer，直接铸 N+1。目标节点从 Redis 载入时，X 可能还没存盘；X 之后的存盘被 epoch CAS 拒绝，实体被当作已废黜、不存盘就销毁，造成进度回档。
- trade、guild、inbox、match 的 Locator 判 B「不在线」（scenenode/locator.go:160-163）。

b 只出现在 MarkOffline 的同步路径。enqueue 和租约路径有 cleanup-pending 屏障，但 b4 例外。

━━ d：落点 Lua 不认会话，陈旧请求改写 location 和人数 ━━
d1，陈旧登录：
1. 链 A 的 EnterScene RPC 在 zrpc 侧超时，或链 A 心跳丢锁；scene_manager 的处理协程仍在跑，已在 :289/:296 观察到 L0=(S1,X,N)。
2. 链 A 失败、释放锁。链 B 提交会话后发 EnterScene，命中 samePlacement，不写 location，路由发往 gateB。
3. A 的协程继续：目标 S2 在同一节点 X，不铸造，luaSetLocationIfEpoch(N) 通过，因为 B 没动 epoch。location 变成 S2，S1 -1、S2 +1，路由发往 gateA。
4. gateA 按 gA 转发后，scene 的栅栏判 kRejectStale。
结果：location 写着 S2，B 实际在 S1；S1 少 1、S2 多 1，直到下一次落点才收敛。这正是 scene-enter-session-fence.md §9 残余 2。

d2，旧会话在途的换图：
1. 实体还绑着 sA 时发出换图 EnterScene{sA, S2 同节点, gen 0}。
2. B 登录 samePlacement 先到 scene_manager。
3. A 的换图后到，同样不铸造就改写成 S2、人数挪动，路由经 gateA 到 scene 后被栅栏拒绝。
结果同 d1。
- A 若是跨节点：凭标记铸造时 B 一侧的 epoch 也被推进，X 上已绑 sB 的实体之后存盘被拒、被废黜。是否会走到这一步取决于 scene 侧交接状态机：B 的入场会不会中止 A 的冻结，未核实。
- A 若是跨区第一条腿：会把 B 的有效落点改成「等待落点」。

现有 owner_epoch CAS 只能挡住两个都铸造（或一方铸造）的并发。挡不住的是同节点不铸造的落点和 samePlacement，因为 Lua 既不比原文，也不比会话。

━━ 共同根因 ━━
location 只记「在哪」和「哪一代归属」，不记「哪次登录放行了它」。LeaveScene 只能按 SceneId 和原文识别；同落点重登又刻意不写，原文不变，就把新登录伪装成了旧登录。
## design_options
  - [0]
    **name**: A：会话身份写进 PlayerLocation（proto 新字段）+ Go 侧判定 + Lua 原文 CAS 保证原子（推荐）
    **description**: PlayerLocation 新增 session_id(8)/login_generation(9)。EnterScene 读到 location 后，在任何预占和写入之前，由纯函数 decidePlacementIdentity 比对「location 身份」与「请求身份」，规则与 enter_fence::Decide 对齐。放行时，所有落点都把应记录的身份写进新 location；同落点重登在身份变化时用克隆加身份做一次原文 CAS 写入。两段落点 Lua 增加 location 原文期望（ARGV[4] 为模式，ARGV[5] 为原文），使 Go 侧判定与提交原子。LeaveSceneRequest 新增 session_id(5)/login_generation(6)，由 player_locator 填被删会话记录的原值；scene_manager 解码后按代次决定删不删，删除 Lua 本身不变，仍是原文 CAS 加原子减人数。scene 自发的 EnterScene 把实体 PlayerLoginIdentityComp 的代次填进现有的 12 号字段。
    **pros**: location 仍是唯一的权威记录，不新增键、TTL 或清理路径，旧读者（C++、match、trade、inbox、merge_zone 手写解码器）对新字段无感；沿用仓库已有的「Go 观察判定 + Lua 原文 CAS」模式（LeaveScene、回滚已在用）；b 与 d 共用同一份身份，规则单一；新旧值、新旧调用方任意组合都有确定的退化语义（0 等于无身份，沿用旧行为）；scene 和 login 都不需要新增字段。
    **cons**: 落点 Lua 加原文 CAS 后，同节点不铸造落点的并发语义从「后写者赢」变成「后到者拿 19 重试」；同落点重登从「不写」变成「身份变化时写一次」，多一次 EVAL；scene 需要在 3 处发送点补代次（RequestSceneChange、交接重发、疏散票据）；字段 8 原本被 GO-3 预留，需要协调号段。
  - [1]
    **name**: B：独立旁路键 player:{id}:placement_identity（"sid:gen" 文本）在 Lua 里直接比较
    **description**: 不动 PlayerLocation，另设一把字符串键存身份。落点 Lua、回滚 Lua、LeaveScene Lua 都把它列进 KEYS，与 location 成对读写，比较全在 Lua 内完成。
    **pros**: 比较在一段 Lua 里原子完成，不需要 Go 先解码，也不需要给落点 Lua 加原文 CAS。
    **cons**: 同一份事实拆成两把键，所有写者都必须成对维护：两段落点 Lua、回滚、LeaveScene、merge_zone 的 DEL、运维手工清 location 的 runbook；部署用的 Redis 是 allkeys-lfu（enterscenelogic.go:603 注释），两把键会被分别淘汰而漂移；C++ 改派核实和取证读不到身份；违反「一条业务知识一个权威来源」（AGENTS §11.2 DRY）。
  - [2]
    **name**: C：只在 player_locator 加屏障修 b（MarkOffline 先登记 claim、再删会话、LeaveScene 成功后 ack）
    **description**: 复用 cleanup-pending 屏障：MarkOffline 先把清理条目写进 processing 和 payload，再执行删会话的 Lua，同步 LeaveScene 成功后 ack；失败就留给 LeaseMonitor 重试。新登录的 SetSession（missing 模式）在这段时间回 2，被 fail-closed 挡住。
    **pros**: 不改 proto，scene_manager 不动；直接复用现成的屏障机制。
    **cons**: 只修 b，修不了 d；屏障期间合法的新登录拿到 cleanup pending，需要重试，有活性代价；b4（RPC 客户端超时而服务端迟到执行）的窗口变窄但不为零，ack 后仍可能落地；修法依赖时间顺序而非身份，与交接给出的方向「LeaveScene 带身份」不符。
  - [3]
    **name**: D：Lua 内手写 protobuf 解码，读出 location 的身份字段再比较
    **description**: 在落点和 LeaveScene 的 Lua 里用 string.byte 解 varint，按字段号取出 8/9，与 ARGV 比较后再写或删。
    **pros**: 判定与提交在同一段 Lua 内，原子性最强。
    **cons**: 手写 wire-format 解码很脆弱（字段顺序、未知字段、wire type 都要处理），与 merge_zone 一样要和 storage.proto 同步改号；可读性和可测性差，违反 AGENTS §11.2 KISS；仓库已有「Go 判定 + 原文 CAS」能达到同等原子性。
**recommended**: 推荐方案 A，并且 scene 自发请求要带代次（下文称 Y 口径）。

━━ 1. 身份的定义 ━━
身份 = (session_id uint32, login_generation uint64)。G2 保证非 0 的代次与会话一一对应，因此判定以代次为主，会话号用于两件事：
- 「同一会话」放行（与栅栏一致）；
- 发现异常（代次相等、会话不同）。

━━ 2. EnterScene 判定 decidePlacementIdentity（纯函数，零副作用）━━
位置：enterscenelogic.go:296-299 读完 epoch 之后、:313 Gone 过滤之前。用观察到的原始 currentLoc，不受 Gone/Dead 过滤影响。拒绝时只会走到已有的 defer，释放去重占位。

L = location 中记录的身份；R = 请求的 (in.SessionId, in.LoginGeneration)。

| 情形 | 结论 | 写入的身份 |
|---|---|---|
| L 不存在（无 location，或 sid_L=0 且 gen_L=0） | admit_fresh | R |
| sid_R≠0 且 sid_R==sid_L | admit_same_session | (sid, max(gen_L, gen_R)) |
| gen_R==0（会话不同） | admit_legacy，计数 | (sid_R, 0) |
| gen_L==0，gen_R≠0 | admit_newer | R |
| gen_R>gen_L | admit_newer | R |
| gen_R<gen_L | reject_stale | — |
| gen_R==gen_L，会话不同 | sid_L≠0 时 reject_conflict；sid_L=0 时按同会话处理 | — |

拒绝回新码 25 ErrPlacementSessionSuperseded，指标 reason 为 session_stale 或 session_conflict。

与 scene 栅栏只差一行：「请求无代次、当前有代次、会话不同」在栅栏判 kRejectUnversioned，这里判 legacy 放行。理由是 location 的寿命长于实体：被踢的旧实体会销毁，location 却还在。如果这里拒绝，login 滚动混版或回滚到无代次版本期间，location 带代次的玩家每次经旧 login 登录都会被拒，直到 30 秒以上的租约清掉 location，这是活性回退。scene 栅栏仍然在入口兜底。d2 必须靠 scene 自发请求带上实体代次来关闭（见第 6 节），否则它们以代次 0 走 legacy。

━━ 3. 何时写身份 ━━
- 所有放行的落点（同 zone 落点、跨区第一条腿的等待落点、死节点接管、首次落点）：placePlayerLocation 新建的 PlayerLocation 带上应写的身份。
- 同落点重登（samePlacement）：应写的身份不等于 L 时，克隆 currentLoc（保留 owner_epoch、rollback_receipt、update_time、pending 等全部字段），只改身份，用不铸造 Lua 加原文 CAS 写入。CAS 失败回 19，不发路由。随后推路由失败的话，用 keep 模式回滚到旧原文：调 rollbackPlacementAfterPushFailure，不动人数。
- 身份不变时维持现状，不写。
- 只送连接的重定向、各类拒绝：不写。

━━ 4. 落点 Lua 的原文 CAS ━━
- 两段 Lua 都在 epoch 比对之后增加：ARGV[4] ∈ {any, missing, present}，present 时 location 必须等于 ARGV[5]，否则返回 0（翻成 19）。
- 铸造版：原有的重放识别分支放在原文检查之前，保持不变。
- 不铸造版：新增「location 已等于 ARGV[2] 就返回 1」，保持 go-redis 原样重发时的幂等。
- 期望原文用观察值 observedLocRaw。Gone/Dead 分支清空的 currentLocRaw 继续只作为回滚基线。
- 理由：没有原文 CAS 时，A 在 B 更新身份之前做出观察和判定，之后照样能用同一个 epoch 覆盖掉 B 写入的身份。

━━ 5. LeaveScene：按代次比较，不做会话号精确匹配 ━━
在 SceneId 比对之后，按下表判定：

| 情形 | 结论 |
|---|---|
| gen_R==0 或 gen_L==0 | legacy：沿用旧的原文 CAS 删除，计数 |
| gen_L>gen_R | keep_newer：不删、不减人数，回成功 |
| gen_L==gen_R 且会话不同（都非 0） | keep_conflict：不删，打 ERROR（违反 G2） |
| 其余（gen_L<gen_R，或身份完全相同） | 照旧原文 CAS 删除并减人数 |

为什么不能按会话号精确匹配：同一连接登出后再登录时，会话号不变而代次变大；精确匹配会把这次新登录的 location 误删。

为什么 gen_L<gen_R 必须删：location 的主人比正在离场的会话还旧，已经被它顶替，不删就泄漏。泄漏的后果是幽灵人数；A′ 释放标记 300 秒 TTL 过期后，下一次登录落到别的节点会永久拿 18。这一行的行为与现状完全相同。

所以这次改动只收窄了一种情况：location 的主人比正在离场的会话新。

━━ 6. scene 自发请求带身份 ━━
会话号已经都在填，不用改。代次复用 EnterSceneRequest 的 12 号字段，填实体 PlayerLoginIdentityComp.loginGeneration，前提是 comp.sessionId 与 req.session_id 相同，否则填 0。改动点：
- RequestSceneChange 统一盖章，覆盖客户端换图、镜像、跟随三处；
- 交接重发；
- 疏散：DispatchEmergencyRelocate 在 :3605-3618 已经读到了这个代次，把它传给 SendEmergencyRelocateEnterScene。

scene_manager 继续把 12 号字段原样放进 RoutePlayerEvent。gate 的 ApplyRouteIdentity 对同一角色取 max；实体代次 ≤ 会话值，所以会话值不变。

━━ 7. 人数一致 ━━
- 判定在一切预占之前，拒绝时没有东西要退。
- 只改身份的写入不动人数。
- 原文 CAS 失败沿用 :737 退还预占。
- LeaveScene 仍是「谁删谁减」，同在一段 Lua 里。keep_newer 不减，这是对的：b1 中玩家仍在 S1，计数本来就应该保留；b2 中新落点已在 6b 扣过旧场景。

邻域发现（预先存在，不在本次范围）：「同 scene_id、换了节点、不是死节点接管」的落点，6b（:757）只在 scene 变化时才扣，会多算 1。以前迟到的 LeaveScene 碰巧抵消了这个多算，修好 b 之后它就暴露出来，建议另开一项修。

━━ 推荐理由 ━━
正确性和数据一致性优先，b 与 d 用同一份身份；不新增键和清理路径；每个组件都能独立滚动和回滚，退化为旧行为；scene 和 login 都不加字段；规则与已落地的入场栅栏同构，一套判定表 Go 与 C++ 对照测试。
**contract_changes**: 【proto】
1. proto/scene_manager/storage.proto 的 PlayerLocation 追加两个字段：
   - uint32 session_id = 8;
   - uint64 login_generation = 9;
   语义：最近一次被 scene_manager 放行并写入或确认本 location 的那次 EnterScene 所代表的会话。0/0 表示无身份（旧版 scene_manager 写入，或请求没带会话），所有比较退回旧行为。只由落点 Lua 写入；回滚 bump 克隆旧值，keep 恢复原字节，两种都会保留身份。
   同时把 :41 的「GO-3 的新字段从 8 起取」改为「下一个从 10 起取」。需要与 GO-3 负责人或用户确认号段（见 open_questions）。类型符合 AGENTS §4：session_id 用 uint32，代次用 uint64。
2. proto/scene_manager/scene_manager_service.proto 的 LeaveSceneRequest 追加：
   - uint32 session_id = 5;
   - uint64 login_generation = 6;
   语义：被清理的那条会话记录的原值，由 player_locator 从被删的 PlayerSession 里取。0/0 表示旧版调用方，按旧行为删除。
3. EnterSceneRequest.login_generation=12 不加字段，只改语义注释：
   - login：本链铸出的代次；
   - scene 自发：实体 PlayerLoginIdentityComp 中已放行的代次，会话号与快照一致时才填，可落后于会话记录；
   - scene_manager 不再只透传，而是用它做落点身份裁决，同时仍原样进入 RoutePlayerEvent.login_generation(7)；
   - 仍不进 request_id 指纹（enterscenelogic.go:100-124 不变）。
4. 其余 proto 不变：EnterSceneResponse、PlayerSession(14)、MarkOfflineRequest、RoutePlayerEvent、BindSessionEvent。

【错误码】scene_manager 私有数轴，不是 tip 码
- go/scene_manager/internal/constants/errors.go 新增 ErrPlacementSessionSuperseded uint32 = 25：落点会话身份已被更新的登录取代（stale 或 conflict），一个字节都不改；陈旧请求方不应重试同一会话。
- C++ player_lifecycle.h（工作区 :1100-1145）同步手抄 kSmErrPlacementSessionSuperseded = 25，并加入 SmRejectedBeforeAnyPlacementWrite。这样做合法：该码的唯一出处早于任何落点写。

【Lua】
1. luaMintEpochAndSetLocation（owner_epoch.go:103-122）：
   - KEYS 不变：owner_epoch、location、handoff；ARGV[1..3] 不变。
   - 新增 ARGV[4] = location 期望模式（"any" | "missing" | "present"），ARGV[5] = 期望原文（仅 present 时使用）。
   - 执行顺序：epoch 比对（含原有重放识别）→ location 期望（不符返回 0）→ 标记比对（-1）→ INCR+SET。返回值集合不变。
2. luaSetLocationIfEpoch（:130-140）：
   - 同样新增 ARGV[4]/ARGV[5]。
   - epoch 不符返回 0；location 已等于 ARGV[2] 返回 1（重发幂等）；location 与期望不符返回 0；否则 SET 后返回 1。
3. luaDeletePlayerLocationIfUnchanged（leavescenelogic.go:33-49）不变；身份判定在 Go 侧，原文 CAS 保证原子。
4. luaRollbackPlayerPlacement（owner_epoch.go:209-242）不变。

【Redis 键值】不新增键。player:{id}:location 的值多两个 varint 字段（tag 8/9，wire type 0）。旧读者兼容性已核对：
- Go/C++ 生成代码会忽略或保留未知字段；
- merge_zone 手写解码器在 scene_hot_state.go:186-197 按 varint 跳过未知字段号。

【RPC 签名】不变：SceneManager.EnterScene/LeaveScene、PlayerLocator.MarkOffline 都不变。

【Kafka】格式不变。scene 自发路由的 RoutePlayerEvent.login_generation 从 0 变成实体代次（≤ gate 会话值），gate 合并的结果不变。

【指标】低基数，可选：
- scene_manager_placement_identity_total{site=enter|leave, verdict=fresh|same_session|legacy|newer|stale|conflict|keep_newer|deleted}；
- 现有 enter_scene_rejected_total 新增 reason=session_stale、session_conflict。
- 不得以 player_id 作 label。
**code_changes**: Go 行号按 HEAD（工作区与 HEAD 相同）；C++ 行号按工作区。

1. proto：按 contract_changes 改 storage.proto:17-43 和 scene_manager_service.proto:150-156，并更新 :107-113 注释。生成物由 Codex 重新生成，不手改。

2. go/scene_manager/internal/logic/placement_identity.go（新文件）
   - type placementIdentity struct{ sessionID uint32; loginGeneration uint64 }
   - identityOfLocation(*PlayerLocation) (placementIdentity, bool)
   - identityOfEnterRequest(in)
   - 平铺枚举 placementVerdict（kCount 加名表，名表长度用编译期断言或测试守住）
   - decidePlacementIdentity(cur, curExists, req) (verdict, record)，判定表见 recommended
   - leaveVerdict 与 decideLeaveIdentity(loc, in)
   - 两个纯函数单独测试。

3. enterscenelogic.go：
   - :296-299 之后、:300 之前：observedLocRaw := currentLocRaw；调 decidePlacementIdentity。被拒时：ObserveEnterSceneRejected(in.GateZoneId, reason)，打一行 Infof（玩家、请求与当前身份、结论），返回 errResp(ErrPlacementSessionSuperseded)。
   - :425：跨区 guard 加上 identity: record 和 expectLoc: presentOrMissing(observedLocRaw)。
   - :649-653：guard 同样补这两项。
   - :658-674 samePlacement：退还预占（:660-662）之后，若 record 与 L 不同，就克隆 currentLoc、只改身份，调新增的 setLocationIdentityIfUnchanged（不铸造 Lua，present 模式，期望 observedLocRaw，epoch 用 observedEpoch）。冲突回 19。:668-671 推路由失败时，用 placedLocation{epoch: observedEpoch, minted: false, raw: 新原文} 调 rollbackPlacementAfterPushFailure（keep），不调用会动人数的 rollbackEnterSceneAfterRouteFailure。
   - :744-750：19 的日志措辞改成「epoch 或 location 被并发推进」。
   - :1335-1338 的注释同步更新；:1357 不变。

4. changesceneutil.go：
   - :91-104：placementGuard 新增 identity 和 expectLoc{mode, raw}。
   - :128-155：placePlayerLocation 给 loc 填 SessionId/LoginGeneration，Eval 追加 mode 和 raw 两个参数。
   - :69-76：UpdatePlayerLocation 使用 mode "any"（测试夹具）。
   - 新增 setLocationIdentityIfUnchanged。

5. owner_epoch.go:55-140：两段 Lua 按 contract_changes 修改，并同步 ARGV 文档注释；:259-261 errOwnerEpochConflict 的注释改为「epoch 或 location 被并发推进」。

6. leavescenelogic.go：
   - :137-141 SceneId 比对之后插入 decideLeaveIdentity：keep_newer 打 Infof 后返回成功；keep_conflict 打 Errorf 后返回成功；legacy 记日志或计数后往下走；deleted 照旧走到 :143。
   - 改写 :99-115 的「挡不住的两类」注释。

7. constants/errors.go：新增码 25 及注释。

8. 指标（可选）：在 scene_manager/internal/metrics 增加 placement_identity 计数。

9. go/player_locator/internal/logic/leasemonitor.go:304-307：LeaveSceneRequest 填 SessionId: session.SessionId、LoginGeneration: session.LoginGeneration。MarkOffline（markofflinelogic.go:109）、租约路径（:201）、enqueue 重试（:118-120 的克隆保留了代次）都经这一个函数，不需要再改别处。markofflinelogic.go:90-98 的注释顺带更新。

10. C++ scene（工作区行号）：
   - enter_fence.h:36-40 旁新增纯函数 ActingLoginGeneration(const PlayerLoginIdentityComp*, SessionId reqSession)：comp 存在且 sessionId 相同返回代次，否则返回 0。
   - player_lifecycle.cpp:4905-4925 RequestSceneChange：在 SendCorrelatedEnterScene 之前，若 req.login_generation()==0，就用实体身份盖章。这一处覆盖 player_scene_handler.cpp:141-157、scene_manager_response_handler.cpp:161-174、player_team.cpp:490-503，这三个文件本身不改。
   - :5449-5473 交接重发：playerEntity 在作用域内，同样盖章。
   - :3826-3857 SendEmergencyRelocateEnterScene：新增参数，或在 EmergencyRelocateTicket（:101-107）加 loginGeneration，取自 :3605-3618 已读出的值，用 req.set_login_generation 写入。
   - player_lifecycle.h:1100-1145：新增 kSmErrPlacementSessionSuperseded=25，并加 case。
   - scene 入口栅栏、SessionMap、gate 一行不动。

11. 文档：
   - scene-enter-session-fence.md：§2 结论表、§4（scene 自发请求带代次，scene_manager 参与裁决）、§7「LeaveScene」与「scene_manager 只透传」两行、§9 残余 2 改为已解决并写清剩余项；新增一节「落点会话身份（b/d）」写判定表。或者另起 docs/design/ 文档并登记到 docs/README.md。
   - cross-zone-scene-travel.md：GO-3 的字段号说明。
   - shared-redis-split-by-purpose.md:129：location 值字段。
   - docs/PROGRESS.md 追加一条。
   - Java 仓 PARITY.md 登记一行。
**rollout_compat**: 【默认值语义】
- PlayerLocation 中 session_id 和 login_generation 都为 0：无身份，EnterScene 判 admit_fresh，LeaveScene 判 legacy。
- LeaveSceneRequest 两项都为 0：旧版调用方，判 legacy（与现状一致）。
- EnterSceneRequest.login_generation 为 0 且会话不同：判 admit_legacy，计数。
- 「legacy 计数」在 scene、scene_manager、player_locator 全部升级后应趋近 0。login 在入场栅栏那一批已经开始铸代次。

【建议顺序】三者之间任意顺序都安全：
1. proto 生成（no-unity 配置）；
2. scene（开始在自发请求上盖代次。旧 scene_manager 会把它透传进路由，gate 取 max 后会话值不变，无行为变化）；
3. scene_manager 全部副本；
4. player_locator。
login 和 gate 不需要发版。

【混版窗口】
- scene_manager 新旧副本并存：旧副本落点时从零构造 location，身份被抹成 0，没有原文 CAS，samePlacement 也不写身份。这些玩家在新副本上退化为 legacy。效果是 b/d 维持现状，不会出现新的失败模式。新旧副本各自通过 EVAL 发自己的脚本文本，不存在脚本版本错配。
- 新 player_locator 配旧 scene_manager：旧版忽略 5/6 号字段，等于旧行为。
- 旧 player_locator 配新 scene_manager：走 legacy 删除，等于旧行为。
- 新 scene 配旧 scene_manager：12 号字段非 0，只进入路由，无影响。
- 旧 scene 配新 scene_manager：自发请求代次为 0；同会话照常放行；不同会话走 legacy 放行，此时 d2 维持现状。
- login 混版或回滚到无代次版本：请求代次为 0，走 legacy 放行，不会卡死。这正是不照搬栅栏 kRejectUnversioned 的原因。

【回滚】
- 任一组件都可以单独回滚，顺序不限。
- 已写入 8/9 号字段的 location 对旧代码无害：Go 的 proto.Clone/Marshal 会保留未知字段，所以旧二进制做 bump 回滚时身份仍在；keep 回滚恢复原字节；C++ 忽略这两个字段；merge_zone 会跳过它们。
- 回滚之后再升级：location 中的身份只可能比真实持有者旧，不可能更新（代次单调）。最坏情况是 legacy 删除或 newer 放行，不会误拒合法登录。
- 码 25 只有新版 scene_manager 会发。旧 login 对任何非 0 码都按失败处理，推 3023（entergamelogic.go:815-831）；scene 侧对非 18 的码给 kEnterSceneFailed，跟随请求只记日志（player_lifecycle.cpp:5843-5880）。

【并发语义变化】同节点不铸造落点从「后写者赢」变为「后到者拿 19 重试」。同一玩家正常最多只有 login 和 scene 两个来源，scene 侧又是单槽在途，所以只影响本来就该串行化的跨来源竞态。

【去重】请求字段没有增加，指纹不变，旧去重条目与新代码互认。

【不需要开关】判定在每次请求中都可推导，零值天然退化，与栅栏 §6「不设开关」口径一致。
**invariants_check**: 1. 入口栅栏必须是第一步，且零副作用：保持。
   - C++ HandleEnterGameNode 第 0 步不改。本方案在 C++ 侧只改发往 scene_manager 的请求盖章（RequestSceneChange、交接重发、疏散），这些都不在入场处理路径上。
   - scene_manager 一侧照同一原则：身份判定放在读完 location 和 epoch 之后、任何预占（:475）、补种（:623）、写入和路由之前，纯函数、零副作用。之前唯一的副作用是已有的去重 SETNX，由 defer 在错误应答时释放（:173-197）。

2. 同一会话一律放行：保持。
   - scene 栅栏不改。
   - scene_manager 判定表同样对同会话一律放行，代次取 max，不会因为代次落后（G4）拒绝同会话请求。
   - 同会话唯一新增的失败是原文 CAS 冲突回 19。它发生在发路由之前，gate 根本没有收到路由，不会出现「gate 已在 kSent 提交而 scene 未执行」的三方分叉。
   - 残余（已登记的 R13，不是本次引入）：gate 重启后会话号被复用，scene_manager 也会按同会话放行。

3. SessionMap 只擦本玩家的映射（EraseSessionMappingIfOwned）：保持。本方案不碰任何 C++ SessionMap 擦除路径。

4. gate 的 loginGeneration 只经 ApplyLoginBinding/ApplyRouteIdentity 写入，只抬高、不丢路由：保持。
   - gate 代码不改。
   - scene 自发路由现在带实体代次，它 ≤ gate 会话值。ApplyRouteIdentity 对同一角色执行 MergeLoginGeneration 取 max（scene_route_helper.h:165-179），会话值不变；对别的角色不动。
   - scene_manager 拒绝请求意味着根本不产生路由，不属于 gate 丢弃路由。

其他不变量对照：
- 只新增字段号（PlayerLocation 8/9、LeaveSceneRequest 5/6），不复用；
- 码 25 属于 scene_manager 私有数轴，不是 tip 码，不触发 §7.5；
- Kafka target_instance_id 和 topic key 不变；
- 不新增以 player_id 为 label 的指标；
- 登录代次不进存盘 CAS（G5），本方案只用于落点裁决，不进 DBTask 和 owner_epoch。
**tests**: 先补回归测试，在 HEAD 上应为红：

【go/scene_manager/internal/logic】miniredis 夹具沿用 leavescene_cas_test.go 与 login_generation_test.go
1. TestLeaveScene_StaleLeaveAfterSamePlacementRelogKeepsLocation（b1）：location 为 S1/节点10/身份(11,gA)，人数为 1。B(22,gB) 走 samePlacement 的 EnterScene，然后 LeaveScene{S1, 11, gA}。断言 location 仍在、身份为(22,gB)、人数仍为 1。HEAD 上 location 会被删、人数归 0。
2. TestLeaveScene_SameConnectionRelogKeepsLocation：同一会话号 11，location 代次 g2，离场代次 g1 < g2，断言不删。用来钉住「不能按会话号精确匹配」。
3. TestLeaveScene_NewerPlacementResolvedByLocatorKept（b2）：B 已把 location 写到 S2，身份(22,gB)，LeaveScene{S2, 11, gA}，断言不删、S2 人数不变。
4. TestLeaveScene_SameSceneRewrittenByNewerLoginKept（b3）。
5. TestLeaveScene_OlderLocationIdentityIsDeleted：location(11,g1)，LeaveScene(22,g2)，断言删除且减一次人数。防泄漏，行为同旧。
6. TestLeaveScene_LegacyRequestWithoutIdentityDeletes 与 TestLeaveScene_LegacyLocationWithoutIdentityDeletes：兼容旧行为。
7. TestLeaveScene_EqualGenerationDifferentSessionNotDeleted。
8. TestEnterScene_StaleLoginAfterNewerSamePlacementRejected（d1）：B 先 samePlacement；A(11, gA<gB) 指定同节点的 S2。断言回 25，location 原文不变，S1/S2/节点人数不变，Kafka 无消息。HEAD 上会改写成 S2。
9. TestEnterScene_StaleSessionSceneChangeRejected（d2）：location 身份(22,gB)，请求 session 11、代次 gA、CorrelationId≠0。断言回 25，什么都没写。
10. TestEnterScene_SameSessionWithoutGenerationAdmittedPreservesGeneration：旧 scene 兼容，同会话代次 0 放行，location 的代次保持 gB。现有的 TestEnterScene_RouteEventCarriesRequestLoginGenerationAcrossSamePlacementTakeover 必须仍然通过。
11. TestEnterScene_UnversionedDifferentSessionAdmittedAsLegacy：钉住混版口径；若用户改为拒绝则相应改断言。
12. TestEnterScene_SamePlacementUpdatesIdentityPreservingOtherFields：location 带 rollback_receipt、pending、update_time，samePlacement 之后只有 8/9 变，owner_epoch 键不变，人数不变。
13. TestEnterScene_SamePlacementIdentityCASFailsWhenLocationDeleted：用 miniredis 钩子在 GET 与 EVAL 之间删 location，模拟 LeaveScene，断言回 19、不发路由。
14. TestEnterScene_PlacementRejectedWhenIdentityChangedSinceObservation：钩子改写身份，同节点不铸造落点回 19，location 不被覆盖。
15. TestSetLocationIfEpoch_ReplayIsIdempotent 与 TestMintScript_LocationExpectation：直接执行 Lua，覆盖 any/missing/present 三种模式和重发分支。
16. TestEnterScene_SamePlacementRouteFailureRollsBackIdentity：Kafka writer 返回错误时，location 退回旧原文，人数不动。
17. TestEnterScene_CrossZoneFirstLegStaleRejected：陈旧请求不写等待落点。
18. TestDecidePlacementIdentity_Table 与 TestDecideLeaveIdentity_Table：纯函数逐行覆盖，与 C++ EnterFence.* 对照。

【go/player_locator/internal/logic】
19. TestMarkOffline_LeaveSceneCarriesDeletedSessionIdentity：用实现 smpb.SceneManagerClient 的假 client 捕获请求，断言 SessionId 和 LoginGeneration 取自被删的会话记录。
20. TestLeaseExpiry_LeaveSceneCarriesSessionIdentity。
21. TestMarkOffline_EnqueuedRetryPreservesGeneration。

【C++】
22. cross_zone_test：ActingLoginGeneration.*（comp 不存在、会话不一致、正常三种）；SmRejectedBeforeAnyPlacementWrite(25)==true。
23. 可选：在可测宿主里验证 RequestSceneChange 和交接重发的盖章。
24. routing_identity_test 全量回归。

【交给 Codex 执行】Claude 不运行（AGENTS §10.1）。
1. 在仓库根执行 pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build，再执行 proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml。
2. go/scene_manager 与 go/player_locator：GOTOOLCHAIN=local go test ./internal/... -count=1，并跑 go vet ./...。
3. C++：msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64，跑 cross_zone_test.exe 与 routing_identity_test.exe。
4. 联机：用 robot 做「LeaveGame 后立即同角色重进，落在同一频道」，检查 player:{id}:location 仍在、instance 人数等于 1。再回归 travel_smoke。

通过标准：原有用例数量不减少，且全部为绿。
**cross_item_dependencies**: - 前置：入场栅栏那一批（登录代次）是本方案的基础，包括 login 铸代次、PlayerSession.login_generation=14、EnterSceneRequest.login_generation=12、scene 的 PlayerLoginIdentityComp。它在 scene-enter-session-fence.md §11 记为「未编译、未测试」，b/d 的验证要排在它验证通过之后。
- a（Kick 加 player_id、把踢线挪到 SetSession 成功之后）：代码上独立。踢线丢失时，旧会话 A 会继续游戏，在本方案下它的换图会拿 25；a 落地后这种情况会减少。
- c（ReleasePlayer 带 owner_epoch）：会改同一个 enterscenelogic.go（:723-728 的 dispatchReleasePlayer），需要同批或串行合并。c 还要改 proto/scene_manager/scene_node_service.proto，那里目前有其他会话的未提交改动（AssetAckReceipt rpc），c 需要先与之协调；b/d 不碰这个文件。
- e（ExitGame 带登录代次、断线上报带 gate 实例号）：可以复用本方案在 enter_fence.h 新增的 ActingLoginGeneration。若 e 引入 gate_instance_id，以后可以把它扩进 location 身份和 LeaveScene，收紧 R13（会话号复用），字段号从 10 起。
- GO-3（cross-zone-scene-travel.md §12.3，在 PlayerLocation 中记属主进程实例）：storage.proto:41 写着「GO-3 的新字段从 8 起取」。本方案占用 8/9，GO-3 顺延到 10 起，需要拍板。
- 邻域问题：「同 scene_id 换节点的落点」在 6b（enterscenelogic.go:757）多算一人。迟到的 LeaveScene 原本碰巧抵消了它，b 修好后会显形，建议另开一项。
- 收件箱、trade、guild、match 的 Locator 只读 location，不受影响。b 修好后「误判不在线」会减少。
- Java 版：按 AGENTS §12 需要在 PARITY.md 登记。栅栏文档 §12 说 Java 结构上不存在这类问题（每次登录都在库行上自增 epoch），但 b/d 涉及的「位置记录和离场清理」在 Java 版里是什么形态，未核实。
**open_questions**: 1. 字段号：PlayerLocation 用 8/9 存身份，GO-3 顺延到 10 起，是否同意？GO-3 本身目前是「待用户拍板、不落」。
2. 无代次请求的口径：scene_manager 对「请求代次为 0、location 有代次、会话不同」判 legacy 放行（推荐，避免 login 混版或回滚时把 location 带代次的玩家锁在门外），还是与 scene 栅栏一样直接拒绝（更严格，但 location 比实体活得久，会造成 30 秒以上的登录卡死）？
3. 新错误码 25（ErrPlacementSessionSuperseded）还是复用现有码？推荐新码：语义清楚，C++ 可以列入「先于任何落点写」的表。
4. scene 自发请求复用 12 号字段带实体代次（推荐，最少改动），还是新开 13 号「acting_login_generation」，让 12 号保持「只由 login 铸造」的旧注释口径？
5. 同落点只改身份的那次写入，推路由失败后是否按 keep 回滚身份（推荐）？
6. 落点 Lua 加原文 CAS，会让并发的同节点不铸造落点从「后写者赢」变为拿 19 重试，是否接受？
7. LeaveScene 遇到「代次相等、会话不同」（违反 G2）时选择不删并打 ERROR，在契约被破坏时会有泄漏风险，是否接受？
8. 6b 在「同 scene_id 换节点」时多算一人的邻域问题，是并入本批修，还是另开？
9. 是否加 scene_manager_placement_identity_total 指标，并在 deploy/k8s 告警里观察 legacy 计数归零？
10. 未核实项：
   - b 的实际发生频率，现在没有任何指标；
   - scene 侧 HandleTravelEnterSceneReply 对未知非 0 码的分支，推测会走取证并解冻；
   - B 的入场是否会中止 A 在途的同 zone 交接冻结（与 d2 跨节点变体有关）；
   - Java 版的对应情况。
