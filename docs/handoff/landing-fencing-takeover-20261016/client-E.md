<!-- 只读分析代理产出（2026-10-16，接手会话工作流 wf_f7177272-747），原样保存供落码时查证；结论未经运行验证。 -->

**summary**: 客户端提交 24ff81a 的落地恢复主体（LandingSyncClient 与 SyncDigestStore）总体符合设计 §4.3：落地判定、在途标志、15 秒超时、15/30/60 秒退避、UNAVAILABLE 或 g 变化时全量拉、先读后拉且成功才 Ack、按玩家分键的缓存，都已在代码里落实，测试也覆盖了设计列出的 8 个场景。

但这批改动目前实际不生效。全仓 grep 结果是：LandingSyncBindings.BindTeam 和 BindFriendRequests 只在定义处和 EditMode 测试里出现；TeamUiRoot、SocialUiRoot、GuildUiRoot 都没有调用 Bind；BindGuild 根本不存在。所以每次落地只会取摘要、标脏，没有任何域会被拉取或 Ack，缓存永远停在只有 g、没有计数的状态。

另外有几点：
- 「序号戳」里的序号比较恒为真，推送也没法和具体请求对应。实际丢弃旧推送靠的是「本次落地已发过请求、还没拿到摘要、连接引用一致」三个条件，再加上「请求数大于推送数时全量拉」兜底。结论是安全的，但做法与设计文字不一致。
- PlayerPrefs 的键只按 player_id 区分，没有按环境、区或 gateway 隔离（低风险）。
- 客户端工作区不干净。TeamClient.cs、TeamUiRoot.cs、GameClient.cs、MessageIds.cs 等有别的会话正在改，没有提交（RegionProbe、邮件、排行榜、商贸相关，本次复核期间仍在变）。其中 TeamUiRoot.cs 正是 BindTeam 要接线的文件，接线前需要先和那个会话协调。

修复方案见 fix_plan：三个 UI 宿主接线；GuildClient 增加读取确认钩子、后台排队入口和隐藏时的 Drain；新增 BindGuild；顺带修掉接线后才会暴露的「打开窗口时 Refresh 因 Busy 被吞」问题。以上全部未编译，待 Codex 验证。
## findings
  - [0]
    **id**: E-1
    **severity**: high
    **title**: 消费者绑定没有接到任何 UI 宿主，落地恢复的客户端半边实际不生效
    **location**: D:/luyuan/mmorpg-client/Assets/Scripts/Game/Sync/LandingSyncClient.cs:543-584（HEAD，定义处）；Assets/Scripts/UI/Ugui/Team/TeamUiRoot.cs:105-123、232-244（在途，有他人改动）；Assets/Scripts/UI/Ugui/Social/SocialUiRoot.cs:49-61、125（HEAD）；Assets/Scripts/UI/Ugui/Guild/GuildUiRoot.cs:98-105、196-201（HEAD）
    **detail**: 在客户端全仓 grep LandingSyncBindings、BindTeam、BindFriendRequests、BindGuild，只命中定义处（LandingSyncClient.cs:543/550/572）、TeamClient.cs:115 和 GameClient.cs:191 的注释，以及测试 LandingSyncClientTests.cs:152/185。真实的宿主类是 TeamUiRoot（TeamClient 在 Update 的 `_game != game` 分支里创建，见 111-115 行）、SocialUiRoot（FriendClient 在 `differentGame` 分支创建，见 55-58 行）、GuildUiRoot（GuildClient 在 98-105 行创建），三处都没有调用 Bind。

后果是：每次落地，LandingSyncClient.Tick 照常发 GetSyncDigest，推送到达后 Apply 把各域标脏，但 _consumers 为空，DeliverDirty 不通知任何人。TeamClient 和 FriendClient 的 SyncReadStarted 都是 null，所有拉取都不取票，Ack 永远不会被调用。本地缓存只有 g，各域计数一直为空，下次落地照样全部域标脏。整条链路只有请求开销，没有收益。

用户可见的影响暂时有限：好友申请窗口和帮会窗口打开时本来就会拉，组队每次落地也会拉。但批次 E 交付说明里的「社交域回源确认」并没有真正发生。
    **suggested_fix**: 三个宿主各存一个 IDisposable 字段。在创建域客户端的那个分支里 Bind，在销毁域客户端之前 Dispose。只在 GameClient 实例变化时 Bind 一次；Toggle、HidePanel、OnDisable、Reset 都不碰这个绑定，这样不会重复 Bind，也不会泄漏。具体改法见 fix_plan 第 1、2、5 条。
  - [1]
    **id**: E-2
    **severity**: medium
    **title**: 帮会四个域没有消费者：缺 BindGuild、缺 GuildClient 的确认钩子和后台排队入口，GuildUiRoot 窗口隐藏时也不消费
    **location**: Assets/Scripts/Game/Guild/GuildClient.cs:378-397（DrainQueued）、401-431（Refresh）、449-459、624-644、716-739、821-847、1413-1437（Request），均为 HEAD；Assets/Scripts/UI/Ugui/Guild/GuildUiRoot.cs:126（HEAD）
    **detail**: LandingSyncClient.cs:541 的注释自己写着帮会「待 GuildClient 提供排队刷新入口与读取确认钩子后接入」。GuildClient 现在只有 RefreshQueued、ActivitiesQueued 等推送排队标志，没有 SyncReadStarted 钩子，也没有摘要专用的排队位。GuildUiRoot.cs:126 只在 `_window.IsVisible` 时调用 DrainQueued，窗口隐藏时任何排队都不会被消费。

还有一个需要权衡的事实：GuildClient.Reset（249-259 行）在每次落地时会清空 Info、Donations、Shop、Activities；进入经济页和活动页时，GuildWindow.MaybeAutoRequest（GuildWindow.cs:226-251）会因为快照为 null 自动拉取；帮会状态在窗口之外没有任何显示。因此后台主动拉经济页和活动页，在窗口打开之前不会产生任何可见效果，却会给每个帮众每次落地多 2 到 3 个 RPC（升级这类全帮扇出会让最多 100 人同时多拉）。而且 Request 的错误路径（1428-1436 行）会把 GuildClient 置成 RequiresReconnect 隔离状态，后台拉取超时会让玩家在没做任何操作的情况下就看到「请重新登录角色后再试」。
    **suggested_fix**: 给 GuildClient 增加三样东西：
- [Flags] GuildSyncRead 枚举和 `Func<GuildSyncRead, Action> SyncReadStarted` 钩子，在 Request 真正发出时取票，在各读取成功落定处确认；
- `QueueSyncRefresh(GuildSyncRead)` 和 `DrainSyncQueued()`；
- 新增 `LandingSyncBindings.BindGuild`。

域与请求的对应关系：
- GUILD：拉 GetPlayerGuild，Apply 成功或走到 NotInGuild 分支时 Ack；
- GUILD_APPLICATION：不在帮时拉 ListMyGuildApplications，MyApplications 落定后 Ack；在帮时不拉、不 Ack；
- GUILD_ECONOMY：GetGuildDonateOptions 和 GetGuildShop 两个请求都在摘要到达之后发出并成功，才 Ack；
- GUILD_ACTIVITY：拉 GetGuildActivities，Activities 落定后 Ack。

建议 GUILD 和 GUILD_APPLICATION 在窗口隐藏时也后台主动拉；ECONOMY 和 ACTIVITY 只被动确认（玩家进页时的自动拉取顺带 Ack）。如果坚持设计原文的「四域都主动拉」，只需在 BindGuild 里多注册两行。这一点需要用户拍板。完整改法见 fix_plan 第 3、4、5 条。
  - [2]
    **id**: E-3
    **severity**: medium
    **title**: 接入后台帮会拉取后，打开窗口时那次 Refresh 会因 Busy 被静默吞掉
    **location**: Assets/Scripts/Game/Guild/GuildClient.cs:401-431、1418（HEAD）；Assets/Scripts/UI/Ugui/Guild/GuildUiRoot.cs:145、177（HEAD）
    **detail**: Refresh 在 Request 被 Busy 挡住时（1418 行 `if (Busy) return;`）只把 RefreshQueued 恢复成调用前的值（430 行），不会新排一次。Toggle（145 行）和 OpenPendingTrialInvite（177 行）都直接调用 `_client?.Refresh()`。

现在帮会请求只在窗口打开时发，所以这个问题几乎不会出现。一旦按 E-2 加了后台拉取：落地后后台的 GetPlayerGuild 或 ListMyGuildApplications 还在途时玩家按 G 打开窗口，这次 Refresh 会被吞，窗口停在旧状态或「请刷新帮会信息」，玩家必须手动点刷新。
    **suggested_fix**: GuildClient 新增 `public void QueueRefresh()`，只设置 RefreshQueued = true。Toggle 和 OpenPendingTrialInvite 改成 `if (_client.Busy) _client.QueueRefresh(); else _client.Refresh();`，窗口可见后下一帧由 DrainQueued 发出。这条必须和 E-2 同批改。
  - [3]
    **id**: E-4
    **severity**: low
    **title**: 「序号戳」的序号比较恒为真；推送和拒绝回包都无法与具体请求对应
    **location**: Assets/Scripts/Game/Sync/LandingSyncClient.cs:339-347、350-352、367-375、385-390、324-326（HEAD）
    **detail**: SendRequest 每次都执行 `_stampSeq = ++_requestSeq`，所以 StampIsCurrent 里的 `_stampSeq == _requestSeq` 永远成立。HandleDigest 根本不比较序号。

实际丢弃旧落地推送靠的是三个条件：`_sentThisLanding == 0`、`_hasDigest`、连接引用比较。新连接上无法区分的迟到推送，由 BeginLanding 里「`_requestsSent > _pushesReceived` 时本次全量拉」兜住，L-1 和 L-2 都成立。

残留的小问题有两个：
- 同一次落地里，第一个请求超时后发了第二个；第一个请求被 gate 拒绝的回包晚到时，HandleRequestReply 会把第二个请求判失败，多退避一轮。GameClient 收包时 mc.Id 会回显请求号（GameClient.cs:1686），但 SendOneWay 不返回这个号，没法对应。
- 被 gate 拒绝的请求和 TrySendOneWay 实际没发出的请求，都已经计进了 `_requestsSent`，却永远不会有推送回来。这会让 120 秒内的下一次落地无谓地全量拉。
    **suggested_fix**: 可选，属于低优先级：
- 在 HandleRequestReply 确认被拒时补一次 `++_pushesReceived`，表示这个请求不会再有推送；
- 删掉恒真的 `_stampSeq` 比较，或者在类注释里写明「序号只用于日志」，并把设计 §4.3 第 2 条改成按实际机制描述；
- 如果以后要精确对应，让 IBattleTransport.SendOneWay 返回请求号，拒绝回包按 mc.Id 匹配。
  - [4]
    **id**: E-5
    **severity**: low
    **title**: PlayerPrefs 摘要缓存键没有按环境、区或 gateway 隔离，也没有清理
    **location**: Assets/Scripts/Game/Sync/SyncDigestStore.cs:108-110（HEAD）
    **detail**: 键是 `mmorpg.syncdigest.{playerId}`。不同角色之间按 player_id 隔离；换号时 BeginLanding 会重新载入对应玩家的记录，Ack 由批次号绑定在当前玩家上，不会串号，这一点成立。

问题在于：同一台机器切换不同的 gateway 或环境（本地开发服和共享测试服、清库后 player_id 被重新发号）时，同一个 player_id 会读到另一个环境的缓存。g 是 hash 创建时刻的毫秒数，正常情况下必然不同，会触发全量拉，所以通常无害。只有两边都是 g=0（摘要从没 bump 过或已过期）时，才可能把计数相等误判成没变而不拉。

仓库里有先例，ClientSettings.cs:72 的 `mmorpg.lastplayer.{zoneId}` 就带了区号。此外，缓存键会随登录过的角色数增长，没有任何清理。
    **suggested_fix**: 可选：键里加上 gateway 地址的短哈希（或区号），比如 `mmorpg.syncdigest.{envHash}.{playerId}`。PlayerPrefsSyncDigestStore 的构造函数收一个命名空间参数，由 GameClient 传入 Session.GatewayBaseUrl 的哈希。另外，格式版本号目前是 "1"，改键之后旧键会自然失效，不需要迁移。
  - [5]
    **id**: E-6
    **severity**: low
    **title**: FriendClient.QueueRefresh 排上的落地拉取，会因为另一次在途请求失败而丢失
    **location**: Assets/Scripts/Game/Social/FriendClient.cs:41-48、214-219（HEAD）
    **detail**: 摘要触发 QueueRefresh 时，如果 FriendClient 正忙于别的请求（例如玩家刚点了同意），只会设置 `_refreshQueued = true`。之后那次请求如果走了错误路径（218 行 `_refreshQueued = false`），这次落地拉取就没了。而 LandingSync 已经在 DeliverDirty 里把 Pending 清掉，这一批不会再通知第二次。脏标还在，要等到下次落地或玩家打开申请窗口。这符合设计「失败保留脏标」的口径，代价只是这次落地少拉一次。
    **suggested_fix**: 可以不改。如果要改：错误路径里保留 `_refreshQueued`，或者在 BindFriendRequests 里额外订阅 Disconnected 或下一帧复查 `sync.IsDirty(FriendRequest)`。我建议维持现状，只在设计文档里写明。
  - [6]
    **id**: E-7
    **severity**: low
    **title**: 实现与设计 §4.3 第 5 条有出入：没有 FriendInboxClient，也没有 pending 数角标；TEAM 实际几乎只靠面板刷新 Ack
    **location**: 服务端 docs/design/landing-recovery-sync-digest.md §4.3 第 5 条（主仓，标题行已有在途改动）；Assets/Scripts/UI/Ugui/Social/SocialWindow.cs:51（HEAD）
    **detail**: 设计写的是新增 FriendInboxClient，暴露 pending 数，并作为 NotifyFriendEvent 的唯一注册点。实现改为复用 FriendClient，界面上也没有任何好友申请角标，「好友申请」按钮不带计数。因此 FRIEND_REQUEST 即使按 E-1 接上，后台拉回的数据也要等玩家打开申请窗口才看得到，而打开窗口本来就会拉。

TEAM 和 TEAM_INVITE 没有注册消费者。落地探针通常早于摘要到达，所以不 Ack；之后只有面板每 30 秒的刷新、手动 Refresh 或推送触发的重拉才会 Ack。这与 BindTeam 注释的取舍一致，但缓存实际很少推进。
    **suggested_fix**: 把设计 §4.3 第 5 条同步成现状：复用 FriendClient、目前没有角标、TEAM 只做被动确认、帮会按 E-2 的取舍。如果希望落地恢复对玩家可见，另开任务给好友申请和帮会入口加「只来自拉回内容」的计数角标（符合 §3.5 第 4 条），需要用户决定。
  - [7]
    **id**: E-8
    **severity**: low
    **title**: EditMode 测试有缺口：没覆盖 120 秒窗口、绑定解除、消费者异常隔离，也没有任何宿主或帮会测试
    **location**: Assets/Tests/EditMode/Net/LandingSyncClientTests.cs（HEAD）
    **detail**: 设计 §4.3 第 6 条列出的 8 个场景都有对应用例：首登全拉、g 变化、只拉变化域、失败不推进、连续落地丢旧推送、超时退避、UNAVAILABLE、身份变化。另有坏包、迟到推送、重入、编解码、缓存读写失败等用例。

缺的是：
- StalePushHorizonSeconds（120 秒）的路径；
- BindTeam 或 BindFriendRequests 的 Dispose 之后钩子被置回 null、消费者被移除；
- DeliverDirty 里某个消费者抛异常不影响其他域；
- 回调里换了落地导致批次号变化时停止投递；
- 隐私只写在注释里，没有任何断言；
- 宿主接线（MonoBehaviour）和帮会消费者都没有测试。
    **suggested_fix**: 用例清单见 fix_plan 第 6 条。宿主接线没法在 EditMode 里直接测，交付说明里要写明需要联机手工验证。
  - [8]
    **id**: E-9
    **severity**: low
    **title**: 客户端工作区不干净：他人在途改动涉及 TeamClient.cs 和 TeamUiRoot.cs，24ff81a 本身依赖尚未提交的生成物
    **location**: D:/luyuan/mmorpg-client 工作区（git status --short）
    **detail**: 本次复核期间工作区还在变：第一次 status 时 TeamClient.cs 和 TeamUiRoot.cs 是干净的，之后出现了改动。

在途改动分两类：
- 手写代码：TeamClient.cs（PrepareMatchNetwork、StartMatch 改造）、TeamUiRoot.cs:114（注入 RegionProbe）、GameClient.cs（注册 NetworkProbe 编解码、Battle.RegionProbe）、BattleClient.cs，新增未跟踪的 RegionProbeClient.cs 及其测试。这批是地域探测会话的改动。
- 生成物与脚本：MessageIds.cs（现在含 253/254，另有邮件、排行榜、商贸、网络探测等号）、tools/gen_messageids.ps1、tools/gen_proto.ps1，大量 Generated 下的 handler 与协议类。其中包括本批需要的 SceneSceneClientPlayer{Get,Notify}SyncDigestHandler.cs，以及含 SyncDomain 的 PlayerScene.cs。

HEAD 的 MessageIds.cs 里没有 GetSyncDigest 和 NotifySyncDigest，HEAD 的 PlayerScene.cs 里也没有 SyncDomain。也就是说，24ff81a 单独检出时依赖这些尚未提交的生成物。LandingSyncClient.cs、SyncDigestStore.cs、FriendClient.cs、GuildClient.cs、GuildUiRoot.cs、SocialUiRoot.cs、LandingSyncClientTests.cs 目前没有在途改动。
    **suggested_fix**: 按 E-1 修 TeamUiRoot.cs 之前，先问地域探测会话，避免互相覆盖。BindTeam 的接线插在它新增的 `_client.PrepareMatchNetwork = ...` 那一行之后、`_client.Changed += SyncState;` 之前或之后都可以，不要动它那一行。提交时，253/254 的生成物要和本批一起入库，不要夹带邮件、排行榜、商贸、地域探测会话的改动；如果做不到，就请用户决定按整个工作区打快照。
  - [9]
    **id**: E-10
    **severity**: nit
    **title**: LandingSync 从不 Dispose；24ff81a 夹带了与本批无关的帮会限流文案和测试
    **location**: Assets/Scripts/UI/AppBootstrap.cs:237-243（HEAD）；24ff81a 中 GuildClient.cs:1327 与 GuildUiTests.cs:319-337
    **detail**: AppBootstrap.OnDestroy 只调用了 GameClient.Features?.Dispose()，没有释放 LandingSync。GameClient 随 App 一起销毁，影响很小。

24ff81a 的提交说明是落地恢复，但里面带了 `common_error.KRateLimitExceeded => "申请过于频繁"` 这条文案和对应测试，与本批无关。
    **suggested_fix**: AppBootstrap.OnDestroy 里补一行 `GameClient.LandingSync?.Dispose();`。夹带的改动已经入库，不必回退，在交接文档里注明即可。
**fix_plan**: 以下改动全部未编译，待 Codex 验证。改前请再执行一次 `git -C D:/luyuan/mmorpg-client status --short`；TeamUiRoot.cs 和 TeamClient.cs 有地域探测会话的在途改动，动手前先协调。

【1】Assets/Scripts/UI/Ugui/Team/TeamUiRoot.cs（在途文件，只追加，不动别人改的行）
- using 里加 `System` 和 `MmorpgClient.Game.Sync`。
- 字段：`private IDisposable _landingSync;`
- Update 里 `if (game != null) { ... }` 块中，在 `_client.Changed += SyncState;`（工作区 115 行）之后插入：
  `if (game.LandingSync != null) _landingSync = LandingSyncBindings.BindTeam(game.LandingSync, _client);`
- DisposeClient()（工作区 232 行）方法体第一行插入：
  `_landingSync?.Dispose(); _landingSync = null;`
  要放在 `_directory` 处理和 `if (_client == null) return;` 之前，保证一定释放。
- OnDestroy 已经调用 DisposeClient，不用改。
- Toggle、HidePanel、OnDisable 不碰绑定。`_client.Reset()` 不会清 SyncReadStarted，所以换角或重连不需要重绑。

【2】Assets/Scripts/UI/Ugui/Social/SocialUiRoot.cs（HEAD，干净）
- using 里加 `MmorpgClient.Game.Sync`。
- 字段：`private IDisposable _friendSync;`
- 51 行改为先解绑再销毁：
  `_friendSync?.Dispose();_friendSync=null;Client?.Dispose();Client=null;Friends?.Dispose();Friends=null;_game=game;`
- 55-56 行 `Friends=new FriendClient(...)` 之后加：
  `if(game.LandingSync!=null)_friendSync=LandingSyncBindings.BindFriendRequests(game.LandingSync,Friends);`
- 125 行 OnDestroy 开头加：`_friendSync?.Dispose();_friendSync=null;`，位置在 `Friends?.Dispose()` 之前。

【3】Assets/Scripts/Game/Guild/GuildClient.cs（HEAD，干净）
(a) 在 namespace 内、GuildRoles 附近新增枚举：
  `[Flags] public enum GuildSyncRead { None = 0, Guild = 1, MyApplications = 2, Donations = 4, Shop = 8, Activities = 16 }`
  注释写明「落地恢复读取种类；本类不认识 SyncDomain」。

(b) 新增公开成员：
  - `public Func<GuildSyncRead, Action> SyncReadStarted { get; set; }`
    注释照 TeamClient.cs:115-123：发出时调用，返回的委托在本次读取成功应用后调用一次，null 表示不需要确认。
  - `public void QueueSyncRefresh(GuildSyncRead reads)`：
    `if (_disposed || reads == GuildSyncRead.None) return; if ((reads & ~GuildSyncRead.Guild) != 0 && !HasLoaded) reads |= GuildSyncRead.Guild; _syncReads |= reads;`
    不写 Status，也不触发 Changed（隐私，设计 §3.5 第 4 条）。
  - `public void QueueRefresh()`：`if (!_disposed) RefreshQueued = true;`（给 E-3 用）。
  - `public void DrainSyncQueued()`：`if (_disposed || Busy || RequiresReconnect || !_net.IsReady) return; DrainSync();`

(c) 新增私有成员：`GuildSyncRead _syncReads;`、`Action _currentSyncAck;`，以及下面三个方法。

  - `private void CompleteSyncRead() { var ack = _currentSyncAck; _currentSyncAck = null; ack?.Invoke(); }`

  - `private void SendSync(GuildSyncRead bit, Action send)`：
    `int before = _generation; _syncReads &= ~bit; send(); if (_generation == before) _syncReads |= bit;`
    （没发出去就放回，与 Refresh 的 430 行同一做法。）

  - `private void DrainSync()`，按以下顺序处理：
    1. `if (_syncReads == None || Busy) return;`
    2. `if ((_syncReads & Guild) != 0) { SendSync(Guild, Refresh); return; }`
    3. `if (!HasLoaded) { _syncReads = None; return; }`
       帮会快照没拉成时其余位作罢，脏标留给下次落地或开窗，避免每帧重试。
    4. `if (Info == null) { _syncReads &= ~(Donations | Shop | Activities); if ((_syncReads & MyApplications) != 0) SendSync(MyApplications, LoadMyApplications); return; }`
    5. `_syncReads &= ~MyApplications;`
       已入帮时，服务端入帮那一刻已删光本人申请，所以不拉、不确认。
    6. 依次处理 Activities、Donations、Shop，每次只发一个：
       `SendSync(Activities, RefreshActivities)`、`SendSync(Donations, RefreshDonations)`、`SendSync(Shop, RefreshShop)`。

(d) DrainQueued（378-397 行）：
  - 最后一行 `if (ShopQueued) {...}` 的块内补 `return;`；
  - 方法末尾加 `if (!Busy) DrainSync();`。
  这样窗口可见时摘要位排在推送排队之后消费。

(e) Request<T>（1413 行）：
  - 签名加参数 `GuildSyncRead read = GuildSyncRead.None`。
  - 在 `int generation = ++_generation;` 之后加：
    `Action syncAck = read == GuildSyncRead.None ? null : SyncReadStarted?.Invoke(read);`
    写请求一律不取票。
  - 成功回调改为：
    `Busy = false; var saved = _currentSyncAck; _currentSyncAck = syncAck; try { success(response); } finally { _currentSyncAck = saved; } Changed?.Invoke();`
    保存再恢复，是为了让回调里嵌套的 Request 不会顶掉外层的确认委托（GuildUiVerification 的同步替身也安全）。
  - 错误回调丢弃 syncAck，不需要额外处理。

(f) 各读取处传入读取种类，并在成功落定后调用 CompleteSyncRead：
  - Refresh（409 行）：传 `read: GuildSyncRead.Guild`。
    NotInGuild 分支在 `if (MyApplications == null) MyApplicationsQueued = true;` 之后、return 之前调用 `CompleteSyncRead();`。
    正常分支把 426 行改为：
    `bool applied = Apply(response.Guild); if (applied && writeNotice != null) Status = writeNotice; if (applied) CompleteSyncRead();`
  - LoadMyApplications（452 行）：传 `MyApplications`；在 `MyApplications = new List<...>` 之后调用。
  - RefreshDonations（627 行）：传 `Donations`；在 `Donations = response; _donationsStale = false;` 之后调用。
  - RefreshShop（719 行）：传 `Shop`；在 `Shop = response; _shopStale = false;` 之后调用。
  - RefreshActivities（832 行）：传 `Activities`；在 `Activities = response; _activitiesStale = false;` 之后调用。
  - Browse（GetGuildRank）和 LoadApplications（审批人看的申请人列表）不传。申请人数量在 GuildInfo 里，归 GUILD 域。

(g) Reset（249 行）加 `_syncReads = GuildSyncRead.None; _currentSyncAck = null;`；Dispose（1441 行）同样加上。

(h) 可选：后台发出的那次 Refresh 不要消费 `_pendingNotice` 和 `_writeNotice`（加一个私有参数 background）。不加的话，只有「推送恰好落在摘要拉取在途期间」这个很窄的窗口里，「你已被请离帮会」的提示可能被提前取走。

【4】Assets/Scripts/Game/Sync/LandingSyncClient.cs（HEAD，干净）中的 LandingSyncBindings
- using 里加 `MmorpgClient.Game.Guild`；541 行的类注释改成「帮会经 BindGuild 接入」。
- 新增 `public static IDisposable BindGuild(LandingSyncClient sync, GuildClient guild)`：
  - 先判空，写法同 BindTeam。
  - `var economy = new EconomyAck(sync);`
  - 钩子：
    `Func<GuildSyncRead, Action> hook = read => read switch { GuildSyncRead.Guild => AckOnSuccess(sync, SyncDomain.Guild), GuildSyncRead.MyApplications => AckOnSuccess(sync, SyncDomain.GuildApplication), GuildSyncRead.Activities => AckOnSuccess(sync, SyncDomain.GuildActivity), GuildSyncRead.Donations => economy.Begin(GuildSyncRead.Donations), GuildSyncRead.Shop => economy.Begin(GuildSyncRead.Shop), _ => null };`
    然后 `guild.SyncReadStarted = hook;`
  - 注册两个主动消费者：
    `var r1 = sync.RegisterConsumer(SyncDomain.Guild, _ => guild.QueueSyncRefresh(GuildSyncRead.Guild));`
    `var r2 = sync.RegisterConsumer(SyncDomain.GuildApplication, _ => guild.QueueSyncRefresh(GuildSyncRead.MyApplications));`
  - ECONOMY 和 ACTIVITY 默认只被动确认（理由见 E-2）。如果用户要求主动拉，再加：
    `sync.RegisterConsumer(SyncDomain.GuildEconomy, _ => guild.QueueSyncRefresh(GuildSyncRead.Donations | GuildSyncRead.Shop))`
    `sync.RegisterConsumer(SyncDomain.GuildActivity, _ => guild.QueueSyncRefresh(GuildSyncRead.Activities))`
  - 返回值：
    `return new Binding(() => { r1.Dispose(); r2.Dispose(); if (guild.SyncReadStarted == hook) guild.SyncReadStarted = null; });`
- 新增私有类 `EconomyAck`，字段：`LandingSyncClient _sync; SyncPullTicket _ticket; bool _has, _donations, _shop;`
  - `Begin(part)`：`if (!_sync.TryBeginPull(SyncDomain.GuildEconomy, out var t)) return null; return () => Complete(t, part);`
  - `Complete(t, part)`：
    1. `if (!_has || _ticket.Epoch != t.Epoch) { _ticket = t; _has = true; _donations = _shop = false; }`
    2. 按 part 置位 `_donations` 或 `_shop`；
    3. `if (_donations && _shop) { _sync.Ack(_ticket); _has = false; }`
  - Epoch 是 internal，同一程序集可见。两个请求都在摘要之后发出并成功，才推进 ECONOMY，满足 L-2。
- 可选（E-4）：在 HandleRequestReply 确认被拒、调用 Fail 之前加 `++_pushesReceived;`；删掉 StampIsCurrent 里恒真的序号比较，或者改注释。

【5】Assets/Scripts/UI/Ugui/Guild/GuildUiRoot.cs（HEAD，干净）
- using 里加 `System` 和 `MmorpgClient.Game.Sync`；字段 `private IDisposable _guildSync;`
- 100 行，在 `_client.Dispose()` 之前加：`_guildSync?.Dispose(); _guildSync = null;`
- 103 行 `if (_client != null) { ... }` 块内追加：
  `if (game.LandingSync != null) _guildSync = LandingSyncBindings.BindGuild(game.LandingSync, _client);`
- 126 行改为：
  `if (_window.IsVisible) { _client?.DrainQueued(_window.ShowingApplications, _window.ShowingActivities); _window.Tick(); } else _client?.DrainSyncQueued();`
  这一行位于 120 行 `!_available` 的 return 之后，所以战斗中不拉，摘要位留着。
- 145 行（Toggle）和 177 行（OpenPendingTrialInvite）都改为：
  `if (_client != null) { if (_client.Busy) _client.QueueRefresh(); else _client.Refresh(); }`
  （修 E-3。）
- 198 行 OnDestroy，在 `_client.Dispose()` 之前加：`_guildSync?.Dispose(); _guildSync = null;`

【6】测试
(a) Assets/Tests/EditMode/Net/LandingSyncClientTests.cs 追加：
  - StalePushHorizon_OldUnansweredRequestStopsForcingFullPullAfter120s
  - BindingDispose_UnhooksReadHookAndConsumer：Team、Friend 各一例
  - ConsumerThrows_OtherDomainsStillNotified
  - Guild_DirtyQueuesHiddenPull_AppliedSnapshotAcks
  - Guild_NotInGuildReplyAcksGuild
  - Guild_ApplicationPulledOnlyWhenNotInGuild
  - Guild_EconomyAcksOnlyAfterBothDonationsAndShop
  - Guild_ReadSentBeforeDigestDoesNotAck
  - Guild_ErrorReplyKeepsDirty
  - Guild_BusyBlockedSyncBitIsRestored
  - Guild_ResetClearsSyncReads
  写法沿用 SyncFakeTransport，或者 GuildUiTests 里现有的帮会假传输。
(b) Assets/Tests/EditMode/Guild/GuildUiTests.cs 追加：
  - DrainSyncQueued_SendsWhileWindowHidden
  - Toggle_WhileBackgroundPullBusy_QueuesRefresh
  - 回归：原有用例在 Request 新增默认参数之后全部保持通过。

【7】AppBootstrap.cs:237-243，可选：OnDestroy 里补 `GameClient.LandingSync?.Dispose();`。

【8】文档（主仓，只追加或局部改）
- docs/design/landing-recovery-sync-digest.md §4.3 第 5 条同步为实际做法：复用 FriendClient、没有角标、TEAM 被动确认、帮会主动和被动的取舍；第 2 条把「序号戳」改成按实际机制描述。注意该文件头部已有他人在途改动。
- 交接文档 §3.1 标注本次复核结论。
- docs/PROGRESS.md 追加一条。

【交给 Codex 验证】
- 在 D:\luyuan\mmorpg-client 执行 `tools/client_compile_check.ps1`；
- Unity：`-batchmode -runTests -testPlatform EditMode -testFilter "LandingSyncClient|GuildUi|FriendRequest|Team"`；
- 通过标准：原有用例数不减、全绿；
- 联机：登录后日志依次出现「[sync] 落地:准备取摘要」和「摘要已到」；打开、关闭帮会窗口和好友申请窗口各一次；做一次跨 zone 重定向；确认没有重复请求，帮会也没有误入「请重新登录」的隔离状态。
## verified_ok
  - [0]
    落地判定成立：LandingSyncClient.cs:282-294 只在 `_inGame() && _net.IsReady` 时取 (PlayerId, 连接身份)，二者有一项变化就 BeginLanding。IsReady 对应 GameClient.IsGateReady（GameClientBattleTransport.cs:24）；InGame 只在收到 NotifyEnterScene 之后置真（GameClient.cs:883，工作区行号），只在 ResetConnectionState 里清零（GameClient.cs:2085），所以同 zone 换图不算落地，取摘要请求也一定发在实体建好之后（满足设计 §4.2 最后一条）。
  - [1]
    在途标志先于发送：SendRequest（339-347 行）先写 `_awaiting`、戳、截止时间和计数，再调用 SendOneWay；Tick 只在 `!_awaiting && !_hasDigest` 时才发。测试 ReentrantTick_DoesNotSendTwiceOrNotifyTwice 覆盖了这一点。
  - [2]
    超时与退避：DigestTimeoutSeconds = 15（67 行）；RetryBackoffSeconds = {15,30,60}，取 `Math.Min(_failures, 3) - 1`，第 3 次起封顶 60 秒（76、358 行）；收到摘要或新落地时 `_failures` 清零。测试 DigestTimeout_BacksOff15_30_60AndFallsBackToFullPullOnce 覆盖；只在第一次失败时全量通知（363 行）。
  - [3]
    旧落地推送的处理在结论上安全（机制见 E-4）：BeginLanding 重置 `_sentThisLanding` 和 `_hasDigest`；HandleDigest（385-390 行）丢弃本次落地还没发请求时到达的推送；无法区分的情况由 `_requestsSent > _pushesReceived` 触发本次全量拉（324-326 行）。有三个测试覆盖：ConsecutiveLandings、LandingBegunWhileRequestUnanswered、LatePushAfterTimeout。
  - [4]
    UNAVAILABLE 或坏包时删缓存并全量拉，Ack 不写计数（412-419 行，Ack 在 242 行判断 HasVersion）；g 变化时新建记录、改记新 g、全量拉（430-436 行）；比较一律用「不相等」，摘要里缺席的域按 0 处理（441-444 行）。
  - [5]
    先读后拉、成功才 Ack：TryBeginPull 只在标脏有效（摘要到达或失败兜底）之后才给票（219-227 行）；Ack 校验批次号、HasVersion 和版本（237-238 行），只把缓存写成落地时读到的值。TeamClient 在读请求真正发出时取票（TeamClient.cs:766），在带视图且非冲突的成功回包（396 行）或邀请列表成功（434 行）时确认；FriendClient 在 101 行取、119 行确认；错误回包都不确认。
  - [6]
    隐私（设计 §3.5 第 4 条）在现有代码里成立：全仓没有 UI 读取 IsDirty 或 LandingSyncClient.Changed；FriendRequestsWindow 只在可见时按拉回的 Pending 渲染；GetPendingRequests 只保留发给本人的申请（FriendClient.cs:111），被动方变化不会出现在界面上。
  - [7]
    换号不串数据：BeginLanding 把 `_record` 置空后按新玩家重新载入；Ack 和 SaveRecord 只写当前 `_player`，凭证的批次号随落地失效。测试 PlayerChange_UsesThatPlayersCache 和 IdentityChange_ResetsDirtyAndInvalidatesTickets 覆盖。
  - [8]
    生成的 SyncDigest handler 空壳（工作区未提交）不会顶掉 LandingSync 的注册：HandlerRegistry.Register 全仓没有调用方（见 GameClient.cs:1748 的注释，以及 grep 结果）。
  - [9]
    缓存读写失败一律按「没有缓存」处理，不会打断流程（502-520 行；PlayerPrefs 实现吞掉所有异常）。测试 StoreFailures_DoNotBreakTheFlow 覆盖。
  - [10]
    设计 §4.3 第 6 条列出的 8 个测试场景都有对应用例。
**notes**: 1. 只读复核：没有修改任何文件，没有新建文件，也没有运行任何编译或测试。客户端仓的读取已获授权，只用了 git show、git diff 和直接读文件。

2. 工作区在复核过程中一直在变：TeamClient.cs、TeamUiRoot.cs、MessageIds.cs、gen_messageids.ps1 是在两次 git status 之间变成已修改的，说明有别的会话或 Codex 正在跑生成脚本和地域探测功能。文中 TeamUiRoot 和 GameClient 的行号以复核时的工作区为准，其余文件都是 HEAD，与工作区一致。

3. 一个设计层面的观察，供用户决定：组队、好友、帮会三个域客户端每次落地都会 Reset，清掉本地数据；除了组队入口（TeamUiRoot.RenderState 显示申请数和邀请数，而组队每次落地本来就拉），好友和帮会在窗口之外都没有任何显示。所以即使按本方案把消费者接上，落地恢复对玩家的可见收益目前也主要是推进缓存、少拉几次。要让 L-1 对玩家可见，需要另外给好友申请和帮会入口加「只来自拉回内容」的计数角标。

4. 帮会 ECONOMY 和 ACTIVITY 是「被动确认」还是「主动拉」，需要用户拍板。默认推荐被动，理由见 E-2；改成主动只需在 BindGuild 里多注册两行。

5. 以下几点未核实：
   - 核心 Redis 是全局一份还是每个 zone 一份：如果是每个 zone 一份，跨 zone 往返会因为 g 不同而每次都全量拉，只影响性能；
   - 连到尚未实现 GetSyncDigest 的服务端（Java 版）时，gate 是否把未知消息号计入非法包、累计到阈值后断线：如果会，客户端每 60 秒重试一次可能在约 50 分钟后被踢；
   - Windows 上两个 Unity 实例共用 PlayerPrefs 时，互相 Save 会不会覆盖对方的键。

6. 相关路径（都在 D:/luyuan/mmorpg-client 下）：
   - Assets/Scripts/Game/Sync/LandingSyncClient.cs、SyncDigestStore.cs
   - Assets/Scripts/Game/Social/FriendClient.cs
   - Assets/Scripts/Game/Team/TeamClient.cs
   - Assets/Scripts/Game/Guild/GuildClient.cs
   - Assets/Scripts/UI/Ugui/Team/TeamUiRoot.cs
   - Assets/Scripts/UI/Ugui/Social/SocialUiRoot.cs
   - Assets/Scripts/UI/Ugui/Guild/GuildUiRoot.cs
   - Assets/Scripts/UI/Ugui/Guild/GuildWindow.cs
   - Assets/Tests/EditMode/Net/LandingSyncClientTests.cs
   - Assets/Scripts/UI/AppBootstrap.cs
