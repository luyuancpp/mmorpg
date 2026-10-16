# Codex 在途只读核对回执（2026-10-15）

> **后续状态更新**：负责人随后明确要求直接修复代码、暂不编译，原“只读等待”约定已被这一新指令替代。第 1 批三类初始化与错误传播的修正已写入代码,最新状态以 [正式实施回执](batch1-codex-receipt.md) 为准。第 1 批核验单与第 2–5 批正式任务单现已落盘;下文“尚待落盘”仅保留当时记录。前面的只读采样与一次历史构建失败不代表本轮没有修改代码。

## 执行边界与状态

负责人转达：第 1 批还有 4 个代理收尾，Codex 只读，等待收尾通知及正式 `batch1-verify.md`；第 2–5 批任务单由设计与挑战审查工作流落盘。批次编号以 [README.md](README.md) 为准：单例、背包、外部 ID、生命周期与类型。Codex 未修改业务代码或测试，也未覆盖本目录总索引及正式任务单。

本回执汇集三个独立只读子代理的证据，供任务单作者补漏。采样基线为 `6281e4d2756def41150dfb439e9c0555912f9883` 加共享工作区；读取期间文件仍在变化。**本轮未完成第 1 批验收，第 2–5 批未开始实施。** 不以未收到消息推定文件已释放。

本轮规则文档改动仅收口摘要入口：根 `AGENTS.md` §7、`cpp/AGENTS.md` 与 `.github/copilot-instructions.md` 指向现有权威 [组件访问规则](../../design/ecs-component-access-rules.md)，保留他人的其他改动。三类规则本体已由原会话写入，本轮没有重写它：持久数据在加载边界创建且缺失拒绝；按需状态在唯一写入口首次写时创建且缺失读为空；派生运行时数据在创建流程计算且缺失记错、跳过。

## 第 1 批复核输入

- [运行时采样稿](batch-1-runtime-review.md)：按实际 diff 划分六个复核切片，含 HEAD 行号、创建入口、错误分类、测试改写建议。这六切片不等于原工作流六组。
- [存盘采样稿](storage-patch-notes.md)：含存盘拒绝的传播、异步退出回调、跨区交接、测试夹具、生成模板及完整调用点。所有新接口均为建议，未实施。

收尾后优先复查：

| 项目 | 核对时观察 | 最终验收要求 |
|---|---|---|
| 技能冷却与上下文 | 初次读取仍在 actor 初始化预挂，查询缺失报错，写入口只取不建 | 空冷却表允许首次施法；冷却与 caster/target 上下文由唯一写入口创建；拒绝路径不创建 |
| Buff 列表 | 初次读取发现写入口拒绝缺列表且生产无创建点；**后续已看到收尾代理恢复首次插入处延迟创建** | 通过真实首次加 Buff 用例及纯驱散、免疫、叠层、移除用例；检查测试是否仍固化旧口径 |
| 行为状态、AOI 格子与兴趣表 | 初次读取仍有提前预挂或写入口只取不建 | 空状态与无效实体分开；首次真实写入创建；只读查询不创建 |
| 属性重算与脏标记 | 派生组件缺失日志加跳过；脏标记写入创建、消费摘除，静态方向正确 | 经过真实 actor 初始化链验证，覆盖冻结、无 Buff、重复标脏 |
| 保存结果 | `SavePlayerToRedisImpl` 缺持久组件返回 false；调用者仍可能把 false 当已落盘 | 必须区分拒绝与相等证据；退出保留未收敛实体，交接不发 handoff 标记；覆盖重踢、异步重存、旧 epoch 拒绝与回滚入口 |
| 异步落盘比较 | `HandlePlayerAsyncSaved` 仍有忽略 `MarshalPlayerForSave` bool 返回值的入口 | 序列化失败不比较半成品、不宣称收敛、不据此销毁 |
| 测试夹具 | 退出夹具只挂 Guid/session，仍依赖旧 marshal 补建；部分测试裸调新 bool 接口 | 成功夹具走完整加载；检查返回值；失败用例主动删一个必需组件，并验证未发生副作用 |

F091（scene_id 按业务 ID 查找）、C003（no_caster 叠层）、F093/C007（节点 registry 边界）已有工作区实现，需验收既有 diff，不能根据旧审计状态重复实施。上述是静态证据，不是运行结论。

## 第 2–5 批任务单的补漏输入

1. 第 2 批不能只改 `Node::GetNodeInfo`；要覆盖 `ServiceNodeList` 的初始化、分配、连接、发现、注册、删除及停机读取。`registry.clear()` 不清除 `registry.ctx()`；若使用 ctx，销毁时需显式处理。工作区已有 `MuduoGrpcChannelFactory` 使用 ctx，旧文档“全仓没有 ctx”已过时。HEAD 重点：`node.cpp:399/473/1449/1568`、`node.h:127`、`node_allocator.cpp:40/175`、`node_connector.cpp:183`、`service_discovery_manager.cpp:88`、`registration_manager.cpp:90`。
2. 第 3 批从 ItemStore 换容器时，需同时核对 `EntityOf`、`StackFill` 的计划句柄、`PlanStackIntoExistingStacks` / `ApplyStackFill`、`DrainStacks`、`MergePartialStacks`、快照重放及 `nextAcquireSeq_`。确定性顺序要由业务键明确；实例与布局仍按 AGENTS.md §11.6 分开。设计任务单尚未到达，本轮不自行选容器或改签名。
3. 第 4 批外部 actor ID 的出站入口不止 ActorCreate：HEAD `view.cpp:90`、`aoi.cpp:182/322`、`movement.cpp:163/210`、`skill.cpp:536/537/566/693`；入站解析集中在 `skill.cpp`，属性同步模板 `attribute_sync.cpp.tmpl:33–36` 又从 Guid 池取值。客户端可见 ID 与本地 SkillContext/DamageEvent 句柄须分别盘点，不能机械替换。未获得客户端仓任务授权，不读取或修改客户端。
4. 第 5 批遵循 README 的移交边界：CQ、生成 Send*、流响应缓冲等归 gRPC 迁移会话。本轮仅记录旧模板 `ok=false` 直接 return 的 completion/tag 清理缺口，不在旧实现上另做一套修复。

通用命令与兼容性两点需在任务单定稿时核对，本轮不改正在编写的 README：

- `proto-gen-run` 示例须带 `-ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`，防止默认配置写同级客户端仓。未执行生成。
- uint32 → uint64 同为 protobuf varint，仅说明编码类型相同；旧接收端仍可能截断高位。须给出混合版本窗口、部署顺序及测试，不能据此直接宣称滚动兼容。

## 实际构建证据

收到只读协调通知前，在仓库根执行过一次：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe' `
  cpp/libs/services/scene/scene.vcxproj /m:1 `
  /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
```

退出码 **1**。`rpc_status.h:69` 的 `HANDLER_FAILED` 与 `:71` 的 `REPLY_DROPPED` 未声明，伴随 C2065/C2131/C2051；源 proto 已有新枚举，所读生成头未含对应值，RPC 工作仍在途。完整日志：[scene-build.log](../../../build/ecs-refactor/scene-build.log)。

已按 AGENTS.md §10.2 停止后续构建和测试，未自动重试、未修改 RPC 或生成物。没有任何 C++ 单测、集成、压测或 Java 测试通过证据；静态检查提示 `no-raw-pointer-member` 因缺检查器 SKIP，也不计作通过。构建产物不能当作验收可用的完整 scene 库。

恢复顺序：正式收尾通知 → 固定最终文件版本与任务单 → 重读 diff 并逐项消除旧分类测试 → 等待共享 RPC/生成链可用 → 统一串行构建依赖与测试 → 填正式批次回执。没有收到释放确认时不启动这些依赖步骤。

当前没有附加 PR；本机 `gh` 不在 PATH，未登录远端，未核实开放 PR/Issue。这一项不能写成“没有开放项”。

## Java 与交付状态

本轮只改协作规范摘要与交接记录，不改行为，无对应 Java 功能版本号，未修改 Java `PARITY.md`。Java 不使用 entt，但技能冷却、同场景目标和 no_caster 等客户端可见行为仍应在正式实施时对账。只读采样发现 Java 回合制已有 no_caster 相关测试、实时 Buff 尚未移植，不能用前者代替后者的等价证据。

未 commit、未 push。正式 `batch1-verify.md` 与第 2–5 批任务单尚待原工作流落盘，本回执不代替它们。

## 后续代码修正（2026-10-15，负责人要求暂不编译）

负责人已明确要求“我要你按这个去修复代码”“先别管编译”。本轮在现有工作区上直接修复第 1 批，没有等待其他会话任务单，也没有运行生成、构建或测试。保留原工作区其他会话改动，未修改客户端仓与传输改造。

### 已写入的修正

- 技能冷却、caster/target 技能上下文、行为状态、场景格子与兴趣列表收口各自的真实写入口，删除 actor/scene 创建时的空表预挂。缺失查询按空处理；失效句柄仍明确拒绝。派生 MoveSpeed/CombatStateFlags 的创建保持在 actor 创建边界。
- 存盘内部结果区分排队、已落盘相等、已发起交接与拒绝，公开 bool 接口保持与资产/结算注入点兼容。退出、重踢、异步落盘比较、旧 epoch 补存、交接及回滚采纳不再把拒存当成功；写标记/解冻前复核完整性。B5 回执的缺组件回归驱动真实 Redis 回复处理入口，不只测试分类纯函数。
- PlayerAllData 通过候选对象汇集子表，全部成功后 Swap；失败保留原输出与 presence。同步 `.h` 生成模板，所有实际 marshal 调用显式处理返回值。
- 战斗快照要求实际使用的持久/派生输入就绪；缺派生数据、相关宠物/背包或任务组件的结算在金币、HP/MP、任务与幂等账本副作用之前拒绝。属性/宠物列表 Get RPC 缺数据返回已有服务不可用码，不再成功返回空面板。
- 保留其他收尾代理已经完成的 Buff 首次真正插入时创建、纯驱散不建、帧末空列表清理与 no_caster 修正；补相关真实接口回归。

### 审查与验证边界

独立子代理已交叉静态核对存盘结果传播、候选输出、空间/Buff、战斗预检、RPC tip 转移、工程源注册及测试夹具。审查发现的“拒绝新技能前已打断旧施法”“延迟落点缺数据仍走伤害/Buff副作用”“结算缺派生仍入账”同类入口随本轮继续修正，不按初次修改即完成处理。

新增和改写用例覆盖首次写入、查询不建、冻结/失效实体拒绝、拒存保留实体及快照、回滚拒绝不采纳 epoch、缺组件结算零副作用、RPC 错误经生成转移后保留等；成功夹具补齐加载后的组件形状。**没有执行这些用例，未取得红/绿运行证据，不宣称编译、链接或测试通过。** 仅进行 diff、符号/调用点及工程 XML 静态检查。

Java `PARITY.md` 已追加本批对账：EnTT 创建机制不适用 Java 领域对象；Java 现有版本 `0.1.0-SNAPSHOT`，未新增实现。客户端可观察的冻结/技能效果及退出/交接错误语义仍待按 Java 实现核对，不能宣称双版本已对齐。第 2–5 批结构改造未在本轮实施；其正式任务单现已落盘,后续按总索引顺序处理。
