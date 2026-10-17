# 调研规格:D1 映射持久化第 1b 批(2026-10-16)

> **状态**:只读调研产出,**未落码**。由开服线会话 2026-10-16 的调研工作流生成(一名调研者出规格、一名核实者逐条反驳),原样保存,未经人工改写。
> **怎么用**:前半部分是调研者的规格;「# VERDICT」之后是核实者意见。**落码前必须先吸收核实意见里的 must / should**,被判 refuted 的事实以核实者为准。行号与工作区状态是 2026-10-16 03:48~04:35 的快照,工作区有多个会话在并行修改,落码前按函数名重新定位。
> **汇总、拍板项与推荐顺序**:见 [docs/PROGRESS.md](../../PROGRESS.md)「2026-10-16 开服线剩余工作调研」条目,以及交接 [handoff-launch-readiness-and-d1-20261013.md](../handoff-launch-readiness-and-d1-20261013.md) 文首 10-16 段。

## item
D1 映射持久化第 1b 批规格。内容包括：对账 leader（route_reconcile 租约）、全量预热与 mapping:route:ready 就绪标记、Redis 身份变化检测与回看修补、merged_into 镜像同步、水位与回档检测（writable=0）、每 30s 增量推送、rewarm、reconcile-from-cache 反向修库，以及 route_tool 并进 data_service 主程序入口。按审阅规模拆成 1b-1（工具、入口、库原语）和 1b-2（leader）；K8s 配置载体与告警另列为 1b-3。
## already_done
无。核对依据：提案 §11.2 进度表把第 1b 批记为「未开始」（docs/design/player-route-mapping-durability-proposal.md:437）；PROGRESS 2026-10-15 收尾汇总把「D1 第 1b / 2 / 3 批」列为仍未做（docs/PROGRESS.md:8208）；10-16 的各条目都与 D1 无关。代码里 placement.RouteReadyKey 只有定义，没有读写方（go/shared/placement/route.go:28-30）。route_reconcile 只由迁移种行，不读也不写（go/data_service/internal/store/route_store.go:32、:1794-1815）。go/data_service/internal/routereconcile 与 routetoolcli 两个目录都不存在。第 1a 批已经备好的底座可以直接复用：五张表与 route_reconcile 的列（leader / lease_until_ms / watermark_ms / cache_verified_ms），ScanRoutes 键集翻页，set-if-newer 发布脚本，import / audit / tidb-prepare，以及 R1–R16 SQL。
## summary
问题与根因：第 1a 批把 player_route 等五张表、W1 的 db_first 开关和 route_tool 的导入、审计、预切都做了，但没有任何组件维护 Redis 副本的完整性。具体有五个缺口：
1. db_first 下「已提交、未发布」的孤儿行没人补发，所以 P3 的 audit=0 门禁不可达。
2. Redis 重启或切主后，丢失的已确认发布没人修补。
3. 全局库回档（K8s 下全局库与 zone 库在同一个 mysql 实例）没人发现。
4. 就绪标记没有写者，第 3 批读方无从「认标记」。
5. route_tool 是独立 cmd，而 K8s 镜像只构建 /app/service 单一入口（Dockerfile.go-svc:139、:220），生产跑不了。

做法：
- **1b-1**（约 19 个文件，不改服务运行行为）：
  - 把 cmd/route_tool 的逻辑迁到 internal/routetoolcli，用 `data_service -f <yaml> -route-tool <子命令>` 进主程序；
  - 新增 freeze / unfreeze / reconcile-from-cache 三个子命令，以及 store 的冻结态反向修库原语（R17–R22）；
  - placement 增加标记值编解码和 CAS / CAD 脚本。
- **1b-2**（约 21 个文件）：
  - 新包 internal/routereconcile 实现 leader：租约用库时钟，非 leader 先一致读再抢租约；
  - 每拍做身份、淘汰自检和库内原子的水位与回档判定；
  - 全量预热、回看修补、增量推送都走「MGET 预过滤 + EVALSHA 发布脚本 + NOSCRIPT 重载」；
  - merged_into 只增不删的镜像；
  - 用 `SET NX + 同连接 WAITAOF` 挂标记，再做身份复核；
  - 另加 rewarm 和 audit `-require-maprev`。
- **对 8 个设计问题的结论**：
  1. redis 模式下不构造 leader，也不碰标记。db_first 下 leader 默认运行，但只有 `RouteStore.Reconcile.ArmReadyMarker=true` 才挂标记，这个开关在 P3 之后才开。
  2. 标记值是 string JSON `{v,generation,redis_run_id,master_replid,set_at_ms,watermark_ms,leader}`。
  3. 收敛靠 rev 单调加 set-if-newer。挂标记前要求零未决结局、身份前后一致，并用同连接 WAITAOF 按全局偏移覆盖此前所有推送。
  4. 回档检测容差默认 0。判据有两条：库水位低于标记水位，或推送时复读确认「缓存 rev > 库 rev」。writable 只有 leader（受租约谓词约束）和 route_tool 的 freeze / unfreeze 写。
  5. 反向修库只在 writable=0 时允许，逐行加锁、按 rev CAS，可以幂等重跑。同 rev 而值不同默认只报告，显式 `-resolve-ties=cache` 才覆盖。
  6. 租约到期一律按库时钟（dbNowMsSQL）判；本地单调时钟只用于「续约发出后 Lease−Safety 内有效」的自行降级。库侧写入靠 SQL 谓词作 fencing，Redis 侧的效果幂等或自证。不加 epoch 列，因为加列要改 DDL、触发形状守卫。
  7. FenceShards TiDB 档放到 1b-3。
  8. 指标全部是封闭小集合的 label。

几处偏离提案：
- 解冻不换 generation；
- 第 2 批之前不做围栏镜像对账，因为在 1b 里「删除」方向会删掉 merge_zone 的真围栏；
- 回看起点取 min(标记水位, cache_verified_ms)−60s，用来防 Redis 从旧备份恢复；
- 增量推送的窗口是上次起点减重叠量，不是「最近 N 分钟」。
## est_files: 36
## current_facts
F1. 第 1b 批未开始，D1 后续批仍挂在开服交接 §4 第 8 项
   证据: docs/design/player-route-mapping-durability-proposal.md:437（「第 1b 批 | 未开始」）；docs/handoff/handoff-launch-readiness-and-d1-20261013.md:155-159；docs/PROGRESS.md:8208（已亲阅）
F2. 就绪标记键只定义、无读写方，注释明确由 1b 的对账 leader 写
   证据: go/shared/placement/route.go:28-30 `const RouteReadyKey = "mapping:route:ready"`（已亲阅）；全仓 grep RouteReadyKey 只有这一处定义
F3. route_reconcile 表现有列 leader / lease_until_ms / watermark_ms / cache_verified_ms，没有 epoch 列；迁移只种 id=1 行，1a 不读不写
   证据: 提案 :128-135 DDL；go/data_service/internal/store/schema.go:266-278；route_store.go:32、:248-250（R16 种行）、:1794-1815（已亲阅）
F4. route_meta 由迁移种随机 generation 与 writable=1，ODKU no-op 永不换代；W1 对 writable 是普通读，置 0 时在途事务仍会提交
   证据: route_store.go:1766-1792（bootstrapRouteMetaRow）；:643-654（requireRouteWritable 注释「1b 的流程要先等它们结束」）（已亲阅）
F5. 库时钟统一用 dbNowMsSQL，updated_at_ms 在语句执行时取值、提交可能更晚
   证据: route_store.go:153-154 `CAST(UNIX_TIMESTAMP(NOW(3)) * 1000 AS UNSIGNED)`；R4 / R6 / R9 都用它（:192-227）（已亲阅）。TiDB 上 NOW() 取各 tidb-server 本机时钟，节点间偏差【未核实】
F6. 发布脚本是 set-if-newer：rev 按长度再逐字节比；无 maprev 时有遗留值守卫，值不同回 {2} 不写；回 0 时 touch maprev；可选 KEYS[4] 围栏键，回 {3} 不写
   证据: go/shared/placement/route.go:154-207（脚本）、:274-336（ClassifyPublishReply 只认四种形状）（已亲阅）
F7. W1 发布用 go-zero 客户端（带熔断器）、EVAL、要求 HomeZone≠0；WAITAOF 探针为隔离熔断器另建 *redis.Redis
   证据: go/data_service/internal/routing/route_publish.go:258-273（拒 HomeZone==0）、:298-304（PipelinedCtx + Eval + Do("WAITAOF")）、:380-398（ProbeWaitAOF 注释说明熔断器风险）；router.go:48 `redis.MustNewRedis(c.MappingRedis)`（已亲阅）
F8. ScanRoutes 沿聚簇键 (route_shard, player_id) 键集翻页并校验错片；增量推送模板已在提案给出（按片 0..63、(updated_at_ms, player_id) 游标）
   证据: route_store.go:1658-1690；提案 :813（1b 增量推送 SQL 模板）、:821-825（1b 约束：水位在 route_reconcile、回档检测跨两表要写明原子范围、leader 标识 ASCII）（已亲阅）
F9. 导入语义：库无行就插入 rev=maprev（无 maprev 取 1）；maprev > 库 rev 记 rev_regression、不写；导入事务要求 writable=1
   证据: route_store.go:1415-1465（decideImport）、:1246（importRoutesTx 内 requireRouteWritable）（已亲阅）
F10. import 与 audit 都在内存里保留 O(N) 的已见 id 集合，规模到 10 亿行时不可行
   证据: go/data_service/internal/routetool/importer.go:157-158（seen map）；auditor.go:262-266（redisSeen / dbSeen，注释「按 100 万行估算约几十 MB」）（已亲阅）
F11. player_route 的写语句由 AST 守卫钉在 route_store.go 一个文件里
   证据: go/data_service/internal/store/route_store_test.go:328-388，:334 `const allowed = "internal/store/route_store.go"`（已亲阅）
F12. route_tool 是独立 cmd，用 conf.Load 保证失败一律退出码 2，stdout 只留一行统计
   证据: go/data_service/cmd/route_tool/main.go:9、:26（K8s 单入口的限制）、:102-113（logx 改写到 stderr、conf.Load）（已亲阅）
F13. data_service 主入口先把版本行打到 stdout，再 MustLoad；只认 -migrate / -version，多余的位置参数被静默忽略
   证据: go/data_service/data_service.go:41-49（flag）、:76-89（Parse → 版本行 stdout → MustLoad → -migrate）、:94-100（RouteStore 校验）（已亲阅）
F14. K8s 上 data-service 只有一个二进制 /app/service，容器参数只有 -f，没有位置参数
   证据: deploy/k8s/Dockerfile.go-svc:139（-o /app/service ${ENTRY}）、:220（ENTRYPOINT）；deploy/k8s/manifests/go-svc/data-service.yaml:80 `args: ["-f", "/app/etc/data_service.yaml"]`（已亲阅）
F15. K8s ConfigMap 的 RouteStore 段只写固定的 Mode: redis 和镜像来的两项池大小，没有 FenceShards / WaitAOF / Reconcile 载体；该脚本工作区里有他人未提交的大改动
   证据: tools/scripts/k8s_deploy.ps1:5812-5824（已亲阅）；`git diff --stat` 为 +666/−28，mtime 2026-10-16 03:53；提案 :1033-1046 登记了 FenceShards TiDB 档的缺口
F16. 本地 yaml 的 RouteStore 是 redis 模式，FenceShards: 16；配置非法一律拒启，与 Mode 无关
   证据: go/data_service/etc/data_service.yaml:177-185；go/data_service/internal/config/config.go:294-378（RouteStoreConfig / Normalize / Validate）（已亲阅）
F17. mapping Redis：K8s 单实例没设 maxmemory（缺省 noeviction）、开 AOF；compose 已改 noeviction；redis-ha 设了 min-replicas-max-lag 10
   证据: deploy/k8s/manifests/infra/redis.yaml:131-133；deploy/docker-compose.yml:256；deploy/k8s/manifests/infra/redis-ha.yaml:259-260（已亲阅）
F18. 容量目标下 player_route 会长到 10 亿行（N_reg=10×DAU，DAU 仍是 1 亿，§7.2 说按 DAU 增长的量不乘二）；mapping Redis 三键需要 100~300GB
   证据: docs/design/capacity-model-hok-scale.md:57、:77（N_reg S3 = 10 亿）、:290（mapping Redis 内存）、:894-910（§7.2）（已亲阅）
F19. merged_into 的 Redis 值是 merge_zone 用 json.Marshal 写出的 {"dst":N,"run_id":"…"}，routetool 有一份解析
   证据: tools/merge_zone/merged_into.go:39-42、:106-115；go/data_service/internal/routetool/redisread.go:329-350（已亲阅）
F20. audit 现在不把 maprev 缺席算差异，能报 redis_rev_ahead
   证据: go/data_service/internal/routetool/auditor.go:188-240（classifyRoute）（已亲阅）；runbook docs/ops/merge-zone-runbook.md §9.3「maprev 缺席不算差异」
F21. 在线清库脚本先 FLUSHALL Redis，再 TRUNCATE 路由表并把 route_reconcile 水位复位为 0（服务运行中清）
   证据: tools/scripts/currency_crash_window.ps1:114-115、:189-191、:220（已亲阅）
F22. db_first 才装配 route store，失败返回 nil；MissingStores 只在 db_first 时要求 player_route
   证据: go/data_service/internal/svc/servicecontext.go:153-169、:296-336（已亲阅）
F23. 指标包的惯例：Subsystem 前缀、label 用封闭集合、启动时 Prime 预置 0
   证据: go/data_service/internal/metrics/metrics.go:269-300、:417-428（PrimeRoute）（已亲阅）
F24. Java 版的归属区就是库内列 game.player.zone_id，读方直接读库，没有 Redis 完整副本
   证据: D:\luyuan\xuanming-server-mmo\game-common\src\main\java\com\game\common\player\PlayerHomeZones.java:11-21；D:\luyuan\xuanming-server-mmo\PARITY.md:139（已亲阅，PARITY.md 工作区有他人未提交改动）
## plan
- [docs/handoff/launch-readiness-20261013/spec_D1_batch1b.md（新增，先例 spec_D1_batch1a.md）] 把本规格的设计结论落成文档，代码注释里引用的编号以它为准。内容如下。

**【D1 模式】**
- redis 模式：svc 不构造 reconciler。不建 go-redis 客户端，不抢租约，不发 INFO，不推送，不读写标记；已存在的标记也不碰。理由：这时 Redis 才是提交点，库只是 P1 时刻的快照，往缓存推会用落后的库值回填缓存缺的键。遗留值守卫只挡「值不同且无 maprev」，挡不住这种回填。
- db_first：leader 默认运行（Reconcile.Enabled 为 nil 时当 true，写 false 作急停）。它做五件事：租约、每拍身份与淘汰自检、水位与回档检测、增量推送（含孤儿行）、merged_into 只增镜像。
- 只有 ArmReadyMarker=true 才挂标记。这个开关在 P3「全部副本切完、二次导入、audit -require-maprev=0」之后才改。原因：P3 之前库不是 Redis 的超集，Redis 部分丢失后从库修补修不回只在 Redis 的登记，挂了标记就会把它们当成权威缺席。
- ArmReadyMarker=false 时，leader 首次当选会从 cache_verified_ms−RepairLookbackMs 推一遍（初值 0，等于全量），用来补齐 maprev、清掉 db_only_unpublished。

**【D2 标记值】**
- 形状：string 类型的 JSON `{"v":1,"generation":…,"redis_run_id":…,"master_replid":…,"set_at_ms":…,"watermark_ms":…,"leader":…}`。必须是 string：第 3 批 go/db 用 MGET 读，MGET 对非 string 回 nil，会被当成缺席。读方只看「存在且非空」。字段只增不改名，解析时忽略未知字段，v 只在不兼容时 +1。
- 各字段的用途：
  - generation：库世代，不等就冻结；
  - run_id / replid：挂标记时的身份，不等就撤标记并修补；
  - set_at_ms：诊断，也是 rewarm 的等待判据；
  - watermark_ms：每拍 CAS 更新为刚提交的库水位，用于回档检测；它同时是「缓存截至时刻」，身份变化后的回看起点取 min(watermark_ms, cache_verified_ms)−RepairLookbackMs，防 Redis 从旧备份恢复；
  - leader：诊断。
- 读写规则：只有 leader 用 SET NX 写，任何一方都可以 compare-and-delete（fail-closed 方向）。值不是 string 类型（WRONGTYPE）时无条件 DEL。

**【D3 收敛】**
- 前提：库 rev 对同一行严格递增（库回档另由 D4 检测）。发布脚本按 rev 取大，每次发布带的就是该 rev 的全部列。所以对每把键，缓存最终都停在「被发布过的最大 rev」的值。
- 预热读到旧 rev 而在线写者已发布新 rev：脚本回 superseded，leader 复读库。库 rev ≥ 缓存 rev 就是正常竞态，库 rev < 缓存 rev 就是 cache_ahead，冻结。
- 预热开始（T0）之后才提交的行：由写者自己发布；发布失败的属于未确认，没有依赖方，由增量推送补。
- 挂标记的条件：本轮零未决结局（conflict / legacy_conflict / fenced / db_invalid / error 都算未决）；身份前后一致；writable=1 且 generation 未变；租约有效。
- 挂标记的 SET NX 之后，在同一连接上发 WAITAOF。依据：AOF / 复制偏移全局有序，标记落盘就意味着此前所有推送都已落盘（按 Redis 7.2 语义推断，【未核实】）。挂完再读一次身份，变了就用 compare-and-delete 撤掉自己写的值。

**【D4 回档与 writable】**
- 两条判据：
  - 标记在、且 marker.watermark_ms > 库 watermark_ms + RollbackToleranceMs（默认 0）；
  - marker.generation ≠ 库 generation；
  - 另加推送时复读确认的 cache_ahead。
  命中就冻结（writable=0）、告警，停止库到缓存的推送，标记不动。
- 判定在一个库事务里完成：先锁 route_reconcile，再普通读 route_meta，必要时 UPDATE route_meta。锁序是 route_reconcile → route_meta。W1 只普通读 route_meta，不成环。
- 容差取 0 的依据：标记水位只来自已提交的库值，且先写库、后写标记，正常运行时不可能倒退。一旦倒退，一定是库丢了已提交的写（PITR、异步复制切主），这时 player_route 也可能丢了已确认的登记。
- writable 的读方：W1、导入（要求 1）、反向修库（要求 0）、leader 每拍、第 2 批 merge_zone。
- writable 的写方：leader 冻结（租约谓词约束）、route_tool freeze / unfreeze、迁移种行（只插不改）、本地清库脚本复位。

**【D5 反向修库】**
- 前提：writable 不为 1，库里没有生效围栏；开始前等 FreezeGrace（默认 10s），覆盖在途 W1（服务端 2s 加锁等待 1s）。
- 每批一个事务：加锁读，然后逐行判定：
  - 库无行：插入，rev=maprev，没有 maprev 取 1；
  - maprev > 库 rev：用缓存整行覆盖，rev 置为 maprev（不是 +1），让两边 rev 相同；
  - maprev = 库 rev 且值相同：match；
  - maprev = 库 rev 且值不同：tie，默认报告，`-resolve-ties=cache` 才覆盖；
  - maprev < 库 rev：db_ahead，不动；
  - 无 maprev：退回导入语义，只补 NULL 列，冲突报告。
- 写法：UPDATE 带 `AND rev = 读到的库 rev` 做 CAS。重跑时已修过的行判 match，所以幂等。
- merged_into：缓存有、库无就插入；值冲突报告；永不删除。

**【D6 租约】**
- 判据：`lease_until_ms` 与库时钟比较。非 leader 先一致读，看起来已过期才发 UPDATE（规模到上百副本时避免抢同一行锁）。
- 降级：本地单调时钟只算「续约发出时刻 + LeaseMs − LeaseSafety（LeaseMs/5）」，过点就自行降级，取消在途任务。
- 库侧 fencing：所有库写都带 `leader=? AND lease_until_ms >= now` 谓词。
- Redis 侧：推送是 set-if-newer，幂等；标记 CAS 是幂等或自证的；过期 leader 挂上的标记，现任 leader 下一拍按身份 / 世代判失效并撤掉。
- leader 标识：route_leader@{host}:{8hex}，保证同一台机器上多个进程不同名。

**【D7】** FenceShards 的 K8s 载体另列 1b-3，理由见那一项。

**【D8】** 指标清单见 metrics 一项。

**不变量**：
- I1：任何映射写事务从写 updated_at_ms 到提交，再到发布完成，耗时小于 IncrementalOverlapMs（15s）且小于 RepairLookbackMs（60s）；
- I2：库时钟偏差小于 1s；
- I3：mapping Redis 是 noeviction 的单实例（Type=node）。

**偏离表（B1–B12）**：
- B1：不换 generation；
- B2：围栏镜像推迟到第 2 批；
- B3：回看起点的取法；
- B4：ArmReadyMarker 开关；
- B5：增量窗口加 MGET 预过滤；
- B6：leader 用 EVALSHA；
- B7：回档检测用双判据；
- B8：`-route-tool <子命令>`；
- B9：rewarm 等于撤标记加等 leader 重挂；
- B10：不加 epoch 列；
- B11：新增 freeze / unfreeze；
- B12：K8s 载体另列。
   工作区: 新文件，不存在；所在目录干净（spec_G4b*.md 有他人改动，与本文件无关）
- [go/shared/placement/route.go（1b-1；1b-2 再改一次）] **1b-1：**
- 新增 `RouteReadyFormatVersion = 1` 和 `type RouteReadyMarker struct{V int; Generation, RedisRunID, MasterReplID string; SetAtMs, WatermarkMs uint64; Leader string}`，json 标签与 D2 一致。
- `EncodeRouteReadyMarker(m) (string, error)`：先校验再 json.Marshal。
- `DecodeRouteReadyMarker(raw) (RouteReadyMarker, error)`：
  - 未知字段忽略；
  - 要求 v==1、generation 为 1..32 个 [0-9a-f]、run_id 与 replid 各 40 个 [0-9a-f]、set_at_ms 与 watermark_ms 大于 0、leader 为 1..96 字节且字符集同 updated_by；
  - 失败一律用 `%w` 包 ErrMalformed。
  - 手写字符校验，不引 regexp，保持只依赖标准库。
- 两个 Lua 常量：
  - `RouteReadyCompareAndSetScript`：GET==ARGV[1] 就 SET ARGV[2] 回 1，否则回 0；
  - `RouteReadyCompareAndDeleteScript`：GET==ARGV[1] 就 DEL 回 1，否则回 0。
- 改 RouteReadyKey 注释为「1b：db_first 对账 leader 用 SET NX 写，任何一方可 compare-and-delete；读方（第 3 批）只看存在」。

**1b-2：**
- 新增 `MergedIntoKeyPrefix="merge:merged_into:"`、`MergedIntoKey(src uint32)`。
- 新增 `EncodeMergedIntoValue(dst uint32, runID string) (string, error)`，产出与 merge_zone json.Marshal 逐字节相同的 `{"dst":N,"run_id":"…"}`。这是 DRY 的权威出处，merge_zone 第 2 批改用它。
   工作区: git status 干净；mtime 2026-10-15 04:23；无他人未提交改动，冲突风险低
- [go/shared/placement/route_test.go] 新增以下用例：
- `TestRouteReadyMarkerRoundTrip`：编码后解码逐字段相等，未知字段忽略；
- `TestRouteReadyMarkerRejects`：表驱动，覆盖 v=2、缺 generation、run_id 长度错、watermark=0、leader 非 ASCII；
- `TestRouteReadyScriptsShape`：只钉住脚本文本里的 GET / SET / DEL 判据，执行语义由 routereconcile 的 miniredis 用例覆盖；
- 1b-2 加 `TestMergedIntoValueMatchesMergeZoneEncoding`：与 json.Marshal(struct{Dst uint32 `json:"dst"`; RunID string `json:"run_id"`}) 逐字节相等。
placement 包顶层用例数照实更新。
   工作区: 干净；mtime 2026-10-13 09:09
- [go/data_service/internal/store/route_store.go（1b-1）] **文件头**：「唯一写入方」改为：player_route 只在本文件（AST 守卫）；route_meta / route_reconcile 的写入在本文件与 route_reconcile.go（第二个 AST 守卫）。

**新增 SQL**（编号接 1a 的 R16）：
- R17 `routeRepairOverwriteSQL`：`UPDATE player_route SET home_zone=?, storage_id=?, placement_version=?, frozen_run_id=?, rev=?, updated_by=?, updated_at_ms=`+dbNowMsSQL+` WHERE route_shard=? AND player_id=? AND rev=?`；
- R18 `routeReadMetaSQL`：`SELECT generation, writable FROM route_meta WHERE id = 1`；
- R19 `routeFreezeSQL`：`UPDATE route_meta SET writable = 0 WHERE id = 1`；
- R20 `routeLockReconcileWatermarkSQL`：`SELECT watermark_ms FROM route_reconcile WHERE id = 1 FOR UPDATE`；
- R21 `UPDATE route_reconcile SET watermark_ms = GREATEST(watermark_ms, ?) WHERE id = 1`；
- R22 `UPDATE route_meta SET writable = 1 WHERE id = 1 AND writable = 0`。

**新哨兵**：`ErrRouteNotFrozen`（反向修库要求 writable≠1）、`ErrRouteReconcileMissing`（route_reconcile 缺 id=1，提示 -migrate）。

**新类型与函数**：
- `type RouteMeta{Generation string; Writable uint64}`、`ReadRouteMeta(ctx)`。
- `requireRouteFrozen(ctx,q)`：writable==1 时返回 ErrRouteNotFrozen。
- `RouteRepairOptions{DryRun, ResolveTies bool}`；`RouteRepairStats{Inserted, Restored, Matched, DBAhead, Filled, TiesResolved int; Ties, Conflicts []RouteImportConflict}`。
- `RepairRoutesFromCache(ctx, rows []RouteImportRow, opts)`：
  - 入参校验复用 validateRouteImportRows；
  - dry-run 走不加锁的 loadRoutes 加 plan；
  - 否则 runRouteTx：BEGIN → requireRouteFrozen → loadRoutes FOR UPDATE（聚簇键升序）→ planRouteRepair → R7 批量插入（受影响行数不等就按 errRouteImportRaced 重来）→ 对每行 restore / tie-resolve 执行 R17，必须恰好 1 行 → COMMIT。
- 纯函数 `decideRepair(db *RouteRow, r RouteImportRow, resolveTies bool) repairDecision`，语义见 D5：
  - 库行落点过不了校验的报 conflict；
  - 无 maprev 的委托 decideImport。
- `RepairMergedIntoFromCache(ctx, rows, dryRun)`：把 importMergedIntoTx 抽出一个 `gate func(ctx, routeQueryer) error` 参数。ImportMergedInto 传 requireRouteWritable，Repair 传 requireRouteFrozen，其余逻辑不变。
- `FreezeRouteWriters(ctx) (changed bool, err error)`：执行 R19；受影响 0 行时再读 R18，writable==0 就当已冻结，缺行返回 ErrRouteMetaMissing。
- `UnfreezeRouteWriters(ctx, minWatermarkMs uint64) (changed bool, err error)`：runRouteTx 内 R20 → 当 minWatermarkMs 大于当前值时执行 R21 → R22 → COMMIT。锁序 route_reconcile → route_meta，与 1b-2 的每拍事务一致；generation 不换（B1）。

**不碰的东西**：W1 / 导入的任何既有 SQL 与行为不动；不加 DDL。
   工作区: 干净；mtime 2026-10-15 04:25（最近一次随 ab0f27907 入库）；文件约 1824 行，冲突风险低，但 10-14/15 有多轮 P5 修复，落码前先 git log 确认没有新改动
- [go/data_service/internal/store/route_store_test.go] 新增以下用例：
- `TestDecideRepair`：表驱动覆盖 D5 全部分支，包括无 maprev 委托导入、库行无效；
- `TestRouteRepairOverwriteSQLBindsMapRevNotIncrement`：R17 的 9 个实参顺序，以及 rev=maprev；
- `TestRouteMetaAndReconcileWritesOnlyInStoreFiles`：AST 守卫。正则匹配 `UPDATE|INSERT INTO|DELETE FROM` 加 route_meta / route_reconcile，允许集合为 {internal/store/route_store.go, internal/store/route_reconcile.go}，命中数必须大于 0，防止正则失效。
既有的 `TestNoPlayerRouteWritesOutsideRouteStore` 不改，R17 在允许的文件里。
   工作区: 干净；mtime 2026-10-15 04:21
- [go/data_service/internal/store/route_store_contract_test.go] 用假驱动钉住语句序列与结局：
- `TestRepairRoutesFromCacheRefusedWhenWritable`：R1 返回 1 → ErrRouteNotFrozen、ROLLBACK、零写；
- `TestRepairRoutesFromCacheStatementSequence`：BEGIN / R18 或 R1 / R8 FOR UPDATE / R7 / R17×n / COMMIT 及其实参；
- `TestRepairRoutesFromCacheDryRunOpensNoTx`；
- `TestRepairMergedIntoRequiresFrozen`；
- `TestFreezeRouteWritersIdempotent`：受影响 0 行加 writable=0 → changed=false；
- `TestUnfreezeRaisesWatermarkBeforeFlippingWritable`：R20 → R21 → R22 → COMMIT 的顺序；
- `TestUnfreezeAlreadyWritableIsNoop`；
- `TestReadRouteMetaMissingRow`。
   工作区: 干净；mtime 2026-10-15 04:19
- [go/data_service/internal/store/route_store_integration_test.go（integration tag）] 新增三个真库用例：
- `TestRouteStore_RepairFromCacheRestoresRolledBackRows`：
  1. 用 ImportRoutes（MapRev=3）种一行，FreezeRouteWriters；
  2. Repair 传 maprev 5 → 库行 rev=5、值为缓存值；
  3. 原样再跑 → Matched，updated_at_ms 不变；
  4. 冻结期 RegisterHome 返回 ErrRouteNotWritable；
  5. writable=1 时 Repair 返回 ErrRouteNotFrozen。
- `TestRouteStore_RepairTieReportedUnlessResolved`。
- `TestRouteStore_FreezeUnfreezeRoundTrip`：watermark 按 GREATEST 抬高，generation 不变。
全部不依赖墙钟：只比较相对大小与相等。
   工作区: 干净；mtime 2026-10-15 04:27
- [go/data_service/internal/routetool/repair.go（新增，1b-1）] `ReconcileFromCache(ctx, rd mappingReader, db repairDB, opts RepairOptions) (RepairReport, error)` 与 `RepairExitCode`。

**依赖与参数**：
- repairDB 由消费方定义：ReadRouteMeta、RepairRoutesFromCache、RepairMergedIntoFromCache，*store.RouteStore 天然满足。
- RepairOptions：DryRun（默认 true）、ResolveTies、Batch、MaxReport、FreezeGrace（默认 10s）、Sleeper（可注入，测试用假的）、Target、Stdout、Stderr。

**步骤**：
1. ReadRouteMeta：writable==1 → 退出码 2，提示先 freeze。
2. listActiveFences 非空 → 退出码 3。
3. 等 FreezeGrace。
4. 三遍 SCAN，用无状态去重，内存只占一批（不用 importer 那种 O(N) 的 seen）：
   - A 遍 `player:zone:*`；
   - B 遍 `player:placement:*`，MGET 出的三元组里 zone 非空就跳过；
   - C 遍 `player:maprev:*`，zone 或 placement 非空就跳过，剩下的只有 maprev 计 MALFORMED。
   每页 readRouteTriples → parseRedisRoute → 按 player_id 排序 → RepairRoutesFromCache。
5. scanMergedInto → RepairMergedIntoFromCache。
6. 结尾再查围栏，出现就退出码 3。

**输出**：
- stdout 一行：`route_reconcile_from_cache dry_run=… players_scanned=… inserted=… restored=… filled=… matched=… db_ahead=… ties=… ties_resolved=… conflicts=… malformed=… merged_into_inserted=… merged_into_conflicts=…`；
- stderr 明细：TIE / CONFLICT / MALFORMED / DB_INVALID。

**退出码**：0 干净；1 有 ties / conflicts / malformed；2 失败；3 有围栏。
   工作区: 新文件，不存在
- [go/data_service/internal/routetool/routetool_test.go（1b-1；1b-2 再加 rewarm / require-maprev）] 用 miniredis 加假 repairDB：
- `TestReconcileFromCache_RefusedWhenWritable`（退出码 2，零写）；
- `_FenceExitsThree`；
- `_StatelessDedupeAcrossPasses`：同一 id 三族键都在，只提交一次；
- `_MapRevOnlyIsMalformed`；
- `_TiesReportedByDefault`；
- `_ResolveTiesCache`；
- `_DryRunWritesNothing`；
- `_MergedIntoInsertedNeverDeleted`；
- `_GraceUsesInjectedSleeper`：断言调用了 Sleeper(10s)，不真睡。
1b-2 再加 `TestRewarm_*` 与 `TestAudit_RequireMapRev*`（见那两项）。
   工作区: 干净；mtime 2026-10-14 23:47
- [go/data_service/internal/routetool/routetool_integration_test.go] 新增 `TestReconcileFromCache_RealDBEndToEnd`：
1. 真 MySQL 加 miniredis；
2. 导入后在缓存把某行 maprev 加到 rev+2 并改值，模拟库回档；
3. freeze → reconcile-from-cache（-dry-run=false）→ audit 退出码 0 → unfreeze → RegisterHome 恢复可写。
   工作区: 干净；mtime 2026-10-13 09:13
- [go/data_service/internal/routetoolcli/cli.go（新增，1b-1；1b-2 加 rewarm 与 audit -require-maprev）] **迁移**：把 cmd/route_tool/main.go 的 run / parseArgs / parseIDList / connectMappingRedis / openRouteStore / runTiDBPrepare 原样迁过来，导出 `func Run(ctx, args []string, stdout, stderr io.Writer) int`。

**新子命令**：
- `reconcile-from-cache`：参数 -dry-run=true、-resolve-ties=report|cache、-batch、-timeout、-max-report、-freeze-grace=10s。
- `freeze`：参数 -reason=<文本>，文本只打到 stderr，不入库，因为没有列。
- `unfreeze`：必须带 -confirm-audited，否则退出码 2。流程：
  1. ReadRouteMeta，已经可写就退出 0，打 `already writable`；
  2. 读标记：畸形就 compare-and-delete，当作不存在；generation ≠ 库 generation 时，用 RouteReadyCompareAndSetScript 把 generation 改成库值，CAS 输了退出码 1，要求重跑；
  3. 调 `UnfreezeRouteWriters(ctx, marker.WatermarkMs)`。
  顺序是先对齐标记、再置 writable=1：冻结期间 leader 不会因为世代不符再次冻结（D4），中途崩溃重跑也幂等。

**模式门禁**：
- reconcile-from-cache / unfreeze 在 yaml 为 redis 模式时退出码 2（「库不是真源，用 import-from-redis」）；
- freeze 允许，但 redis 模式下打 WARN「redis 模式的 W1 不读 writable」。

**沿用的约定**：conf.Load、logx 改到 stderr、只留 Error 级别，退出码约定 0/1/2/3，usage 同步更新。
   工作区: 新目录与新文件，不存在
- [go/data_service/internal/routetoolcli/cli_test.go（新增）] - 迁入 cmd/route_tool/main_test.go 的 5 个用例，改名前缀为 `TestRun*`；
- 新增 `TestRunSubcommandFlagsAreScoped`：新子命令的参数互不串用；
- `TestRunReconcileAndUnfreezeRefusedInRedisMode`；
- `TestRunUnfreezeNeedsConfirmAudited`；
- `TestRunFreezeUnreachableDBExitsTwo`；
- `TestRunStdoutCarriesOnlyTheSummary`：logx 输出不进 stdout。
   工作区: 新文件，不存在
- [go/data_service/cmd/route_tool/main.go] 薄包装：`func main(){ os.Exit(routetoolcli.Run(context.Background(), os.Args[1:], os.Stdout, os.Stderr)) }`。文件头改为指向 `data_service -f <yaml> -route-tool <子命令>`（K8s 用这个入口），本 cmd 只为 dev 的 `go run` 与既有 runbook 命令保留，行为逐字相同。
   工作区: 干净；mtime 2026-10-15 04:21
- [go/data_service/cmd/route_tool/main_test.go] 删除，用例已整体迁入 routetoolcli/cli_test.go。验证命令里 `./cmd/route_tool/...` 仍能编译（没有测试文件）。
   工作区: 干净；mtime 2026-10-15 04:21
- [go/data_service/data_service.go（1b-1 入口；1b-2 启停 leader）] **1b-1：**
- 新 flag：`routeTool = flag.Bool("route-tool", false, "run a route_tool subcommand: data_service -f <yaml> -route-tool <sub> [flags], then exit")`。
- flag.Parse 之后先调纯函数 `selectEntry(migrate, routeTool bool, args []string) (entryMode, error)`：
  - -migrate 与 -route-tool 同时出现 → 错；
  - -route-tool 但没有子命令 → 错；
  - 没有 -route-tool 却带位置参数 → 错，防止把 `data_service -f x audit` 起成一个服务。
  出错时 stderr 打一行，退出码 2。
- -route-tool 模式：把 buildinfo 版本行改打到 stderr（route_tool 的 stdout 只许一行统计），在 conf.MustLoad 之前执行 `os.Exit(routetoolcli.Run(ctx, append([]string{"-f", *configFile}, flag.Args()...), os.Stdout, os.Stderr))`。
- -version 与 -migrate 的现有路径不变。

**1b-2：**
- svcCtx 建好、metrics.Start 之后，如果 svcCtx.RouteReconciler != nil：调 metrics.PrimeRouteReconcile()，再用 safego.Go 跑 Reconciler.Run(rcCtx)。
- 在 `defer svcCtx.Close()` 之后注册 defer，所以按 LIFO 先执行：rcCancel()，再等退出，最多 5s。Run 退出时用独立 2s ctx 释放租约（leader=NULL）。
- 横幅加一行：`reconcile: leader=on arm_ready=<bool>` 或 `off(redis mode)`。
   工作区: 干净；mtime 2026-10-13 05:31；G2a / G2b 也改过它，1b-1 与 1b-2 是两次独立 hunk，落码前先 git diff 确认没有新增他人改动
- [go/data_service/data_service_entry_test.go（新增，package main）] `TestSelectEntry`：表驱动覆盖六种组合：serve、migrate、route-tool 加子命令、route-tool 缺子命令、migrate 加 route-tool、位置参数但没有 route-tool。
   工作区: 新文件，不存在；根包目前没有任何测试文件
- [go/data_service/internal/store/route_reconcile.go（新增，1b-2）] **SQL**：
- R23 续约：`UPDATE route_reconcile SET leader=?, lease_until_ms=<now>+? WHERE id=1 AND (leader IS NULL OR leader=? OR lease_until_ms IS NULL OR lease_until_ms < <now>)`；
- R24 一致读：`SELECT leader, lease_until_ms, <now> FROM route_reconcile WHERE id=1`；
- R25 释放：`UPDATE … SET leader=NULL, lease_until_ms=NULL WHERE id=1 AND leader=?`；
- R26 每拍锁行：`SELECT leader, lease_until_ms, watermark_ms, cache_verified_ms, <now> FROM route_reconcile WHERE id=1 FOR UPDATE`；
- R27 推进：`UPDATE route_reconcile SET watermark_ms=GREATEST(watermark_ms, ?), cache_verified_ms=? WHERE id=1`，实参用 R26 读到的 now，保证同一事务内一致；
- R28 leader 冻结：`UPDATE route_meta SET writable=0 WHERE id=1 AND writable=1`；
- R29 增量扫描：按提案 :813 的模板；
- R30 `SELECT <now>`。

**API**：
- `NewRouteLeaderID(host string, rnd io.Reader) (string, error)`：形如 route_leader@{洗过的 host，截到 78 字节}:{8hex}，满足 validRouteWriter。
- `RouteLease{Held bool; Holder string; LeaseUntilMs, NowMs uint64}`。
- `PeekReconcileLease(ctx)`：R24。
- `RenewReconcileLease(ctx, leaderID, leaseMs)`：R23；受影响 0 行时再 R24 判 held（同毫秒续约的边界）。
- `ReleaseReconcileLease`。
- `ReconcileTickRequest{LeaderID string; Marker *placement.RouteReadyMarker; ToleranceMs uint64; IdentityVerified bool}`；`ReconcileTickResult{Outcome(Advanced|Frozen|AlreadyFrozen|LeaseLost); FreezeReason(watermark_regressed|generation_mismatch|cache_ahead); Generation string; WatermarkMs, CacheVerifiedMs, NowMs uint64}`。
- `AdvanceReconcile(ctx, req)`：retryRouteTx 只重试 1213 / 9007。事务内：R26 → R18 普通读 → 纯函数 `decideReconcileDB(row, meta, marker, tol, leaderID)` → 视结论执行 R28 或 R27 → COMMIT → 返回提交后的水位（max(old, now)）。
- `FreezeByLeader(ctx, leaderID, reason)`：同一事务形状，只做租约校验加 R28，给 cache_ahead 用。
- `RouteUpdatedCursor{UpdatedAtMs, PlayerID uint64}`、`ScanRoutesUpdatedSince(ctx, shard uint32, after, limit)`：校验 shard 小于 64、limit 在 [1, 1000]、返回行的片必须等于入参片（否则 ErrRouteShardMismatch）。
- `DBNowMs(ctx)`。

**租约判据**：leader==me 且 lease_until_ms ≥ now，都在库时钟上判。
   工作区: 新文件，不存在
- [go/data_service/internal/store/route_reconcile_test.go（新增）] - `TestDecideReconcileDB`：表驱动覆盖租约丢失、已冻结、世代不符、水位倒退（容差 0 和 1000 两档）、无标记只推进、IdentityVerified=false 时 cvm 不变；
- 假驱动契约用例：`TestRenewReconcileLeaseAcquireRenewRefuseTakeover`、`TestRenewZeroRowsFallsBackToPeek`、`TestAdvanceReconcileFreezeIssuesNoWatermarkUpdate`（倒退时不得执行 R27）、`TestAdvanceReconcileLeaseLostWritesNothing`、`TestScanRoutesUpdatedSinceSQLAndShardCheck`、`TestNewRouteLeaderIDUniqueAndValid`。
   工作区: 新文件，不存在
- [go/data_service/internal/store/route_reconcile_integration_test.go（新增，integration tag）] 用 storetest 一次性库：
- `TestRouteReconcile_LeaseContention`：两个 store、两个不同 id，只有一个 Held；用 `UPDATE route_reconcile SET lease_until_ms=1` 模拟过期后另一个接管。不靠睡眠，不依赖墙钟。
- `TestRouteReconcile_RegressionFreezesWriters`：标记水位大于库水位 → writable=0，此后 RegisterHome 返回 ErrRouteNotWritable。
- `TestRouteReconcile_WatermarkMonotonic`。
- `TestRouteReconcile_TickAndUnfreezeNoDeadlock`：并发 AdvanceReconcile 与 UnfreezeRouteWriters，记录 lockRetries。
- `TestScanRoutesUpdatedSince_UsesIdxRouteUpdated`：原样打印 EXPLAIN，不断言计划。
   工作区: 新文件，不存在
- [go/data_service/internal/routereconcile/reconciler.go（新增，1b-2）] **消费方接口**：
- Store：ReadRouteMeta、PeekReconcileLease、RenewReconcileLease、ReleaseReconcileLease、AdvanceReconcile、FreezeByLeader、ScanRoutes、ScanRoutesUpdatedSince、GetRoutes、ListMergedInto。
- Cache：Identity、ReadyMarker、ArmReadyMarker、CompareAndSetReady、CompareAndDeleteReady、DeleteReadyUnconditionally（只给 WRONGTYPE 用）、Push、MirrorMergedInto。
- Clock：Now 单调，加 NewTimer。

**并发结构**：一个决策循环 goroutine，独占全部可变状态；一个任务 goroutine，同一时刻最多一个任务；用 atomic 发布「本地租约有效至」。

**每 TickInterval 一拍**，按顺序：
1. 每 RenewInterval 续约一次：非 leader 先 Peek，看起来过期才 Renew；未持有就设 leader 指标为 0、取消任务、结束本拍。
2. 读 Identity：错误、role≠master 或 run_id 缺失 → tick 记 redis_error，不撤标记。
3. 读标记：值是 WRONGTYPE 就无条件 DEL；畸形就 compare-and-delete，当作不存在。
4. AdvanceReconcile（Marker=当前标记，IdentityVerified = VI==当前身份）：
   - Frozen 或 AlreadyFrozen：停止推送；此时若标记身份 ≠ 当前身份或淘汰异常，用 compare-and-delete 撤标记；
   - LeaseLost：降级。
5. 淘汰策略不是 noeviction（CONFIG GET 不可用且 AssumeNoEviction=false 也算），或 evicted_keys>0：撤标记，VI=nil，禁止挂标记。
6. 标记在且身份不等：compare-and-delete，VI=nil，排一个 repair 任务，起点 min(marker.WatermarkMs, cvm)−RepairLookbackMs。
   标记不在、VI 非空且 ≠ 当前身份：排 repair，起点 cvm−RepairLookbackMs。
7. 标记不在且 Arm=true：排 prewarm。
   标记不在且 Arm=false 且 VI==nil：排 catch-up，起点 cvm−RepairLookbackMs，也就是首次当选时的全量。
8. 标记在且身份相等：VI=当前身份；用 CompareAndSet 把 watermark_ms 改成刚提交的值，CAS 失败只计数。
9. 距上次增量满 IncrementalInterval 且没有任务：排 incremental，起点为上一轮起点（库时钟）−IncrementalOverlapMs，并做一次 MirrorMergedInto。

**任务**：
- 共同规则：每批之前检查租约有效与 ctx；按 PushMaxRowsPerSec 用注入的 Clock 节流。
- prewarm：ScanRoutes 全表；repair / catch-up / incremental 对 64 个片各用 ScanRoutesUpdatedSince。
- Push 结果里 Superseded 的行用 GetRoutes 复读：库 rev < 缓存 rev 就 FreezeByLeader(cache_ahead) 并中止。
- prewarm 与 repair 收尾：MirrorMergedInto（只增）。满足 D3 的挂标记条件就 ArmReadyMarker，值的 set_at 取 DBNowMs、wm 取最近一次提交水位；之后复读身份，变了就 compare-and-delete 自己的值；成功则 VI=身份。

**明令不做**：任何对 merge:in_progress:* 的写或删（B2）。

**Run 退出**：取消任务、释放租约。
   工作区: 新目录与新文件，不存在
- [go/data_service/internal/routereconcile/cache.go（新增）] **客户端**：`NewRedisCache(mapping redis.RedisConf, rc config.RouteStoreConfig) (*RedisCache, error)`。
- mapping.Type 非空且 ≠ "node" 就拒绝：多键脚本与 INFO 语义要求单实例。
- 独立 `goredis.NewClient`：Addr=Host、Password=Pass、DB 0、PoolSize=Reconcile.RedisPoolSize、MaxRetries=-1（关掉 go-redis 自身重试，v9 的语义【未核实】，用例里取证）、ReadTimeout=WaitAOFTimeoutMs+1s、Tls 时带 TLSConfig。
- 理由：熔断器与连接池都和 Router 的 go-zero 客户端隔离（先例 ProbeWaitAOF）；只有 leader 进程会真正建连接。

**Identity**：一个 pipeline 发 `INFO server`、`INFO replication`、`INFO stats`、`CONFIG GET maxmemory-policy`。纯函数 `parseRedisInfo` 取 run_id、role、master_replid、evicted_keys；CONFIG 回错误回复时 policy 记为 unknown。

**标记操作**：
- ArmReadyMarker：同连接 pipeline 发 `SET key v NX` 加 `Do("WAITAOF", local, replicas, timeout)`，复用 routing 的 checkWaitAOFReply 判据，需要导出或复制一份纯函数，注明出处。WAITAOF 两项都为 0 时不发。
- CAS / CAD：用 placement 的脚本，EVAL 即可（频率 1 次每秒）。

**Push(rows)**：
1. 一次 MGET 取 zone / placement / maprev（3N 键）；
2. 与 (HomeValue, PlacementValue, FormatRev) 全等的行计 skipped_current；墓碑行计 tombstone_skipped，不推（推了会造出只有 maprev 的键，1a 工具判它畸形）；落点无效计 db_invalid；
3. 剩下的行 pipeline 发 `EvalSha(sha1(PublishRouteScript), PublishRouteKeys(id, home), rev, home, plc)`，home 为 0 时不带围栏键；
4. 任一回复是 NOSCRIPT：ScriptLoad 后只重发这些行，脚本幂等；
5. 用 ClassifyPublishReply 分类，网络错误整批返回 error。

**MirrorMergedInto**：对每行发 `SET merge:merged_into:{src} <EncodeMergedIntoValue> NX`；没写成就 GET 解析：同 dst 同 run_id 计 present，否则计 conflict。永不覆盖、永不删除。
   工作区: 新文件，不存在
- [go/data_service/internal/routereconcile/reconciler_test.go（新增）] 假 Store、假 Cache、假 Clock，直接调 tickOnce / runTask，不起计时器，不依赖墙钟。用例：
- `NotConstructedInRedisMode`；
- `NonLeaderOnlyPeeks`：不发 Renew，不发 Redis 写；
- `PrewarmArmsOnlyWhenArmEnabled`；
- `PrewarmConvergesWithNewerConcurrentPublish`：superseded 加库复读 rev 更大 → 不冻结，照常挂标记；
- `CacheAheadFreezes`：调 FreezeByLeader(cache_ahead)，不挂标记，后续不再推送；
- `IdentityChangeDropsMarkerBeforeRepair`：断言顺序 CAD → 64 片扫描 → Push → Arm，起点取 min(标记水位, cvm)−60s；
- `OldBackupMarkerUsesItsWatermarkAsLookbackStart`；
- `IdentityFlipDuringPrewarmDoesNotArm`；
- `EvictionPolicyOrEvictedKeysRefuseArmingAndDrop`；
- `WatermarkRegressionFreezesAndKeepsMarker`；
- `FrozenStopsPushesButDropsOnIdentityChange`；
- `LeaseExpiryMidPrewarmAbortsBeforeNextBatch`：推进假钟；
- `StaleArmIsDroppedNextTick`；
- `LegacyConflictOrFencedBlocksArming`；
- `NeverTouchesMergeFenceKeys`：断言 Cache 收到的键里没有 merge:in_progress:*；
- `IncrementalWindowCursorMath`；
- `ShutdownReleasesLease`。
   工作区: 新文件，不存在
- [go/data_service/internal/routereconcile/cache_test.go（新增）] 用 miniredis：
- `TestPushSkipsCurrentRowsWithoutEval`：用 SetPreHook 计数，零 EVALSHA；
- `TestPushAppliesAndClassifies`；
- `TestPushReloadsOnNoScript`：两批之间 SCRIPT FLUSH；
- `TestPushTombstoneSkipped`；
- `TestReadyCASAndCAD`；
- `TestArmReadyNXDoesNotOverwrite`：WAITAOF 配 0/0，miniredis 是否支持 WAITAOF【未核实，Q4】；
- `TestMirrorMergedIntoAddOnly`；
- `TestParseRedisInfo`：用捕获的 Redis 7.2 INFO 文本做表驱动，含 role:slave、缺 run_id、CRLF。

miniredis 对 INFO 的支持【未核实】，所以身份解析只测纯函数，不经 miniredis。
   工作区: 新文件，不存在
- [go/data_service/internal/routereconcile/cache_integration_test.go（新增，integration tag）] 需要 `DATA_SERVICE_IT_REDIS_HOST`，指向一次性 redis:7.2 --appendonly yes；`itKeyPatterns` 非空时跳过，同 routing 的做法。用例：
- `TestRealRedisIdentityParses`：真 INFO 能解析，role=master，CONFIG GET 返回 noeviction；
- `TestRealRedisArmWithWaitAOF`：1/0 得到确认；
- `TestRealRedisEvalShaAfterScriptFlush`；
- `TestRealRedisMaxRetriesDisabled`：记录 go-redis 的行为，作为取证。
   工作区: 新文件，不存在
- [go/data_service/internal/config/config.go（1b-2）] 在 RouteStoreConfig 下新增 `Reconcile RouteReconcileConfig json:",optional"`。字段（零值或 nil 表示没配，由 Normalize 填默认）：
- Enabled *bool：nil 当 true；
- ArmReadyMarker bool：默认 false；
- LeaseMs：10000，范围 [3000, 60000]；
- RenewIntervalMs：3000，要求 ≤ (LeaseMs − LeaseMs/5)/2；
- TickIntervalMs：1000，范围 [200, 10000]；
- IncrementalIntervalMs：30000，范围 [1000, 600000]；
- IncrementalOverlapMs：15000，范围 [5000, 600000]；
- RepairLookbackMs：60000，范围 [15000, 86400000]；
- RollbackToleranceMs *int：nil 当 0，范围 [0, 60000]；
- PushBatch：500，范围 [1, 1000]（与 store.RouteBatchLimit 同值，注释写明；config 不 import store）；
- PushMaxRowsPerSec：20000，范围 [100, 1000000]；
- RedisPoolSize：4，范围 [1, 32]；
- AssumeNoEviction bool。

**Validate**：与 Mode 无关一律校验范围；另外 ArmReadyMarker=true 时要求 Mode=db_first 且 Enabled≠false。

**方法**：`ReconcileEnabled()`（db_first 且 Enabled）、`ToleranceMs()`、`LeaseSafetyMs()`（LeaseMs/5）。

**避开他人改动**：本文件第 224 行有他人未提交的 IdSegment 标签改动，与本段不相邻，不要动它。
   工作区: M（他人改动：DefaultIdSegmentBootstrapTags 一行，与本项无关）；mtime 2026-10-15 23:06；冲突风险低，用 Edit 精确插入
- [go/data_service/internal/config/config_test.go] 新增：
- `TestRouteReconcileDefaults`；
- `TestRouteReconcileRejects`：表驱动覆盖各越界、Arm 加 redis 模式、Arm 加 Enabled=false、Renew 太大；
- `TestShippedYamlReconcileIsInert`：仓库 yaml 为 Mode=redis、ArmReadyMarker=false 且校验通过。
   工作区: 干净；mtime 2026-10-13 09:13
- [go/data_service/etc/data_service.yaml（1b-2）] 在 RouteStore 段末加 `Reconcile:` 块，全部写默认值，ArmReadyMarker: false。注释写明三点：
- 只在 db_first 下运行；
- P3 门禁满足后才把 ArmReadyMarker 改成 true，全部副本同一取值；
- 在线清库（currency_crash_window.ps1）前，要么先停 data_service，要么保持 Mode=redis，否则 leader 会把将被清掉的行推回刚 FLUSHALL 的 Redis，或者把水位复位误判成回档。
不动第 221 行他人改的 BootstrapTags。
   工作区: M（他人改动：IdSegment.BootstrapTags 一行）；mtime 2026-10-15 23:06；冲突风险低
- [go/data_service/internal/metrics/metrics.go（1b-2）] 新增指标，全部是封闭集合，不带 player_id 或 zone label：
- `route_reconcile_leader` gauge 0/1；
- `route_reconcile_arm_enabled` gauge 0/1，给告警作闸；
- `route_ready_marker` gauge 0/1，leader 观测值；
- `route_writable` gauge，leader 观测值；
- `route_cache_verified_age_seconds` gauge；
- `route_reconcile_tick_total{result}`：ok / not_leader / lease_lost / db_error / redis_error / frozen；
- `route_reconcile_event_total{event}`：marker_armed / marker_dropped_identity / marker_dropped_eviction / marker_dropped_malformed / arming_refused_eviction / arming_refused_unresolved / freeze_watermark / freeze_generation / freeze_cache_ahead / task_started / task_completed / task_aborted；
- `route_reconcile_push_total{phase,result}`：phase 取 prewarm / repair / incremental，result 取 skipped_current / applied / confirmed / superseded / cache_ahead / conflict / legacy_conflict / fenced / tombstone_skipped / db_invalid / error，共 3×11；
- `route_reconcile_mirror_total{result}`：added / present / conflict / error；
- `route_reconcile_task_seconds{phase}` histogram；
- `route_reconcile_task_rows{phase}` gauge，表示进度。

**预置**：`PrimeRouteReconcile()` 逐个预置 0，只在 leader 被构造时调用。
   工作区: 干净；mtime 2026-10-15 14:03
- [go/data_service/internal/metrics/metrics_test.go] 新增 `TestPrimeRouteReconcileSeries`：各 Vec 的子序列数与封闭集合一致，例如 push 有 33 条。
   工作区: 干净；mtime 2026-10-13 05:29
- [go/data_service/internal/svc/servicecontext.go（1b-2）] - newPlayerRouteStore 内部改为先拿到 *store.RouteStore；赋给接口前继续判空，保住「非 nil 接口包 nil 指针」那道防线。
- 新字段 `RouteReconciler *routereconcile.Reconciler`。只在 c.RouteStore.ReconcileEnabled() 且 route store 装配成功时构造：
  - 依赖：NewRedisCache(c.MappingRedis, c.RouteStore)、store.NewRouteLeaderID(hostname, crypto/rand)、真实 Clock；
  - 构造失败（Type=cluster 等）只打 ERROR，保持 nil，不拒启，也不进 MissingStores；
  - 构造时不开连接，也不起 goroutine，单测可以多次构造。
- Close 不管 reconciler，由 main 先停。
   工作区: 干净；mtime 2026-10-15 04:21
- [go/data_service/internal/svc/servicecontext_missing_stores_test.go] 补表驱动：redis 模式下 RouteReconciler 为 nil；db_first 但 Enabled=false 时为 nil；Mode=redis 时 MissingStores 仍不含 player_route（回归）。
   工作区: 干净；mtime 2026-10-12 15:30
- [go/data_service/internal/routetool/rewarm.go（新增，1b-2）加 cli.go 子命令 `rewarm [-wait=15m] [-poll=5s]`] `Rewarm(ctx, rd, opts)`：
1. GET 标记的原值；
2. 存在就 compare-and-delete，CAS 输了退出码 1；
3. `-wait > 0` 时要求 yaml 是 db_first（否则退出码 2，提示没有 leader），用可注入的 Sleeper 轮询，直到出现「可解码且 set_at_ms 大于被撤值」的新标记，成功退出 0，超时退出 1，并提示检查 ArmReadyMarker 与 leader 指标；
4. `-wait=0` 只撤标记：在 db_first 回退到 redis 时，于读方撤掉之后执行，用来清理标记。
标记只有 leader 写（单写者），所以 rewarm 不自己推送。
   工作区: 新文件，不存在；cli.go / cli_test.go / routetool_test.go 同批追加用例：TestRewarm_DropsByCAS、TestRewarm_WaitNeedsDBFirst、TestRewarm_PollWithInjectedSleeper
- [go/data_service/internal/routetool/auditor.go（1b-2）] AuditOptions 新增 `RequireMapRev bool`，cli 的 audit 加 `-require-maprev` 参数。打开时，库行不是墓碑而缓存 maprev 缺席或小于库 rev，记新类别 `redis_rev_behind`，计入 Differences 与退出码 1。默认关，1a 时期的行为不变。用作 P3「挂标记前 audit=0」的严格口径。routetool_test.go 加 `TestAudit_RequireMapRevCountsBehind`、`TestAudit_DefaultIgnoresMapRev`。
   工作区: 干净；mtime 2026-10-13 09:13
- [docs/design/player-route-mapping-durability-proposal.md] - §11.2 进度表把「第 1b 批」改成「1b-1 / 1b-2 已落码，未编译、未测试；1b-3 未开始」。
- 新增「第 1b 批」一段，内容包括：
  - 改动清单；
  - D1–D8 结论摘要（引用 spec_D1_batch1b.md）；
  - 偏离表 B1–B12；
  - 运行时要点：租约、标记生命周期、回档冻结与恢复流程；
  - 交 Codex 的验证与失败时要保留的内容（沿用 :1022-1031 那张表）；
  - Java 状态。
- §5.4 / §6 / §8.3 正文不回改，只加一句「以 §11.2 第 1b 批为准」。
   工作区: 干净；mtime 2026-10-15 10:45
- [docs/ops/merge-zone-runbook.md] §9.3 补以下内容：
- K8s 入口：`kubectl -n <ns> exec deploy/data-service -- /app/service -f /app/etc/data_service.yaml -route-tool <子命令>`，并说明 -timeout 要按行数设，超大表等 1b-3 的 Job；
- 新子命令与退出码表；
- 「库回档恢复」小节：freeze → reconcile-from-cache（先 dry-run）→ audit 两个方向为 0 → unfreeze -confirm-audited；
- 「Redis 丢失（1b 之后）」：在 db_first 且 ArmReadyMarker 打开时，leader 会自动全量预热并挂标记；§9.2 的手工重放只用于 redis 模式或库与 Redis 同时丢失；
- 回退到 redis 模式时先撤读方，再执行 `rewarm -wait=0`；
- 在线清库与 leader 互相干扰的告诫。
   工作区: 干净；mtime 2026-10-15 14:17
- [docs/PROGRESS.md 与 docs/handoff/handoff-launch-readiness-and-d1-20261013.md] - PROGRESS 末尾追加 1b-1、1b-2 各一条：范围、文件、未编译未测试、给 Codex 的命令、Java 状态。merge=union，只追加。
- 交接文首追加一行 10-16 进度：§4 第 8 项的 1b 拆成 1b-1 / 1b-2 / 1b-3。
   工作区: PROGRESS.md 为 M（他人追加了 196 行），mtime 2026-10-16 04:04，只追加，风险低；交接为 M（他人 +2 行），mtime 2026-10-15 22:15，只在文首段落末追加
- [【另列 1b-3，不在本批】tools/scripts/k8s_deploy.ps1、tools/scripts/tests/k8s_deploy_contract.tests.ps1、deploy/k8s/data-service-route-alerts.yaml（新增）、tools/scripts/k8s_zone_rollback.ps1 及其测试、deploy/k8s/README.md] 它是 P2 切 db_first 的前置，与 WaitAOF 两项、镜像入口并列。内容：
- 按档位把 RouteStore.FenceShards 写进 ConfigMap：MySQL 档 16；TiDB 档 64，等 k8s_deploy 有 TiDB 档位、也就是 TiDB Phase 1 部署方式定了再补；
- Reconcile 段与 WaitAOF 两项（redis-ha 的 NumReplicas=1）；
- 契约用例：钉住 FenceShards 在 [1, 64] 并等于档位值，Reconcile 在 redis 档保持默认值；
- 告警：NoLeader `sum(leader)==0 for 1m`、MultiLeader `>1 for 1m`、ReadyMissing `max(arm_enabled)==1 and max(ready_marker)==0 for 10m`、WritesFrozen `min(route_writable)==0`、Rollback/CacheAhead 事件 `increase>0`、ArmingRefused、PushConflicts、CacheVerifyStale `>30s for 2m`；
- zone 回档脚本加入 freeze / reconcile-from-cache / audit / unfreeze 四步；
- Redis 连接总账计入 leader 的 RedisPoolSize；
- 可选的 route-tool Job 清单。

另列的理由：K8s 各档位的 Mode 固定为 redis，这些键在切 db_first 之前没有任何运行效果；而 k8s_deploy.ps1 与契约测试眼下有他人 +666 / +741 行未提交改动，冲突风险高，不宜夹进 1b。
   工作区: k8s_deploy.ps1 为 M（他人 +666/−28，mtime 2026-10-16 03:53）；契约测试为 M（他人 +741，mtime 2026-10-16 03:50）；冲突风险高，1b-3 开工前重新核对
## compat_and_rollout
**无 schema 变更**：不改 DDL、proto、C++，也不改客户端契约；新表列、新键沿用 1a 已建好的（route_reconcile 的四列、mapping:route:ready）。

**1b-1**：在役 data_service 的行为只有一处变化：没有 -route-tool 却带位置参数时，退出码 2 拒启。K8s 的 args 只有 -f（data-service.yaml:80），不受影响。新的 store 方法只被工具调用。cmd/route_tool 保留，命令行逐字兼容。

**1b-2**：leader 只在 db_first 加 Enabled 时构造。当前所有档位都是 redis（本地 yaml 显式 redis，K8s ConfigMap 固定 redis），所以任何已部署环境零运行变化。新配置键都是可选的，带默认值；非法值不论 Mode 都拒启，与 1a 同口径。混跑窗口里，1a 的二进制既不读写 route_reconcile 也不碰标记，与新 leader 不冲突。标记格式 v=1，以后只加字段。

**与提案 §7 的衔接**：
1. P2：切 db_first，此时 Enabled 默认 true、ArmReadyMarker=false。leader 推送补齐 maprev 和孤儿行，做水位与回档检测。
2. P3：二次导入（-allow-db-first），然后 `audit -require-maprev` 退出 0。
3. 全部副本把 ArmReadyMarker 改 true，滚动重启。leader 全量预热并挂标记，再跑一次 audit 确认为 0。
4. P4：第 2 批 merge_zone 库先写，同批补围栏镜像双向对账（B2）。
5. P5：第 3 批读方开始要求标记。

**K8s**：1b-3 落地之前不得在 K8s 切 db_first，因为那时 ConfigMap 缺 FenceShards、Reconcile、WaitAOF，也没有告警与回档脚本步骤。

**回退**，按代价从低到高：
- ArmReadyMarker 改回 false：不再挂标记，已有标记照常维护，身份变化时撤掉。
- Enabled=false：急停 leader，孤儿行无人补发，回档不再检测；第 3 批读方开启前可以接受。
- 回到 redis 模式：先撤读方（第 3 批的开关），再撤写方，然后 `route_tool rewarm -wait=0` 撤掉标记；重新前进时按提案 §7 重新冻结、重跑 P3。
- 回退不删表，也不清 route_reconcile。
## tests
- **新功能用例的「红」**：在 HEAD 上因为符号不存在而编译失败就算红。Codex 在 HEAD 上记录一次 `go vet ./internal/store/... ./internal/routetool/... ./internal/routereconcile/...` 的编译错误作为红证据即可，不需要另开 worktree。
- **data_service 根包** `TestSelectEntry`：六种组合，其中「没有 -route-tool 却带位置参数」在 HEAD 上会被静默忽略，是行为红。
- **placement**：
- `TestRouteReadyMarkerRoundTrip`；
- `TestRouteReadyMarkerRejects`（v、generation、run_id 长度、watermark=0、leader 非 ASCII）；
- `TestRouteReadyScriptsShape`；
- 1b-2 加 `TestMergedIntoValueMatchesMergeZoneEncoding`。
- **store 纯函数**：
- `TestDecideRepair`：覆盖库无行、maprev 大于 / 等于 / 小于库 rev、同 rev 值同或不同且 resolveTies 开关两态、无 maprev 退回导入语义、库行无效；
- `TestDecideReconcileDB`：覆盖租约丢失、已冻结、世代不符、水位倒退（容差 0 和 1000）、正常推进、IdentityVerified=false 时 cvm 不变；
- `TestNewRouteLeaderIDUniqueAndValid`。
- **store AST 守卫**：`TestRouteMetaAndReconcileWritesOnlyInStoreFiles` 守住只有 route_store.go 和 route_reconcile.go 能写这两张表，命中数必须大于 0；既有的 `TestNoPlayerRouteWritesOutsideRouteStore` 保持绿。
- **store 假驱动契约**：
- `TestRepairRoutesFromCacheRefusedWhenWritable`（零写）；
- `TestRepairRoutesFromCacheStatementSequence`；
- `TestRepairRoutesFromCacheDryRunOpensNoTx`；
- `TestRepairMergedIntoRequiresFrozen`；
- `TestFreezeRouteWritersIdempotent`；
- `TestUnfreezeRaisesWatermarkBeforeFlippingWritable`；
- `TestUnfreezeAlreadyWritableIsNoop`；
- `TestRenewReconcileLeaseAcquireRenewRefuseTakeover`；
- `TestRenewZeroRowsFallsBackToPeek`；
- `TestAdvanceReconcileFreezeIssuesNoWatermarkUpdate`；
- `TestAdvanceReconcileLeaseLostWritesNothing`；
- `TestScanRoutesUpdatedSinceSQLAndShardCheck`。
- **store 真库**（integration tag，MySQL ≥ 8.0.16）：
- `TestRouteStore_RepairFromCacheRestoresRolledBackRows`：含幂等重跑，冻结期 RegisterHome 返回 ErrRouteNotWritable；
- `TestRouteStore_RepairTieReportedUnlessResolved`；
- `TestRouteStore_FreezeUnfreezeRoundTrip`；
- `TestRouteReconcile_LeaseContention`：用 UPDATE lease_until_ms=1 模拟过期，不睡眠；
- `TestRouteReconcile_RegressionFreezesWriters`；
- `TestRouteReconcile_WatermarkMonotonic`；
- `TestRouteReconcile_TickAndUnfreezeNoDeadlock`；
- `TestScanRoutesUpdatedSince_UsesIdxRouteUpdated`：原样记录 EXPLAIN。
- **routetool**（miniredis 加假 DB）：
- ReconcileFromCache：`RefusedWhenWritable`、`FenceExitsThree`、`StatelessDedupeAcrossPasses`、`MapRevOnlyIsMalformed`、`TiesReportedByDefault`、`ResolveTiesCache`、`DryRunWritesNothing`、`MergedIntoInsertedNeverDeleted`、`GraceUsesInjectedSleeper`；
- 1b-2 加 `TestRewarm_DropsByCAS`、`TestRewarm_WaitNeedsDBFirst`、`TestRewarm_PollWithInjectedSleeper`、`TestAudit_RequireMapRevCountsBehind`、`TestAudit_DefaultIgnoresMapRev`；
- 真库端到端：`TestReconcileFromCache_RealDBEndToEnd`。
- **routetoolcli**：
- 迁入的 5 个 `TestRun*`；
- `TestRunSubcommandFlagsAreScoped`；
- `TestRunReconcileAndUnfreezeRefusedInRedisMode`；
- `TestRunUnfreezeNeedsConfirmAudited`；
- `TestRunFreezeUnreachableDBExitsTwo`；
- `TestRunStdoutCarriesOnlyTheSummary`。
- **routereconcile**（假 Store / Cache / Clock，直接调 tickOnce / runTask，不依赖墙钟）：
- `NotConstructedInRedisMode`；
- `NonLeaderOnlyPeeks`；
- `PrewarmArmsOnlyWhenArmEnabled`；
- `PrewarmConvergesWithNewerConcurrentPublish`；
- `CacheAheadFreezes`；
- `IdentityChangeDropsMarkerBeforeRepair`（断言调用顺序和回看起点）；
- `OldBackupMarkerUsesItsWatermarkAsLookbackStart`；
- `IdentityFlipDuringPrewarmDoesNotArm`；
- `EvictionPolicyOrEvictedKeysRefuseArmingAndDrop`；
- `WatermarkRegressionFreezesAndKeepsMarker`；
- `FrozenStopsPushesButDropsOnIdentityChange`；
- `LeaseExpiryMidPrewarmAbortsBeforeNextBatch`；
- `StaleArmIsDroppedNextTick`；
- `LegacyConflictOrFencedBlocksArming`；
- `NeverTouchesMergeFenceKeys`；
- `IncrementalWindowCursorMath`；
- `ShutdownReleasesLease`。
- **routereconcile cache**（miniredis）：
- `TestPushSkipsCurrentRowsWithoutEval`；
- `TestPushAppliesAndClassifies`；
- `TestPushReloadsOnNoScript`；
- `TestPushTombstoneSkipped`；
- `TestReadyCASAndCAD`；
- `TestArmReadyNXDoesNotOverwrite`；
- `TestMirrorMergedIntoAddOnly`；
- `TestParseRedisInfo`（捕获的 7.2 文本，含 role:slave、缺字段、CRLF）。
- **routereconcile 真 Redis**（integration tag，一次性 redis:7.2 加 AOF）：
- `TestRealRedisIdentityParses`；
- `TestRealRedisArmWithWaitAOF`；
- `TestRealRedisEvalShaAfterScriptFlush`；
- `TestRealRedisMaxRetriesDisabled`（取证用）。
- **config / metrics / svc**：
- `TestRouteReconcileDefaults`；
- `TestRouteReconcileRejects`；
- `TestShippedYamlReconcileIsInert`；
- `TestPrimeRouteReconcileSeries`；
- servicecontext 表驱动：redis 模式或 Enabled=false 时 RouteReconciler 为 nil。
## verify_commands
- **通用约定**：
- 工作目录以仓库根为起点，PowerShell 串行执行，前一步红了就停下归因，不连续重试，不 `gofmt -w`，不改用例迁就；
- 失败时按提案 :1022-1031 的表原样保留：gofmt -d 输出、第一条编译错误原文、失败用例的完整 -v 输出、MySQL 错误号、`SELECT VERSION()`、`SHOW CREATE TABLE player_route`；
- 口令不进回报；
- 前置：先按复验清单、第 0 / 1a / G1a / P5 各段把已落码部分跑完（提案 §11.2），再跑本批。
- **1b-1 第 1 步**：`cd go/shared`
- `gofmt -l ./placement`：期望无输出；
- `go vet ./placement/...`；
- `go test -count=1 -v ./placement/... -run "RouteReady|Route|Rev|Publish"`：全部 PASS；
- 记录 `go test ./placement/... -list . | Measure-Object -Line` 得到的用例数。
- **1b-1 第 2 步**：`cd go/data_service`
- `gofmt -l ./internal/store ./internal/routetool ./internal/routetoolcli ./cmd/route_tool ./data_service.go ./data_service_entry_test.go`：期望无输出；
- `go build ./...`；
- `go vet ./internal/store/... ./internal/routetool/... ./internal/routetoolcli/... ./cmd/route_tool/... .`；
- `go test -count=1 -v ./internal/store/... -run "DecideRepair|Repair|Freeze|Unfreeze|RouteMeta|NoPlayerRouteWrites|WritesOnlyInStoreFiles"`；
- `go test -count=1 ./internal/store/... ./internal/routetool/... ./internal/routetoolcli/... ./cmd/route_tool/... .`；
- 通过标准：全绿；模块级 build / vet 带进的他人改动按文件归因，单独列出。
- **1b-1 第 3 步：MySQL 真库**
- 环境：`$env:DATA_SERVICE_IT_MYSQL_HOST='127.0.0.1:3306'`、USER=root、PASSWORD 取 compose，不进日志；
- `go test -tags=integration -count=1 -v ./internal/store/... -run "Repair|Freeze|Unfreeze"`；
- 已知红项 `TestMigrateSchema_UpgradesLegacyHandWrittenTables` 不在这个正则里；
- 出现 SKIP 如实报「未验证」。
- **1b-1 第 4 步：Redis 加 MySQL 端到端**
- `docker run -d --rm --name ds-it-redis -p 127.0.0.1:6390:6379 redis:7.2 redis-server --appendonly yes`（本地没有镜像需要拉取时先征得用户同意）；
- `$env:DATA_SERVICE_IT_REDIS_HOST='127.0.0.1:6390'`；
- `go test -tags=integration -count=1 -v ./internal/routetool/... -run "ReconcileFromCache"`；
- 跑完 `docker rm -f ds-it-redis`。
- **1b-1 第 5 步：入口冒烟**（本机，不用 go run 断言退出码）
- `go build -o "$env:TEMP\ds1b.exe" .`；
- `& "$env:TEMP\ds1b.exe" -f etc/data_service.yaml -route-tool audit 1>out.txt; "exit=$LASTEXITCODE"`：期望 out.txt 只有一行 `route_audit …`，版本行在 stderr；
- `& "$env:TEMP\ds1b.exe" -f etc/data_service.yaml audit; "exit=$LASTEXITCODE"`：期望 exit=2，且没有起服务；
- `& "$env:TEMP\ds1b.exe" -f etc/data_service.yaml -migrate -route-tool audit`：期望 exit=2；
- 最后 `Remove-Item` 掉 exe 和 out.txt。
- **1b-2 第 1 步**：`cd go/data_service`
- `gofmt -l ./internal/store ./internal/routereconcile ./internal/config ./internal/metrics ./internal/svc ./internal/routetool ./internal/routetoolcli ./data_service.go`：期望无输出；
- `go build ./...`；
- `go vet ./...`；
- `go test -count=1 -v ./internal/store/... -run "ReconcileLease|AdvanceReconcile|DecideReconcileDB|ScanRoutesUpdatedSince|LeaderID|WritesOnlyInStoreFiles"`；
- `go test -count=1 -v ./internal/routereconcile/...`；
- `go test -count=1 -v ./internal/config/... -run "RouteStore|Reconcile|ShippedYaml"`；
- `go test -count=1 -v ./internal/metrics/... -run PrimeRouteReconcile`；
- `go test -count=1 ./internal/svc/... ./internal/routetool/... ./internal/routetoolcli/...`；
- 最后 `go test -count=1 ./...` 全量，既有失败按复验清单第 4 步归因。
- **1b-2 第 2 步：真库**
- 同 1b-1 第 3 步的环境：`go test -tags=integration -count=1 -v ./internal/store/... -run "RouteReconcile|ScanRoutesUpdatedSince"`；
- 原样保留 EXPLAIN 和 lockRetries。
- **1b-2 第 3 步：真 Redis**
- 一次性 redis:7.2 加 AOF，做法同上：`go test -tags=integration -count=1 -v ./internal/routereconcile/...`；
- `TestRealRedisArmWithWaitAOF` 与 `TestRealRedisEvalShaAfterScriptFlush` 必须 PASS，不能 Skip；
- 回报 `TestRealRedisMaxRetriesDisabled` 记录到的 go-redis 行为。
- **1b-2 第 4 步（可选）：本机全栈冒烟**，要求用户同意重启本机共享 Redis，且期间不跑 merge-zone 任何写模式：
1. 本地 yaml 临时改为 Mode=db_first、ArmReadyMarker=true，按 runbook §9.3 先导入、audit 为 0；
2. 起 data_service，确认 `data_service_route_reconcile_leader` 为 1，并依次出现 event task_completed 与 marker_armed；
3. `docker restart redis` 后，期望依次出现 marker_dropped_identity、task_completed（repair）、marker_armed；
4. `audit -require-maprev` 退出 0；
5. 做完还原 yaml。
- **不需要的项**：不重编 C++（没有 proto 变更），不跑任何压测。建角与 leader 推送的压测并入 P2 门禁的 L-12 压测，由负责人另行安排，按 AGENTS §6 出对比表。
## doc_updates
- docs/handoff/launch-readiness-20261013/spec_D1_batch1b.md（新增）：D1–D8 设计结论、不变量 I1–I3、偏离 B1–B12、文件清单
- docs/design/player-route-mapping-durability-proposal.md：
- §11.2 进度表与新增的「第 1b 批」段：改动、偏离、运行时要点、验证、Java 状态；
- §5.4 / §6 / §8.3 各加一句「以 §11.2 第 1b 批为准」，原文不回改
- docs/ops/merge-zone-runbook.md：
- §9.3 补 K8s 入口 `/app/service -f … -route-tool <子命令>`、新子命令与退出码；
- 新增「库回档恢复（freeze → reconcile-from-cache → audit → unfreeze）」「Redis 丢失（1b 之后由 leader 自动预热）」两节；
- 回退到 redis 模式时用 `rewarm -wait=0`；
- 在线清库与 leader 互相干扰的告诫
- docs/PROGRESS.md：1b-1、1b-2 各追加一条（只追加，写明未编译未测试、给 Codex 的命令、Java 状态）
- docs/handoff/handoff-launch-readiness-and-d1-20261013.md：文首追加 10-16 进度一行（§4 第 8 项的 1b 拆成 1b-1 / 1b-2 / 1b-3）
- go/data_service/etc/data_service.yaml 与 cmd/route_tool/main.go 的注释：入口与 Reconcile 段说明
- Java 仓库 D:\luyuan\xuanming-server-mmo\PARITY.md：第 1 批（1a + 1b）全部落码后按 Q14 追加一行，只追加，那里有他人未提交改动
- 1b-3 落地时：deploy/k8s/README.md 补 route-tool 运维入口与 Redis 连接总账，以及 AGENTS.md §6.2 的 Q8（仍待负责人确认，本批不改）
## java_parity
不需要对等改动，状态登记为「不适用（结构不同）」。依据：Java 版的归属区就是库内列 game.player.zone_id。读方 PlayerHomeZones 直接按主键读库，查不到或 zone_id=0 视为缺项，SQL 出错抛 DependencyException（D:\luyuan\xuanming-server-mmo\game-common\src\main\java\com\game\common\player\PlayerHomeZones.java:11-21；PARITY.md:139）。Java 没有 Redis 完整副本，所以就绪标记、对账 leader、身份检测、回档检测与反向修库都没有作用对象。

1b 只涉及服务端内部存储，不改客户端契约（帧、客户端可见 proto、tip 码、配表都不变），不需要两版同批改。按提案 Q14，第 1 批（1a + 1b）全部落码后在 Java 的 PARITY.md 追加一行，写明三点：
- Java 归属区本来就以库为真源；
- 如果将来给归属区加 Redis 缓存，必须做「完整副本 + 就绪标记」或「未命中按主键回源」，不得把缓存缺键当成缺席；
- 本仓库对应的提交号。

本仓库的 PROGRESS 条目注明「对应 Java 版本号：无」。
## risks
- **S3 规模下的恢复时长**（推算，未测）：N_reg 到 10 亿行（capacity-model:77），单个 mapping Redis 执行 EVALSHA 发布脚本估计每秒 5~10 万行，Redis 整体丢失后的全量预热要数小时；mapping 三键还需要 100~300GB（:290），超出单实例。预热期间，第 3 批读方开启后 go/db 写延后、guild 按封锁处理。反向修库要 SCAN 全部缓存键并逐批读库，冻结期间全服建角不可用，也是小时级。见待拍板第 1 条。
- **回看修补正确的前提**：不变量 I1 / I2 / I3。具体是：映射写事务从写 updated_at_ms 到发布完成小于 60s；库时钟偏差小于 1s（TiDB 的 NOW() 取各 tidb-server 本机时钟，【未核实】）；redis-ha 复制滞后不超过 10s。任一条被打破，就可能修补漏行却重新挂标记，读方把漏掉的行当成权威缺席。
- 「标记落盘 ⇒ 此前所有推送已落盘」依赖 WAITAOF 按全局复制偏移判定的语义，是按 Redis 7.2 语义推断的，【未核实】。真 Redis 用例只能证明确认份数，证明不了偏移语义。
- **回档检测容差默认 0**：MySQL 异步复制切主、丢了最后一拍水位写入，也会冻结建角，属于误报但安全；只能人工 reconcile-from-cache 加 unfreeze 解冻。反过来，把容差调大会漏检秒级回档（提案 §8.3 第 6 条）。
- **过期 leader**：由 SQL 租约谓词、自证标记和本地单调时钟降级共同兜底，但过期 leader 挂上的标记最多会存活一拍，与提案 §5.1 的残留窗口同一类。另外 TiDB 各节点时钟偏差可能让新 leader 提前接管（偏差小于 LeaseSafety 的 2s 时安全，【未核实】）。
- **围栏镜像推迟到第 2 批**（B2）：1b 期间 Redis 整体丢失后，merge_zone 写的 Redis 围栏无法从库恢复。好在冻结窗口本来就禁止合服，影响受限。第 2 批必须同批实现「库到镜像」的增和删两个方向。
- **写者纪律**：手工 redis-cli SET、旧版 merge_zone 这类绕过脚本的写者会让推送回 conflict 或 legacy_conflict，挂标记被阻塞、读方 fail-closed，只能靠告警与 audit 人工处理（提案 §8.3 第 1 条）。
- **db_first 下跑在线清库会与 leader 互相干扰**：currency_crash_window.ps1 先 FLUSHALL，再清表、把水位复位为 0（:114-115、:189、:220）。leader 可能把将被清掉的行推回刚清空的 Redis，或者把水位复位误判成回档并冻结建角。本批只写进文档告诫，脚本逻辑不改。
- **import / audit 仍在内存里保留 O(N) 的已见 id 集合**（importer.go:157-158、auditor.go:262-266），到 10 亿行不可行。本批只把反向修库做成无状态去重；import / audit 的改造登记为后续项。
- **推送负载**：开服峰值（建角 5 万每秒）时，增量推送每 30s 读出的行数乘以 3 把键的 MGET 会落在共享的 mapping Redis 上。已经用 PushMaxRowsPerSec 限速，峰值时增量推送可能落后（不丢，窗口连续），需要纳入 L-12 压测。
- **外部依赖语义未核实**：go-redis v9 里 `MaxRetries=-1` 是否就是关闭重试；托管 Redis 可能禁用 INFO / CONFIG（AssumeNoEviction 的降级会削弱自检）；miniredis 对 INFO / WAITAOF 的支持（Q4）。
- **基线未验证**：1b 叠在未编译、未验证的 1a / G1a / P5 代码上，验证顺序必须先跑复验清单和既有各段。新增的 leader 让每个 db_first 副本多一个 go-redis 池（只有 leader 真正建连接），1b-3 要把它计入 Redis 连接总账。
## open_decisions
- **S3 规模下的恢复时长是否可接受**：Redis 整体丢失后全量预热约数小时，全局库回档后反向修库期间全服禁建角也是小时级，都是推算。
- 不可接受的话，要在第 3 批读方落码前把就绪标记改为按行片的 64 个 `mapping:route:ready:{shard}`：读方 MGET 的第三把键换成本片标记，预热每完成一片就挂一片；反向修库也改为按片冻结。这改的是读方契约，必须在 1b-2 落码前定。
- 推荐：1b 先用单标记，同时打出 task_rows / task_seconds 指标，压测出实测速率后再定。
- **回档检测容差默认值**：
- 取 0：fail-closed，MySQL 异步切主丢最后一拍水位也会冻结建角，要人工反向修库后解冻；
- 取秒级：减少误报，但会漏检秒级回档，而回档可能带走已确认的登记。
- 推荐 0。

# VERDICT
## overall
规格的事实底座基本可靠。23 条 current_facts 里，22 条经亲自打开代码确认属实；k8s_deploy.ps1 那一条内容属实，但行号和 diff 统计已经过时：文件仍在被他人快速改动，当前 RouteStore 段在 :5919-5922。

结论层面，以下几点成立：
- redis 模式下不构造 leader；
- 标记用 string JSON；
- 收敛依靠 rev 单调加 set-if-newer；
- 锁序为 route_reconcile → route_meta；
- 租约按库时钟判定，再加 SQL 谓词做 fencing；
- 反向修库以 rev=maprev 做 CAS；
- B2 推迟围栏镜像；
- Java 判为不适用(已亲核 PlayerHomeZones)；
- FenceShards 另列到 1b-3，理由是 K8s 固定 redis 模式、且该脚本冲突风险高。

问题集中在 leader 状态机的几处空白，照着现在的规格落码会出运行期问题：
1. Arm=false 下 catch-up 收尾不设 VI，cvm 的减法也没要求饱和(must)。后果是全表 catch-up 无限重排，或首次 catch-up 静默成为空操作。
2. 增量推送的游标没有持久化，初值也没定义(must)。每次滚动发布导致 leader 切换后，孤儿行可能永远补不上，P3 的 `audit -require-maprev = 0` 门禁会反复失守。
3. Arm=false 期间其实没有水位回档检测，与 compat 的陈述矛盾。
4. 恢复流程有三处会卡住或放过危险状态：冻结期 audit 达不到 0；unfreeze 不防标记缺失；连不上 Redis 的 leader 一直占着租约。
5. 监控：gauge 预置 0 加 min/sum 的告警写法会永远误报，或者漏报。
6. NewRouteLeaderID 的长度算成 100 字节，超出 96。
7. 关停顺序会挤掉 Kafka 的 flush(go-zero 5.5s 强杀)。
8. TiDB 缓存表下冻结语句的超时没写。

另外，exec 进在役 Pod 跑 O(N) 工具有 OOM 风险；把测试改名为 TestRun* 会让前几批的验证命令静默丢失覆盖。

建议：两条 must 补进 D 节，并配上对应用例，然后再落 1b-2。1b-1(入口、工具、库原语)基本可以按原样开工，前提是修正反向修库里普通读复核冻结态、测试改名、修库时同页去重这三处。should 级的监控和关停项可以与 1b-2 同批改；告警表达式在 1b-3 按 leader 过滤。
## fact_checks
- [confirmed] 第 1b 批未开始，D1 后续批仍挂在开服交接 §4 第 8 项
   亲阅 docs/design/player-route-mapping-durability-proposal.md:437「第 1b 批 | 未开始」；docs/PROGRESS.md:8208「第 8 项(D1 第 1b / 2 / 3 批)」仍未做；docs/handoff/handoff-launch-readiness-and-d1-20261013.md:155-159。PROGRESS 9044-9052(10-16 全局单点订正)只是改派说明，没有 1b 落码；go/data_service/internal/ 下不存在 routereconcile、routetoolcli 目录(ls 亲核)。
- [confirmed] 就绪标记键只定义、无读写方
   go/shared/placement/route.go:28-30 亲阅。grep 全仓 go/ 与 tools/：除定义外只有 go/shared/placement/route_test.go:36-37 断言常量值，无生产读写方。「只有这一处」的说法略不精确(漏了测试引用)，不影响结论。
- [confirmed] route_reconcile 现有列 leader / lease_until_ms / watermark_ms / cache_verified_ms，无 epoch 列；迁移只种 id=1 行
   亲阅 go/data_service/internal/store/schema.go:271-278(DDL，leader 为 varchar(96) ascii)；route_store.go:31(文件头)、:247-250(R16)、:1794-1815(bootstrapRouteReconcileRow)。
- [confirmed] route_meta 由迁移种随机 generation 与 writable=1，ODKU no-op；W1 对 writable 普通读
   亲阅 route_store.go:1766-1792、:643-654(注释「1b 的流程要先等它们结束」)、:1816-1824(newRouteGeneration 32 hex)。
- [confirmed] 库时钟统一用 dbNowMsSQL，updated_at_ms 在语句执行时取值
   亲阅 route_store.go:153-154；R4 :192-196、R6a/R6b :203-211、R9 :227-230 均嵌入 dbNowMsSQL。TiDB 各 tidb-server 时钟偏差仍属未核实(调研者已标)。
- [confirmed] 发布脚本 set-if-newer、遗留值守卫、touch、可选围栏键；ClassifyPublishReply 只认四种形状
   亲阅 go/shared/placement/route.go:154-207、:274-336。
- [confirmed] W1 发布用 go-zero 客户端、EVAL、拒 HomeZone==0；WAITAOF 探针另建 *redis.Redis
   亲阅 go/data_service/internal/routing/route_publish.go:268-273(HomeZone==0 拒)、:298-304(PipelinedCtx+Eval+Do WAITAOF)、:380-398(ProbeWaitAOF 注释)；router.go:48 redis.MustNewRedis。
- [confirmed] ScanRoutes 沿聚簇键翻页并校验错片；增量推送模板在提案给出
   亲阅 route_store.go:1658-1690；提案 :813、:819-827。
- [confirmed] 导入语义：库无行插入 rev=maprev(无则 1)；maprev>库 rev 记 rev_regression 不写；导入事务要求 writable=1
   亲阅 route_store.go:1415-1465(decideImport)、:1246(importRoutesTx 内 requireRouteWritable)、:1579(importMergedIntoTx 同样)。
- [confirmed] import 与 audit 在内存保留 O(N) 已见 id 集合
   亲阅 go/data_service/internal/routetool/importer.go:157-158、auditor.go:262-266。
- [confirmed] player_route 写语句由 AST 守卫钉在 route_store.go
   亲阅 route_store_test.go:328-388，:334 const allowed。
- [confirmed] route_tool 独立 cmd，conf.Load 保证失败退出码 2，stdout 只一行统计
   亲阅 go/data_service/cmd/route_tool/main.go:9、:26、:104-114(logx 改 stderr + conf.Load)。另：main.go 依赖 svc.MySQLConfigOf(:328)，迁入 routetoolcli 后 routetoolcli→svc 依赖仍成立(无环)。
- [confirmed] data_service 主入口先打版本行到 stdout 再 MustLoad；只认 -migrate / -version，多余位置参数被静默忽略
   亲阅 go/data_service/data_service.go:41-49、:75-89、:94-100。根包无 *_test.go(ls 亲核)。
- [confirmed] K8s 上 data-service 只有一个二进制 /app/service，容器 args 只有 -f
   亲阅 deploy/k8s/Dockerfile.go-svc:139、:220；deploy/k8s/manifests/go-svc/data-service.yaml:80。补充(调研者未提)：同文件 :106-112 内存 limit 512Mi、replicas:2(:36)，且注释 :30 说明多个 zone 的 data-service 共用同一全局库——对 exec 跑工具与全局单 leader 都有影响。
- [refuted] K8s ConfigMap 的 RouteStore 段只写 Mode: redis 与两项池大小(k8s_deploy.ps1:5812-5824)，该脚本有他人 +666/−28 未提交改动、mtime 03:53
   内容属实，但定位与统计已过时：现在 RouteStore 段在 tools/scripts/k8s_deploy.ps1:5919-5922(亲阅)，:5812-5824 处已是 login 排队的 QueueToken 代码；git diff --stat 现为 +825(契约测试 +978，规格写 +741)，mtime 2026-10-16 04:18(契约测试 04:17)。文件仍在被他人快速改动，1b-3 开工时必须重新定位。
- [confirmed] 本地 yaml RouteStore 为 redis 模式、FenceShards: 16；配置非法一律拒启
   亲阅 go/data_service/etc/data_service.yaml:182-189；internal/config/config.go:294-378。
- [confirmed] mapping Redis：K8s 单实例未设 maxmemory(缺省 noeviction)、开 AOF；compose 已改 noeviction；redis-ha 设 min-replicas-max-lag 10
   亲阅 deploy/k8s/manifests/infra/redis.yaml:131-133；deploy/docker-compose.yml:256；deploy/k8s/manifests/infra/redis-ha.yaml:259-260(该文件工作区有他人 +9/−6 未提交改动)。
- [confirmed] 容量目标下 player_route 10 亿行；mapping Redis 三键 100~300GB
   亲阅 docs/design/capacity-model-hok-scale.md:57、:77、:290、:894-910。
- [confirmed] merged_into Redis 值是 json.Marshal 的 {"dst":N,"run_id":"…"}，routetool 有一份解析
   亲阅 tools/merge_zone/merged_into.go:39-42、:106-115；routetool/redisread.go:329-350。
- [confirmed] audit 不把 maprev 缺席算差异，能报 redis_rev_ahead
   亲阅 auditor.go:188-240；docs/ops/merge-zone-runbook.md:812。
- [confirmed] 在线清库脚本先 FLUSHALL 再 TRUNCATE 路由表并复位 route_reconcile
   亲阅 tools/scripts/currency_crash_window.ps1:113-115、:189-191、:220；stress_round19.ps1:102-118、:150-162 同形。注意复位语句还把 leader / lease_until_ms 置 NULL——服务运行中执行会直接打掉在任 leader 的租约。
- [confirmed] db_first 才装配 route store；MissingStores 只在 db_first 要 player_route
   亲阅 go/data_service/internal/svc/servicecontext.go:150-169、:296-336。
- [confirmed] 指标包惯例：封闭集合 label、启动 Prime 预置 0
   亲阅 internal/metrics/metrics.go:269-300、:417-428。
- [confirmed] Java 版归属区就是库内列 game.player.zone_id，直接读库，没有 Redis 完整副本
   亲阅 D:\luyuan\xuanming-server-mmo\game-common\src\main\java\com\game\common\player\PlayerHomeZones.java:11-21；HomeZones.java:12「Java 没有 data_service」；grep Java 源码无 player:zone / mapping:route 键；PARITY.md:139 是「归属区映射」行，工作区为 M。
- [confirmed] (规格风险项)go-redis v9 的 MaxRetries=-1 是否等于关闭重试【未核实】
   亲阅模块缓存 github.com/redis/go-redis/v9@v9.18.0/options.go:114-118(注释「-1 (not 0) disables retries」)与 :384-388(-1 归一为 0)。go.mod 用的正是 v9.18.0。可以删掉这个未核实标注和 TestRealRedisMaxRetriesDisabled 取证用例。
- [confirmed] (规格 Q4)miniredis 是否支持 WAITAOF / INFO【未核实】
   亲阅 alicebob/miniredis/v2@v2.37.0：只注册了 WAIT(cmd_generic.go:59)，没有 WAITAOF，也没有 CONFIG；INFO 只支持 clients / stats 段(cmd_info.go:19-43)；EVALSHA 与 SCRIPT FLUSH 都支持(cmd_scripting.go:22-24、:256)。规格「身份解析只测纯函数、单测 WAITAOF 配 0/0」的做法成立。
## plan_problems
- [must] Arm=false 下的 catch-up 收尾没有定义「设 VI」，cvm 的减法也没说要饱和。步骤 7 写的是「标记不在且 Arm=false 且 VI==nil：排 catch-up」。任务一节只写了 prewarm / repair 收尾时「成功则 VI=身份」，catch-up 没有。按字面实现，catch-up 结束后 VI 仍是 nil：(a) 只要没有在跑的任务，下一拍又排 catch-up；(b) IdentityVerified 恒为 false，R27 里 cvm 永远不推进，起点永远是 0−60s，也就是每轮都扫全表，同时对共享 mapping Redis 做 3N 键的 MGET。P2~P3 期间在 S3 规模下就是不停地做全量扫描。另外 cvm 初值是 0，「cvm−RepairLookbackMs」如果按 uint64 直接相减会回绕成接近 2^64 的数，ScanRoutesUpdatedSince 扫不到任何行，首次当选的 catch-up 就成了静默空操作，与「初值 0 等于全量」正好相反。
   修法: 规格里写明：catch-up 任务开始前和结束后各读一次身份，两次相等且本轮没有 error 时置 VI=该身份；之后各拍 IdentityVerified=true，cvm 开始推进。所有「X−LookbackMs / −OverlapMs」一律饱和到 0。reconciler_test 加两个用例：`CatchUpCompletesSetsVerifiedIdentityAndDoesNotRequeue`(断言 catch-up 只跑一次，后续拍 AdvanceReconcile 收到 IdentityVerified=true)和 `LookbackSaturatesAtZero`(cvm=0、wm=0 时起点是 0，不是回绕值)。
- [must] 增量推送的游标初值没有定义，而且没有持久化。「上一轮起点（库时钟）−IncrementalOverlapMs」只存在 leader 内存里，有三处没定义：(1) leader 切换后(每次滚动发布都会切)新 leader 第一次增量从哪儿开始；(2) prewarm / repair / catch-up(S3 下要几小时)完成后，第一次增量从哪儿开始；(3) 增量被 PushMaxRowsPerSec 节流落后时，cvm 照样每拍推进。实现者如果取「当前时刻−15s」，旧 leader 最后一轮之后、或长任务期间、在已扫过的片里提交但发布失败的孤儿行，就再也没有机制推送。这直接违背本批「增量推送（含孤儿行）」的目标，`audit -require-maprev = 0` 这个 P3 门禁在每次发布之后都可能退回非零；第 2 批 merge_zone 依赖的「发布失败由增量补」也不成立。
   修法: 不加 DDL，改 cvm 的语义：R27 只在「身份已验证」且「已有一轮增量(或 prewarm / repair / catch-up)从起点 S 完整跑完、期间身份不变」时，把 cache_verified_ms 推进到 min(本拍 now, S)，也就是 cvm 表示「updated_at_ms < cvm−Overlap 的行都已推过，并且这段时间 Redis 身份没变」。新 leader 的第一次增量从 max(0, cvm−IncrementalOverlapMs) 开始；长任务结束后的第一次增量从该任务开始时的库时钟减 Overlap 开始。repair 的回看起点继续用 min(marker.wm, cvm)−Lookback，只会更保守。补用例：`FailoverIncrementalStartsFromPersistedCvm`、`IncrementalAfterPrewarmStartsAtPrewarmStart`、`ThrottledIncrementalDoesNotAdvanceCvmPastItsStart`。
- [should] Arm=false 期间(P2→P3)其实没有水位回档检测，compat_and_rollout 却写着「P2：leader … 做水位与回档检测」。D4 的两条判据(marker.wm > 库 wm、marker.generation ≠ 库 generation)都要求标记存在，而 Arm=false 时不挂标记，R27 又是 GREATEST(wm, now)，库回档后水位会被直接推高、洗掉。剩下的 cache_ahead 只覆盖增量窗口里被扫到的行，回档丢掉的行不在 updated_at_ms 窗口里。K8s 下 zone 回档会连带回档全局库(提案 :303)。P3 的二次导入和 audit 能事后兜住一部分，但规格对这项安全性质的陈述是错的，运维会据此在 P2 跳过手工冻结。
   修法: 二选一。A(推荐)：另设一把不表示「完整」的 Redis 键 `mapping:route:watermark`，string JSON {v,generation,watermark_ms}。db_first 下 leader 不论 Arm 取值，每拍在 R27 提交之后单调地 CAS 写它；AdvanceReconcile 拿它与 route_meta.generation、route_reconcile.watermark_ms 比较。读方不认它，所以 P3 之前写它也安全。rewarm 和回退 redis 模式时一并删掉。B：改掉 compat 的说法，明确「Arm=false 期间不检测库回档」，并在 runbook / k8s_zone_rollback 流程里规定：db_first 下凡是回档过全局库所在实例，都必须 freeze → reconcile-from-cache → audit → unfreeze。
- [should] Arm=false 保留已有标记，这个回退杠杆不是 fail-closed 的。回退表把「ArmReadyMarker 改回 false」列为代价最低的回退，同时又写「已有标记照常维护」。改回 false 往往就是因为怀疑副本不完整，而读方(第 3 批)只认标记是否存在，会继续把缺键当成权威缺席。回退到 redis 模式时，如果忘了跑 `rewarm -wait=0`，残留标记会一直留到下次切回 db_first：库落后于 Redis，leader 看到标记在、身份相同，直接把 VI 设成当前身份；之后 Redis 一旦部分丢失，60s 回看修补修不回只存在于 Redis 的登记。
   修法: Arm=false 时 leader 每拍对现存标记做 compare-and-delete，计 event=marker_dropped_disarmed，让「关 Arm」真正等于「撤销信任」。如果坚持现在的语义，就在 D1 和回退表里写明「要让读方停止信任，必须 rewarm -wait=0」。另外，redis→db_first 重新前进时，leader 首拍如果发现标记的 set_at_ms 早于本次 db_first 启用时刻，一律视作畸形并删掉。
- [should] 因持续性原因挂不上标记时，prewarm 会无限重排。步骤 7「标记不在且 Arm=true：排 prewarm」没有闩锁。以下三种情况下，prewarm 跑完挂不上，下一拍立即再排全量 prewarm(S3 下约 14 小时：10 亿行 ÷ 每秒 2 万行)：库里任何一行有 legacy_conflict / conflict / fenced / db_invalid(D3 要求零未决)；淘汰策略不是 noeviction；evicted_keys>0。后果是对全局库和共享 mapping Redis 的永久性满负荷扫描。而且 evicted_keys 是自实例启动以来的累计计数，一旦发生过淘汰，只有重启 Redis 或 CONFIG RESETSTAT 才会归零，规格没写恢复路径。
   修法: 挂标记被拒后进入 armingBlocked 状态，记下原因与本轮的未决 id 清单(有上限)。之后只做增量，并定期复读这些未决行；未决清空后再尝试挂标记，不重跑全表。淘汰类原因只在身份变化或 evicted_keys 回到挂标记时的基线之后才解除。也可以在标记里记 evicted_keys 基线，判据改成「比基线增加」。runbook 写明恢复办法。补用例 `ArmingRefusedDoesNotRestartFullPrewarm`。
- [should] leader 观测型 gauge 与告警的口径有问题。PrimeRouteReconcile 把 route_writable / route_ready_marker 等 gauge 在每个构造了 reconciler 的副本上预置为 0，只有 leader 会刷新。1b-3 的告警因此出三类错：(1) WritesFrozen `min(route_writable)==0`：非 leader 恒为 0，告警永远在响；(2) ReadyMissing `max(ready_marker)==0`：前任 leader 掉主后旧值 1 不清，会把真缺标记掩盖掉；(3) NoLeader `sum(leader)==0`：任何副本都没构造 reconciler 时，这条序列根本不存在，表达式不出数据，告警不会响。
   修法: 掉主时把 leader 观测型 gauge 置为 NaN，或用 DeletePartialMatch 删掉，只有当前 leader 导出它们。告警一律按 leader 过滤，例如 `max(route_writable * on(pod) group_left route_reconcile_leader)`。另加 `route_reconcile_assembled`(0/1)：db_first 的每个副本启动时都预置，NoLeader 告警改为 `sum(route_reconcile_leader) == 0 and sum(route_reconcile_assembled) > 0` 或 `absent(...)`。metrics_test 钉住「掉主后 gauge 被清」。
- [should] reconciler 构造失败时只打 ERROR。servicecontext 一项写「构造失败只打 ERROR、保持 nil、不拒启、不进 MissingStores」。db_first 下如果所有副本都因同一原因构造失败(例如 MappingRedis.Type 被配成非 node)，就既不补发孤儿行，也不做回档检测，指标上也看不出来(见上一条，序列不存在)。
   修法: 至少加上 `route_reconcile_assembled` 指标和告警。更稳妥的做法是：ReconcileEnabled() 为真且 Store.Required=true 时把「route_reconciler」纳入 MissingStores，与 player_route 同口径拒启。svc 的单测需要一个接缝，例如抽出纯函数 `newRouteReconciler(c, rs)`，否则「Enabled=false 时为 nil」在没有库的单测里是空过：route store 本来就装配不起来。
- [should] NewRouteLeaderID 的长度算错了。「route_leader@」13 字节 + host 截到 78 字节 + 「:」1 字节 + 8 位 hex = 100 字节，超过 route_reconcile.leader varchar(96)(schema.go:272)，也超过 validRouteWriter 的 96 上限(route_store.go:699-701)。主机名长时 leader 标识非法：严格模式下写库被拒，或者构造直接失败。
   修法: host 截到 96−13−1−8=74 字节。TestNewRouteLeaderIDUniqueAndValid 加一组 200 字节、含非 ASCII 的 host，断言结果不超过 96 字节且 validRouteWriter 为真。
- [should] 关停顺序会挤掉 Kafka 的 flush。规格把 reconciler 的 defer 注册在 Kafka defer 之后，按 LIFO 它先执行：最多等 5s，再用独立的 2s ctx 释放租约。go-zero v1.10.0 收到 SIGTERM 后 5.5s 强杀进程(亲阅模块缓存 core/proc/shutdown.go:16-21 的 defaultWaitTime=5500ms、:94-96 的 syscall.Kill)，data_service 的 yaml 和 ConfigMap 都没有调这个值。现有的 Kafka 宽限期 8s 本来就超了，再前插 7s，txlog 的 flushOnShutdown 基本做不完，只能靠 offset 重放兜底。
   修法: reconciler 停止只 cancel，等待不超过 500ms；释放租约设为尽力而为，可以和 Kafka 停止并行，或者干脆不等：租约 10s 后自然过期。把它注册在 Kafka defer 之前，让它在 Kafka flush 之后执行，并在规格里写明总预算不超过 go-zero 的 WaitTime。
- [should] TiDB 缓存表的约束被漏掉了。提案 :824 要求：route_meta 设成缓存表(S2 起必选)之后，`UPDATE route_meta SET writable=0` 最长要阻塞一个读租约(tidb_table_cache_lease 默认 3s【未核实】)，1b 冻结步骤的超时必须不小于租约加余量。规格的 AdvanceReconcile / FreezeByLeader 在持有 route_reconcile FOR UPDATE 的情况下执行 R28，但没写这一步的 ctx 预算。如果按拍间隔 1s 设 ctx，在 TiDB S2+ 上冻结会一直超时、永远冻不成，而冻结正是回档时的 fail-closed 动作。CLI freeze 的超时同样没写。提案 :836 留给 1b 的「tidb-prepare 是否顺带 ALTER TABLE route_meta CACHE」也没有回应。
   修法: 含 R28 / R19 / R22 的事务单独设超时：不小于 max(5s, 缓存表租约+2s)，作为配置项 FreezeTimeoutMs。拍间循环要允许这一拍拖长，同时不能让本地租约过期：冻结前先检查本地剩余租约是否足够，不足就先续约。在 D 节写明是否把 `ALTER TABLE route_meta CACHE` 并进 tidb-prepare，或者明确留给 1b-3 / TiDB Phase 1。
- [should] 反向修库对冻结态的复核是普通读。requireRouteFrozen 沿用 readRouteWritable 的普通一致读(route_store.go:631-641)。修库进行中有人执行 unfreeze 时，修库事务照样提交，同时 W1 已经恢复写入，违背 AGENTS §11.3「提交点用同一并发控制机制原子复核」。行级 CAS 能挡住同行冲突，但「冻结期内修库」这条前提在提交点上没有被复核。
   修法: RepairRoutesFromCache / RepairMergedIntoFromCache 事务内用 `SELECT writable FROM route_meta WHERE id=1 FOR UPDATE`(或 FOR SHARE)复核。锁序是 route_meta → player_route；unfreeze 是 route_reconcile → route_meta；W1 只做普通读。不成环。补契约用例：修库事务持有 meta 锁时，unfreeze 的 R22 必须等待。
- [should] 恢复流程里的「audit 两个方向为 0」在冻结期可能达不到。冻结期间 leader 停止推送，reconcile-from-cache 只有缓存→库一个方向。库里只有库有的行(回档前发布失败的孤儿行、回档后到发现前提交而发布失败的行)会一直被 audit 记为 db_only_unpublished，退出码 1，unfreeze -confirm-audited 的前提永远满足不了。运维只能跳过或忽略 audit，门禁形同虚设。
   修法: 明确恢复口径：冻结期 audit 允许 db_only_unpublished(这类行解冻后由增量推送补齐)，其余类别必须为 0。可以给 audit 加 `-frozen-recovery` 开关，输出这一口径下的退出码。或者在 reconcile-from-cache 末尾增加「库独有行 → 缓存」的一遍推送，复用 routereconcile 的 Push：冻结期内库是被修好的一边，缓存为超集的前提不受影响。runbook 同步。
- [should] unfreeze 时如果标记不在，规格没有任何防护。冻结期间如果 Redis 发生身份变化(从旧备份恢复、换实例)，leader 会撤掉标记。这时缓存可能已不是超集，库和缓存可能同时缺同一批登记(提案 §6「同时丢失」那一行)，而 audit 看不见两边都缺的行。unfreeze 读不到标记就按 minWatermark=0 直接置 writable=1，随后 Arm=true 的 leader 从库全量预热、重新挂上标记，读方就把两边都丢了的登记当成权威缺席。
   修法: unfreeze 读不到有效标记时，默认退出码 2。要放行必须显式加 `-cache-was-not-verified-acknowledged`，并在 runbook 写明：这种情况要先按 §9.2 清单重放。标记存在但身份与当前 Redis 不等时同样拒绝。补用例 `TestRunUnfreezeRefusesWithoutVerifiedMarker`。
- [should] leader 连不上 Redis 时会一直占着租约。步骤 2：Identity 出错时只记 redis_error、不撤标记，却继续续约。leader 与 mapping Redis 之间断网、而库可达时，它会一直持有租约，其它能连上 Redis 的副本永远接不了手，身份变化检测就停了；这段时间里 Redis 重启后从 AOF 带回的旧标记会被读方当真。
   修法: 连续 Redis 失败超过 LeaseMs 时主动 ReleaseReconcileLease 并降级(计 event=lease_released_redis_unreachable)，让别的副本接手。补用例 `SustainedRedisErrorsReleaseLease`。
- [should] runbook 让运维 `kubectl exec` 进在役 Pod 跑 route-tool。data-service Pod 的内存上限是 512Mi(data-service.yaml:110-112)，import / audit 的已见集合是 O(N)，注释按 100 万行估约几十 MB，千万行就是几百 MB。在役容器里跑可能触发 cgroup OOM，把正在服务的 data_service 一起杀掉。提案 Q1 原定的是「与 -migrate Job 同形」，规格却把 Job 清单降成 1b-3 的可选项。
   修法: runbook 默认用一次性 Job 或独立 Pod，同一镜像、同一 ConfigMap，args 为 `-f … -route-tool <sub>`，资源单独设置。exec 方式只限 dry-run 或小库，并写明内存风险。Job 清单从可选改为 1b-3 的必做项，作为 P1 在 K8s 上执行的前置。
- [should] 迁移测试时把 `TestRouteTool*` 改名为 `TestRun*`，会让前几批的验证命令静默丢失覆盖。提案 :709、:870、:940、:1002 的验证命令都对 `./cmd/route_tool/...` 跑 `-run "Route|…"`。规格又要求 Codex 先跑完前几批再跑本批。main_test.go 删掉、用例改名后，前几批的命令在 cmd/route_tool 下变成 no test files，routetoolcli 下的新名字也匹配不上 Route，五个用例在旧口径下静默消失。
   修法: 迁入 routetoolcli 时保留原名(TestRouteTool*)；或者在本批验证里写明旧命令应改指 `./internal/routetoolcli/...` 并逐个列出用例名的对应关系。
- [should] 「全局只有一个 leader」所依赖的不变量没有写出来。多个 zone 的 data-service 共用全局库(data-service.yaml:30)，route_reconcile(id=1) 选出的是全服唯一的 leader，但它只维护自己配置里的 MappingRedis。G4a 拆 Redis(shared-redis-split-by-purpose.md:205、:278-281)要求所有 zone 同一拍切换映射实例，并预期「换实例等于身份变化，1b leader 撤标记、修补、再挂回」。问题是：身份变化修补只回推 60s，前提是新实例保有旧实例的全部数据，这只对主从切换成立。用 SCAN+DUMP/RESTORE 拷贝迁移时如果连标记一起拷、又漏了键，修补后会重新挂标记，信任一份不完整的副本。
   修法: 在 D 节不变量里加一条 I4：共享同一全局库的所有 data_service 必须指向同一个 MappingRedis。可以把 MappingRedis 的地址指纹写进标记，或者在 leader 日志里记下来。runbook / G4a 交接里写明：迁移映射实例时不得拷贝 mapping:route:ready，或者切换后立即 `rewarm`(全量预热)，不能依赖身份变化修补。
- [should] 清库脚本与 db_first leader 的冲突只写进了注释。L-12 建角压测就是 P2 门禁，要在 db_first 下跑，而 AGENTS §6.2 要求压测前清库，用的正是 currency_crash_window / stress_round19。这两个脚本先 FLUSHALL(Redis 身份不变，leader 看到的只是标记没了，随即全量 prewarm，把即将被 TRUNCATE 的行推回 Redis)，再把 route_reconcile 的 leader / 租约 / 水位置成 NULL / 0，期间还可能让 leader 误判 cache_ahead 而冻结建角。只在 yaml 注释里提醒，压测时很容易踩到。
   修法: 两份脚本在清库前探测 `route_reconcile.lease_until_ms > NOW()` 的毫秒值：有在任 leader 就拒绝执行，提示先停 data_service，或者显式加 `-AllowWithRunningLeader`。这属于脚本行为变更，可以并进 1b-3，但规格的 compat 和 L-12 门禁必须写明这是前置条件。
- [nit] 有几处未核实标注可以撤掉。go-redis v9.18.0 的 MaxRetries=-1 就是关闭重试(options.go:114-118、:384-388)；miniredis v2.37.0 没有 WAITAOF / CONFIG，INFO 只支持 clients / stats 两段(cmd_generic.go:59、cmd_info.go:19-43)。
   修法: 删掉 TestRealRedisMaxRetriesDisabled 这个取证用例和风险表里对应的条目；Q4 标为已核实，写明 miniredis 不支持。
- [nit] 有几处遗漏或细节不清。(a) push 指标的 phase 集合 {prewarm, repair, incremental} 里没有 catch-up 任务；(b) NewRedisCache 只传了 Password，漏了 go-zero RedisConf.User，配了 ACL 用户时会认证失败；(c) 标记解码遇到 v>1 就当畸形 CAD，以后滚动升级到 v2 时，旧 leader 会删新格式的标记，两边来回打架；(d) D5 写的是「库里没有生效围栏」，实现查的却是 Redis 的 merge:in_progress:*；(e) 无状态去重只管跨遍重复，同一页 SCAN 回来的重复键会撞上 validateRouteImportRows 的重复检查，整批失败；(f) Superseded 之后用 GetRoutes 复读，读不到行时怎么处理没写，应按 cache_ahead 处理。
   修法: phase 加上 catch_up(或者明确映射到 repair)；Username 取 mapping.User；v 大于已知版本时只记日志、不删、不信任；措辞改成 Redis 围栏；每页先去重再提交；复读不到行按 cache_ahead 冻结，并补一个用例。
- [nit] 运维细节有几处缺口。(a) 规格的 Validate 要求 ArmReadyMarker=true 时 Mode 必须是 db_first，但回退表没写「先把 Arm 改成 false」，紧急回退到 redis 模式时会因配置非法而拒启；(b) rewarm 等于撤掉标记、等 leader 全量重挂，S3 下第 3 批读方会因此 fail-closed 数小时，runbook 应当醒目提示；(c) open_decisions 第 1 条前面说「必须在 1b-2 落码前定」，推荐却是「1b 先用单标记」，两处说法自相矛盾，实际的截止点是第 3 批之前；(d) spec_D1_batch1b.md 和提案 §11.2 的「第 1b 批」各写一份 D1–D8 和偏离表，形成两个事实源。
   修法: 回退表补上「先改 Arm=false」；runbook 加告警提示；把截止点统一写成第 3 批读方落码前；以提案 §11.2 为准，spec 文件只作交接快照，并在文首注明「以提案为准」。
## missed
- docs/design/zone_data_rollback.md:171 仍写「映射丢失后的恢复见 runbook §9.2」。1b 之后 db_first 下全局库回档的恢复改成 freeze → reconcile-from-cache → audit → unfreeze，这份设计文档要同步，规格的 doc_updates 里没有它(k8s_zone_rollback.ps1 已列入 1b-3)。
- 提案 :836 把「tidb-prepare 是否顺带 `ALTER TABLE route_meta CACHE`」留给第 1b 批；:824 要求冻结步骤的超时不小于缓存表租约加余量。规格对两处都没有回应。
- 前几批的验证命令(提案 :709、:870、:940、:1002)依赖 `./cmd/route_tool/...` 下的 TestRouteTool* 用例名；1b-1 删掉 main_test.go 并改名后，这些命令要同步改。
- 清库脚本 tools/scripts/currency_crash_window.ps1、stress_round19.ps1 会在服务运行中把 route_reconcile 的 leader / lease 置 NULL，并先 FLUSHALL。db_first 下这会与 leader 互相干扰，而 L-12 压测正是 P2 门禁。规格只在 yaml 和 runbook 里提醒，没有给脚本加防护。
- 多个 zone 的 data-service 共用全局库，route_reconcile 选出的是全服唯一 leader：缺少「所有副本必须指向同一个 MappingRedis」这条不变量；G4a 换映射实例的步骤(shared-redis-split-by-purpose.md:205、:278-281)需要加「排除标记或切换后 rewarm」。
- 恢复与运维路径缺几项：冻结期 audit 口径(db_only_unpublished 不可能清零)；unfreeze 时标记缺失的处理；leader 连不上 Redis 时让出租约；evicted_keys 累计计数的恢复步骤(CONFIG RESETSTAT 或重启)。
- 监控：leader 观测型 gauge 在非 leader 上恒为 0、前任 leader 留下陈旧值；NoLeader 告警在 reconciler 一个都没装配起来时序列不存在；没有 `route_reconcile_assembled` 指标。
- 关停预算：go-zero v1.10.0 收到 SIGTERM 后 5.5s 强杀(core/proc/shutdown.go:16-21、:94-96)，规格把 reconciler 的等待排在 Kafka flush 之前。
- K8s 跑 route-tool 的方式：data-service Pod 内存上限 512Mi(data-service.yaml:110-112)，exec 进在役 Pod 跑 O(N) 内存的 import / audit 有 OOM 风险，Job 清单应当是必做项，不是可选项。
- 工作区：tools/scripts/k8s_deploy.ps1 现为 +825 行(04:18)、k8s_deploy_contract.tests.ps1 +978 行(04:17)、docs/PROGRESS.md +329 行(04:19)，规格里的行号(:5812-5824)和 diff 统计都已过时。go/data_service 模块里另有他人未提交的改动：internal/store/snapshot_store.go、id_segment_store.go、snapshot_store_test.go，internal/server/dataserviceserver.go，generated/pb/game/message_id.go，以及新增的 internal/logic/asset_sql_commit.go 和 asset_sql_commit_test.go。模块级 `go build ./...`、`go vet ./...` 的结果会混进这些改动，归因时要逐一点名。本批规划改动的文件里，只有 config.go 与 data_service.yaml 各有他人一行与本项无关的改动(IdSegment BootstrapTags)，冲突风险低。
- 测试缺口：catch-up 收尾设 VI、cvm 推进；回看起点饱和到 0；leader 切换后增量起点；长任务结束后的增量起点；挂标记被拒后不重跑全量预热；掉主时清 gauge；长主机名下 leader 标识不超过 96 字节；unfreeze 在标记缺失时拒绝；Redis 持续不可达时让出租约；关停有时间上界；svc 层 Enabled=false 的判定要有不依赖库的接缝。
