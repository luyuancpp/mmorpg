# MySQL Backup & PITR Runbook

> **状态**: v1 — 2026-05-15
> **范围**: K8s 部署的 MySQL(`deploy/k8s/manifests/infra/mysql.yaml` + `mysql-backup-cronjob.yaml`)的备份、归档、恢复 SOP。
> **关联文档**: `docs/design/zone_data_rollback.md` §3(灾难恢复级 zone 回档),`docs/design/server_merge_design.md` §5.1(合服前备份要求)。

---

## 1. 资产清单

| 资源 | 用途 | 关键路径 |
|---|---|---|
| `mysql-data-pvc`(20Gi RWO)| MySQL 数据 + binlog | `/var/lib/mysql/{data,binlog}` |
| `mysql-backup-pvc`(50Gi RWX)| 备份归档目标 | `/backup/{dump,binlog}` |
| `mysql-config` ConfigMap | 启用 binlog 的 my.cnf | mounted at `/etc/mysql/conf.d` |
| `mysql-backup` CronJob | 每天 03:17 UTC 跑一次 | `kubectl get cronjob mysql-backup` |

**保留策略**:dumps 30 天,binlog 14 天。改 `mysql-backup-cronjob.yaml` 的 `find -mtime` 数字调整。

---

## 2. 首次部署 / 升级现有集群

### 2.1 全新集群(2026-10-15 按当前部署入口修订)

先按 [K8s 开服手册](k8s-open-server-runbook_zh.md) 准备目标 context、kubeconfig、区服清单、不可变镜像和该环境的密钥注入。下面是只准备共享基础设施的示例:跳过全局 Go 服务并不代表完整游戏栈已经部署,`BattleReplicas=0` 也不会删除已有 battle 池。

当前 `mysql.yaml` 挂载 `mysql-init-sql` ConfigMap,它包含区服库与全局库初始化 SQL,由 `k8s_deploy.ps1 infra-up` 在 MySQL Deployment 之前生成。不能再只对 `mysql.yaml` 单独 apply。

**备份任务的凭据须先准备:** 仓库 `mysql-backup-cronjob.yaml` 的 `MYSQL_PASSWORD` 仍是开发环境的固定值,不会自动跟随 prod 注入或密码轮换。运维必须先在目标 namespace 创建匹配该数据库的凭据 Secret,准备 Git 外的审定清单副本,把 CronJob 的 `MYSQL_PASSWORD` 改为对应 `secretKeyRef` 并核对 `MYSQL_HOST / MYSQL_USER`、权限与 PVC 挂载。下方只 apply 这份已准备的清单,不能直接把仓库开发清单当成 prod 备份任务。

```bash
# 在仓库根目录;替换路径与镜像占位,密钥按开服手册从受控环境注入。
KUBE_CONTEXT='<目标 context>'
KUBE_CONFIG='<kubeconfig 绝对路径>'
INFRA_NS='mmorpg-infra'
NODE_IMAGE='<镜像仓库>/mmorpg-node:<不可变 tag>'
BACKUP_MANIFEST='<Git 外审定的 mysql-backup CronJob 清单绝对路径>'
pwsh -File tools/scripts/k8s_deploy.ps1 -Command infra-up \
  -KubeContext "$KUBE_CONTEXT" -KubeConfig "$KUBE_CONFIG" \
  -InfraNamespace "$INFRA_NS" -ReleaseProfile prod -OpsProfile managed-cloud \
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml -NodeImage "$NODE_IMAGE" \
  -BattleReplicas 0 -SkipGoSvc -WaitReady

# infra-up 不包含备份 CronJob;在同一 namespace 单独安装它。
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  get configmap mysql-init-sql
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  apply -f "$BACKUP_MANIFEST"

# 验证 binlog 已开
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  exec deploy/mysql -- sh -ec \
  'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?容器未注入 root 凭据}"; exec mysql -uroot "$@"' \
  mysql-client -e "SHOW VARIABLES LIKE 'log_bin';"
# 期望:Value=ON

# 手动触发一次备份验证 CronJob 工作
BACKUP_JOB="mysql-backup-smoke-$(date +%s)"
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  create job --from=cronjob/mysql-backup "$BACKUP_JOB"
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  logs "job/$BACKUP_JOB" -f
```

以下 Bash 示例沿用本节目标变量,命令均显式指定 context、kubeconfig 和 namespace。SQL 客户端在容器内读取 `MYSQL_ROOT_PASSWORD`,不把口令写进宿主命令、参数或日志;运行时凭据必须与数据库当前口令一致,轮换后须先同步环境 / Secret。容器不提供该变量时,交互检查可使用 `kubectl exec -it ... -- mysql -uroot -p` 手工输入,不要把口令拼进 `-p` 后。首次安装还须验证备份 PVC 可挂载并完成一次恢复检查。

`infra-up` 会处理共享 etcd / Redis / Kafka 等资源,并非只安装 MySQL。存量集群执行前必须先审查对应拓扑与迁移差异;`mysql-init-sql` 只在空数据目录首次启动时执行,不会自动修复存量库。

### 2.2 从老版本(emptyDir + 无 binlog)升级 — 数据迁移

```bash
# 1. 先 mysqldump 老数据到本地(不能丢)
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  exec deploy/mysql -- sh -ec \
  'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?容器未注入 root 凭据}"; exec mysqldump -uroot "$@"' \
  mysqldump-client --all-databases --single-transaction > pre-upgrade-dump.sql

# 2. 删老 Deployment(emptyDir 数据随 pod 销毁)
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  delete deployment mysql

# 3. 按上节同一参数调用 infra-up,先生成 mysql-init-sql,再创建 PVC 与 MySQL。
#    删除旧 emptyDir 实例前,必须已经验证步骤 1 的备份可恢复;
#    共享基础设施的其他变更也须已审查,不要只复制一条 apply mysql.yaml。
pwsh -File tools/scripts/k8s_deploy.ps1 -Command infra-up \
  -KubeContext "$KUBE_CONTEXT" -KubeConfig "$KUBE_CONFIG" \
  -InfraNamespace "$INFRA_NS" -ReleaseProfile prod -OpsProfile managed-cloud \
  -ZonesConfigPath deploy/k8s/zones.ops-recommended.yaml -NodeImage "$NODE_IMAGE" \
  -BattleReplicas 0 -SkipGoSvc -WaitReady

# 4. 等新 pod ready
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  rollout status deploy/mysql

# 5. 恢复数据
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  exec -i deploy/mysql -- sh -ec \
  'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?容器未注入 root 凭据}"; exec mysql -uroot' \
  < pre-upgrade-dump.sql

# 6. apply CronJob
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  apply -f "$BACKUP_MANIFEST"
```

---

## 3. 日常运维

### 3.1 检查备份是否在跑

```bash
# CronJob 上次成功时间
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  get cronjob mysql-backup -o jsonpath='{.status.lastSuccessfulTime}'

# 最近 7 天的 Job 历史
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  get jobs --sort-by=.metadata.creationTimestamp

# 备份卷里的实际文件
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  run -it --rm peek --image=busybox:1.36 --restart=Never \
  --overrides='{"spec":{"volumes":[{"name":"b","persistentVolumeClaim":{"claimName":"mysql-backup-pvc"}}],"containers":[{"name":"peek","image":"busybox:1.36","stdin":true,"tty":true,"volumeMounts":[{"name":"b","mountPath":"/backup"}]}]}}' \
  -- sh -c 'ls -lh /backup/dump /backup/binlog'
```

### 3.2 手动触发备份(合服 / 灾难前)

```bash
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  create job --from=cronjob/mysql-backup \
  mysql-backup-pre-$(date +%Y%m%d-%H%M)
```

### 3.3 取回备份文件到本地

```bash
# 用临时 pod 把文件 cp 出来
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  run dumpfetch --image=busybox:1.36 --restart=Never \
  --overrides='{"spec":{"volumes":[{"name":"b","persistentVolumeClaim":{"claimName":"mysql-backup-pvc"}}],"containers":[{"name":"c","image":"busybox:1.36","command":["sleep","3600"],"volumeMounts":[{"name":"b","mountPath":"/backup"}]}]}}'

kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  cp dumpfetch:/backup/dump/dump-20260515-0317.sql.gz ./dump-20260515.sql.gz
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  delete pod dumpfetch
```

---

## 4. 灾难恢复 / PITR(Point-in-Time Recovery)

### 4.1 完整时间点恢复(从 dump + binlog)

**场景**:数据被误删 / Bug 导致脏写,需要恢复到 `2026-05-15 14:23:00 UTC` 的状态。

```bash
# 1. 找最近的 full dump(必须 ≤ 目标时间)
ls -la /backup/dump/dump-*.sql.gz
# 比如最近的是 dump-20260515-0317.sql.gz(03:17 UTC,目标 14:23 之前 ✓)

# 2. 找该 dump 之后的所有 binlog(从 dump 头部读 master coords)
zcat dump-20260515-0317.sql.gz | head -50 | grep -i "MASTER_LOG_FILE\|MASTER_LOG_POS"
# 期望输出类似:
#   -- CHANGE MASTER TO MASTER_LOG_FILE='mysql-bin.000042', MASTER_LOG_POS=1234567;

# 3. 在已准备的隔离 namespace 起影子 MySQL,使用运维审定的清单。
#    清单须包含可恢复的数据卷,并从该 namespace 的 Secret 注入 MYSQL_ROOT_PASSWORD;
#    这里不在 kubectl 参数中传口令,也不复用生产 Pod / PVC。
SHADOW_NS='<隔离恢复 namespace>'
SHADOW_MANIFEST='<Git 外审定的影子 MySQL 清单绝对路径>'
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$SHADOW_NS" \
  apply -f "$SHADOW_MANIFEST"
# 清单中的影子 Pod 名约定为 mysql-shadow,先确认其 Ready 再灌数据。
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$SHADOW_NS" \
  wait --for=condition=Ready --timeout=5m pod/mysql-shadow

# 4. 灌 full dump
zcat dump-20260515-0317.sql.gz | \
  kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$SHADOW_NS" \
    exec -i mysql-shadow -- sh -ec \
    'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?影子容器未注入 root 凭据}"; exec mysql -uroot'

# 5. 重放 binlog 到目标时间(从 dump 记录的位置开始)
for f in mysql-bin.000042 mysql-bin.000043 mysql-bin.000044; do
  cat /backup/binlog/$f
done | mysqlbinlog \
  --start-position=1234567 \
  --stop-datetime="2026-05-15 14:23:00" \
  - | kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$SHADOW_NS" \
    exec -i mysql-shadow -- sh -ec \
    'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?影子容器未注入 root 凭据}"; exec mysql -uroot'

# 6. 验证影子库数据合理(查关键表)
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$SHADOW_NS" \
  exec mysql-shadow -- sh -ec \
  'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?影子容器未注入 root 凭据}"; exec mysql -uroot "$@"' \
  mysql-client -e "SELECT COUNT(*) FROM zone_101_db.player_database;"
# zone_101_db 仅为示例,必须替换为本次恢复计划中的真实业务库。

# 7a. 若仅是抽取单个玩家:从影子库 dump 该玩家行 → 导回生产
# 7b. 若是全库回档:停生产 mysql,把影子 PVC 推上去
#     ⚠ 全库回档 = 连同 mmorpg_global 里 id_segment 的发号水位一起倒回目标时刻,这是 ID 安全事件。
#     MySQL 里的消费表虽然一起回档,但 Kafka 里的 tx_id / snapshot_id、Redis 里在线玩家 blob 的物品 guid、
#     各进程内存里已经领走的号段都不会回退;不处理,下一次领段就把用过的号再发一遍
#     (login 的 ON DUPLICATE KEY UPDATE 静默覆盖别人的角色行,流水表 INSERT IGNORE 静默丢新流水)。
#     推上去之后、放开流量之前,按 deploy/k8s/README.md「恢复全局库前须先核对 id_segment.max_id」三步做完:
#       ① 停 data-service,重启所有持有号段的进程(login / guild / scene);
#       ② 逐个 biz_tag 拿消费侧最大号(含各 zone 库、Kafka 两个 topic、mmorpg_trade / mmorpg_guild 独占库);
#       ③ max_id 不大于该最大号的,抬到「最大号 + 1 + 至少一个 step」。
#     7a 的单玩家抽取不涉及水位,不需要这一步。
# 7c. 7a / 7b 之后、放开流量之前:删除受影响玩家的资产 SQL 提交证据 player:{<player_id>}:asset_sql_commit
#     (共享 Redis DB 0;7b 为该 zone 全部玩家)。证据指向已被回滚的提交,不删的话资产通道会拿回滚前的账本终结
#     对侧账,回档后的新存盘也发布不上去(docs/design/db-write-fencing-mysql.md §6.6、§9.2)。删除方向安全:
#     待办等玩家下一次存盘重新发布。
```

### 4.2 单玩家 / 单 zone 数据抽取(更常见)

合服失败 / 客服补单这种场景,只想从 PITR 影子库抽某玩家的几行,**不要替换整库**。

```bash
# 在影子库上 dump 单玩家
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$SHADOW_NS" \
  exec mysql-shadow -- sh -ec \
  'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?影子容器未注入 root 凭据}"; exec mysqldump -uroot "$@"' \
  mysqldump-client \
  --where="player_id=12345678" \
  zone_101_db player_database \
  > player-12345678-recovered.sql

# 在生产库 review 后再灌
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  exec -i deploy/mysql -- sh -ec \
  'export MYSQL_PWD="${MYSQL_ROOT_PASSWORD:?容器未注入 root 凭据}"; exec mysql -uroot "$@"' \
  mysql-client zone_101_db < player-12345678-recovered.sql
```

---

## 5. 与其他流程的衔接

### 5.1 合服前(server_merge_design.md §5.1)

**强制**:在 `k8s-zone-down` source/target 之后,手动触发一次备份,再开始合服:

```bash
BACKUP_JOB="mysql-backup-pre-merge-$(date +%s)"
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  create job --from=cronjob/mysql-backup "$BACKUP_JOB"
# 等 Job 完成
kubectl --context "$KUBE_CONTEXT" --kubeconfig "$KUBE_CONFIG" -n "$INFRA_NS" \
  wait --for=condition=complete --timeout=2h "job/$BACKUP_JOB"
```

### 5.2 zone 灾难回档(zone_data_rollback.md §3)

§3 列的「步骤 2 MySQL PITR」直接用本文 §4.1 的流程。**先恢复到影子库**,确认无误后再决定换正式库还是抽数据。
没有整体清空共享 Redis 时(`k8s_zone_rollback.ps1` 默认拒绝 FLUSHDB),按 §4.1 第 7c 步逐个删除该 zone 玩家的资产 SQL 提交证据键。

---

## 6. 故障排查

| 症状 | 原因 | 处置 |
|---|---|---|
| `kubectl get cronjob` 显示 `LAST SCHEDULE` 为空 | controller-manager 挂了 / 时区配错 | 看 kube-controller-manager 日志 |
| Job pod 起来但秒退 | mysqldump 连不上 mysql Service | 在本节同一 context / namespace 中定位备份 Pod,核对其 Secret 引用与数据库账号;交互测试用 `mysql -h mysql -uroot -p`,不在 `-p` 后拼口令 |
| dump 文件大小 = 0 / 接近 0 | dump 命令报错被 gzip 吃掉 | 改 cronjob 加 `set -o pipefail`(下次迭代) |
| binlog 目录不存在 | initContainer 没跑 / 老 pod 升级遗留 | 进 pod `ls /var/lib/mysql-binlog`(PVC 根的 binlog/ 子目录,不在 datadir 里),手动 `mkdir -p` 后重启 |
| 备份卷快满 | 保留策略太宽 / 实例写入暴增 | 改 `mysql-backup-cronjob.yaml` 的 `-mtime +30` 数字,或扩 PVC |
| PITR 时 binlog 缺失 | binlog 在 CronJob 跑之前就过期(>7d) | 改 `binlog_expire_logs_seconds`(`mysql-config`)调到 14d,改 CronJob 改成每 6h 跑一次 |

---

## 7. 没做完的事(未来 iteration)

| 项 | 优先级 | 说明 |
|---|---|---|
| 备份 → S3/OSS 异地归档 | 高 | PVC 只防 pod 重启,不防整个集群 / 机房挂 |
| 自动恢复演练(每月) | 中 | 加一个 CronJob 把昨天的 dump 灌进临时影子库,验 row count;失败告警 |
| 备份失败告警 | 中 | 接 Prometheus AlertManager:`kube_cronjob_status_last_successful_time` 超过 36h 报警 |
| 仓库模板改为 Secret 引用 | 高 | 仓库模板仍含开发固定口令;本手册的 prod 步骤要求先准备 Git 外的 Secret 引用清单,不允许直接发布开发模板 |
| 多副本 / 主从复制 | 低 | 当前 single-instance + Recreate 策略,合服 / 大改动时有几分钟 downtime |

---

## 8. Changelog

- **2026-05-15 v1**: 初版。落地 `mysql.yaml`(PVC + binlog 配置)+ `mysql-backup-cronjob.yaml`(每日 dump + binlog)。
- **2026-10-15**:首次安装与升级改用正规 infra 入口,明确 `mysql-init-sql`、目标 context / namespace、备份 CronJob 仍须单独安装及存量初始化边界。仅修订手册,未执行部署或恢复演练。
- **2026-10-15 补充**:现行命令显式携带目标 context / kubeconfig / namespace;SQL 工具改为在容器内读 root 凭据;备份与影子实例先准备匹配目标环境的 Secret 引用清单,不再发布开发固定口令。
- **2026-10-16**:§4.1 加第 7c 步、§5.2 加一句:回档后删除受影响玩家的资产 SQL 提交证据键 `player:{id}:asset_sql_commit`(db-write-fencing-mysql.md §6.6、§9.2)。仅修订手册,未演练。
