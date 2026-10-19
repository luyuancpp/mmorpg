# 线上玩家数据拉取与本地复现

2026-10-19 重写(此前版本的 `-mode player` 示例读的是没有生产写者的键族,照做拿不到玩家数据)。
入口仍是 `go/data_service/cmd/debug_fetch`(只读拉取)与 `go/data_service/cmd/debug_import`(导入本地);没有 ps1 包装器。

## 0. 结论

- **查某服某玩家的问题:`debug_fetch -mode player-bundle`**。一次拿全:核心 Redis 里该玩家的全部相关键(父快照、存盘屏障、归属代次、在线 / 会话、映射与落点、账号角色目录)、有效落点库里的两张玩家表、全局库的快照 / 货币物品流水 / 回档审计 / 名字,外加工具判读(在不在线、最近一次存盘落库没有、Redis 与 SQL 差在哪些字段)。
- **要在本地复现:`debug_import -file <包>`**。导入后本地 login 走真实冷加载链路就能把这个玩家加载起来。
- 「某个服」不是独立的存储单元:全服共用一台 Redis(DB 0,玩家键**不带 zone**),MySQL 是每 zone 一个库但同一实例。所以**按玩家拉,不要整库拷贝**——RDB / BGSAVE 会带走所有 zone 的玩家和以明文 token 为键名的会话键。合服后以 `player:zone:<id>` 的 home_zone 为准,不以玩家看到的服号为准;工具会自动解析。
- 旧的 `-mode player` / `snapshot` 读的是 data_service 自己的 `player:{id}:<field>` 键族,不是玩家实时数据;`-mode zone` 会 SCAN 全部映射键,线上不要用。

## 1. 玩家数据在哪(新鲜度由高到低)

| 位置 | 内容 | 新鲜度 |
|---|---|---|
| scene 进程内存(ECS) | 在线玩家的真实最新态 | 只能等玩家下线后落盘再拉 |
| 核心 Redis `PlayerAllData:<id>` | 父快照,scene 存盘写入,无 TTL | 周期存盘最多落后内存约 300 秒;资产操作即时存盘 |
| 落点库 `player_database` / `player_database_1` | Kafka DBTask 经 go/db 异步落库,行内带围栏 `fence_owner_epoch` / `fence_save_seq` | 落后父快照一段 Kafka 积压 + 重试时间;进死信时一直落后 |
| `player:{<id>}:save_barrier` | `pending` 与两张表的 ACK | **三者相等 = 最近一次存盘两张表都已落库** |
| 全局库 `player_snapshot`(source=1) | 登录 / 登出时 C++ 拍的快照 | 历史对比用 |
| 全局库 `transaction_log` | 货币 / 物品流水(按 `from_player` / `to_player`) | 「东西丢了」的主要凭证;投递失败不阻塞玩法,可能有缺口 |

导出包的 `analysis` 段直接给出这几项的判读。

## 2. 权限与数据保护

- 连线上(port-forward、取口令)由运维本人操作;AI 不登录 k8s,只处理已经拉到本地的文件(AGENTS §9)。
- 导出包含该玩家的游戏数据和账号名(角色目录);账号目录里的 `password` 字段在导出时清空且不带原字节。包文件以 0600 创建,**放仓库外的受控目录**,按工单留存,结案删除,不要进 git、不要上传。
- 工具只发读命令:Redis 一次 MULTI 里只有 TYPE / PTTL / GET / HGETALL / ZSCORE,不 SCAN;MySQL 只在可重复读事务里发固定 SELECT,结束回滚。仓库还没有 SELECT-only 账号,线上用的是业务账号——这是遗留风险,建议运维先建只读账号再用。
- 口令不写进 yaml 副本:用 `-redis-password-env` / `-mysql-password-env` 给环境变量名,工具只读变量,不打印。

## 3. 拉取

前置:**检出线上镜像对应的 commit**(镜像 tag 是 git 短 sha)。解码用的是本地检出的 proto,字段不一致时 protobuf 会「成功」解出错误的 JSON。把这个 sha 填进 `-server-revision` 留档。

```powershell
# 运维在另外两个终端里保持端口转发(示例端口):
#   kubectl -n mmorpg-infra port-forward svc/redis 16379:6379
#   kubectl -n mmorpg-infra port-forward svc/mysql 13306:3306
# 配置:把 go/db/etc/db.yaml 与 go/data_service/etc/data_service.yaml 复制到仓库外(例如 D:\secure\),口令留空。
Set-Location go/data_service
go run ./cmd/debug_fetch -mode player-bundle -player <player_id> -reason "<工单或反馈编号>" -server-revision <线上镜像 sha> `
  -db-config D:\secure\db.yaml -config D:\secure\data_service.yaml `
  -redis-addr 127.0.0.1:16379 -mysql-addr 127.0.0.1:13306 -global-mysql-addr 127.0.0.1:13306 `
  -redis-password-env DBG_REDIS_PASS -mysql-password-env DBG_MYSQL_PASS `
  -out D:\secure\player-<player_id>.json
```

参数要点:

- `-db-config`:落点库所在 MySQL(`ServerConfig.Database`)+ 核心 Redis(`ServerConfig.RedisClient`)+ 映射 Redis(`Placement.Redis`,不写时同核心,与 go/db 一致)。
- `-config`:全局库(`SnapshotMySQL`)。读不到只在包里记 `global.errors`,不影响其余部分;`-skip-global` 跳过。
- 落点库按 go/db 同一规则解析:`player:placement:<id>` → `player:zone:<id>` → `-zone`(缺省 db.yaml 的 ZoneId)。结果和依据写在 `routing`;`-database` 可显式指定,会留警告。
- `-limit` 控制快照 / 流水 / 回档审计的条数(默认 20);`-timeout-seconds` 默认 60。
- `-skip-account` 不读账号角色目录。

读包的顺序:`analysis`(online / barrier.conclusion / parent_vs_sql / warnings)→ `routing` → `redis`(每把键带用途说明、类型、TTL、解码结果)→ `mysql.tables` → `global`。

- `parent_vs_sql` 列出父快照与 SQL 行取值不同的顶层字段(忽略围栏两列):屏障已对齐还有差异,说明父快照在最后一次落库之后又存过盘。
- 玩家在线时导出的不是最终态(`warnings` 会写明):要确切数据,等玩家下线、`barrier.conclusion` 显示两表都已落库后重导。

## 4. 导入本地复现

前置:本地全套服务可用;本地检出与包里的 `server_revision` 一致并已跑本地迁移;本地同号玩家不在线。

```powershell
Set-Location go/data_service
# 先预览(默认 dry-run,不写任何东西)
go run ./cmd/debug_import -file D:\secure\player-<player_id>.json -local-account dev_tester
# 确认后执行;本地已有同号存档时加 -replace
go run ./cmd/debug_import -file D:\secure\player-<player_id>.json -local-account dev_tester --dry-run=false
```

导入做的事:

1. 选数据来源 `-source`:`auto`(默认,父快照存在就用父快照——它包含线上还没落库的最后状态)/ `redis-parent` / `sql`(只复现 SQL 里最后一次两表都确认的版本)。
2. 在本地**重铸一个存盘版本** V=(E, S):E 取线上与本地观察到的最大归属代次 + 1,S 取观察到的最大存盘序号。两张 SQL 行的围栏、`save_barrier` 的 pending 与两个 ACK、`owner_epoch` 都写成 V——这是 login 冷加载接受有历史数据玩家的条件。不照搬线上的屏障与代次;本地残留的旧 DBTask 代次更低,会被行内围栏拒掉。
3. 一个事务里替换两张表;一次 MULTI 里删本地父快照 / 子表缓存 / 资产提交证据 / 交接标记、写屏障与 `owner_epoch`、把玩家移出本地待落库索引、写 `player_to_account:<id>` = `-local-account`;最后登记 `player:zone:<id>` = 本地 zone。
4. 写前闸门:MySQL、核心 Redis、data_service 映射 Redis(db_first 时还有全局库)必须是 `localhost` / 回环地址;本地 docker 服务名用 `-allow-target-host=<名>` 显式放行。本地同号玩家在线一律拒绝;已有同号存档要 `-replace`;本地映射指向别的 zone 时拒绝。

登录:用 `-local-account` 指定的 dev_ / robot_ 账号**先 Login 一次**(建出账号目录),再 EnterGame 这个 player_id;EnterGame 在账号目录里找不到角色时会按 `player_to_account` 自愈补进去。客户端角色列表可能显示不出来,用能直接指定 player_id 的 robot 发 EnterGame。

限制:

- 只导入登录加载链路读的两张玩家表;好友 / 公会 / 邮件 / 交易 / 收件箱等其他服务的数据、名字注册表(名字可能显示为空)、配置表都不导入,本地配表版本须与线上一致。
- 不支持 `-player` 换号(blob 内的身份不会跟着改);撞号用 `-replace`。导入写了 `owner_epoch`,本地建角的撞号探测会避开这个号。
- 中途失败(例如 SQL 写完、Redis 没写)时 login 会 fail-closed 拒绝该玩家,带 `-replace` 重跑即可。

## 5. 其他模式

```powershell
# 只读 SQL:-database 可指向同一实例上的其他库(全局库、其他服务的独占库)
go run ./cmd/debug_fetch -db-config D:\secure\db.yaml -mode sql -database mmorpg_trade -sql "SELECT * FROM trade_asset_op WHERE player_id = <id> LIMIT 20"

# 扫描某库里以 player_id / role_id / uid 等列命中的所有表
go run ./cmd/debug_fetch -db-config D:\secure\db.yaml -mode player-db -zone <home_zone> -player <id>
```

- SQL 模式只接受单条 SELECT / SHOW / DESCRIBE / EXPLAIN,并拒绝 `EXPLAIN ANALYZE`、`INTO OUTFILE/DUMPFILE/@变量`、锁定读、`GET_LOCK` / `SLEEP` 等有副作用的写法。
- 其他服务按玩家查时注意列名不是 `player_id`:`mail_message.owner_player_id`、`trade_listing.seller_player_id`、`trade_commerce.seller_player_id/buyer_player_id`、`friend_request.from_player_id/to_player_id`、`transaction_log.from_player/to_player`。资产指令终态行(guild / trade)与收件箱终态条目约 30 天后清理,接到「东西丢了」的反馈要尽快导出。
- `player-db` 模式:2026-10-19 修复了单连接池下游标未关就发下一条查询、必然卡到超时的问题;单表查询失败记在 `errors` 里,不再静默跳过;超过 `-limit` 的表标 `truncated`,旧 `debug_import` 拒绝导入被截断的表。它导出的行围栏照搬线上值,灌回本地后 login 冷加载会拒绝该玩家——本地复现用 player-bundle。

## 6. 要「问题发生那一刻」的状态

Redis 只有 AOF,没有历史归档;MySQL 有每日全量 + binlog。先用包里 `global.snapshots` 的登录 / 登出快照与 `global.transaction_log` 对比;不够再按 [mysql-backup-pitr-runbook.md](mysql-backup-pitr-runbook.md) §4.1 恢复到影子库,§4.2 抽单玩家。
