# 接手记录：落地恢复与围栏交接的复核结论、拍板与落码计划（2026-10-16）

> 上游交接：[handoff-landing-recovery-and-fencing-20261016.md](../handoff-landing-recovery-and-fencing-20261016.md)。
> 本目录其余 9 份是只读分析代理的原始报告（未经运行验证），落码时按需查证。
> 本轮只做了分析与文档，**没有改任何代码**。

## 1. 负责人本轮拍板（2026-10-16）

| 事项 | 决定 |
|---|---|
| 来源不明的在途围栏实现 | 「你合并，并继续，按最标准的做」：由本线合并接管，按标准做法落码 |
| 253 号 GetSyncDigest 限流 | 接受默认档，不加 MessageLimiter 行 |
| 客户端绑定与 BindGuild | 直接改客户端仓 `../mmorpg-client` |
| 进场链 a–e | 设计与落码一起做 |

各项内的待选点，负责人没有逐条拍板，本线按「最标准」取各报告的推荐项，见 §3。

## 2. 复核结论速览

| 块 | 结论 | 报告 |
|---|---|---|
| 客户端 E | 主体逻辑符合设计；**三个 UI 宿主都没调用 Bind，BindGuild 不存在，落地恢复客户端半边实际不生效**（高）；另有 Busy 吞刷新等 2 中 6 低 | [client-E.md](client-E.md) |
| robot sync-smoke | 与设计 §10 第 6 步逐条对应，无假绿路径；6 个 low/nit；默认限流档下 1.1s 间隔安全 | [robot-sync-smoke.md](robot-sync-smoke.md) |
| 在途围栏（来源：「全域到账续批」会话，为资产实例过户/托管做 SQL 提交证据） | 4 高：被拒写静默 ACK 并推进游标；同版本重放用 proto.Equal 读回比对必误判；C++ 每实体计数器无墙钟下限；错误未打 storeerr 标记。另：无 legacy/dual 开关，C++ 回滚即静默丢盘 | [fence-inflight.md](fence-inflight.md) |
| v0.3 补审 | 主体可实现，SaveConditional 语义已对源码核对成立；但 §5.5（tonumber、先写游标后校验）、§4.6（Stop 取消 SQL 与 G1 冲突）、§6（未按 runDBOp 内 verify 重写）、§5.2/§10（重复指标、RedisSteps 固定契约）、§8.1 Stage 0 口径过时，需出 v0.4 | [fence-v03-review.md](fence-v03-review.md) |
| 13 项待拍板 | 逐项备忘与推荐 | [fence-decisions.md](fence-decisions.md) |
| a 踢线 | 竞态在 HEAD 仍成立，登录代次栅栏没有缓解 | [a-login-kick.md](a-login-kick.md) |
| b/d LeaveScene 与落点 Lua | 均成立；b 还有交接没写到的更宽变体（新落点被现读 location 的 LeaveScene 删掉） | [bd-leave-and-landing-lua.md](bd-leave-and-landing-lua.md) |
| c ReleasePlayer | 成立；只加 epoch 挡不住「落点/路由失败后 epoch 不动」那一类，必须同时把派发挪到提交之后 | [c-release-player.md](c-release-player.md) |
| e ExitGame / 断线 | 「三道守卫已精确」只在单实例前提下成立；player_locator 链有现实反例（30s 后误踢活玩家） | [e-exitgame-disconnect.md](e-exitgame-disconnect.md) |

## 3. 本线按「最标准」选定的做法

### 3.1 客户端（按 client-E.md fix_plan）
- 三个宿主（TeamUiRoot、SocialUiRoot、GuildUiRoot）各存一个 IDisposable，在创建域客户端的分支里 Bind、销毁前 Dispose。
- GuildClient 加 `GuildSyncRead` 读取种类、`SyncReadStarted` 钩子、`QueueSyncRefresh` / `DrainSyncQueued` / `QueueRefresh`；新增 `LandingSyncBindings.BindGuild`。
- **帮会 ECONOMY / ACTIVITY 只被动确认**（玩家进页时的自动拉取顺带 Ack），GUILD / GUILD_APPLICATION 后台主动拉。理由：窗口外无任何显示，后台拉只增 RPC，且 Request 错误路径会把 GuildClient 置成需重登隔离态。改主动只需在 BindGuild 多注册两行。
- 同批修 E-3（Busy 时 Refresh 被吞）、E-4（拒绝回包补计数、去掉恒真序号比较）、E-5（PlayerPrefs 键加环境哈希）、E-10（AppBootstrap 释放 LandingSync）。
- TeamUiRoot.cs / TeamClient.cs 有地域探测会话的在途改动：只追加，不动其行。

### 3.2 robot（按 robot-sync-smoke.md fix_plan，全做）
预清理传输失败即 fail 并复查残留；S3 识别 pending 同代次重放；S6 补三条与时序无关的硬断言；`syncSmokeConnect` 超时包装；tip 提示按消息号区分；文件头与 yaml 改成实际时序。

### 3.3 进场链 a–e
| 项 | 选定 | 要点 |
|---|---|---|
| a | 方案 B | 踢线挪到 SetSession 成功之后；`KickPlayerEvent` 加 `uint64 player_id = 2`、`uint64 login_generation = 3`，走 payload；gate 用纯函数 `ClassifyKick`（复用 ClassifyPush，代次 ≥ 则不踢）；SetSession 结果未知时短超时复读消歧；跳过同会话的判断改为比 (GateInstanceID, SessionID)；加 `login_kick_total{result}`；未绑定会话 fail-closed |
| b + d | 方案 A | `PlayerLocation` 加 `session_id = 8`、`login_generation = 9`（GO-3 顺延到 10 起）；`LeaveSceneRequest` 加 5/6；EnterScene 纯函数 `decidePlacementIdentity` + 两段落点 Lua 加原文 CAS；无代次请求 legacy 放行；新私有码 25 `ErrPlacementSessionSuperseded`；scene 自发请求复用 12 号字段带实体代次；同落点改身份推路由失败按 keep 回滚 |
| c | 方案 B | `ReleasePlayerRequest` 加 `uint64 expected_owner_epoch = 4`（取 observedEpoch）；派发挪到本请求提交之后；scene 纯函数 `release_fence::Decide`：期望 > 缓存忽略、缓存为 0 照旧释放并计数；响应保持 Empty |
| e | 方案 A | `SetDisconnectingRequest.gate_instance_id = 4`、`MarkOfflineRequest.expected_gate_instance_id = 4`，两边都非空且不等即忽略；只改 Go；与 b 同批生成 proto |
| 不做 | — | e 方案 B/C、§9 残余 7、6b「同 scene_id 换节点多算一人」：登记为残余，另立项 |

改动面：proto 6 个、go/login、go/player_locator、go/scene_manager、gate、scene，预计 40+ 文件，超过 AGENTS §10.2 的 30 文件提示线；负责人已明确要求设计与落码一起做。

### 3.4 MySQL 行内围栏（合并在途实现，以 v0.3 路线为准，先出 v0.4）
- **路线**：以 v0.3 为准，把在途实现收敛过去；保留在途的号位（`DBTask.save_seq = 9`、`PlayerAllData.last_save_seq = 7`）与「资产 SQL 提交证据」这一消费方（只在 dual、class-1、结局为 applied/equal 时发布，并入写路径令牌脚本）。
- **原语**：目标是 proto2mysql `SaveConditional`（`D:\luyuan\proto2mysql` 分支 `feat/conditional-save` = `e81a1e7`，语义已核对）。go/db 现钉 `v0.2.0`，用它需要负责人合并、打 `v0.3.0` 标签并推送（本线不得 push/tag），再升 go.mod。在那之前用 v0.2.0 的事务 + `FindOneByPKForUpdate` 做过渡实现，结局枚举与 v0.4 相同，换原语只改一处。
- **equal**：按幂等生产者口径，同 (epoch, seq) 视为重放，**不比内容**（v0.3 方案 1）。撞号靠 C++ 进程级发号器的墙钟下限消除；资产证据层保留「同版本不同内容即报错并计数」。这与 fence-decisions.md 第 1 项推荐的方案 3 不同，理由：那条推荐的前提是在途实现的每实体计数器会撞号，而换成发号器之后撞号只剩时钟回拨，且读回比对在 proto2mysql 的空子消息往返上必然误判（fence-inflight.md F2）。
- **其余 12 项按 fence-decisions.md 的推荐**：Observe 超界用个人下限、容差 60s；失租不豁免预算；Stage 1b 前置「L-04 第 2 批死信工具」（归 go/db 线，排在 R2/R3 后）；冷登录竞态单独立项；epoch-raise 独立 `go/db/cmd/epoch_raise`、收件箱经 GetSummary、余量常数 K；proto 生成用 no-unity；告警在 DbTaskDeadLetterAdded 里排除三个围栏 reason；进程换代必铸 epoch（归 scene_manager 线）；G4a 加只读 OwnerEpochRedis 并登记令牌脚本跨实例问题；铸造加墙钟下限（另立项）；两个可选项不做；两项待核另开任务。
- **C++**：删掉 `PlayerOwnerEpochComp.lastSaveSeq`，改用进程级 `SaveSeqAllocator`（`tlsIdGeneratorManager` 成员，`Next = max(last+1, nowMs<<10)`，Observe 有上界并配个人下限）；C++ 不写子表 body 围栏；剥离放在 `PlayerLastPersistedSnapshotComp::Replace`。
- **go/db**：被拒结局以 `(outcome, nil)` 交回 handleTask，不推进游标与 epoch；older_seq 先 confirm 再 ack；stale_owner / owner_epoch_regressed / legacy_after_seq 经 `quarantineOrderingConflict` 进死信；错误一律 `storeerr.Tag`；加 `Fencing.Mode=legacy|dual`；推进脚本用 `luaDecimalGreaterFn`、先校验后写。
- **资产通道**：自动放弃要求 Redis blob 与 SQL 证据都「未见」；SQL 证据要求按 zone 开关（dual 且 C++ 新版后才要求）；证据键登记、加入合服/回档清理。

## 4. 协作现状与阻塞

- **go/db 存盘管线接手会话**：边界已约定——本线拥有 `player_row_fence.go` 与 `key_ordered_consumer.go` 里三处在途 hunk；其余 `go/db/internal/kafka`、`proto_sql` 归它。它已通知 `runDBOp` 定稿（Redis 步骤顺序：SQL → finish → 失租分支 → reportStoreResult → confirm → verify → 缓存 SET；返回后 handleTask 成功分支 markAppliedSeq → markAppliedOwnerEpoch → ack），R2（④ + WAITAOF）、R3（保留期）不碰 runDBOp / handleDBWriteOp / publishDBWriteResult。go/db 全部落完后它把文件清单发给本线。它转来的 R1 意见已并入 §3.4。
- **「全域到账续批」写入方**（不在 Claude Desktop 会话列表，疑为 Codex 或 CLI）：截至 2026-10-15 23:05 仍在改 `go/shared/assetop`、data_service、mail、rank，以及 `player_lifecycle.cpp`。**围栏代码合并必须等它停手**，否则两边互相覆盖；需要负责人告知它：围栏相关文件（`player_row_fence.go`、KOC 三处 hunk、C++ save_seq 发号、`assetop/sql_commit.go`、`data_service/asset_sql_commit.go`）已移交本线。
- **额度**：本轮分析后 5 小时窗口已到 87%（9 个会话共用）。落码改到额度重置后分批进行。

## 5. 落码批次（额度重置后）

1. 客户端修复 + robot 修复（两个独立实现代理，各配一个复核）。
2. a–e：设计文档（新文件，登记 docs/README.md）→ 两个视角的对抗复核 → 修订 → 三路实现（a+e：login/locator/gate；b/c/d Go：scene_manager + locator leasemonitor；b/c/d C++：scene）→ 复核与修复。
3. 围栏：先出设计 v0.4（只改文档）；等「全域到账续批」写入方停手、go/db 清单到达后，再落 C++ 发号器、go/db 结局处理与资产通道收口。

每个实现代理的提示都带续做说明（先看 git diff，在半截基础上补完），被额度打断后可直接续跑。

## 6. 落码进展（2026-10-16 白天，全部未编译、未测试）

| 批 | 状态 | 依据 / 记录 |
|---|---|---|
| 客户端接线（§3.1） | 已落码两轮（实现 + 复核修复 R-1/R-2/R-4/R-6）。帮会：GUILD / GUILD_APPLICATION 后台主动拉，在帮快照一并确认 GUILD_APPLICATION；ECONOMY / ACTIVITY 被动确认 | [client-E.md](client-E.md)；设计 [landing-recovery-sync-digest.md](../../design/landing-recovery-sync-digest.md) §4.3 已按实现订正 |
| robot sync-smoke（§3.2） | 已落码两轮（F1–F8 + 复核 R2–R5） | [robot-sync-smoke.md](robot-sync-smoke.md) |
| 253 限流 | 拍板接受默认档，设计 §4.1 / §10 已改 | — |
| 进场链 a–e（§3.3） | 设计 v0.2（两路对抗复核）→ 三组落码（W1 a+e、W2 b/c/d Go、W3 b/c/d C++），各配复核与修复，复核均无正确性问题 | [scene-enter-chain-residuals.md](../../design/scene-enter-chain-residuals.md) §12、§13 |
| 围栏 v0.4（§3.4） | 设计已修订到 v0.4（只改文档）；**代码合并未做**，等「全域到账续批」写入方停手与 go/db 会话 R2/R3 文件清单 | [db-write-fencing-mysql.md](../../design/db-write-fencing-mysql.md) |
| 围栏先行修复 | 应死锁审计会话报告，已修 `player_row_fence.go` 的跨玩家 1213（无行先非锁定探测再自动提交 INSERT，撞 1062 才进 FOR UPDATE）；同版本重放不比内容；调用点打 `storeerr` 标记；补两条真库回归。**死锁审计会话已真库验证：4 条集成用例全过，lock_deadlocks = 0**（go/db 其余包回归未跑） | 设计 v0.4 §4.8 先行修复注；`docs/ops/incident-friend-lock-order-deadlock-2026-09-21.md` §9.6.3 #24 |

**给 Codex**：全部命令汇总在 [codex-verification.md](codex-verification.md)（客户端、robot、a–e 三组的两轮验证、W1 红跑切换脚本）。a–e 的 proto 改动已在工作区，§8 的生成基线只能在临时 worktree 里做或降级为人工核对。

**仍待负责人 / 协调**：
- 「全域到账续批」写入方是否已停手围栏相关文件（`player_lifecycle.cpp` 本轮仍有别人在改）。
- go/db 会话 R2（④ + WAITAOF）、R3（保留期）落完后的文件清单；之后才能做围栏 v0.4 的 go/db 结局处理（会动 `handleTask`，需与其约时间）。
- proto2mysql `feat/conditional-save` 合并、打 `v0.3.0` 标签并推送（Claude 不 push/tag）；在此之前用过渡实现。
- 围栏 v0.4 §14 第 21、22 条：proto2mysql 发版；快路径是否剥离 `stress_test_probe`（影响压测口径）。
- a–e 另立项：e 方案 C（按 (gate 实例, session_id) 定义同一会话）、C-F8（实例号为空时提交前拒绝）、§9 残余 7、6b 多算一人、W3-1 盖章接线的行为测试。

## 7. 围栏 v0.4 代码合并进展（2026-10-16 晚，全部未编译、未测试）

| 块 | 状态 | 记录 |
|---|---|---|
| go/db `Fencing.Mode`（legacy / dual，缺省 legacy） | **已落**：`ServerConfig.Fencing.Mode`、`Validate()`、`db.go` 启动期 `kafka.SetFencingMode`、`db.yaml`；legacy 下玩家表走普通 Save、不发布资产证据；单测与真库夹具已切 dual | 设计 §4.7 落码注（含过渡实现下 legacy 会把围栏列写回 body 值的语义差异）；K8s ConfigMap 渲染由 go/db 会话在 R5b 之后补，之前 K8s 恒 legacy |
| C++ 进程级发号器 | **已落**：`save_seq.h`（纯）、`save_seq_floor.h`（种子函数，主线拆出，避免 `id_generator.h` 带进 protobuf）、`PlayerSaveSeqFloorComp` 个人下限、删 `PlayerOwnerEpochComp.lastSaveSeq`、C++ 不写子表 body 围栏、快路径改回 `dirty_save::IsEqual` 并在 `Replace` 里剥离、`[SaveSeq]` 汇总行、14 个用例（`cross_zone_test/save_seq_test.cpp`） | 容差仍是常量（scene 配置键未做）；饱和复用 `kRejected`，退出路径在该结局下会不存盘丢弃（有 Observe 上界，正常到不了） |
| 资产通道 | **已落**：`AssetOp.RequireSQLCommit`（按服务，缺省关，关闭时回到 HEAD 的 Redis blob 口径）；开关打开时 `CheckWiring` 启动断言；自动放弃要求证据与 blob **都确定未见**，任一侧读失败不放弃；发布脚本改逐字节比较、`Script.Run`、证据更新计数；merge_zone 清证据键；PITR runbook 补删键；shared-redis 登记键族 | 设计 §6.6 落码记录 |
| go/db 结局处理（被拒写进死信、不推进游标、令牌发布脚本、`db_fence_write_total`、RedisSteps 12 个） | **未做**：会改 go/db 会话的 `handleTask` / metrics / stress_summarize，需与它约时间；其 R7 语义复核未跑完 | 设计 §4.4–§4.6、§5、§6.2、§10 |
| proto2mysql `SaveConditional` | **未做**：需负责人合并 `feat/conditional-save`、打 `v0.3.0` 标签并推送 | 设计 §14 第 21 条 |

**新发现的耦合（要负责人知道）**：「全域到账续批」的 mail 发奖 Finalize、trade 合同 / 商城到账（ORDER）强制要求 SQL 证据，而证据只在 go/db `Fencing.Mode=dual` 时发布。资产代理已让 mail 固定打开 `RequireSQLCommit`、trade 在配了 Commerce / Mall 却没开开关时拒启；但在 go/db 切 dual 之前，这几项仍会停在 AwaitDurable。**负责人 2026-10-16 已定**：这两项上线前先把对应 zone 的 go/db 按 v0.4 §8.3 切到 dual，在此之前保持关闭；已作为上线门禁写进设计 v0.4 §8.7、`mail-system.md` 末节、`jubaozhai-market.md` §15.1。

**其余未做（登记）**：自动放弃的第二条放行条件（证据版本 ≥ 父键 `last_save_seq`，要改 proto 才拿得到版本）；`PublishSQLCommit` 并进 go/db 令牌发布脚本后删除；`data_service/asset_sql_commit.go` 补「证据键与 go/db Redis 同实例」注释；`zone_data_rollback.md`、`k8s_zone_rollback.ps1`、`merge-zone-runbook.md` 补删证据键步骤；`evidence_ahead`、`sql_evidence_behind` 两个信号的告警规则；scene 主机时钟偏差监控；容差改 scene 配置键。

### 7.1 go/db 结局处理的约定（go/db 接手会话 2026-10-16 答复）

由本线按 v0.4 落；开工信号是 go/db 会话发来「handleTask 可以动了」（附当时成功分支代码、RedisSteps 现状、R7 动过的函数清单），在此之前不动 `handleTask` 与 metrics。落码时必须同时满足：
1. RedisSteps 扩到 12 个时四处同批改：`go/db/internal/metrics/pipeline.go`（RedisSteps 与常量）、`metrics_test.go`（`TestRedisStepNamesPinned`、`TestRedisStepNamesMatchStressSummarize`）、`tools/scripts/stress_summarize.ps1` 2e 段 `$redisSteps`、`tools/scripts/test_stress_summarize.ps1` 2e 断言（列数、假快照、表头正则）。漏一处，压测汇总会静默打 n/a。
2. 令牌发布脚本取代 verify 后，S4 的 `lease_verify` 计时、`deploy/k8s/db-task-retry-alerts.yaml` 的 db-task.ordering-lease 组里 `DbOrderingLeaseLost` 的 `verify_error` reason、`ordering_lease.go` 头注释「核锁在缓存回写之前」的不变量，要么保留语义要么同步改。
3. 被拒写进死信复用 `quarantineOrderingConflict` 的 R2 路径（Kafka 分支 `persistBeforeAck` 异步确认，`CountTaskDeadLetter` 只在确认成功后计数），不另写同步 LPUSH。
4. older_seq 先 confirm 再 ack；confirm 失败按现有「可重试、不推进游标」处理。
5. `db_fence_write_total` 等新指标追加在 `metrics.go` 现有指标之后，label 为封闭集合，不带 player_id。
