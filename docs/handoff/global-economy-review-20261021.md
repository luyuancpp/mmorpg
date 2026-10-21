# 全球交易、角色归属与双版交付审核交接

本文件按用户要求供 Claude Code 审核。当前为实施中的交接底稿，**整体未完成，不是验收通过报告**。最终交付必须补齐本轮三语言构建、真库与真实双区故障结果；源码、静态检查、历史绿灯不得替代本轮运行证据。

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
| 官方商城 | `go/mall`，Java `game-mall` | 订单绑定原账号和修订；支付成功后角色换主且尚无 op，只对原单退款；金额与 uint64 wire 为规范字符串 |
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
