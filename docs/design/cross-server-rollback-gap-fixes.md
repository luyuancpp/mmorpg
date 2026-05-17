# 跨服回档 — 差距修复方案

> **生成日期**: 2026-05-17
> **前置阅读**: [`cross-server-rollback-merge-audit.md`](./cross-server-rollback-merge-audit.md)
>
> **范围**: 不重写已有的 `RollbackPlayer` / `RollbackZone` 系统(已 394 行成熟代码)。
> 只补 2026-05-17 现状盘点里**真正没做完**的事:
> - **P0-E** MySQL 定时备份 + binlog 归档(`zone_data_rollback.md` §3 列为"高优先级缺失")
> - **P1-B** 跨服节点崩溃后的"未刷写状态"回滚
> - **P2-A** 跨 zone 主动 `RollbackPlayer`(玩家 home 在 zone X,在 zone Y 出问题)
> - **P2-C/D** Kafka offset 重置 + 一键 zone-rollback 脚本

---

## 一、P0-E: MySQL 定时备份 + binlog 归档

### 1.1 现状

`zone_data_rollback.md` §3 表格里**自己已经标了"高优先级缺失"**:

> "MySQL 定时备份 + binlog 归档 — K8s MySQL pod 未配置自动备份,需加 CronJob 或用云 RDS"

每 zone 是 K8s namespace `mmorpg-zone-{zoneName}`,各自跑一个 MySQL pod,**目前 binlog 在 pod 临死时就丢**。这意味着:

- "回档到 30 分钟前"在生产环境**不可行**(没 binlog 没 dump 没法 PITR)
- 现有的 `RollbackZone` 应用级回档**依赖 player_snapshot 表存在 + 每次登录/重大事件触发的快照**,如果快照本身被同一份 MySQL 实例丢了,就废了

### 1.2 决策

**两条路线,deployment 二选一**(不是工程问题,是运维选型):

#### 路线 A: 云 RDS(推荐生产)

- 启用云厂商 RDS 的 PITR(阿里云 RDS / AWS Aurora)
- 默认按天 dump + binlog 实时归档,7-30 天保留
- 控制台一键回档到任意时间点
- 工程师**只需要文档化操作**,不用写 CronJob

**这个项目目前没用 RDS** —— `db_zone_isolation.md` 暗示是自建 MySQL 跑在 K8s pod 里。

#### 路线 B: 自建 K8s MySQL + CronJob 备份

- 加一个 K8s `CronJob` 资源跑 `mysqldump` + binlog rotate
- 备份对象存储用 PVC + 定期同步到 S3-compatible 存储(MinIO 之类)

**P0-E 这一片我做出 K8s YAML 模板**(路线 B),**不替你决策选哪条** —— 选型由你/ops 拍板。

### 1.3 K8s CronJob 模板

```yaml
# deploy/k8s/zone-mysql-backup.cronjob.yaml
apiVersion: batch/v1
kind: CronJob
metadata:
  name: mysql-backup
  namespace: mmorpg-zone-${ZONE_NAME}
spec:
  schedule: "0 */6 * * *"  # 每 6 小时一次,可调
  successfulJobsHistoryLimit: 4
  failedJobsHistoryLimit: 4
  concurrencyPolicy: Forbid
  jobTemplate:
    spec:
      backoffLimit: 1
      template:
        spec:
          restartPolicy: OnFailure
          containers:
          - name: mysqldump
            image: mysql:8.0
            command:
            - /bin/sh
            - -c
            - |
              set -e
              TS=$(date -u +"%Y%m%d_%H%M%S")
              FILE="/backups/zone_${ZONE_ID}_${TS}.sql.gz"
              mysqldump \
                --host="${MYSQL_HOST}" \
                --user="${MYSQL_USER}" \
                --password="${MYSQL_PASSWORD}" \
                --single-transaction \
                --master-data=2 \
                --routines --triggers --events \
                "zone_${ZONE_ID}_db" | gzip > "${FILE}"
              # Retain only the last 28 backups locally; older
              # snapshots should already be synced offsite by the
              # backup-rotator sidecar (slice E-2 below).
              ls -1t /backups/zone_${ZONE_ID}_*.sql.gz | tail -n +29 | xargs -r rm -f
              echo "[backup] ${FILE} ($(du -h ${FILE} | cut -f1))"
            env:
            - name: ZONE_ID
              value: "${ZONE_ID}"
            - name: MYSQL_HOST
              value: "mysql.mmorpg-zone-${ZONE_NAME}.svc.cluster.local"
            - name: MYSQL_USER
              valueFrom:
                secretKeyRef:
                  name: mysql-backup-secret
                  key: user
            - name: MYSQL_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: mysql-backup-secret
                  key: password
            volumeMounts:
            - name: backups
              mountPath: /backups
          volumes:
          - name: backups
            persistentVolumeClaim:
              claimName: mysql-backup-pvc
```

**`${ZONE_NAME}` / `${ZONE_ID}` 占位符** — 必须由部署脚本(`tools/scripts/k8s_deploy.ps1`)填充。

### 1.4 后续 slice(本片不做)

- **E-2: 离站同步** — 一个 sidecar 把 PVC 里的 dump 推到 S3-compatible(MinIO/阿里 OSS)。不放在主 CronJob 里因为它需要 `aws-cli` 镜像 + bucket 凭证,不属于工程师"写一份 yaml 就完成"的范畴。
- **E-3: 备份验证** — 月度 chaos-style 拉一份 dump 还原到影子 MySQL,跑一遍 `pt-table-checksum`。

### 1.5 真正落地

```
deploy/k8s/zone-mysql-backup.cronjob.yaml      ← 本片,模板
tools/scripts/k8s_deploy.ps1                   ← 加 -Command k8s-zone-mysql-backup-up,把模板渲染并 apply
docs/ops/mysql-backup-runbook.md               ← 操作手册:第一次启用 / 验证 / 临时停用
```

我**没在这次落 K8s YAML 文件本身** — 因为 `deploy/k8s/` 目录里 namespace 命名约定要先和 ops 对齐(我看到的是 `scene-manager-alerts.yaml` 一份 alert YAML,但没看到完整的 zone-namespace 模板)。等你确认部署形态我再写 YAML 实文件。

---

## 二、P1-B: 跨服节点崩溃状态回滚

### 2.1 问题描述

**场景**: 玩家 P 的 home zone 是 zone-1。P 通过 SceneManager 切换到 zone-2 的某个副本进行跨服战斗。P 在 zone-2 战斗 5 分钟,期间打掉一个 boss、获得一件装备、消耗 100 金币。**zone-2 scene 节点进程突然崩溃**(SIGSEGV / OOM kill)。

期望: P 重连后,**之前 5 分钟的战斗结果应该不丢**(或者明确地告诉 P "因服务器异常,部分战斗未保存,请重新挑战")。

实际: zone-2 scene 节点崩溃 = 内存里所有未刷写的玩家状态丢失。`#280 layered defense` 在**进程正常退出**的情况下保护了 30s 重连窗口,但**进程崩溃时根本没机会调 SavePlayerToRedis**。

### 2.2 现状

- `single_player_rollback.md` 设计了快照系统,**默认快照频率是"登录 + 关键事件 + 维护前"**,**不是高频** — 战斗中典型快照间隔是数分钟到登录间。
- `transaction_log` 已经存在(详见 `single_player_rollback.md` §"Anti-Duplication"),**但只覆盖"双向操作"**(交易、邮件、公会银行、拍卖、组队分赃),不覆盖"打怪掉装备"这种纯加项。
- `#105 fatal-signal forensics`(`HandleFatalSignal` 在 `node.cpp`)记录了崩溃时间戳和堆栈,**没保存玩家数据**。

### 2.3 设计选择 — 三条路线对比

| 路线 | 工作量 | 数据完整度 | 性能成本 | 推荐度 |
|---|---|---|---|---|
| **B1**: 提高快照频率到每 30s | M | 中(30s 内丢) | 中(30s 一次全玩家序列化) | ⭐ |
| **B2**: 给所有"加项"操作也写 transaction_log | L | 高(回放粒度精确到操作) | 中(每次掉装备/经验加 1 次 Kafka 写) | ⭐⭐⭐ 推荐 |
| **B3**: 实时把每次 mutation 写 Redis(write-through) | XL | 完美 | 高(每次 HP 变化都 Redis 写) | ❌ 不推荐 — 抹掉了"Redis 是缓存层"的架构假设 |

### 2.4 推荐方案 — B2 扩展 transaction_log

**核心思想**: `transaction_log_system.h` 已经在记录 currency 和 item 的所有变更。把它**扩展到"所有 player 状态可恢复操作"**,包括:

- 装备获得 / 升级 / 强化
- 任务完成
- 关卡通关
- 等级提升
- 关键 buff 获取(永久 buff 或 30 分钟以上)

崩溃恢复时:
1. P 重连到任意 scene
2. Scene 调 `data_service::LoadPlayerData(P)` — 从 home zone Redis 拿到**最后一次成功 save 的状态**
3. Scene 调 `data_service::QueryTransactionLog(P, since=last_save_ms)` — 拿到从最后一次 save 以来的所有"加项"
4. Scene 在内存里 replay 这些 log,把战利品补回来
5. 最后一次状态(HP/Position/buff)如果在 30s lease 内就**还原 last save 之前的位置**,反正玩家会被传送回 home zone 重新进入跨服

### 2.5 落地步骤

| Slice | 范围 | 工作量 |
|---|---|---|
| **B2-1** | 扩展 `transaction_log` 协议加 op type: ITEM_AWARD / QUEST_COMPLETE / LEVEL_UP / BUFF_GAIN | S |
| **B2-2** | 在 cpp scene 的对应 system 里 hook 到 `transaction_log_system::Record` | M(每个 system 一次) |
| **B2-3** | `data_service::QueryTransactionLog` 接口加 `since_ms` 参数 + 按 op type 过滤 | S |
| **B2-4** | scene 端写一个 `PostCrashReplay(player_id)` 工具,在玩家重连时被调用 | M |
| **B2-5** | 配置:`replay_after_crash_enabled` env knob,默认 false 关闭(担心 replay bug 反而做得更糟) | S |
| **B2-6** | 压测 + 灰度:小规模灰度玩家 → 监控 `replay_count` / `replay_duration` / `replay_error` 三个 metric | L |

### 2.6 已知风险 — 我必须告诉你

- **跨服 replay 的物品复制问题**: 如果玩家 P 在 zone-2 战斗中把装备**邮寄给好友 Q**,zone-2 崩溃,P 走 replay 把装备恢复 — 那 Q 已经收到的装备 + P 复活的装备 = 复制。**必须依赖 transaction_log 的 anti-dup 机制**(`single_player_rollback.md` §"Anti-Duplication"),把"已转移"操作扣除 replay 范围。
- **回放幂等性**: replay 必须能多次执行得到同样结果。每个 op 必须有 unique tx_id 让 scene 检查"这个 op 我已经 apply 过没"。
- **回放时序**: 多个 op 的执行顺序必须和 origin 一致,否则可能出现"先扣 100 金币再奖励 200 金币" vs "先奖励再扣"的不同净结果(虽然在常规设计里两者交换律,但策划可能有"金币不能为负"这种约束)。

**这三个风险点,设计成本都不在 transaction_log 本身,而在 cpp 各 system 的 hook 是不是统一**。

### 2.7 这一片不做的事

- B2-2 的"每个 system hook"是**整片中最耗时的**,涉及战斗 / 任务 / 关卡 / 等级 / buff 等多个 module。属于**主程级别的工作分配**,不是我一两次会话能塞完的。本片只把**框架决策 + slice 拆分**记录下来。
- 客户端 SDK 的"提示用户"展示部分。

---

## 三、P2-A: 跨 zone 主动 RollbackPlayer

### 3.1 现状

`RollbackPlayer` RPC 在 `data_service` 暴露,但**调用者必须提供 player_id 而不指定 zone**。data_service 内部根据 `player_id → home_zone` 路由,从 home zone Redis 找 snapshot 恢复。

**这意味着 RollbackPlayer 已经天然是跨 zone 的** — 因为 player 数据本来就在 home zone,RollbackPlayer 走的是同样的路由。

### 3.2 真实差距

不是"还没做",而是**没文档化**和**没 GM 工具入口**。

需要的事:
- GM 控制台入口(可能是 Java 网关上的 admin 路由)
- `dev_tools.ps1 -Command rollback-player -PlayerId <id> -TargetTime <ts>` 命令(给 ops 命令行用)
- `docs/ops/rollback-player-runbook.md` 标准操作手册

**工作量 S** — 把已有 RPC 包成一层壳。

---

## 四、P2-C/D: Kafka offset 重置 + 一键 zone-rollback

### 4.1 现状

`zone_data_rollback.md` §2 已经写了 ops 操作流程,**但每一步是 ops 手动跑命令**:

```bash
kafka-consumer-groups.sh --reset-offsets --to-datetime ... --group ... --execute
```

错一个时间戳就完蛋。

### 4.2 应做

`tools/scripts/dev_tools.ps1` 加 `k8s-zone-rollback` 命令,把这 5 步包成原子操作:

1. `k8s-zone-down`(已存在)
2. MySQL PITR(调云 API 或本地 binlog)
3. `redis-cli FLUSHDB`
4. Kafka offset 重置(参数化时间戳)
5. `k8s-zone-up`(已存在)

**安全保护**:
- 必须 `-DryRun` 先跑一遍
- 必须 `-Apply -Confirm "I understand this is destructive"` 才真改
- 输出的 audit log 推到对象存储

工作量 S(把现成命令串成 PowerShell script)。

---

## 五、本片总结 — 我现在做什么 / 不做什么

### 这个 commit 里**只**有的:

- `cross-server-rollback-gap-fixes.md`(本文件)— 决策记录 + slice 拆分

### **不做**的事 + 原因:

- **不写 K8s CronJob YAML 实文件** — 需要先和 ops 对齐 namespace / secret / PVC 命名约定
- **不动 `transaction_log_system.h` 加新 op type** — 需要先和后端 / 策划过 op type 列表
- **不在 `dev_tools.ps1` 加新命令** — 没法本地跑 K8s 测试,改了也不知道对不对
- **不动 cpp scene 各 system 的 transaction_log hook** — 主程级别工作

### 推荐你下一步选

| 选项 | 工作量 | 价值 |
|---|---|---|
| **A. 本片就停** — 用作未来排期讨论的基础 | 0 | ⭐⭐⭐ 高 — 让你 review 后再分配工作 |
| **B. 我接着写 K8s CronJob YAML 实文件** | S | ⭐⭐ 中 — 能看到具体效果,但 ops 配置要现场调 |
| **C. 我接着写 `dev_tools.ps1 rollback-player` 包装** | S | ⭐⭐ 中 |

我建议 A —— 本片是**决策文档**,你 review 后再切下一步。

---

## 参考索引

- 已有: `single_player_rollback.md` / `zone_data_rollback.md` / `db_zone_isolation.md` / `cross_server_architecture_principle.md`
- 已有代码: `go/data_service/internal/logic/rollback_logic.go` / `go/data_service/internal/routing/router.go`
- 已有运维入口: `pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-{up,down}`
- 配套盘点: `cross-server-rollback-merge-audit.md`
