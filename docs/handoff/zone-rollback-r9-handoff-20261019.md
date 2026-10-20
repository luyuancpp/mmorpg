# 交接：整 zone 回档清理工具（R9）与两个小项的落码（2026-10-19）

> **接手第一步**：读本文 §0–§3，先请负责人回答 §3 的规模问题，再按 §4 落码。
> **上游**：存盘管线总交接 [godb-pipeline-impl-handoff-20261015.md](godb-pipeline-impl-handoff-20261015.md) §7（R1–R8 已完成项、给 Codex 的验证顺序见 §7.6）；设计 [zone_data_rollback_discard_tool.md](../design/zone_data_rollback_discard_tool.md)（v3，**唯一依据**）；回档语义与脚本侧落码见 [zone_data_rollback.md](../design/zone_data_rollback.md)「2026-10-18 修订」。
> **为什么交接**：负责人换账号继续（2026-10-19）。本会话（「go/db pipeline 交接文档」）只做到设计 v3，**R9 一行代码都还没写**。

---

## 0. 给新账号的开场提示词（可直接复制）

```
你接手整 zone 回档清理工具 R9 与两个小项（死信去重、探针剥离与 op=other 告警）的落码。
先走 AGENTS.md 会话启动门禁，然后读 docs/handoff/zone-rollback-r9-handoff-20261019.md，
再读 docs/design/zone_data_rollback_discard_tool.md（设计 v3，唯一依据）。
第一步：按交接 §3 请负责人确认规模（只做 B4+B5 共 23 个文件，还是 109 个全做），确认后按交接 §4 的批次顺序落码。
约束：你只写代码，不编译、不测试、不跑协议生成或冒烟（交 Codex），每批给出 Codex 验证命令；
不 commit / push；同一工作区有别的会话在写，改文件前先 git diff、只改自己的段落；
docs/PROGRESS.md 只追加。全部用中文，运行期日志与错误文本用英文。
```

---

## 1. 背景与授权（负责人原话）

- **2026-10-17**：K8s 整 zone 回档语义 =「丢弃 TargetTime 之后的写」（选项原话「丢弃 T 之后的写 (推荐)」）。脚本侧已按此改写，见 `tools/scripts/k8s_zone_rollback.ps1`，其文件头的「zone_rollback_discard 契约」一节由 B3 同批改写。
- **2026-10-18**：「你就按最标准的去做就行，数据一致性而且可水平扩展」。回档工具的 7 项待定和三个小项由接手会话按标准做法拍板，决定见设计 §1。
- **2026-10-18**：项目**尚未上线**（AGENTS.md 会话启动门禁、`docs/design/db-write-fencing-mysql.md` 文末「上线状态」）。首次上线按「go/db dual + scene / login 严格提交屏障」整批起步；混跑、降级、灰度不阻塞上线；**回档 `-Apply` 在上线前不可用，也不算阻塞项**。
- **2026-10-19**：「同意按 73 个文件、9 批的规模开工 R9：先并入 28 条复核意见并补齐缺的两路调研，再按 B1→B6 分批落码，每批交 Codex 验证。」
- **2026-10-18（长期）**：「你只实现代码，协议生成 编译冒烟交给 codex」。

---

## 2. 现状

| 项 | 状态 |
|---|---|
| R1–R8（④ + WAITAOF、③ 消费组、S4 / S5、部署与告警、回档脚本改写等） | 代码与文档已落完，基本已随负责人快照入库（截至 `c63b22dca`）。**全部未编译、未测试**；验证顺序见上游总交接 §7.6 |
| R9 设计 | **v3 已定稿**，未落码。已补维护围栏与三个小项两路代码核实；已逐条处置两面对抗复核的 28 条（设计 §6.1、§6.2，原文在附录 A）；已按上线前范围收窄（§6.3，删掉的内容与补做触发条件见 §8）；复查轮 11 条已处理（§6.4） |
| R9 代码 | **0 行**。`go/db/cmd/zone_rollback_discard` 不存在。现在 `k8s_zone_rollback.ps1 -Apply` 停在入口校验，dry-run 能跑完 |

设计 v3 的要点（详见设计 §1、§2）：
- 维护围栏复用合服广播围栏 `merge:in_progress:<zone>`，值为 `zone_rollback_discard:<rid>`，永久键；工具新增 fence / release 两个模式。他区在线的本 zone 玩家等其下线、不踢，有界等待。
- 回档对处理范围内的玩家做纪元抬升：owner_epoch 与两表的 applied_epoch 抬到 owner_stop+1，在同一段 Lua 里先抬后删。
- 只支持 strict 协议（严格屏障按 PITR 之后的 SQL 逐人重建）；legacy 一律拒绝，上线后再补。
- 放弃 unchanged 优化，全部冷读。T 之后合入、迁入本 zone 的玩家，证明不了早于 T 就一律拒绝回档。
- 可水平扩展：每个玩家相互独立、可分片并发；目标值来自不可变的停服基线，可断点续跑；SCAN 限速、有预算。

---

## 3. 先请负责人定（接手第一件事）

1. **规模**：设计 v3 共 **109 个文件**（新增 58、修改 51；另有 Java 仓 `PARITY.md` 1 个），比 10-19 授权的 73 多 36。按 AGENTS §10.2，开工前须负责人确认。本会话给出的两个选项（10-19 已问，负责人尚未回答就换了账号）：
   - **只做 B4 + B5（共 25 个文件，推荐先做）**：
     - B4 死信去重（15 个）：WAITAOF 一直确认不了时，死信会一轮多一份字节相同的副本，这是线上真实会出现的放大；
     - B5 探针剥离与 op="other" 告警（8 个）；
     - 另加 B1 中 B4 依赖的 `go/db/internal/kafka/dead_dedup.go` 与 `dead_dedup_test.go`（2 个）。

     这几项上线前就该修。其余 84 个文件属于回档工具本体（B1 余下部分、B2a–B2d、B3、B6），按「上线状态」一节不阻塞上线，可以留到上线前单独排期。（本会话 10-19 问负责人时说的是「23 个」，漏算了这 2 个依赖文件，以本处为准。）
   - **109 个全做**：按 B1 → B6 分批落码。
2. **持久副本目录**（设计 §7 第 2 条，只在全做时需要）：工具要求 `-ArchiveDir`（ps1 为 `-ArchiveDir`，`-Apply` 必填），指向操作者保证持久、可从其它机器访问的目录。代码里它只是一个必填参数，不阻塞落码；放在哪是运维部署问题。

---

## 4. 落码顺序

批次、依赖、逐文件清单、每批末尾「交 Codex 的验证」都在设计 §3；测试用例按批列在 §4。

- **只做 B4 + B5**：两批互不依赖。设计 §3 写的是「B4 依赖 B1」，但实际只依赖 B1 里的 `dead_dedup.go` / `dead_dedup_test.go`（`DeadDedupField`、`RetryDeadIndexKey` 两个纯函数；B1 其余文件是给回档工具用的 scenenode、guildcheck 等）。所以只做小项时，把这两个文件放进 B4 一起落即可，不需要整批 B1。这一点是本会话的判断，设计原文没写，落码时先核对一遍。
- **全做**：B1 → B2a → B2b → B2c → B2d。B3 依赖 B2a 的 CLI 契约，可以和 B2b–B2d 并行；B2d 依赖 B2c 和 B3。B4、B5 独立。B6 最后做。
- **两道防 fail-open 的闸**（复查 [1]）：
  - B2a 起，工具 CLI 带一道接线闸：除 release、quarantine-drop、`-version` 外，一律以退出码 3 退出；
  - B3 保留 ps1 入口对 strict 的拒绝。

  这两道闸都只能由 B2d 删除，并各自补测试。不能先删闸、后补实现。
- 每批落完：给出该批的 Codex 验证命令（取设计 §3 该批末尾）；在 `docs/PROGRESS.md` 追加一条；在本文 §6 登记该批的文件清单与偏离。

---

## 5. 协作约束

- **「D1 玩家归属与存储持久化」会话**：在落 D1 第 3 批「读方门禁」，范围是 go/db 的 `placement_route.go`（lookup / confirm / retryBudgetExempt）、`store_failure.go`（deferReason）、`key_ordered_consumer.go` 消费循环里的「就绪闸」、`config.go` 的 Placement 段、`metrics.go`、`etc/db.yaml` 的 Placement 段。
  - 本会话答应过：**R9 要导出 go/db/internal/kafka 的键名函数，或要动 `placement_route.go` 的 lookup 附近时，先发消息给它，等它回复再改**。名字用 ListAgents 查；它若已停，问负责人。
  - 本会话给过它三处接缝提醒：就绪闸要保留重新入组退避，并能被 pullStop / ctx 打断；新指标追加在已有之后，不动预建序列；deferReason 只加常量，不改判定顺序。
  - **2026-10-19 它已落完 D1 第 3 批**（未编译），清单见上游总交接 §7.7。R9 落码时要注意：
    - `placement_route.go` 的 lookup / confirm 现在共用一个读取入口，`Placement.RequireRouteReady=true` 时走 `placement.RouteReadyReadScript`；
    - `key_ordered_consumer.go` 的 ConsumeClaim 取消息前、重试循环分拣认领前，各有一道就绪闸；
    - ~~「闸能被 pullStop / ctx 打断」这一条它还没核对。~~ 2026-10-20 已由 go/db 交接会话静态核对：成立，R7 退避也原样保留。顺带修了「闸由开转关时空闲 claim 仍漏取一条」（只改 `route_ready_gate.go` 的 `gatedMessages`），补了 `route_ready_gate_shutdown_test.go`。都未编译，待 Codex 验证。见上游总交接 §7.7 与 PROGRESS 2026-10-20 同名条目。

    R9 若要导出键名或碰这些函数，先读它的最新代码。
- **有他人未提交改动的文件**（设计 §3「文件归属」，截至 `c63b22dca`）：
  - `tools/scripts/k8s_deploy.ps1` 及其契约测试（B3 的 C13，可单独顺延）；
  - B6 的 `db-retry-owner-routing.md`、`shared-redis-split-by-purpose.md`、`mixed-version-persist-field-protection.md`、`godb-pipeline-launch-todo.md`、`merge-zone-runbook.md`、`PROGRESS.md`。

  动手前先 `git status` / `git diff`，与持有方协调；对方没合入时只做本项最小增量。
- **「落地恢复与围栏」会话已停**，有三项接缝要它的接手方核对，见上游总交接 §7.8。R9 的纪元抬升和屏障重建直接依赖围栏 v0.4 的现状（`advanceApplied`、`save_barrier` 字段、`barrierrestore.MaintenanceKey`、`SchemaGenFloor`），落码时以代码为准。
- **不改**：另一会话的 `go/db/internal/barrierrestore` 与 `cmd/save_barrier_restore`。设计只复用它们已导出的符号，B1 再导出 `SchemaGenFloor`。

---

## 6. 批次登记（接手方逐批追加）

| 批次 | 状态 | 文件清单 | 偏离设计 | Codex 结果 |
|---|---|---|---|---|
| — | — | — | — | — |

---

## 7. 经验（本会话踩过的坑）

- **工作流额度**：并行的大工作流反复撞上会话额度或周额度，代理中途退出，会留下半截改动。按批次跑；每个代理的提示里都写「先 git diff 核对已有改动、在其基础上补完、不要重复添加」；续跑用 `resumeFromRunId`。
- **重启会清空 scratchpad**：中间结论要及时写进仓库文档，不要只放在临时目录。
- **只读代理会把文件 `git show` 重定向到 %TEMP%**：提示里明确禁止，结束后核对。
- **生成器**：`python tools/scripts/gen_docs_index.py` 会整段重写 `docs/README.md` 的索引区，改掉别的会话手写的条目。登记新文档请手工插一行，位置照生成器的排序。
- **定位**：设计里的行号以 `c63b22dca` 为准，代码在持续变化，落码时一律按函数名和锚点文字定位。
