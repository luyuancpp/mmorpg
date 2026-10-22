# 全球交易、角色归属与双版交付审核交接

本文件按用户要求供 Claude Code 审核。当前为实施中的交接底稿，**整体未完成，不是验收通过报告**。最终交付必须补齐本轮三语言构建、真库与真实双区故障结果；源码、静态检查、历史绿灯不得替代本轮运行证据。

较新的实际结果按文末章节追加；前段表格保留当时状态，不代表当前总验收结论。

## 1. 审核范围与工作区保护

主仓：`D:/luyuan/xuanming-server-mmo-cpp`，本批基点 `1c9c1e6c19c415728fae625f985d98da945c5fce`。独立 Java 仓：`D:/luyuan/xuanming-server-mmo`，当前另有既有提交 `98c3236cfcb574e04d2f7d224ea99ad5cb512c7b`。客户端：`D:/luyuan/mmorpg-client`。三仓都有既有工作区改动，不能把全部 `git diff` 直接归因本批；分批源码 manifest 用于确定本批文件，后续修订另留快照，不覆盖旧证据。本任务不 commit、push、tag，不回退他人改动，不手改生成树。

原设计核对源为 `D:/luyuan/xuanming-server` 的实际好友和经济交易实现。全局能力按数据库权威、事务边界与全部调用路径判断，不能按 `MySQLFriendRepo` 类型名推断只支持单服。

## 2. 用户已确认的业务规则

| 项目 | 本批要求 |
| --- | --- |
| 聚宝斋默认值 | 公示 48 小时、寄售 7 天、手续费 5% 向上取整到分、卖家收入冷静期 3 天；均可配置 |
| 角色出售 | 出售前必须离帮，好友保留；以真实排空与 SQL 权威为准 |
| 货币 | 默认金币可交易、钻石关闭、绑定货币不允许交易 |
| 支付、退款、提现 | 完成本地 mock 全链路；真实渠道默认关闭，后续再接 |
| 商城选型 | 保留 macrozheng/mall 集成方向；游戏资产仍由游戏侧事务与持久收据决定 |
| 分页 | 两版稳定游标；前 20 单长期离线不能饿死第 21 单 |
| 匹配 | 区域优先，等待超时且本区人数不足后跨区兜底；跨区逐人核延迟上限 |
| 完成标准 | C++/Go、独立 Java、客户端同步；受影响构建、测试、真库及真实双区故障验收通过 |

## 3. 本批实现与审核接缝

| 范围 | 主要入口 | 必须核对的不变量 |
| --- | --- | --- |
| 原会话身份 | `go/shared/loginsession`、`go/shared/roleidentity`、login/gate 路由，Java `SessionContext` / `AuthorityJdbc` | 原账号与原修订不可用当前目录补齐；缺原件拒绝；过户、账号 ABA、冻结均拒绝旧请求 |
| 普通交易与拍卖 | `go/trade`，Java `game-trade` | 卖方、买方、竞价参与者的原 identity 持久化；实际提交首锁 P；后台依原件恢复；未知提交不能重铸资产 |
| 人民币寄售 | `trade/commerce`、`jubaozhai`，Java `game-trade` | kind6 工作流持久托管、原快照返还、原支付单退款；真实终局才消费资产 op，人工态保留 |
| 官方商城 | `go/trade/internal/commerce`，Java `game-mall` | 订单绑定原账号和修订；支付成功后角色换主且尚无 op，只对原单退款；金额与 uint64 wire 为规范字符串 |
| 角色出售排空 | `role_admission_system`、`player_lifecycle`、data_service/native trade，Java player-store/trade | scene 在任何载入副作用前登记入场世代；有战斗先结算；seal 仅覆盖原 instance/revision 的固定世代上界；收到 RPC 不等于 SQL 排空完成 |
| 好友 | `go/friend/internal/data/friend_repo.go` 及相关入口，Java `game-friend` | 加好友、接受申请、双向关系原子提交；全员 P 按无符号升序，再容量与关系锁；读写和守卫补建均校验原会话 |
| 帮会与历练 | `go/guild`，Java `game-guild` | 原 consent 私有持久化；全员 P 升序后 G；发外部请求前提交原 launch 与 ACTIVE 投影；EXPIRED 不代表不会迟到结算，不释放 ACTIVE |
| 安全取消开战 | `match_internal`、Match authority，Java `game-match` | Cancel 同原 Start 的 direct PK 提交永久墓碑；迟到 Start 不复活；已有 battle 只回真实状态；双向签名回复绑定原请求摘要 |
| 邮件 | `go/mail`，Java `game-mail` | 玩家邮箱实际事务先锁原角色 P；再邮箱守卫、状态、seq、op；旧幂等请求也不得跨过户；内部后台不伪造玩家身份 |
| 配置与总账 | `tools/scripts/k8s_deploy.ps1` | friend/guild/mail 显式全局 LoginRedis 和目录库；与 login 同 DB；独立连接池全部进入 Redis 连接预算 |

Java 内部角色修订沿既有存储能力限定 `1..Long.MAX_VALUE`，到上限显式拒绝；不对高半区 uint64 静默窄化。Mall 的 wire 标识继续使用完整 uint64 字符串。Java 内部存储与 RPC 按自身惯用方式实现，不要求逐表复制 Go；客户端可见规则与失败语义必须一致。

## 4. 关键协议与生成边界

主 proto 增量为原 identity、角色入场/交易 seal、资产/角色转让内部方法、交易工作流和 Guild Trial launch/cancel 契约。`SessionDetails.owner_revision=9`；`BattleActivityContext.launch_request_id=7`；`MatchDirectIntentRecord.cancelled=7`；Guild launch 原件含原 roster、账号、修订及已核验 cancel 回执。字段均在事实源追加，正式生成器输出，不手改绑定。

Cancel 方法为 `/match.MatchInternal/CancelActivityBattle`，回复签名方法追加 `/reply`。调用方 `guild`、回复方 `match`，只从装配层注入 `MMORPG_MATCH_INTERNAL_SECRET_GUILD`；正文签名覆盖原 Start，回复覆盖清除 outer auth 后的原 Cancel 摘要。密钥不足或依赖缺失拒绝。只有已核签且绑定原件的“持久墓碑且 battle=0”或真实 formation=5 才能证明安全终结；普通 Start 的负答或状态字段不能独立充当释放凭据。

## 5. 已有本轮运行证据

| 项目 | 实际结果与限制 | 证据 |
| --- | --- | --- |
| 生成器回归 | 274 条实际测试通过，0 test fail、0 test skip。原统计器把 6 个无测试包误计为 skip，原失败与 JSON 保留，独立复核区分 package 无测试和 test skip；没有跳测试或重跑冒充首次通过 | `build/verification/current-completion-20261021/generator-current-counter-review/proof.json`，SHA `E3503C06097FAD702B353DB2C6D63C52BB8FBC7A8FC187DB8C96C425368FFFC5` |
| 正式生成器构建 | 官方 proto-gen-build 退出 0；125 源、19127 依赖文件、14 根无漂移；两 EXE 同 SHA `9e925d10b78cda692e0edf3b911c87ef2ec73616242ce9fba4282cac9d0497d2` | `build/verification/current-completion-20261021/formal-build-current-admitted/formal-build-current-20261021T083049793Z-96adbcc2/proof.json`，SHA `41D6E9A965BD2D1E4B1FDB258FC6162D9D6EBC811ED24942D1489F23340E2A48` |
| 本轮正式协议生成首轮 | **未通过**。生成运行中，采样器的两份进程快照将本 run 快速新建的 protoc 判成未知且采样超过 2 秒；原输出与103项部分生成变化保留。必须修正采样并重新正式生成，不能接受半成品 | `build/verification/current-completion-20261021/formal-generation-current-admitted/formal-run-20261021T085907554Z-fabe97ca/proof.json` |
| 服务构建、真库、双区 | **本轮尚未完成**，不能引用历史产物当作当前验证 | 后续按实际结果追加 |

源码 manifest 与静态检查只说明文件内容、格式或差异，不说明业务已运行。所有正式验证必须绑定当前源码、正式生成物及原命令退出状态，拒绝零条测试、跳过真库、断言关闭或损坏后不明重试。

## 6. Claude Code 审核顺序

1. 按三仓规范完成启动门禁；读取对应设计及 `PARITY.md`，核对本批清单与原有 dirty/staged。
2. 从客户端入口追原账号/修订，经实际 SQL 首锁直至资产提交和后台恢复；逐项检查同类入口与清理路径，而不只看新 helper。
3. 对照原仓好友接受和 auction 的实际事务，检查双方关系原子提交、锁序、幂等键、未知提交与故障恢复是否完整。
4. 专审角色出售与 Trial：未结算/未发奖投影、人工态 op、迟到 Start/Cancel/结果、旧 scene 复活、固定 seal 上界、买家新世代不受旧 seal 影响。
5. 核 mock 支付、退款、提现全链路及两版商城公平分页；不能把仅 SQL 订单终结认成游戏资产到账。
6. 检查本轮原始运行结果：db_first、旧 E CAS 拒写、SQL 不回退、首 ACK 到达仍等第二 ACK、跨区 +11/+7 恰好一次，必须为真实两区路径。
7. Claude 按仓库职责只审源码和给出验证命令；编译、测试、压测仍由 Codex 执行。发现问题给出路径、行号、触发时序、损害与最小修正，不以“整体看起来合理”代替结论。

## 7. 待最终填入

- 最终分批文件清单、源与生成物 hash，以及本批对既有 dirty 的增量归属。
- 各语言构建、格式检查、静态检查与测试的命令、工作目录、退出状态、计数、日志位置。
- 真 TiDB、Redis、服务进程及真实双区故障演练的当前原始证据和清理记录。
- 区域优先匹配与每人延迟约束、客户端恢复语义、mall 真实后端管理链路的最终核对结论。
- 全部未验项、失败修复过程与尚存风险。只有上述要求实际完成后，才可把本文件改为最终交付状态。

## 8. 全球逻辑服收尾增量（源码阶段，尚未整体验收）

用户继续要求所有非战斗逻辑服按全球同服、多副本推进，先完成移植再作正式验收。“水平扩展”表示增加副本并共享同一事务权威；单次队伍人数、候选页、工作槽和内存准入仍有明确上限，未作无限容量或性能承诺。

- Go Team 已由生产 Redis 权威改为 SQL：双向邀请原归属、成员 epoch/离队永久墓碑、九类写请求 UUID 收据、完整队伍 launch、整队候选单位、区域交集和逐人延迟、失败整队重排、全阶段恢复。scene 的成员查询改为原入场身份绑定的签名 SQL 查询；Redis 位置仍仅用于跟随显示。
- 观战以原请求 UUID、固定节点实例、原 Add 内容与截止时间登记。battle 对取消先于 Add 留墓碑；Remove 的 `cancelled` 正证明才允许关闭 SQL 占用，`ABSENT` 不能独自证明迟到 Add 不会执行。每条直连请求均复核当前连接与观战 op，旧连接不能停止新观战。
- Go 单人 Join 新增 `PrepareJoin`：完整 P 下固定原 UUID、原票、原网络探测、原观战退出对象，当前非空 state0 票阻止另一业务插入；退出后重核原件再入队。旧成功请求回放不退出当前新观战，取消原 UUID 永久拒绝迟到 Join。活动和挑战接受同样先持久固定完整成局/成员原件和退出列表，后台只恢复原件。
- `CancelActivity` 按完整原 P→Direct 顺序取消原待提交票。首读缺失后恰有 Start 提交时先回滚，再仅重取一次原 P；不在持有 Direct 后补锁 P。已成局只报告实际原成局状态。
- Native 转卖护栏补齐 SQL Team 成员、邀请/申请、原 launch 与观战；未知状态、孤儿原件与非空 state0 默认拒绝。Java 的历史恢复须同时提供独立审计的 E 和归属修订上界，旧单上界入口拒绝。整库权威 PITR 不能靠被恢复的数据库自证当前归属/合同，必须独立审计后受控解冻。
- 客户端 Team 九类写操作持久保存原 UUID 和完整正文，重连或重建时只重放原请求；损坏记录保留并拒绝新写。观战回调绑定本次请求 UUID，旧响应不改新状态。新增 Team 恢复三项与观战旧回调回归源码，尚未运行。
- 部署入口增加唯一全球 TiDB SQL host，对冲突别名和注入内容拒绝；本机开发证明按服务最小权限注入。新增 `tools/scripts/tests/global_authority_deploy.tests.ps1` 已实际执行：3 项通过、0 失败；相关七份 PowerShell 解析通过。该结果仅覆盖配置/注入行为，不代表业务构建或真库通过。

Go Match 静态快照为 `build/verification/current-completion-20261021/go-match-complete-08210d6891fa4781a6af1d6509eef51a/source-manifest.json`，SHA `4C5618B0C2E511C1B7E76E3D33DD658363536A41B8848261748776188537140F`，80 路径及 6 份主协议依赖；仅 gofmt 与差异检查，不含生成、编译、测试或真库证据。随后如修改该批源，必须刷新最终 pin。

本节写入时，Java Join/活动/挑战观战退出和 QueueUnit、Java 完整合区/撤销作业，以及 Go 合区真库夹具迁移仍在实施。正式协议生成、各语言当前源构建、真库全链路和真实双区故障验收均未完成。资源只读复核可用内存约 1.04 GiB，并有新的外部 MSBuild、UnrealEditor 和 Java 进程；不沿用旧 PID 授权关闭新进程。

## 9. 验证窗口恢复后的实际进展（2026-10-21）

用户确认已释放资源后，只读复核可用内存约10.52 GiB，未沿用旧PID授权终止新进程。生成监控的原失败证据均保留；只读专项20次、32次本任务子进程自然退出及原句柄终止专项通过后，正式主生成执行退出0。

主生成的首个完整结果因新增账本/工程/handler差异而保持未验收。root逐项审核：原消息1..316不动，生成器追加317..343；工程只新增role_admission及TeamAuthority绑定/客户端；QueryObserver与SealAccountRoleTrade均在守护段接正式逻辑。复跑稳定生成通过：`build/verification/current-completion-20261021/formal-generation-global-20261021T140914762Z/formal-run-20261021T141033951Z-7c8a7fdf/proof.json`，SHA `D820ECBA2F476B15A90E2B784033582FDECA9DFF852E134EEEAF7B491A4B2AFE`。该证据仅代表当时128个主协议树文件的生成与保护范围稳定；随后收件箱注释修订仍需进入最终源绑定。

收件箱两版补齐原角色首锁、条目原账号/修订和永久唯一收据。主协议新增item owner_account24/owner_revision25，Fetch admission8/auth9，Ack admission7/auth8。C++仅发送SQL已确认的原入场身份并签名，回包再次核原身份；Go消费同P核完整SQL已登记原件，Redis不再冒充准入权威。DS的显式RoleTradeInboxSchema和挂起守卫覆盖转卖、移除、导入；精确终态2/3/4/5才允许换主。Java沿既有player_final_save登记完整准入，不建平行归属表；首次同P加载、原件范围取消及历史恢复挂起保护已有回归源码。

当前DS首次正式构建尚未通过：运行中监控读取陌生cmd身份被拒，保守停止；原stderr另有Go compile退出0xc0000142，两者因果尚未证实。原件在`build/verification/current-completion-20261021/go-data-build-20261021T141733809Z/`，不能计作编译通过，也不以关闭断言、跳过门禁或盲目重试处理。正在检查只读身份查询的最小权限接缝。

截至本条，各服务本轮编译、业务测试、真库及真实双区仍未全部完成。Java本批功能源、客户端回归源码、Go Match/Inbox和合区源码清单均不等于运行验收。

## 10. 后续源码冻结与验证增量（2026-10-21）

监控已改用最小只读权限的v3原句柄查询，保留未知进程拒绝、退出身份复核和运行采样期限。实际只读20次与自有子进程32次专项通过。随后Go data_service、player_inbox正式构建分别退出0；Inbox实际261项单测通过、0失败、0跳过。证据依次为`current-completion-20261021/go-data-build-20261021T143827220Z/proof.json`、`go-inbox-build-20261021T144424169Z/proof.json`和`go-inbox-unit-20261021T144829368Z/test-counter-review.json`（均位于`build/verification/`）。这些结果早于本节新增的路由和双向准入守卫，不能替代最终重编译与回归。

Go Inbox所有写入口补上共享的route_meta→原home的pid%64合服围栏→原route复核→完整角色P前缀；缺路由、维护态、冻结或不明状态均拒绝。Match与Guild历练反向检查真实票、永久launch及私有slot，终局必须父原件与角色投影同时匹配。DS转卖、移除、导入同步检查私有大厅和永久原件；仅角色投影先终结不能放行。新增父原件/投影不一致、孤儿与空占位回归源码，尚待实际真库运行。

Guild生产私有大厅已移至同TiDB事务权威；正式proto新增不可变Original、Consents、lobby和slot表记录。无永久launch的取消先于Prepare写永久结束墓碑；已有未知launch不能因TTL或解散释放。解散后的真实终局仍须完整绑定原件，只清原合同及原slot，不重建帮会或奖励。生产部署显式镜像MatchAuthoritySchema。

最后一轮主生成在native启动前拒绝：生成器的两份C++工程去重源码已被同仓整合更新，与早先125源构建快照不符。失败现场`formal-generation-global-20261021T152920623Z/formal-run-20261021T153257126Z-190a4085/`保留，没有执行部分生成；需重新验证当前生成器并完成官方build后再绑定最终生成。共享Go整包也在测试启动前暴露缺少已有testify及间接依赖声明，诊断和修正过程原件保留，未将未运行测试算通过。

本任务基础设施项目为`codex-asset-arrival-20261017`，真实TiDB端点`127.0.0.1:24000`、Redis`26379`、etcd`22379`、Kafka`29092`。所有建库与清理只允许本任务随机白名单库；尚未成功开始本轮Inbox建库验收。为释放编译资源暂时停止本任务Kafka及TiDB/PD/TiKV，数据卷保留；运行验收前恢复。不控制其他项目容器。当前仍未完成最终两版构建、真库全链路与真实双区验收。

## 11. 当前源重新生成与构建启动证据（2026-10-21）

共享Go完整单测实际1182通过、0失败、14真实外部环境跳过；JSON解析0错误。14项分别为assetop9、物理TiDB会话1、muduo互通1、Kafka3，必须另行实跑，不能把skip当通过。证据`build/verification/current-completion-20261021/go-shared-unit-final-r3-20261021T153907143Z/test-counter-review.json`，SHA `C1D05C33EF1F725BC69DECE70EE992DD5B06843FBDD626B3D58D90F266BB2698`。shared/go.mod补齐已在缓存及sum中的mysql、testify和difflib声明；未下载或升级工具。

生成器两次实际红分别揭示RPC方法夹具未同步，以及新增方法后旧测试下标指错目标。保留原有拒绝断言与守护段逐字保留检查，只同步正式方法表并按方法名选取流式拒绝目标。当前回归实际275通过、0失败/跳过、8项必需通过；随后官方proto-gen-build退出0，125源/19127依赖零漂移，两EXE同SHA `937310b99fdc8f20a6d7466576f52a1eac48ce3a7e97bb5fe0e95abae6566ce8`。对应proof为`generator-dedupe-fixtures-current-r3/regression/current-20261021T155420021Z-cc2b262c/proof.json`（SHA `0BB7AD9EE3BD805A777C1895A61F160E6DF77B54A06A59FA5807FFDADEB1C649`）及`build/admitted-20261021T155555597Z-a70e3361/formal-build-current-20261021T155604481Z-fe518e70/proof.json`（SHA `F2C01ED04F8B7EEFE2B39EE47487F30E53CCBFDFE8FECC3E8439D9BAD1101F78`），两路径均从`build/verification/current-completion-20261021/`起。

最终主生成已实际通过：`generator-dedupe-fixtures-current-r3/bootstrap/formal-generation-root-normalized-20261021T160131932Z/formal-run-20261021T160143723Z-ecdad8e1/proof.json`，SHA `7E983F5D0FA7C05B45D7DB38A2539EA3636B9AAC866A035D9CFF7D18E69139CE`。26守护文件、11工程文件、3账本和128协议树文件零漂移，33正常输出更新；客户端RPC仅同步同仓整合已有的UpgradeSkill317，已核正式源和生成差异，不把该既有业务归为本任务实现。

客户端两个正规脚本实际退出0，输入漂移0，生成Proto变化10文件、MessageIds变化1文件；日志仅一条unused import。proof为`client-formal-final-20261021T160626857Z/client-generation-20261021T160643999Z-7d673fcd/proof.json`，SHA `B4CC96007C047F209199A8C522D05A1ACEE4E1CEFF2CA10D7B1AC70AA49C6783`。此证据不包含Unity编译或测试。Java当前931源、250受管文件、2402保护文件和253工具文件已纯双观察封存，尚未同步或构建。C++完整1867源和26守护段已双观察绑定，正规串行Build与真实NoRaw流程已启动，尚无成功结论。

同仓外部保存使主HEAD变为`6e2f024484008c638c5d37679b9abeeaef5775b6`；本任务没有commit。各新计划按实际源字节与当前HEAD重新绑定，旧失败和历史证据保留。所有服务最终构建、真库全链与真实双区仍待完成。

## 12. 当前 Inbox、Java 红灯及部署入口验证（2026-10-21）

最终生成后的 Inbox 正式构建退出0，828项源码和33054项实际Go依赖保持一致。证据为`build/verification/current-completion-20261021/go-module-current-candidate-v4-r3/rebase-player_inbox-Build-20261021T163725117Z-ae26dac6/execution-20261021T164209817Z-9a55629f/proof.json`，SHA `33185E059EF659898E675CFC90BC9BD94FCF3EC8A7C1FD34B390164971962253`。随后同源实际单测261通过、0失败、0跳过，证据为`go-module-current-target-reuse-r1/derive-player_inbox-Unit-20261021T164234389Z-260f0aff/execution-20261021T164443133Z-e7263abf/proof.json`（同样位于`build/verification/current-completion-20261021/`）。此处尚不包含真实TiDB集成验收。

Go执行监控曾把启动7 GiB误用于运行中，导致一次链接被停止；保留原失败，修正为启动7 GiB、运行2.75 GiB、两秒采样后才执行上述构建。没有关闭其他应用或放宽未知进程门禁。

Java正式契约同步及check退出0；第一轮全新私有仓库的30模块构建在game-api测试处停止。实际2685项测试中2672通过、2失败、0错误、11外部环境跳过。失败为新增QUEUED值及owner_revision字段后的两处旧数量断言；仅修正相应测试，保留旧默认值、旧字段号和旧线格式断言，新增QUEUED=4及owner_revision=10的实际解码断言。原私有仓库、XML和失败proof保留于`build/verification/java-current-final-candidate-r6-20261021T1607470490555Z/normal-reactor-20261021T163531320Z-f8eb0365/`。新r7源已重新捕获，须实际重跑，不能宣称30模块通过。

实际部署渲染首次暴露生产GoSvcCatalogue缺少guild，已在真实catalogue补齐全球服务、50300端口及迁移配置。修正后的渲染脚本实际退出0，证据`build/verification/current-completion-20261021/deployment-render-20261021T162444179Z/proof.json`；覆盖全局配置、TiDB、三种battle探针和两种scene Secret引用，仅为本地渲染证据，未操作集群。

C++旧预检硬编码21库，面对当前150库时在MSBuild前拒绝；没有编译成功结论。新计划已按46个实际参与工程、17个产库目标、133个固定依赖及player仅头接口重新审查，保留默认输出和真实StrictNoRaw门禁，待执行实际MSBuild元数据、编译及测试。Unity输入双观察已完成，实际Import及测试尚待执行。两版最终全量构建、真库全链与真实双区仍未全部完成。

## 13. 当前源回归红灯与修复记录（2026-10-21，验收仍在进行）

C++完整构建在约20分钟时被进程身份监控停止：原句柄查询link.exe映像返回Win32 5，不能据此认定编译器或业务失败，也不能把已完成的部分工程算成完整构建通过。原结果为`build/verification/cpp-current-v3-layout-r2-candidate-20261021/run-20261021T165416001Z-2d3c109b/result.json`，SHA `5D43BC99066F4C1B829132F559497E07390122DE778E984255FC8E26248A7B51`。后续v4仅在原句柄、同PID和创建时间下，按剩余两秒采样预算等待至多500毫秒；只有确证进程已退出才接受身份查询期间退出，活进程、未知身份和不匹配仍拒绝。只读20次、自有进程32次、受控延迟退出7项实际通过，未复现该次Win32 5事故，不声称已证明其唯一原因。

真实BattleRoomManager入口复核发现观察者分配签名失败时残留原operation的缺陷，已在对应失败清理和缺路由移除路径同步清除；旧operation仍不得移除新观战。新增两项真实Manager契约回归，原有42项battle契约接入正式RPC测试工程。当前C++源为1868项；正在执行私有反事实红灯和正式RPC绿灯，随后仍需完整game.sln与独立RPC的当前源编译及全套测试。静态清单、旧部分构建和本节修复均不能替代运行结果。

当前Go data_service的Build、Unit、Vet与运行EXE构建四阶段均实际通过，顺序结果为`build/verification/current-completion-20261021/go-module-sequential-entry-r1/sequence-data_service-20261021T173101002Z-4c332eee/sequence-result.json`，SHA `3C2BB1E6231BA777CC6BAADBF3DAB97A63FB67D8C0E6F93EA7FACD88998A857D`。单测1586通过、0失败、89项真实环境跳过；这些跳过已逐项映射到后续原生SQL专项，不能计作真库通过。Match构建实际发现一个未使用导入，仅移除该导入后已重新完成真实GoList闭包捕获；Build、Unit、Vet及运行EXE仍待本轮执行。Inbox当前Vet也已通过。

Java r8正规同步与check实际通过，最终2654项输入零漂移；全新私有仓库的完整30模块构建仍未通过，在game-player-store停止。实际3156项测试中2990通过、11失败、19错误、136项外部环境跳过。30项红灯全部从原XML分类：H2不支持的UNSIGNED时间表达式22项、高位uint64夹具1项、约束名大小写1项、重复建表2项、BLOB映射2项及角色创建夹具zone=0两项。修复保持数据库时钟、完整路由首锁、原准入字节及拒绝语义；新增三项时钟契约回归。原失败目录为`build/verification/java-current-final-candidate-r8-20261021T1728043483770Z/normal-reactor-20261021T174547121Z-dc850d02/`。八项源码修复尚未运行，新增一个文件后必须重新绑定当前源，先执行受影响模块再全量重跑。

尚待完成C++、Go、Java和Unity的当前源剩余构建/测试、两版真实TiDB专项、mock支付退款提现与分页游戏到账，以及真实双区故障验收。尚未恢复的本任务TiDB/Kafka数据卷保持原样；未操作其他应用或项目资源。

## 14. Match 回归、网关运行包及后续实错（2026-10-21）

Match完整单测首次实际失败一项：直接比较protobuf对象时把内部sizeCache的29/0差异也视为业务差异。仅把该条断言改为proto.Equal，保留全部身份字段和未知字段语义。修复后当前937源实际598项通过、0失败、86项真实环境跳过；Vet及独立运行EXE构建也通过，EXE尚未启动。Unit证据`build/verification/current-completion-20261021/go-match-proto-semantic-test-overlay-r1/derive-match-Unit-20261021T181441858Z-13531ab9/execution-20261021T181645263Z-df73e82f/proof.json`，SHA `AA7AABFD86DE58E7DE91F0AFE56AAD7B0B3C48524A25CC532D0FE404470058C0`。运行包为同目录体系下`derive-match-RuntimeBuild-20261021T181444974Z-4f74f23e/bin/match.exe`，SHA `59b2eeedfab7c3052b5820ec9022b80c1ae19826c4a2b7cedaf575add922a40b`。旧Build仅说明原输入编译通过，不改写为修复后测试已经运行的证据。

86项跳过逐项映射：84行（含子例）对应既有原生SQL专项，另两项分别需要真实rank/mail进程和三broker Kafka故障恢复。映射文件`sql-specialists-static-preparation-20261021/match-86skip-exact-followup-map.json`，SHA `A20C2CE6A0834AF90C9A30E9FC4CB8B76277FFAF03CF9ACAC46A0BDFCE66F7DE`，从`build/verification/current-completion-20261021/`起，当前均为待执行。

主仓Java网关实际Maven构建退出0，236项中231通过、0失败/错误、5项真实环境跳过；MallAdminClientTest和MallAdminControllerTest均非零且无跳过。原验证器先发生局部变量name覆盖导致日志命名错误，修正后又暴露两处错误的JAR路径假设：生产ClassPathResource资源在BOOT-INF/classes根目录，正式pom的build-info在META-INF根目录。没有移动生产资源来迎合验证器；保留原失败和真实XML、日志，另按实际路径逐字核验三份资源、Boot入口、当前commit和完整源码/工具/离线仓一致后封存运行包。独立复核proof为`build/verification/current-completion-20261021/gateway-package-root-post-audit-20261021T182227211Z/proof.json`，SHA `44D630C3AD44242AFEE08843BD4E7FC32FA7611F4DDE7EDE4ABBA621C188900B`；归档JAR SHA `65d68bff093404437a192ff5583a342e84c24ef6f466ea16d392e2e1a492cccb`，尚未启动，不含真库通过结论。

主仓Mall首次全量实际94项中87通过、1失败、6错误、0跳过。六个HTTP上下文错误均定位到Spring无法为final的@Repository创建代理；相邻同类声明也一并核查。另一个付款后查询断言仍拿付款前AWAITING_PAYMENT整条记录比较，实际生产合同是返回当前DELIVERY_PENDING。修复将保留真实支付、库存、原请求与永久收据的恰好一次断言，尚无修复后运行结果。原件在`main-gateway-mall-current-v4-20261021/capture-20261021T181219273Z-016608db/run-Mall-Package/`，从同一current-completion目录起。

C++新增Manager入口回归在真正接入正规门禁后依次暴露三处问题：缺OpenSSL/yaml-cpp头目录、旧RoomTable测试的裸指针计数成员、新测试误用Go风格错误码符号。仅修正规工程路径和测试实现，保留全部业务断言、正式tip生成源和NoRaw门禁；各轮原始失败保留，不能算预期反事实红灯。当前1868源摘要为`8d31f801f7fd18dd94c7a8183c43a474d7b48ccb9a67ebE9c23911019dc8eea5`（大小写无关），正式红/绿测试尚待运行。

独立Java r9当前2655项输入的正规sync/check实际通过；修复后的TargetedH2五模块正在执行。125项既有外部环境跳过按实际XML逐例绑定，其中玩家SQL8项、discovery Redis117项，均单列未验。Redis夹具包含全实例SCRIPT FLUSH及固定键，后续须在独立本任务Redis实例运行，不能仅靠切换DB隔离。完整30模块、真库专项及双区全链路仍未完成。

## 15. 当前运行结果与剩余权限接缝（2026-10-21）

主仓Mall修复两个@Repository的final代理限制及付款后状态断言后，当前完整94项测试实际全部通过，无失败、错误或跳过；正式Maven打包退出0。证据`build/verification/current-completion-20261021/main-gateway-mall-current-v4-20261021/capture-20261021T183100960Z-9c40cd30/run-Mall-Package/proof.json`，SHA `49E2D0614665AB7814692E7B7E4A772ADFC41BAD242B3E8B04D935D5A4ACC5BB`。归档JAR SHA `a447c8e2822da05a481254891ab80591b93afe85cca63d333229ed566dd2db04`。尚未运行真实TiDB、HTTP mock全链路和游戏到账验收。

独立Java r9 TargetedH2五模块实际598通过、0失败/错误、125外部环境跳过，输入2655项零漂移；证据`build/verification/java-current-final-candidate-r9-20261021T1806358775238Z/targeted-h2-20261021T182828709Z-740e33f0/proof.json`，SHA `41235268441DA360EEFC873924A528B131052955F2B738F331C1ACE983EDB84C`。随后完整30模块构建实际已执行3195项，其中3058通过、137外部环境跳过，无测试失败/错误，但在game-scene-manager的testCompile停止：DruidDataSource.getConnection的静态返回类型是DruidPooledConnection，夹具却mock为java.sql.Connection。只修正该mock类型，保留原断言；修复后尚未重跑完整构建。该失败必须记为编译失败，不能称30模块通过或业务测试失败。

C++观察者早期门禁已经得到真实反事实红灯与正式绿灯。r7结果`build/verification/cpp-observer-early-gate-v4-r7-20261021/run-20261021T185927442Z-1542d75d/result.json`，SHA `4B080707F51784F5B2478C7665FA1DACFEBBD1D8C0F80F39E24584AC15C5DB91`：144注册项、142实际通过、0失败/跳过，两项原有DISABLED基准未运行，不作性能结论。红灯及17个正式编译单元的实际构建/Strict检查按原结果链复核，未改写历史失败。当前1868源摘要`8d31f801f7fd18dd94c7a8183c43a474d7b48ccb9a67ebe9c23911019dc8eea5`；全量game.sln与独立RPC的正式串行构建已另行启动，尚无完整成功结论。

Go Guild正式Build退出0，随后Unit暴露两个配置契约问题：一条新测试误把已有契约中0代表有限默认连接数视为非法；默认yaml缺少全局登录会话配置。修复保持0映射有限默认20连接，继续拒绝负数及129；yaml补齐GlobalLoginSessions及本地登录Redis入口。两项修复尚未运行，不能把旧Build算成当前四阶段通过。

全局权限审计另发现两版Chat原来只凭player_id授权。角色转让后，旧账号会话可能继续收发该角色私聊，正在同一任务范围补齐：原登录会话、完整路由围栏、角色P及原account/owner_revision在SQL提交点共同复核；成功发送永久幂等收据，Pull直接读取SQL权威，保留新合法拥有者的角色历史。不新增proto，不以Redis缓存作为聊天权威。两版实现、部署与回归仍在收口，尚无编译或真库通过结论。

Unity第一次正式Import被未知重型进程门禁停止，未计作导入通过。实际日志确认该.NET编译服务由本轮Unity的VBCSCompilerPreWarmer启动；它已自行退出，未关闭其他应用。正在补齐此短生命周期父进程的监管接缝及工具输入；Unity正式导入、meta复核和四程序集EditMode测试仍未完成。

## 16. 全量构建后续红灯与默认配置缺口（2026-10-21）

C++ r4全量仍未通过，原结果SHA `ED771E48D2040DBDAEA165A0F3F3DD55548F58CD96D06A40CDB42182B3E14DD2`。监控在capture_open对PID 27508返回Win32 5；只读查询确认它是两天前启动的受保护safesvr.exe，其旧父PID恰被本轮编译器复用。没有控制该系统服务；230个已登记本轮进程清理后均已退出。该次没有得到编译退出结果，不能认定源码编译失败或全量成功。v5-r2以新鲜内核创建时间只排除不可能属于原父的旧关系，仍逐原句柄核验真正新子进程、仍拒未知重型进程。root实际16项关系专项、20次采样、32项自然退出及原句柄终止、7项受控延迟退出全部通过；证据在`build/verification/current-completion-20261021/formal-monitor-parent-creation-candidate-v5-r2/`。未强迫系统PID复用，形状专项与原事故证据分别记录。新C++正式计划已绑定四组原件、1868源及150库/32EXE当前库存，完整Build仍待执行。

独立Java r10-r2正规sync/check实际通过，输入2664项；完整构建实际3821项中3616通过、29失败、0错误、176外部环境跳过，在game-login停止。原件`build/verification/java-current-final-candidate-r10-chat-bound-r2-20261021T1930Z/normal-reactor-20261021T194653874Z-72dbf438/`。27条红为两个旧测试助手的完整PB期望缺少owner_revision=1，另两条为回落顺序期望缺少目标区围栏检查；只补完整期望值和两区围栏顺序，不移除断言或生产校验。Druid修复已实际编译通过；尚未运行到Chat模块，不能称Chat编译或回归通过。两Login测试的当前修复与后续完整构建仍待实际运行。

部署实际结果：Chat SQL契约3/3、启动器端口与归属21/21、全球SQL注入3/3、迁移门禁41/41通过。端口夹具只修历史邮件变量及文件名；迁移门禁按现役生产者签名Secret取代历史M1 AdminToken期望，并保留迁移顺序及密钥值不外泄检查。trade模板移除与既有动态注入冲突的手写资产密钥引用。完整部署合同285项中232通过、53失败，原件`build/verification/chat-sql-deployment-20261021/root-deploy-contract-20261021T194347376Z-28312a0b/`；大量连带失败来自prod夹具缺GlobalAuthorityTiDBHost，已补显式虚构DryRun端点，尚未重跑。其他旧夹具与默认配置须按最终真实规则逐项对齐，不能将静态修改算通过。

源码审计确认两项仍需闭合的实质问题。SceneManager旧逻辑允许600秒无持有者释放证明后强制清位置，正在改为UNKNOWN保持，正规释放证明才可推进。data_service默认仍是RouteStore.Mode=redis、SQL角色交易确认关闭，DB与login的强路由读门禁默认也未启用；这些默认值使本批全球强SQL迁移不完整，正在沿既有db_first、导入、reconcile及ready框架统一配置和启动闭环，不新增第二套权威。旧DS通过证据只代表当时输入，后续须重新验证受影响的DS/DB/login/SM。

Unity私有Job监管8项专项由root实际通过，原件SHA `FACB2E8629AD2D006CE256954E9A331DBA0AFEE00173F9CD2F46986752E13689`。仅自建无名Job核验真实后代，关闭Job不杀进程，未纳管外部进程。此前失败导入留下的空锁文件，经创建时间、旧家族全部退出、当前无Unity和独占文件打开核验后归档；没有删除缓存。新的正式Import仍在运行，尚无导入或EditMode成功结论。

## 17. 全球默认收口与定向验证现状（2026-10-21）

DS/SM本批42路径源码完成收口，root实际格式化31个Go文件；另格式化Rank SQL可观测性10个Go文件。格式化原件`build/verification/current-completion-20261021/go-ds-sm-global-default-source-20261021/gofmt-root-receipt.json`，SHA `693147D92A5D121F9534B7D2A001A18DBF850308BB3C5DE384036390635145A3`；DS/SM最终清单SHA `9718689FAD5C786E253A3E1DCDBF2D6F5B8ED68C429FDCE58D4669809C04505C`。默认全球模式要求TiDB、db_first、reconcile/Arm/read-ready及角色SQL证明；只有显式LegacyNonGlobal且dev/test才可运行隔离旧夹具。首次空路由不能仅凭空Redis/空路由就Arm：必须确认权威目录及全部显式白名单玩家表为空，缺表、历史、超时均拒绝。SceneManager不再以600秒等待授权释放，并给位置、done和回滚接入原owner_revision标签及单调观察下界；只读目录caller使用独立密钥。以上当前源码尚待新四阶段及真库验证，旧DS1586通过记录不冒充新源结果。

C++全量r5实际运行约14分钟后在Cross退出原因表static_assert失败：生产已新增角色交易冻结，但测试仍只列六种原因。仅补第七种和冻结压制回归，保留static_assert及原断言。root复核1868实际文件hash，与旧批准相比只有Cross.cpp变化，26守护段保持原正式批准；新全源摘要`758af4385dc812f5db5313c26896cc57a946c9ad4cf56fe56a36741501c484e3`。定向正规Build再次实际发现Cross项目缺OpenSSL头路径：role_admission_test.cpp引用service_call_auth.h时找不到openssl/sha.h，NoRaw严格解析退出2，MSBuild退出1。原失败保留于`build/verification/cpp-cross-exit-cause-recovery-20261021/run-20261021T203740243Z-f13c985f/`；正在修正规项目依赖，不跳Strict，尚无Cross或完整Build通过结论。

Unity第二次正式Import原生退出0，5个所需程序集均有本轮Csc及对应DLL证据。原验证器因为限定只允许一个meta变化而失败，原失败保持不变。root另做只读精审：151个已有贴图meta是Unity标准规范化，保留GUID、已有策略及无压缩语义；另两个新增meta对应已有源码。exact153清单SHA `C0D62743B47A755AF4F8E55559DA097844338F9F422081B14CD40825997CB3F0`；独立后审实际退出0，原件`build/verification/unity-owned-job-monitor-candidate-20261021/actual-import-post-audit-proof-root.json`，SHA `D6D4F6BF5CEFC20450204F5B4059CD602FB4F252B01840EB8519B89A8F50BD64`，不含测试通过结论。随后四程序集EditMode实际被运行资源门禁中止：可用内存2,382,724KiB低于2,883,584KiB，unknown_heavy为空，未生成测试XML；原件`D:/luyuan/mmorpg-client/Builds/verification/current-global-editmode/tests-20261021T202656181Z-d04f83b7/`。仅清理本任务已登记家族，不控制外部进程；不得将该轮计为测试通过。

独立Java r11已准备两个Login夹具的74项定向回归及后续30模块完整构建，尚待实际结果。真实TiDB、独立Redis117、三broker Kafka、mock支付退款提现和第21单交付、真实双区故障恢复仍未完成；未执行项和外部skip均不算验收通过。

## 18. 角色准入根因修复与当前真实回归（2026-10-21）

C++原生回归最终确认并修复生产名字查找缺陷：RoleAdmissionSystem有嵌套类型LocalNode，成员RequestEnter和Seal里的LocalNode()优先构造该类型，未调用匿名空间的同名节点读取函数，导致空实例身份拒绝。将helper和两个调用统一改为ReadLocalNode，共三行，所有身份、原版本、会话、封存及SQL确认条件保持。夹具增加真实生产前置检查与注入节点提供者调用断言，辅助函数fatal向外传播，避免断言失败后继续vector越界；38处行号宏各占一行。原运行崩溃和诊断夹具同线宏导致的编译失败均保留，未替换成预期反事实红灯。

本轮先正规重建scene.lib并完成其全部TU Strict，再用新库正规构建Cross、执行四TU Strict和真实list/all。339项实际全部通过，零失败、跳过或未运行；15个native阶段均退出0且清理无错。原件`build/verification/cpp-cross-admission-name-fix-r5-20261021/run-20261021T211416983Z-5569ed67/result.json`，SHA `8528F8F55E73C83E53427A67206BD7F98088AEFB63994E01F5CCB702B2E8747B`；当前1868源摘要`BCD2FB1B804D4A01DB23883BC83D80BCC633630232147461023EAF4992CF3166`，root完整源批准SHA `91C981DC5A1D86FEB6CEE02B117F52A09384CF05A8F6D351045A8CBF61C6EA2B`。这只证明当前scene/Cross目标；完整game、独立RPC及29个测试EXE尚待后续，旧观察者142通过不能直接冒称新scene库链接的当前结果。

独立审计又闭合两个源码边界，尚待新普通/真库执行：有效位置PB的新原修订sidecar损坏改走独立错误，LeaveScene不得误用旧坏PB毒数据成功ACK而丢清理任务；新空世界探针在每个正规白名单玩家库还检查centre和旧user_accounts非空角色blob，密码空目录不算玩家、不触碰独立密码DSN，缺正规表或未知blob拒绝fresh。最终DS/SM42路径清单`manifest-final-audit-formatted.json` SHA `006D3917073F5F00280A0DAAA327990319E0E17060F21A77A13B241ED7516DC2`，位于`build/verification/current-completion-20261021/go-ds-sm-global-default-source-20261021/`。

Java r11登录定向实际74项中70通过、4失败，剩余四条旧事件期望同样漏了session目标区围栏。只补完整序列并将一条过时测试名改成两区核验，r12正式sync/check已实际通过，当前2664输入；新74及完整30模块仍待运行。

Go Chat首次实际GoList被-mod=readonly拒绝；独立离线go mod tidy -diff诊断实际表明现依赖选择要求mysql1.9.3。诊断未改任何模块文件，不产Build绿；root随后仅把go.mod的1.9.0更新为该已缓存版本，并替换go.sum两条对应校验，保留其他内容与换行。原诊断在`build/verification/current-completion-20261021/chat-mod-tidy-diff-execution-20261021T211007934Z-6f89da76/`；新源仍需重新捕获/构建/测试，不沿用旧805源计划。

Unity顺序导入候选尚未启动Editor就因监控证据写盘超时停止：真实采样11ms、可用约8.69GiB且unknown_heavy为零，但D盘该741字节证据落盘延后约7秒，完整2秒门禁正确终止。未认定磁盘故障、未放宽门禁；root另建C盘私有证据根并逐字复制原helper，保持其BASE输出边界、全部输入/meta/5Csc批准、四程序集及Job监管，当前新计划SHA `69D17974CFD98C6E62B741A80FE78F11F7E685DB788B8D4BEB0B953D0EDC2E5F`待实际运行。批量纯hash也会产生I/O，现与native验证错开执行。

新增robot的commerceacceptance独立tag驱动已root格式化，SHA `8FA78C62E59A38AD5D65D86BDA5A6B6C27BE0722926871E8EB0258249C894E0F`，尚未编译/运行。覆盖官方25单、RMB托管25单的前20离线/尾5真实到账、重登、卖家收入提现及取消后迟到付款退款；所有经济操作走真实Gate会话/RPC与本地mock正常接口，不手写SQL资产终态或回执。真实支付渠道继续关闭。真库、全栈驱动及双区故障恢复仍未完成。

## 19. 登录74项独立归档复核与余量（2026-10-21）

Java r12登录原生Maven实际退出0，两类29+45项全部通过，零失败、错误或跳过。原归档器仅因策略按Unicode排列、XML名称按en-US文化排列而判红，参数化名称[1]与[10]等六处顺序不同；逐项名称集合没有变化。root实际执行独立只读复核，两侧均使用StringComparer.Ordinal并拒绝重复，确认原73名称加一处已批准改名、原2664输入与6506依赖首终原件、253工具执行链、资源及家族退出事实。新proof为`build/verification/java-login74-readonly-post-audit-r12-20261021/audit-20261021T214123704Z-cea12bb4/proof.json`，SHA `C9D3E3E839FBA738A3E43ADAD04170B59130D1D77E8AEA061A0FB9AA29783FE4`；原失败proof不改写、不以复核冒充重新执行Maven或完整30模块通过。

commerceacceptance驱动随后补一处message_id的显式uint32转换，当前最终SHA为`2FA3C9089B75A8BED684656E674965EAA39DDE1A975DF6B5AD1738D6983FA260`；第18节8FA为转换前历史格式化结果。专用tag的GoList、编译及四子例真实运行仍待执行。

按本任务各域已冻结交接，当前没有登记尚未实现的已知业务项；这不是全仓无缺陷保证。未完成验收仍分四块：当前源C++/Go/Java剩余构建测试与Go互操作；Unity四程序集；TiDB、独立Redis及三broker Kafka真实专项与商城mock资金和游戏到账；双区旧围栏、双ACK、恢复与解冻。完整C++正规串行构建正在执行，未有全量成功结论。物理容量与权威整库恢复另受存储容量、备份及外部审计围栏约束，不宣称无限容量或整库丢失已提交决定后仍能凭空恢复。

## 20. 当前C++完整工程构建通过（2026-10-21）

root已实际完成本轮game.sln与独立RPC工程串行正规Build，114个native阶段全部退出0；game实际267次Strict解析、38份新鲜通过标记，RPC实际17次解析、一份新鲜通过标记，均无缺引擎跳过或缓存标记冒充。输入与产物首终封存通过，`result.json` SHA `EBA695463F65294C83911BE8FD483E7E0AE9676A94649DDD871319779B621D07`；目录`build/verification/cpp-current-v5-fullfresh-r7-admission-fix-pending-20261021/run-20261021T213255244Z-7db2275a/`。产物manifest SHA `6C36DF8007EC6D5ABB3403F06B1BB53B3CF60B22988ACB65ACF14F527F01E6EC`，复核报告SHA `F3A66D0877A8A4EA886058449A3EC8E588EEE12B8CE4101BC173177CE13AB6F3`。这是当前源正规增量构建及全部适用Strict通过，不宣称全量Rebuild，也不替代29个测试EXE及互操作的真实执行。

随后当前26个测试程序及原三轮定向/乱序回归实际全部通过：29个执行阶段、60个native均退出0，2930次列名包含2928次实际PASS和两个既有DISABLED基准，零失败、跳过或非预期未运行。次数包含乱序重跑，不称为2928个独立测试。原件`build/verification/cpp-gtest29-current-v5-pending-20261021/run-20261021T215303610Z-9232cbf4/result.json`，SHA `B9204403E1E242DB0A49A1D901FE430D55EA555DC08243F6A7EA02003E737DCB`。独立三个测试工程的正规Build及fresh Strict也实际通过，`supplemental-build-20261021T215555539Z-a6e18180/result.json` SHA `B2F3AAC9874984B789EE7F1D9E950BACFC7C5725E559B09B4414EC68851E310D`；1868源、150库及32EXE路径边界保留，仅这三个具名测试EXE变化。三个程序及其真实Go互操作尚未运行。

## 21. 全量验证新发现的依赖与API错误（2026-10-21）

Java全30模块首次当前执行在game-friend的testCompile停止，三个真库夹具使用Spring的ResourceDatabasePopulator却未声明spring-jdbc。原生退出1，原proof SHA `68C4CFCE048B0D10BBF4FB05345F3603811F46EF822D75F5565B9805CBA111B6`，位置在第19节full-reactor的`normal-reactor-20261021T215811799Z-d814f5ed/`。仅Friend POM增加test-scope的spring-jdbc，沿根BOM既有6.2.19及本机已缓存JAR，不改生产依赖、测试或SQL解析。POM新SHA `1012C7CB2C9910D053246D6589FBA5D61E4B77EBCF283154DFEFE72CE4623FD1`；修复后的正式sync/check及全30模块尚待执行。

Go DS实际GoList通过，但随后正规Build原生退出1：新空世界启动探针误用redis.New(config)，而仓现有配置构造API为redis.NewRedis(config)。仅修该函数名并实际gofmt，servicecontext.go新SHA `CE83E477284D18CCE41F0CE206C099AEE1A9CC5EA2A98AF7DB344380691BC8AB`，保留错误返回及全部启动拒绝条件。旧Build proof SHA `7EC75FF16EF52CAB491292D390C4737B19444D062ACDEC029F8BFAF3D85CA81E`完整保留；当前新的GoList、Build/Unit/Vet及运行包未执行，不复用旧DS绿灯。

商城tag驱动又补真实双区观测：26账号卖家A、买家交替B/A，按每个账号原区走正式Gateway分配；登录后通过SceneInfo(with_channel_directory=true)的新回包确认zone及当前scene。当前gofmt后SHA `E297C1A0DE89E191EA5ECC357F8C5B3F3905D59254D4B080F099E7C23047E903`，此前2FA3为单区驱动历史，未编译运行。双区26账号的owned启动接缝、当前迁移工具及实际全栈仍在准备，不能以JSON配置模板代替真实启动或到账证据。

## 22. Go 当前构建、Java 夹具与客户端资源失败（2026-10-21）

Go DS 修正 API 后重新捕获 939 项源码、34483 项依赖及 109 个根，正规 Build 实际通过；Unit 为 1625 PASS、3 FAIL、90 SKIP，JSON 解析零错误，原 Capture/Build/Unit 家族全部退出且清理无错。原 Unit proof SHA `A0DB24E625A468AB60A632DED2A1006BF404775DACA2861372F234EBE895FBB2`；暂停检查点 SHA `924A2B4B7B8830F64854C8654914B3787592E78AC776636F757DABFF765E3255`。三个失败均为随仓配置仍应使用 redis、关闭 read-ready/Arm 的旧断言；现在按已批准的全球 db_first/64/ready/Arm 更新，并保留原始组件缺省及 redis 模式拒启测试，未改变生产配置值。`config_test.go` SHA `5840DC3F0B5EFB645C9EDDAA1A1431031C3187650B938CA8D394FBB53A3CF387`；YAML 只纠正历史回退说明，SHA `F1107AEE5FB46C31A6B0217CC2A17BBFFA77AFB4A21A2140C8B7586F647E45C1`。修复后四阶段尚未运行。90 项外部跳过已逐例映射后续门禁，新增空路由遇到已有玩家历史的 TiDB 回归包含于 DS24；不沿用旧 89 项清单。

Java r13 正规 sync/check 与新鲜 Prepare 通过；全构建已越过 spring-jdbc 缺依赖，随后 Friend 的 125 项出现 3 失败、1 错误、52 外部跳过，四个失败均在 JdbcFriendStoreBudgetTest。旧 connection mock 未提供生产路由守卫要求的元数据；只补 MySQL metadata、真实 route_meta 与 writable/shards，并保留生产守卫、截止时间及事务回滚断言，未注入空守卫。修复夹具 SHA `8E70D839B86C493AA3928FF63250BCA1F045AFF1F381C78728A066908CC54344`；原失败 proof SHA `AE06A2A9D1CFB3C8B02E0A7115651704379A7A9D716FD59F598B859DA7C1C121`。r14 源仍需实际验证；后续完整 Maven 使用官方 `-fae`，失败仍返回非零，仅让不依赖失败模块的其他模块继续，不将被依赖阻断的模块计为通过。

Unity 改用 C 盘证据根后采样写入正常，两次真实 Tests 仍因可用内存低于原 2.75 GiB 门槛停止，均未生成测试 XML。后一次已进入 Tianyong 测试，最低 2874836 KiB，比门槛低 8748 KiB；unknown_heavy 为零，21 条原家族记录均退出。原失败结果 SHA `EEBE6BE4BA8403F0082DBBE94AAC911692A03B5F22EBFE6835AB232E05EC4A65`，不计通过。当前准备将原四程序集各放入独立 Editor 依次执行，保留全部 case、必需 3+1 回归、原 153 meta/5 Csc/91309 输入与资源门槛；新计划 SHA `F513FB2C3234D850258DCC7ADC2ED3E6FE5EB03636FC46FF16461BE02C66EB30`，尚未执行，不删缓存或降低贴图导入质量。

## 23. 完整 Java 诊断、客户端实际结果与事务修复（2026-10-21）

Java r14 正规同步、两份新鲜来源检查及全部 30 模块 `-fae clean install` 已实际执行。原生退出 1，非监管中止，源码与依赖首终未漂移；60 条原家族记录全部退出，清理无错。553 个 suite、7568 项实际测试为 7200 PASS、59 FAIL、7 ERROR、302 外部 SKIP；模块为 16 SUCCESS、14 FAILURE、零模块 SKIPPED，产出 16 个 JAR，不冒充完整当前制品。原件 `build/verification/java-current-fail-at-end-candidate-r14-20261021/normal-reactor-20261021T230122040Z-6ba9bbf6/proof.json` SHA `3299BD345709528A9463B63E00E04584ACED4142DDF9B96831D44ABACEBF886A`。Friend Budget 原五项及 Login74 这轮均实际通过。

编译错误已按正式契约修源：TeamSnapshotS2C 的字段是 team，改正错误 getView/setView 并保留实时 SQL 匹配投影；Mall 配置是 record，五处规则调用改为其正式 accessor，不改默认、公示、手续费或旧单冻结语义。另五个测试 POM 仅补 test-scope spring-jdbc、两测试补缺少的 List/BigInteger import。其他 13 个失败按实际房间、票据、场景/Inbox、路由元数据、namespace 与配置表契约修复，保留原断言并增加四个回归。源码组 manifest 分别为 `2BB3AAA4D76C0268004C77275FA51F151A6CA85244266133FCF0CB8605EAB143`、`78AC063496A66644F369667AE581FCFE4AC1B4015F754C6866371FA2F7B18300`；两组均位于 `build/verification/current-completion-20261021/` 对应 r15 目录，尚未重跑。

Data 的 51 失败、2 错误主要暴露普通释放缺少调用方事务：SQL 夹具按已记录合同显式构造 PlayerStore、不依赖 Spring 代理，AdminOwnership 却直接调用普通释放；新的同库路由锁正确拒绝，造成释放未提交及后续请求仍判在线。保存构造时同源事务模板，仅包裹 release/releaseMany 的普通释放，带截止的 claim/recover 继续使用 PlayerStore 自有事务；原路由锁、旧 E、PITR、最终存盘与回滚断言均保留。两个直接释放的 SQL 夹具同样补合同要求的外层事务。导入拒绝文案补齐本来已经检查的未终局交易，不改 guard 或测试断言。四路径 manifest `java-r15-data-release-root-20261021/manifest.json` SHA `F76EEAD300D3F0203BFF848A3586EBC4F4D00A50A7271071A984170581C1EE76`。r15 总 28 路径静态变更待正规当前 TargetedH2、全 30 模块重跑；不继续扩白名单复用旧 H2 绿。

Unity 四 Editor 方案实际已跑到第二个程序集：Net 70 PASS；Tianyong 1122 项为 1101 PASS、21 FAIL、零 SKIP，原生退出 2，非资源中止。后续 Battle/Jubaozhai 未执行；四个 native 的原家族分别 2/2/21/24 条，全部退出且清理、输出、证据无错。原结果 `C:/Users/luyua/.codex/tmp/xuanming-global-unity-evidence-20261021-2f813027fbca4d4887a50cbded210d13/tests-20261021T231248001Z-8140c4b9/result.json` SHA `1ABBFCF0FD5BE2294D25ED70292510DEEA0F2CFD040670E64E01789818CDCABC`。客户端 Team 修复首次 Gate 明确限流未清已保存意图的问题；已有未知尝试、故障回包或重建原件遇到后续限流仍保留同 UUID。新增三条正负回归，原未知恢复三条必需回归保留。两个 UI 夹具按实际启用文字及正式行底图选择器观察，原文字/排序/着色断言不删。地图异步预算与边界另按真实 Dispose 合同修复；既有提交删除的旧天墉城资源涉及 11 条历史失败，处理方向已向用户确认中，不能擅自恢复旧生产场景。

新增端口归属叶 API 五项实际全部通过，原生退出 0、监管与清理无错，最长读取 6ms；`C:/Users/luyua/.codex/tmp/commerce-endpoint-probe-1b0b58299be9497191329253419bc465/proof.json` SHA `085622FC4A05A15B0BE8138EB5406E8244B7ACBCA4B54A0756912D608E296B8D`。这是端口归属专项，不是商城全栈通过。

## 24. 当前 DS 四阶段完成与 Match 显式迁移（2026-10-21）

DS 新 Capture 实际通过，939 源、34483 依赖闭包及原工具均绑定当前原件；随后 D 盘 Build 因资源复核外围耗时超 2 秒停止。样本本身 41ms、内存 9693688KiB、unknown=0；返回样本至证据落盘外围约 2426ms，不是源码编译结果。只将四份 execution-plan 逐字拷贝到 C 盘本任务私有证据根，runner、Go 命令、VCS、GOCACHE、完整源与闭包及 7GiB/2.75GiB/2s 门槛均不变；原失败保留。新 ready SHA `9562E68606DEC5D014177EEBD9C2193CCF602A8FABFDEDBB35F858758EC42EBF`，C 根 `C:/Users/luyua/.codex/tmp/xuanming-global-go-evidence-20261021T233020397Z-4d221b14`。

C 根下 Build、Unit、Vet、RuntimeBuild 四阶段实际全通过，sequence SHA `4A656CBF2F741CEF0A28448939266B0F220EB2AC4FBED632854267ABBC577B56`。Unit 为 1628 PASS、0 FAIL、90 外部 SKIP，解析零错误、四个无测试包另列；90 项逐例映射后续真库，仍不当通过。Runtime proof SHA `B27DBEA6DA660D9E0920B8C0925E81C010AFD76EF8C4B45179F256BFAEA7B9DC`，正规 AMD64 EXE SHA `07DB9E40AC5F3621B38554BBE74E0A37F8554F6ADA5F4E378E0D50DC08FF1FAF`，尚未启动。Capture 及四阶段原家族 2/6/22/3/3 条均退出、清理与输出无错；窗口归还 SHA `F298764215BDD1A32F134CB3115ECD9BAC951B86A7F1DA8135DE746741412B69`。当前总队列其余服务仍须逐项运行，不能以 DS 代替全 Go。

owned 启动静审找到 Match 的实质缺口：常驻 Open 无条件 schemamigrate.Up，即使已迁移仍尝试建账本表，DML 账号无法接流。沿既有迁移框架补正规独立 `-migrate` 与迁移凭据；常驻只 Plan、结构未齐或需人工迁移拒启，原候选索引检查保留。表清单仍唯一来自 proto；迁移后再核索引，缺历史索引转 Manual/非零，不自动 ALTER。同步本机命令、部署 Job 与 Secret 隔离；运行子进程不继承迁移身份。最终 12 路径 manifest `go-match-explicit-migration-source-r2/manifest.json` SHA `3DE29306183EB2EE715AA877321E2C5F63D5C5D217D96D8D7FD5FBDCFF6AEF58`，未编译或跑真库。新增五项普通、一个 TiDB 权限回归；真库总项从 200 扩为 201，迁移合同预期 47、端口合同 22，均待实际执行。

## 25. r15 实际 H2、全量中止与新增移植遗漏（2026-10-21 UTC）

r15 当前 TargetedH2 实际通过，723 项为 598 PASS、125 既有外部 SKIP，零失败、错误或未知跳过；八个必需 suite 的 131 项为 124 PASS、7 既有 SQL SKIP。21 条原家族记录均已退出，清理无错。原 proof SHA `86194956546A2F04CC474DFFE2947223923F1B0E11913AE2AB96CBA1A75B1682`。这些是本轮 H2 证据，外部跳过仍不是真库通过。

随后同源全模块实际运行因一次资源复核外围耗时超过 2 秒中止，未取得 Maven 最终退出码，不能声明全 30 模块完成。624 个已落盘 suite 为 9035 项：8137 PASS、18 FAIL、32 ERROR、848 SKIP；21 个 JAR 不作为完整制品。原 proof `java-current-final-candidate-r15-native-full-diagnostics-20261021/normal-reactor-20261021T234715801Z-291ef0f9/proof.json` SHA `2251E133BDFF3EC28A6DF65D16A06370FA622BD098582C1208C72684B2EC7A0E`，67 条家族记录均退出、清理无错，首终源码及 canonical 等价检查通过。Data/Gateway/Robot 尚未执行，Data 四路径修复仍待实际验证。最后资源样本内部 11ms、可用内存 8183328KiB、未知重进程为零；样本开始至 D 盘证据文件创建约 3889.5ms，原件未分段区分 JSON、调度及文件打开耗时，不武断归因于写盘本身。后续新鲜候选将使用 C 盘任务私有证据目录，门槛不降低。

本轮又发现并修复真实移植遗漏：Team 旧建票接口缺少 `TEAM_TICKETS_QUEUED → Tickets.Queued` 映射；Java 战斗共用判据遗漏文档与 C++ 已有的非空 `passive_stat` 拒绝施放规则，十二个新被动被误判可主动施放，保留正式 46 行/30 可施放 golden；两个 Mall `@Repository` 类的 final 阻止 Spring 持久化异常转换代理。均增加或保留对应公开接口、正式 Spring context 和边界回归，没有修改 schema、导表产物或放宽断言。Team 两路径 manifest SHA `2767C5685C5BCA992C67D7A00F4867F50FF811307641FD8AD405A94D0FFF53F8`；Mall/Scene 五路径 manifest SHA `071A7A7265878533E9CF9CDA34CC938E0C2B7C9E2269B413F13EAD505A6A5F10`，尚未重编译。

其余 43 项失败已对应九路径修复：无 Global provider 的显式开发/测试装配先 NPE 遮蔽门禁 28 项、组件缺正规评分 DataSource 11 项、以私有账号作显示名的旧夹具两项、真实 route_meta 首锁顺序一项、SHUTDOWN 既有 FIN 期望一项。生产 `GlobalMatchConfiguration` 的 SQL/TiDB 拒启门禁不改，并新增生产缺权威拒启回归；评分夹具使用正式 H2 SQL 与路由守卫，锁断言仍区分 H2/TiDB。manifest `build/verification/java-match-context-battle-fin-overlay-20261021/source-manifest.json` SHA `FA8634CE4C3C4751665A2521AB91C55C6F2573967C9FCA4952A485A767A7DA04`。与上述两组共 16 个不同文件，覆盖本轮 50 项失败；原测试保留，修复尚未运行。下一新鲜 C 盘候选位于 `C:/Users/luyua/.codex/tmp/j16-79f0a6b2`，候选 SHA `4D75375FBC564E6DE9F01969A75E37C3AFD7F2ECB474F80E3A8925E272AB1C4E`，不是已完成的构建证据。

部署合同本轮 Chat3、Ports22、Global5 均实际 native 退出 0；监管汇总因旧计划将 Global 写为 3 而拒绝，不算五套全通过。原 proof `C:/Users/luyua/.codex/tmp/deployment-current-c6cf03d1537d47de9cbe722262f89667/proof.json` SHA `16B530941DA62303ED7FAE3084E0AE1265D65FD9DD63081343CC8DC604774A25`，六条原家族记录均退出、清理无错。已依据原 stdout 与现有五个源用例将计划改为 3/22/5/47/285，源码和断言不改；迁移与完整部署合同尚未运行。

Unity 新完整捕获 91306 路径两次观察相同，捕获 SHA `16223B251D2DDCB8AF4A2D72A7D23FB4849814A52558022CC7A02C317C5771BE`。精确九项源码 overlay 首次硬拒，额外四项为官方 Performance Testing 包正常清理的两个临时 JSON 及各自 meta。已独立读取缓存包的固定路径、创建和 `PostBuildCleanup → DeleteFileAndMeta` 实现，只批准四个历史确切 SHA 到缺失的有限来源，根批准 SHA `A227F53CB2408F26C9D722720BACD38494F6AF4397AB78D90951BD086ADFF087`。没有恢复、删除文件或忽略整个目录；本轮 Import 首终仍须零输入变化、原 153 meta 不动、新五个 Csc 实际编译，尚未执行。

## 26. 客户端本轮编译复核与实际 13 项失败（2026-10-22 UTC）

新 Import 已实际执行五个 native 阶段并全部退出 0，五个目标程序集均为本轮 Csc 与新 DLL，91306 项输入首终相同，原 153 meta 未变。父汇总脚本在 Import 的空 attempted 列表属性访问处异常退出 1，没有父 result；原 stage 仍保留 `passed=false/root_review_required=true`，不改写成全流程通过。root 独立只读核对全部 native 原件、原句柄退出、完整输入、五个产物 SHA/时间及正式生成引用后出具编译后审：原 stage SHA `0CC3E64F6B519C70D7C0B087317E8A8BCC8B497F37AE53A35CBA59BF25F40F6D`，只读审计 `FA6F10CFFB79D0A84B4D85CA5969BA12398E31BBC7B3C95278F72B16FFD8EADA`，根编译批准 `AF103A7CA1E1B726D0F4AD0F283D0C2BD15DA92509A226C0AD84736313389228`。这只闭合当前编译证据，不代表测试通过。

随后四 Editor 回归实际完成 Net 70 PASS；Tianyong 1125 项为 1112 PASS、13 FAIL、零 SKIP，原生退出 2。原结果 `C:/Users/luyua/.codex/tmp/xuanming-global-unity-evidence-20261021-2f813027fbca4d4887a50cbded210d13/tests-20261022T003851147Z-64e471ce/result.json` SHA `CDE9BF11CF4D36202B9AFD209432BA8CD9C6E193C1DF24191B8E5F8301D0BA03`；四阶段原家族 2/2/19/20 条均已退出、清理无错。失败为原 11 项历史资源契约与两项 SceneChannelWindow；新增三项 Team 意图回归及本轮地图边界/Dispose 修复已进入真实运行，不能将源码完成代替整体绿灯。

Battle/Jubaozhai 本次未执行。有限诊断确认 fail-at-end 外层无法取得非零 native 原件：共享 RunNative 先保存 row，随后 throw；候选调用者尚未来得及做纯 NUnit 失败分类便退出。后续只在调用者有限接回本次唯一新增且原始退出 2 的 row，仍须通过现有 XML、新鲜度、零 skip、日志、家族和完整输入检查才能执行其他程序集；共享监管和原失败均保留。该修复仍待审与实际执行，绝不将未执行两程序集记为通过。
