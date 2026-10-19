# 交接：Java 网关按 fault 列把登录故障回 500

> 2026-10-19 更新。接手前先读根 `AGENTS.md` 的会话启动门禁；权威设计见 [tip-code-axis.md](../design/tip-code-axis.md) §2.10。本文记录已落地范围、实际验证证据和剩余事项，不代表整批验收完成。

## 1. 行为与范围

第三方登录的渠道故障、令牌签发失败、账号目录 Redis 失败等，原先虽然已回 login 的 in-band 故障 tip，Java 网关仍统一映射为 JSON `code:401`。导表器现在从 `Tip.xlsx` 的同一份 `fault` 列生成 Go `faults.go` 与纯 JDK 的 Java `TipFaults.java`，两版网关据此把故障 tip 回 `code:500`；业务拒绝、缺少 tip、当前网关不认识的 tip 仍回 401。HTTP 状态仍为 200。

message 的取法未改：没有参数文案时通常为 `upstream_err=<tip>`；有 `TipInfoMessage.parameters` 时沿用各版网关原有的解析或拼接结果。当前客户端只按 JSON `code` 分支，不按 message 精确匹配，也没有通用 tip 文案表；tip 保留用于排障。

第三方登录失败后的恢复流程重新拉起渠道：微信重新授权拿新 code，QQ 重新拉起渠道取得新凭据，不把旧 `auth_token` 原样重发作为恢复流程。微信 code 一次性，签发失败时已被消费，渠道故障时也不能假设尚未消费；此恢复策略不表示 QQ token 也是一次性的。口令登录可以用原凭证重试。

| 位置 | 已落地改动 |
|---|---|
| 导表器与 CI | `_generate_tip_faults` 同源生成 Go/Java 故障表；新模板 `tip_faults.java.j2`；Java `tip/` 另部署到 gateway，整棵树仍部署到 config_node；漂移检查及三份副本比较；新增 4 项导表器回归并调整 1 项夹具 |
| 本仓网关 | `LoginService.httpCodeForUpstreamTip` 与 `RefreshTokenService` 共用 fault 映射；故障告警不带凭证；DTO 注释与登录、刷新、端点集成测试 |
| 独立 Java 版 | `ContractSync` 受管同步 `game-table/.../TipFaults.java`，复制前核对码名/码值；gateway 依赖 game-table；`LoginHttpService` 登录与刷新共用映射及对应测试 |
| 文档 | 设计 §2.10、第三方登录设计、沙盒 runbook 排障表与 curl 期望、本交接与索引 |

故障分类只认表的 `fault` 列，不在服务内另维护故障码集合。

## 2. 补丁与入库状态

历史顺序为 `legacy-gate-killswitch-fix.patch` → `third-party-login-fix.patch` → 可选的 `killswitch-reject-before-auth.patch` → `tip-fault-gateway-500.patch`；500 补丁依赖第三方登录补丁对网关和设计/runbook 的修改。已有 check/apply 回执，不应在当前主仓重复整份应用。功能源码已随 `e611b5d8d` 入库；本轮只补缺失文档增量，保留之后的微信 10 分钟、K8s 防误关和客户端核对订正。

共享笔记目录为：

```text
C:\Users\luyua\AppData\Local\Packages\Claude_pzs8sxrjxfjjc\LocalCache\Roaming\Claude\side-session-notes\local_60048af7-2ce6-4202-88a6-233b6df8a795\
```

本轮只读封存核对的最新 `tip-fault-gateway-500.patch` 实际有 **18 个文件**，SHA256 为 `9C6749A20CD49C5B5F265AED1E55FBB229BFB81DED85711A02A602305181B16B`。共享笔记曾有 16 文件及旧 SHA 的口径，此前 15 文件应用回执也记录的是更早版本；补丁身份以文件实际 SHA 为准，执行步骤仍须与当前回执核对。最新补丁的目标逐 hunk 重建后均与其 Git blob index 相符，运行逻辑已落地；缺失的重试、message、交接及索引按当前事实补齐，不能用旧补丁覆盖并行增量。

## 3. 已有证据与适用条件

仓库外笔记和旧设计中的「未导表、未编译、未测试」是原交付时的状态，以下实际回执已覆盖其中相应阶段；旧失败和挂起原件仍保留，不改判。

| 阶段 | 证据与结论 |
|---|---|
| 正式服务端导表 | 历史回执 `build/verification/proto-table-build-20261019T022113341Z/formal-tables-20261019T031954520604Z/receipt.json`：42 表、11 部署、0 失败，不部署客户端。新增码为 2035/2036 |
| 故障表与导表器 | `build/verification/third-party-login-20261019T030744247Z/tip-fault-consistency.json`：Go/Java 45 码一致，三份 Java 副本相同；`exporter-tests.log` 为 147 passed、原生退出 0。本轮三份 Java 副本 SHA 仍为 `ABAB951C51F8A9D34616A44F9CA159F65264A1C8AFD89A823FF08A19597FA63B`；其他导表输入须以原回执及本轮 `table-history-current-comparison.json` 为准，不据旧成功宣称当前全输入重新生成通过 |
| Go 旧行为红灯 | 本轮在私有 overlay 恢复旧行为，两例均精确命中预期断言：账号目录故障返回错误、签发失败缺少错误码。命令带 60 秒超时，原生退出 1 符合预期；不是旧轮人工终止的挂起结果 |
| Go 当前代码 | 本轮定向 7 项通过；`go vet ./...`、`go build ./...` 原生退出 0；全量 519 个顶层与 375 个子测试通过（合计 894 条通过事件），4 个顶层原有环境条件跳过，0 失败。来源清单首终无漂移，范围以各回执为准 |
| 主仓 Java gateway | 本轮 209 项：204 通过、5 项实库条件跳过，0 失败/错误；其中 LoginService 69 项、Refresh 10 项、Endpoint 8 项共 87 项全部通过。端点测试使用真实 Spring/MockMvc 与 mock gRPC，不是实网络冒烟。私有源码快照与原仓核对无漂移；不替代跳过场景或实栈验收 |
| 主仓 Java config_node | 本轮 367 份源码编译通过，无测试；这是生成表的编译证据 |
| 主仓示例 starter | `DevLoginProfileGateTest` 在 Surefire 3.5.4 加载阶段因缺少 `org/apache/maven/plugin/surefire/SurefireReportParameters` 失败，原生退出 1，尚无测试执行；来源首终无漂移。按 AGENTS.md §10.2 停止后续执行，等待处理决策 |
| 独立 Java 版 | 0.1.0-SNAPSHOT；历史 login 30 项、gateway 12 项通过，仅在关键子树 SHA 与原件匹配的条件下复用。不能称全 reactor 通过，也不能称本轮已重跑 |

本轮原件统一在 `build/verification/tip-fault-resume-20261019T075057587Z/`：`go-red.{json,log}`、`go-targeted.{json,log}`、`go-vet.{json,log}`、`go-build.{json,log}`、`go-all.{json,log}`、`java-main-gateway.{json,log}`、`java-main-config.{json,log}`、`java-main-starter.{json,log}`，以及隔离 Java 输出与源快照。日志、原生退出码和来源清单共同界定结论。

## 4. 剩余验收与接手约束

1. 保留 starter 首错原件，先取得其 Surefire 加载阻断的处理决策；不要绕过测试或把未执行记为成功。K8s 脚本及后续验证按实际回执补记，不能从上表推导已完成。
2. 独立 Java 版重新同步前先读其 `AGENTS.md`。当前 ContractSync 必须明确服务端契约源与独立地图源：保留 `--maps-source` 和 `--maps-commit`，并分别固定两份来源；地图源历史 commit 为 `fea7f5e9be55017597752bf7b4ec433bd3e12850`。当前 `SOURCE.properties` 仍记 dirty 快照基线 `8b9592de3`，该 Git 提交不含 `TipFaults.java`，Java CI 的 sparse-checkout 也缺 `generated/code/java/tip`；来源与 CI 闭环待修，历史 fullcheck 不证明 clean Git 可重建。不能复制旧笔记里仅传 `--mmorpg` 的单源命令，也不能把历史局部成功称为全 reactor 通过。当前关键 107 输入与历史 42 项用例的比较原件见本轮 `java-historical-equivalence.json`。
3. 未做真实微信/QQ 凭据联调、HTTP 实栈及最终冒烟；mock/单元测试不能替代这些证据。未做压测，不作性能结论。
4. 无客户端生成或修改授权时，导表与协议生成须继续使用服务端专用配置，禁止写同级客户端仓；不要照抄可能包含 C# 部署的 `dev.bat gen`。
5. 不重复运行输入未变化的成功阶段；需要续验时，先核对前置生成物、来源 SHA 和对应运行条件，再按受影响范围执行。失败时保留原文，按项目失败门禁报告，不改断言或连续重试。

## 5. 客户端与发布边界

负责人另开的客户端核对会话已只读确认（当时 HEAD `5e2d4561`）：客户端只按 JSON `code` 分支；HTTP 登录失败不清 token、不强制重新认证、不自动重试；HTTP `/api/refresh-token` 没有业务调用点，刷新走 gate TCP。因此这次 401→500 不破坏该客户端的现有分支。当前客户端尚未接微信/QQ；可选 `client-login-500-text.patch` 未打，仍需独立客户端任务授权。

网关只认识构建时的故障表。login 新增故障码而网关未重建会回旧行为 401；某码从 fault 改为非 fault 而网关未重建，旧网关仍回 500。改 login 会返回的 tip 的 `fault` 列须与两版网关同批发布，并通知客户端。Java 版当前状态和既有并行修改以其 `PARITY.md` 与实际工作区为准，不能覆盖或回退。

## 6. 当前导表器实跑与独立复核补记（2026-10-19）

- 当前执行 `python -m pytest -q`（另指定独立临时目录与 JUnit 输出，关闭 pytest 缓存写入），原生退出 0：**147 通过、0 失败、0 错误、0 跳过**。其中 `tests/` 为 133 项，根目录真实表冒烟为 14 项；两种收集范围的数量差异不表示漏测。
- 在独立目录复制码轴 state，调用当前 `generate_tip_enums` 重生 tip 产物，未运行 protoc 或部署。34 次逐字节比较全部一致：tip proto、state、文案、Go 生成与部署副本，以及 Java 生成源、本仓 config_node / gateway_node 和独立 Java 版的副本；Go / Java 共 45 个故障码的码值、组名与码名一致。
- 本轮已列 174 项导表器、模板、测试、源表 / schema 与 state 输入首终 SHA 无漂移；`tools/data_table_exporter/tip/` 不存在。原件为 `build/verification/tip-fault-gateway-500-current-20261019/` 下的 `result.json`、`pytest.xml`、`pytest.log`、`inputs-before.json`、`inputs-after.json` 与 `tip-comparisons.json`。这是当前 pytest 与纯 tip 生成证据，不替代完整全量导表重跑。
- 独立复核既有两版 Java 日志、XML、来源与产物，回执在 `build/verification/tip-fault-audit-20261019/receipt.json`。主仓网关的历史 clean package 确有 native 0，当前唯一来源差异严格确认为一句 Javadoc；config_node 新一轮 367 源编译 native 0、无测试，372 项来源 SHA 匹配；独立 Java login 本批四类 30 项与 LoginHttpServiceTest 12 项实际通过。各回执的条件跳过与全 reactor 首错保留，不称全版本全绿。
- 当前共享笔记 `tip-fault-gateway-500-codex-verify.md` 第 14 行已写 18 文件及 `9c6749a2…`，与 §2 的补丁实物一致；其中未验证状态及单源同步命令仍按本文订正。
- Java CI 的确切缺口在独立 Java 仓 `.github/workflows/ci.yml` 与 `contract-drift.yml`：主源 sparse-checkout 各缺一行 `generated/code/java/tip`，而 ContractSync 无条件读取该文件，地图源清单不涉及此文件。来源与 CI 闭环仍按 §4 待处理；本轮只记录，没有修改 Java CI、补依赖或重试先前失败阶段。
