# 删 location 绑定"持有者已释放"(审计 §2.1 修复)

- 日期:2026-10-18
- 状态:设计定稿,代码与测试同批落在工作区;**未编译、未测试,待 Codex 验证**(命令见 §10 与 `docs/PROGRESS.md` 同日条目)。
- 来源:[data-overwrite-rollback-audit-20261018.md](data-overwrite-rollback-audit-20261018.md) §2.1 的三条 medium:
  load-handover#1(⊇ login-session-route#0)、load-handover#0、cpp-save-producer#0(= load-handover#2)。
- 关联:[scene-owner-reentry-barrier.md](scene-owner-reentry-barrier.md)(owner_epoch 与换手门)、
  [scene-enter-session-fence.md](scene-enter-session-fence.md)(登录代次)、
  [scene-enter-chain-residuals.md](scene-enter-chain-residuals.md)(b 离场身份、c 释放栅栏)、
  [player-async-save-loss-windows.md](player-async-save-loss-windows.md) §4.1、
  [gate-login-rpc-boundary.md](gate-login-rpc-boundary.md)(LeaveGame 走 gate → login)、
  [cross-zone-scene-travel.md](cross-zone-scene-travel.md) §12.6(A1′ / A2′)。

## 1. 问题

`player:{id}:location` 是换手门(CZ-4)的入口条件:location 在,跨节点落点必须出示当前 owner_epoch 的 handoff 标记;
location 不在,scene_manager 按"首次落点"处理 —— 铸造新 epoch、不过换手门(`enterscenelogic.go` 的 `crossNodeHandoff`
要求 `currentLoc != nil`,`guard.mint = !samePhysicalNode`)。

修复前,location 的删除只看"会话结束",不看"持有它的 scene 是否已经落盘、释放":

| 路径 | 删除点 | 持有节点此刻的状态 |
|---|---|---|
| (1) LeaveGame | login → player_locator `MarkOffline` 删会话后**同步**调 `LeaveScene` | gate 不处理 LeaveGame,TCP 不断就不会代发 ExitGame:实体还活着、没退出、没存盘 |
| (2) 断线租约 | 30s 到期,`LeaseMonitor` 调 `LeaveScene` | gate 断线时已代发 ExitGame,但退出存盘可能还在 Redis 重试里(16s 封顶持续重试,永不放弃,§4.1) |
| (3) gate 崩溃 | player_locator 对账器把会话转租约 → 同 (2) | 没有任何 ExitGame:实体成了僵尸 |

删掉之后的下一次进场:首次落点铸 E+1 →
- 落到别的节点:新节点从 Redis 读到持有节点**上一次成功存盘**的快照;持有节点之后的存盘被 owner CAS 拒
  (`redis_client.h` 的 `SaveWithCommitBarrier`),`HandlePlayerSaveRejected` 把实体当"已废黜"直接销毁(不存盘)。
- 落回同一节点、同一频道:同一条连接上 gate 的 `ApplyRoute` 只看 `pendingEnterGsType` / scene_id / 节点有没有变,不看
  epoch,E+1 的路由被当成重复事件吞掉(load-handover#0)。实体永远停在 E,下一次真正写盘必然被拒并销毁。

两种结局都是**回档到上一次成功存盘**。

`markofflinelogic.go` 的同步 `LeaveScene` 本身是为修"正常登出 location 永久残留"加的:那时 location 永不删除,
幽灵人数 +1、destroy-on-empty 永不触发,多节点区服下次登录选到别的节点被换手门永久拒绝。修复不能把它带回来(§5)。

## 2. 结论一览

| 问题 | 决定 |
|---|---|
| 什么时候允许删 location | 只有拿到"持有者已释放"的证据(§3 不变量 R1):当前代 handoff 标记已写出、持有节点确认已死、location 本身没有持有者。拿不到就**保留** location,回可重试错误 |
| 谁让持有者释放 | scene_manager 在拿不到证据时向持有节点发 `ReleasePlayer{expected_owner_epoch=E, session_ended=true}`;持有节点持有 → 按客户端断线退出(存盘后 A1′ 写标记);不持有 → 按 owner_epoch==E 条件补写凭证标记 |
| LeaveGame 的持有节点 | gate 收到 LeaveGame 时向会话绑定的 scene 代发 ExitGame(与断线同一封包),并清空会话的 scene 路由状态 |
| 重试 | player_locator 现有清理通道(at-least-once claim);MarkOffline 入队首拍延后 2s,之后按 claim 超时节拍;登出清理不给 gate 发 LeaseExpired |
| 永久卡死的兜底 | 等待超过 `LeaveSceneHolderReleaseWaitSeconds`(默认 600s)仍拿不到证据 → 按旧行为删除并计 `forced`、打 ERROR(有记录的 fail-open,§3.6) |
| gate 同节点新 epoch | `ApplyRoute` 加第 4 条:路由 epoch 大于上次实际转发给 scene 的 epoch 时,以 0 再转发一次 |
| 回档证据 | scene `[OwnerEpoch]` 行新增 `stale_owner_rejected_exiting`(退出中的实体被 owner CAS 拒 = 退出存盘从未落地);scene_manager `scene_manager_leave_holder_release_total{verdict}`。都不以 player_id 作 label |

## 3. 不变量与方案

### 3.1 不变量

- **R1(删 location 的前提)**:scene_manager 删除 `player:{id}:location` 之前,必须拿到以下证据之一,证明没有任何节点
  还持有该玩家在当前 owner_epoch(记 E)下**未落盘**的状态:
  1. `player:{id}:handoff` 的标记 epoch == E(A1′ 退出标记、交接标记、改派标记、本文的凭证标记都满足"E 代状态已落盘、
     写标记者不再写 E");
  2. location 的持有节点确认已死且再入屏障已过,或整 zone 已无节点(与 EnterScene 死节点接管同一判据
     `playerLocationOwnerDead` / `playerLocationOwnerGone`);
  3. location 没有持有者(`node_id` 为空的等待落点)。

  例外(沿用旧行为删除并计数):E == 0(存量玩家,存盘本来不带 CAS,epoch 守卫保护不到);dev 旁路
  `AllowUnsafeCrossNodeHandoff=true`(换手门本身已被旁路);功能关闭(`LeaveSceneHolderReleaseWaitSeconds <= 0`)。
- **R2(判定与提交无窗口)**:证据 1 在删除 Lua 里原子复核:location 原文未变、owner_epoch 仍为 E(缺键视作未变)、
  标记 epoch == E,三者同时成立才删。Go 侧的预读只决定走哪种模式。
- **R3(释放请求只发给可证明属于离场会话的持有者)**:只有离场身份判定为 `deleted`(location 的登录身份 ≤ 被删会话,
  scene-enter-chain-residuals.md §3.2)时才发 `session_ended` 释放;`legacy`(任一侧无身份,混版窗口)只等证据、不发,
  因为那条 location 可能属于一次更新的登录,退出它等于踢掉在线玩家。
- **R4(释放只看归属代际)**:scene 侧沿用 `release_fence::Decide` 的严格相等;交接在途、缓存与期望不等一律不动实体。
  凭证只在本节点**完全不持有**该玩家(没有实体、没有在途载入、没有改派待确认条目)时写,且以 owner_epoch == E 为条件。
- **R5(gate 先通知再遗忘)**:gate 清空会话的 scene 指向之前,必须先向该节点代发 ExitGame;之后同一条连接上的进场
  只能由新的路由事件驱动,不按 LeaveGame 之前的旧路由补发。
- **R6(location 不永久残留)**:持有者活着时 R3 的释放会让它退出或写凭证;持有者死了走证据 2;两者都拿不到时 §3.6 的
  等待上限兜底。

### 3.2 scene_manager:LeaveScene

在现有"SceneId 比对 → 离场身份判定"之后插入释放判定(`leave_release.go`,纯函数 `decideHolderRelease` + 粘合):

| 顺序 | 条件 | 结论(指标 verdict) | 动作 |
|---|---|---|---|
| 0 | 身份 `keep_newer` / `keep_conflict` | — | 不变:不删、回成功 |
| 1 | 功能关闭或 dev 旁路 | `disabled` | 原文 CAS 删除(旧行为) |
| 2 | E == 0(owner_epoch 键与 location 记的 epoch 都为 0) | `legacy_epoch0` | 原文 CAS 删除 |
| 3 | location `node_id` 为空 | `no_holder` | 原文 CAS 删除 |
| 4 | 持有节点已死 / zone 已无节点 | `owner_dead` | 原文 CAS 删除 |
| 5 | 标记 epoch == E | `released` | "released" 模式删除(R2);Lua 复核不过 → 按第 6 行处理 |
| 6a | 其余,等待未超限,身份 `deleted` | `pending` | 保留 location;异步派发 `ReleasePlayer{E, session_ended}`;回 `FailedPrecondition` |
| 6b | 其余,等待未超限,身份 `legacy` | `pending_unproven` | 保留 location;不派发;回 `FailedPrecondition` |
| 6c | 其余,等待已超限 | `forced` | 原文 CAS 删除,ERROR 日志(接受回档风险) |

- E 取 owner_epoch 键的值;键为 0 / 缺失时取 location 记的 epoch(与 EnterScene "两处同时为 0 才算存量"同一口径)。
  键缺失而 location 记着 N 时,释放模式的 Lua 接受缺键(标记只能在键为 N 时写出,已有的 "N:t" 仍是有效证据)。
- 删除 Lua(`luaDeletePlayerLocationIfReleased`)在原脚本上加 owner_epoch / handoff 两个键与模式参数;删 location 与扣人数
  仍在同一次提交里(GO-4 残余的修法不变)。返回 1 已删 / 0 键已不在 / -1 原文已变 / -2 未释放。
- `FailedPrecondition` 不是故障:go-zero 的熔断不计它;player_locator 据此识别"等持有者释放"(§3.4)。
- 等待计时:`player:{id}:leave_release_wait = "{E}:{node}:{sid}:{gen}:{first_ms}"`,EX = 上限 + 600s。第一次拒绝时写入,
  location 的 (E, 节点, 登录身份) 变了就重置;删除成功后尽力删掉。读写失败按"刚开始等"处理(不会因此提前 forced)。
- 派发 `dispatchSessionEndedRelease`:后台协程,单发 1s 截止,不在内部重试 —— 下一次 LeaveScene 重试会再派发。包级钩子
  `sessionEndedReleaseDispatcher` 只为单测同步断言。

### 3.3 scene_manager → scene:`ReleasePlayer.session_ended`

`proto/scene_manager/scene_node_service.proto` 的 `ReleasePlayerRequest` 追加:

```proto
  // 玩家的登录会话已结束(登出 / 断线租约到期 / gate 消失),没有接手节点。docs/design/location-release-binding.md §3.3。
  // true 时 scene:持有该玩家且缓存 owner_epoch == expected → 按客户端断线退出(存盘落地后写 A1′ 断线释放标记);
  // 本节点完全不持有 → 按 owner_epoch == expected 条件补写凭证标记;其余忽略。false = 交接释放(原语义)。
  bool session_ended = 5;
```

scene 侧 `PlayerLifecycleSystem::HandleSessionEndedRelease(playerId, expectedOwnerEpoch)`(守护段按字段分派):

1. 本节点有该玩家实体:走与交接释放同一个 `release_fence::Decide`;判释放 → `HandleExitGameNode(kClientDisconnect)`。
   已在退出中 → `MergeExitCause` 只粘性置 `clientDisconnected`,原因不变。退出收尾的 A1′ 由既有判定决定写不写。
   - 为什么用 `kClientDisconnect` 而不新增原因:语义就是"客户端会话已死";它在 A1′ 的可写集合里,并且会作废疏散票据
     (给死会话改派没有意义)。来源区分由 `[ReleaseFence]` 行的 `session_ended` 计数承担。
2. 本节点不持有(`playerList` 没有、没有待入场条目、没有改派待确认条目):纯判定 `release_fence::DecideSessionEndedCredential`:
   期望 epoch 为 0、A1′ 开关关闭、本节点身份不确认、dev 旁路开着 → 不写;否则 `SendConditionalMarkWrite(E)`
   (`MarkWriteSite::kSessionEndedCredential`,与 A1′ 同一段 Lua,owner_epoch != E 时不写)。
   安全性与"改派踢线前的凭证补写"同理:本节点没有该玩家的任何内存态,盘上就是最终态;条件写保证不会给更新的一代发证。
3. `playerList` 里留着无效句柄 → 不写凭证(表不一致,fail-closed)。

### 3.4 player_locator

- `notifySceneManagerLeave`:`status.Code(err) == codes.FailedPrecondition` → 包成 `errHolderReleasePending`。
- `MarkOffline`:同步 LeaveScene 回 `errHolderReleasePending` 时打 INFO(不是故障),入队的 ready 分数延后
  `holderReleaseFirstRetryDelay = 2s`(给 gate 代发的 ExitGame / 释放请求留出落盘时间,首拍重试大概率拿到标记);
  其它错误照旧立即入队、打 ERROR。
- **入队载荷清空 `gate_id` / `gate_instance_id`**:登出的连接仍在线(停在选角界面)。原先入队的清理到期后会给 gate 发
  `PlayerLeaseExpired`,gate 对仍连着的会话 `forceClose` —— 修复前只在 LeaveScene 同步失败时才入队,这条缺陷少见;
  现在"等持有者释放"是常态,必须一并修掉。
- `handleLeaseExpiry`:LeaveScene 回 `errHolderReleasePending` 时打 INFO,claim 照旧保留、按 claim 超时(30s)重试。
- 不改 claim / 会话 Lua:清理在途时首次登录 SetSession 仍回 `ErrSessionCleanupPending`(既有屏障)。代价见 §6。

### 3.5 gate

**LeaveGame**:`DispatchClientRpcMessage` 在协议分派前对 `ClientPlayerLoginLeaveGame(17)` 调
`gate_scene_entry::OnClientLeaveGame`:

1. `GateClientSessionHandler::NotifySceneExitGame`(从 `HandleConnectionDisconnection` 抽出的同一段代码,带 player_id 反向
   身份栅栏)向会话绑定的 scene 代发 ExitGame;
2. `gate_scene_route::ResetSceneRoute` 清空节点指向、sceneId、待入场登录类型、欠账、lastSceneRoute、ownerEpoch、
   homeZoneId、forwardedOwnerEpoch;保留 playerId、loginGeneration、sessionVersion(下一次 BindSession 按既有规则处理)。

在收包时做而不是等 login 回包:直连 / 路由两种模式共用一个点;LeaveGame 在 login 侧失败时实体退出、location 保留,
下一次 EnterGame 走同落点重连或换手门(均不回档),是良性的。"之后同连接的 Bind 按 LeaveGame 之前的旧路由补发"这类
陈旧补发因此消失:Bind 先到时 `lastSceneRoute.sceneId == 0`,等新路由到达再一起转发。

**ApplyRoute 第 4 条**(load-handover#0):`SessionInfo` 新增 `forwardedOwnerEpoch` = 上一次实际转发给 scene 的
`PlayerEnterGameNodeRequest.owner_epoch`。`sceneId != 0`、`forwardedOwnerEpoch != 0`(scene 此前被告知过一个 epoch)且路由
epoch **大于** `forwardedOwnerEpoch` 时,以 LOGIN_NONE(0) 转发一次;转发成功后记下这次带出的 epoch。

- 只认"更大":epoch 只增不减,迟到的旧路由不值得再转发;0(旧版 scene_manager)永不触发。
- scene 从没被告知过 epoch(路由先于登录类型到达、还没转发过)时不触发:下一次转发本来就会带上会话当前的值。
- scene 侧对它的处置已有:恰好 +1 → 复用实体、`EnterScene` 把缓存抬到新值(退出中的顺带取消退出);≥ +2 →
  `DiscardDeposedEntityOnReentry` 销毁重载(中间有别的持有者,复用才是回档)。gate 以前把这些路由吞掉,scene 的判据从来见不到。

### 3.6 等待上限(有记录的 fail-open)

拿不到证据的合法原因:持有节点存盘卡住(Redis 对该节点写失败)。这正是要等的情形,删了就回档。但还有拿不到证据的
非预期情形:A1′ 开关被运维关掉、实体缓存 epoch 落后于 location(已被废黜的僵尸,`release_fence` 判 `skip_ahead_of_holder`)、
标记写与凭证写长期失败。清理不 ack 会让该玩家的首次登录一直回 `ErrSessionCleanupPending`,不能无限期。

所以等待超过上限(默认 600s,`LeaveSceneHolderReleaseWaitSeconds`)按旧行为删除:计 `forced`、打 ERROR(带 E、节点、
已等时长)。这是 AGENTS §11.3 允许的、带日志与指标的 fail-open:比修复前"立刻删"严格得多,只把回档留给持续 10 分钟拿不到
释放证据的情形。`forced` 正常应恒 0,出现即查。

### 3.7 指标

| 指标 | 位置 | 含义 |
|---|---|---|
| `scene_manager_leave_holder_release_total{verdict}` | scene_manager | §3.2 判定表的结论;`pending` 是登出 / 断线后的正常短暂态,`forced` 应恒 0 |
| `stale_owner_rejected_exiting` | scene `[OwnerEpoch]` WARN 行 | owner CAS 拒绝时实体正在退出 = 这次退出的存盘从未落地、内存差额被丢弃。**回档的直接证据**,应恒 0 |
| `session_ended` / `credential_*` | scene `[ReleaseFence]` 行 | 会话结束释放的次数、凭证判定分布与条件写结果 |

全部不以 player_id 作 label;玩家号只进日志。

## 4. 时序走查

记 X 为持有节点、E 为当前 owner_epoch,A 为旧会话 (sA, gA)。

**LeaveGame 后不断线就重进(同一连接)**

1. gate 收到 LeaveGame:代发 ExitGame(sA) 给 X,清空会话路由状态;转发 LeaveGame 给 login。
2. X 退出存盘 → A1′ 写 "E:t" → 销毁实体。与此并行:login → MarkOffline 删会话 → LeaveScene:
   - 标记已在 → `released` → 删 location;
   - 标记还没写 → `pending` → 派发 session_ended(X 已在退出,合并后无动作)→ player_locator 延后 2s 入队 → 2s 后重试拿到标记 → 删。
3. 客户端 EnterGame(同 session_id,新代次 gB):Bind 先到 → 会话没有旧路由,不补发;scene_manager:
   - location 已删 → 首次落点铸 E+1。X 已存盘销毁,新节点(或 X 自己)从盘上读到最新状态;
   - location 还在(2s 窗口内重登,首次登录被清理屏障挡住,客户端收 3023 重试;或 LeaveGame 在 login 侧失败、会话仍在)→
     同节点走同落点重连 / 同物理节点,跨节点过换手门(标记在就放行并铸造,不在回 18)。
4. 新路由到达 gate:按新 epoch 转发。

**LeaveGame 后快速重登(新连接或换设备)**:同上,区别只在 gate 会话是新的。location 保留期间登录过换手门,
不再走首次落点;迟到的离场重试看到更新的登录身份判 `keep_newer`(b 的既有规则)。

**断线租约到期时退出存盘还在重试(cpp-save-producer#0)**:LeaveScene 拿不到标记 → `pending`,location 保留、claim 保留,
派发 session_ended(实体已在退出,合并)。Redis 恢复 → 退出存盘落地 → A1′ → 下一次重试删除。期间首次登录被清理屏障挡住
(fail-closed,宁可登不上也不回档);等待超过 600s 才 `forced`。

**gate 崩溃**:对账器把会话转租约 → 到期 → LeaveScene `pending` → session_ended 让 X 上的僵尸实体按断线退出、存盘、写 A1′ →
下一次重试删除。修复前这一步直接删 location,僵尸手里未落盘的进度丢失。

**Bind 先到、按旧路由补发,随后同节点新 epoch 路由**(不经 LeaveGame 的残留路径,如 LeaveGame 在 gate 之外丢失):
补发带旧 epoch E;随后 E+1 的路由同场景同节点,第 4 条判 E+1 > E → 以 0 再转发 → X 复用实体并抬到 E+1。

## 5. 为什么不把"location 永久残留"带回来

那个缺陷的根因是"登出路径从不删 location"。现在:

- 登出 / 断线 / gate 崩溃三条路径都会调 LeaveScene,且失败一律进 at-least-once 重试(不变);
- 持有者活着时,`pending` 会派发 session_ended:持有 → 退出并写 A1′;不持有 → 写凭证。下一拍重试拿到证据就删;
- 持有者死了走证据 2;没有持有者走证据 3;
- 以上都拿不到时 §3.6 的上限兜底,最迟约 10 分钟后按旧行为删除。

所以 location 只会"晚删",不会"不删";晚删期间登录要么落回持有节点(复用内存实体,零损失),要么过换手门。

## 6. 代价与残余

1. **登出后 ~2s 内的首次登录**可能被清理屏障挡住(`ErrSessionCleanupPending` → 客户端 3023 重试),只在同步 LeaveScene
   早于 A1′ 标记时出现。gate 在收到 LeaveGame 时就代发 ExitGame,多数情况下标记先到。
2. **存盘卡住期间该玩家登不上**,最长到等待上限。这是有意的 fail-closed。
3. **`legacy` 身份不发释放**:混版窗口里 gate 崩溃留下的僵尸只能等上限兜底(与修复前同样回档,但晚 10 分钟)。全量升级后
   `pending_unproven` 应恒 0。
4. **`skip_ahead_of_holder` 的僵尸**(实体缓存落后于 location 记的 epoch)不写凭证,等上限兜底。它已被废黜、写不进任何东西,
   兜底删除不丢数据;登记备查。
5. **登录在清理之前、且被选到别的节点**:gate 崩溃后玩家立刻从别的 gate 重登,location 指向僵尸,换手门回 18,而释放要等旧会话
   的清理到期才发。修复前同样回 18(这不是本次引入的),登记为后续:可让登录类 EnterScene 在 `no_marker` 时也派发 session_ended。
6. 换手门证据的强度与 CZ-4 相同:同代标记存在即视为已落盘。撤回失败的交接标记(`WithdrawHandoffMark` 重试中)在 CZ-4
   与本文里是同一个已知窗口,不另做处理。

## 7. 新旧并存与发布顺序

- **发布**:proto 生成 → scene → player_locator → scene_manager → gate。
  - scene 先于 scene_manager:旧 scene 不认 `session_ended`,把它当交接释放(`kReleasedByTransfer`,A1′ 不写),
    不持有时也不写凭证,location 会一直等到上限。
  - player_locator 先于 scene_manager:旧 player_locator 把 `FailedPrecondition` 当普通失败,功能上可用(照样保留 claim
    重试),但入队载荷不清 gate、会踢掉登出后仍在线的连接。
  - gate 任意时刻都可以滚;第 4 条与 LeaveGame 处理不依赖其它组件。
- **回滚**:按反序。scene_manager 可单独把 `LeaveSceneHolderReleaseWaitSeconds` 设为 0 立刻退回旧行为(不必回滚二进制)。
  **要关 scene 的 A1′ 开关(`SCENE_EXIT_RELEASE_MARK=0`)之前,先把 scene_manager 的上限设为 0**,否则关掉之后拿不到标记,
  每次登出都要等满上限。
- 不改 Redis 键格式;新增键 `player:{id}:leave_release_wait`(带 TTL,丢了只会让等待重新计时)。

## 8. 测试与验收映射

| 验收项 | 用例(工程) |
|---|---|
| 持有者未释放:保留 location、不扣人数、回 FailedPrecondition、派发 session_ended(期望 = E) | `TestLeaveScene_HolderNotReleasedKeepsLocationAndRequestsRelease`(go/scene_manager) |
| 当前代标记 → 删;旧代标记不算 | `TestLeaveScene_CurrentEpochMarkerReleasesLocation`、`TestLeaveScene_OlderEpochMarkerIsNotRelease` |
| 判定与提交之间标记被撤回 → 不删 | `TestLeaveScene_MarkerWithdrawnBeforeDeleteKeepsLocation` |
| 例外:epoch 0、等待落点、关闭开关、dev 旁路 | `TestLeaveScene_LegacyEpochZeroDeletes`、`..._AwaitingPlacementDeletes`、`..._ReleaseWaitDisabledDeletesImmediately`、`..._DevBypassDeletesImmediately` |
| legacy 身份只等不发 | `TestLeaveScene_LegacyIdentityWaitsWithoutRequestingRelease` |
| 超过上限 forced | `TestLeaveScene_WaitBudgetExceededForcesDelete` |
| 判定表全覆盖 | `TestDecideHolderRelease_Table`、`TestLeaveReleaseVerdictNamesCoverEveryValue` |
| **LeaveGame 后快速重登**:location 保留期间跨节点登录过换手门而不是首次落点;标记写出后放行并铸造;迟到的离场判 keep_newer | `TestLeaveGameQuickReloginGoesThroughHandoffGate`(go/scene_manager) |
| 登出清理:pending 延后入队、载荷不带 gate | `TestMarkOffline_HolderReleasePendingEnqueuesDelayedCleanupWithoutGate`(go/player_locator) |
| 其它 LeaveScene 失败照旧立即入队、仍清 gate | `TestMarkOffline_LeaveSceneFailureEnqueuesImmediatelyWithoutGate` |
| 分类 | `TestNotifySceneManagerLeave_ClassifiesHolderReleasePending` |
| scene:持有 → 按断线退出;代际不等 / 交接中 → 不动 | `ReleaseFenceEcs.SessionEndedReleaseExitsHolderAsClientDisconnect`、`..._SessionEndedReleaseOnStaleEntityIgnored`、`..._SessionEndedReleaseDuringHandoffIgnored`(cross_zone_test) |
| scene:不持有 → 凭证;载入在途 → 不写 | `ReleaseFenceEcs.SessionEndedReleaseWithoutHolderWritesCredential`、`..._SessionEndedReleaseWithPendingLoadSkipsCredential` |
| 凭证判定表 | `ReleaseFence.SessionEndedCredentialDecideTable`、`ReleaseFence.SessionEndedCredentialNamesCoverEveryValue` |
| 回档证据计数 | `OwnerEpochStats.RejectedExitingSaveCountsAsRollbackEvidence`(cross_zone_test) |
| **LeaveGame 后不断线就重进**:重置后 Bind 先到不补发,新路由带新 epoch 与登录类型一起转发 | `SceneRouteEntry.LeaveGameResetThenRelogForwardsFreshRouteOnly`(routing_identity_test) |
| **Bind 先到按旧路由补发,随后同节点新 epoch** | `SceneRouteEntry.BindReplayThenSameNodeNewerEpochIsForwarded` |
| 第 4 条边界:同 epoch / 更旧 / 0 不转发 | `SceneRouteEntry.SameNodeSameOrOlderEpochStaysDeduped`、`..._ZeroEpochRouteNeverForwardsByEpoch` |

## 9. 改动清单

手写(生成产物由 Codex 重生,不手改):

- proto:`proto/scene_manager/scene_node_service.proto`(`ReleasePlayerRequest.session_ended = 5`)。
- scene_manager:`internal/logic/{leave_release.go(新), leavescenelogic.go, scene_node_client.go}`、`internal/config/config.go`、
  `internal/metrics/metrics.go`、`etc/scene_manager_service.yaml`;测试 `leave_release_test.go(新)`、
  `leavescene_cas_test.go` / `leavescene_identity_test.go`(夹具补释放标记)。
- player_locator:`internal/logic/{leasemonitor.go, markofflinelogic.go, session_cas.go}`;测试 `leave_release_pending_test.go(新)`。
- scene:`player/system/{release_fence.h, player_lifecycle.h, player_lifecycle.cpp}`、`core/system/redis.cpp`、
  `nodes/scene/handler/grpc/scene_node_service.cpp`(守护段)、`cpp/tests/cross_zone_test/cross_zone_test.cpp`。
- gate:`libs/services/gate/session/comp/session_info_comp.h`、`nodes/gate/handler/event/{scene_route_helper.h,
  scene_entry_dispatch.h, scene_entry_dispatch.cpp}`、`nodes/gate/handler/rpc/{client_session_handler.h,
  client_session_handler.cpp}`(工作区里由别的会话从 client_message_processor.* 改名而来)、`cpp/tests/routing_identity_test/routing_identity_test.cpp`。
- 文档:本文、审计 §2.1、`docs/README.md`、`docs/PROGRESS.md`。

## 10. 验证(Codex)

见 `docs/PROGRESS.md` 2026-10-18「删 location 绑定持有者释放」条目:先 proto-gen,再 Go 两个模块的 vet / test,
再串行 MSBuild 与 C++ 两个测试工程。通过标准:新增用例全绿,`leavescene_*`、`release_dispatch_test`、`ReleaseFence*`、
`SceneRoute*` / `SceneEntry*` 等既有用例不回归。

## 11. Java 版(AGENTS §12)

Java 版(`D:\luyuan\xuanming-server-mmo`)按其 `PARITY.md`(本次只读核对,未读代码)结构上不存在同类问题:

- 位置记录 `game:location:{player_id}` 由持有归属的 scene 唯一写入,没有"会话服务删位置、持有者还在写"这条路径,也没有
  scene_manager 侧的首次落点铸造(PARITY「场景分配」「短线重连与落点」行)。
- 归属靠库行 `owner_epoch` 夺权:新登录撞上仍被持有时请持有的 scene 让出(旧实例写回后释放),持有者 3s 内不让出回 2005、等
  30s 归属租约过期(PARITY「顶号」行),不会从旧快照起新实例。
- LeaveGame 下发 `UnbindPlayer`,gate 向 scene 发 `PlayerLeave{voluntary=true}` 并解绑玩家(PARITY「LeaveGame 后回选角」行):
  本仓本次补的"gate 收到 LeaveGame 代发 ExitGame"在 Java 版本来就有。

结论:本修复在 Java 版**不适用(结构不同)**,需在 `PARITY.md` 登记一行(待本仓提交后补 commit 号)。本次 proto 改动只在
服务端内部(`scene_node_service.proto` 的 `ReleasePlayerRequest.session_ended`),不涉及客户端契约;Java 仓需在本仓提交后跑
`ContractSync` 同步 `proto/` 副本。**待核实**:Java gate 转发层是否也有"同频道同节点按场景去重、吞掉新代际进场"的形态
(对应本仓 `ApplyRoute` 第 4 条)。
