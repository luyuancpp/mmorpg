# Merge-Zone Runbook(合服操作手册)

> **状态**: v1 — 2026-05-23
> **范围**: K8s 部署形态下,把 source zone 合并进 target zone 的端到端 ops SOP。
> **关联**:
> - 设计:[`docs/design/server_merge_design.md`](../design/server_merge_design.md) / [`docs/design/server-merge-gap-fixes.md`](../design/server-merge-gap-fixes.md)
> - 工具:`tools/merge_zone/` + `pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone`
> - 备份:[`mysql-backup-pitr-runbook.md`](mysql-backup-pitr-runbook.md)
> - 灾难回滚:[`tools/scripts/k8s_zone_rollback.ps1`](../../tools/scripts/k8s_zone_rollback.ps1)
>
> **谁该读**: 运维 + 主程值班。**首次合服**前请同时读完上面 4 篇前置文档。

---

## 0. TL;DR — 四件不能搞错的事

1. **必须先备份再合服**。`mysql-backup-pitr-runbook.md` §2 的手动备份 + `mysql-backup` CronJob 跑过当天的 dump,二者缺一不行。
2. **player blob 拷贝必须在 mapping remap 之前做**(`tools/merge_zone/main.go` 顶部注释明确写了)。dev_tools.ps1 的 `merge-zone` 命令已经��这个顺序,**不要手动调换 flag**。
3. **dry-run 必须跑两遍**:T-1 day 一次 + T-0 维护窗口最开始一次。两次输出的玩家数 / guild 数应**完全一致**,否则中间有玩家又登录写�� mapping —— 说明 zone 没真正下线,**立刻终止合服**。
4. **失败回滚的硬截止线 = T-0 的 Step 4** (`merge-zone -apply` 跑完之前)。一旦 apply 完成,Redis mapping / MySQL guild zone_id 已写,**不能简单回退**,只能从 Step 3 备份还原,损失维护窗口期间所有写入。

---

## 1. 资产清单

| 资源 | 在哪 | 用途 |
|---|---|---|
| `tools/merge_zone/main.go` | repo | 合服核心工具,5 步流程 |
| `tools/merge_zone/audit_resources.go` | repo | dry-run 资源审计(mail/friend/auction 等是否完整迁移) |
| `tools/scripts/dev_tools.ps1 -Command merge-zone` | repo | 包了一层的 ops 入口 |
| `mysql-data-pvc` / `mysql-backup-pvc` | k8s | source / target zone 的 MySQL 数据 + 备份 |
| `tools/scripts/k8s_zone_rollback.ps1` | repo | **如果合服失败**,从备份还原 source/target |
| `dev-robot-zones` | dev_tools.ps1 | 合服后烟雾测试入口 |

---

## 2. 时间线总览

```
                  T-7d                   T-1d                  T-0                  T+1 ~ T+7
   ┌───────────────┴───────────────────────┴────────────────────┴──────────────────────┴─────►
   │ 公告 + 容量评估             dry-run + audit         维护窗口 60 min          观察期
   │                                                    ┌─────────────────────┐
   │                                                    │ 1. 公告踢人 (10m)    │
   │                                                    │ 2. zone-down (5m)   │
   │                                                    │ 3. 备份 (5m)         │
   │                                                    │ 4. dry-run + apply (15m) │
   │                                                    │ 5. audit verify (10m) │
   │                                                    │ 6. zone-up (10m)    │
   │                                                    │ 7. smoke test (5m)  │
   │                                                    └─────────────────────┘
```

---

## 3. T-7 days(预备期)

### 3.1 公告

- 客户端弹窗 + 官网公告 + 社区 / 群通知
- **必须包含**:精确合服时间(精确到分钟)、合服后的服名、预计停服时长
- 公告时间点要避开节假日 / 大版本更新

### 3.2 容量评估

```bash
# 检查 source + target 玩家总数能否容纳
kubectl exec -n mmorpg-zone-<src> deploy/mysql -- \
  mysql -uroot -p<pwd> -e "SELECT COUNT(*) FROM mmorpg.player;"
kubectl exec -n mmorpg-zone-<dst> deploy/mysql -- \
  mysql -uroot -p<pwd> -e "SELECT COUNT(*) FROM mmorpg.player;"
```

判断标准:合并后总数 < target zone scene 节点单节点 player 上限 × 节点数 × 0.7。**到 0.7 阈值就要扩容 target zone**。

### 3.3 排期登记

把合服窗口写进 `docs/ops/release-checklist.md` 的"近期窗口"区块,确保:
- 没有撞 release / hotfix 窗口
- ops 主备值班都在线
- 主程能远程支援(主合服窗口主程必须在线)

---

## 4. T-1 day(彩排日)

### 4.1 跑 dry-run merge

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone `
  -MergeSourceZone <SRC_ID> -MergeTargetZone <DST_ID> `
  -DryRun
```

**期望输出**(关键数字):

```
Mapping: N players with home_zone=<SRC_ID>
[DRY-RUN] Would migrate up to M guild rows (raw count M, minus K name conflicts)
Redis rank: P ZSET members would be migrated  guild_rank:zone <SRC>→<DST>
Mapping Redis: N players matched, N home_zone values would be migrated
=== Done (players_in_source=N guild_rows=M ...) ===
```

**记下这些数字**,T-0 第二次 dry-run 必须**完全相同**(只允许 ±1 行因为后台过期 key 删除)。

### 4.2 跑资源审计

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone-audit `
  -MergeSourceZone <SRC_ID> -MergeTargetZone <DST_ID>
```

报告里**必须人工 review** 的内容:

| 资源类 | 关注点 |
|---|---|
| `friend` / `friend_request` | 全局表(无 zone_id 列),按 player_id 索引,合服后自然存活,只需看数量是否符合预期 |
| `guild_member` | 同上 |
| `online_keys` | source zone 的 `friend:online:{pid}` Redis TTL keys 必须 = 0,否则说明 zone-down 不彻底,**禁止合服** |
| `player.name (manual)` | **block 级提醒** — 自动检测当前**未实现**(详见 §4.4),必须人工处理 |

**当前 audit 工具 NOT 覆盖的(因为底层数据结构限制)**:

- **昵称冲突**: 项目���没有 `player(name, zone_id)` 表,玩家昵称在 `player_database` 的 protobuf MEDIUMBLOB 里。SQL 无法直接查,要做需引 protobuf 依赖到 merge_zone 工具。**当前走 §4.4 人工流程**。
- **邮件 / 拍卖 / 聊天历史**: 这些 Go 服务**当前未实现**(grep `deploy/mysql-init/` 仅有 `guild` / `friend` / `user` 系列表)。等服务上线再补 audit。
- **公会申请 (guild_application)**: 同上,目前不存在。

**任何 block 级或"超过预期"的数字 → 立刻停下来,联系主程确认。**

### 4.3 演练备份恢复(可选但强烈建议)

在 staging 环境跑一次完整 §6 流程,确认:
- 备份命令能跑通
- `k8s_zone_rollback.ps1` 能从备份还原
- audit 工具能跑出报告

### 4.4 处理昵称冲突(P0-G)

⚠️ **2026-05-23 现状**:**自动检测当前未实现**。原因:

1. 项目里没有 `player(name, zone_id)` 这种 SQL 表 — 玩家昵称在 `player_database` 的 protobuf MEDIUMBLOB 里
2. 玩家 `home_zone` 不在 MySQL 列里,只在 mapping Redis (`player:zone:{id}`)
3. 要做检测需 merge_zone 工具引 protobuf 依赖 + 解 blob —— 待后续会话实现

**当前必须走的人工流程**:

- **方案 A(强烈推荐)**:合服公告里写明 **N 天前自动改名规则**:"合服当日,源服与目标服重名玩家,源服一方将被自动加 `_<src_zone_id>` 后缀"。N=3 天给玩家充分窗口自己改名。**这是当前唯一能闭环的办法**。
- **方案 B(临时上线后补救)**:合服后玩家投诉到客服,GM 用单玩家改名工具处理。劣化体验。

**等自动检测落地后**,本节会改为"工具自动 stamp `force_rename_required` flag,客户端登录时见 flag 弹改名 UI"。届时方案 A/B 都不再需要。

**force_rename_required flag 链路是预先搭好的**(proto 字段、login 端读取、客户端字段)—— 缺的只是合服时的"到底该 stamp 哪些 player_id"。所以将来上 protobuf 解码后只需要补一个 stamp 的 player_id 列表即可,基础设施不会再动。

### 4.5 通知 ops 维护窗口

- 维护窗口前 24h 在监控群 @ ops 主备
- 准备好回滚物料(备份位置、k8s_zone_rollback 命令模板)

---

## 5. T-0:维护窗口流程(60 分钟)

### Step 1 [10 min] 公告 + 踢玩家

```bash
# 客户端推全服公告(各项目 GM 工具实现不同)
# 同时关闭登录入口(Java Gateway 把 source + target 都标 maintenance)

# Java Gateway 设置维护
curl -X POST https://<gateway>/admin/maintenance \
  -H "Authorization: Bearer <admin-token>" \
  -d '{"zones": ["<SRC>", "<DST>"], "enabled": true}'

# 等待 5 分钟让在线玩家自然下线;然后强踢剩余
# (cpp gate 有 admin 端口 / kubectl exec 直接发 SIGTERM 给 scene)
```

**Gate / Scene `kubectl get pods -n mmorpg-zone-<src>` 必须全部 Running 但 active connection ≈ 0**。

### Step 2 [5 min] zone-down(source + target 都要 down)

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName <SRC>
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName <DST>
```

**只 down scene/gate/go-zero**,**不 down 共享 infra**(MySQL / Redis / Kafka / etcd 留着,merge_zone 工具要读写)。

`k8s-zone-down` 的实现已经是这种半 down 形态,确认行为再跑(看 `dev_tools.ps1` 的实现代码确认 namespace 范围)。

### Step 3 [5 min] 备份 source + target MySQL + Redis

```bash
# MySQL: 触发 CronJob 立即跑一次(覆盖性的额外备份,不依赖每天 03:17 那一次)
kubectl create job -n mmorpg-zone-<src> --from=cronjob/mysql-backup \
  mysql-backup-pre-merge-$(date +%s)
kubectl create job -n mmorpg-zone-<dst> --from=cronjob/mysql-backup \
  mysql-backup-pre-merge-$(date +%s)

# 等到两个 job complete
kubectl wait --for=condition=complete job/mysql-backup-pre-merge-<id> -n mmorpg-zone-<src> --timeout=10m
kubectl wait --for=condition=complete job/mysql-backup-pre-merge-<id> -n mmorpg-zone-<dst> --timeout=10m

# Redis: SAVE 命令触发同步 RDB
redis-cli -h <src-mapping-redis> SAVE
redis-cli -h <dst-mapping-redis> SAVE
redis-cli -h <src-data-redis> SAVE
redis-cli -h <dst-data-redis> SAVE
```

**这一步如果失败,立刻终止合服**。备份是回滚唯一保障。

### Step 4 [15 min] dry-run + apply

#### 4.1 第二次 dry-run(必做,即使 T-1 day 已跑)

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone `
  -MergeSourceZone <SRC_ID> -MergeTargetZone <DST_ID> `
  -DryRun
```

**对比 §4.1 记下来的数字** —— 如果 players_in_source / guild_rows / map_matched 任何一个**变了**(超过 ±1):

- **立刻终止**。说明 source zone 没真正下线,有玩家在 §4.1 之后又写了数据。
- 排查:为什么 zone-down 后 mapping 还在变?是否漏了某个 go-zero 服务?是否 cron 任务还在跑?
- 排查清楚再决策是否继续。

#### 4.2 apply

```powershell
# 注意 -migrate-player-blobs 仅在 source / target 用不同 Redis cluster 时加
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone `
  -MergeSourceZone <SRC_ID> -MergeTargetZone <DST_ID>
# (dev_tools.ps1 的 merge-zone 默认走 -apply,不带 -DryRun 即写)
```

**watch 输出**:

```
=== Merge zone <SRC> → <DST> (dry-run=false apply=true) ===
Mapping: N players with home_zone=<SRC>
MySQL: M guild rows migrated for zone <SRC> → <DST>
Redis rank: P ZSET members migrated guild_rank:zone <SRC>→<DST>
Mapping Redis: N players matched, N home_zone values migrated  (<addr> db=<n>)
=== Done (...) ===
```

**任何 ERROR / Fatal → 立刻进 §7 失败回滚**。

### Step 5 [10 min] 合服后审计验证

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command merge-zone-audit `
  -MergeSourceZone <SRC_ID> -MergeTargetZone <DST_ID> `
  -VerifyMerged
```

`-VerifyMerged` 模式:audit 工具检查"按预期所有 source 数据应已不存在于 source zone 视图,而出现在 target zone 视图"。

期望:

| 检查项 | 期望 |
|---|---|
| `mapping Redis` 中 home_zone=`<SRC>` 的 player 数 | **0** |
| `mapping Redis` 中 home_zone=`<DST>` 的 player 数 | T-1 day 测的 src + dst 总和(±1 容差) |
| MySQL `guild` 表 zone_id=`<SRC>` 的��数 | **0** 或 = 名字冲突数(被跳过的) |
| MySQL `guild` 表 zone_id=`<DST>` 的行数 | T-1 day 测的 src 非冲突数 + dst 原数 |
| Redis `guild_rank:zone:<SRC>` ZSET | **不存在或空** |
| Redis `guild_rank:zone:<DST>` ZSET 成员数 | T-1 day 测的合并数 |

**任何不一致 → 立刻进 §7 失败回滚**。

### Step 6 [10 min] zone-up(只 up target;source 永久退役)

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up `
  -ZoneName <DST> -ZoneId <DST_ID> -WaitReady

# source zone 不再 zone-up。namespace 保留 7 天给可能的回滚,7 天后清理
```

**不要立刻删 source namespace**,§8 §"T+7"删除。

### Step 7 [5 min] 烟雾测试

```powershell
# robot 模拟登录 + 基础操作
pwsh -File tools/scripts/dev_tools.ps1 -Command dev-robot-zones -Zones <DST_ID>

# 检查关键 metric
kubectl logs -n mmorpg-zone-<DST> deploy/scene-0 --tail=200 | grep -E "ERROR|FATAL"
```

人工验证:

- [ ] 选一个**原 source zone 的玩家** account 登录,确认 home_zone 已是 dst
- [ ] 该玩家公会数据在(姓名、等级、成员)
- [ ] 公会排行榜上能看到原 source 的公会
- [ ] 该玩家邮件 / 好友数据在
- [ ] 取消 Java Gateway 维护标记

```bash
curl -X POST https://<gateway>/admin/maintenance \
  -d '{"zones": ["<DST>"], "enabled": false}'
```

**完成。**合服窗口结束。

---

## 6. 失败回滚(从 Step 3 备份还原)

**触发条件**:Step 4 / Step 5 / Step 6 / Step 7 任一阶段发现致命错误。

### 6.1 立刻 zone-down(防止玩家进入半合并状态)

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName <DST>
```

### 6.2 还原 MySQL

按 [`mysql-backup-pitr-runbook.md`](mysql-backup-pitr-runbook.md) §4 执行。target zone 必须**完整还原到 Step 3 的 dump 时间点**,否则 dst 玩家在合服窗口前发生的事都丢。

### 6.3 还原 Redis

```bash
# Redis 简单粗暴 — 把 RDB 文件 restore
kubectl exec -n mmorpg-zone-<dst> deploy/redis-mapping -- \
  redis-cli FLUSHDB
kubectl cp <dst>-mapping-pre-merge.rdb \
  mmorpg-zone-<dst>/redis-mapping-0:/data/dump.rdb
kubectl rollout restart deploy/redis-mapping -n mmorpg-zone-<dst>
```

(具体路径以你 Redis 部署形态为准)

### 6.4 重新 zone-up source + target

```powershell
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up -ZoneName <SRC> -ZoneId <SRC_ID> -WaitReady
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up -ZoneName <DST> -ZoneId <DST_ID> -WaitReady
```

### 6.5 公告

立刻发"合服延期"公告,说明回滚原因 + 重新排期。**不要让玩家自己发现合服失败**。

### 6.6 复盘

合服失败必须在 24h 内出复盘文档,提交到 `docs/design/server-merge-postmortem-<date>.md`。模板参考 [`stress-test-2026-05-ephemeral-port.md`](../design/stress-test-2026-05-ephemeral-port.md)。

---

## 7. T+1 ~ T+7 观察期

### 7.1 监控指标

每天检查:

- [ ] 玩家投诉(客服工单)— 重点关注"我的东西不见了 / 改名 / 邮件丢失"
- [ ] `force_rename_required` 触发率(如果客户端 UI 已就绪)
- [ ] data_service rollback metric — 单玩家回档请求是否飙升
- [ ] `merge-zone-audit -VerifyMerged` 跑一遍,确认数据持续一致

### 7.2 客服快速通道

为合服 ops 期间建立专属 escalation:

- 客服工单标"merge-<src>-<dst>" tag
- 主程值班 4h 响应 SLA
- 单玩家数据问题用 `RollbackPlayer` RPC 修(见 [`single_player_rollback.md`](../design/single_player_rollback.md))

### 7.3 T+7:清理 source zone

```powershell
# 7 天观察期没 escalation 后,清 namespace
kubectl delete namespace mmorpg-zone-<SRC>

# 备份保留 90 天(归档到 S3-compatible,不走自动 prune)
# (具体归档命令依你的对象存储实现)
```

---

## 8. 已知限制(2026-05-23)

| 限制 | 影响 | 应对 |
|---|---|---|
| **昵称自动检测未实现** | 重名玩家合服后没自动 stamp force_rename,客户端不会弹改名 UI | §4.4 走人工方案 A(公告 + 自动加后缀);自动检测要等 merge_zone 引 protobuf 解 player_database blob |
| **`mail` / `auction` / `chat_history` / `guild_application` 表都不存在** | audit 工具不会扫这些(也无须扫) | 等对应 Go 服务上线再补 audit;当前合服不涉及这些数据 |
| **聊天历史不迁移** | 即便未来 chat 服务上线,合服时历史也不带过去 | 公告告知;未来若需迁移,实现专项工具 |
| **`force_rename_required` flag 闭环靠客户端** | 服务端字段 + Redis flag + login 读取已就位,但客户端 UI 还没接 | 客户端配合后即可生效 |
| **撤回合服不支持** | 合服 apply 后只能从备份恢复,不能"逻辑撤回" | 备份是唯一保障,务必 §5 Step 3 完整 |
| **player blob 跨 Redis cluster 拷贝是流式** | 大 zone(>10w 玩家)拷贝时间分钟级 | 选 source/target 同 Redis cluster 部署可跳过此步 |

---

## 9. 附录:命令快参

| 操作 | 命令 |
|---|---|
| dry-run | `dev_tools.ps1 -Command merge-zone -MergeSourceZone <s> -MergeTargetZone <d> -DryRun` |
| apply | `dev_tools.ps1 -Command merge-zone -MergeSourceZone <s> -MergeTargetZone <d>` |
| 资源审计 | `dev_tools.ps1 -Command merge-zone-audit -MergeSourceZone <s> -MergeTargetZone <d>` |
| 合服后验证 | `dev_tools.ps1 -Command merge-zone-audit -VerifyMerged ...` |
| zone-down | `dev_tools.ps1 -Command k8s-zone-down -ZoneName <name>` |
| zone-up | `dev_tools.ps1 -Command k8s-zone-up -ZoneName <name> -ZoneId <id> -WaitReady` |
| 触发 MySQL 备份 | `kubectl create job --from=cronjob/mysql-backup mysql-backup-manual-$(date +%s) -n <ns>` |
| 灾难回滚整个 zone | `tools/scripts/k8s_zone_rollback.ps1 -ZoneName <n> -ZoneId <i> -TargetTime <iso>` |
| 单玩家回档 | TODO: `dev_tools.ps1 -Command rollback-player -PlayerId <id> -TargetTime <iso>`(P2-A) |

---

## 修订历史

- **2026-05-23 v1**: 初版。基于 `tools/merge_zone/` 当前实现 + `server-merge-gap-fixes.md` 设计。`merge-zone-audit` 命令本文档发布时**与 audit_resources.go 同步落地**(详见任务 #5)。`force_rename_required` 字段同步落地但客户端 UI 暂缺(详见 §4.4 / §8)。
- **2026-05-23 v1.1**: 现状对齐。发现项目里 `mail` / `auction` / `chat_history` / `guild_application` / `player(name, zone_id)` 等假设表**全部不存在**;之前的 audit 工具有一半 auditor 在静默空跑。改:
  - audit_resources.go 删 5 个空跑 auditor(mail / auction / chat / guild_application / player_to_account 探针),保留 4 个真有用的(friend / friend_request / guild_member / online_keys)
  - audit_resources.go 加 `auditPlayerNameConflicts` 显式 block 级"未实现"提示,让人看到必须人工处理而非误以为 OK
  - main.go 删 `checkPlayerNameConflicts` 的 SQL 探测,改打印明确"未实现"
  - data_consistency_check 删 `checkMailRecipientOrphans`(也是空跑)
  - §4.2 / §4.4 / §8 改为反映真实情况
