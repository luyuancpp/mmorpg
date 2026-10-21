# 四职业技能接入:交 Codex 的生成 / 编译 / 测试清单(2026-10-19)

设计与规则见 [docs/design/class-skills.md](../design/class-skills.md);需求来源是客户端仓 `Docs/SKILL_LOGIC_HANDOFF.md`。
**本文所有命令 Claude 都没有执行**(AGENTS §10.1)。Claude 只跑过:沙盒导表(对仓库只读,拷回差异)与客户端仓的
离线 Roslyn 编译检查 / 离线 NUnit 跑具(客户端仓不受 §10.1 约束,结果见客户端交接文档 §8)。

## 0. 分支与合入

| 仓 | 分支 | 基线 | 提交 |
|---|---|---|---|
| 服务端 | `feat/class-skills`(工作树 `E:/work/xuanming-server-mmo-wt-skills`) | origin/main `c63b22dca2` | `9c5bac0540` 配表与协议源 → `1e2a6bbc6d` 导表产物 → `6d7fbe325c` 逻辑与测试 → `e86b8e0c76` 测试补 include → 本文档 |
| 客户端 | `feat/class-skills`(工作树 `E:/work/mmorpg-client-wt-skills`,稀疏检出) | 客户端 main `d6338af1` | 见客户端交接文档 §8 |

- 服务端工作树没有初始化子模块(`third_party/*` 为空)、没有 `lib/`,**不能在工作树里编译**。在主目录 `E:/work/xuanming-server-mmo` 编译:
  主目录 `main` 当前比 origin/main 落后 19 个提交(另有 5 个本地提交),先由负责人把 origin/main 合进本地 main,再
  `git merge --no-ff feat/class-skills`。本分支只在 origin/main 之上追加,预期无冲突;若 `data/tip/Tip.xlsx` 冲突(二进制),
  取 main 的版本,再跑 §1 的 apply 脚本重新追加 7 个码(幂等),然后重新导表。
- 另一个老分支 `feat/class-skills-localmain`(基于本地 main `ed75b64c88`,消息号用的是旧编号 276)**作废,不要合**;它只是重排基线前的草稿。

## 1. 配表(已在分支里,只有合入冲突时才需要重做)

导表产物已提交(manifest v30,`source_rev = 9c5bac0540`,`data_dirty = false`)。重做方法:

```powershell
# 在主目录;apply 脚本是幂等的(已存在的行 / 列 / 码不重复写)
py C:\Users\luyua\AppData\Roaming\Claude\side-session-notes\local_3986d9cd-23d1-496f-b77e-63f0b42cd9d6\apply_class_skill_tables.py . E:\work\image\designs\class-skills-20260928\catalog.json
# 改了 xlsx 先单独提交,再导表(source_rev 才指向真实提交)
dev.bat gen   # 会写客户端仓 Assets/Scripts/Table/Generated;只想动服务端用 sandbox_export.py --compare 再拷回差异
```

期望:与仓库产物的差异只有 Skill / Buff / Class / Tip 相关的 25 个 `generated/` 文件 + 14 个部署文件
(`cpp/generated/table/...` 5、`go/shared/generated/...` 4、`java/config_node/...` 5)。

## 2. 协议生成(必须做:分支里只有 proto 源,没有生成物)

origin/main 的生成器源码 10-19 有改动,仓内 `proto-gen.exe` 是 10-17 的,`dev_tools` 会拒绝旧二进制,所以先编生成器:

```powershell
cd E:\work\xuanming-server-mmo
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -UseBinary -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml
```

生成后逐项核对(任何一项不对都停下来):
1. `git diff proto/message_id.txt` 为空(317 已登记;生成器若发了别的号说明 main 里有人新加了 RPC,要重新同步客户端 MessageIds)。
2. `cpp/generated/rpc/service_metadata/rpc_event_registry.h` 的 `kMaxRpcMethodCount = 318`。
3. `cpp/nodes/scene/handler/rpc/player/player_skill_handler.h` 出现 `UpgradeSkill` 声明与 `case 4` 分派;
   `player_skill_handler.cpp` 里 Claude 写好的 `UpgradeSkill` 守护段内容原样保留(`PlayerSkillSystem::UpgradeSkill(player, *request, *response);`)。
4. 新文件 `robot/logic/handler/scene_skill_client_player_upgrade_skill.go`,`message_body_handler.go` 多一行 UpgradeSkill。
5. `scene_node_service.cpp` 的 Agones `AcquireCreatePermitBlocking` 块没有被吃掉(git diff 不应出现该文件)。
6. 只应改动:`cpp/generated/proto/{battle/battle_data,common/component/player_skill_comp,scene/player_skill}.pb.*`、
   `cpp/generated/rpc/service_metadata/{player_skill_service_metadata.h,rpc_event_registry.*}`、`generated/proto/{_unified,db,login}/...` 对应副本、
   `go/proto/{battle/battle_data,common/component/player_skill_comp,scene/player_skill,scene/player_skill_grpc}.pb.go`、
   各服务 `generated/pb/game/message_id.go`、robot 的 message_id 与 handler。出现别的文件 = 生成器或 main 有别的未生成改动,先查清楚。
7. robot vendor 同步(robot 默认 `-mod=vendor`):
   ```powershell
   foreach ($f in 'scene/player_skill.pb.go','scene/player_skill_grpc.pb.go','battle/battle_data.pb.go','common/component/player_skill_comp.pb.go') {
     Copy-Item go/proto/$f robot/vendor/proto/$f -Force
   }
   ```
   之后 `fc /b go\proto\scene\player_skill.pb.go robot\vendor\proto\scene\player_skill.pb.go` 等逐个比对为相同。

## 3. 编译(串行 /m:1)

```powershell
cd E:\work\xuanming-server-mmo
& "D:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe" game.sln /m:1 /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

- 通过标准:0 错误。battle / scene 库是 `/WX`(警告即错误),出现 C4xxx 也算失败,保留完整输出。
- 涉及:`battle.lib`(引擎 + `class_skill_rules.h`)、`scene.lib`(player_skill / player_attribute / player_battle / loader)、scene 节点(handler)、battle 节点。
- Go:`pwsh -File tools/scripts/go_services.ps1 -Command build`(go/proto 的 scene / battle 包变了,match / guild / friend / scene_manager / shared 都引用);
  `cd robot; go build ./...; go vet ./...`(robot 改了 ListSkills 处理,读 `response.Catalog`)。
- Java:`cd java; .\mvnw -q -pl config_node -am compile`(导表器更新了 `com.game.table.SkillTable*` 与 `SkillErrorTip`)。

失败时保留:MSBuild 第一处 error 前后 30 行;`go build` 完整输出;不要连续自动重试。

## 4. 测试

```powershell
cd E:\work\xuanming-server-mmo
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter turn_battle_engine_test -LogDir build/verification/class-skills
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter skill_test -LogDir build/verification/class-skills
pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter bag_test -LogDir build/verification/class-skills
cd go/match; go test ./...; cd ../..
cd robot; go test ./logic/...; cd ..
```

通过标准与必看项:
- `turn_battle_engine_test`:新增 `ClassSkillRulesTest.*`(5)、`ClassSkillEngineTest.*`(17)、`TableBattleDataProviderTest.ClassSkillCatalogMatchesContentBaseline`(1)全部通过;
  既有用例(含同种子回放、装备连击 / 反击 / 反震、`PoisonBuffTicksEachRoundAndExpiresAfterTwoRounds`)不得回退。
  **核对实际执行的用例数**:gtest filter 没匹配到也会返回 0,汇总表里新用例必须出现。
- `skill_test`:新增 `PlayerSkillBookTest.*`(13)与既有 `PlayerSkillSystemTest.*` 通过。本工程这次补链了 `battle.lib`、`rdkafka.lib`(player_skill 现在依赖 player_battle / player_attribute),链接报错先看这两项。
- `bag_test`:`PlayerFeaturePersistenceTest.NewPlayerInitializesDefaultSkillsAndMissionScopeWithoutPlayerEventSinks`
  (首个有效职业种子现在是 1101)与其余存档 / 结算用例通过。
- 退出码非 0 但没有 `[FAILED]` 行 = 中途崩溃,不能算通过。

## 5. 联机冒烟(编译测试都过之后)

1. 照 `docs/ops` 的本机开机 runbook 起栈(先 `cmd/migrate up`),`robot login-test` 应仍为 23/23(技能施法用例现在用 1101)。
2. 客户端合入 `feat/class-skills` 后进游戏:角色属性窗口「技能」→ 技能书显示当前职业 8 个技能、初始 2 个已学 1 重、余额、上限 → 修习 1 次 / 10 次 →
   进 PVE 战斗,技能面板出现已学主动技能 → 施放(友方治疗点队友 / 自己,自身技能点自己)→ 出战斗 → 重登复查等级与余额。
3. 期望日志:`[PlayerSkill] Skill upgraded: ... rank=a -> b cost_gold=N`;老号首次登录有 `[PlayerSkill] Initial skill granted`。

## 6. 交接文档 §7 最低测试清单对照

| # | 项 | 状态 |
|---|---|---|
| 1 | 目录 4×8、ID / 职业 / 解锁 / 图标键一致;无重复字段号 / 消息号;hit_kind 未丢;指纹不一致拒绝开局 | **已写用例未跑**(真表契约 + 字段号人工核对:21 / 28 / 15 / 6 / 7 均未占用;指纹整表序列化,新列自动纳入,沿用既有指纹用例) |
| 2 | 初始学习、学习 / 单次 / 10 次 / 部分成功 / 满级 / 等级不足 / 金币不足 / 错职业 / 非法 count / 重复请求 / 旧 expected_rank;乱序回包 / 断线 / 切宠物 | 服务端:**已写用例未跑**(player_skill_book_test);客户端:**已实跑**(SkillBookClientTests 18 条,离线 NUnit);宠物主人校验 **未做**(无宠物技能协议) |
| 3 | 属性即时更新、重算 / 重登不叠加、旧档兼容、战斗 / 传送 / 交接窗口拒写、扣费与等级不丢一边 | 逻辑已落;窗口拒写与不扣费 **已写用例未跑**;被动即时重算 / 不叠加 **未写专门用例**(需属性组件全套,建议联机冒烟看面板) |
| 4 | 32 技能逐项效果、最强叠加、驱散、冷却、被动不可施放、scene / battle 不重复加成 | 引擎按类别 **已写用例未跑**(用形状同真表的内存行,非 32 行逐一);逐技能数值核对 **未做** |
| 5 | 截图新机制、宠物技能真实行动路径 | **未做**(不在本批) |
| 6 | ManaDelta 全链、群体只扣一次、回放 / 观战 / 重连一致 | 服务端 MANA / ITEM 带 mana_delta、群体一次扣蓝 **已写用例未跑**;客户端 TurnPlan 消费 **已实跑**(ManaDeltaPlanTests 5 条 + 既有 43 条回归);观战 / 重连 **未测** |
| 7 | 两端联网:开技能页 → 刷新 → 学习升级 → 战斗施放 → 出战斗 → 重登 | **未做**(需 §2~§4 先过) |
