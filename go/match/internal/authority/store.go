package authority

import (
	"bytes"
	"context"
	"database/sql"
	"encoding/binary"
	"errors"
	"fmt"
	"os"
	"sort"
	"strings"
	"time"

	"github.com/google/uuid"
	"google.golang.org/protobuf/proto"
	"match/internal/config"
	pb "proto/match"
	"schemamigrate"
	"shared/roleidentity"
)

type Store struct {
	roleSchema       string
	entryGuildSchema string
	DB               *sql.DB
	Secret           []byte
	Config           config.GlobalMatchConf
	TeamConfig       config.TeamConf // 启动时由同一SQL策略指纹准入，运行中不热改。
	roleGuard        func(context.Context, *sql.Tx, []*pb.MatchAuthorityTicketRecord) error
	entryGuard       func(context.Context, *sql.Tx, []*pb.MatchAuthorityTicketRecord, *matchEntryOriginal) error
}

const ticketColumns = "player_id,ticket,request_id,mode,config_id,team_size,state,enqueued_ms,not_before_ms,zone_id,rating_centi,formation_id,team_id,network_profile,expires_ms,selected_region,preferred_region,owner_account,owner_revision"
const formationColumns = "battle_id,operation_id,state,physical_region,target_node_id,target_instance_id,target_endpoint,mode,config_id,members,create_request,created_ms,deadline_ms,retry_at_ms,activity_context,requeue_on_failure,prepared"

var ErrJoinCancelled = errors.New("匹配请求已经撤销")

func Open(ctx context.Context, c config.GlobalMatchConf) (*Store, error) {
	secret := []byte(os.Getenv(c.ProbeSecretEnv))
	if len(secret) < 32 {
		return nil, errors.New("全球匹配探测密钥至少32字节，必须从环境注入")
	}
	if c.CrossRegionWaitSeconds == 0 || c.HardLatencyMs < 20 || c.HardLatencyMs > 1000 || c.ProbeTTLSeconds < 30 || c.ProbeTTLSeconds > 600 {
		return nil, errors.New("全球匹配数据库或地域配置无效")
	}
	db, err := openAuthorityDatabase(ctx, c)
	if err != nil {
		return nil, err
	}
	ok := false
	defer func() {
		if !ok {
			db.Close()
		}
	}()
	if err = requireCurrentSchema(ctx, db, c.Database, schemamigrate.Plan); err != nil {
		return nil, err
	}
	store := &Store{DB: db, Secret: secret, Config: c}
	if err = store.CheckCandidateIndexes(ctx); err != nil {
		return nil, err
	}
	ok = true
	return store, nil
}

type scanner interface{ Scan(...any) error }

func scanTicket(s scanner) (*pb.MatchAuthorityTicketRecord, error) {
	t := &pb.MatchAuthorityTicketRecord{}
	err := s.Scan(&t.PlayerId, &t.Ticket, &t.RequestId, &t.Mode, &t.ConfigId, &t.TeamSize, &t.State, &t.EnqueuedMs, &t.NotBeforeMs, &t.ZoneId, &t.RatingCenti, &t.FormationId, &t.TeamId, &t.NetworkProfile, &t.ExpiresMs, &t.SelectedRegion, &t.PreferredRegion, &t.OwnerAccount, &t.OwnerRevision)
	return t, err
}
func (s *Store) Ticket(ctx context.Context, pid uint64) (*pb.MatchAuthorityTicketRecord, error) {
	if _, present := roleidentity.Original(ctx); present {
		return s.ticketForActor(ctx, pid)
	}
	if _, e := s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET state=4 WHERE player_id=? AND state=1 AND expires_ms<=CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED)", pid); e != nil {
		return nil, e
	}
	t, e := scanTicket(s.DB.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=?", pid))
	if errors.Is(e, sql.ErrNoRows) {
		return nil, nil
	}
	return t, e
}
func now(ctx context.Context, q interface {
	QueryRowContext(context.Context, string, ...any) *sql.Row
}) (uint64, error) {
	var n uint64
	err := q.QueryRowContext(ctx, "SELECT CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED)").Scan(&n)
	return n, err
}
func (s *Store) Now(ctx context.Context) (uint64, error) { return now(ctx, s.DB) }

func (s *Store) SavePlan(ctx context.Context, pid uint64, p *pb.MatchNetworkProfile, expires uint64) error {
	if p == nil {
		return ErrOriginalPlayer
	}
	actor := originalPlayer(ctx, pid)
	p = proto.Clone(p).(*pb.MatchNetworkProfile)
	p.OwnerAccount, p.OwnerRevision = actor.OwnerAccount, actor.OwnerRevision
	b, e := proto.Marshal(p)
	if e != nil {
		return e
	}
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return e
	}
	defer tx.Rollback()
	if e = s.lockActor(ctx, tx, pid); e != nil {
		return e
	}
	if _, e = tx.ExecContext(ctx, "INSERT INTO match_network_profile(player_id,payload,expires_ms) VALUES(?,?,?) ON DUPLICATE KEY UPDATE payload=VALUES(payload),expires_ms=VALUES(expires_ms)", pid, b, expires); e != nil {
		return e
	}
	return tx.Commit()
}
func (s *Store) Profile(ctx context.Context, pid uint64) (*pb.MatchNetworkProfile, uint64, error) {
	var b []byte
	var expires uint64
	e := s.DB.QueryRowContext(ctx, "SELECT payload,expires_ms FROM match_network_profile WHERE player_id=?", pid).Scan(&b, &expires)
	if e != nil {
		return nil, 0, e
	}
	p, e := DecodeProfile(b)
	return p, expires, e
}
func (s *Store) CommitProfile(ctx context.Context, pid uint64, plan, binding string, receipts []*pb.NetworkProbeReceipt, expires uint64) error {
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return e
	}
	defer tx.Rollback()
	if e = s.lockActor(ctx, tx, pid); e != nil {
		return e
	}
	var b []byte
	var oldExpiry uint64
	if e = tx.QueryRowContext(ctx, "SELECT payload,expires_ms FROM match_network_profile WHERE player_id=? FOR UPDATE", pid).Scan(&b, &oldExpiry); e != nil {
		return e
	}
	p, e := DecodeProfile(b)
	if e != nil {
		return e
	}
	actor := originalPlayer(ctx, pid)
	if p.OwnerAccount != actor.OwnerAccount || p.OwnerRevision != actor.OwnerRevision {
		return ErrOriginalPlayer
	}
	n, e := now(ctx, tx)
	if e != nil {
		return e
	}
	if p.PlanId != plan || p.SessionBinding != binding || oldExpiry <= n || expires <= n {
		return errors.New("地域计划已经失效")
	}
	p.Receipts = receipts
	b, e = proto.Marshal(p)
	if e != nil {
		return e
	}
	if _, e = tx.ExecContext(ctx, "UPDATE match_network_profile SET payload=?,expires_ms=? WHERE player_id=?", b, expires, pid); e != nil {
		return e
	}
	// 等待过程中刷新同一会话的实测数据，不修改票号或最初等待时刻。
	active, e := scanTicket(tx.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=? FOR UPDATE", pid))
	if e != nil && !errors.Is(e, sql.ErrNoRows) {
		return e
	}
	// 整队unit固定入队时全员探测原件；单人刷新不得绕过P全员/Team/launch锁顺序改单票。
	if e == nil && active.State == 1 && active.TeamId == 0 {
		oldProfile, decodeErr := DecodeProfile(active.NetworkProfile)
		if decodeErr != nil {
			return decodeErr
		}
		if oldProfile.SessionBinding == binding {
			if _, e = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET network_profile=?,expires_ms=? WHERE player_id=? AND state=1", b, expires, pid); e != nil {
				return e
			}
			active.NetworkProfile, active.ExpiresMs = b, expires
			if e = s.replaceTicketRegions(ctx, tx, active, n); e != nil {
				return e
			}
		}
	}
	return tx.Commit()
}

// Join 的锁和提交只在 SQL：同请求回放返回原票，不重置最初等待时间。
func (s *Store) Join(ctx context.Context, t *pb.MatchAuthorityTicketRecord, plan, binding string) (*pb.MatchAuthorityTicketRecord, bool, error) {
	if t == nil {
		return nil, false, ErrOriginalPlayer
	}
	actor := originalPlayer(ctx, t.PlayerId)
	t = proto.Clone(t).(*pb.MatchAuthorityTicketRecord)
	t.OwnerAccount, t.OwnerRevision = actor.OwnerAccount, actor.OwnerRevision
	high, low, e := joinRequestParts(t.RequestId)
	if e != nil {
		return nil, false, e
	}
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return nil, false, e
	}
	defer tx.Rollback()
	if e = s.lockActor(ctx, tx, t.PlayerId); e != nil {
		return nil, false, e
	}
	if _, e = tx.ExecContext(ctx, "INSERT IGNORE INTO match_authority_ticket("+ticketColumns+") VALUES(?,'','',0,0,0,0,0,0,0,0,0,0,?,0,0,0,'',0)", t.PlayerId, []byte{}); e != nil {
		return nil, false, e
	}
	old, e := scanTicket(tx.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=? FOR UPDATE", t.PlayerId))
	if e != nil {
		return nil, false, e
	}
	// Join 与 Cancel 永远先争同一玩家行锁。历史不能只放当前票据，否则换票后重放旧请求会复活。
	var original []byte
	var cancelled bool
	var intentOwner string
	var intentRevision uint64
	var exitPrepared bool
	var exitRaw []byte
	pending := false
	e = tx.QueryRowContext(ctx, "SELECT ticket_record,cancelled,owner_account,owner_revision,exit_prepared,spectator_exit FROM match_join_intent WHERE player_id=? AND request_high=? AND request_low=?", t.PlayerId, high, low).Scan(&original, &cancelled, &intentOwner, &intentRevision, &exitPrepared, &exitRaw)
	if e == nil {
		if intentOwner != t.OwnerAccount || intentRevision != t.OwnerRevision {
			return nil, false, ErrOriginalPlayer
		}
		if cancelled {
			return nil, false, ErrJoinCancelled
		}
		replayed := &pb.MatchAuthorityTicketRecord{}
		if e = proto.Unmarshal(original, replayed); e != nil || replayed.Ticket == "" {
			return nil, false, errors.New("匹配意图原票损坏")
		}
		if replayed.OwnerAccount != t.OwnerAccount || replayed.OwnerRevision != t.OwnerRevision {
			return nil, false, ErrOriginalPlayer
		}
		if replayed.Mode != t.Mode || replayed.ConfigId != t.ConfigId || replayed.TeamSize != t.TeamSize {
			return nil, false, errors.New("重复请求的匹配参数不同")
		}
		if replayed.State != 0 {
			if old.Ticket == replayed.Ticket {
				replayed = old
			} else {
				replayed.State = 4
			}
			return replayed, false, tx.Commit()
		}
		if !exitPrepared || old.State != 0 || old.Ticket != replayed.Ticket || old.RequestId != replayed.RequestId {
			return nil, false, ErrOriginalPlayer
		}
		if len(exitRaw) != 0 {
			exit := &pb.MatchSpectatorRecord{}
			if proto.Unmarshal(exitRaw, exit) != nil || exit.PlayerId != t.PlayerId {
				return nil, false, ErrOriginalPlayer
			}
			if e = s.RequireSpectatorExitsClosedTx(ctx, tx, []*pb.MatchSpectatorRecord{exit}); e != nil {
				return nil, false, e
			}
		}
		t, pending = replayed, true
	}
	if e != nil && !errors.Is(e, sql.ErrNoRows) {
		return nil, false, e
	}
	if e = s.requireMatchEntryTx(ctx, tx, []*pb.MatchAuthorityTicketRecord{t}, nil); e != nil {
		return nil, false, e
	}
	if e = s.RequireNoSpectators(ctx, tx, []uint64{t.PlayerId}); e != nil {
		return nil, false, e
	}
	currentTime, e := now(ctx, tx)
	if e != nil {
		return nil, false, e
	}
	if old.State == 1 && old.ExpiresMs <= currentTime {
		old.State = 4
		if _, e = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET state=4 WHERE player_id=?", t.PlayerId); e != nil {
			return nil, false, e
		}
	}
	if !pending && old.RequestId == t.RequestId && old.Ticket != "" {
		if old.OwnerAccount != t.OwnerAccount || old.OwnerRevision != t.OwnerRevision {
			return nil, false, ErrOriginalPlayer
		}
		if old.Mode != t.Mode || old.ConfigId != t.ConfigId || old.TeamSize != t.TeamSize {
			return nil, false, errors.New("重复请求的匹配参数不同")
		}
		if e = saveJoinIntent(ctx, tx, old); e != nil {
			return nil, false, e
		}
		return old, false, tx.Commit()
	}
	if !pending && old.State == 0 && TicketOccupied(old) {
		return nil, false, errors.New("玩家已有待提交匹配原意图")
	}
	if old.State > 0 && old.State < 4 {
		if old.OwnerAccount != t.OwnerAccount || old.OwnerRevision != t.OwnerRevision {
			return nil, false, ErrOriginalPlayer
		}
		return old, false, tx.Commit()
	}
	var b []byte
	var exp uint64
	if pending {
		b, exp = t.NetworkProfile, t.ExpiresMs
	} else if b, exp, e = s.joinProfileTx(ctx, tx, t, plan, binding); e != nil {
		return nil, false, e
	}
	n, e := now(ctx, tx)
	if e != nil {
		return nil, false, e
	}
	if pending {
		p, decodeErr := DecodeProfile(b)
		if decodeErr != nil {
			return nil, false, decodeErr
		}
		if p.PlanId != plan || p.SessionBinding != binding || exp <= n || len(Latencies(p, n, s.Config.HardLatencyMs)) == 0 || p.OwnerAccount != t.OwnerAccount || p.OwnerRevision != t.OwnerRevision {
			return nil, false, errors.New("原匹配探测已失效，需取消原请求")
		}
	}
	t.State = 1
	t.EnqueuedMs = n
	t.NetworkProfile = b
	t.ExpiresMs = exp
	_, e = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET ticket=?,request_id=?,mode=?,config_id=?,team_size=?,state=1,enqueued_ms=?,not_before_ms=0,zone_id=?,rating_centi=?,formation_id=0,team_id=?,network_profile=?,expires_ms=?,selected_region=0,owner_account=?,owner_revision=? WHERE player_id=?", t.Ticket, t.RequestId, t.Mode, t.ConfigId, t.TeamSize, t.EnqueuedMs, t.ZoneId, t.RatingCenti, t.TeamId, t.NetworkProfile, t.ExpiresMs, t.OwnerAccount, t.OwnerRevision, t.PlayerId)
	if e != nil {
		return nil, false, e
	}
	if e = s.replaceTicketRegions(ctx, tx, t, n); e != nil {
		return nil, false, e
	}
	if e = saveJoinIntent(ctx, tx, t); e != nil {
		return nil, false, e
	}
	return t, true, tx.Commit()
}

func (s *Store) Cancel(ctx context.Context, pid uint64, ticket string) error {
	return s.CancelIntent(ctx, pid, ticket, "")
}

func saveJoinIntent(ctx context.Context, tx *sql.Tx, t *pb.MatchAuthorityTicketRecord) error {
	high, low, e := joinRequestParts(t.RequestId)
	if e != nil {
		return e
	}
	b, e := proto.MarshalOptions{Deterministic: true}.Marshal(t)
	if e != nil {
		return e
	}
	_, e = tx.ExecContext(ctx, "INSERT INTO match_join_intent(player_id,request_id,ticket_record,cancelled,request_high,request_low,owner_account,owner_revision,state) VALUES(?,?,?,0,?,?,?,?,2) ON DUPLICATE KEY UPDATE ticket_record=VALUES(ticket_record),state=2", t.PlayerId, t.RequestId, b, high, low, t.OwnerAccount, t.OwnerRevision)
	return e
}

func joinRequestParts(request string) (uint64, uint64, error) {
	parsed, e := uuid.Parse(request)
	if e != nil || parsed == uuid.Nil || parsed.String() != request {
		return 0, 0, errors.New("匹配请求编号无效")
	}
	return binary.BigEndian.Uint64(parsed[:8]), binary.BigEndian.Uint64(parsed[8:]), nil
}

// CancelIntent 的提交是 UNKNOWN Join 可以安全放弃的唯一依据。墓碑不影响玩家的新 request_id。
func (s *Store) CancelIntent(ctx context.Context, pid uint64, ticket, request string) error {
	if request != "" {
		if _, _, e := joinRequestParts(request); e != nil {
			return e
		}
	} else if ticket == "" {
		return errors.New("撤销需要原请求编号或票据")
	}
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return e
	}
	defer tx.Rollback()
	if e = s.lockActor(ctx, tx, pid); e != nil {
		return e
	}
	if _, e = tx.ExecContext(ctx, "INSERT IGNORE INTO match_authority_ticket("+ticketColumns+") VALUES(?,'','',0,0,0,0,0,0,0,0,0,0,?,0,0,0,'',0)", pid, []byte{}); e != nil {
		return e
	}
	old, e := scanTicket(tx.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=? FOR UPDATE", pid))
	if e != nil {
		return e
	}
	if request == "" && old.Ticket == ticket {
		request = old.RequestId
	}
	actor := originalPlayer(ctx, pid)
	if old.Ticket != "" && ((request != "" && old.RequestId == request) || (ticket != "" && old.Ticket == ticket)) && (old.OwnerAccount != actor.OwnerAccount || old.OwnerRevision != actor.OwnerRevision) {
		return ErrOriginalPlayer
	}
	if request != "" {
		high, low, parseErr := joinRequestParts(request)
		if parseErr != nil {
			return parseErr
		}
		var owner string
		var revision uint64
		read := tx.QueryRowContext(ctx, "SELECT owner_account,owner_revision FROM match_join_intent WHERE player_id=? AND request_high=? AND request_low=? FOR UPDATE", pid, high, low).Scan(&owner, &revision)
		if read == nil && (owner != actor.OwnerAccount || revision != actor.OwnerRevision) {
			return ErrOriginalPlayer
		}
		if read != nil && !errors.Is(read, sql.ErrNoRows) {
			return read
		}
		if _, e = tx.ExecContext(ctx, "INSERT INTO match_join_intent(player_id,request_id,ticket_record,cancelled,request_high,request_low,owner_account,owner_revision,state) VALUES(?,?,?,1,?,?,?,?,3) ON DUPLICATE KEY UPDATE cancelled=1,state=3", pid, request, []byte{}, high, low, actor.OwnerAccount, actor.OwnerRevision); e != nil {
			return e
		}
	}
	if (old.State == 1 || (old.State == 0 && old.Ticket != "")) && ((request != "" && old.RequestId == request && (ticket == "" || old.Ticket == ticket)) || (request == "" && old.Ticket == ticket)) {
		if _, e = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET state=4 WHERE player_id=? AND state IN(0,1)", pid); e != nil {
			return e
		}
		if e = deleteTicketRegions(ctx, tx, pid); e != nil {
			return e
		}
	}
	return tx.Commit()
}

func (s *Store) RequestCancelled(ctx context.Context, pid uint64, request string) (bool, error) {
	if request == "" {
		return false, nil
	}
	high, low, e := joinRequestParts(request)
	if e != nil {
		return false, e
	}
	var cancelled bool
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return false, e
	}
	defer tx.Rollback()
	if e = s.lockActor(ctx, tx, pid); e != nil {
		return false, e
	}
	var owner string
	var revision uint64
	e = tx.QueryRowContext(ctx, "SELECT cancelled,owner_account,owner_revision FROM match_join_intent WHERE player_id=? AND request_high=? AND request_low=?", pid, high, low).Scan(&cancelled, &owner, &revision)
	if errors.Is(e, sql.ErrNoRows) {
		return false, nil
	}
	if e != nil {
		return false, e
	}
	actor := originalPlayer(ctx, pid)
	if owner != actor.OwnerAccount || revision != actor.OwnerRevision {
		return false, ErrOriginalPlayer
	}
	return cancelled, tx.Commit()
}
func (s *Store) Candidates(ctx context.Context) ([]*pb.MatchAuthorityTicketRecord, error) {
	return s.CandidatesAfter(ctx, 0, 0)
}
func (s *Store) CandidatesAfter(ctx context.Context, afterMs, afterPlayer uint64) ([]*pb.MatchAuthorityTicketRecord, error) {
	r, e := s.DB.QueryContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE state=1 AND expires_ms>CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED) AND (enqueued_ms>? OR (enqueued_ms=? AND player_id>?)) ORDER BY enqueued_ms,player_id LIMIT 32", afterMs, afterMs, afterPlayer)
	if e != nil {
		return nil, e
	}
	defer r.Close()
	var out []*pb.MatchAuthorityTicketRecord
	for r.Next() {
		t, e := scanTicket(r)
		if e != nil {
			return nil, e
		}
		out = append(out, t)
	}
	return out, r.Err()
}

func (s *Store) Claim(ctx context.Context, f *pb.MatchFormationRecord, group []*pb.MatchAuthorityTicketRecord) error {
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return e
	}
	defer tx.Rollback()
	if e = s.claimTx(ctx, tx, f, group, nil); e != nil {
		return e
	}
	return tx.Commit()
}

func (s *Store) claimTx(ctx context.Context, tx *sql.Tx, f *pb.MatchFormationRecord, group []*pb.MatchAuthorityTicketRecord, source *matchEntryOriginal) error {
	if e := s.lockOriginalPlayers(ctx, tx, group); e != nil {
		return e
	}
	if e := s.requireMatchEntryTx(ctx, tx, group, source); e != nil {
		return e
	}
	players := make([]uint64, 0, len(group))
	for _, member := range group {
		players = append(players, member.PlayerId)
	}
	if e := s.RequireNoSpectators(ctx, tx, players); e != nil {
		return e
	}
	locked := append([]*pb.MatchAuthorityTicketRecord(nil), group...)
	sort.Slice(locked, func(i, j int) bool { return locked[i].PlayerId < locked[j].PlayerId })
	actual := make(map[uint64]*pb.MatchAuthorityTicketRecord, len(locked))
	for _, want := range locked {
		t, e := scanTicket(tx.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=? FOR UPDATE", want.PlayerId))
		if e != nil {
			return e
		}
		actual[want.PlayerId] = t
	}
	n, e := now(ctx, tx)
	if e != nil {
		return e
	}
	allPreferred := true
	for _, want := range locked {
		t := actual[want.PlayerId]
		p, e := DecodeProfile(t.NetworkProfile)
		if e != nil {
			return e
		}
		rtt, ok := Latencies(p, n, s.Config.HardLatencyMs)[f.PhysicalRegion]
		// 候选扫描之后可能等待行锁；整份探测的有效期也必须在提交点重新检查。
		if t.Ticket != want.Ticket || t.State != 1 || t.NotBeforeMs > n || t.ExpiresMs <= n || !ok || rtt == 0 || !bytes.Equal(t.NetworkProfile, want.NetworkProfile) || t.OwnerAccount != want.OwnerAccount || t.OwnerRevision != want.OwnerRevision || p.OwnerAccount != t.OwnerAccount || p.OwnerRevision != t.OwnerRevision {
			return errors.New("成局成员代次或地域条件已经变化")
		}
		if PreferredRegion(p, n) != f.PhysicalRegion {
			allPreferred = false
		}
	}
	if f.RequeueOnFailure && !allPreferred {
		wait := uint64(s.Config.CrossRegionWaitSeconds) * 1000
		for _, member := range locked {
			if n < wait || actual[member.PlayerId].EnqueuedMs > n-wait {
				return errors.New("跨区须全员原等待达到阈值")
			}
		}
	}
	f.CreatedMs = n
	f.DeadlineMs = n + 300000
	f.State = 1
	f.RetryAtMs = n
	f.Members = nil
	for _, t := range group {
		f.Members = append(f.Members, &pb.MatchFormationMember{PlayerId: t.PlayerId, Ticket: t.Ticket})
	}
	// repeated 消息按 proto2mysql 的同一序列化格式写入，不制造平行成员模型。
	memberValue := &pb.MatchFormationRecord{Members: f.Members}
	mb, e := proto.Marshal(memberValue)
	if e != nil {
		return e
	}
	if f.ActivityContext == nil {
		f.ActivityContext = []byte{}
	}
	_, e = tx.ExecContext(ctx, "INSERT INTO match_formation("+formationColumns+") VALUES("+strings.TrimSuffix(strings.Repeat("?,", 17), ",")+")", f.BattleId, f.OperationId, 1, f.PhysicalRegion, f.TargetNodeId, f.TargetInstanceId, f.TargetEndpoint, f.Mode, f.ConfigId, mb, []byte{}, f.CreatedMs, f.DeadlineMs, f.RetryAtMs, f.ActivityContext, f.RequeueOnFailure, []byte{})
	if e != nil {
		return e
	}
	for _, t := range locked {
		if _, e = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET state=2,formation_id=?,selected_region=? WHERE player_id=? AND ticket=? AND state=1", f.BattleId, f.PhysicalRegion, t.PlayerId, t.Ticket); e != nil {
			return e
		}
		if e = deleteTicketRegions(ctx, tx, t.PlayerId); e != nil {
			return e
		}
	}
	return nil
}

// ReserveGroup 与成局在同一事务提交，预组队不会短暂暴露为可拆散的单人候选。
func (s *Store) ReserveGroup(ctx context.Context, f *pb.MatchFormationRecord, group []*pb.MatchAuthorityTicketRecord) error {
	if len(group) == 0 || len(group) > 10 {
		return errors.New("整组人数无效")
	}
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return e
	}
	defer tx.Rollback()
	if e = s.reserveGroupTx(ctx, tx, f, group, nil); e != nil {
		return e
	}
	return tx.Commit()
}

func (s *Store) reserveGroupTx(ctx context.Context, tx *sql.Tx, f *pb.MatchFormationRecord, group []*pb.MatchAuthorityTicketRecord, source *matchEntryOriginal) error {
	if e := s.lockOriginalPlayers(ctx, tx, group); e != nil {
		return e
	}
	if e := s.requireMatchEntryTx(ctx, tx, group, source); e != nil {
		return e
	}
	players := make([]uint64, 0, len(group))
	for _, member := range group {
		players = append(players, member.PlayerId)
	}
	if e := s.RequireNoSpectators(ctx, tx, players); e != nil {
		return e
	}
	n, e := now(ctx, tx)
	if e != nil {
		return e
	}
	ordered := append([]*pb.MatchAuthorityTicketRecord(nil), group...)
	sort.Slice(ordered, func(i, j int) bool { return ordered[i].PlayerId < ordered[j].PlayerId })
	for i, t := range ordered {
		if t.PlayerId == 0 || (i > 0 && ordered[i-1].PlayerId == t.PlayerId) {
			return errors.New("整组身份无效")
		}
		if _, e = tx.ExecContext(ctx, "INSERT IGNORE INTO match_authority_ticket("+ticketColumns+") VALUES(?,'','',0,0,0,0,0,0,0,0,0,0,?,0,0,0,'',0)", t.PlayerId, []byte{}); e != nil {
			return e
		}
		old, e := scanTicket(tx.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=? FOR UPDATE", t.PlayerId))
		if e != nil {
			return e
		}
		if TicketOccupied(old) && !(old.State == 0 && old.Ticket == t.Ticket && old.RequestId == t.RequestId && old.OwnerAccount == t.OwnerAccount && old.OwnerRevision == t.OwnerRevision) {
			return errors.New("整组成员已有在途票据")
		}
		var data []byte
		var exp uint64
		if e = tx.QueryRowContext(ctx, "SELECT payload,expires_ms FROM match_network_profile WHERE player_id=?", t.PlayerId).Scan(&data, &exp); e != nil {
			return e
		}
		p, e := DecodeProfile(data)
		if e != nil {
			return e
		}
		if p.OwnerAccount != t.OwnerAccount || p.OwnerRevision != t.OwnerRevision {
			return ErrOriginalPlayer
		}
		if exp <= n {
			return errors.New("整组成员的地域探测已过期")
		}
		if _, ok := Latencies(p, n, s.Config.HardLatencyMs)[f.PhysicalRegion]; !ok {
			return errors.New("整组没有符合硬延迟的共同机房")
		}
		t.NetworkProfile = data
		t.State = 1
		t.EnqueuedMs = n
		t.ExpiresMs = exp
		if _, e = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET ticket=?,request_id=?,mode=?,config_id=?,team_size=?,state=1,enqueued_ms=?,not_before_ms=0,zone_id=?,rating_centi=?,formation_id=0,team_id=?,network_profile=?,expires_ms=?,selected_region=0,owner_account=?,owner_revision=? WHERE player_id=?", t.Ticket, t.RequestId, t.Mode, t.ConfigId, t.TeamSize, n, t.ZoneId, t.RatingCenti, t.TeamId, data, t.ExpiresMs, t.OwnerAccount, t.OwnerRevision, t.PlayerId); e != nil {
			return e
		}
		if e = s.replaceTicketRegions(ctx, tx, t, n); e != nil {
			return e
		}
	}
	if e = s.claimTx(ctx, tx, f, group, source); e != nil {
		return e
	}
	return nil
}

// FindDirect 是同一业务意图的恢复入口。查询缺失只表示当前没有已提交成局，不能授权换请求号重试。
func (s *Store) FindDirect(ctx context.Context, key *pb.MatchDirectIntentRecord) (*pb.MatchFormationRecord, []byte, error) {
	if key == nil {
		return nil, nil, ErrDirectConflict
	}
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return nil, nil, e
	}
	defer tx.Rollback()
	actor := originalPlayer(ctx, key.OwnerId)
	if key.Kind != 2 {
		if e = s.lockActor(ctx, tx, key.OwnerId); e != nil {
			return nil, nil, e
		}
	}
	var id uint64
	var payload []byte
	var cancelled bool
	var owner string
	var revision uint64
	e = tx.QueryRowContext(ctx, "SELECT battle_id,payload,cancelled,owner_account,owner_revision FROM match_direct_intent WHERE kind=? AND owner_id=? AND request_high=? AND request_low=?", key.Kind, key.OwnerId, key.RequestHigh, key.RequestLow).Scan(&id, &payload, &cancelled, &owner, &revision)
	if errors.Is(e, sql.ErrNoRows) {
		return nil, nil, nil
	}
	if e != nil {
		return nil, nil, e
	}
	if key.Kind != 2 && (owner != actor.OwnerAccount || revision != actor.OwnerRevision) {
		return nil, nil, ErrOriginalPlayer
	}
	if e = tx.Commit(); e != nil {
		return nil, nil, e
	}
	if cancelled {
		return nil, payload, ErrDirectCancelled
	}
	if id == 0 {
		return nil, payload, nil
	}
	f, e := s.Formation(ctx, id)
	return f, payload, e
}

// 拒战和应战竞争相同意图主键；拒战墓碑没有成局，迟到接受不能越过它。
func (s *Store) DeclineDirect(ctx context.Context, key *pb.MatchDirectIntentRecord, payload []byte) (bool, error) {
	if key == nil || key.Kind != 3 || key.OwnerId == 0 || (key.RequestHigh == 0 && key.RequestLow == 0) || len(payload) == 0 {
		return false, errors.New("拒战原意图无效")
	}
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return false, e
	}
	defer tx.Rollback()
	if e = s.lockActor(ctx, tx, key.OwnerId); e != nil {
		return false, e
	}
	actor := originalPlayer(ctx, key.OwnerId)
	if _, e = tx.ExecContext(ctx, "INSERT IGNORE INTO match_direct_intent(kind,owner_id,request_high,request_low,payload,battle_id,cancelled,owner_account,owner_revision) VALUES(?,?,?,?,?,0,0,?,?)", key.Kind, key.OwnerId, key.RequestHigh, key.RequestLow, payload, actor.OwnerAccount, actor.OwnerRevision); e != nil {
		return false, e
	}
	var original []byte
	var battle uint64
	var owner string
	var revision uint64
	if e = tx.QueryRowContext(ctx, "SELECT payload,battle_id,owner_account,owner_revision FROM match_direct_intent WHERE kind=? AND owner_id=? AND request_high=? AND request_low=? FOR UPDATE", key.Kind, key.OwnerId, key.RequestHigh, key.RequestLow).Scan(&original, &battle, &owner, &revision); e != nil {
		return false, e
	}
	if owner != actor.OwnerAccount || revision != actor.OwnerRevision {
		return false, ErrOriginalPlayer
	}
	return battle == 0 && bytes.Equal(original, payload), tx.Commit()
}

func DirectUUIDKey(kind uint32, owner uint64, request string) (*pb.MatchDirectIntentRecord, error) {
	high, low, e := joinRequestParts(request)
	if e != nil {
		return nil, e
	}
	return &pb.MatchDirectIntentRecord{Kind: kind, OwnerId: owner, RequestHigh: high, RequestLow: low}, nil
}

// ReserveDirect 原业务意图、全成员占用和固定机房成局一次提交；超时重试返回原成局，不再发新 operation。
func (s *Store) ReserveDirect(ctx context.Context, key *pb.MatchDirectIntentRecord, payload []byte, f *pb.MatchFormationRecord, group []*pb.MatchAuthorityTicketRecord) (*pb.MatchFormationRecord, error) {
	original, err := s.PrepareDirectAdmission(ctx, key, payload, f, group)
	if err != nil {
		return nil, err
	}
	if original.BattleId != 0 {
		return s.Formation(ctx, original.BattleId)
	}
	return s.CompletePreparedDirect(ctx, original)
}
func scanFormation(s scanner) (*pb.MatchFormationRecord, error) {
	f := &pb.MatchFormationRecord{}
	var members, prepared []byte
	e := s.Scan(&f.BattleId, &f.OperationId, &f.State, &f.PhysicalRegion, &f.TargetNodeId, &f.TargetInstanceId, &f.TargetEndpoint, &f.Mode, &f.ConfigId, &members, &f.CreateRequest, &f.CreatedMs, &f.DeadlineMs, &f.RetryAtMs, &f.ActivityContext, &f.RequeueOnFailure, &prepared)
	if e != nil {
		return nil, e
	}
	m := &pb.MatchFormationRecord{}
	if e = proto.Unmarshal(members, m); e != nil {
		return nil, e
	}
	f.Members = m.Members
	if e = proto.Unmarshal(prepared, m); e != nil {
		return nil, e
	}
	f.Prepared = m.Prepared
	return f, nil
}

// retry_at_ms 同时是工作租约的代次和截止时间，只有当前代次才能提交状态。
// 租约不续期：所有备战/建房准入期限不超过本代次；失联后须到期才能恢复。
var ErrWorkLease = errors.New("成局工作租约已失效或被其他副本持有")

const WorkLeaseMs uint64 = 120000

func (s *Store) AcquireWork(ctx context.Context, id uint64) (*pb.MatchFormationRecord, error) {
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return nil, e
	}
	defer tx.Rollback()
	f, e := scanFormation(tx.QueryRowContext(ctx, "SELECT "+formationColumns+" FROM match_formation WHERE battle_id=? FOR UPDATE", id))
	if e != nil {
		return nil, e
	}
	n, e := now(ctx, tx)
	if e != nil {
		return nil, e
	}
	if f.State >= 4 || f.RetryAtMs > n {
		return nil, ErrWorkLease
	}
	if f.State == 1 && f.RetryAtMs != f.CreatedMs {
		f.State = 3
	}
	f.RetryAtMs = n + WorkLeaseMs
	if _, e = tx.ExecContext(ctx, "UPDATE match_formation SET retry_at_ms=?,state=? WHERE battle_id=?", f.RetryAtMs, f.State, id); e != nil {
		return nil, e
	}
	if e = tx.Commit(); e != nil {
		return nil, e
	}
	return f, nil
}
func checkWork(ctx context.Context, tx *sql.Tx, f, work *pb.MatchFormationRecord) error {
	n, e := now(ctx, tx)
	if e != nil {
		return e
	}
	if work == nil || work.BattleId != f.BattleId || work.RetryAtMs == 0 || f.RetryAtMs != work.RetryAtMs || f.RetryAtMs <= n || f.State >= 4 {
		return ErrWorkLease
	}
	return nil
}
func (s *Store) CheckWork(ctx context.Context, work *pb.MatchFormationRecord) error {
	if work == nil {
		return ErrWorkLease
	}
	var active bool
	e := s.DB.QueryRowContext(ctx, "SELECT state IN(1,2,3) AND retry_at_ms=? AND retry_at_ms>CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED) FROM match_formation WHERE battle_id=?", work.RetryAtMs, work.BattleId).Scan(&active)
	if e != nil {
		return e
	}
	if !active {
		return ErrWorkLease
	}
	return nil
}

// 先持久备战时精确端点，恢复取消不再重新读取可能已经迁移的玩家位置。
func (s *Store) Prepared(ctx context.Context, work *pb.MatchFormationRecord, pid uint64, endpoint string) error {
	id := work.GetBattleId()
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return e
	}
	defer tx.Rollback()
	f, e := scanFormation(tx.QueryRowContext(ctx, "SELECT "+formationColumns+" FROM match_formation WHERE battle_id=? FOR UPDATE", id))
	if e != nil {
		return e
	}
	if e = checkWork(ctx, tx, f, work); e != nil {
		return e
	}
	if f.State != 1 {
		return errors.New("备战所属成局已经进入恢复或终态")
	}
	for _, p := range f.Prepared {
		if p.PlayerId == pid {
			return nil
		}
	}
	f.Prepared = append(f.Prepared, &pb.MatchPreparedEndpoint{PlayerId: pid, Endpoint: endpoint})
	b, e := proto.Marshal(&pb.MatchFormationRecord{Prepared: f.Prepared})
	if e != nil {
		return e
	}
	if _, e = tx.ExecContext(ctx, "UPDATE match_formation SET prepared=? WHERE battle_id=? AND state=1", b, id); e != nil {
		return e
	}
	return tx.Commit()
}
func (s *Store) Formation(ctx context.Context, id uint64) (*pb.MatchFormationRecord, error) {
	return scanFormation(s.DB.QueryRowContext(ctx, "SELECT "+formationColumns+" FROM match_formation WHERE battle_id=?", id))
}
func (s *Store) Pending(ctx context.Context) ([]*pb.MatchFormationRecord, error) {
	r, e := s.DB.QueryContext(ctx, "SELECT "+formationColumns+" FROM match_formation WHERE state IN(1,2,3) AND retry_at_ms<=CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED) ORDER BY retry_at_ms LIMIT 32")
	if e != nil {
		return nil, e
	}
	defer r.Close()
	var out []*pb.MatchFormationRecord
	for r.Next() {
		f, e := scanFormation(r)
		if e != nil {
			return nil, e
		}
		out = append(out, f)
	}
	return out, r.Err()
}
func (s *Store) SaveIntent(ctx context.Context, work *pb.MatchFormationRecord, request []byte) ([]byte, error) {
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return nil, e
	}
	defer tx.Rollback()
	f, e := scanFormation(tx.QueryRowContext(ctx, "SELECT "+formationColumns+" FROM match_formation WHERE battle_id=? FOR UPDATE", work.GetBattleId()))
	if e != nil {
		return nil, e
	}
	if e = checkWork(ctx, tx, f, work); e != nil {
		return nil, e
	}
	if f.State == 1 {
		if _, e = tx.ExecContext(ctx, "UPDATE match_formation SET state=2,create_request=? WHERE battle_id=?", request, f.BattleId); e != nil {
			return nil, e
		}
		f.CreateRequest = request
	}
	if e = tx.Commit(); e != nil {
		return nil, e
	}
	return f.CreateRequest, nil
}
func (s *Store) Unknown(ctx context.Context, work *pb.MatchFormationRecord) error {
	if work == nil {
		return ErrWorkLease
	}
	r, e := s.DB.ExecContext(ctx, "UPDATE match_formation SET state=3 WHERE battle_id=? AND retry_at_ms=? AND retry_at_ms>CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED) AND state IN(1,2,3)", work.BattleId, work.RetryAtMs)
	if e != nil {
		return e
	}
	n, e := r.RowsAffected()
	if e == nil && n == 0 {
		return s.CheckWork(ctx, work) // 已是 UNKNOWN 的同代重放仍然幂等。
	}
	return e
}
func (s *Store) Finish(ctx context.Context, work *pb.MatchFormationRecord, success bool) error {
	id := work.GetBattleId()
	tx, e := s.DB.BeginTx(ctx, nil)
	if e != nil {
		return e
	}
	defer tx.Rollback()
	f, e := scanFormation(tx.QueryRowContext(ctx, "SELECT "+formationColumns+" FROM match_formation WHERE battle_id=? FOR UPDATE", id))
	if e != nil {
		return e
	}
	if e = checkWork(ctx, tx, f, work); e != nil {
		return e
	}
	n, e := now(ctx, tx)
	if e != nil {
		return e
	}
	state, ticketState := uint32(5), uint32(4)
	if success {
		state = 4
		ticketState = 3
	}
	if !success && f.RequeueOnFailure {
		ticketState = 1
	}
	if _, e = tx.ExecContext(ctx, "UPDATE match_formation SET state=? WHERE battle_id=?", state, id); e != nil {
		return e
	}
	for _, m := range f.Members {
		formationID, region := id, f.PhysicalRegion
		var notBefore uint64
		if ticketState == 1 {
			formationID = 0
			region = 0
			notBefore = n + 1000
		}
		if _, e = tx.ExecContext(ctx, "UPDATE match_authority_ticket SET state=?,formation_id=?,selected_region=?,not_before_ms=? WHERE player_id=? AND ticket=? AND formation_id=?", ticketState, formationID, region, notBefore, m.PlayerId, m.Ticket, id); e != nil {
			return e
		}
		current, readErr := scanTicket(tx.QueryRowContext(ctx, "SELECT "+ticketColumns+" FROM match_authority_ticket WHERE player_id=? FOR UPDATE", m.PlayerId))
		if readErr != nil {
			return readErr
		}
		if current.Ticket != m.Ticket {
			continue
		}
		if e = s.replaceTicketRegions(ctx, tx, current, n); e != nil {
			return e
		}
	}
	return tx.Commit()
}

func (s *Store) ReleaseReady(ctx context.Context, pid uint64, ticket string, id uint64) error {
	_, e := s.DB.ExecContext(ctx, "UPDATE match_authority_ticket SET state=4 WHERE player_id=? AND ticket=? AND formation_id=? AND state=3", pid, ticket, id)
	return e
}
