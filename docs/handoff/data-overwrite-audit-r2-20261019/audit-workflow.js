export const meta = {
  name: 'audit-r2-data-overwrite',
  description: '第二轮只读审计：数据问题与覆盖回档（分波查找、low 复核、换视角查找、对抗复核）',
  phases: [
    { title: 'Wave2', detail: '新落码增量评审：字段保护、战斗结算修复、神秘商人、路由自动对账、合服、login' },
    { title: 'Wave3', detail: '补覆盖与其余增量：资产杂项、lifecycle 未读段、写入口与闸、帮会与回档脚本、匹配与 go/db 余项' },
    { title: 'Wave4', detail: '第一轮 22 条 low 复核' },
    { title: 'Dsweep', detail: '换视角连续查找，连续两轮无新发现才停' },
    { title: 'Verify', detail: 'high / medium 与修复缺陷的对抗复核' },
  ],
}

const WAVES = (args && args.waves) || [2]
const FIND_ONLY = !!(args && args.findOnly)
const D_MAX = (args && args.dMaxRounds) || 3
const EXTRA_VERIFY = (args && args.extraVerify) || []
const WAVE1_TITLES = (args && args.wave1Titles) || []

async function batched(thunks, size) {
  const out = []
  for (let i = 0; i < thunks.length; i += size) {
    const part = await parallel(thunks.slice(i, i + size))
    for (const p of part) out.push(p)
  }
  return out
}

const PRE = [
  '你是只读审计员，参与 mmorpg 服务器仓库（D:\\luyuan\\xuanming-server-mmo-cpp，main 分支）上线前第二轮「数据问题 / 覆盖回档」审计。用中文作答。',
  '',
  '【只读约束，必须遵守】',
  '- 不创建、不修改、不删除任何文件（包括 %TEMP%、会话草稿目录、/tmp）。',
  '- 按提交读文件只用 git show <rev>:<path> 直接输出到终端，或用 git grep <rev>；git diff 也只直接输出。不得用 > 或 Out-File 之类重定向成文件。',
  '- 不跑任何构建、测试、静态检查（go build / go test / go vet / msbuild / cmake / mvn 等一律不跑，AGENTS.md §10.1）。只读文件、grep、git log/show/diff。',
  '- 工作区有其他会话在改，读到半成品时注明。行号以当前工作区为准。',
  '',
  '【要找的问题】',
  '数据问题：丢失、重复（资产复制）、部分提交、ID 冲突、跨存储不一致。',
  '覆盖回档：旧数据盖新数据、双写者、乱序或重试旧载荷、加载旧快照后再存盘。',
  '项目尚未上线（负责人 2026-10-18 确认），首次上线以 go/db dual + scene/login 严格提交屏障整批起步。只在混版、滚动升级、降级窗口出现的问题照报，但标明"仅混版窗口"，严重度降一级。',
  '',
  '【已知问题不要重报】开工前先读两份文档：docs/design/data-overwrite-rollback-audit-20261018.md 的 §2–§4（第一轮，约 140 行）；docs/design/data-overwrite-rollback-audit-r2-20261019.md 的 §3–§4（本轮第一波）。已登记条目不重报；若发现已登记条目的修复本身有缺陷，或是已登记条目的新变种，要报，并在 related 里写关联编号。',
  '',
  '【严重度】high = 默认配置下可达的资产复制，或回档 / 丢失资产；medium = 默认可达的丢进度，或需特定故障才可达的资产问题，或门控功能里的严重问题；low = 其余。运维工具需要人为误用才触发的，最高 low，除非缺省参数就会出错。',
  '',
  '【方法】每条先自己尝试推翻（读调用方、配置缺省值、上游守卫、Lua 原子性）再报；推翻成功的不进 findings，只在 excluded 里各写一行。locations 写真实的 文件:行（当前工作区）。trigger 一步一步写谁先谁后。reachability 写默认配置能否触发、受哪个开关门控（配置键、缺省值与出处 文件:行）。registered 写设计文档里是否已登记为已知残余（文档:行 或"未登记"）。refute_attempt 写你尝试推翻它为什么没成功。只写结论和证据，不贴大段代码。unread 列出没读完的部分。',
].join('\n')

const FINDING = {
  type: 'object',
  properties: {
    title: { type: 'string' },
    severity: { type: 'string', enum: ['high', 'medium', 'low'] },
    kind: { type: 'string', enum: ['修复缺陷', '新问题', '已登记变种'] },
    related: { type: 'string' },
    locations: { type: 'array', items: { type: 'string' } },
    trigger: { type: 'string' },
    reachability: { type: 'string' },
    registered: { type: 'string' },
    fix: { type: 'string' },
    confidence: { type: 'string', enum: ['确定', '很可能', '存疑'] },
    refute_attempt: { type: 'string' },
  },
  required: ['title', 'severity', 'kind', 'locations', 'trigger', 'reachability', 'registered', 'fix', 'confidence', 'refute_attempt'],
}
const FIND_SCHEMA = {
  type: 'object',
  properties: {
    findings: { type: 'array', items: FINDING },
    excluded: { type: 'array', items: { type: 'string' } },
    unread: { type: 'array', items: { type: 'string' } },
    coverage: { type: 'string' },
  },
  required: ['findings', 'excluded', 'unread'],
}
const REVIEW_SCHEMA = {
  type: 'object',
  properties: {
    items: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          no: { type: 'integer' },
          title: { type: 'string' },
          verdict: { type: 'string', enum: ['成立', '部分成立', '推翻', '已修复'] },
          severity_after: { type: 'string', enum: ['high', 'medium', 'low', 'none'] },
          locations: { type: 'array', items: { type: 'string' } },
          reason: { type: 'string' },
          fix: { type: 'string' },
        },
        required: ['no', 'title', 'verdict', 'severity_after', 'locations', 'reason'],
      },
    },
    new_findings: { type: 'array', items: FINDING },
  },
  required: ['items'],
}
const VERDICT_SCHEMA = {
  type: 'object',
  properties: {
    verdict: { type: 'string', enum: ['成立', '部分成立', '推翻'] },
    severity_after: { type: 'string', enum: ['high', 'medium', 'low', 'none'] },
    reason: { type: 'string' },
    evidence: { type: 'array', items: { type: 'string' } },
  },
  required: ['verdict', 'severity_after', 'reason', 'evidence'],
}

const WAVE2 = [
  { key: 'persist-schema', scope: [
    '【本次范围：混版本持久字段保护（2026-10-19 新落码，未编译、未测试），增量对抗评审】',
    '设计：docs/design/mixed-version-persist-field-protection.md（v0.4，先读 §9 各批状态与 §15 实施调整，再按需读不变量与判定章节）。',
    '代码（git diff 8767c5027 HEAD -- <路径> 加 git diff HEAD -- <路径> 看改动，再按需读全文）：',
    '- proto/db/proto_option.proto、proto/persist_schema_gen.txt、proto/common/database/mysql_database_table.proto、player_cache.proto、bag_quest_mail_data.proto',
    '- tools/proto_generator/protogen/internal/persistschema/ 全部；生成产物 cpp/generated/persist_schema/、go/shared/generated/persistschema/（若存在）',
    '- go/db/internal/kafka/：persist_schema.go、player_row_fence.go、key_ordered_consumer.go、store_failure.go、run_db_op 相关；go/db/db.go；go/db/etc/db.yaml（工作区有改动）',
    '- go/db/internal/barrierrestore/restore.go、scripts.go；go/db/cmd/save_barrier_restore；go/db/cmd/data_stress/main.go',
    '- C++：cpp/libs/engine/core/utils/proto/unknown_field_carry.h（工作区未跟踪新文件，可能是半成品）、cpp/libs/engine/infra/storage/redis_client/redis_client.h、cpp/libs/services/scene/player/system/player_database_loader.cpp、player_data_loader.h，以及存盘快照组装处',
    '- go/login/internal/logic/pkg/dataloader/（父提交屏障是否比较世代）；go/data_service/cmd/debug_import/bundle_import.go（工作区改动）；tools/merge_zone/player_blob_migrate.go（是否保留未知字段）',
    '- deploy/k8s/persist-schema-alerts.yaml',
    '重点问题：',
    '1. 每一跳（C++ 加载→内存→存盘、Redis Lua、go/db 写 SQL、data_service 读写、debug 工具、merge_zone 拷贝）是否真的"要么完整保留未知字段、要么拒绝或延后"？有没有哪一跳仍静默丢字段？',
    '2. C0 / C1 / C1b / C2 判定：会不会把合法的新版本存盘永久拒绝或无限延后（丢进度）？延后的任务去了哪里，会不会在更新的任务之后才落库（旧盖新）？死信后数据还能恢复吗？',
    '3. 引导关闭标记 db:persist_schema:bootstrap_closed:zone:{z}：Redis 丢键或主从切换后"只关不开"是否仍成立？读失败时是 fail-closed 吗？',
    '4. 两张表（player_database / player_database_1）一张被拒、一张被接受时，会不会留下跨表不一致的部分提交？',
    '5. save_barrier_restore 接受 schema_gen 字段后，会不会把旧世代的屏障恢复回来，使旧 DBTask 被重新接受？',
    '6. 缺省配置下这套机制是否生效？生成产物还没生成时，代码路径会走到哪个分支（只读判断，不编译）？',
  ].join('\n') },
  { key: 'battle-settlement', scope: [
    '【本次范围：第一轮 §2.4 战斗结算三处的修复（2026-10-19 落码，未编译、未测试），增量对抗评审】',
    '先读第一轮审计 §2.4 的"修复"段、docs/PROGRESS.md 里「2026-10-19 审计 §2.4 战斗结算三处：丢数据修复」条目（grep 标题定位），以及 docs/design/cross-zone-matchmaking.md §13.6 的「2026-10-19 更正」、docs/design/turn-battle-gap-closure.md §5 第 3 条。',
    '代码（git diff 8767c5027 HEAD -- <路径> 加 git diff HEAD -- <路径>，再按需读全文）：',
    '- cpp/libs/services/scene/battle/system/player_battle.cpp/.h',
    '- cpp/libs/modules/bag/bag_service.h/.cpp（MaxNewInstancesFor）',
    '- cpp/libs/services/battle/system/result_commit.h；cpp/libs/services/battle/settlement/settlement_outbox.h、settlement_reply_shape.h',
    '- cpp/nodes/battle/logic/battle_room_manager.h/.cpp；cpp/nodes/battle/main.cpp',
    '- cpp/libs/services/scene/player/system/asset_op_system.cpp 改动；货币系统 AddCurrency 的返回码定义',
    '重点问题：',
    '1. cpp-economy#0：号段余量预检与实际铸号之间，其他系统（收件箱、资产通道、商店、神秘商人）会不会消耗号段，导致预检通过、入包时仍铸不出号，又回到"吞掉掉落 + RecordApplied"？MaxNewInstancesFor 的上界对可堆叠、不可堆叠、临时格、多 config 是否真的够？',
    '2. cpp-economy#1：kAssetBlocked 跳过金币后登记账本，解封后金币还能补发吗（是否按设计放弃）？kAssetFrozen 仍延后——冻结期间下一局开打会不会覆盖 pending（第一轮原问题的后半段）？',
    '3. social-match-battle#2：停机 flush / 回执处理顺序改了之后，是否还存在评分与帮会历练已入账、个人结算永不派发？反过来会不会重复派发（同一结算发两次 → 双倍奖励）？settlement_outbox 的去重键是什么？',
    '4. 结算应用的幂等：pending 部分应用后崩溃、重投时会不会二次发放已发部分？账本记录与背包 / 货币写入是否在同一存盘快照里？',
    '5. 结算在玩家退出中、跨场景交接冻结中、或被 release 释放中到达时的行为。',
  ].join('\n') },
  { key: 'mystery-merchant', scope: [
    '【本次范围：神秘商人（2026-10-15~17 在别的分支开发，2026-10-19 合入 main，第一轮审计时还不在 main 上），完整资产审计】',
    '设计：docs/design/mystery-merchant.md。',
    '代码：',
    '- cpp/libs/services/scene/player/system/player_mystery_merchant.cpp/.h、mystery_merchant_rules.h',
    '- cpp/nodes/scene/handler/rpc/player/player_mystery_merchant_handler.h/.cpp',
    '- proto/scene/player_mystery_merchant.proto；proto/common/component/player_mystery_merchant_comp.proto（是否持久化、挂在哪张玩家表）',
    '- go/scene_manager/internal/logic/mystery_merchant.go；scene_manager 的 config 与 etc yaml 里相关段',
    '- 配置表生成代码只在需要时读（cpp/generated/table/code/mysterymerchant*）',
    '重点问题（按资产生命周期逐步追）：',
    '1. 购买：扣货币、发物品、扣库存、记限购计数，这几步是否在同一事务或同一存盘快照里？中途失败（背包满、号段不足、货币不足、Redis 失败）时哪些已生效、哪些回滚？',
    '2. 全服或全场景共享库存（如在 scene_manager / Redis）：并发购买会不会超卖？scene 侧扣了钱、scene_manager 侧扣库存失败（或超时后实际成功）会不会白扣或白送？',
    '3. 重试与重复请求：客户端重发、RPC 超时重试会不会买两次？有没有请求号或幂等键？',
    '4. 限购计数与刷新：计数存在哪里？加载旧快照、回档、跨场景交接后计数回退，能否反复购买？刷新时间用的是哪个时钟，时钟回拨会不会重置限购？',
    '5. 冻结闸与退出闸：玩家在跨场景交接冻结中、退出中、被 release 中能否购买？购买修改的持久组件是否会被置脏并进入存盘？',
    '6. 商人出现 / 消失（scene_manager 调度）与玩家购买并发：商人已消失后的购买是否仍被接受？',
  ].join('\n') },
  { key: 'route-reconcile', scope: [
    '【本次范围：data_service 路由自动对账（routereconcile，2026-10-19 新落码，约 2700 行，未编译）与相关配置，增量对抗评审】',
    '设计：docs/design/player-route-mapping-durability-proposal.md、docs/design/data_service_role_and_scope.md、docs/design/player-storage-placement.md（工作区均有改动，按需读与对账相关的章节）。',
    '代码（git diff 8767c5027 HEAD -- <路径> 加 git diff HEAD -- <路径>，再按需读全文）：',
    '- go/data_service/internal/routereconcile/reconciler.go、cache.go（工作区另有未跟踪测试，可不读）',
    '- go/data_service/internal/config/config.go、etc/data_service.yaml、data_service.go、internal/svc/servicecontext.go、internal/metrics/metrics.go 的接线',
    '- go/data_service/internal/logic/data_logic.go 的改动、internal/store/id_segment_store.go、internal/store/route_store.go 与 route_reconcile.go 被对账器调用的部分',
    '- go/shared/placement/route.go、route_ready.go（工作区有改动）',
    '重点问题：',
    '1. 自动对账会写或修复映射吗？写入是否带 rev / maprev 比较，会不会用旧映射盖新映射，把玩家指回旧物理库？',
    '2. 与 merge_zone 发布、玩家迁移（relocate）、rewarm、freeze 并发时有无 CAS 或锁？两个 data_service 副本同时对账（选主失效、脑裂）会怎样？',
    '3. cache.go 的缓存过期与失效：缓存陈旧时，读写会不会落到旧库，读到空数据后被当成新玩家或覆盖？',
    '4. 对账发现"映射缺失"时是补种还是 fail-closed？补种的落点依据是什么，会不会与真实数据所在库不一致（跨存储不一致）？',
    '5. 缺省是否开启？开启后默认节奏下的行为。',
  ].join('\n') },
  { key: 'merge-zone', scope: [
    '【本次范围：合服工具 tools/merge_zone 与帮会合服围栏，增量对抗评审】',
    '第一轮审计 ops-tools-java 区覆盖过 merge_zone 的旧代码，§4 已登记的 4 条 low（P3 只取一次积压快照、-skip-player-rows 不校验落点、撤销时榜单 ZADD 分数回退、不识别 db_first）不要重报，后面会单独复核。',
    '设计与手册：docs/ops/merge-zone-runbook.md（工作区有改动）以及其中引用的设计文档。',
    '代码（git diff bf3c60632 HEAD -- tools/merge_zone go/guild 加 git diff HEAD -- tools/merge_zone，再按需读全文）：',
    '- tools/merge_zone/route_store.go（约 2200 行改动）、route_publish.go、player_blob_migrate.go、fence.go、merge_run.go、relocate_run.go、audit_checks.go、audit_resources.go、storage_audit.go、main.go',
    '- go/guild/internal/logic/merge_fence.go、go/guild/guild.go、go/guild/internal/config/config.go',
    '- tools/data_consistency_check/main.go',
    '重点问题：',
    '1. 合服或迁移拷贝时玩家在线、或 go/db 仍有在途 DBTask：拷贝的是旧快照吗？拷完后源库的迟到写会不会让两边分叉，或拷贝覆盖目标库更新的数据？',
    '2. 路由发布与数据拷贝的先后：会不会先发布后拷贝，让玩家在目标库读到空数据（被当成新角色或覆盖）？',
    '3. 中断后重跑、撤销、resume 是否幂等？会不会重复搬资产或回滚到中间态？',
    '4. ID 冲突：两个区的 player_id、item guid、mail id、帮会 id、交易单号合到一起会不会撞号？号段在合服后从哪里续？',
    '5. 帮会合服围栏：围栏期间帮会资金、成员、贡献的写入是被拒绝还是丢失？围栏解除时序与路由切换是否一致？',
    '6. relocate_run（单玩家迁库）的双写者窗口与锁。',
  ].join('\n') },
  { key: 'login', scope: [
    '【本次范围：go/login 的增量对抗评审（含第一轮未读的 loginqueue）】',
    '第一轮 §2.3（login_session 键绑定 gate 实例与代次）已修，见 docs/design/client-request-ordering.md §7.2 第 2 条；把这次修复当作"可能有缺陷"来查。',
    '代码（git diff bf3c60632 HEAD -- go/login 加 git diff HEAD -- go/login，再按需读全文）：',
    '- internal/logic/pkg/loginsession/loginsession.go、internal/logic/pkg/sessionfence/fence.go',
    '- internal/logic/clientplayerlogin/：entergamelogic.go、loginlogic.go、createplayerlogic.go、account_self_heal.go（新）、deprecation.go、legacy_gate_killswitch.go、session_lifecycle.go、disconnectlogic.go、leavegamelogic.go、player_class_backfill.go',
    '- internal/logic/admin/remove_players_from_accounts.go、internal/logic/loginadmin/',
    '- internal/logic/pkg/loginqueue/：queue.go、gatetoken.go、dispatcher.go、capacity.go（第一轮未读）',
    '- internal/logic/pkg/dataloader/：ensure_player_all_data_async.go、parent_commit_barrier.go',
    '- internal/logic/pkg/node/node_cache.go、watcher.go',
    '重点问题：',
    '1. 账号角色目录（账号 blob）的写者：建角、account_self_heal、remove_players_from_accounts、player_class_backfill——是否都走同一把锁或 CAS？并发时会不会把刚建的角色从账号里丢掉，或把已删除的角色复活？',
    '2. login_session 新键与兼容键并存：Disconnect / LeaveGame / 过期清理会不会删掉另一个连接的状态？旧版 gate 兼容键还能被跨账号继承吗？',
    '3. loginqueue：同一账号或同一角色能否被并发放行两次，导致两个登录链同时进场、出现两个持有者？gate token 重复、过期、被重放时怎样？',
    '4. dataloader：异步确保数据加载与在途存盘并发时，会不会把旧快照装进缓存，随后被当作最新数据进场（加载旧快照后再存盘）？父提交屏障与字段保护的世代比较是否一致？',
    '5. legacy gate kill switch、deprecation：拒绝点前移后，会不会在已有部分副作用（会话写入、账号写入）之后才拒绝，留下残留状态？',
  ].join('\n') },
]

const WAVE3 = [
  { key: 'assets-misc', scope: [
    '【本次范围：资产相关的其余增量（收件箱、号段引导、交易、好友、邮件），增量对抗评审】',
    '第一轮已登记 go-asset-services#0/#1（交易）、收件箱 Ack 以 Redis 落盘为依据（low），不要重报。',
    '代码（git diff bf3c60632 HEAD -- <路径> 加 git diff HEAD -- <路径>，再按需读全文）：',
    '- cpp/libs/services/scene/player/system/player_inbox_system.cpp/.h；cpp/nodes/scene/rpc_replies/player_inbox_response_handler.cpp/.h',
    '- cpp/nodes/scene/id_segment_bootstrap.cpp/.h；cpp/nodes/scene/main.cpp 中的接线；cpp/libs/modules/id_segment/',
    '- cpp/libs/services/scene/player/system/asset_op_system.cpp',
    '- go/trade/internal/data/asset_op_repo.go、go/trade/internal/commerce/engine.go、go/trade/internal/svc/assetchannel.go',
    '- go/friend/internal/data/sweep_repo.go、friend_repo.go、block_repo.go',
    '- go/mail/：internal/data/sweep.go、policy.go、repo.go、campaign.go，internal/maintenance/sweep.go，internal/server/server.go，cmd/mailadmin/main.go，etc/mail.yaml；设计 docs/design/mail-system.md',
    '重点问题：',
    '1. 收件箱：应答乱序、重复、超时后实际成功时，资产会不会发两次或丢一次？与玩家退出、交接并发时怎样？',
    '2. 号段引导：节点重启、号段 Redis / 库回退、多个 scene 并发引导时，item / mail 等 guid 会不会重复发号？引导失败时是 fail-closed 吗？',
    '3. 邮件清扫：会不会删掉带未领附件的邮件（资产丢失）？清扫与领取并发时会不会附件已发、邮件未删（可重复领取），或邮件已删、附件未发？mailadmin 的批量操作有无 dry-run 与鉴权？',
    '4. 交易 asset_op_repo 改动：恢复路径会不会重复执行已完成的资产腿，或把已终结的合同重新打开？',
    '5. 好友清扫：会不会误删在用关系；双向关系一边删了另一边没删（跨存储不一致）。',
  ].join('\n') },
  { key: 'lifecycle-unread', scope: [
    '【本次范围：第一轮未读完的 player_lifecycle 段落与存盘链底层】',
    'cpp/libs/services/scene/player/system/player_lifecycle.cpp 很大，只读以下段落及其直接调用链（用 grep 定位函数）：',
    '- 交接取证（handoff 证据的写入与读取）；',
    '- 改派待确认表（relocate pending confirm，含 relocate_confirm.h、travel_freeze_cap.h）；',
    '- EnterScene 应答分派；',
    '- HandleSessionEndedRelease、DecideSessionEndedBinding、WriteExitReleaseMarkIfEligible、HandlePlayerAsyncSaved（第一波只读了注释）。',
    '另读：cpp/libs/engine/thread_context/redis_manager.cpp/.h（2026-10-19 改动约 160 行）、cpp/libs/engine/infra/storage/redis_client/redis_client.h（存盘 Lua）、cpp/libs/services/scene/player/system/player_scene.cpp 改动、cpp/libs/services/scene/player/comp/player_ownership_comp.h。',
    '相关设计：docs/design/location-release-binding.md、docs/design/player-async-save-loss-windows.md、docs/design/scene-owner-reentry-barrier.md（按需读）。',
    '重点问题：',
    '1. 交接取证：证据写入失败或丢失时，新节点会不会在旧节点数据未落地时加载旧快照？证据与存盘的先后是否保证？',
    '2. 改派待确认表：确认丢失、超时、重复时，玩家会不会被两个节点同时持有（双写者），或被放弃导致未落盘数据丢失？',
    '3. EnterScene 应答分派：应答到达时实体已被销毁重建、或属于旧会话时，会不会对新实体执行旧操作？',
    '4. HandlePlayerAsyncSaved：存盘回执乱序（旧 save_seq 的回执晚到）会不会把"已落地"标记推到错误版本，导致后续以为已落地而删除实体或写 A1′？',
    '5. redis_manager 改动：连接切换、重连、主从切换期间排队的存盘命令会不会丢失、重发（旧载荷重放）或跨连接乱序？',
  ].join('\n') },
  { key: 'write-entries-gates', scope: [
    '【本次范围：补覆盖——各业务系统修改持久组件的写入口，是否绕过冻结闸或退出闸】',
    '背景：玩家跨场景交接时有"冻结"（旧节点停止修改、等新节点接手），退出流程有"退出闸"（开始退出后不再接受修改，先存盘后销毁）。若某个写入口在冻结或退出中仍修改持久组件，修改会落在已经快照或已经交出的实体上：要么丢失，要么与新节点双写。',
    '步骤：',
    '1. 先找出冻结闸与退出闸的实现与判定函数（grep travel_freeze、freeze、Exiting、exit_state、IsPlayerExiting、kExiting、handoff、relocate_confirm 等；player_lifecycle.cpp、travel_freeze_cap.h、player_gm_guard.h、客户端消息分发处的统一守卫）。确认统一守卫覆盖了哪些入口（客户端消息？RPC？Kafka 事件？定时器？）。',
    '2. 列出修改持久组件的全部写入口：客户端请求 handler（cpp/nodes/scene/handler/rpc/player/*）、节点间 RPC、gRPC（scene_node_service.cpp）、Kafka 消费（收件箱、资产通道、GM、帮会、邮件通知）、定时器 / tick 系统（任务、buff、体力恢复、限时商人刷新）、战斗结算回调。持久组件以 proto 里 player_database / player_database_1 的字段和 cpp/libs/services/scene 下 *Comp 为准，参考 docs/design/ecs-component-access-rules.md。',
    '3. 对每个统一守卫没覆盖的入口，检查它自己有没有判冻结与退出；没有就报。',
    '重点：资产类（货币、背包、装备、宠物、邮件附件领取、神秘商人购买、任务奖励、战斗结算、GM 发放）优先；非资产的进度类其次。在 coverage 字段里附一张"入口 → 守卫"的覆盖表（每行一句：入口名、是否被守卫、靠哪个守卫）。',
  ].join('\n') },
  { key: 'guild-rollback', scope: [
    '【本次范围：补覆盖——帮会逻辑层正文与区服回档脚本（第一轮未读）】',
    '代码：',
    '- go/guild/internal/logic/ 全部业务逻辑（资金、贡献、成员进出、职位、仓库 / 捐献、帮会战或历练结算入账、解散），go/guild/internal/data/（存储与事务），go/guild 的 Kafka 或事件消费（若有）。合服围栏 merge_fence.go 另有人审，不重点看。',
    '- tools/scripts/k8s_zone_rollback.ps1 正文（第一轮未读），以及 docs/design/zone_data_rollback.md、docs/design/zone_data_rollback_discard_tool.md。第一轮已登记 ops-tools-java#1（PITR 只回退 zone 库），不重报。',
    '重点问题：',
    '1. 帮会资金与贡献：入账与扣款是否幂等（Kafka 重投、RPC 重试会不会重复入账或重复扣款）？玩家侧与帮会侧双边记账（玩家捐献扣钱、帮会加资金）是否原子，失败时有无补偿，会不会钱扣了帮会没加或反之？',
    '2. 成员进出与解散：玩家退帮 / 被踢 / 解散时，帮会侧与玩家侧（玩家持久组件里的帮会 id）会不会不一致？',
    '3. 帮会数据存储的并发写：多个 guild 实例或多协程写同一帮会时有无锁或版本，会不会旧盖新？',
    '4. 回档脚本：缺省参数是否 dry-run？会不会只回退部分存储（zone 库回退而 Redis 缓存、Kafka 位点、全局库、帮会库不回退），造成复制或白亏？执行前是否强制停服、排空？',
  ].join('\n') },
  { key: 'match-rank-db', scope: [
    '【本次范围：组队匹配、评分排行与 go/db 剩余未读部分】',
    '代码：',
    '- go/match/internal/logic/：party.go、party_match.go、queue.go、rating.go、matcher.go、gather.go、cancelqueuelogic.go、keys.go；go/match/internal/kafka/rating_snapshot.go；go/match/internal/team/；go/match/etc/match_service.yaml、internal/config/config.go',
    '- go/rank/（读写与消费评分快照的部分）',
    '- go/db/internal/kafka/retry_stranded.go、durable_enqueue.go 主体（第一轮和本轮第一波都只读了文件头与 flush 主路径）',
    '第一轮已登记：评级来源停写后结果消费卡死（rating.go，low）、全球匹配票据卡死（§2.5，GlobalMatch.Enabled 门控）、已打完的战斗被恢复误判（low）。不重报。',
    '重点问题：',
    '1. 评分：同一场战斗结果重复消费会不会重复加减分？乱序消费会不会旧分数盖新分数？评分快照出口给 rank 的数据与 match 内部是否会不一致？',
    '2. 组队：队伍状态在 Redis 的并发修改（入队、退队、解散、队长变更）有无 CAS？会不会留下幽灵队员，使玩家永远进不了匹配（只记 low，除非涉及资产）？',
    '3. durable_enqueue / retry_stranded：持久入队与 Kafka ack 的先后，崩溃时会不会既没入队又已 ack（丢存盘）？搬移搁浅任务时会不会把旧任务搬到新任务之后执行（旧盖新，dual 下行内围栏能否兜住）？',
  ].join('\n') },
]

const LOW_REVIEW = [
  { key: 'low-1-12', range: '第 1–12 条（C++ 存盘与加载 4 条、C++ 资产 3 条、login 2 条、资产服务 1 条、匹配 2 条）' },
  { key: 'low-13-22', range: '第 13–22 条（运维工具与 Java 7 条、身份原语 3 条）' },
]

const D_ROUNDS = [
  [
    { key: 'asset-currency', lens: '按资产追踪：货币的完整生命周期。产出（战斗结算、任务、GM、邮件附件、收件箱、交易、神秘商人退款）、消耗（商店、神秘商人、强化、交易）、存储（CurrencyComp、欠款 owed / paid）、跨服与合服。逐个产出点和消耗点追幂等、原子性、存盘归属与回档后的状态。' },
    { key: 'fault-redis', lens: '按故障：Redis 丢尾或主从切换。逐个列出 zone Redis、核心 Redis、mapping Redis、共享 Redis 各自持有哪些不可再生状态（屏障、owner_epoch、location、会话、号段、意图桶、标记），丢一段尾巴或切到落后的从库后，各条链路（存盘、登录、交接、补投、资产通道、号段）的行为。' },
    { key: 'entry-scene-client', lens: '按入口：C++ scene 的客户端请求 handler 全量走一遍（cpp/nodes/scene/handler/rpc/player/ 下全部），每个改动玩家数据的 handler 是否校验请求者身份与会话代次、冻结与退出状态、幂等（重发会不会做两次）。' },
  ],
  [
    { key: 'asset-item', lens: '按资产追踪：物品、装备、宠物的完整生命周期。铸号、入包、堆叠拆分、临时格、交易托管、邮件附件、销毁与分解、合服搬迁。追 guid 唯一性、数量守恒、跨存储一致。' },
    { key: 'fault-kafka-crash', lens: '按故障：Kafka 重投、乱序、消费组重平衡、位点回退，以及节点（scene / gate / battle / 各 Go 服务）在关键两步之间崩溃。逐个 topic 列消费者，判断是否幂等、key 是否为业务实体 id、重投旧载荷能否被挡住。' },
    { key: 'entry-go-admin', lens: '按入口：Go 服务的 gRPC / HTTP 入口与 admin / GM 接口（login、data_service、scene_manager、player_locator、guild、mail、trade、match、rank，以及 java/gateway_node 的 admin API），关注鉴权、幂等、对玩家数据或资产的直接写。' },
  ],
  [
    { key: 'asset-guild-mail-reward', lens: '按资产追踪：帮会资金与玩家帮会贡献、邮件附件、任务奖励、成就与活动奖励的完整生命周期。' },
    { key: 'fault-clock-upgrade', lens: '按故障：时钟回拨与跳变（所有用墙钟做比较、过期、排序、去重、限购刷新的地方），以及滚动升级混版（新旧 scene / go/db / login / gate / scene_manager 并存时各协议字段的语义）。混版问题按规则降一级。' },
    { key: 'entry-consumers-timers', lens: '按入口：Kafka 消费者与周期任务入口（每个 consumer、每个 sweeper / reconciler / leader 任务 / 定时器），关注重复执行、并发执行（多副本）、与玩家在线写的竞态。' },
  ],
  [
    { key: 'asset-crosszone-idseg', lens: '按资产追踪：跨区与合服、号段（item / mail / player / snapshot / txlog）的全链路，关注撞号、重发号、跨区资产复制。' },
    { key: 'fault-partition-lease', lens: '按故障：网络分区、etcd 租约丢失与恢复、gRPC 超时后实际成功、半开连接。关注双持有者、超时重试导致的重复执行。' },
    { key: 'entry-ops-cli', lens: '按入口：运维与调试 CLI（tools/ 下全部 Go 工具与 tools/scripts 下改数据的 ps1），关注缺省参数是否安全、是否校验目标环境、是否会在在线时写入。' },
  ],
  [
    { key: 'fresh-eyes-save', lens: '自由视角：从 C++ 存盘快照组装到 go/db 写 SQL 的整条链，挑你认为最可能出错的三个接缝深挖。' },
    { key: 'fresh-eyes-load', lens: '自由视角：从 login 加载、scene_manager 落点到 scene 建实体的整条链，挑你认为最可能出错的三个接缝深挖。' },
    { key: 'fresh-eyes-asset', lens: '自由视角：所有跨服务资产流转（交易、邮件、收件箱、帮会、商城），挑你认为最可能出错的三个接缝深挖。' },
  ],
]

const all = []
const lowReview = []
const dRounds = []

function collect(wave, key, res) {
  if (!res) { log('代理失败或被跳过：' + key); return }
  for (const f of res.findings || []) all.push(Object.assign({ wave: wave, source: key }, f))
}

async function runFinders(wave, list) {
  phase('Wave' + wave)
  const res = await batched(list.map(f => () => agent(PRE + '\n\n' + f.scope, {
    label: 'find:' + f.key, phase: 'Wave' + wave, schema: FIND_SCHEMA,
  })), 6)
  const out = []
  list.forEach((f, i) => { collect(wave, f.key, res[i]); out.push({ key: f.key, result: res[i] }) })
  log('第' + wave + '波完成：累计发现 ' + all.length + ' 条')
  return out
}

const waveResults = {}
if (WAVES.includes(2)) waveResults.wave2 = await runFinders(2, WAVE2)
if (WAVES.includes(3)) waveResults.wave3 = await runFinders(3, WAVE3)

if (WAVES.includes(4)) {
  phase('Wave4')
  const res = await batched(LOW_REVIEW.map(r => () => agent(PRE + '\n\n' + [
    '【本次任务：复核第一轮审计 §4 的 low 条目，逐条尝试推翻】',
    '第一轮审计 docs/design/data-overwrite-rollback-audit-20261018.md §4 在"下面 23 条没有复核"之后列了一组 low（SanitizeSkillList 两个区域重复报告，算 1 条，共 22 条），按出现顺序编号 1–22。你负责' + r.range + '。',
    '对每条：按当前工作区代码核对（行号可能已漂移，用 grep 重新定位）；判断它现在是否仍成立（可能已被后续改动修掉，例如"存盘不保留未知字段"可能已由 2026-10-19 落码的字段保护处理）；尝试推翻；给出复核后的严重度与修复方向。',
    'items 里每条一项，no 用 1–22 的编号。复核过程中若发现与该条相关、但不同的新问题，放进 new_findings（格式同发现）。',
  ].join('\n'), { label: 'review:' + r.key, phase: 'Wave4', schema: REVIEW_SCHEMA, model: 'sonnet' })), 6)
  LOW_REVIEW.forEach((r, i) => {
    const x = res[i]
    if (!x) { log('代理失败或被跳过：' + r.key); return }
    for (const it of x.items || []) lowReview.push(it)
    for (const f of x.new_findings || []) all.push(Object.assign({ wave: 4, source: r.key }, f))
  })
  log('low 复核完成：' + lowReview.length + ' 条')
}

if (WAVES.includes(5)) {
  phase('Dsweep')
  let dry = 0
  let round = 0
  while (dry < 2 && round < D_MAX && round < D_ROUNDS.length) {
    const seen = WAVE1_TITLES.concat(all.map(f => f.title))
    const seenText = seen.map((t, i) => (i + 1) + '. ' + t).join('\n')
    const list = D_ROUNDS[round]
    const res = await batched(list.map(p => () => agent(PRE + '\n\n' + [
      '【本次任务：换视角查找（第 ' + (round + 1) + ' 轮）】',
      '前面各波已经按子系统和按改动查过。这一轮换一个视角查全仓，找前面漏掉的问题。',
      '你的视角：' + p.lens,
      '',
      '【本轮之前已发现的问题（不要重报，只报这个列表和两份审计文档里都没有的新问题）】',
      seenText,
      '',
      '如果按你的视角查完确实没有新问题，findings 返回空数组，并在 excluded 里写你查过并排除了什么。',
    ].join('\n'), { label: 'sweep:' + p.key, phase: 'Dsweep', schema: FIND_SCHEMA })), 6)
    let fresh = 0
    const roundOut = []
    list.forEach((p, i) => {
      const x = res[i]
      if (!x) { log('代理失败或被跳过：' + p.key); return }
      fresh += (x.findings || []).length
      for (const f of x.findings || []) all.push(Object.assign({ wave: 'D' + (round + 1), source: p.key }, f))
      roundOut.push({ key: p.key, result: x })
    })
    dRounds.push({ round: round + 1, fresh: fresh, results: roundOut })
    log('换视角第 ' + (round + 1) + ' 轮：新发现 ' + fresh + ' 条')
    dry = fresh === 0 ? dry + 1 : 0
    round++
  }
  if (dry < 2) log('换视角查找在第 ' + round + ' 轮因轮数上限停止，尚未连续两轮无新发现')
}

let verified = []
if (!FIND_ONLY) {
  phase('Verify')
  const cands = all.filter(f => f.severity === 'high' || f.severity === 'medium').concat(EXTRA_VERIFY)
  const jobs = []
  cands.forEach((f, idx) => {
    const lenses = f.severity === 'high'
      ? ['代码路径与时序：逐步核对每个 文件:行 与触发时序的每一步是否真的成立', '可达性与上游守卫：缺省配置、开关、上游校验、Lua 原子性、已登记的设计残余是否已挡住或已接受']
      : ['代码路径、时序与可达性：逐步核对每个 文件:行 与触发时序，并检查缺省配置、上游守卫和已登记残余']
    lenses.forEach((lens, li) => jobs.push({ idx: idx, f: f, lens: lens, li: li }))
  })
  const res = await batched(jobs.map(j => () => agent([
    '你是对抗复核员，任务是尽力推翻下面这条审计发现。用中文作答。',
    '只读：不创建、修改、删除任何文件；git show 只直接输出不重定向；不跑任何构建、测试、静态检查。行号以当前工作区（D:\\luyuan\\xuanming-server-mmo-cpp）为准。',
    '项目尚未上线；只在混版窗口出现的问题严重度降一级。已在设计文档登记为接受的残余，按"已登记"处理，不算推翻但要写明。',
    '',
    '【待复核发现】',
    JSON.stringify(j.f, null, 2),
    '',
    '【你的视角】' + j.lens,
    '读真实代码核对；拿不准时倾向判"推翻"或降级，但必须给出 文件:行 证据。输出 verdict（成立 / 部分成立 / 推翻）、复核后严重度（high / medium / low / none）、理由与证据。',
  ].join('\n'), { label: 'verify:' + (j.f.source || 'extra') + '#' + j.idx + '.' + j.li, phase: 'Verify', schema: VERDICT_SCHEMA })), 6)
  verified = cands.map((f, idx) => ({ finding: f, verdicts: jobs.map((j, k) => j.idx === idx ? res[k] : null).filter(Boolean) }))
  log('复核完成：' + cands.length + ' 条候选')
}

return { waves: WAVES, findOnly: FIND_ONLY, findings: all, lowReview: lowReview, dRounds: dRounds.map(r => ({ round: r.round, fresh: r.fresh })), waveResults: waveResults, dRoundResults: dRounds, verified: verified }
