# 全球同服、交易与角色归属：Claude Code 审核入口

状态：**实现与验收仍在收尾，整体未通过最终验收**。本文件是负责人要求的精简审核入口；完整实施过程与失败原件见 [原交接](global-economy-review-20261021.md)。下面的历史通过结果只覆盖其记录的源码、依赖、环境与命令，不能替代后来变化的源码或重启后的数据库验收。

## 1. 最新版本与改动归属

2026-10-23（America/New_York）续跑时只读核对；后续未提交改动另外按实际输入记录：

| 仓库 | HEAD | 保护范围 |
| --- | --- | --- |
| `D:/luyuan/xuanming-server-mmo-cpp` | `7a23b4fed3381cb3e06ef5438f829c93473f2200` | 保留所有既有 dirty、生成器、存盘、路由与并行修复 |
| `D:/luyuan/xuanming-server-mmo` | `73de4efdbd9c5efdb9b698edd0328d0e443d528f` | 保留最新刷新令牌家族模型，在其基础上接齐流程 |
| `D:/luyuan/mmorpg-client` | `242e35060a7364bb2d3a28bfb2581b04e530f290` | 保留最新帮会经济恢复提示与断言；使用 CurrentMaps，不恢复旧天墉城，不覆盖新地图、资源、prefab 或 meta |

三仓均不是本任务独占工作区。**不要把整仓 `git diff` 归为本批，也不要用旧提交文件覆盖当前文件。** 原交接 §3、§8–60 与分批输入快照用于定位本任务接缝；输入快照是验证输入清单，不自动等于本任务作者清单。尚无逐行完整归属清单，不能声称所有 dirty 均由本任务产生。本任务没有 commit、push 或 tag。

## 2. 已实现的主要接缝与审核重点

| 功能 | 入口 | 本批行为与需审核的不变量 |
| --- | --- | --- |
| 原会话身份 | `go/shared/loginsession/reader.go`、`go/shared/roleidentity/context.go`；Java SessionContext / AuthorityJdbc | 账号、角色、归属修订来自原请求；缺原件拒绝，不能查当前目录把旧请求补成新身份 |
| 登录令牌家族（本轮接手，尚未最终验收） | `go/login/internal/logic/pkg/token/{token,family,family_test}.go`、既有 `docs/design/dual-token-authentication.md`；独立 Java `RedisLoginTokens.java` 及其单测、真实 Redis 测试和 `testing/InMemoryLoginTokens.java` | 保留最新 TokenData 五字段模型与 RedisKeys 家族键。新认证随机家族，原子轮换核原值、账号、head；access 校验家族有效性；仅缺字段旧 JSON 兼容，显式损坏字段拒绝。Go 保严格一次性，Java 保原受限宽限。核 Lua 首写前 TYPE/归属检查、TTL 损坏、淘汰旧候选与并发轮换，不把凭据家族和原请求角色身份混为一物 |
| 交易、拍卖 | `go/trade/internal/commerce/{engine,market,auction,role_authority}.go`；Java game-trade | 买卖双方与竞价参与者持久固定原身份；提交锁完整角色记录 P，按无符号玩家号升序；未知提交保持原操作恢复，不重铸资产 |
| 人民币寄售 | `go/trade/internal/commerce/{rmb,rmb_mall}.go`；Java game-trade | 持久托管、原资产快照返还、原支付单退款、卖家入账；暂时离线与终局拒绝区分，人工态不能伪装终态 |
| 官方商城、交付分页 | `go/trade/internal/commerce/mall.go`；Java game-mall | 原账号/修订绑定订单；角色已换主且无原资产操作时对原单退款；稳定游标避免前 20 个离线订单阻塞第 21 单 |
| 角色出售排空 | `cpp/libs/services/scene/player/system/role_admission_system.{h,cpp}`、player_lifecycle；data_service / trade；Java player-store / trade | 加载副作用前登记入场世代；seal 固定原 instance/revision 与世代上界；必须等待真实 SQL 持久结果，RPC 收到或内存冻结不等于排空完成 |
| 好友 | `go/friend/internal/data/friend_repo.go`；Java game-friend | 接受申请与双向好友原子提交；先全员 P，再容量和关系锁；旧身份与旧幂等请求不能跨过户写入 |
| 帮会与历练 | `go/guild`；Java game-guild | 原 consent、完整 roster、launch 与 ACTIVE 投影持久化；外部开战前先提交原件；TTL 到期不能证明没有迟到结果 |
| 安全取消开战 | Match authority / match_internal；Java game-match | Cancel 与原 Start 共用永久墓碑；迟到 Start 不复活；回复签名绑定原请求摘要；普通负答不能证明安全释放 |
| 邮件与收件箱 | `go/mail`、`go/player_inbox`；Java game-mail / inbox | 实际事务先原角色 P，再 inbox、seq、op；条目原身份及永久收据核对；后台恢复不伪造玩家会话 |
| Team 与匹配 | Go Match SQL authority；Java game-team / game-match | 九类写 UUID 收据、完整队伍原件、成员修订、永久取消、故障恢复；先本区，不足且超时才跨区；逐人核延迟上限 |
| 观战与客户端恢复 | Match / Battle；客户端 Team / spectator | 观战退出固定原对象；取消先于 Add 留墓碑；客户端持久保存原 UUID 与正文，损坏拒绝新写，旧回调不能改变新状态 |
| 部署 | `tools/scripts/k8s_deploy.ps1` | 唯一全局 TiDB 权威、明确全局 LoginRedis 与目录库、独立池进入连接预算；不是仅把服务副本数改大 |

协议事实源仍在 `proto/`，生成产物只由正规生成流程更新。Java 内部修订限定 `1..Long.MAX_VALUE`，溢出显式拒绝；Mall 的 uint64 wire 使用规范十进制字符串，不能静默截成有符号 long。

业务默认值已经负责人确认：公示 48 小时、寄售 7 天、手续费 5% 向上取整到分、卖家收入冷静期 3 天，均可配置；出售前离帮、好友保留；默认仅金币可交易，钻石关闭、绑定货币禁止。支付、退款、提现采用 mock 全链路；真实渠道关闭，未来另接。

## 3. 已有运行证据与覆盖边界

| 项目 | 已实际取得的结果 | 不能据此推断 |
| --- | --- | --- |
| C++ | 历史完整构建 118 条原生命令；29 个测试 EXE 共 2624 PASS，另 2 个原有 disabled benchmark；互通必需项 5 项通过 | 之后变化的 19 个源码文件已经重新编译或测试 |
| Unity | 2228 PASS、0 skip：网络 70、CurrentMaps 1125、Battle 955、聚宝斋 78；18 个必需项 | 服务端真库、真实双区资产交付通过；旧地图回归失败已按现行合同处理，不能恢复旧资源 |
| Go 普通验证 | 17 模块的既有 Build / Unit / Vet 结果与 14 个运行产物已有实际记录 | 所有外部 skip 已闭合，或之后新增的 Go 改动已验证 |
| Go 真库专项 | 已取得 Friend、Guild、共享资产操作等专项结果；Mail 真 Kafka/Redis 原通知用例实际 PASS、0 skip，所属临时 Kafka 已精确清理 | 父测试 PASS 覆盖其仍 SKIP 的子项，或完整交易故障链通过 |
| 主仓 Java | Mall 95 单测及 68 TiDB；Gateway 231 单测、4 MySQL、16 TiDB 的历史实际结果 | 独立 Java 30 模块与其所有外测都通过 |
| 独立 Java | 外测 33 阶段中历史完成 20；SceneManager 14、Team 80 真 Redis 用例通过 | 剩余 13 阶段、413 展开项，或 33 进程的全局双副本验收完成 |
| 主双区 | 一次真实启动已使 22 个核心 Go 副本就绪；随后 Scene 因临时表目录缺尾分隔符而失败；该私有配置已修 | 完整 36 角色就绪、两区交易、21 单持久交付或故障恢复通过 |

关键原件：

- C++ 历史汇总：`C:/Users/luyua/.codex/tmp/cpp-current-full29-interop-summary-20261022-r1.json`。
- Unity 结果：`C:/Users/luyua/.codex/tmp/u22-exact-final-input-audit-evidence-r7/tests-20261022T201420569Z-b0c6b893/result.json`，SHA `0451694F8FF00B0402F74DE305A374D1161989A0F9D3ABD17AB26109A6E025DC`。
- Go 原外部用例账：`C:/Users/luyua/.codex/tmp/xuanming-global-go-evidence-20261021T233020397Z-4d221b14/go-current-parallel-queue-r4/actual-unit-skip-coverage-ledger-r21.json`，SHA `EDA2F2967A85BE341FD59932FEA2A5CE3E8416E4D657BAE624D7C418CB91D439`。
- 主双区最后失败：`C:/Users/luyua/.codex/tmp/commerce-owned-5dfd29b7aebc4139b4961f412d0452da/result.json`，SHA `538F577FCE342F649084C026398D30570BF69597847CAE807526CEA247F12D0E`。14 个 Go 就绪后，下一启动实测 7332380 KiB，低于原 7340032 KiB 门禁而拒绝；不是业务通过。

## 4. 最新源码静态复核

下面是恢复执行前的文件读取与 SHA 比对，**不是编译或业务运行**；后续实际结果与源码变化见第 7 节。独立 Java 随后提交了 `73de4efd` 的四个令牌模型相关源改动，负责人已授权本任务接齐；此前“仅 PARITY 不同”的观察不能覆盖这次变化。Go 的正式生成也已在后继通过，不能继续将下面的历史待办当作最新状态。

- C++：1868 个已记录源、150 库、32 EXE；19 个源不同，库与 EXE 相同。因此旧二进制尚不能代表最新源码。差异涉及配置、Redis 异步存读、货币、inbox、持久化、战斗结果和相关测试。完整路径见 `C:/Users/luyua/.codex/tmp/cpp-current-static-review-20261023-r1/recorded-input-comparison.json`，SHA `71735143B3B04F4D147B33D620371F93938CA3B55A3DD130A4D813E7CE085263`。
- 独立 Java：2664 个已记录源中仅 PARITY.md 不同，253 工具与 17 个运行 JAR 相同。原静态观察将 6506 个第三方缓存文件中的 6 个报为缺失；随后 .NET 对这 6 个路径实际读取成功，SHA 全等批准值、修改时间仍为原 10 月 19 日，没有复制或覆盖。原观察 `C:/Users/luyua/.codex/tmp/java-current-static-review-20261023-r1/recorded-input-comparison.json`（SHA `C6532E574E7BBC9BAC8C7BE96B4ED03F32E448CD5D35D1E9AC3C5B0FEE5F4C1E`）保留；纠正原件为 `C:/Users/luyua/.codex/tmp/java-current-b0b7551-current-epoch-preparation-r1/six-tail-existing-current-readonly.json`，SHA `9A6B849D56965E7B8A681A5DC2DD42D709687009A632567B920BE529DF5A1149`。不把前次观察当作确证文件丢失，原因尚未证实。
- 客户端：31805 个已记录项目/包/服务端输入中，客户端与包文件 SHA 一致；唯一服务端差异是 `bag_quest_mail_data.proto` 新增保留容量的中文注释，无字段、编号或类型改变。Assets / Packages / ProjectSettings 共 18217 个路径重枚举无新增或缺失；106 个 DLL/PDB SHA 一致。没有重枚举所有工具、包缓存或编辑器工具链新成员，不能扩大结论。原件在 `C:/Users/luyua/.codex/tmp/client-current-static-source-audit-20261023-r1/`。
- Go：5360 个已绑定路径已去重读取。新增 shared/savebarrier 三文件及 data_service、db、login、scene_manager 的实际修改需要补验；还须沿 shared/assetop 等传递依赖确定影响范围，不能只凭直接 import 免验。已记录的 1842 生成输出、26 守护段、工程与账本等字节相同；生成器源码另有修改、新增与删除，正规生成门禁尚待按当前源闭合。只读审计 `C:/Users/luyua/.codex/tmp/xuanming-global-go-evidence-20261021T233020397Z-4d221b14/go-current-parallel-queue-r5-20261023/current-canonical-byte-audit-r1.json`，SHA `5C84F8837DCB4971AAB95F314B4CAA8339608F34E972BFB5FFC3CCA5680FD709`；没有把旧验证改标成当前通过。

## 5. 尚未闭合的验收清单

- [ ] C++ 最新源重新捕获，受影响串行编译、测试及正式运行产物绑定；原始红灯保留。
- [ ] Go 最新源与既有 17 模块结果逐项核对，受影响 Build / Unit / Vet / RuntimeBuild 补验。
- [ ] Go 原 515 外部键中，历史 232 个已闭合，**283 个仍待**：data_service 90、match 87、shared 2、db 14、chat 19、rank 5、friend 9、guild 16、trade 32、mail 9。最新源码变化可能使已闭合项需要复验。
- [ ] 标签专用真库入口、DB 围栏、GUID、Rank/Mail 链与观察者等专项，不能只用普通包的 skip 账代替。
- [ ] 独立 Java 剩余 13 外测阶段、413 展开项；按原方法名与 XML 逐项计数，不拼接成一次完整新构建。
- [ ] 客户端最新 `242e35060` 的 Guild 整程序集编译和回归；两份已有源码/测试文件新增两方法、六个展开用例。该提交不含地图改动，保留其实现，不用旧 Unity 2228 项结果覆盖新增 Guild 回归。
- [x] 六个 Java 验收工具正式 javac；仅验证原 18 个 JAR 的工具 API，不代表新增登录源码已编译。
- [ ] 独立 Java 16 个非战斗服务各双副本，加 1 个 Battle，共 33 个进程的实际就绪与业务链。
- [ ] 基于最新模型补齐两版刷新令牌家族：签发、原子轮换、access 校验、撤销与裁剪、旧 JSON 兼容及单测、真实 Redis 验证；负责人已明确授权接手。
- [ ] 两版 mock 支付、退款、卖家收入冷静、提现和完整资产到账；两版“前 20 单离线，第 21 单仍持久到账，再恢复前 20 单”。SQL OMS 成功不能替代真实 Scene 资产落库。
- [ ] 主栈完整双区：两区真实客户端入口、跨区与逐人延迟、旧会话/旧 scene 拒写、双 ACK、崩溃窗口、恢复后资产恰好一次，最后 source-after 与所属进程清理闭合。
- [ ] 独立 Java 的 Scene 崩溃、Battle 已存结果后故障及原合同恢复；真实业务与辅助进程全部清理。
- [ ] 核对并行数据回档审计与混版本持久化修复的最新状态，相关未修复问题不得被本任务测试数量掩盖；入口见 `docs/design/data-overwrite-rollback-audit-r2-20261019.md` 与 `docs/design/mixed-version-persist-field-protection.md`。

283 个原测试键的完整审核清单已导出至本机被忽略的 `D:/luyuan/xuanming-server-mmo-cpp/build/verification/global-economy-claude-review-20261022/go-pending-original-283-keys.json`。其中两个历史未映射的新键实际上已列入成熟 SQL 12 阶段计划；这里只补审核映射，不增加 PASS、不重复计数。Windows signal 用例在 Windows 不适用，仍须 Linux 验证，不能记成通过。

## 6. 环境恢复与不可作出的保证

本任务新隔离世界为 `codex-global-clean-20261023-r1`，TiDB `127.0.0.1:24000`、Redis `26379`；etcd 原项目端口 `22379`；专用三副本 Kafka `19492–19494`；Java Redis `13379`。旧世界、旧数据卷保持，不导入旧路由、不降低号段水位、不重置 Kafka offset、不清他人数据。本任务 MySQL `13316` 已正常停用，其 tmpfs/旧 UUID 不可当作未来当前身份。

Docker Engine 曾退出，旧基础设施 StartedAt 与协议证据只能作历史。负责人现已释放内存并要求继续；必须对同 CID、卷、端口、标签、镜像及新启动时间重新取证，重新核对强会话与正式 CHECK，不能把旧结果改成新 epoch。执行进程也已自然退出，需用当前 PID、创建时间和原句柄重新登记三语言队列。

门禁保持启动可用内存至少 7340032 KiB、运行至少 2883584 KiB、采样预算 2000 ms、未知进程拒绝；C++ `/m:1 /nr:false /p:CL_MPCount=1`。不终止外部任务、不改防火墙、不为通过而关闭断言或跳过测试。

“全球同服、可加副本”仍受已验证容量、数据库事务及部署边界限制。单机双区不能证明跨机房容灾或无限容量。任意整库 PITR 不能靠被恢复的数据库自证最新归属，必须有独立审计的围栏与修订上界；未做真实故障验收前，不能承诺绝对不回档、不损坏。

Claude Code 请先审第 2 节的实际调用链与 SQL 提交顺序，再核第 4–5 节遗漏入口；输出文件、行号、触发时序、损害与最小修正。按 AGENTS.md §10.1，Claude 只评审/改码并交具体命令，编译、测试、真库与压测由 Codex 执行。

## 7. 本轮续跑实际结果（后续继续追加）

| 项目 | 最新实际状态 |
| --- | --- |
| 最新 C++ 完整构建 | 105 条原生记录后，新的 VSCode Java 后台触发原未知进程门禁，04-game-build 被停止；203 个所属家族进程均退出，清理错误为空。原结果 `C:/Users/luyua/.codex/tmp/cpp-current-full-evidence-r9-20261022/current-20261023-r1/run-20261023T030455384Z-09da1c0e/result.json`，SHA `94C176210FAB860D756832881430B021C0527739129E9E99D5CD2DEF419DABC5`。部分库已输出，整轮不计通过；后继重新捕获实际产物 |
| 新 VSCode 授权收尾 | 用户明确授权关闭实例 25444 及后台；执行前原根已退出，只对原观察身份的 3 个残留使用原句柄关闭，未控制其他应用。原件 `C:/Users/luyua/.codex/tmp/authorized-vscode25444-residual-close-20261023.json`，SHA `D00EA756AFEE269687F94885E888E405FF054283391C972F907411FC1A6EF7E5` |
| 当前生成器源码 | 三处字符串内实际 BOM 已改为等价 `\uFEFF`，保留运行时字节；一处冲突负例的完整方法边界期望由空格对齐实际分号，保留拒绝与零产物断言。完整回归 **352 PASS、0 FAIL、0 test SKIP**，原件 `C:/Users/luyua/.codex/tmp/xuanming-global-go-evidence-20261021T233020397Z-4d221b14/generator-current-source-20261023-r8-method-boundary/current-20261023T035002806Z-b104b9d0/proof.json`，SHA `6BB351722889C092312AD5522BB1F6965CA9BF80429FF05D8EF23E5EBD4D0BE7`。此前真实红灯全部保留；使用原批准 SHA 相同的本地 protoc 私有路径，没有安装或覆盖 SDK |
| 正式协议生成 | 已按当前源正规构建工具并执行 `proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml -UseBinary`，原生命令退出 0；127 生成器源、128 proto、26 守护段及工程、账本等首终核对通过。26 生成输出和 4 个混合文件发生预期更新，C++ 输出与工程无变化；没有读写独立客户端。原件 `C:/Users/luyua/.codex/tmp/xuanming-global-go-evidence-20261021T233020397Z-4d221b14/formal-gen-current-20261023-r3/formal-run-20261023T035855748Z-c06c9007/proof.json`，SHA `46BE7F53C460C952E368857D5050A167C5DF14164FC70F4170D0A260FD14A30A` |
| 六个 Java 验收工具 | 一轮实际 javac 因 TradeSceneLedger 的旧包名失败；私有工具仅按正式生成源修正 import，原解析、SQL 和断言保留。后继六个具名 class 全部编译通过，1 条原生命令退出 0，所属进程清理闭合。原件 `C:/Users/luyua/.codex/tmp/java33-six-tools-current-evidence-20261023-r1/compile-20261023T040057158Z-d036086b/proof.json`，SHA `996E63B2A36A641BED5FFC333E95340606F3FDEB788540E12C46D6D644CC787F`；仅覆盖原 18 个实际 JAR 的 API，新增登录模型仍须单独编译与验证 |
| 最新 Discovery 库 | 基于 `73de4efd` 的新 Redis 家族键正规执行 `-pl game-discovery clean install`，1 条原生命令退出 0；303 个原 case 中 186 PASS、117 个原外部依赖 SKIP、0 FAIL/ERROR，普通库 JAR 已归档。原件 `C:/Users/luyua/.codex/tmp/java-current-b0b7551-current-epoch-preparation-r1/refresh-family-maven-r1/run-DiscoveryBuild/proof.json`，SHA `7E9AB4270057C64E3F9243F0DFBAB1EFFDF6DF8E43A6C4521B428559A2A6DD64`。仅该模块编译与普通测试通过，不把 117 个 SKIP 当作真库通过；Login 家族流程仍在补验 |
| 两版令牌家族的阶段结果 | Go 首轮原 5 个 Refresh 与新增 11 个 family 顶层全部通过，含子例 38 PASS、0 SKIP（原件 SHA `045AAA4A162F48601485763CD3B2D8B4207C4D9B44D53FB43A579A39F53ED723`）；具名真 Redis 原例 1 PASS、0 SKIP（`B84AF31E6A0AFBCF19125AA616DD49082F41D3F9643F81D7BBC9294B8228BA08`）。后续空/null 家族与永久 TTL 损坏回归实际出现 6 个失败子项（`10C1475E5B2A62E5C7CE8765203B443E6E59CF98C254BF089D95EAA016640A28`），正在最小修复；首轮通过不能替代修后验证。独立 Java 首个失效家族用例先实际红（`EDA5B9453690BD43417ABC57226C759950C3B652F5D741A3973CF8F4AA3D267F`），修后 Login 普通构建取得 332 PASS、36 外部 SKIP、0 FAIL/ERROR，原件 `C:/Users/luyua/.codex/tmp/java-current-b0b7551-current-epoch-preparation-r1/refresh-family-maven-r1/run-LoginBuild-positive-ttl-r3/proof.json`，SHA `BE17B22576FB0E5C5BFAC268285F55ADAC3D04DA1A9EEFA517AC93AB156D7E4C`。正常轮换与 access 并发读快照仍在补回归和修复，真实 Redis 及最终构建尚未闭合 |
| Kafka 单副本退出 | Kafka1 恢复后退出，ExitCode1、OOM=false；已取得停机超时日志，最初退出原因未确证。只正常启动该原 CID 一次，不改卷、配置、offset 或其它副本，新 StartedAt 如实登记；原件 SHA `42F1ABCA9952C0C12A6E0C954E543997B78509A5D284977E90049EDAD41AB8A5` |
| 新 epoch 基础设施协议 | **21 条只读原生命令全部退出 0**；八 CHECK 表达式与 SHOW CREATE、坏行 0、GLOBAL CHECK=1、TiDB 两列产品身份、两 Redis、etcd，以及五 Kafka 主题逐分区 RF3/ISR3/minISR2/保留期均通过。九 CID 首终标签、卷、网络、资源、端口和各自 StartedAt 一致，41 个所属进程均退出。原件 `C:/Users/luyua/.codex/tmp/java-current-b0b7551-current-epoch-preparation-r1/actual-protocol-20261023T033550108Z-db9135fd/proof.json`，SHA `FC7C2F606BB0272637A21AB3DB976C0B6E15FC49A317B5DFCCC38BBD179B5E1F`。player 号段高水位为 801/version8，高于六玩家表 max0，没有降水位或写入业务数据 |
| 当前真库强会话 S1 | 原用例实际 **1 PASS、0 SKIP**，4 条独立物理连接各核 7 项强会话设置，随机测试库 CREATE/DROP 完成；772 个源、17620 个依赖文件及正式生成输入首终无变化。原件 `C:/Users/luyua/.codex/tmp/xuanming-global-go-evidence-20261021T233020397Z-4d221b14/sql-s1-current-generation-new-epoch-20261023-r2/execution-shared-physical-strong-session-20261023T040939784Z-25f8c114/proof.json`，SHA `EE453B97B4729ECFB081137812C01AE27075A0CE2C7E3E1147ED27CAD1688E73`；后继九 CID 只读核对仍与本轮 FC7 协议 epoch 一致，原件 SHA `449364EACBE5E3045CE75A49B060E3F54A50E9DE4AF1AA08E21CB13846F33426` |

后继状态：Java 正常轮换与 access 多次读的竞态已由具名真实 Redis 用例复现，原件 SHA `9F8FFBFC5E43D86D85745E839E32235A43C8B6D2C0F015FDD4B73031F6A2EA5C`（唯一断言失败，原生退出 1）。修复改为一次只读 Lua 原子校验原值、access/family/head TTL 与归属；修后完整 Login 构建取得 332 PASS、37 个外部 SKIP、0 FAIL/ERROR，原件 `C:/Users/luyua/.codex/tmp/java-current-b0b7551-current-epoch-preparation-r1/refresh-family-maven-r1/run-LoginBuild-atomic-access-r4/proof.json`，SHA `A297D2EE93A05AB8CB4577F4B7244B7D787A7220283BCEA782813437409F2335`。随后全 23 项真实 Redis 测试有一项夹具恢复 family 时漏还剩余 TTL 而失败；仅修测试恢复原剩余期限，不延长 TTL、不改生产代码和断言。最终完整 **23 PASS、0 FAIL/ERROR/SKIP**，原件 `C:/Users/luyua/.codex/tmp/java-current-b0b7551-current-epoch-preparation-r1/refresh-family-maven-r1/run-LoginRedis-type-fixture-r5/proof.json`，SHA `C5B033D465807328182549B6459BFFF131B3311AE21FF6FE194F8143B0618EA2`。2664 项源码映射仅这一测试文件不同，生产 JAR 仍由 A297 构建，等价桥 SHA `695671D339C018D14F0C7510F328D9B0F03743E5A4DFE572D21BCC5689CE6946`。37 个原外部 SKIP 中本类的 23 个已据真 Redis 闭合，另 14 个须按各自证据核对。

Go 家族最终修复保留仅缺失字段兼容旧 JSON，显式空/null 家族拒绝，新增 access、head refresh 和 family 的正 TTL 检查。最终 miniredis 原 16 个顶层及子项共 **44 PASS、0 FAIL/SKIP**，原件 SHA `FA312E7B402FBDDB913A20DFB87E02273E42D4B91571DDDA97DAEA7CE8483052`。真实 Redis 的父项、null 裁剪保持全候选不变，以及三类 PERSIST 损坏子项共 **5 PASS、0 FAIL/SKIP**，原件 `C:/Users/luyua/.codex/tmp/xuanming-global-go-evidence-20261021T233020397Z-4d221b14/login-token-family-final-current-validation-20261023-r3/execution-login-token-family-final-real-redis-20261023T051352153Z-7b8141fc/proof.json`，SHA `B3E4217BB62EB982F287AE1A3F4EA2EC1AD35FB93C0B1B866581015254531DA0`。原损坏回归红灯保留；这些专项通过不代替 Go Login 整模块的最终 Build / Unit / Vet / RuntimeBuild。

C++ 后继完整编译实际退出 1：`battle_room_manager.cpp` 缺失 `RetireExhaustedStaging` 声明/实现，三处测试仍引用已删除的 `kMaxStagingRounds`；原件 `C:/Users/luyua/.codex/tmp/cpp-current-full-evidence-r9-20261022/current-20261023-r2/run-20261023T043239204Z-8745e935/result.json`，SHA `F2DBC2D953D956D01976977EAD1B3BCD9B7D46BE88B582D1FCD21776CC4AF8AE`。已按当前 24 轮快速、5 分钟慢重试和 6 小时总跨度补齐三个源码/测试文件，保留终局未知、禁止发结果及房间生命周期约束；新增虚拟单调时间验证慢重试与 Redis 不可用时的总跨度截止。修后最新完整构建 **118 条原生命令全部退出 0**，所有所属进程退出、清理/输出/证据错误为 0，1868 源、150 库、32 EXE 核对通过；原件 `C:/Users/luyua/.codex/tmp/cpp-current-full-evidence-r9-20261022/current-20261023-r4/run-20261023T050945262Z-401eadc1/result.json`，SHA `6F688A9DA99C65BACFD82E916FE89A9C1F294E05B3953EAFDE3488540F930CFD`。

同一当前源的 26 个测试 EXE 已取得 **2407 PASS、0 FAIL/SKIP，另 2 个原有 disabled 未运行**；两个附加路由入口另列 69 与 140 PASS，不重复合入 unique 总数。原件 SHA `03BC1885BEA01F3FC80E33E99E4FE3BD134012CD382780769241817C039C3114`；新增慢重试与总跨度三项实际执行通过。补充三个工程的 no-raw scope 工具遇到 `supplemental_projects` 元数据缺失，原生退出 1（原件 SHA `A1B760E7F95C6E49548F7306146431B7A9212865F3162ACEB16131C13A071334`），尚未计补三项或互通通过；该失败不改判，也不重新冒用旧的 29 EXE 总数。

Go data_service 旧测试替身缺少最新 `DeleteWatermarkWrongType` 接口，导致整包编译失败；替身已按正式无条件删除合同补齐，保留正常水位不调用删除的业务断言。后继完整 Unit 取得 **1653 PASS、90 个真实外部依赖 SKIP、0 FAIL**（`FA65680B162F52288A26DE0BA08A069850617B10A50BE4894221BD25EE969FCE`），Vet 与 RuntimeBuild 实际通过。Go db 整包测试随后发现旧断言仍要求恢复时保留 Redis 的 schema_gen=9；最新正式设计已规定按 SQL 两行最小值 4 覆盖旧索引，仅此期待值同步，旧索引 9 的输入及 writer 字段清理、幂等断言保留；原失败 SHA `B0A9481C500C23B84D7FE8BFC7FDC9A33F1A62A427F241B7129050C2AD6DB1E9`，完整修后回归仍在执行。

独立 Java 剩余外测的第一组 Chat3 已真正执行，三个用例均在 `PlayerRouteAuthority.requireMeta` 的 `LOCK IN SHARE MODE` 处报错，尚未进入业务断言；原件 `C:/Users/luyua/.codex/tmp/java-external-current-green-full-candidate-20261023-r16/run-TiDB-game-chat-20261023T053553637Z-5c66440f/proof.json`，SHA `FD483EFC4752B439EE956946A0EC8D68D2A4D31731D37BA1480A9111DCD307D8`。TiDB 不提供这里要求的真正共享锁；[官方事务文档](https://docs.pingcap.com/tidb/stable/pessimistic-transaction/) 说明其共享锁提升实际使用排他锁。已授权按真实 TiDB 方言显式使用 `FOR UPDATE`，保留 MySQL 共享锁、H2 原排他锁及锁序、预算和冻结原子范围，不启用 noop 绕过。该修复及所有受影响 JAR 的重建、真实冻结互斥回归仍待完成；单例 meta 的排他锁将串行化此提交入口，**不能据此宣称水平扩容后的吞吐不变或无限容量**。

该方言修复的 Common 正规普通库构建已取得 **150 PASS、0 FAIL/ERROR/SKIP**，完整 2664 源、253 工具、6506 第三方与原 154 依赖 head 首终无变化；原件 `C:/Users/luyua/.codex/tmp/java-current-b0b7551-current-epoch-preparation-r1/tidb-route-lock-correctness-r1/run-CommonBuild/proof.json`，SHA `30EE611E2417AFB25E5B171CB7C3D6F429B2BD89CA3108D311F1D7EFAC05FB52`。原 Chat3 与 Data17 的真实互斥验证及所有嵌入 Common 的运行 JAR 仍待后继结果，不能将旧六 JAR 或旧 30 模块结果改标为包含该修复。

数据回档审计的交叉核对只确认当前五个 C++ 数据安全回归在原 XML 中实际运行通过：同 key 未落定存盘先于新加载、只重试未确认成员、24 轮后的慢重试、总跨度截止、Redis 不可用时的总跨度截止。具名投影原件 `C:/Users/luyua/.codex/tmp/commerce-r19-current-resumption-static-20261023-r1/current-cpp-five-data-safety-cases-root-review.json`，SHA `D7FB5681949B65D8F5CB705934974288F18D5D6400DBBD3E47CF4E994F3FAF6D`；这是读取实际 XML 的复核，不是新增运行，不等于整个数据审计已闭合。`settlement_outbox.h` 的旧轮数/9.5 分钟注释仍需和当前 6 小时合同对齐，不能据该注释恢复旧策略。

依赖协议和 S1 的通过仍不代表 13 外测、交易业务、33/36 角色运行或真实双区故障通过。整体状态保持未完成。
