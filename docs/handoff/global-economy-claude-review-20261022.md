# 全球同服、交易与角色归属：Claude Code 审核入口

状态：**实现与验收仍在收尾，整体未通过最终验收**。本文件是负责人要求的精简审核入口；完整实施过程与失败原件见 [原交接](global-economy-review-20261021.md)。下面的历史通过结果只覆盖其记录的源码、依赖、环境与命令，不能替代后来变化的源码或重启后的数据库验收。

## 1. 最新版本与改动归属

2026-10-22（America/New_York）只读核对：

| 仓库 | HEAD | 保护范围 |
| --- | --- | --- |
| `D:/luyuan/xuanming-server-mmo-cpp` | `940ef9746776ec9fd1e0f51f09bc0e1409fcf38c` | 保留所有既有 dirty、生成器、存盘、路由与并行修复 |
| `D:/luyuan/xuanming-server-mmo` | `b0b7551afe3369b89c38caed6439b32d7a009264` | 独立 Java，按本仓 AGENTS 与 PARITY 核对 |
| `D:/luyuan/mmorpg-client` | `17cead02555adc7e75397cb9a32e5525f079fbfc` | 使用最新 CurrentMaps；不恢复旧天墉城，不覆盖新地图、资源、prefab 或 meta |

三仓均不是本任务独占工作区。**不要把整仓 `git diff` 归为本批，也不要用旧提交文件覆盖当前文件。** 原交接 §3、§8–60 与分批输入快照用于定位本任务接缝；输入快照是验证输入清单，不自动等于本任务作者清单。尚无逐行完整归属清单，不能声称所有 dirty 均由本任务产生。本任务没有 commit、push 或 tag。

## 2. 已实现的主要接缝与审核重点

| 功能 | 入口 | 本批行为与需审核的不变量 |
| --- | --- | --- |
| 原会话身份 | `go/shared/loginsession/reader.go`、`go/shared/roleidentity/context.go`；Java SessionContext / AuthorityJdbc | 账号、角色、归属修订来自原请求；缺原件拒绝，不能查当前目录把旧请求补成新身份 |
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

这些结果均为文件读取与 SHA 比对，**不是编译或业务运行**。

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
- [ ] 六个 Java 验收工具正式 javac；独立 Java 16 个非战斗服务各双副本，加 1 个 Battle，共 33 个进程的实际就绪与业务链。
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
