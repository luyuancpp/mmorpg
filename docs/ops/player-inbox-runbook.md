# 玩家持久收件箱(player_inbox)运维手册

> **状态**:v1 — 2026-10-15,随设计第 4 批「运维、部署与对账」落码,**未编译、未测试、未在任何环境演练**,待 Codex 验证(§15)。
> 服务本体 `go/player_inbox`(第 2 批)与 scene 消费端(第 3 批)以各自落码为准;本手册里凡是写「以服务实现为准」的地方,落码后要回来核对一次。
> **依据**:`docs/design/player-inbox-service.md`(下称「设计」)v1.0,尤其 §1.4、§1.6、§7.8、§8、§9、§11.7;
> 计数器倒退的通用处置见 `docs/design/db-write-fencing-mysql.md` §9.1。
> **部署接线**:`deploy/k8s/README.md`「玩家收件箱 player-inbox」;告警 `deploy/k8s/player-inbox-alerts.yaml`;合服审计见 `docs/ops/merge-zone-runbook.md`。

---

## 1. 一览

| 项 | 值 | 出处 |
|---|---|---|
| 进程 | `go/player_inbox`,全服一份、多副本(K8s 在 `mmorpg-infra`,`Global = $true`) | 设计 §1.4(D3) |
| 端口 | gRPC 50950、metrics 9270(本地 `-Zone 2` 位移为 51950 / 10270) | D14 |
| 服务 | `player_inbox.PlayerInbox`(scene 调 `FetchItems` / `AckItems`)、`player_inbox.PlayerInboxAdmin`(运维) | `proto/player_inbox/` |
| 发现 | etcd 前缀 `PlayerInboxNodeService.rpc`(节点类型 30);自己的 go-zero `Etcd.Key` 为空(D-13) | `bin/etc/base_deploy_config.yaml` |
| C++ deadline | `GrpcClient.CallDeadlineMs.PlayerInboxNodeService = 6000`,部署门禁核对 ≥ 服务 `Timeout` + 2000 | 同上 |
| 库 | 独占库 `mmorpg_player_inbox`:`player_inbox_stream` / `player_inbox_item` / `player_inbox_audit` | `deploy/mysql-init/00_init_zone_dbs.sql` |
| 消费端账本 | 各 zone 库 `player_database.inbox_ledger`(字段 20),与 currency 同一次存盘 | 设计 §4.4 |
| 运维口令 | Secret `player-inbox-admin-token` → 容器 `PLAYER_INBOX_ADMIN_TOKEN`;ConfigMap 只写 `PlayerInbox.AdminTokenEnv`(**Go 侧须认这个键**,见下) | `k8s_deploy.ps1` |
| 入箱开关 | 服务 yaml `PlayerInbox.EnqueueTypes`(空 = 停用入箱) | 设计 §3.3 |
| 消费开关 | scene `game_config.yaml` 的 `player_inbox.consumer_enabled`;K8s 由 `k8s_deploy.ps1 -PlayerInboxConsumer on|off` 渲染 | 设计 §11.5、§11.7 |
| 对账读口 | data_service `GetPlayerInboxLedger`(读 zone Redis 的 `PlayerAllData` blob,不读 MySQL) | 设计 §5.1、§9.3 |

**部署侧与服务配置的键名契约**(K8s ConfigMap 由 `k8s_deploy.ps1` 生成,键名必须与 `go/player_inbox/internal/config` 逐字一致):
`Name` / `ListenOn` / `Timeout` / `Mode` / `Etcd{Hosts, Key:""}` / `ZoneId` / `LeaseTTL` / `MetricsListenAddr` / `KillSwitchPrefix` /
`MySQL{Host, User, Password, DBName, MaxOpenConn, MaxIdleConn}` / `Schema.AutoMigrate` / `CoreRedis{Host, Password, DB}` /
`Kafka{Brokers, WriteTimeoutMs}` / `DataServiceRpc` / `PlayerInbox{AdminTokenEnv, EnqueueTypes, TerminalRetentionDays, SweepIntervalSeconds,
NotifyResendIntervalSeconds, GaugeIntervalSeconds, ReconcileIntervalMinutes, ReconcileWindowHours}`。
其中 `PlayerInbox.AdminTokenEnv`(口令所在的环境变量名;非空时从该变量取口令,与字面 `AdminToken` 二选一,二者都非空应拒启)
**需要服务侧支持**:2026-10-15 的第 2 批配置只有字面 `AdminToken`。补上之前,带口令的部署会在写操作之前被拒,不带口令时运维接口停用。
仓库里的 `etc/player_inbox.yaml` 永远不带口令值。

**它不是什么**:不是邮件、不是资产通道。v1 只有一个域 `SCENE_GM_SANCTION`,三种类型:
`GM_BLOCK_CURRENCY_GAIN` / `GM_UNBLOCK_CURRENCY_GAIN` / `GM_ATTACH_CURRENCY_DEBT`。收件箱不可用不阻断进场(IB-7):scene 只退避。

---

## 2. 上线顺序与回滚下限(设计 §11.7)

**严格按顺序**,每一步验收通过再进下一步。任一步失败就停在那一步,不要跳。

| 步 | 动作 | 验收 |
|---|---|---|
| 1 | 每个 zone 库跑 go/db 的 `cmd/migrate`(先 `-command plan` 再 `-command up`),加 `inbox_ledger` 列;收件箱库与表就位(K8s:`infra-up` 的 `player-inbox-migrate` Job;本地:启动期 AutoMigrate 或 `player_inbox -f etc/player_inbox.yaml -migrate`);TiDB 上做 §12 的索引预切 | `plan` 只出现 `inbox_ledger`(以及已放行的 `fence_*`)的 ADD COLUMN;`SHOW COLUMNS FROM zone_<N>_db.player_database` 末列是 `inbox_ledger`;`kubectl -n mmorpg-infra get job player-inbox-migrate` 为 1/1 |
| 2 | 全部 zone 的 go/db 升到认识新字段的版本 | `kubectl get deploy db -o jsonpath='{..image}'` 各 zone 一致 |
| 3 | 部署收件箱服务,`EnqueueTypes` **为空** | `player-inbox` 两副本 Ready;etcd 有 `PlayerInboxNodeService.rpc/...` 键;`GmSanctionPlayer` 返回 `kPlayerInboxDisabled` |
| 4 | 全部 scene 升级,`consumer_enabled=false`(K8s:`zone-up` 不带 `-PlayerInboxConsumer`) | **这就是回滚下限**。存档能原样往返 `inbox_ledger`,scene 不发 Fetch(收件箱 `player_inbox_fetch_total` 不涨) |
| 5 | 全部 scene 打开 `consumer_enabled`(K8s:逐 zone `zone-up -PlayerInboxConsumer on`,然后滚动 scene;ConfigMap 变了不会自己重启进程) | `player_inbox_fetch_total{path="fast"}` 随登录上涨,`fence` 以 `NO_STREAM` 为主 |
| 6 | 全部收件箱实例打开 `EnqueueTypes`:改服务 yaml 的白名单(只能是 `PLAYER_INBOX_MSG_*` 枚举名)并重跑 `infra-up` | 生成期打印的 ConfigMap 里 `EnqueueTypes` 有值;一次测试处置能入箱并在测试账号上生效 |

**回滚**:回退到下限(第 4 步的版本)以上的任意版本都安全。**低于下限**(scene 或 go/db 回到不认识 `inbox_ledger` 的版本)是破坏性变更,按 §11 的前置步骤执行。

**为什么第 4、5 步要分开**:scene 旧版本按组件重建存档会丢掉新字段;go/db 旧版本按主键只更新已知列,账本列会停在旧值(违反 IB-3)。先让全部 scene / go/db 都「认识并原样回写」,再开消费,任何时刻回退都不会丢账本。

---

## 3. 运维接口的调用方式

所有 `PlayerInboxAdmin` 方法都要 metadata `x-admin-token`。口令为空 = 整个运维面停用(返回 `PermissionDenied` 或停用错误,以服务实现为准),**这不是故障**。

```powershell
# K8s:从运维机转发(不要把服务暴露到集群外)
kubectl -n mmorpg-infra port-forward svc/player-inbox 50950:50950

# 口令只放进当前进程环境,不要写进命令历史文件或工单
$env:PLAYER_INBOX_ADMIN_TOKEN = '<从密钥库取>'

# 查询某玩家各域概况(含 max_fence_epoch)
grpcurl -plaintext -import-path . -proto proto/player_inbox/player_inbox_admin.proto `
  -H "x-admin-token: $env:PLAYER_INBOX_ADMIN_TOKEN" `
  -d '{"player_id": 123456}' 127.0.0.1:50950 player_inbox.PlayerInboxAdmin/GetSummary
```

- 在仓库根目录执行(proto 的 import 路径以仓库根为准)。
- 其它方法同法:`ListItems`(`player_id` + `domain` + 可选 `state`,`after_seq` 翻页,`limit ≤ 200`)、`LookupByRequest`、`VoidItem`、`ReconcilePlayer`、`GmSanctionPlayer`。
- 枚举在 JSON 里可写名字,例如 `"domain": "PLAYER_INBOX_DOMAIN_SCENE_GM_SANCTION"`、`"state": "PLAYER_INBOX_ITEM_PENDING"`。
- 所有响应的业务错误在 `error_message`(tip 段 29000,`kPlayerInbox*`);只有 `kPlayerInboxStoreUnavailable` 是服务端故障。

**口令轮换**:换环境变量 `MMORPG_PLAYER_INBOX_ADMIN_TOKEN` 后重跑 `infra-up`。Pod 模板上的口令指纹会变,Deployment 自动滚动,旧口令随旧 Pod 退场。
不注入口令(任何档位)= 运维接口停用;注入了则 staging / prod 的占位口令、短于 32 位,任何档位短于 16 字节或与其它口令相同,部署在写操作之前被拒。
**停用运维面**:不带 `MMORPG_PLAYER_INBOX_ADMIN_TOKEN` 重跑 `infra-up`,残留 Secret 会在新 Deployment 之后删除。

---

## 4. GM 处置与批量处置脚本规范

### 4.1 单条处置的生产方契约(设计 §6.2)

- `request_id` 与 `issued_at_ms` **按工单只生成一次**,先记进工单再发第一次请求;**重试原样带回两者**。
- 刷新 `issued_at_ms` 重试会得到 `kPlayerInboxIdempotencyConflict`:这是保护,不要「换个时间再试」。
- `issued_at_ms` 只接受 `[now−7d, now+5min]`;超过 7 天的工单不能重放,只能用新的 `request_id` 重新下一条。
- 响应 `created=false` = 同键同内容已存在(DUPLICATE),返回原 `seq` 与当前状态,**算成功**。
- `operator` 必填(≤ 64B),`reason` 必填(≤ 256B);`attach_debt.gm_operator` 由服务端用 `operator` 覆盖。
- 处置是按 seq 严格顺序执行的。**要改主意,下一条新的处置**(新 `request_id`):例如封锁之后要解封,就下一条解封;
  v1 没有「人工补发」,把旧条目挪到队尾会颠倒封锁与解封的先后(设计 §7.9)。

### 4.2 批量处置脚本(限速与重试)

批量脚本(按名单对很多玩家下同一类处置)必须满足下面全部规则,评审时逐条核对:

1. **先落盘名单与键**:为每个玩家生成 `(player_id, request_id, issued_at_ms)` 写进本地 CSV / 工单附件,**写完才开始发请求**;
   脚本中断后从这份文件续跑,不重新生成任何键。`request_id` 用 64 位随机数或工单号派生,不能用自增计数器(两次跑会撞)。
2. **限速**:单个脚本并发 ≤ 4、总速率 ≤ 20 次/秒(GM 域按小量设计,入箱持流行锁;值待压测确认)。多个运维同时跑脚本时总速率合计不超过这个数。
3. **只重试可重试的失败**:gRPC `Unavailable` / `DeadlineExceeded`,以及 `kPlayerInboxStoreUnavailable`。
   指数退避(起 200ms、封顶 5s、±20% 抖动),**单条最多 5 次、单条总预算 30s**;超出就把这一条标为「未完成」写回 CSV,继续下一条,不无限重试。
4. **不重试、直接记录并跳过**:`kPlayerInboxPlayerNotFound`、`kPlayerInboxPayloadInvalid`、`kPlayerInboxPayloadTooLarge`、
   `kPlayerInboxTextTooLong`、`kPlayerInboxIssuedOutOfWindow`、`kPlayerInboxUnknownType`、`kPlayerInboxIdempotencyConflict`、`kPlayerInboxFull`。
   `Conflict` 说明名单或脚本有缺陷,**停下整批**查清再继续;`Full` 说明该玩家未决条目已满(`max_pending` 64),先查这名玩家为什么不消费。
5. **`kPlayerInboxDisabled` 立刻停整批**:白名单没开或被关了,不是这条的问题。
6. **结束时出对账表**:成功(`created` / DUPLICATE)、跳过(按错误码分组)、未完成三类计数,附在工单上;未完成的按第 1 条续跑。
7. 脚本只打印 `player_id` 与错误码,**不打印口令**,不把口令写进 CSV。

---

## 5. 告警处置(`deploy/k8s/player-inbox-alerts.yaml`)

| 告警 | 级别 | 含义 | 处置 |
|---|---|---|---|
| `PlayerInboxFenceRegressed` | critical | 判 STALE 且请求 epoch 等于 Redis 当前值:核心 Redis 的 `owner_epoch` 倒退了 | §7 |
| `PlayerInboxEpochAhead` | critical | 请求 epoch 超过 Redis 当前值(只会来自缺陷或伪造),已拒绝、未改状态 | 按日志里的 `scene_node_id` 找发送方,核对版本与 epoch 来源;**不要抬任何计数器** |
| `PlayerInboxCursorAhead` | critical | 消费方账本水位 ≥ 收件箱 `next_seq`:收件箱库被恢复过或流行被删 | §8 |
| `PlayerInboxAckAnomaly` | critical | Ack 出现 MISMATCH / CONFLICT / NOT_DELIVERED / SKIP_NOT_REQUESTED / UNKNOWN_SEQ | §5.1 |
| `PlayerInboxReconcileMismatch` | critical | 每小时对账发现「已终结但账本没到」或「仍 PENDING 但账本已处理」 | §10 |
| `PlayerInboxDeliveredUnackedTooOld` | warning | 已投递未确认的最老条目 > 1 小时 | §6 |
| `PlayerInboxPoisonSuspect` | warning | 某条目投递 ≥ 5 次仍未终结 | §6 |
| `PlayerInboxNotifyBacklog` | warning | 在线提醒到点未发 > 5 分钟 | 查 Kafka 写入(`player_inbox_notify_total{result}`)与领导者选举;提醒只是加速,条目不丢 |
| `PlayerInboxSqlPoolSaturated` | warning | 某副本独占库连接池在用 > 90% 持续 15 分钟(`go_sql_*{db_name="player_inbox"}`) | 看 `go_sql_wait_duration_seconds_total` 与 `fetch_total{path="locked"}`;调大池要同步 MySQL 连接总账与 `max_connections` |
| `PlayerInboxSettledUnknown` | warning | UNKNOWN 报告以 SETTLED_UNKNOWN 收尾 | 对照 scene 的 `[PlayerInbox] applied` 日志与玩家存档人工核实,必要时用新 `request_id` 重下 |
| scene 日志 `metric=inbox_unknown_type` | warning | 新类型先于 scene 支持被加进白名单,scene 停在队头不记 REJECTED | 立刻把该类型移出 `EnqueueTypes`(重跑 `infra-up`),或先把 scene 升到支持它的版本 |
| scene 日志 `metric=inbox_halted` | warning | 该玩家该域被停用(MISMATCH 等之后) | 同 §5.1;停用随实体销毁解除,下次登录重拉 |

### 5.1 Ack 异常结果

- **CONFLICT**:终态行与 scene 报告的结局不同。**永不翻转**(IB-4)。保留现场:`ListItems` 导出该行、对照 scene 日志里同 seq 的 `[PlayerInbox]` 行与玩家存档,
  判断哪边是真;需要补偿就下一条新处置。不要改库。
- **MISMATCH**:同 seq 的 `request_id` 不一致(IB-9)。最常见于收件箱库恢复之后(§8):恢复回来的旧行与消费方账本记的不是同一条。按 §8 处理。
- **NOT_DELIVERED / SKIP_NOT_REQUESTED / UNKNOWN_SEQ**:scene 报告了从未投递给它的行、或在没有作废指令时报 SKIPPED、或报告了不存在的 seq。
  只会来自 scene 缺陷或内网伪造(设计 §1.6)。按日志定位 scene 节点与版本;状态没改,不需要回滚任何东西。

---

## 6. 卡住与毒消息(设计 §7.9、D11)

v1 **不做自动隔离**。判断与处置:

1. `ListItems`(`state=PENDING`)找出 `delivery_count` 高、`last_delivered_ms` 老的行;记下 `domain` / `seq` / `request_id` / `msg_type`。
2. 对照 scene 日志(`[PlayerInbox]`,按 player_id 搜)看是哪种:
   - **反复投递、每次都没确认,同时 scene 有崩溃 / 重启**:疑似毒消息(载荷让 scene 崩溃)。
   - **scene 一直判 `inbox_deferred`**(冻结 / 交接 / 战斗中 / epoch 为 0):不是毒消息,等条件解除;战斗中的处置在解冻时立即重查。
   - **存盘失败导致一直不 Ack**:看 scene / go/db 的存盘告警,先修存盘。
3. 确认是毒消息后,`VoidItem`(`reason=PLAYER_INBOX_VOID_QUARANTINED`,`operator` 写自己,`note` 写工单号):
   - 从未投递过(`delivery_count=0`):直接 VOIDED。
   - 已投递:只挂 VOID 指令,scene 下次拉到时**不解码载荷**就报 SKIPPED,不会再触发崩溃。
4. 隔离之后要不要重下一条修正过的处置,由负责人定;重下一律用新 `request_id`。
5. 作废限速与批量脚本同(§4.2 第 2、3 条)。

---

## 7. owner_epoch 计数器倒退(设计 §7.8、§8 #14;围栏设计 §9.1)

**症状**:`PlayerInboxFenceRegressed` critical;某玩家进场后收件箱一直判 STALE,GM 处置不生效。通常同时有 go/db 的 `DbOwnerEpochRegressed` / 存盘被拒告警。

**原则**:**定真源、抬计数器、不归零**。收件箱**没有**归零围栏的接口,也不要直接改库把 `fence_epoch` 调小:僵尸 scene 会把围栏重新推回旧的高值。

**步骤**(按围栏设计 §9.1 a–f,本节只补收件箱这一项):

1. **定真源**、**踢下线**并确认 `player:{id}:location` 不存在(§9.1 a、b)。
2. **取收件箱的高水位**:`GetSummary(player_id)` 的 `max_fence_epoch`(各域流 `fence_epoch` 的最大值)。
3. **抬计数器**:`C' = max(当前计数器, 各表各落点库的 fence_owner_epoch, 各表各 topic 世代的 applied_epoch, 收件箱 max_fence_epoch) + 余量 K`。
   - `epoch-raise` 入口(围栏设计 §9.1「入口」,go/db 命令行)**尚未落码**;接入收件箱这一来源的方式二选一,与它的 §14 第 5 条一起定
     (工具直接读 `mmorpg_player_inbox`,或工具加 `-floor` 参数、由本步骤先调 `GetSummary` 再传入)。
   - 入口落地之前**不要**手写 Lua 去改计数器;按围栏设计的人工流程由负责人执行,`max_fence_epoch` 作为取最大值的一个输入写进工单。
4. 缓存与死信按 §9.1 d、e。
5. **验证**:玩家下一次进场拿到 `C'+1`,大于收件箱围栏,判 ADVANCE;`GetSummary` 的 `fence_epoch` 变为新值,积压的处置开始生效。

---

## 8. 收件箱库恢复(设计 §8 #18)

`mmorpg_player_inbox` 在开发 MySQL 上由 `mysql-backup-cronjob.yaml` 的 `mysqldump --all-databases` 覆盖;迁到 TiDB 后要另纳入 BR / PITR(未做)。

**恢复后系统自己会做的**:
- 新 seq 取 `max(next_seq, now_ms << 12)`,带时间下界,一定大于恢复前已分配的号,不会复用;
- 消费方水位领先 `next_seq` 时,Fetch 记 CursorAhead 并在同一事务里把 `next_seq` 抬到水位之上(告警 `PlayerInboxCursorAhead`);
- 恢复回来、已被消费方处理过的旧行,scene 按 IB-9 核对 `(seq, request_id, msg_type)`:一致就回放原结局确认,在环外就以 UNKNOWN 收尾,不一致就 MISMATCH 并停用该玩家该域。

**要人工做的**:
1. 记下恢复点时刻 T0 与恢复完成时刻 T1。
2. **恢复窗口内丢失的条目**(T0 之后入箱、恢复后不存在):对照运维侧工单记录(§4.2 第 1 条的 CSV)与 scene 的 `[PlayerInbox] applied` 日志,逐条判断「是否已经在玩家身上生效」。
   - 已生效:不用补;
   - 未生效、仍要执行:**用新的 `request_id` 重下**。**不能按原 `request_id` 重放**:原条目若已在某个 scene 上应用过,账本按 seq 去重,用原键重放会得到新 seq、被再执行一次。
3. **MISMATCH 停用的玩家**:逐个 `ReconcilePlayer` 看差异;确认恢复回来的旧行与账本记录的不是同一条后,对旧行 `VoidItem(QUARANTINED)`,玩家下次登录恢复正常。
4. 恢复期间收件箱不可用不影响进场(IB-7),不需要停服。

---

## 9. 回档预检(设计 §8 #16、D7)

玩家回档会把账本(`inbox_ledger`)连同货币一起退回快照时刻,而收件箱里那段时间已终结的 APPLIED 行不会再投递 —— 回档之后,那些处置在玩家身上「消失」了。

- **现状**:单人回档的栅栏未接线,`RollbackPlayer` 目前一律返回 NotImplemented(`go/data_service/internal/logic/rollback_logic.go`)。
  预检的自动化与回档接线同批做;在那之前,任何回档(含整区回档)都按下面的人工步骤预检。
- **人工预检**:
  1. 取快照里该玩家 `player_database.inbox_ledger` 各域的 `decided_through_seq`(快照数据的取法见 `docs/ops/online-debug-data-fetch.md`)。
  2. `ListItems(state=PLAYER_INBOX_ITEM_APPLIED)`,列出 seq **大于**快照水位的行 —— 这些就是回档后会「消失」的处置。
  3. 差异清单写进回档工单,负责人**显式接受并写明原因**才继续回档。
  4. 回档完成后,对清单里仍要执行的处置**用新 `request_id` 重下**(§4.1)。
- 整区回档时,这一步对全区玩家逐个做不现实:先用对账作业(§10)的结果圈出受影响的玩家,只对他们做。

---

## 10. 对账不一致(设计 §9.3)

- **定时作业**:领导者每小时一轮,取最近 24 小时由消费方终结的行,经 data_service `GetPlayerInboxLedger` 读已落盘账本,检查 `decided_through ≥ seq`。
  不满足记 `player_inbox_reconcile_mismatch_total` 并打一行带 player_id 的 ERROR;读不到存档(`found=false`)计 unknown、不告警。
- **单人复核**:`ReconcilePlayer(player_id)`,差异两类:
  - `FINAL_NOT_IN_LEDGER`(收件箱已终态、账本没到):多半是回档或 Redis 丢了最后一段(AOF 丢尾)。处置效果已随数据回退 → 核实后用新 `request_id` 重下。
  - `PENDING_BUT_DECIDED`(收件箱仍 PENDING、账本已处理):确认丢了且玩家一直离线。玩家下次登录会补确认;长期离线的不用处理。
- `ledger_found=false`:该玩家在 zone Redis 里没有存档 blob(从未进场或 Redis 丢数据),结论为 unknown,不比对。
- data_service 侧:`data_service_player_inbox_ledger_read_total{result="error"}` 非零看 data_service 同一时刻的 `[player-inbox-ledger]` ERROR 日志
  (home_zone 查不到回 Unavailable、Redis 读失败 / blob 损坏 / player_id 不符回 Internal,**绝不**伪装成 found=false)。

---

## 11. 回滚下限与破坏性降级(设计 §8 #21、§11.7)

- **回滚下限** = 认识 `inbox_ledger` 的 scene 与 go/db 版本(上线第 4 步那一版)。回退到下限以上任意版本都安全,照常 `rollout undo` / 重跑旧 commit 的部署。
- **低于下限的降级**(scene 或 go/db 回到不认识新字段的版本)会丢账本或让账本停在旧值,再升回来时已应用、未确认的 `ATTACH_DEBT` 会被**重复执行**。必须先做:
  1. **关入箱**:把 `EnqueueTypes` 置空并重跑 `infra-up`(确认 `GmSanctionPlayer` 返回 `kPlayerInboxDisabled`)。
  2. **关消费**:全部 zone `zone-up`(不带 `-PlayerInboxConsumer`,即 off)并滚动 scene;确认 `player_inbox_fetch_total` 不再增长。
  3. **隔离已投递未确认的行**:对每个域 `ListItems(state=PENDING)`,把 `delivery_count > 0` 的行逐条 `VoidItem(reason=QUARANTINED, note=<降级工单号>)`,
     按 §4.2 的限速执行,结果清单附进工单。
  4. 执行降级。
  5. **升级回来之后**:先回到第 4 步的版本(`consumer_enabled=false`),逐条按第 3 步清单对照玩家存档核实,确认无误后再按 §2 的第 5、6 步重新打开。
- 降级窗口里收件箱服务本身可以照常运行(只是没有生产也没有消费)。

---

## 12. TiDB 索引预切(设计 §4.2,D8)

`PRE_SPLIT_REGIONS` 只预切**行数据**;主键(非聚簇时就是一个唯一索引)、唯一键与二级索引建表时各只有 1 个 Region,
写入尾部(按 64 个 shard 打散)在首日仍压在同一个 Region 上。player_route 为此有 `route_tool tidb-prepare`,但那是 data_service 自己的工具
(读 data_service 的配置、只认它的两张表),**收件箱没有现成的命令**;这里给手工 SQL,MySQL 上**不要执行**(MySQL 没有 Region)。

何时做:TiDB 上建表之后、开服之前(低峰);BR / PITR 恢复之后、TRUNCATE 之后重做一次。

```sql
-- 0) 先核对索引名(proto2mysql 命名:普通索引 idx_<表名>_<序号>,唯一键 uk_<表名>;以 SHOW INDEX 的实际结果为准)
SHOW INDEX FROM mmorpg_player_inbox.player_inbox_stream;
SHOW INDEX FROM mmorpg_player_inbox.player_inbox_item;
SHOW INDEX FROM mmorpg_player_inbox.player_inbox_audit;

-- 1) 预切出来的空 Region 不许被合并回去(与 route_tool tidb-prepare 同一做法)
ALTER TABLE mmorpg_player_inbox.player_inbox_stream ATTRIBUTES 'merge_option=deny';
ALTER TABLE mmorpg_player_inbox.player_inbox_item   ATTRIBUTES 'merge_option=deny';
ALTER TABLE mmorpg_player_inbox.player_inbox_audit  ATTRIBUTES 'merge_option=deny';

-- 2) 每条索引按 shard(0..63)每 4 片切一刀 → 16 段。所有键都以 shard 开头,切点只给 shard,其余列补 0。
--    stream:PRIMARY(shard, player_id, domain)、idx_player_inbox_stream_1(shard, notify_due_ms)
SPLIT TABLE mmorpg_player_inbox.player_inbox_stream INDEX `PRIMARY`
  BY (4,0,0),(8,0,0),(12,0,0),(16,0,0),(20,0,0),(24,0,0),(28,0,0),(32,0,0),(36,0,0),(40,0,0),(44,0,0),(48,0,0),(52,0,0),(56,0,0),(60,0,0);
SPLIT TABLE mmorpg_player_inbox.player_inbox_stream INDEX `idx_player_inbox_stream_1`
  BY (4,0),(8,0),(12,0),(16,0),(20,0),(24,0),(28,0),(32,0),(36,0),(40,0),(44,0),(48,0),(52,0),(56,0),(60,0);
--    item:PRIMARY(shard, player_id, domain, seq)、uk_player_inbox_item(shard, player_id, producer_kind, request_id)、
--          idx_player_inbox_item_1(shard, state, updated_ms)
SPLIT TABLE mmorpg_player_inbox.player_inbox_item INDEX `PRIMARY`
  BY (4,0,0,0),(8,0,0,0),(12,0,0,0),(16,0,0,0),(20,0,0,0),(24,0,0,0),(28,0,0,0),(32,0,0,0),(36,0,0,0),(40,0,0,0),(44,0,0,0),(48,0,0,0),(52,0,0,0),(56,0,0,0),(60,0,0,0);
SPLIT TABLE mmorpg_player_inbox.player_inbox_item INDEX `uk_player_inbox_item`
  BY (4,0,0,0),(8,0,0,0),(12,0,0,0),(16,0,0,0),(20,0,0,0),(24,0,0,0),(28,0,0,0),(32,0,0,0),(36,0,0,0),(40,0,0,0),(44,0,0,0),(48,0,0,0),(52,0,0,0),(56,0,0,0),(60,0,0,0);
SPLIT TABLE mmorpg_player_inbox.player_inbox_item INDEX `idx_player_inbox_item_1`
  BY (4,0,0),(8,0,0),(12,0,0),(16,0,0),(20,0,0),(24,0,0),(28,0,0),(32,0,0),(36,0,0),(40,0,0),(44,0,0),(48,0,0),(52,0,0),(56,0,0),(60,0,0);
--    audit:PRIMARY(shard, player_id, domain, seq, action)
SPLIT TABLE mmorpg_player_inbox.player_inbox_audit INDEX `PRIMARY`
  BY (4,0,0,0,0),(8,0,0,0,0),(12,0,0,0,0),(16,0,0,0,0),(20,0,0,0,0),(24,0,0,0,0),(28,0,0,0,0),(32,0,0,0,0),(36,0,0,0,0),(40,0,0,0,0),(44,0,0,0,0),(48,0,0,0,0),(52,0,0,0,0),(56,0,0,0,0),(60,0,0,0,0);

-- 3) 核对:每条索引应有约 16 个 Region
SHOW TABLE mmorpg_player_inbox.player_inbox_item INDEX `uk_player_inbox_item` REGIONS;
```

- 切点密度(每 4 片一段)是按 GM 域「写入量小、读多」取的,未压测;落地拉取的快路径是流行的主键点读,压力主要在 `player_inbox_stream` 的 `PRIMARY`。
  指标显示热点时再加密切点。
- 行片模数 64 与 `placement.RouteRowShards` 同值但**常量独立**(设计 §4.2),这里的切点按 64 写。改模数 = 新建表,不在这里改。
- 以后若给 go/player_inbox 加 `tidb-prepare` 子命令(照 route_tool,MySQL 上自动跳过),本节改为调用它。

---

## 13. 合服

收件箱三张表按全局唯一的 player_id 存,**没有 zone 列,合服不搬不改**。`tools/merge_zone` 的合服前审计多了 `player_inbox` 一行:
- 源区或目标区 zone 库缺 `player_database.inbox_ledger` 列 → **block**:先对那个 zone 跑 go/db `cmd/migrate up`。否则拷贝玩家行时会以 SCHEMA MISMATCH 停在维护窗口里。
- 收件箱库 / 表查不到 → warn「NOT VERIFIED」(该环境多半没部署收件箱,合服本身不受影响);其余查询失败 → INFRA。
- 正常时 info,报 PENDING 条数。

---

## 14. 已知未完成 / 待确认

- **`epoch-raise` 入口**未落码,收件箱 `max_fence_epoch` 的接入方式待围栏会话与其 §14 第 5 条一起定(§7)。
- **回档预检自动化**随单人回档栅栏接线同批做(§9)。
- **TiDB 索引预切**只有手工 SQL(§12);收件箱库迁到 TiDB 后的 BR / PITR 未纳入备份。
- **robot 的 GM 处置冒烟场景**(设计 §11.6)未做。
- 告警规则依赖的指标名与 label 取值以 `go/player_inbox/internal/metrics` 的实现为准,落码后核对 `player-inbox-alerts.yaml` 文件头的「label 取值契约」。
- 批量处置脚本的限速值(§4.2)待压测确认。
- K8s 上的抓取前提(指向 9270 的 ServiceMonitor / PodMonitor)仓库里没有,与其它 go-svc 同一缺口。

---

## 15. 验证(交 Codex,未运行)

在仓库根目录:

```powershell
# 部署脚本契约(含连接总账、deadline 门禁、ConfigMap 形状、口令注入、迁移门禁、本地启动契约)
pwsh -NoProfile -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/k8s_migrate_gate.tests.ps1
pwsh -NoProfile -File tools/scripts/tests/start_game_command_contract.tests.ps1

# data_service 的 GetPlayerInboxLedger 与资产账本(共用读路径)
cd go/data_service; go vet ./... ; go test ./internal/logic/... ./internal/server/... -count=1

# 合服审计
cd tools/merge_zone; go vet ./... ; go test -run 'PlayerInbox|ZoneDBName|RunAuditMode' -count=1 .
```

通过标准:三个 `.tests.ps1` 的汇总 fail=0;Go 测试全绿。部署脚本的用例要求 `go/player_inbox/etc/player_inbox.yaml` 已由第 2 批落地且含本手册 §1 列出的键,
在那之前 deadline 门禁与 infra-up 相关用例会因「找不到文件 / 查不到键」失败 —— 这是预期的集成检查,不是脚本缺陷。
