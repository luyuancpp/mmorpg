package data

import (
	"cmp"
	"context"
	"database/sql"
	"errors"
	"fmt"
	"math/rand/v2"
	"slices"
	"time"

	assetpb "proto/common/asset"
	tradepb "proto/trade"

	"shared/assetop"
	"shared/safego"

	"github.com/go-sql-driver/mysql"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// 编译前置(写下本文件时未满足):本文件用到 TradeAssetOpRecord 的 resolved_by(22)/
// resolve_reason(23)。这两个字段只在 proto/trade/trade_table.proto 里,生成物还停在
// 字段 21 —— 直接 go build 会报 `rec.ResolvedBy undefined`,那不是代码写错,是**没跑
// 生成**。先跑仓库既有的 proto 生成(dev.bat proto),再 build/vet/test。
// 生成会改到:go/proto/trade/trade_table.pb.go、cpp/generated/proto/trade/trade_table.pb.{h,cc}、
// generated/proto/{_unified,db,login}/proto/trade/trade_table.proto;robot/vendor/proto/trade/
// trade_table.pb.go 是 `replace proto => ../go/proto` 的 vendor 副本,按仓库惯例一并同步。

// 表名。唯一事实源仍是 proto/trade/trade_table.proto 的 OptionTableName;这里是手写 SQL
// 需要的字面量,两边必须一致。改表名要同改这两处(schemamigrate 会按 proto 的名字建表,
// 改漏一边的表现是"表建出来了但所有查询报 table doesn't exist",启动期查不出来)。
const (
	AssetOpSeqTableName = "trade_player_op_seq"
	AssetOpTableName    = "trade_asset_op"
)

// assetOpColumns 是 outbox 行的全列,顺序即 scanAssetOp / InsertOp 的参数顺序。
const assetOpColumns = "`op_id`, `player_id`, `stream`, `stream_epoch`, `seq`, `kind`, `status`, `durable`, " +
	"`attempts`, `next_attempt_ms`, `deadline_ms`, `lease_until_ms`, `lease_token`, `tx_type`, " +
	"`ref_kind`, `ref_id`, `payload`, `last_outcome`, `last_reason`, `created_ms`, `updated_ms`, " +
	"`resolved_by`, `resolve_reason`, `last_send_ms`"

// PendingStatus 是 outbox 的"待办"状态数值,给 assetop 的 seq 分配与重投循环用。
// 取 proto 枚举而不是写 0 / 1:§4.43 #23 明确 assetop.Status 不绑库值,库值由业务表自己定。
func PendingStatus() uint32 { return uint32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING) }

// AssetOpRepo 是通用资产通道 outbox(trade_asset_op)与 seq 分配器(trade_player_op_seq)的
// MySQL 实现,同时满足 assetop.Store。
//
// 契约:
//   - 除 InsertOp / AllocateSeqsTx 之外的方法自带 opTimeout 上限,调用方 ctx 更早到期时以 ctx 为准
//     (EnsureSeqRows 自己开短事务,所以也带 opTimeout);
//   - InsertOp / AllocateSeqsTx 只在调用方给的事务里执行,不自带超时(事务的预算由调用方统一给);
//   - 返回的 error 一律是存储故障,唯一的例外是 Claim 的 assetop.ErrPoisonRow(payload 解不开)。
type AssetOpRepo struct {
	db        *sql.DB
	opTimeout time.Duration
	tables    assetop.SeqTables
}

var (
	_ assetop.Store            = (*AssetOpRepo)(nil)
	_ assetop.ManualResolver   = (*AssetOpRepo)(nil)
	_ assetop.PendingAgeReader = (*AssetOpRepo)(nil)
	// 落地恢复(docs/design/landing-recovery-sync-digest.md §5.3 / §6.2 / §6.3)的三个可选能力,
	// 与 guild 的 GuildAssetStore 同口径实现:落地唤醒、日对账、自动放弃。
	_ assetop.PlayerPendingLister = (*AssetOpRepo)(nil)
	_ assetop.PendingClassCounter = (*AssetOpRepo)(nil)
	_ assetop.Abandoner           = (*AssetOpRepo)(nil)
)

// NewAssetOpRepo。opTimeout 是每次调用的上限(constants.StoreOpTimeout),必须 > 0。
// 表名经 assetop.NewSeqTables 校验(正则 ^[a-z][a-z0-9_]{0,62}$),防止拼接注入。
func NewAssetOpRepo(db *sql.DB, opTimeout time.Duration) (*AssetOpRepo, error) {
	tables, err := assetop.NewSeqTables(AssetOpSeqTableName, AssetOpTableName, PendingStatus())
	if err != nil {
		return nil, fmt.Errorf("trade: 资产 outbox 表名非法: %w", err)
	}
	return &AssetOpRepo{db: db, opTimeout: opTimeout, tables: tables}, nil
}

// DB 暴露连接池,供调用方用 assetop.WithTxRetry 开业务事务(seq 分配必须与业务写同事务)。
func (r *AssetOpRepo) DB() *sql.DB { return r.db }

// Tables 返回 seq / outbox 的表名对,供 assetop.AllocateSeq / EnsureSeqRowTx 使用。
func (r *AssetOpRepo) Tables() assetop.SeqTables { return r.tables }

func (r *AssetOpRepo) bounded(ctx context.Context) (context.Context, context.CancelFunc) {
	return context.WithTimeout(ctx, r.opTimeout)
}

// ---------------------------------------------------------------------------
// seq 行的建行守卫
// (2026-09-21 死锁审计 #16 在 trade 的同形;2026-09-28 落哨兵守卫行;
//
//	2026-09-29 把"锁哨兵 + 建行"从业务事务里拆出来,放进它自己的 RC 短事务)
//
// ---------------------------------------------------------------------------
//
// # trade 的全局取锁顺序(唯一事实源;新增任何写路径先回来对一遍)
//
// 两段,**分属两个事务**,前一段提交之后后一段才开始:
//
//	[短事务 A:建 seq 行]  trade_player_op_seq 的哨兵守卫行 (0, 0)
//	                        → trade_player_op_seq 的缺失玩家行
//	                          (按 (player_id, stream) **升序** INSERT IGNORE)
//	                        ── COMMIT ──
//	[业务事务 B]            trade_listing → trade_order(P3 未落)
//	                        → trade_player_op_seq 的玩家行
//	                          (按 (player_id, stream) **升序** FOR UPDATE)
//	                        → trade_asset_op
//
// **消环的关键性质**:哨兵行只出现在事务 A 里,而 A 去拿它的时候手上一把锁都没有 ——
// A 的前置探针是 RC 普通读(不取行锁),哨兵行是 A 取的第一把锁,取到之后 A 只再碰它要插的那几行,
// 插完立刻提交。事务 B 一次都不碰哨兵行。于是全仓画不出"持业务锁 → 等哨兵行"这条反向边,
// "哨兵行 → 玩家 seq 行"与全序同向。
//
// 逐条核对(2026-09-29 重做了一遍,新增写路径必须回来补):
//   - InsertListing / InsertFavorite / DeleteFavorite(listing_repo.go):单表单语句,整份文件
//     一条 FOR UPDATE 都没有,也不在同一事务里碰 seq / outbox —— 不参与本条全序。
//   - EnqueueEscrowDebit(reconcile/pipeline.go):**唯一**同时碰 seq 与 outbox 的路径。它先调
//     EnsureSeqRows(事务 A,自带提交),再开事务 B 调 AllocateSeqsTx(确认读 → 升序 FOR UPDATE)
//     与 InsertOp。事务 B 里没有任何语句提到哨兵键。
//   - EnsureSeqGuardRow(启动期 bootstrap):自动提交的单条 INSERT IGNORE,只碰哨兵行本身,
//     跑在任何业务事务之外。
//   - Finalize / ResolveManually / Abandon:事务里只有一条 trade_asset_op 的主键 CAS,不碰 seq 表。
//   - Claim / Reschedule / markPoison / ReleaseLease:自动提交的单行主键 UPDATE(RC,见 svc.BuildDSN)。
//   - CleanupTerminalOps:逐行 RC 短事务{trade_asset_op 主键点锁 → 带复核条件的主键点删},只删终态行,
//     不碰 seq 表(seq 表的"永不删除"前提不受影响,见 TestSeqTableRowsAreNeverDeleted)。
//   - ListDue / OldestPendingCreatedMs / getOp / ListPendingByPlayer / CountPendingByClass /
//     ListAbandonCandidates / 清理的候选读:不加锁读。
//
// P3 落订单与卖家账时:商品 / 订单行的锁排在事务 B 的最前面(全序第一、二段),
// **而 EnsureSeqRows 要排在事务 B 之外、之前**(它自己就是事务 A)。
//
// # 为什么要守卫行(旧写法哪里错)
//
// 最早的写法在业务事务之外用自动提交 INSERT IGNORE 建 seq 行,没有任何串行化载体。seq 行建出后
// 永不删除,所以并发补行的后到者拿 S 看到的是**已提交**的重复键、判重即结束,不会升 X。
// 只剩一种 1213:首个插入者在提交前回滚(连接被 KILL、刷盘失败、实例关闭、上层 ctx 到期),
// 排在它那条未提交记录后面做重复键检查的多个 INSERT 会同时继承到同一段间隙上的锁,随后各自申请
// 插入意向锁、被对方的间隙锁挡住,InnoDB 牺牲其一。
//
// 有界重试能把它吸收掉(更早的代码就是这么兜的),但**能从 SQL 层消掉的环不该用重试兜底**:让所有
// 首次建行者先排在一行**已存在、已提交**的守卫行上,同一时刻最多一个插入者,排队的是守卫行上的
// 记录锁;守卫持有者回滚只是放锁,不会把锁继承成间隙锁。环被消掉,正确性不再取决于重试次数够不够。
//
// # 为什么把"锁哨兵 + 建行"拆进独立的短事务(2026-09-29)
//
// 消环的性质来自"所有首次建行者排在同一行**已提交、永不删除**的记录锁上",与这把锁是否与业务写
// 同处一个事务**无关**。同事务的代价是哨兵行的 X 要一直持到业务事务提交 —— 中间还夹着 AllocateSeq
// (玩家 seq 行 FOR UPDATE + 对 trade_asset_op 的未决行普通读)和插 outbox 行。开服 / 新区首日几乎
// 每一次上架都是首次建行,那段窗口里**全服托管入队串行**在这一行上;而且 op 表未决行查询的执行计划
// 一旦退化,局部变慢会立刻变成全局停摆。
//
// 拆开之后,哨兵行的持锁时间被压到"一次主键点查 + 每个缺失键一条 INSERT IGNORE",与业务 RPC、
// outbox 写入、事务重试退避全部无关。稳态更省:EnsureSeqRows 的前置探针命中时连事务都不开。
//
// # 拆开之后为什么仍然正确:短事务提交之后、业务事务开始之前,seq 行不可能消失
//
//  1. 事务 A 是**提交**过的,不是"写了就算"。提交之后那几行是已提交数据,业务事务 B 回滚只回滚
//     它自己,碰不到 A 写下的行。
//  2. 全仓对 trade_player_op_seq **没有任何 DELETE / TRUNCATE / DROP**,所以行不会被业务代码删掉。
//     这条前提不靠人记:asset_op_seq_guard_test.go 的 TestSeqTableRowsAreNeverDeleted 扫 **trade
//     模块**全部非测试源码,出现 DELETE 就判红(判据范围与本段措辞必须一致,改一处要改另一处)。
//  3. 于是事务 B 里的确认读(同一条 sqlSeqRowExists)在 RC 下必然命中:RC 每条语句取一份新快照,
//     而 A 在 B 的 BeginTx **之前**就已经提交。
//  4. 万一真读不到(只可能是有人在库上手工删了行,或将来有人加了清理路径),**fail-closed**:
//     回 ErrSeqRowMissingInTx、计一次 trade_assetop_seq_row_missing_in_tx_total,本次上架失败。
//     绝不在事务 B 里就地补行 —— 那正是"持业务锁再去拿哨兵锁"那条反向边。
//
// ⚠ 第 3 条的前提是"单个 MySQL 主库、同一个连接池"(本服务就是这么连的,见 svc.OpenMySQL)。
// 将来若把读切到从库 / follower read,确认读可能落在落后的快照上,这一条要重新论证。
// **本组未连库,以上全部是静态论证,未在真库验证。**
//
// # 守卫行选谁:哨兵行 (player_id=0, stream=0)
//
// trade 没有天然的 per-player 行可锁(帮会用的是 guild_member(G, p),玩家同一时刻只在一个帮):
//   - trade_listing 主键是 listing_id,同一卖家可以并发上架两件商品,锁两件不同的商品行串行不了同一玩家;
//   - trade_favorite 主键是 (player_id, listing_id),不保证存在,且与上架毫无业务关系;
//   - trade_order 要到 P3 才有,而且同样是 per-order。
//
// 所以守卫行取 seq 表自己的哨兵行 (player_id=0, stream=0):
//   - player_id=0 不可能是真玩家:PlayerId 由 login 的 bwmarrin/snowflake 发,不发 0(AGENTS §7 不变量 1);
//   - stream=0 是 ASSET_OP_STREAM_UNSPECIFIED,不是 trade 独占的两条流之一,assetop 永不给它分配 seq;
//   - 全仓对 trade_player_op_seq 只有**五条**语句,全是完整主键等值,各自的锁语义如下
//     (2026-09-29 重数;上一版写"三条"并漏掉了后两条,清单自己不全就等于没人能"对一遍"):
//     1. assetop.AllocateSeq 的 `SELECT next_seq, epoch ... FOR UPDATE` 点查 —— 玩家行上的 X 记录锁;
//     2. assetop.AllocateSeq 的 `UPDATE ... SET next_seq ... WHERE player_id = ? AND stream = ?`
//     点更新 —— 同一行的 X;
//     3. 本文件 sqlSeqRowExists 普通读(探针 / 确认读两处共用)—— RC 下不取行锁;
//     4. 本文件 sqlLockSeqGuardRow(`FOR UPDATE`)—— X,锁集只落哨兵行;
//     5. assetop.ensureSeqRowFormat 的 `INSERT IGNORE` —— 插入意向锁,撞重复键时在那条索引记录上取 S。
//     没有任何 COUNT / MIN / 范围扫描 / 巡检会把哨兵行算进去。
//     **新增任何按 stream 分组或全表聚合的查询时必须显式排除 (0, 0)**,并补一条断言
//     (机械守卫:TestNoAggregateQueryOverSeqTable)。
//
// # 多个 key 时为什么必须按 (player_id, stream) 升序
//
// 哨兵行串行化的只是"**建行**",不是"**分配**"。玩家 seq 行一旦建出,后续所有分配都只是普通读命中 +
// 玩家行 FOR UPDATE,全程不碰哨兵行。P3 的交付要在同一个事务里给买家 CREDIT 和卖家 CREDIT,两笔
// 交易若以相反顺序去锁两条**都已存在**的玩家行(买家A/卖家B 与 买家B/卖家A),就直接在
// trade_player_op_seq 上成环,守卫行帮不上任何忙。
//
// 所以本文件把顺序钉死在**一个确定性全序**上:(player_id, stream) 升序,建行(事务 A)与分配
// (事务 B)都按它走,并且只由 EnsureSeqRows / AllocateSeqsTx 两个方法内部排序 —— 调用方拿不到
// 乱序的机会(机械守卫:TestSeqKeysAreProcessedInAscendingOrder)。
//
// # TiDB 口径(迁库后这一段要重新算账)
//
// AGENTS §1 的目标库是 TiDB,而 **TiDB 没有间隙锁**,本节要消的那个环在那边本来就不存在。
// 迁过去之后哨兵行不再是"消环的载体",只剩"一个所有首次建行都要写/锁的全局热行":悲观事务下
// 是同一个 Region 的一行上排队,乐观事务下是反复的写冲突(9007)重试。短事务化已经把这个热点的
// 持有窗口压到最短,但没有消掉它。迁库时的处置意向(二选一,届时按实测定):
//   - 保留守卫,作为 MySQL / TiDB 两边一致的锁序,接受热行代价;
//   - 或把建行彻底前置(建角 / 首次登录时就把两条流的 seq 行建好),让上架路径永远走不到缺行分支,
//     哨兵行随之退化成一条谁也不碰的历史数据。
//
// 以上 TiDB 行为取自 TiDB 文档的公开描述,**本组未在 TiDB 上实测**。
//
// # 滚动升级窗口
//
// 旧版本副本仍在事务外、不经守卫地自动提交建行。新旧混跑期间原环仍可能出现(与 guild 同一条结论),
// 而且有一层放大:新副本的事务 A 持着哨兵行 X 去插同一条 seq 行,旧副本那条未提交记录一回滚,
// 事务 A 就可能成为旧环的牺牲者或长等待者 —— 它倒下之前一直握着全局哨兵行,影响面是
// **首次建行全局停摆一个锁等待周期**,不是"偶发一个请求 1213"。
// 短事务化把这个窗口压短了(A 里不再夹着 RPC 与 outbox 写),但没有消除它。
// 所以本项要停服切换或按 zone 逐个切换,不能新旧副本长期并存;这条约束必须落到发布单上,
// 不能只活在这段注释里。当前 AssetOp.Enabled 默认 false 且 EnqueueEscrowDebit 还没有生产调用方,
// 所以它是"开启前的前置条件",不是当下的线上风险。
const (
	// seqGuardPlayerID / seqGuardStream 是哨兵守卫行的完整主键。理由见上文。
	seqGuardPlayerID uint64                = 0
	seqGuardStream   assetpb.AssetOpStream = assetpb.AssetOpStream_ASSET_OP_STREAM_UNSPECIFIED

	// sqlSeqRowExists 是**普通读**(完整主键等值,不带任何锁定子句;RC 下不取行锁),两处共用:
	//   - EnsureSeqRows 的前置探针:seq 行建出后永不删除,常态直接命中,连短事务都不必开;
	//   - AllocateSeqsTx 的确认读:业务事务里只用它确认行在,不在业务事务里建行。
	// 给它加任何锁定子句都等于取消整个守卫,理由见 TestSeqRowExistsProbeTakesNoLocks 的头注。
	sqlSeqRowExists = "SELECT 1 FROM " + AssetOpSeqTableName + " WHERE `player_id` = ? AND `stream` = ?"

	// sqlLockSeqGuardRow 是守卫行的**锁定点查**:完整主键等值 + FOR UPDATE(EXPLAIN 应为
	// key=PRIMARY、key_len=12 = bigint unsigned 8 + int unsigned 4,两列都 NOT NULL 所以没有空值字节)。
	// RC 下未命中不取间隙锁,所以"哨兵行不存在"只能 fail-closed(ErrSeqGuardRowMissing),
	// 绝不退回"事务外自动提交建行" —— 那会把审计 #16 的环原样装回去。
	sqlLockSeqGuardRow = sqlSeqRowExists + " FOR UPDATE"
)

// ErrSeqGuardRowMissing:哨兵守卫行不在库里。启动期 bootstrap(svc.ServiceContext.StartAssetChannel
// 调 EnsureSeqGuardRow)失败或被跳过时会看到它。fail-closed:首次上架失败,而不是绕过守卫建行。
var ErrSeqGuardRowMissing = errors.New("trade: trade_player_op_seq 的哨兵守卫行 (player_id=0, stream=0) 不存在")

// ErrSeqRowMissingInTx:业务事务里确认 seq 行时读不到 —— 而 EnsureSeqRows 的短事务刚刚提交过它。
// 按守卫一节第 3 条的论证这不可能发生,能走到这里只剩两种解释:有人在库上手工删了行,
// 或者将来有人给这张表加了清理路径(那种情况下整套锁分析都要重做,见 TestSeqTableRowsAreNeverDeleted)。
// 所以这里**不就地补行**(那会把"持业务锁再拿哨兵锁"的反向边画回来),直接 fail-closed。
var ErrSeqRowMissingInTx = errors.New("trade: 业务事务里确认 trade_player_op_seq 行时行不存在")

// SeqKey 是一条 seq 行的键。取名不叫 SeqRow 是因为它只是键,不含 next_seq / epoch。
//
// 它同时是本文件那条确定性全序的比较键:所有多 key 路径都按 (PlayerID, Stream) **升序**处理,
// 理由见守卫一节"多个 key 时为什么必须按 (player_id, stream) 升序"。
type SeqKey struct {
	PlayerID uint64
	Stream   assetpb.AssetOpStream
}

// ---------------------------------------------------------------------------
// 守卫路径的低基数指标
// ---------------------------------------------------------------------------
//
// 这三条分支都是 fail-closed 的**静默**分支:错误沿调用栈返回给玩家,除了日志没有别的信号。
// 全集群 bootstrap 失败时,"每个玩家首次上架都失败"的唯一线索会是几小时前启动日志里的一行 ERROR
// (AGENTS §11.3:关键路径要有低基数指标)。
//
// 注册在 prometheus.DefaultRegisterer 上,与 svc.assetChannelMetrics 里的 assetop / scenenode 指标、
// 以及 svc 自己的 trade_* 计数器同一个 registry,由 svc.StartMetrics 的 promhttp.Handler() 一起暴露
// —— **不另起一套 registry**。本该挂进 assetop.Metrics,但那些字段与 inc* 方法都未导出,
// 而 shared/assetop 不在本组可改文件范围内,所以退而挂在同一个默认 registry 上,形状与口径保持一致。
//
// 一律**无 label**:这三件事都与具体玩家无关,player_id 只进日志(AGENTS §9)。
// 用普通 Counter 而不是 CounterVec:Counter 一注册就有一条值为 0 的序列,
// "从未发生"与"指标不存在"在告警规则里不会长得一样。
//
// 包级变量只初始化一次,所以 promauto 的 MustRegister 不会因重复注册 panic(单测里同样安全)。
var (
	seqGuardBootstrapFailuresTotal = promauto.With(prometheus.DefaultRegisterer).NewCounter(prometheus.CounterOpts{
		Subsystem: "trade",
		Name:      "assetop_seq_guard_bootstrap_failures_total",
		Help:      "启动期补 trade_player_op_seq 哨兵守卫行失败的次数;>0 表示本副本的首次上架托管会全部失败。",
	})
	seqGuardMissingTotal = promauto.With(prometheus.DefaultRegisterer).NewCounter(prometheus.CounterOpts{
		Subsystem: "trade",
		Name:      "assetop_seq_guard_missing_total",
		Help:      "建 seq 行时发现哨兵守卫行不存在(fail-closed,本次上架失败)的次数。",
	})
	seqRowMissingInTxTotal = promauto.With(prometheus.DefaultRegisterer).NewCounter(prometheus.CounterOpts{
		Subsystem: "trade",
		Name:      "assetop_seq_row_missing_in_tx_total",
		Help:      "业务事务确认 seq 行时读不到(短事务刚建过)的次数;>0 说明有人删了 seq 行,按数据事故处理。",
	})
)

// ObserveSeqGuardBootstrapFailure 记一次启动期 bootstrap 失败。
// 导出给装配层(svc)调:那一层拿得到失败原因,但 data 才是这张表与这条指标的归属方。
func ObserveSeqGuardBootstrapFailure() { seqGuardBootstrapFailuresTotal.Inc() }

// EnsureSeqGuardRow 建哨兵守卫行 (0, 0)。**启动期 bootstrap 专用**,不在请求路径上调。
//
// 何时建:服务启动、schemamigrate 建表之后(go/trade/trade.go 的 ensureSchema 先于
// svcCtx.StartAssetChannel)。刻意不放进 schemamigrate:那一层只按 proto 出 DDL、不播种业务行,
// 放进去会让"补一行数据"和"改表结构"共用一份需要人工确认的迁移台账。
//
// 为什么这里可以用有界重试,而 EnsureSeqRows 的守卫路径不许:哨兵行自己没有守卫可排,它就是
// "事务外同键并发插入 + 首插者回滚"那一种 1213 —— InnoDB 固有、SQL 层去不掉的情形。收敛论证:语句幂等
// (行已存在是空操作,不改纪元)、跑在自动提交里、没有任何副作用要复位,所以整体重跑安全;牺牲者重跑时
// 要么撞上胜者已提交的行(空操作成功),要么自己插入成功 —— 两种都终止。退避带 ±20% 抖动且可取消
// (assetop.EnsureSeqRowRetry 用 DefaultTxRetryConfig:3 次、10ms 起),ctx 到期立刻返回。
// 这一行永不删除,所以整个集群生命周期里这条重试路径至多在"第一批 trade 副本同时启动"时被走到。
func (r *AssetOpRepo) EnsureSeqGuardRow(ctx context.Context, nowMs uint64) error {
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	if err := assetop.EnsureSeqRowRetry(ctx, r.db, r.tables, seqGuardPlayerID, seqGuardStream, nowMs, IsRetryableTxError); err != nil {
		return fmt.Errorf("建 %s 的哨兵守卫行 (player=%d stream=%d) 失败: %w",
			AssetOpSeqTableName, seqGuardPlayerID, int32(seqGuardStream), err)
	}
	return nil
}

// normalizeSeqKeys 校验、去重并按 (PlayerID, Stream) **升序**排序。
//
// 排序是正确性的一部分,不是整洁癖:它是本服务对多把玩家 seq 行锁的确定性全序,理由见守卫一节
// "多个 key 时为什么必须按 (player_id, stream) 升序"。去重是因为同一个 key 传两次会在事务里
// 对同一行取两次锁 —— 无害但会让语句序列的机械断言失真。
//
// 哨兵键 (0, 0) 在这里直接拒:放行的话同一个事务会先锁守卫行、再把它当玩家 seq 行读写,
// AllocateSeq 还会推进它的 next_seq,守卫行从此带上业务语义。
func normalizeSeqKeys(keys []SeqKey) ([]SeqKey, error) {
	out := make([]SeqKey, 0, len(keys))
	for _, k := range keys {
		if k.PlayerID == seqGuardPlayerID || k.Stream == seqGuardStream {
			return nil, fmt.Errorf("trade: 非法的 seq 行键 (player=%d stream=%d):player_id=0 与 stream=0 是哨兵守卫行的键",
				k.PlayerID, int32(k.Stream))
		}
		out = append(out, k)
	}
	slices.SortFunc(out, func(a, b SeqKey) int {
		if c := cmp.Compare(a.PlayerID, b.PlayerID); c != 0 {
			return c
		}
		return cmp.Compare(int32(a.Stream), int32(b.Stream))
	})
	return slices.Compact(out), nil
}

// seqRowReader 是 *sql.DB 与 *sql.Tx 的公共子集,只为让"自动提交探针"与"事务内确认读"
// 共用同一段实现、同一条 SQL 常量。
type seqRowReader interface {
	QueryRowContext(ctx context.Context, query string, args ...any) *sql.Row
}

// missingSeqRows 用 sqlSeqRowExists(普通读,RC 下不取行锁)挑出还不存在的行,保持入参顺序。
func (r *AssetOpRepo) missingSeqRows(ctx context.Context, q seqRowReader, keys []SeqKey) ([]SeqKey, error) {
	missing := make([]SeqKey, 0, len(keys))
	for _, k := range keys {
		var exists int
		switch err := q.QueryRowContext(ctx, sqlSeqRowExists, k.PlayerID, uint32(k.Stream)).Scan(&exists); {
		case err == nil:
			continue
		case errors.Is(err, sql.ErrNoRows):
			missing = append(missing, k)
		default:
			return nil, fmt.Errorf("读 %s 行 (player=%d stream=%d): %w",
				AssetOpSeqTableName, k.PlayerID, int32(k.Stream), err)
		}
	}
	return missing, nil
}

// EnsureSeqRows 按需把 keys 的 seq 行建出来,**在它自己的 RC 短事务里完成并提交**。
//
// 它是全序里的"事务 A",必须在业务事务(事务 B)**开始之前**调用,不能塞进业务事务里
// (那正是 2026-09-29 拆掉的形态,理由见守卫一节"为什么把'锁哨兵 + 建行'拆进独立的短事务")。
//
// 形状:
//  1. 自动提交的普通读探针(每个 key 一条主键点查)。全部命中就**连事务都不开**、一条写语句都不发
//     —— 这是稳态路径(每玩家每流只有第一笔会缺行)。
//  2. 有缺行才开短事务:事务内再探一次(期间别的副本可能已经建出来了,那就连守卫都不必锁)
//     → 锁哨兵守卫行 FOR UPDATE → 按升序逐个 INSERT IGNORE → 提交。
//
// 这里用 assetop.WithTxRetry(RC + 有界重试)。**它不是用来兜死锁的**:环已经由哨兵行从 SQL 层
// 消掉了。它吸收的是 1205(锁等待超时:哨兵行被别人短暂持有)与 9007(TiDB 写冲突),以及滚动升级
// 混跑窗口里旧副本造成的 1213。整段是幂等的(探针只读、INSERT IGNORE 撞行是空操作、不改纪元),
// 没有任何内存副作用要复位,所以整体重跑安全。
//
// 预算:自带 opTimeout(constants.StoreOpTimeout),调用方 ctx 更早到期时以 ctx 为准。超时按失败返回,
// 本次上架失败 —— 宁可让玩家看见一个明确的失败,也不要让一次上架无界地挂在全局守卫行上。
//
// 返回 ErrSeqGuardRowMissing 表示启动期 bootstrap 没做成(见 EnsureSeqGuardRow),本次业务写失败,
// 并记一次 trade_assetop_seq_guard_missing_total。
func (r *AssetOpRepo) EnsureSeqRows(ctx context.Context, nowMs uint64, keys ...SeqKey) error {
	ordered, err := normalizeSeqKeys(keys)
	if err != nil {
		return err
	}
	if len(ordered) == 0 {
		return nil
	}
	ctx, cancel := r.bounded(ctx)
	defer cancel()

	missing, err := r.missingSeqRows(ctx, r.db, ordered)
	if err != nil {
		return err
	}
	if len(missing) == 0 {
		return nil // 稳态:不开事务、不碰守卫行
	}

	return assetop.WithTxRetry(ctx, r.db, txRetryAttempts, IsRetryableTxError, func(tx *sql.Tx) error {
		return r.ensureSeqRowsTx(ctx, tx, nowMs, missing)
	})
}

// ensureSeqRowsTx 是短事务 A 的事务体:普通读 →(仅缺行时)锁哨兵守卫行 → 按升序建行。
//
// 刻意不导出:它只有在"调用方持有的这个事务马上就会提交、而且这个事务里不会再碰任何业务行"的
// 前提下才是安全的,而这个前提没法从签名上表达。唯一的生产调用方是 EnsureSeqRows;
// 集成测试在同包里直接调它,只为把"插了先别提交"的持锁方摆出来。
//
// 契约:tx 必须是 READ COMMITTED。前置探针要看得见前一个建行者已提交的行,靠的正是 RC 的
// "每条语句一份新快照";RR 下快照固定在事务第一条普通读,会看不见而多走一次(无害但白锁守卫行)。
// keys 必须已经过 normalizeSeqKeys(升序、去重、无哨兵键)。
func (r *AssetOpRepo) ensureSeqRowsTx(ctx context.Context, tx *sql.Tx, nowMs uint64, keys []SeqKey) error {
	if tx == nil {
		return errors.New("trade: ensureSeqRowsTx 必须在一个事务里执行(tx 为 nil)")
	}
	missing, err := r.missingSeqRows(ctx, tx, keys)
	if err != nil {
		return err
	}
	if len(missing) == 0 {
		return nil
	}

	// 守卫行必须在**任何**建行之前拿到:所有首次建行者由此串行,插入者一回滚也只是放掉一把记录锁。
	// 这是本事务取的第一把锁 —— 取它的时候手上没有任何别的锁,所以画不出反向边。
	var guard int
	switch err := tx.QueryRowContext(ctx, sqlLockSeqGuardRow, seqGuardPlayerID, uint32(seqGuardStream)).Scan(&guard); {
	case err == nil:
	case errors.Is(err, sql.ErrNoRows):
		seqGuardMissingTotal.Inc()
		return fmt.Errorf("%w:启动期 bootstrap 未完成(svc.ServiceContext.StartAssetChannel 调 "+
			"EnsureSeqGuardRow,失败会打 ERROR 日志并记 trade_assetop_seq_guard_bootstrap_failures_total)。"+
			"本次不绕过守卫建行:那会把并发首次建行的 1213 装回去", ErrSeqGuardRowMissing)
	default:
		return fmt.Errorf("锁 %s 的哨兵守卫行失败: %w", AssetOpSeqTableName, err)
	}

	for _, k := range missing {
		// 语句与纪元语义由 assetop 独占(ensureSeqRowFormat):INSERT IGNORE、next_seq 从 1 起、
		// 已存在是空操作且不改纪元。这里不抄第二份 —— 两份迟早分叉,而分叉的表现是纪元对不上、
		// scene 把整条流当成需要重置。
		if err := assetop.EnsureSeqRowTx(ctx, tx, r.tables, k.PlayerID, k.Stream, nowMs); err != nil {
			return fmt.Errorf("建 %s 行 (player=%d stream=%d): %w", AssetOpSeqTableName, k.PlayerID, int32(k.Stream), err)
		}
	}
	return nil
}

// AllocateSeqsTx 在**调用方的业务事务(事务 B)里**确认 seq 行在,并按 (player_id, stream) 升序
// 逐个分配 seq。返回 key → 分配结果。
//
// 为什么把"确认"和"分配"收进同一个方法:这两步的顺序与**多把玩家 seq 行锁之间的顺序**就是本服务
// 全序的最后一段,而顺序错了的表现是并发下偶发 1213,不是响亮的失败。收进来之后调用方拿不到乱序的
// 机会 —— P3 要同时给买家 CREDIT 和卖家 CREDIT,那正是最容易写反的形状。
//
// 契约:
//   - 事务必须是 READ COMMITTED(assetop.WithTxRetry 的默认值;AllocateSeq 自己也是这条硬契约);
//   - **必须先调过 EnsureSeqRows 且它已经返回成功**。本方法只做普通读确认,一行都不建;
//   - 本事务里的业务行锁(P3 的 trade_listing / trade_order)要排在本方法**之前**;
//   - 调用方不得把哨兵键 (0, 0) 混进 keys(直接拒)。
//
// 确认读读不到时回 ErrSeqRowMissingInTx 并计数,绝不就地补行(理由见 ErrSeqRowMissingInTx)。
// 确认全部做完才开始取第一把 X:这样"行没了"能在一把锁都没拿之前就失败。
func (r *AssetOpRepo) AllocateSeqsTx(ctx context.Context, tx *sql.Tx, nowMs uint64, keys ...SeqKey) (map[SeqKey]assetop.Alloc, error) {
	if tx == nil {
		return nil, errors.New("trade: AllocateSeqsTx 必须在调用方的业务事务里执行(tx 为 nil)")
	}
	ordered, err := normalizeSeqKeys(keys)
	if err != nil {
		return nil, err
	}
	if len(ordered) == 0 {
		return map[SeqKey]assetop.Alloc{}, nil
	}

	missing, err := r.missingSeqRows(ctx, tx, ordered)
	if err != nil {
		return nil, err
	}
	if len(missing) > 0 {
		seqRowMissingInTxTotal.Inc()
		return nil, fmt.Errorf("%w (player=%d stream=%d):EnsureSeqRows 的短事务刚刚提交过它。"+
			"只可能是有人删了 seq 行或新加了清理路径 —— 本次不就地补行(那会把守卫的锁序反过来),"+
			"先查 %s 的数据与近期变更",
			ErrSeqRowMissingInTx, missing[0].PlayerID, int32(missing[0].Stream), AssetOpSeqTableName)
	}

	out := make(map[SeqKey]assetop.Alloc, len(ordered))
	for _, k := range ordered {
		a, err := assetop.AllocateSeq(ctx, tx, r.tables, k.PlayerID, k.Stream, assetop.DefaultLimits, nowMs)
		if err != nil {
			return nil, err
		}
		out[k] = a
	}
	return out, nil
}

// InsertOp 在调用方的事务里插入一行 outbox。主键冲突按故障返回:op_id 来自号段,撞号说明
// 发号源出了问题,绝不静默覆盖(同 InsertListing 的理由)。
//
// last_send_ms 按 LastSendMs 契约第 1 条写成**不小于 created_ms**(insertLastSendMs):插行即持租约,
// 提交后的同步投递紧接着就可能发出。生产者漏填(0)时不能写 0 —— 0 在放弃路径上是"Store 没维护",
// 虽然会 fail-closed 跳过,但下一次领取的懒初始化才会补上,中间那段窗口这一行没有任何安全依据。
func (r *AssetOpRepo) InsertOp(ctx context.Context, tx *sql.Tx, rec *tradepb.TradeAssetOpRecord) error {
	_, err := tx.ExecContext(ctx,
		"INSERT INTO "+AssetOpTableName+" ("+assetOpColumns+") VALUES "+
			"(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
		rec.GetOpId(), rec.GetPlayerId(), rec.GetStream(), rec.GetStreamEpoch(), rec.GetSeq(),
		int32(rec.GetKind()), int32(rec.GetStatus()), rec.GetDurable(),
		rec.GetAttempts(), rec.GetNextAttemptMs(), rec.GetDeadlineMs(), rec.GetLeaseUntilMs(), rec.GetLeaseToken(),
		rec.GetTxType(), int32(rec.GetRefKind()), rec.GetRefId(), rec.GetPayload(),
		rec.GetLastOutcome(), rec.GetLastReason(), rec.GetCreatedMs(), rec.GetUpdatedMs(),
		// 新行一律是空留痕:人工终结只由 ResolveManually 写,插入路径绝不预置操作人。
		rec.GetResolvedBy(), rec.GetResolveReason(),
		insertLastSendMs(rec),
	)
	if err != nil {
		return fmt.Errorf("insert %s %d: %w", AssetOpTableName, rec.GetOpId(), err)
	}
	return nil
}

// insertLastSendMs 是插行时写进 last_send_ms 的值:max(记录上的 last_send_ms, created_ms)。
// 取较大值而不是无条件覆盖成 created_ms:last_send_ms 只能偏大(安全)不能偏小,生产者显式给了更晚的时刻就照用。
func insertLastSendMs(rec *tradepb.TradeAssetOpRecord) uint64 {
	return max(rec.GetLastSendMs(), rec.GetCreatedMs())
}

// ---------------------------------------------------------------------------
// assetop.Store
// ---------------------------------------------------------------------------
//
// # last_send_ms 的维护(docs/design/landing-recovery-sync-digest.md §6.3,以 go/shared/assetop/abandon.go
// 文件头「LastSendMs 契约」为准)
//
// 它是"此前任何一次投递请求**可能**发出的最晚时刻",自动放弃靠它证明"scene 不可能再收到这条 seq"。
// 本文件的五处写法与契约一一对应,改任何一处先回去读契约:
//  1. 插行:insertLastSendMs(不小于 created_ms);
//  2. 领取(sqlClaimAssetOp):在改写 lease_until_ms **之前**把旧租约折进来;
//  3. 重排(sqlRescheduleAssetOp):只在本次确实发过请求(!res.Local)时推进到 nowMs;
//  4. 毒行推迟(sqlPoisonAssetOp)与归还租约(sqlReleaseAssetOpLease):只清租约,不动它;
//  5. 存量行(加列前插入,值为 0)不做单独回填,在第 2 条的 CAS 里懒初始化为 GREATEST(created_ms, updated_ms)。
//
// 列是 schemamigrate 按 proto 自动 ADD COLUMN 加出来的(bigint unsigned NOT NULL DEFAULT 0),
// 存量行的 0 正是第 5 条要处理的形态,所以不需要额外迁移步骤。
const (
	// sqlClaimAssetOp:单行主键 CAS 领取。SET 子句的顺序是正确性的一部分:
	//   - last_send_ms 写在 lease_until_ms 与 updated_ms **之前**。MySQL 单表 UPDATE 按从左到右求值、
	//     后面的赋值看得见前面赋过的新值;写在最前面,"从左到右"与"一律用旧值"两种实现读到的都是**上一个**
	//     租约的 lease_until_ms 和**上一次**写下的 updated_ms(契约第 2、5 条要的正是旧值)。
	//   - 上一个租约正常收尾时 lease_until_ms 已清 0,GREATEST 不起作用;没收尾(发完请求就崩溃、重排落空 /
	//     出错)时它是那个已过期的非零值,而那次发送一定早于它(投递预算 < 租约),折进来就盖住了它。
	//   - IF(last_send_ms = 0, …) 是存量行的懒初始化:此前每次发送之后都有一次写 updated_ms 的重排或终结,
	//     没收尾的那次由 lease_until_ms 盖住,所以初值不小于任何真实发送。偏保守的代价只是放弃晚一个安全间隔。
	// lease_until_ms / updated_ms / last_send_ms 都不在任何二级索引里,这条语句仍只改聚簇记录。
	sqlClaimAssetOp = "UPDATE " + AssetOpTableName + " SET" +
		" `last_send_ms` = GREATEST(IF(`last_send_ms` = 0, GREATEST(`created_ms`, `updated_ms`), `last_send_ms`), `lease_until_ms`)," +
		" `lease_until_ms` = ?, `lease_token` = ?, `updated_ms` = ?" +
		" WHERE `op_id` = ? AND `status` = ? AND `lease_until_ms` < ?"

	// sqlRescheduleAssetOp:带领取令牌的重排。最后一项 last_send_ms 的两个参数是 (!res.Local, nowMs):
	// 本地合成的 NOT_HERE 根本没发出请求,推进它会让停放中的离线行永远"刚发过",自动放弃永远不会发生;
	// 传输失败、坏流号这类不是 Local 的结果一律按"发过"保守处理。nowMs 是 ProcessOne 的开始时刻,比真实
	// 发送早不到一个 OpBudget(< Lease),这段差由 AbandonSafetyGap ≥ Lease 的下限吸收(契约第 3 条)。
	sqlRescheduleAssetOp = "UPDATE " + AssetOpTableName +
		" SET `attempts` = `attempts` + 1, `next_attempt_ms` = ?, `lease_until_ms` = 0," +
		" `durable` = ?, `last_outcome` = ?, `last_reason` = ?, `updated_ms` = ?," +
		" `last_send_ms` = IF(?, GREATEST(`last_send_ms`, ?), `last_send_ms`)" +
		" WHERE `op_id` = ? AND `status` = ? AND `lease_token` = ?"

	// sqlPoisonAssetOp:毒行推迟,只清租约、不动 last_send_ms(契约第 4 条)。
	// 令牌之外还带 `status = PENDING`(与 guild 的 sqlPoisonAssetOp 同形):人工终结的 CAS 只看 status、不换令牌,
	// 可能恰好落在领取的 CAS 与这里之间;只凭令牌会把已终结的行改回"一小时后再来"、抹掉 last_outcome,
	// 还会把终态行的 next_attempt_ms(= 终结时刻,清理按它判龄)推后。
	sqlPoisonAssetOp = "UPDATE " + AssetOpTableName +
		" SET `last_outcome` = 0, `next_attempt_ms` = ?, `lease_until_ms` = 0, `updated_ms` = ?" +
		" WHERE `op_id` = ? AND `lease_token` = ? AND `status` = ?"

	// sqlFinalizeAssetOp:终结 CAS。同写 next_attempt_ms = 终结时刻、清租约(与 guild 的 sqlFinalizeAssetOp 同形):
	// 终态行的 next_attempt_ms 从此就是终结时刻,终态行清理(CleanupTerminalOps)按它判龄、走 idx_0。
	sqlFinalizeAssetOp = "UPDATE " + AssetOpTableName +
		" SET `status` = ?, `durable` = 1, `last_outcome` = ?, `last_reason` = ?," +
		" `lease_until_ms` = 0, `next_attempt_ms` = ?, `updated_ms` = ?" +
		" WHERE `op_id` = ? AND `status` = ?"

	// sqlResolveAssetOp:人工终结 CAS。不置 durable、不改 last_outcome(理由见 ResolveManually),
	// 同样同写 next_attempt_ms = 终结时刻。
	sqlResolveAssetOp = "UPDATE " + AssetOpTableName +
		" SET `status` = ?, `resolved_by` = ?, `resolve_reason` = ?, `lease_until_ms` = 0," +
		" `next_attempt_ms` = ?, `updated_ms` = ?" +
		" WHERE `op_id` = ? AND `status` = ?"
)

// ListDue 非加锁一致性读,只取主键,走 (status, next_attempt_ms) 索引。
// 不用范围 UPDATE 领取:那会在 RR 下加 next-key 锁,与业务事务里 AllocateSeq 的
// FOR UPDATE + 插入新 op 行互相等待,被判死锁牺牲的是玩家的请求(§4.37)。
//
// **两段查询,不是一条 SQL**(X-03 防饿死,契约见 assetop.Store.ListDue):
// 第一段只取新行(attempts < FreshAttemptLimit)并占满 limit;只有它不够时才发第二段取老行,
// 且第二段只补缺口。写成一条 SQL 会饿死新行:老行的退避被 MaxBackoff 封顶在 60s,于是它们
// 永远"早就到期",按 next_attempt_ms 排序必定霸占整批名额,新提交的托管指令永远排不上号。
//
// 两段谓词互斥,但这是**两次独立的非锁读**,期间某行的 attempts 可能刚好从 2 跳到 3 而被两段
// 都取到,所以仍要按 op_id 去重。第一段的 id 排在前面,让 Claim 先抢新行。
func (r *AssetOpRepo) ListDue(ctx context.Context, nowMs uint64, limit int) ([]uint64, error) {
	if limit <= 0 {
		return nil, nil
	}
	ctx, cancel := r.bounded(ctx)
	defer cancel()

	fresh, err := r.listDueSegment(ctx, nowMs, limit, true)
	if err != nil {
		return nil, err
	}
	if len(fresh) >= limit {
		return fresh, nil
	}

	aged, err := r.listDueSegment(ctx, nowMs, limit-len(fresh), false)
	if err != nil {
		return nil, err
	}
	seen := make(map[uint64]struct{}, len(fresh))
	for _, id := range fresh {
		seen[id] = struct{}{}
	}
	for _, id := range aged {
		if _, dup := seen[id]; dup {
			continue
		}
		fresh = append(fresh, id)
	}
	return fresh, nil
}

// listDueSegment 跑 ListDue 的其中一段。freshOnly=true 取 attempts < FreshAttemptLimit 的新行,
// false 取 >= 的老行;两段共用同样的到期条件与排序。
// 排序带 op_id 是为了确定性:同毫秒到期的行在多个副本上顺序一致,减少抢同一行的概率。
func (r *AssetOpRepo) listDueSegment(ctx context.Context, nowMs uint64, limit int, freshOnly bool) ([]uint64, error) {
	cmp := ">="
	if freshOnly {
		cmp = "<"
	}
	rows, err := r.db.QueryContext(ctx,
		"SELECT `op_id` FROM "+AssetOpTableName+
			" WHERE `status` = ? AND `next_attempt_ms` <= ? AND `lease_until_ms` < ?"+
			" AND `attempts` "+cmp+" ?"+
			" ORDER BY `next_attempt_ms` ASC, `op_id` ASC LIMIT ?",
		PendingStatus(), nowMs, nowMs, assetop.FreshAttemptLimit, limit)
	if err != nil {
		return nil, fmt.Errorf("list due %s (fresh=%v): %w", AssetOpTableName, freshOnly, err)
	}
	defer func() { _ = rows.Close() }()
	var ids []uint64
	for rows.Next() {
		var id uint64
		if err := rows.Scan(&id); err != nil {
			return nil, fmt.Errorf("scan due %s: %w", AssetOpTableName, err)
		}
		ids = append(ids, id)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate due %s: %w", AssetOpTableName, err)
	}
	return ids, nil
}

// Claim 单行主键 CAS 领取(autocommit),紧挨着处理前调用,租约从"领到这一行"起算。
// RowsAffected == 0 表示这一行已被别的副本领走(或已终结),返回 (Op{}, false, nil)。
//
// payload 解不开的毒行不能卡住整个循环:把它推迟到 poisonUntilMs 并回 assetop.ErrPoisonRow,
// 循环记 decode 计数后继续下一行(§4.37)。
//
// poisonUntilMs 由 assetop 的重投循环按 LoopConfig.PoisonDelay 算好传进来。**这里不许再自写
// 一份毒行延迟常量**:写了的话 yaml 里改 PoisonDelay 不生效,两份值迟早分叉,而分叉的表现是
// "配置改了没用",没有任何报错。
//
// 领取的同一条 CAS 里维护 last_send_ms(折旧租约 + 存量行懒初始化,见 sqlClaimAssetOp),
// 回读时填 Op.CreatedMs 与 Op.LastSendMs —— 自动放弃只认这两个回读值,0 一律 fail-closed 跳过。
// 落地唤醒(Loop.WakePlayer)与自动放弃(Loop.AbandonOnce)领行走的也是这同一个方法。
func (r *AssetOpRepo) Claim(ctx context.Context, opID, nowMs, leaseUntilMs, poisonUntilMs, token uint64) (assetop.Op, bool, error) {
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	res, err := r.db.ExecContext(ctx, sqlClaimAssetOp,
		leaseUntilMs, token, nowMs, opID, PendingStatus(), nowMs)
	if err != nil {
		return assetop.Op{}, false, fmt.Errorf("claim %s %d: %w", AssetOpTableName, opID, err)
	}
	affected, err := res.RowsAffected()
	if err != nil {
		return assetop.Op{}, false, fmt.Errorf("claim %s %d rows affected: %w", AssetOpTableName, opID, err)
	}
	if affected != 1 {
		return assetop.Op{}, false, nil
	}

	rec, err := r.getOp(ctx, opID)
	if err != nil {
		return assetop.Op{}, false, err
	}
	bundle := &assetpb.AssetBundle{}
	if len(rec.GetPayload()) > 0 {
		if err := proto.Unmarshal(rec.GetPayload(), bundle); err != nil {
			r.markPoison(ctx, opID, token, nowMs, poisonUntilMs)
			return assetop.Op{}, false, fmt.Errorf("%w: op_id=%d: %v", assetop.ErrPoisonRow, opID, err)
		}
	}
	return assetop.Op{
		OpID:        rec.GetOpId(),
		PlayerID:    rec.GetPlayerId(),
		Stream:      assetpb.AssetOpStream(rec.GetStream()),
		Seq:         rec.GetSeq(),
		StreamEpoch: rec.GetStreamEpoch(),
		// correlation_id 取 op_id,**不是** ref_id:§4.38 的人工终结以 scene 流水
		// correlation_id 定位唯一一行 outbox。取 ref_id 的话,同一件商品的托管与回退
		// 两条 op 会在 transaction_log 里落成同一个 correlation_id,取证就失效了。
		// 这里必须与 reconcile 首次投递时填的值一致,否则重投会换一个 correlation。
		CorrelationID: rec.GetOpId(),
		TxType:        rec.GetTxType(),
		Bundle:        bundle,
		Attempts:      rec.GetAttempts(),
		DeadlineMs:    rec.GetDeadlineMs(),
		LeaseToken:    token,
		LastReason:    rec.GetLastReason(),
		CreatedMs:     rec.GetCreatedMs(),
		LastSendMs:    rec.GetLastSendMs(),
	}, true, nil
}

// markPoison 把解不开 payload 的行推迟到 poisonUntilMs,并记下"最近一次结局未知"。
// 用本次领取的 lease_token 且仍为 PENDING 做 CAS:不推迟别的副本刚领走的同一行,也不改写领取之后
// 才被人工终结的行(sqlPoisonAssetOp)。只清租约,不动 last_send_ms(LastSendMs 契约第 4 条)。
// 失败只记在返回给调用方的错误里之外的日志层:这里没有 logger,推迟失败的后果是
// 下一轮再撞一次同一行(仍然跳过),不会丢数据,所以吞掉写错误但不吞解码错误。
func (r *AssetOpRepo) markPoison(ctx context.Context, opID, token, nowMs, poisonUntilMs uint64) {
	_, _ = r.db.ExecContext(ctx, sqlPoisonAssetOp, poisonUntilMs, nowMs, opID, token, PendingStatus())
}

// Finalize 把行终结成 status,并在同一事务里做对侧账。RowsAffected == 1 才算本次终结
// (返回 true);已被别的副本终结时返回 false,调用方不得重复入账。
//
// 锁序:全序的唯一事实源在本文件上方"seq 行的建行守卫"一节(含哨兵守卫行那一段),
// 与 guild 的 guild → guild_member → seq → op 同形。本方法本身只锁一行 trade_asset_op
// (主键 CAS),不碰 seq 表,所以它是全序的最后一段。
//
// 本批没有订单与卖家账,所以事务里只有 outbox 一张表;对侧账的位置留在下面的 TODO 注释处,
// 由 P3 在**同一个事务**里补(拆成两个事务就会出现"发了货没改状态")。
//
// 只接受四个终态(isFinalizableRecord):PENDING / UNSPECIFIED 写进去等于没终结或查不到结局;
// ABANDONED 只能经 Abandon(带令牌、六条证据齐全)写入。重投循环本来就只会传这四个,这道闸防的是绕过循环的调用者。
// 同写 next_attempt_ms = 终结时刻(sqlFinalizeAssetOp),终态行清理按它判龄。
func (r *AssetOpRepo) Finalize(ctx context.Context, op assetop.Op, status assetop.Status, res assetop.Result, nowMs uint64) (bool, error) {
	final := StatusToRecord(status)
	if !isFinalizableRecord(final) {
		return false, fmt.Errorf("finalize %s %d: 终结状态非法 (%s)", AssetOpTableName, op.OpID, status)
	}
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	finalized := false
	err := assetop.WithTxRetry(ctx, r.db, txRetryAttempts, IsRetryableTxError, func(tx *sql.Tx) error {
		finalized = false
		result, err := tx.ExecContext(ctx, sqlFinalizeAssetOp,
			int32(final), uint32(res.Outcome), res.Reason, nowMs, nowMs, op.OpID, PendingStatus())
		if err != nil {
			return fmt.Errorf("finalize %s %d: %w", AssetOpTableName, op.OpID, err)
		}
		affected, err := result.RowsAffected()
		if err != nil {
			return fmt.Errorf("finalize %s %d rows affected: %w", AssetOpTableName, op.OpID, err)
		}
		if affected != 1 {
			return nil // 已被别的副本终结:不是错误,也不做对侧账
		}
		finalized = true
		// P3 对侧账在这里(同一事务):
		//   ESCROW_DEBIT  APPLIED → 商品 ESCROWING → LISTED;REJECTED/ABORTED → ESCROW_REJECTED;
		//   DELIVER_CREDIT APPLIED → 订单 DELIVERING → DELIVERED + 卖家入账;
		//   RETURN_CREDIT  APPLIED → 商品 RETURNING → RETURNED。
		//   APPLIED_PARTIAL 一律不入账、不退款,转人工补偿(§4.33)。
		return nil
	})
	if err != nil {
		return false, err
	}
	return finalized, nil
}

// ResolveManually 是 §4.38 的人工终结通道:把一行卡死的 outbox 按人工判定直接落成终局,
// 并留下操作人与依据。它**不查证据、不猜结论** —— 调用方(管理入口)已经按 scene 流水的
// correlation_id = op_id 判定过,本方法只负责落库 + 留痕 + 与自动路径同一把 CAS。
//
// 与 Finalize 完全同形:同一个事务、同一条 `status = PENDING` 的 CAS、RowsAffected == 1
// 才算本次终结并做对侧账。两条路径共用这个条件,人工与循环同时下手也只会有一个赢家。
//
// durable 刻意**不**置 1:这一行的终局来自人工判定,不是 scene 确认的落盘结局,
// 把它标成 durable 会让事后对账分不清"scene 真的答过"和"人写上去的"。
func (r *AssetOpRepo) ResolveManually(ctx context.Context, m assetop.ManualResolution, nowMs uint64) (bool, error) {
	// 状态合法性、操作人与理由的长度由 assetop.Loop.ResolveManually 在进来之前校验;
	// 这里兜住本表自己的那一份:映射不出库值(落成 UNSPECIFIED 会让客服查不到结局)、PENDING,
	// 以及 ABANDONED —— 放弃只能由自动放弃在全部证据成立时产生,人工判定"放弃"应按证据选 ABORTED
	// (与 assetop.ManualResolution.validate 同一口径;那道校验在包级函数里,直接调本方法会绕过它)。
	final := StatusToRecord(m.Final)
	if !isFinalizableRecord(final) {
		return false, fmt.Errorf("manual resolve %s %d: 终结状态非法 (%s)", AssetOpTableName, m.OpID, m.Final)
	}
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	resolved := false
	err := assetop.WithTxRetry(ctx, r.db, txRetryAttempts, IsRetryableTxError, func(tx *sql.Tx) error {
		resolved = false
		result, err := tx.ExecContext(ctx, sqlResolveAssetOp,
			int32(final), m.Operator, m.Reason, nowMs, nowMs, m.OpID, PendingStatus())
		if err != nil {
			return fmt.Errorf("manual resolve %s %d: %w", AssetOpTableName, m.OpID, err)
		}
		affected, err := result.RowsAffected()
		if err != nil {
			return fmt.Errorf("manual resolve %s %d rows affected: %w", AssetOpTableName, m.OpID, err)
		}
		if affected != 1 {
			return nil // 行不存在或已被终结:不是错误,也不做对侧账
		}
		resolved = true
		// P3 对侧账在这里,与 Finalize 的 TODO 处**同一份**逻辑(抽成一个内部函数,
		// 两条路径都调它);人工与自动走两份对侧账就会出现只有一边改了商品状态。
		return nil
	})
	if err != nil {
		return false, err
	}
	return resolved, nil
}

// Reschedule 退回待办并推迟下一次投递。带 lease_token 做 CAS:租约已被别人接管时本次更新落空,
// 不会把别的副本刚排好的时间覆盖掉。
//
// 落空(RowsAffected == 0)必须回 assetop.ErrLeaseLost,这是契约的一部分。以前这里写的是
// `_, err :=`,把 RowsAffected 丢掉、一律回 nil —— 于是"我这一轮的结果被另一个副本丢弃了"
// 在指标里完全看不见:循环把它当成功,`assetop_reschedule_lost_total` 恒为 0,租约打架
// 只能靠人肉比对日志时间戳才发现。
//
// last_send_ms 只在本次确实发过请求(!res.Local)时推进到 nowMs(LastSendMs 契约第 3 条,见 sqlRescheduleAssetOp)。
func (r *AssetOpRepo) Reschedule(ctx context.Context, op assetop.Op, nextAttemptMs uint64, res assetop.Result, nowMs uint64) error {
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	execRes, err := r.db.ExecContext(ctx, sqlRescheduleAssetOp,
		nextAttemptMs, res.Durable, uint32(res.Outcome), res.Reason, nowMs,
		!res.Local, nowMs,
		op.OpID, PendingStatus(), op.LeaseToken)
	if err != nil {
		return fmt.Errorf("reschedule %s %d: %w", AssetOpTableName, op.OpID, err)
	}
	affected, err := execRes.RowsAffected()
	if err != nil {
		return fmt.Errorf("reschedule %s %d rows affected: %w", AssetOpTableName, op.OpID, err)
	}
	if affected == 0 {
		return fmt.Errorf("%w: op_id=%d", assetop.ErrLeaseLost, op.OpID)
	}
	return nil
}

// OldestPendingCreatedMs 实现 assetop 的可选接口,喂 assetop_pending_oldest_age_seconds。
// 每 30s 一次的聚合查询;本批表小,先不为它单独建索引(积压真上来了再按 (status, stream) 补)。
func (r *AssetOpRepo) OldestPendingCreatedMs(ctx context.Context, stream assetpb.AssetOpStream) (uint64, bool, error) {
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	var oldest sql.NullInt64
	err := r.db.QueryRowContext(ctx,
		"SELECT MIN(`created_ms`) FROM "+AssetOpTableName+" WHERE `status` = ? AND `stream` = ?",
		PendingStatus(), uint32(stream)).Scan(&oldest)
	if err != nil {
		return 0, false, fmt.Errorf("oldest pending %s: %w", AssetOpTableName, err)
	}
	if !oldest.Valid || oldest.Int64 < 0 {
		return 0, false, nil
	}
	return uint64(oldest.Int64), true, nil
}

// ---------------------------------------------------------------------------
// 落地恢复:唤醒 / 日对账 / 自动放弃(docs/design/landing-recovery-sync-digest.md §5.3、§6.2、§6.3)
// ---------------------------------------------------------------------------
//
// 与 guild 的 GuildAssetStore 同口径实现,避免两份分叉。trade 的资产通道默认关闭、P3 未开工,
// 所以这里只有存储侧;对侧账(商品状态迁移、退款)与 Finalize 一样留在 P3 的 TODO 处。

const (
	// sqlListPendingByPlayer:落地唤醒按玩家列 PENDING 行,走 idx_trade_asset_op_1
	// (player_id, stream, stream_epoch, status, seq)的 player_id 前缀,status 在索引里过滤。
	// 非加锁读;不看 next_attempt_ms / lease_until_ms(唤醒就是要提前,租约由随后的 Claim CAS 判)。
	// 按 (stream, stream_epoch, seq) 升序:与索引顺序一致,也让同一条流里先发的 seq 先投。
	sqlListPendingByPlayer = "SELECT `op_id` FROM " + AssetOpTableName +
		" WHERE `player_id` = ? AND `status` = ? ORDER BY `stream` ASC, `stream_epoch` ASC, `seq` ASC LIMIT ?"

	// sqlListPendingFactsPage:日对账按主键分页非加锁读全部 PENDING 行的分类所需列。
	sqlListPendingFactsPage = "SELECT `op_id`, `stream`, `created_ms`, `attempts`, `last_outcome`, `last_reason` FROM " +
		AssetOpTableName + " WHERE `status` = ? AND `op_id` > ? ORDER BY `op_id` ASC LIMIT ?"

	// sqlListAbandonCandidates:自动放弃的候选(非加锁读,按 op_id 翻页)。只给候选,判定在持租约之后逐行复核。
	sqlListAbandonCandidates = "SELECT `op_id` FROM " + AssetOpTableName +
		" WHERE `status` = ? AND `created_ms` < ? AND `op_id` > ? ORDER BY `op_id` ASC LIMIT ?"

	// sqlAbandonAssetOp:放弃 CAS,以 (op_id, status=PENDING, lease_token) 为条件(assetop.Abandoner 契约)。
	// 不置 durable(不是 scene 确认的结局)、不改 last_outcome(保留最后一次真实答复作证据);
	// next_attempt_ms 同写终结时刻(与 sqlFinalizeAssetOp 一致,清理按它判龄)。
	sqlAbandonAssetOp = "UPDATE " + AssetOpTableName +
		" SET `status` = ?, `lease_until_ms` = 0, `next_attempt_ms` = ?, `updated_ms` = ?" +
		" WHERE `op_id` = ? AND `status` = ? AND `lease_token` = ?"

	// sqlReleaseAssetOpLease:放弃判定"这一轮不放弃"之后归还租约。只清租约,不动 attempts / next_attempt_ms /
	// last_send_ms(LastSendMs 契约第 4 条)。lease_until_ms / updated_ms 都不在二级索引里,与 Claim 同为
	// 只改聚簇记录的自动提交单行 UPDATE。
	sqlReleaseAssetOpLease = "UPDATE " + AssetOpTableName + " SET `lease_until_ms` = 0, `updated_ms` = ?" +
		" WHERE `op_id` = ? AND `status` = ? AND `lease_token` = ?"
)

// auditPageSize 是日对账每页读多少行。按主键分页,不让一条大查询长时间占着连接(PendingClassCounter 契约)。
const auditPageSize = 500

// ListPendingByPlayer 实现 assetop.PlayerPendingLister:列出某玩家的 PENDING 行主键,条数 <= limit。
// 没有 PENDING 行时回 (nil, nil)。limit <= 0 直接回空,不发 SQL。
func (r *AssetOpRepo) ListPendingByPlayer(ctx context.Context, playerID uint64, limit int) ([]uint64, error) {
	if limit <= 0 {
		return nil, nil
	}
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	ids, err := r.queryOpIDs(ctx, sqlListPendingByPlayer, playerID, PendingStatus(), limit)
	if err != nil {
		return nil, fmt.Errorf("list pending %s by player: %w", AssetOpTableName, err)
	}
	return ids, nil
}

// CountPendingByClass 实现 assetop.PendingClassCounter:按主键分页非加锁读全部 PENDING 行,
// 逐行交给 assetop.PendingTally(内部用 assetop.ClassifyPending)分类计数 —— 口径只在 assetop 一处,
// 这里不另写 CASE。abandonBeforeMs 由循环按 AbandonAfter 算好传入(0 = 不分出 abandon_candidate)。
//
// 每页各自带 opTimeout 上限;任何一页失败整轮作废(返回错误,循环保留上一轮的 gauge),不报半截计数。
func (r *AssetOpRepo) CountPendingByClass(ctx context.Context, nowMs, abandonBeforeMs uint64) ([]assetop.ClassCount, error) {
	tally := assetop.NewPendingTally(nowMs, abandonBeforeMs)
	var afterOpID uint64
	for {
		if err := ctx.Err(); err != nil {
			return nil, fmt.Errorf("count pending %s by class: %w", AssetOpTableName, err)
		}
		n, lastOpID, err := r.tallyPendingPage(ctx, tally, afterOpID)
		if err != nil {
			return nil, fmt.Errorf("count pending %s by class (after op_id %d): %w", AssetOpTableName, afterOpID, err)
		}
		if n < auditPageSize {
			return tally.Counts(), nil
		}
		afterOpID = lastOpID
	}
}

// tallyPendingPage 读一页(op_id > afterOpID 的至多 auditPageSize 行)并累加,返回本页行数与最后一个 op_id。
func (r *AssetOpRepo) tallyPendingPage(ctx context.Context, tally *assetop.PendingTally, afterOpID uint64) (int, uint64, error) {
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	rows, err := r.db.QueryContext(ctx, sqlListPendingFactsPage, PendingStatus(), afterOpID, auditPageSize)
	if err != nil {
		return 0, 0, err
	}
	defer func() { _ = rows.Close() }()
	var (
		n      int
		lastID uint64
	)
	for rows.Next() {
		var opID, createdMs uint64
		var stream, attempts, outcome, reason uint32
		if err := rows.Scan(&opID, &stream, &createdMs, &attempts, &outcome, &reason); err != nil {
			return 0, 0, fmt.Errorf("scan pending facts: %w", err)
		}
		tally.Add(assetop.PendingFacts{
			Stream:      assetpb.AssetOpStream(stream),
			CreatedMs:   createdMs,
			Attempts:    attempts,
			LastOutcome: assetpb.AssetOpOutcome(outcome),
			LastReason:  reason,
		})
		n++
		lastID = opID
	}
	if err := rows.Err(); err != nil {
		return 0, 0, fmt.Errorf("iterate pending facts: %w", err)
	}
	return n, lastID, nil
}

// ListAbandonCandidates 实现 assetop.Abandoner:status=PENDING 且 created_ms < createdBeforeMs 且 op_id > afterOpID,
// 按 op_id 升序,只回主键,条数 <= limit。非加锁读;真正的判定在持租约之后由 assetop 逐行复核。
func (r *AssetOpRepo) ListAbandonCandidates(ctx context.Context, createdBeforeMs, afterOpID uint64, limit int) ([]uint64, error) {
	if limit <= 0 {
		return nil, nil
	}
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	ids, err := r.queryOpIDs(ctx, sqlListAbandonCandidates, PendingStatus(), createdBeforeMs, afterOpID, limit)
	if err != nil {
		return nil, fmt.Errorf("list abandon candidates %s: %w", AssetOpTableName, err)
	}
	return ids, nil
}

// Abandon 实现 assetop.Abandoner:以 (op_id, status=PENDING, lease_token) 为条件把行终结为 ABANDONED。
// RowsAffected == 0 → assetop.ErrLeaseLost(租约在判定期间被别人接管,别人可能已经发出请求,此时绝不能放弃)。
//
// 放弃的六条证据(持租约、玩家无位置、距最后一次可能的发送超过安全间隔、已落盘账本确定未见……)由
// assetop.Loop.AbandonOnce 在调用之前逐条拿到;本方法只负责"带令牌的终结"这最后一步。令牌为 0 时直接报错:
// 0 是"没有租约"的形态,拿它做 CAS 不能证明"这一刻租约在我手里",而放弃不可逆。
//
// 与 Finalize 同形:一个事务(WithTxRetry,RC),事务里只有这一条 trade_asset_op 的主键 CAS。
func (r *AssetOpRepo) Abandon(ctx context.Context, op assetop.Op, nowMs uint64) error {
	if op.LeaseToken == 0 {
		return fmt.Errorf("abandon %s %d: 缺领取令牌,不放弃", AssetOpTableName, op.OpID)
	}
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	return assetop.WithTxRetry(ctx, r.db, txRetryAttempts, IsRetryableTxError, func(tx *sql.Tx) error {
		result, err := tx.ExecContext(ctx, sqlAbandonAssetOp,
			int32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED), nowMs, nowMs,
			op.OpID, PendingStatus(), op.LeaseToken)
		if err != nil {
			return fmt.Errorf("abandon %s %d: %w", AssetOpTableName, op.OpID, err)
		}
		affected, err := result.RowsAffected()
		if err != nil {
			return fmt.Errorf("abandon %s %d rows affected: %w", AssetOpTableName, op.OpID, err)
		}
		if affected == 0 {
			return fmt.Errorf("%w: abandon op_id=%d", assetop.ErrLeaseLost, op.OpID)
		}
		// TODO(P3,docs/design/landing-recovery-sync-digest.md §6.3「trade:对侧账尚未实现」):对侧账在这里,
		// 与 Finalize 的 REJECTED / ABORTED 分支**同一份**逻辑、同一个事务(拆开会出现"终结了却没退"):
		//   ESCROW_DEBIT   → 商品 ESCROWING → ESCROW_REJECTED(scene 从未扣过卖家,没有钱要退);
		//   DELIVER_CREDIT → 买家已付的款按订单退回(放弃意味着这次交付作废,不退就是买家钱货两空);
		//   RETURN_CREDIT  → 商品 RETURNING 停在原地,转人工(物品仍在托管里)。
		// **P3 的 TRADE_CREDIT 生产者上线之前必须先补齐这里**;v1 没有任何 CREDIT 生产者,放弃只终结并留痕
		// (审计依据:本行的 op_id / ref_id、assetop_abandoned_total 与 assetop 打的放弃日志)。
		return nil
	})
}

// ReleaseLease 实现 assetop.Abandoner:判定"这一轮不放弃"之后归还租约(sqlReleaseAssetOpLease)。
// RowsAffected == 0 → assetop.ErrLeaseLost(行已被别人接管或已终结,没有可还的)。
func (r *AssetOpRepo) ReleaseLease(ctx context.Context, op assetop.Op, nowMs uint64) error {
	ctx, cancel := r.bounded(ctx)
	defer cancel()
	result, err := r.db.ExecContext(ctx, sqlReleaseAssetOpLease, nowMs, op.OpID, PendingStatus(), op.LeaseToken)
	if err != nil {
		return fmt.Errorf("release lease %s %d: %w", AssetOpTableName, op.OpID, err)
	}
	affected, err := result.RowsAffected()
	if err != nil {
		return fmt.Errorf("release lease %s %d rows affected: %w", AssetOpTableName, op.OpID, err)
	}
	if affected == 0 {
		return fmt.Errorf("%w: release lease op_id=%d", assetop.ErrLeaseLost, op.OpID)
	}
	return nil
}

// queryOpIDs 跑一条只回 op_id 的非加锁读。
func (r *AssetOpRepo) queryOpIDs(ctx context.Context, query string, args ...any) ([]uint64, error) {
	rows, err := r.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer func() { _ = rows.Close() }()
	var ids []uint64
	for rows.Next() {
		var id uint64
		if err := rows.Scan(&id); err != nil {
			return nil, fmt.Errorf("scan op_id: %w", err)
		}
		ids = append(ids, id)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate op_id: %w", err)
	}
	return ids, nil
}

// ---------------------------------------------------------------------------
// 终态行清理(对齐 guild 的 GuildAssetStore.CleanupOnce 终态部分;docs/design/guild-phase2/05-economy.md §5.22)
// ---------------------------------------------------------------------------
//
// 保留期(默认 30 天,config.AssetOpConf.TerminalRetentionDays)外的终态行逐行删掉,表不再无界增长。
//   - 只删 APPLIED / REJECTED / ABORTED / ABANDONED(cleanupStatuses)。**不删** APPLIED_PARTIAL(部分发放要人工补偿,
//     证据不能自动消失,X-15)与 PENDING(永不删)。ABANDONED 与其它终态同等对待(落地恢复设计 §6.3「状态与展示」)。
//   - 判龄按 next_attempt_ms:终态行上它等于终结时刻(Finalize / ResolveManually / Abandon 同写),走 idx_0。
//   - 每批"普通读至多 500 个候选主键 → 按主键升序逐行一个 RC 短事务{主键点锁 → 带复核条件的主键点删}",
//     形状与 guild 的 execCleanupDelete 相同:每个短事务只锁这一行的聚簇记录;终态行没有任何别的写者
//     (领取 / 重排 / 毒行 / 终结 / 放弃都带 status = PENDING 条件),所以清理只会单向等待、不会成环。
//   - 每行只尝试一次,失败的行下一轮再删(条件仍满足,不丢任何东西)。
//
// 与 guild 不同的一处:guild 删终态行之前先推进"清理水位"(asset_op_cleanup_watermark.go),因为 data_service 的
// 回档检查要靠它判断哪一段流水已经可能被删(07-rollback-fail-closed.md §7.4.2)。trade 目前**没有**回档检查消费
// trade_asset_op(只有 guild 的 GuildInternal.ListAppliedAssetOpsSince),所以不写水位;P3 若让回档检查覆盖 trade,
// 必须先补同样的 write-ahead 水位再开清理,否则调大保留期之后那段已删的流水会被误当成"可证明且没有分歧"。
//
// 本组语句只碰 trade_asset_op,一行都不碰 trade_player_op_seq(seq 行"永不删除"是守卫一节的前提)。

// CleanupConf 是终态行清理的节律与保留期。配置名与 guild 相同(CleanupIntervalMinutes / TerminalRetentionDays)。
type CleanupConf struct {
	Interval          time.Duration
	TerminalRetention time.Duration
}

// 清理节律,与 guild 同值(05 §5.22):单批 500 行、批间 100ms、每轮至多 20 批,一批候选不足 500 行即停。
const (
	cleanupBatchSize  = 500
	cleanupBatchPause = 100 * time.Millisecond
	cleanupMaxBatches = 20
	// cleanupStmtBudget 是单行清理短事务(点锁 + 点删 + 提交)的上限,与 guild 同值。
	// 跑满说明库有状况,本轮到此为止,已删的行照常计数,下一轮接着删。
	cleanupStmtBudget = 2 * time.Second
)

const (
	sqlListCleanupTerminalOps = "SELECT `op_id` FROM " + AssetOpTableName +
		" WHERE `status` IN (?, ?, ?, ?) AND `next_attempt_ms` < ? ORDER BY `op_id` ASC LIMIT ?"
	// sqlLockAssetOp:清理点删前的主键点锁。只许是完整主键等值 + FOR UPDATE、不带任何复核条件
	// (TiDB 只有这种形状才走 Point_Get 快路径;复核条件留在随后的点删里),理由同 guild 的同名语句。
	sqlLockAssetOp       = "SELECT `op_id` FROM " + AssetOpTableName + " WHERE `op_id` = ? FOR UPDATE"
	sqlCleanupTerminalOp = "DELETE FROM " + AssetOpTableName +
		" WHERE `op_id` = ? AND `status` IN (?, ?, ?, ?) AND `next_attempt_ms` < ?"
)

// cleanupStatuses 是可被清理的四个终态的库值,顺序即两条清理语句里 IN (?, ?, ?, ?) 的参数顺序。
func cleanupStatuses() [4]int32 {
	return [4]int32{
		int32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED),
		int32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_REJECTED),
		int32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABORTED),
		int32(tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED),
	}
}

// cleanupDeletedTotal 是清理删掉的终态行数。与本文件上方的守卫计数器同一个 registry、同一种无 label 形状:
// 长期为 0 而表在长,说明清理没在跑。
var cleanupDeletedTotal = promauto.With(prometheus.DefaultRegisterer).NewCounter(prometheus.CounterOpts{
	Subsystem: "trade",
	Name:      "assetop_cleanup_deleted_total",
	Help:      "终态资产指令清理删掉的行数(保留期外的 APPLIED / REJECTED / ABORTED / ABANDONED;APPLIED_PARTIAL 与 PENDING 永不删)。",
})

// recordCleanupDeleted:生产实现计指标;单测可替换为记录器(同 guild 的惯例,不去读全局注册表)。
var recordCleanupDeleted = func(n int64) { cleanupDeletedTotal.Add(float64(n)) }

// cleanupPoint 是清理 goroutine 与每一轮 recover 的 safego 点位名(safego_panic_total 的 label,必须是常量)。
const cleanupPoint = "trade.asset_op_cleanup"

// RunCleanup 阻塞运行终态行清理循环,ctx 取消即返回;调用方用 safego.Go(cleanupPoint 同名点位)启动,
// 形状与 guild 的 GuildAssetStore.RunCleanup 相同。
//
// 每个副本都跑:删除幂等,多副本同时跑无害;随机初始延迟 [0, Interval) 只是把各副本错开,少抢同一批行锁。
// 每一轮各自 recover:单轮 panic 只丢那一轮,清理不会从此静默停摆。
// Interval 或 TerminalRetention 非正时不启动(打 ERROR):config.Validate 在 CleanupEnabled 时已拦,这里只防绕过装配层。
func (r *AssetOpRepo) RunCleanup(ctx context.Context, c CleanupConf) {
	if c.Interval <= 0 || c.TerminalRetention <= 0 {
		logx.Errorf("[TradeAsset] Invalid cleanup config (interval=%v terminal_retention=%v); cleanup task not started",
			c.Interval, c.TerminalRetention)
		return
	}
	timer := time.NewTimer(time.Duration(rand.Int64N(int64(c.Interval))))
	defer timer.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-timer.C:
		}
		safego.Run(cleanupPoint, func() {
			deleted, err := r.CleanupTerminalOps(ctx, time.Now(), c.TerminalRetention)
			if err != nil && ctx.Err() == nil {
				logx.Errorf("[TradeAsset] Terminal asset op cleanup failed (deleted %d before the error): %v", deleted, err)
				return
			}
			if deleted > 0 {
				logx.Infof("[TradeAsset] Terminal asset op cleanup deleted %d row(s) older than %v", deleted, c.TerminalRetention)
			}
		})
		timer.Reset(c.Interval)
	}
}

// CleanupTerminalOps 跑一轮终态行清理(可测:时刻由调用方给),返回本轮删掉的行数。
// retention <= 0 拒绝执行(删光所有终态行不可能是有意的配置);时钟早于保留期时没有可删的行。
// 出错时已删的行数仍如实返回、照常计指标(每行各自提交,删掉的就是删掉了)。
func (r *AssetOpRepo) CleanupTerminalOps(ctx context.Context, now time.Time, retention time.Duration) (int64, error) {
	if retention <= 0 {
		return 0, fmt.Errorf("trade asset cleanup: 保留期必须为正(当前 %v)", retention)
	}
	nowMs := now.UnixMilli()
	retentionMs := retention.Milliseconds()
	if nowMs <= retentionMs {
		return 0, nil
	}
	cutoffMs := uint64(nowMs - retentionMs)

	var total int64
	for i := 0; i < cleanupMaxBatches; i++ {
		candidates, deleted, err := r.cleanupTerminalOpsBatch(ctx, cutoffMs)
		total += deleted
		if deleted > 0 {
			recordCleanupDeleted(deleted)
		}
		if err != nil {
			return total, fmt.Errorf("cleanup %s: %w", AssetOpTableName, err)
		}
		// 判"还有没有下一批"看候选数而不是删除数:复核落空的候选下一次候选读不会再读到它。
		if candidates < cleanupBatchSize {
			return total, nil
		}
		timer := time.NewTimer(cleanupBatchPause)
		select {
		case <-ctx.Done():
			timer.Stop()
			return total, ctx.Err()
		case <-timer.C:
		}
	}
	return total, nil
}

// cleanupTerminalOpsBatch:普通读至多 cleanupBatchSize 个保留期外的终态 op_id(升序),逐行短事务点锁 + 点删并复核条件。
func (r *AssetOpRepo) cleanupTerminalOpsBatch(ctx context.Context, cutoffMs uint64) (int, int64, error) {
	st := cleanupStatuses()
	readCtx, cancel := r.bounded(ctx)
	opIDs, err := r.queryOpIDs(readCtx, sqlListCleanupTerminalOps, st[0], st[1], st[2], st[3], cutoffMs, cleanupBatchSize)
	cancel()
	if err != nil {
		return 0, 0, fmt.Errorf("list terminal %s: %w", AssetOpTableName, err)
	}
	var deleted int64
	for _, opID := range opIDs {
		n, err := r.deleteTerminalOp(ctx, opID, cutoffMs)
		deleted += n
		if err != nil {
			return len(opIDs), deleted, fmt.Errorf("delete terminal %s %d: %w", AssetOpTableName, opID, err)
		}
	}
	return len(opIDs), deleted, nil
}

// deleteTerminalOp 在一个 RC 短事务里删一行:先主键点锁(不带复核条件),再跑带复核条件的主键点删。
// 返回删掉的行数(0 或 1):点锁读不到 = 已被别的副本删掉;复核落空 = 条件已不成立。
// 超过 1 行只可能是 WHERE 写坏了(不再是完整主键等值),整体回滚并报错。尝试次数为 1(失败的行下一轮再删)。
func (r *AssetOpRepo) deleteTerminalOp(ctx context.Context, opID, cutoffMs uint64) (int64, error) {
	ctx, cancel := context.WithTimeout(ctx, cleanupStmtBudget)
	defer cancel()
	st := cleanupStatuses()
	var deleted int64
	err := assetop.WithTxRetry(ctx, r.db, 1, IsRetryableTxError, func(tx *sql.Tx) error {
		deleted = 0 // 重试契约:结果只在成功返回前写到外层
		var locked uint64
		switch err := tx.QueryRowContext(ctx, sqlLockAssetOp, opID).Scan(&locked); {
		case errors.Is(err, sql.ErrNoRows):
			return nil
		case err != nil:
			return fmt.Errorf("lock: %w", err)
		}
		result, err := tx.ExecContext(ctx, sqlCleanupTerminalOp, opID, st[0], st[1], st[2], st[3], cutoffMs)
		if err != nil {
			return err
		}
		n, err := result.RowsAffected()
		if err != nil {
			return fmt.Errorf("read rows affected: %w", err)
		}
		if n > 1 {
			return fmt.Errorf("point delete removed %d rows (WHERE is no longer a full primary-key match)", n)
		}
		deleted = n
		return nil
	})
	if err != nil {
		return 0, err
	}
	return deleted, nil
}

// getOp 按主键取全列。
func (r *AssetOpRepo) getOp(ctx context.Context, opID uint64) (*tradepb.TradeAssetOpRecord, error) {
	row := r.db.QueryRowContext(ctx, "SELECT "+assetOpColumns+" FROM "+AssetOpTableName+" WHERE `op_id` = ?", opID)
	rec, err := scanAssetOp(row)
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			// 刚 CAS 成功又查不到:只可能是被并发删了(清理只删保留期外的终态行,刚领到的未决行不在其列),按故障返回。
			return nil, fmt.Errorf("select %s %d: 行在领取后消失", AssetOpTableName, opID)
		}
		return nil, fmt.Errorf("select %s %d: %w", AssetOpTableName, opID, err)
	}
	return rec, nil
}

// scanAssetOp 按 assetOpColumns 的列序扫一行。枚举列先扫 int32 再转型,不依赖
// database/sql 对具名整数类型的反射转换(同 scanListing)。
func scanAssetOp(row rowScanner) (*tradepb.TradeAssetOpRecord, error) {
	rec := &tradepb.TradeAssetOpRecord{}
	var kind, status, refKind int32
	if err := row.Scan(
		&rec.OpId, &rec.PlayerId, &rec.Stream, &rec.StreamEpoch, &rec.Seq,
		&kind, &status, &rec.Durable,
		&rec.Attempts, &rec.NextAttemptMs, &rec.DeadlineMs, &rec.LeaseUntilMs, &rec.LeaseToken,
		&rec.TxType, &refKind, &rec.RefId, &rec.Payload,
		&rec.LastOutcome, &rec.LastReason, &rec.CreatedMs, &rec.UpdatedMs,
		&rec.ResolvedBy, &rec.ResolveReason, &rec.LastSendMs,
	); err != nil {
		return nil, err
	}
	rec.Kind = tradepb.TradeAssetOpKind(kind)
	rec.Status = tradepb.TradeAssetOpStatus(status)
	rec.RefKind = tradepb.TradeAssetOpRefKind(refKind)
	return rec, nil
}

// StatusToRecord 把 assetop 的语义终态映射成本表的存储值。
// assetop.Status 刻意不绑库值(§4.43 #23),映射只此一处;它是单射,每个非 UNSPECIFIED 的库值恰好对应
// 一个语义值(asset_op_recovery_test.go 的 TestStatusToRecordIsABijection 机械守住"两边同改")。
func StatusToRecord(s assetop.Status) tradepb.TradeAssetOpStatus {
	switch s {
	case assetop.StatusPending:
		return tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_PENDING
	case assetop.StatusApplied:
		return tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED
	case assetop.StatusRejected:
		return tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_REJECTED
	case assetop.StatusAborted:
		return tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABORTED
	case assetop.StatusAppliedPartial:
		return tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED_PARTIAL
	case assetop.StatusAbandoned:
		return tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABANDONED
	default:
		// 未知语义值不能落成 PENDING(会被循环反复领走),也不能落成 APPLIED(会伪装成功)。
		return tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_UNSPECIFIED
	}
}

// isFinalizableRecord:Finalize 与 ResolveManually 只接受这四个终态。
//   - PENDING 写进去等于没终结,UNSPECIFIED 会让客服查不到结局;
//   - ABANDONED 只能经 Abandon 写入(带领取令牌,且六条证据由 assetop.Loop.AbandonOnce 逐条复核过)。
func isFinalizableRecord(st tradepb.TradeAssetOpStatus) bool {
	switch st {
	case tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_REJECTED,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_ABORTED,
		tradepb.TradeAssetOpStatus_TRADE_ASSET_OP_STATUS_APPLIED_PARTIAL:
		return true
	default:
		return false
	}
}

// txRetryAttempts 是业务写路径撞死锁 / 写冲突时的整体重试次数(§4.37:attempts=2)。
const txRetryAttempts = 2

// 毒行推迟时长**刻意不在这里**:它是 assetop.LoopConfig.PoisonDelay(由 trade.yaml 的
// PoisonDelayMs 喂进来),循环算好绝对时刻经 Claim 的 poisonUntilMs 传进来。
// 这里再写一份 const 就是第二份真相 —— 改 yaml 不生效,且不会有任何报错。

// IsRetryableTxError 报告事务错误是否值得整体重试。shared/assetop 不引 MySQL 驱动,
// 分类由调用方注入(§4.37);取值与 guild 一致:
//
//	1213 死锁 / 1205 锁等待超时 / 9007 TiDB 写冲突。
func IsRetryableTxError(err error) bool {
	var myErr *mysql.MySQLError
	if !errors.As(err, &myErr) {
		return false
	}
	switch myErr.Number {
	case 1213, 1205, 9007:
		return true
	default:
		return false
	}
}
