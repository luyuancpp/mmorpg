# 交接:D1 归属与落点映射持久化——剩余收尾(2026-10-19)

> **给新会话**:先按 AGENTS.md「会话启动门禁」读完规定材料,再读本文。本文只列**还没做的事**和做法;已落码的内容、设计结论与验证命令不在这里重复。
> **状态**:D1 第 0 ~ 3 批代码已全部落码,**全部未编译、未测试**(AGENTS.md §10.1,待 Codex)。原会话因周额度用尽停在这里。
> **2026-10-19 补(另一会话只读核查后追加)**:本文有几处要更正、还有漏项,见 [placement-line-handoff-20261019.md](placement-line-handoff-20261019.md) §3。冲突时以那份为准,它也是整条落点线的总入口。

## 0. 必读(按顺序)

1. 提案 [player-route-mapping-durability-proposal.md](../design/player-route-mapping-durability-proposal.md) **§13**(设计修订 M1–M5、各批落码、上线与回退、残余风险)与 §11.2 进度表。
2. 落码规格 [spec_D1_batch1b2_2_3.md](launch-readiness-20261013/spec_D1_batch1b2_2_3.md)(§13「评审吸收」覆盖正文)。
3. [PROGRESS.md](../PROGRESS.md) 2026-10-19 的三条 D1 条目,最后一条「D1 剩余批次落码完成」里有**全部改动文件**、**未做项**与**交 Codex 的验证顺序**。

## 1. 工作区注意

- 同一工作区常有十多个会话并行;很多 D1 文件已被本机自动保存提交 `1fc103dce` / `8767c5027` / `e611b5d8d` / `53361d055` 收进(不是 D1 会话提交的)。动手前 `git status` + `git diff` 分清现状,Edit 精确插入,不回退别人的行。
- `tools/scripts/k8s_zone_rollback.ps1` 另一项改动(R9,已换新账号接手)在重写,**不要碰**;R9 要动 go/db `placement_route.go` 的 lookup 附近时会先发消息。
- `tools/scripts/k8s_deploy.ps1`、`dev_tools.ps1`、契约测试有他人大量在途改动。
- Java:只登记待做(PARITY D1 行已改为「Java 待做(部分不适用)」),本交接不含 Java 落码。

## 2. 剩余工作(按顺序)

### 2.1 最终语义评审 + 修复(最要紧)

D1 全部落码后**没有做过整体语义评审**。建议两个只读评审代理并行,再按 must / should 修复。评审只看语义与运行期(编译由 Codex 跑),提示里必须写「只读;按提交读文件用 `git show` 直接输出,不重定向成文件」。

- **评审 A(data_service / go/db / guild)**:`go/shared/placement/route_ready.go`;`go/data_service/internal/routereconcile/`、`store/route_reconcile.go`、`store/route_reads.go`、`routing/route_read.go` 与 `router.go` 读路径、config 的 `ReadRequiresReady` 与 `Reconcile` 段、`svc/servicecontext.go`、`data_service.go` 启停、`routetool/{rewarm,freeze,auditor}.go`;`go/db/internal/kafka/{route_ready_gate,placement_route}.go` 与 `key_ordered_consumer.go` 的闸;`go/guild/internal/logic/merge_fence.go`。重点:每条读路径都用同一段 Lua 判身份、不可信时绝不把缺键当缺席;**go/db 就绪闸关闭期间不取消息、不推重试队列,且能被 pullStop / ctx 打断、保留重新入组退避**(go/db 管线会话的提醒,Codex 会重点跑 `consumer_shutdown_test.go`);leader 每拍顺序符合规格 §13.1(标记不在一律全量预热、repair 只在身份不符、令牌 fencing、水位键回档判据、Arm=false 撤标记、围栏镜像双向对账与 foreign_fence、armingBlocked、cvm 语义);租约谓词覆盖所有库写;关停有界;开关全为缺省时行为与改动前一致。
- **评审 B(merge_zone / 部署)**:`tools/merge_zone/` 的 route_store.go、route_publish.go、fence.go、merged_into.go、merge_run.go、placement_ops.go、pin_placement.go、unmerge.go、backfill_home_zone.go、relocate_run.go(`sqlRelocatePlacements`)、storage_audit.go、audit_checks.go、main.go;`k8s_deploy.ps1` 的 D1 转发与粘滞核对、`k8s_route_tool.ps1`、两份清库脚本的 leader 防护。重点:每个写入点「事务内复核围栏 / 版本 / 状态 → 条件更新 → 提交 → 同连接发布 + WAITAOF → 确认后才推进依赖步骤」;批事务锁 63 号片且先确保行存在;续期失权后所有写步骤都停;relocate read 的 catchUp 关掉「R1 已提交未发布 → 续跑认领 → 进 R2」;发布失败不伪装成功;`-route-source` 必填与成对核对;W2 / W3 仍停用;redis 源路径与改动前一致;k8s_deploy 缺省 redis / false、粘滞核对不误拒不漏拒。另核对两个代理在 merge_zone 里留下的少量重复(relocate 代理的 `routeReadSide` / `openRelocateRouteBackend` 与写入点代理的 `routeDBOps` / `openRunRouteBackend`),能合并就合并。

### 2.2 四张路由表的 proto 列漂移守卫(第 1a 批偏离第 1 条)

- 在 `proto/common/database/rollback_database_table.proto` 追加 `player_route`、`zone_merge_fence`、`zone_merged_into`、`route_meta`、`route_reconcile` 五个 message,字段号沿用 DDL 列注释里预留的 `pb:N`(见 `go/data_service/internal/store/schema.go` 的 bootstrap DDL);类型按 AGENTS.md §4(GUID / 时间戳 uint64)。
- 加进 `schema.go` 的 `TableMessages()`,只用作列漂移守卫;删掉 `routeTableShapes` / `assertRouteTableShapes`(或改为委托 proto 守卫);`store.RouteRow` 要么改用生成类型,要么写明它是领域类型并给出转换(AGENTS.md §3:不许有与 proto 重复的并行 struct)。
- 交 Codex:`pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`(生成器源码没变就不必 proto-gen-build);这份 proto 也编进 C++(`cpp/generated/proto/CMakeLists.txt`),要重编 C++ proto 库,MSBuild 串行 `/m:1`;再跑 data_service 的 store 测试与迁移真库用例。

### 2.3 零碎收尾

1. `tools/scripts/dev_tools.ps1` 转发 merge_zone 新参数 `-take-over-fence`(新开关名照既有 `-MergeZone*` 风格),契约测试同步;`docs/ops/merge-zone-runbook.md` §15 补它的用法(DB 围栏下「DEL 键重跑」不再成立,改用它在一个事务里接手中止运行的围栏)。
2. relocate 清单(`tools/merge_zone/relocate_manifest.go`)记录 route source,续跑时与本次参数不一致就拒绝。
3. `tools/merge_zone/relocate.go` 的 Fenced 文案:db 源下实际是库围栏,措辞改为两种源都对(只改文案,引擎与状态机不动)。
4. k8s 包装入口(`k8s_image` 等;`k8s_zone_rollback.ps1` 等 R9 重写落定后再做)转发 k8s_deploy 的五个新参数 `-RouteStoreMode` / `-RouteReconcileArm` / `-RouteReadRequiresReady` / `-DbPlacementRequireRouteReady` / `-GuildRequireRouteReady`(以及 `-ConfirmRouteSwitchChange`);否则集群切 db_first 后经这些入口部署会被粘滞核对拒绝。
5. 新 PowerShell 测试(`tools/scripts/tests/k8s_route_tool.tests.ps1` 等)接进 `.github/workflows/deploy-config-tests.yml`。
6. 每项做完在 PROGRESS 追加一条(只追加),提案 §13.5 对应条目标注已做。

## 3. 验证(交 Codex)

全部命令与顺序在 PROGRESS「2026-10-19 D1 剩余批次落码完成」条目的「交 Codex 的验证」第 0 ~ 9 步;2.2 做完后在第 2 步之前插入 proto 生成与 C++ proto 库重编。要点:基线(复验清单与 §11.2 各段)先行;真库 / 真 Redis 用例设 `*_IT_REQUIRE=1`,逐条报 PASS / FAIL / SKIP;MySQL 通过不能代替 TiDB;`redis.yaml` 改了 Pod 模板,下次 infra-up 要带 `-ConfirmRedisCoreRestart`(维护窗口)。

## 4. 给新账号的开场指令(可直接复制)

```text
接手 D1(玩家归属与落点映射持久化)剩余收尾。仓库 D:\luyuan\xuanming-server-mmo-cpp。
先按 AGENTS.md 启动门禁读完规定材料,再读 docs/handoff/handoff-d1-remaining-20261019.md,按其 §2 顺序做:
2.1 最终语义评审(两个只读评审代理并行)并修复 must / should;2.2 四张路由表的 proto 列漂移守卫;2.3 零碎收尾。
只写代码、测试与文档;协议生成、编译、测试、冒烟交给 Codex(写清命令)。Java 只登记待做。
工作区有多会话并行,动手前分清现状,不碰 tools/scripts/k8s_zone_rollback.ps1。每项做完追加 PROGRESS。
```
