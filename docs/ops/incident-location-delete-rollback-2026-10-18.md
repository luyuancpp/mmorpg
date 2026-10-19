# 事故报告:删 location 不等持有者落盘,玩家回档到上一次成功存盘

- **日期**:2026-10-18 发现并落码修复,2026-10-19 复核与补修(时间为本地 -04:00)。
- **级别**:P1(产品缺陷,默认配置可达的玩家进度丢失)。**线上影响:没有已知事故** —— 由只读审计发现,没有玩家报告,也没有在运行环境复现。
- **发现方式**:全仓数据问题与覆盖回档审计 [data-overwrite-rollback-audit-20261018.md](../design/data-overwrite-rollback-audit-20261018.md) §2.1,三条 medium:
  load-handover#1(⊇ login-session-route#0)、load-handover#0、cpp-save-producer#0(= load-handover#2)。对抗复核全部成立。
- **状态**:修复已落码。第一版在 `8b9592de3`;子会话的两项扩展(登录撞门请释放 §12、释放带覆盖身份 §13)在 `e611b5d8d`;
  2026-10-19 静态复核后的 gate 补修与文档改正**截至本文写入时未提交**(以 `git log -- cpp/nodes/gate/handler/event/scene_route_helper.h` 为准)。
  运行验证只完成了一部分,见 §6。
- **设计**:[location-release-binding.md](../design/location-release-binding.md)(方案、不变量 R1–R6、时序走查、残余)。
- **一句话**:玩家会话一结束(登出、断线租约到期、gate 崩溃),scene_manager 就删掉 `player:{id}:location`,不管持有这名玩家的 scene 节点
  有没有把内存落盘。location 一没,下一次进场按"首次落点"铸新 owner_epoch、不过换手门,新节点读到旧快照;旧节点之后的存盘被
  owner CAS 拒绝、实体被当作"已废黜"直接销毁。owner_epoch 守卫保证了旧数据不会盖到新数据上,代价是被拒的那段进度直接没了。

---

## 1. 现象(会表现成什么)

玩家重新进游戏后,背包、货币、任务、位置停在**持有节点最后一次成功存盘**的时刻,之后的进度消失。服务端可见的痕迹:

- 旧持有节点打 `HandlePlayerSaveRejected: owner_epoch CAS rejected save ... this node has been deposed`,`[OwnerEpoch] stale_owner_write_rejected` 递增;
- 新节点正常载入,没有任何报错;
- 修复前没有任何指标能把"被拒的是一次退出存盘"与其它拒绝区分开 —— 这就是它能藏这么久的原因(修复后加了 `stale_owner_rejected_exiting`,§5.5)。

四个触发形态:

| 编号 | 触发 | 时序 |
|---|---|---|
| A | **LeaveGame 回选角** | gate 不处理 LeaveGame,TCP 不断就不代发 ExitGame:持有节点的实体不退出、不存盘;login → player_locator `MarkOffline` 删会话后**同步**调 `LeaveScene`,location 立刻被删 |
| B | **断线租约到期时退出存盘还在重试** | gate 断线时已代发 ExitGame,但对该节点的 Redis 写在失败重试(16s 封顶持续重试,`player-async-save-loss-windows.md` §4.1);30s 租约到期,`LeaseMonitor` 调 `LeaveScene` 无条件删 location |
| C | **gate 崩溃** | 没有任何 ExitGame,实体成了僵尸;player_locator 对账器把会话转成断线租约,到期后同 B |
| D | **同连接 LeaveGame 后重进同一频道**(load-handover#0) | location 被删后首次落点铸出 E+1,恰好又派回同一节点同一频道;gate 的 `ApplyRoute` 只看登录类型 / scene_id / 节点是否变了,把这条路由当重复事件吞掉。持有节点的实体永远停在 E,下一次真正写盘必然被拒并销毁 |

客户端上线"回选角"之后,D 会**必然**触发;改包客户端今天就能触发 A。

## 2. 时间线

| 时刻 | 事件 |
|---|---|
| 2026-08-09 | 修"正常登出 location 永久残留"(幽灵人数、多节点区服下次登录被换手门永久拒绝):`markofflinelogic.go` 删会话后**同步**调 `LeaveScene`。本缺陷的形态 A 从这里开始存在 |
| 2026-09-18 | per-player 交接 epoch 与换手门落码(`scene-owner-reentry-barrier.md`、`cross-zone-scene-travel.md` CZ-4)。从此旧节点迟到的存盘会被 owner CAS 拒绝 —— 形态 A/B/C 的后果从"旧盖新"变成"丢新" |
| 2026-10-14 | 入场会话栅栏落码,`scene-enter-session-fence.md` §7 把"LeaveGame / 租约到期的 LeaveScene"登记为待办(当时关注的是迟到的 LeaveScene 误删新登录的 location) |
| 2026-10-16 | 进场链余项 b/c/d 落码:LeaveScene 带被删会话的身份,location 已属于更新的登录时不删。修好了"迟到的离场删掉新登录",没碰"删的时候持有者还没落盘" |
| 2026-10-18 | 全仓只读审计发现本缺陷(§2.1),对抗复核成立。同日写设计、落码修复与回归测试;子会话补两项扩展(§12、§13) |
| 2026-10-19 | 用户提交前两轮改动(`8b9592de3`、`e611b5d8d`)。多代理静态评审发现两处问题,补修(§5.6)。Codex 完成协议生成、六个 Go 服务编译与 scene_manager 受影响用例;C++ 尚未编过 |

## 3. 根因(逐环)

### 3.1 换手门的入口条件是"location 存在"

`go/scene_manager/internal/logic/enterscenelogic.go`:`crossNodeHandoff := currentLoc != nil && ...`,落点 `guard.mint = !samePhysicalNode`。
location 在,跨节点落点必须出示当前 owner_epoch 的 handoff 标记(持有者落盘后才写);location 不在,按首次落点铸造、不过门。
所以"删 location"等价于"宣布没有任何节点持有这名玩家" —— 而删除方从不核实这一点。

### 3.2 会话结束不等于持有者停笔

三条删除路径都只看会话:LeaveGame 的 `MarkOffline`、租约到期的 `LeaseMonitor`、对账器转租约后的 `LeaseMonitor`。持有节点什么时候
退出、存盘落没落地,player_locator 与 scene_manager 都不知道,也没问。A 里持有节点甚至根本不知道玩家已经登出。

### 3.3 epoch 守卫只保证"不脏",不保证"不丢"

旧节点之后的存盘带旧 epoch,`redis_client.h` 的 `SaveWithCommitBarrier` 原子拒绝;`player_lifecycle.cpp` 的 `HandlePlayerSaveRejected`
按"已被废黜"销毁实体,不重试、不存盘。设计上这是对的(旧节点确实被废黜了),但这里被废黜的节点持有的是**唯一的最新状态**。

### 3.4 gate 的去重键缺了 owner_epoch

`cpp/nodes/gate/handler/event/scene_route_helper.h` 的 `ApplyRoute` 判"要不要向 scene 转发进场"只看三件事:有没有待入场的登录类型、
scene_id 变没变、节点变没变。同场景同节点、但归属换了代的路由被当成重复事件。scene 侧对"恰好 +1 复用实体并抬到新代、≥+2 销毁重载"
早有处置,只是 gate 从不把这种路由交过去。

### 3.5 为什么会写成这样

- 同步 `LeaveScene` 是为修"location 永久残留"加的:当时 location 永不删除,修复方向是"会话一结束就删"。删了之后谁兜底,没有人问。
- 9-18 的 epoch 守卫让"新旧双写"不再脏数据,各层于是都默认"有 epoch 兜着"。epoch 兜住的是一致性,不是持久性。
- gate 被定位成协议路由器,不解析 LeaveGame;"回选角"客户端还没做,LeaveGame 这条路径没人真正跑过。
- 每一层的前提单独看都成立,组合起来才出问题:会话服务认为删 location 是清理,scene_manager 认为 location 不在就是没人持有,
  scene 认为被拒就是被废黜。

## 4. 影响面

- **丢什么**:持有节点最后一次成功存盘之后、到实体被销毁之间的全部进度。周期存盘的间隔就是单次最大损失;B 里 Redis 对该节点长时间
  写失败时,损失随故障时长增长。
- **不丢什么**:没有资产复制,没有旧数据覆盖新数据(owner CAS 与 go/db 的 applied-epoch 守卫都拦住了)。
- **默认配置可达**:A 只要客户端发 LeaveGame;B 需要退出存盘超过 30s 没落地;C 需要 gate 崩溃;D 需要 A 加上落回同节点同频道。
- **dev 环境看不到**:本地 `AllowUnsafeCrossNodeHandoff: true` 时换手门本来就放行,robot 冒烟覆盖不到这条路径。

## 5. 修复

完整方案与论证见设计文档;这里只列机制。

### 5.1 删 location 必须拿到"持有者已释放"的证据(设计 §3.1 R1、§3.2)

scene_manager `LeaveScene` 在离场身份判定之后,只有拿到三种证据之一才删:当前代的 handoff 标记(A1′ 退出标记、交接标记、改派标记、凭证标记);
持有节点确认已死且再入屏障已过;location 没有持有者(等待落点)。删除 Lua 原子复核 owner_epoch 与标记。拿不到就**保留** location、
回 gRPC `FailedPrecondition`,让下一次进场走换手门。等满 `LeaveSceneHolderReleaseWaitSeconds`(默认 600s,`-1` 关闭整项检查)
仍拿不到才按旧行为删,并计 `forced`(有记录的 fail-open,告警 `SceneManagerLeaveReleaseForced`)。

### 5.2 让持有者释放(设计 §3.3、§3.5)

- scene_manager 拿不到证据时向持有节点发 `ReleasePlayer{expected_owner_epoch, session_ended=true}`:持有 → 按客户端断线退出,存盘落地后写 A1′;
  完全不持有 → 按 owner_epoch 条件补写凭证标记;交接在途 / 代际不等 → 不动。
- gate 收到 LeaveGame 时先向会话绑定的 scene 代发 ExitGame,再清空会话的 scene 路由状态(先通知、后遗忘)。

### 5.3 player_locator 配合(设计 §3.4)

`FailedPrecondition` 识别为"等持有者释放",按正常态打 INFO;MarkOffline 入队的清理首拍延后 2s;登出清理的载荷清空 gate 字段
(顺带修掉"登出后仍在线的连接被 LeaseExpired 踢掉"的潜在缺陷)。

### 5.4 gate 去重补上 owner_epoch(设计 §3.5 第 4 条)

scene 此前被告知过一个非 0 的 epoch、而路由的 epoch 更大时,同场景同节点也以 LOGIN_NONE(0) 再转发一次(`SessionInfo.forwardedOwnerEpoch`)。

### 5.5 回档证据指标(设计 §3.7)

- scene `[OwnerEpoch]` 行 `stale_owner_rejected_exiting`:owner CAS 拒绝时实体正在退出 = 这次退出的存盘从未落地。**应恒 0**,出现即说明又发生了回档。
- scene_manager `scene_manager_leave_holder_release_total{verdict}`;scene `[ReleaseFence]` 行的 `session_ended` / `credential_*` / `binding_*`。
- 都不以 player_id 作 label。

### 5.6 扩展与复核补修

- **§12 登录撞上未释放的持有者**:gate 崩溃后玩家在旧会话清理之前就从别的节点重登,原先一直拿 18;现在 scene_manager 回 18 之前同步请持有节点释放。
- **§13 释放带覆盖身份**:同物理节点重登不铸 epoch,迟到的会话结束释放可能退掉刚复用实体的新登录;`ReleasePlayerRequest` 追加
  `covered_session_id = 6` / `covered_login_generation = 7`,scene 在代际放行后再比实体的登录身份,新登录不退。
- **2026-10-19 静态复核的两处**(6 个区域各一名评审,每条发现两名复核员独立反驳;3 条发现,2 条成立,1 条被推翻):
  1. 设计文档把回滚开关写成"设为 0 关闭",代码里 0 是默认 600s、只有 `-1` 关闭。照旧文档去关 A1′ 会让每次登出都卡满 600s。已改文档。
  2. gate 的"先通知后遗忘"有漏洞:LeaveGame 重置之后才到的旧链路由会重新填上 `lastSceneRoute`,下一次 Bind 按它带旧 epoch 补发。
     已加迟到过滤(`IsRouteFromBeforeLeaveGame`:同一角色、代次为 0 或不大于重置时的代次 → 丢弃)。

### 5.7 不变的东西

"location 永久残留"不会回来:三条路径照旧调 LeaveScene 并进 at-least-once 重试,持有者活着时被请去释放,死了走死节点证据,
都拿不到时上限兜底 —— location 只会晚删,不会不删(设计 §5)。

## 6. 验证状态(截至 2026-10-19,以 `docs/PROGRESS.md` 同日 Codex 条目为准)

| 项 | 状态 | 证据 |
|---|---|---|
| 协议生成(含 `session_ended=5`、`covered_*=6/7`) | 通过 | `build/verification/ownership-generation-current-20261019T043632035Z/` |
| 六个 Go 服务编译(db、login、scene_manager、player_locator、client_rpc_router、data_service) | 通过 | `build/verification/owned-go-source-20261019T045438353Z/` |
| scene_manager internal vet | 通过 | 同上条目 |
| scene_manager 受影响用例 | 54 个唯一用例分两轮取得成功记录,**不是单轮全绿**;首轮提前退出原因未定 | `ownership-sm-argumentlist-20261019T051125572Z/`、`...051445747Z/` |
| player_locator 测试 | 无结果 | — |
| C++ 编译、`cross_zone_test` / `routing_identity_test` | 无结果(当前整解曾在与本修复无关的 gRPC 生成模板 C2679 处停下) | — |
| 2026-10-19 gate 补修 | 未编译、未测试、未提交 | — |
| 真实环境故障演练(登出 / 租约到期 / gate 崩溃 / Redis 写失败) | 未开始 | — |

命令与通过标准见 `docs/PROGRESS.md` 的三条条目:「删 location 绑定持有者释放」「登录撞上未释放的持有者」「会话结束释放带覆盖身份」,
以及 2026-10-19「静态评审复核两项修正」。

## 7. 遗留与后续

1. **【待提交】** 2026-10-19 的 gate 补修与设计文档改正。
2. **【待验证】** §6 表里所有"无结果"的项;验收必须在 `AllowUnsafeCrossNodeHandoff: false` 下跑,dev 默认配置测不到。
3. **【代价,有意】** 登出后约 2s 内的首次登录可能被既有清理屏障挡住(3023 重试);持有节点存盘卡住期间该玩家登不上,最长到 600s 上限。
4. **【残余】** legacy 身份(混版窗口)只等证据不发释放;`skip_ahead_of_holder` 的僵尸等上限兜底;旧版 login 与重排同时出现时新登录路由可能被
   gate 的迟到过滤丢掉,等客户端超时重连。全部登记在设计 §6、§12.5、§13.5、§3.5。
5. **【告警】** `forced` 与 `stale_owner_rejected_exiting` 都应恒 0;`pending` 的速率基线要上线后再定阈值。
6. **【发布顺序】** proto 生成 → scene → player_locator → scene_manager → gate。关 scene 的 `SCENE_EXIT_RELEASE_MARK` 之前先把
   `LeaveSceneHolderReleaseWaitSeconds` 设为 `-1`。
7. **【Java 版】** 不适用(结构不同):Java 的位置记录由持有 scene 唯一写入,归属靠库行 owner_epoch 夺权与让出,LeaveGame 时 gate 已向 scene 发
   `PlayerLeave`(按 Java 仓 `PARITY.md` 核对)。`PARITY.md` 待本仓提交后登记一行。

## 8. 教训

1. **"会话结束"和"持有者停笔"是两件事。** 任何删除归属入口(location、租约、占位)的操作,都要绑定持有者自己给出的释放证据,
   或持有者已死的正面证据;会话层的信号不能代替。
2. **守卫保证不脏,不保证不丢。** owner_epoch / CAS 把双写变成了"后来者赢",被拒的一方就是丢失的一方。有守卫的地方要另配一个
   "被拒的是不是唯一的最新状态"的指标(这次是 `stale_owner_rejected_exiting`),否则丢数据在监控上是安静的。
3. **修残留时要问"删了之后谁兜底"。** 8-09 为修 location 残留加的同步删除,在 9-18 加上 epoch 守卫之后变成了回档入口。
   修复一类问题时引入的立即清理,要按后续加入的机制重新审一遍。
4. **去重键必须包含所有决定语义的字段。** gate 按"场景 + 节点"去重,漏了 owner_epoch,同场景同节点的换代被吞掉。
5. **重置状态之后,在途消息仍会到达。** LeaveGame 清空了会话路由,但之前发出的路由还在 Kafka 里;重置必须配一道"迟到过滤",
   否则旧状态会被迟到的消息重新写回。这一条是落码后的静态复核才抓到的。
6. **dev 旁路会把整条路径藏起来。** 本地 `AllowUnsafeCrossNodeHandoff: true` 时换手门放行,robot 全绿不能说明这条路径可用。
