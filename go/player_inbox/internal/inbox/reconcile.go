package inbox

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"sync/atomic"

	"player_inbox/internal/constants"
	"player_inbox/internal/metrics"

	componentpb "proto/common/component"
	inboxpb "proto/player_inbox"

	"shared/safego"

	"github.com/zeromicro/go-zero/core/logx"
)

// 对账(设计 §9.3):收件箱的终结记录 vs 玩家**已落盘**的消费账本(player_database.inbox_ledger)。
//
// 账本经 data_service.GetPlayerInboxLedger 读(它读 scene 存盘写的那份 blob,不回退读 MySQL,
// 语义同 GetPlayerAssetOpLedger)。读不到存档(found=false)计为 unknown,不告警;读失败计 error;
// 账本结构不合法(reconcileLedgerUsable)计 invalid —— 结论是「无法核对」,不是「无差异」。
// 两个入口共用同一个判定谓词 finalizedLedgerGap(decide.go):比水位,也比最近结局环(评审 M6)。

// LedgerReader 读一名玩家已落盘的收件箱账本。found=false = zone Redis 里没有这名玩家的 blob;
// found=true 时账本非 nil(玩家从未消费过收件箱时是空消息)。实现必须可并发调用:定时对账以有界并发读账本。
type LedgerReader interface {
	ReadInboxLedger(ctx context.Context, playerID uint64) (ledger *componentpb.PlayerInboxLedgerComp, found bool, err error)
}

// DecidedThrough 返回账本里某个域的水位;账本里没有该域时为 0。按 domain 字段查找,不靠下标(AGENTS §11.6)。
func DecidedThrough(ledger *componentpb.PlayerInboxLedgerComp, domain inboxpb.PlayerInboxDomain) uint64 {
	for _, c := range ledger.GetCursors() {
		if c.GetDomain() == uint32(domain) {
			return c.GetDecidedThroughSeq()
		}
	}
	return 0
}

// ReconcilePlayer 是运维手工对账入口:按单个玩家比对收件箱与已落盘账本,报告两类差异
// (FINAL_NOT_IN_LEDGER / PENDING_BUT_DECIDED,见 reconcileDiff)。只读,不改任何状态。
// 账本结构不合法时返回 error(适配层折成 kPlayerInboxStoreUnavailable 并记 ERROR):那份账本当不了证据,
// 回一个空的 diffs 会被读成「无差异」。
func (s *Store) ReconcilePlayer(ctx context.Context, req *inboxpb.ReconcilePlayerInboxRequest, ledgers LedgerReader) (*inboxpb.ReconcilePlayerInboxResponse, error) {
	resp := &inboxpb.ReconcilePlayerInboxResponse{}
	domain := req.GetDomain()
	if req.GetPlayerId() == 0 {
		resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
		return resp, nil
	}
	if domain != inboxpb.PlayerInboxDomain_PLAYER_INBOX_DOMAIN_UNSPECIFIED {
		if _, ok := s.registry.Domain(domain); !ok {
			resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
			return resp, nil
		}
	}
	if ledgers == nil {
		return nil, fmt.Errorf("inbox: ReconcilePlayer has no ledger reader")
	}
	readCtx, cancel := context.WithTimeout(ctx, constants.LedgerReadTimeout)
	ledger, found, err := ledgers.ReadInboxLedger(readCtx, req.GetPlayerId())
	cancel()
	if err != nil {
		return nil, fmt.Errorf("inbox: read persisted ledger player=%d: %w", req.GetPlayerId(), err)
	}
	resp.LedgerFound = found
	if !found {
		return resp, nil // unknown:读不到存档,不比对
	}
	if !reconcileLedgerUsable(ledger) {
		return nil, fmt.Errorf("inbox: persisted inbox ledger of player=%d is structurally invalid "+
			"(duplicate/zero domain, ring not ascending or beyond the watermark, unrecordable outcome, zero request_id, "+
			"or more than %d ring entries); reconcile result is unknown", req.GetPlayerId(), ledgerRecentCapacity)
	}

	rows, truncated, err := s.playerItems(ctx, req.GetPlayerId(), domain)
	if err != nil {
		return nil, err
	}
	if truncated {
		logx.WithContext(ctx).Errorf("[player_inbox] ReconcilePlayer read only the first %d rows of player=%d; result is partial",
			playerItemsLimit, req.GetPlayerId())
	}

	shard := RowShard(req.GetPlayerId())
	domains := []inboxpb.PlayerInboxDomain{domain}
	if domain == inboxpb.PlayerInboxDomain_PLAYER_INBOX_DOMAIN_UNSPECIFIED {
		domains = s.registry.Domains()
	}
	streams := make(map[inboxpb.PlayerInboxDomain]*inboxpb.PlayerInboxStream, len(domains))
	for _, d := range domains {
		st, err := readStream(ctx, s.db, shard, req.GetPlayerId(), d)
		if err != nil {
			return nil, err
		}
		if st != nil {
			streams[d] = st
		}
	}
	resp.Cursors, resp.Diffs = reconcileReport(domains, streams, ledger, rows)
	return resp, nil
}

// 定时对账一轮的工作量上限(GM 域量很小,设计 §9.3;评审 M5)。
const (
	// reconcilePageSize:单片一次读多少行(键集翻页,见 RecentlyFinalized)。
	reconcilePageSize = 100
	// reconcilePerShardLimit:单片一轮最多核对多少行。读满仍有剩余 = 本片截断(先核对最新的,截掉最老的),
	// 继续下一片 —— 一个热点分片不该吃光整轮预算。
	reconcilePerShardLimit = 500
	// reconcileMaxPlayers:一轮最多读多少名玩家的账本(每名一次 data_service 调用)。数到上限 = 本轮截断,
	// 停在当前分片;下一轮的起始分片按墙钟轮转(reconcileStartShard),高号分片不会永远轮不到。
	reconcileMaxPlayers = 2000
	// reconcileLedgerReadConcurrency:账本读的并发路数。串行读 2000 名玩家(每次上限 LedgerReadTimeout = 2s)
	// 会撞穿 2 分钟的轮预算;8 路时 2000 次 × 典型 10ms ≈ 2.5s,同时给 data_service 的压力有上界。
	reconcileLedgerReadConcurrency = 8
)

// reconcileLimits 是一轮定时对账的工作量上限;生产取 defaultReconcileLimits,单测注入小值。
type reconcileLimits struct {
	pageSize        int
	perShard        int
	maxPlayers      int
	readConcurrency int
}

var defaultReconcileLimits = reconcileLimits{
	pageSize:        reconcilePageSize,
	perShard:        reconcilePerShardLimit,
	maxPlayers:      reconcileMaxPlayers,
	readConcurrency: reconcileLedgerReadConcurrency,
}

// ReconcileRoundResult 是一轮定时对账的结果,供后台循环记 gauge 与轮次计数、打汇总日志。
type ReconcileRoundResult struct {
	// Inconsistent 是本轮发现的不一致行数。它是「当前」读数:同一处不一致只要还在窗口里,每轮都会再报一次,
	// 所以只能进 gauge,不能累加进计数器(评审 L11)。
	Inconsistent int
	// PlayersPlanned 是本轮收集到、要读账本的玩家数;PlayersChecked 是实际判定完的(含读失败 / 读不到 / 账本不合法)。
	PlayersPlanned int
	PlayersChecked int
	// Truncated 为真:本轮没有核对完窗口内的全部行(单片读满上限,或玩家数到上限)。
	Truncated bool
}

// ReconcileRecent 是定时对账作业的一轮(只在领导者上跑,设计 §9.3):
//
//  1. 从 startShard 起环形扫描 64 个分片(下一轮的起点由调用方按墙钟轮转),逐片按键集翻页读 sinceMs 之后
//     由消费方终结的行,直到扫满 64 片或触到上限。触到上限计 player_inbox_reconcile_truncated_total 并打 ERROR,
//     写明停在哪片、剩几片没扫到。
//  2. 按玩家以有界并发(reconcileLedgerReadConcurrency 路)读已落盘账本。
//  3. 逐个玩家按 ledgerMismatches 判定(与 ReconcilePlayer 同一个谓词,比水位也比环),每处不一致写一行
//     带 player_id 的 ERROR(player_id 只进日志)。
//
// 返回本轮结果;ctx 结束(超时 / 易主 / 关停)时返回已判定部分与 ctx.Err()。不改任何状态。
func (s *Store) ReconcileRecent(ctx context.Context, sinceMs uint64, startShard uint32, ledgers LedgerReader) (ReconcileRoundResult, error) {
	if ledgers == nil {
		return ReconcileRoundResult{}, fmt.Errorf("inbox: ReconcileRecent has no ledger reader")
	}
	read := func(ctx context.Context, shard uint32, before *FinalizedRef, limit int) ([]FinalizedRef, error) {
		return s.RecentlyFinalized(ctx, shard, sinceMs, before, limit)
	}
	return reconcileRecent(ctx, startShard, defaultReconcileLimits, read, ledgers)
}

// reconcileRecent 是 ReconcileRecent 去掉库之后的部分:分片读取经 read 注入,单测用内存假实现。
func reconcileRecent(ctx context.Context, startShard uint32, limits reconcileLimits, read finalizedPageReader,
	ledgers LedgerReader) (ReconcileRoundResult, error) {
	log := logx.WithContext(ctx)
	startShard %= ShardCount
	scan, err := collectFinalized(ctx, startShard, limits, read)
	res := ReconcileRoundResult{PlayersPlanned: len(scan.players)}
	if err != nil {
		return res, err
	}
	if scan.truncated() {
		res.Truncated = true
		metrics.IncReconcileTruncated()
		log.Errorf("[player_inbox] Reconcile round truncated (start_shard=%d, scanned %d/%d shards): %s; "+
			"rows not checked this round are retried by later rounds (start shard rotates each interval); "+
			"if this persists, shorten ReconcileWindowHours or raise the limits", startShard, scan.scanned, ShardCount,
			scan.truncationSummary(limits))
	}

	reads := readLedgersBounded(ctx, ledgers, scan.players, limits.readConcurrency)
	for i, playerID := range scan.players {
		r := reads[i]
		if ctx.Err() != nil && (!r.done || r.err != nil) {
			continue // 轮次已结束(超时 / 易主 / 关停):没读或读到一半被打断的不算读失败,下一轮再核对
		}
		res.PlayersChecked++
		switch {
		case !r.done:
			metrics.ObserveReconcileRead("error")
			log.Errorf("[player_inbox] Reconcile ledger read did not complete player=%d (reader panicked, see %s)",
				playerID, safego.EventGoroutinePanic)
		case r.err != nil:
			metrics.ObserveReconcileRead("error")
			log.Errorf("[player_inbox] Reconcile could not read persisted ledger player=%d: %v", playerID, r.err)
		case !r.found:
			metrics.ObserveReconcileRead("absent")
		case !reconcileLedgerUsable(r.ledger):
			metrics.ObserveReconcileRead("invalid")
			log.Errorf("[player_inbox] Reconcile skipped player=%d: persisted inbox ledger is structurally invalid "+
				"(duplicate/zero domain, ring not ascending or beyond the watermark, unrecordable outcome, zero request_id, "+
				"or more than %d ring entries); result is unknown, not 'no difference'", playerID, ledgerRecentCapacity)
		default:
			metrics.ObserveReconcileRead("found")
			for _, m := range ledgerMismatches(scan.byPlayer[playerID], r.ledger) {
				res.Inconsistent++
				cursor := findCursor(r.ledger, uint32(m.ref.Domain))
				log.Errorf("[player_inbox] Reconcile mismatch player=%d domain=%d seq=%d state=%d request_id=%d gap=%s "+
					"ledger_decided_through=%d ring_len=%d: inbox finalized the item but the persisted ledger does not hold "+
					"its outcome (rollback or Redis data loss); manual re-issue may be needed",
					playerID, m.ref.Domain, m.ref.Seq, m.ref.State, m.ref.RequestID, m.gap,
					cursor.GetDecidedThroughSeq(), len(cursor.GetRecent()))
			}
		}
	}
	return res, ctx.Err()
}

// finalizedPageReader 读一个分片里排在 before 之后(更旧)的一页消费方终结行;before 为 nil 从最新的一行读起。
// 生产为 Store.RecentlyFinalized(窗口起点由闭包带入)。
type finalizedPageReader func(ctx context.Context, shard uint32, before *FinalizedRef, limit int) ([]FinalizedRef, error)

// finalizedScan 是一轮对账收集到的待核对行与分片覆盖情况。
type finalizedScan struct {
	byPlayer map[uint64][]FinalizedRef
	// players 是玩家首次出现的顺序:读账本、判定与日志都按它走,结果确定(不随 map 遍历变)。
	players []uint64
	// scanned 是读过的分片数(含玩家数到上限时只读了一部分的那一片)。
	scanned int
	// capped 是读满单片上限、仍有剩余的分片,按扫描顺序。
	capped []uint32
	// playerCapHit 为真:玩家数到上限,停在 stoppedShard(该片只读了一部分)。
	playerCapHit bool
	stoppedShard uint32
}

func (s finalizedScan) truncated() bool { return len(s.capped) > 0 || s.playerCapHit }

// unreached 是本轮一行都没读到的分片数。
func (s finalizedScan) unreached() int { return ShardCount - s.scanned }

// truncationSummary 是截断日志里「停在哪、剩多少」的那一段。
func (s finalizedScan) truncationSummary(limits reconcileLimits) string {
	var parts []string
	if len(s.capped) > 0 {
		parts = append(parts, fmt.Sprintf("%d shard(s) %v hit the per-shard limit of %d rows (newest checked, oldest skipped)",
			len(s.capped), s.capped, limits.perShard))
	}
	if s.playerCapHit {
		parts = append(parts, fmt.Sprintf("stopped at shard=%d after reaching %d players, %d shard(s) not reached",
			s.stoppedShard, limits.maxPlayers, s.unreached()))
	}
	return strings.Join(parts, "; ")
}

// collectFinalized 从 startShard 起环形扫描全部分片((startShard+i) % 64),收集待核对的行(评审 M5):
//   - 每片按键集翻页,一页至多 limits.pageSize 行;一片至多 limits.perShard 行,读满后探一行确认还有剩余才算截断
//     (恰好读满不算),然后继续下一片;
//   - 新出现的玩家超过 limits.maxPlayers 时整轮停在当前分片(已收进来的玩家照常核对)。
//
// 读失败或 ctx 结束返回 error,此时收集结果不完整,调用方不得拿去判定「无差异」。
func collectFinalized(ctx context.Context, startShard uint32, limits reconcileLimits, read finalizedPageReader) (finalizedScan, error) {
	scan := finalizedScan{byPlayer: map[uint64][]FinalizedRef{}}
	for i := uint32(0); i < ShardCount; i++ {
		if err := ctx.Err(); err != nil {
			return scan, err
		}
		shard := (startShard + i) % ShardCount
		scan.scanned++
		capHit, err := scan.collectShard(ctx, shard, limits, read)
		if err != nil {
			return scan, err
		}
		if capHit {
			scan.playerCapHit, scan.stoppedShard = true, shard
			break
		}
	}
	return scan, nil
}

// collectShard 读一个分片;返回 true = 玩家数到上限,整轮该停了。
func (s *finalizedScan) collectShard(ctx context.Context, shard uint32, limits reconcileLimits, read finalizedPageReader) (bool, error) {
	var before *FinalizedRef
	taken := 0
	for {
		limit := min(limits.pageSize, limits.perShard-taken)
		if limit <= 0 {
			more, err := read(ctx, shard, before, 1)
			if err != nil {
				return false, err
			}
			if len(more) > 0 {
				s.capped = append(s.capped, shard)
			}
			return false, nil
		}
		page, err := read(ctx, shard, before, limit)
		if err != nil {
			return false, err
		}
		for _, f := range page {
			if _, seen := s.byPlayer[f.PlayerID]; !seen {
				if len(s.players) >= limits.maxPlayers {
					return true, nil
				}
				s.players = append(s.players, f.PlayerID)
			}
			s.byPlayer[f.PlayerID] = append(s.byPlayer[f.PlayerID], f)
			taken++
		}
		if len(page) < limit {
			return false, nil // 本片读完了
		}
		last := page[len(page)-1]
		before = &last
	}
}

// ledgerRead 是一名玩家的账本读结果。done=false:没有读完(轮次已结束、没有发起,或读的 goroutine panic 了)。
type ledgerRead struct {
	ledger *componentpb.PlayerInboxLedgerComp
	found  bool
	err    error
	done   bool
}

// readLedgersBounded 以至多 concurrency 路并发读 players 的已落盘账本,结果按 players 的下标放回。
// 每路用原子计数领下标、只写自己领到的那一格,不共享别的可变状态;wg.Wait 之后调用方才读结果。
// ctx 结束后不再发起新的读;每次读各带 LedgerReadTimeout。单次读 panic 由 safego.Run 兜住(记 goroutine_panic),
// 只丢那一名玩家(下标留 done=false),这一路接着领下一个。
func readLedgersBounded(ctx context.Context, ledgers LedgerReader, players []uint64, concurrency int) []ledgerRead {
	const point = "player_inbox.reconcile_ledger_read"
	out := make([]ledgerRead, len(players))
	readOne := func(i int) {
		readCtx, cancel := context.WithTimeout(ctx, constants.LedgerReadTimeout)
		defer cancel()
		ledger, found, err := ledgers.ReadInboxLedger(readCtx, players[i])
		out[i] = ledgerRead{ledger: ledger, found: found, err: err, done: true}
	}
	var next atomic.Int64
	var wg sync.WaitGroup
	for w := 0; w < min(max(concurrency, 1), len(players)); w++ {
		wg.Add(1)
		safego.Go(point, func() {
			defer wg.Done()
			for {
				i := int(next.Add(1) - 1)
				if i >= len(players) || ctx.Err() != nil {
					return
				}
				safego.Run(point, func() { readOne(i) })
			}
		})
	}
	wg.Wait()
	return out
}
