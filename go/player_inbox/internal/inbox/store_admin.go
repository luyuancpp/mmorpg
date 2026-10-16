package inbox

import (
	"context"
	"database/sql"
	"fmt"

	"player_inbox/internal/constants"
	"player_inbox/internal/metrics"

	inboxpb "proto/player_inbox"

	"shared/assetop"

	"github.com/zeromicro/go-zero/core/logx"
)

// 运维接口的存储操作(设计 §5.2、§7.9):作废、按玩家概况、列条目、按幂等键查。

// VoidParams 是一次运维作废的输入。
type VoidParams struct {
	PlayerID uint64
	Domain   inboxpb.PlayerInboxDomain
	Seq      uint64
	Reason   inboxpb.PlayerInboxVoidReason
	Operator string
	Note     string
	PeerAddr string
}

// Void 作废一条条目(设计 §7.9、IB-5):
//   - 从未投递(delivery_count=0):在流行锁下直接改 VOIDED(finalized_by=ADMIN),open_count−1;
//   - 已投递:只挂 directive=VOID,由消费方裁决(还没处理过 → 记 SKIPPED 再确认;已处理过 → 确认原结局);
//   - 已挂过 VOID 指令:幂等成功,不改行;
//   - 已是终态 → kPlayerInboxItemStateConflict;不存在 → kPlayerInboxItemNotFound。
//
// 改动行的分支与审计行(action=VOID,INSERT IGNORE)同库同事务。
func (s *Store) Void(ctx context.Context, p VoidParams) (*inboxpb.VoidPlayerInboxItemResponse, error) {
	resp := &inboxpb.VoidPlayerInboxItemResponse{}
	if _, ok := s.registry.Domain(p.Domain); !ok || p.PlayerID == 0 || p.Seq == 0 ||
		p.Reason == inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_UNSPECIFIED ||
		inboxpb.PlayerInboxVoidReason_name[int32(p.Reason)] == "" {
		resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
		return resp, nil
	}
	if code := ValidateText(p.Operator, constants.MaxOperatorBytes, true); code != 0 {
		resp.ErrorMessage = tipOf(code)
		return resp, nil
	}
	if code := ValidateText(p.Note, constants.MaxReasonBytes, false); code != 0 {
		resp.ErrorMessage = tipOf(code)
		return resp, nil
	}

	shard := RowShard(p.PlayerID)
	var (
		action voidAction
		state  inboxpb.PlayerInboxItemState
	)
	err := assetop.WithTxRetryConfig(ctx, s.db, s.txCfg, func(tx *sql.Tx) error {
		action, state = 0, inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_UNSPECIFIED
		st, err := lockStream(ctx, tx, shard, p.PlayerID, p.Domain)
		if err != nil {
			return err
		}
		if st == nil {
			action = voidNotFound
			return nil
		}
		row, err := readItem(ctx, tx, shard, p.PlayerID, p.Domain, p.Seq)
		if err != nil {
			return err
		}
		action = decideVoid(row)
		nowMs := s.nowMs()
		switch action {
		case voidNotFound:
			return nil
		case voidTerminal:
			state = inboxpb.PlayerInboxItemState(row.GetState())
			return nil
		case voidDirect:
			upd, err := tx.ExecContext(ctx, "UPDATE "+itemTable+
				" SET `state` = ?, `finalized_by` = ?, `void_reason` = ?, `updated_ms` = ?"+
				" WHERE "+itemPK+" AND `state` = ? AND `delivery_count` = 0",
				uint32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_VOIDED),
				uint32(inboxpb.PlayerInboxFinalizer_PLAYER_INBOX_FINALIZER_ADMIN), uint32(p.Reason), nowMs,
				shard, p.PlayerID, uint32(p.Domain), p.Seq, uint32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING))
			if err != nil {
				return fmt.Errorf("inbox: void item player=%d seq=%d: %w", p.PlayerID, p.Seq, err)
			}
			if err := requireOneRow(upd, "void item"); err != nil {
				return err
			}
			openCount := st.GetOpenCount()
			if openCount == 0 {
				logx.WithContext(ctx).Errorf("[player_inbox] open_count already 0 while voiding a PENDING row player=%d domain=%d seq=%d",
					p.PlayerID, p.Domain, p.Seq)
				openCount = 1
			}
			upd, err = tx.ExecContext(ctx, "UPDATE "+streamTable+" SET `open_count` = ?, `updated_ms` = ? WHERE "+streamPK,
				openCount-1, nowMs, shard, p.PlayerID, uint32(p.Domain))
			if err != nil {
				return fmt.Errorf("inbox: update stream after void player=%d: %w", p.PlayerID, err)
			}
			if err := requireOneRow(upd, "update stream after void"); err != nil {
				return err
			}
			state = inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_VOIDED
		case voidDirective:
			upd, err := tx.ExecContext(ctx, "UPDATE "+itemTable+
				" SET `directive` = ?, `void_reason` = ?, `updated_ms` = ? WHERE "+itemPK+" AND `state` = ?",
				uint32(inboxpb.PlayerInboxDirective_PLAYER_INBOX_DIRECTIVE_VOID), uint32(p.Reason), nowMs,
				shard, p.PlayerID, uint32(p.Domain), p.Seq, uint32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING))
			if err != nil {
				return fmt.Errorf("inbox: attach void directive player=%d seq=%d: %w", p.PlayerID, p.Seq, err)
			}
			if err := requireOneRow(upd, "attach void directive"); err != nil {
				return err
			}
			state = inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING
		case voidAlreadyRequested:
			state = inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING
		}
		return InsertAudit(ctx, tx, &inboxpb.PlayerInboxAudit{
			PlayerId:  p.PlayerID,
			Domain:    uint32(p.Domain),
			Seq:       p.Seq,
			Action:    uint32(inboxpb.PlayerInboxAuditAction_PLAYER_INBOX_AUDIT_VOID),
			Operator:  p.Operator,
			PeerAddr:  p.PeerAddr,
			Reason:    p.Note,
			CreatedMs: nowMs,
		})
	})
	if err != nil {
		return nil, err
	}

	dl, rl := DomainLabel(p.Domain), VoidReasonLabel(p.Reason)
	switch action {
	case voidNotFound:
		resp.ErrorMessage = tipOf(constants.ErrItemNotFound)
	case voidTerminal:
		resp.ErrorMessage = tipOf(constants.ErrItemStateConflict)
		resp.State = state
	case voidDirect:
		resp.State = state
		metrics.ObserveVoid(dl, rl, "direct")
	case voidDirective:
		resp.State, resp.VoidRequested = state, true
		metrics.ObserveVoid(dl, rl, "directive")
	case voidAlreadyRequested:
		resp.State, resp.VoidRequested = state, true
		metrics.ObserveVoid(dl, rl, "already")
	}
	if resp.ErrorMessage == nil {
		logx.WithContext(ctx).Infof("[player_inbox] VoidItem player=%d domain=%d seq=%d reason=%s state=%d void_requested=%t operator=%q peer=%s",
			p.PlayerID, p.Domain, p.Seq, rl, resp.State, resp.VoidRequested, p.Operator, p.PeerAddr)
	}
	return resp, nil
}

// Summary 按玩家列出各域流的概况,另给 max_fence_epoch 供 epoch-raise 取最大值(设计 §7.8、C4)。
func (s *Store) Summary(ctx context.Context, playerID uint64) (*inboxpb.GetPlayerInboxSummaryResponse, error) {
	resp := &inboxpb.GetPlayerInboxSummaryResponse{PlayerId: playerID}
	if playerID == 0 {
		resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
		return resp, nil
	}
	shard := RowShard(playerID)
	rows, err := s.db.QueryContext(ctx, "SELECT "+streamColumns+" FROM "+streamTable+
		" WHERE `shard` = ? AND `player_id` = ? ORDER BY `domain` ASC", shard, playerID)
	if err != nil {
		return nil, fmt.Errorf("inbox: summary streams player=%d: %w", playerID, err)
	}
	var streams []*inboxpb.PlayerInboxStream
	for rows.Next() {
		st, err := scanStream(rows)
		if err != nil {
			rows.Close()
			return nil, err
		}
		streams = append(streams, st)
	}
	if err := rows.Close(); err != nil {
		return nil, err
	}

	type agg struct {
		deliveredUnacked uint32
		oldestCreatedMs  uint64
	}
	aggs := map[uint32]agg{}
	arows, err := s.db.QueryContext(ctx, "SELECT `domain`,"+
		" COALESCE(SUM(CASE WHEN `delivery_count` > 0 THEN 1 ELSE 0 END), 0), COALESCE(MIN(`created_ms`), 0)"+
		" FROM "+itemTable+" WHERE `shard` = ? AND `player_id` = ? AND `state` = ? GROUP BY `domain`",
		shard, playerID, uint32(inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_PENDING))
	if err != nil {
		return nil, fmt.Errorf("inbox: summary aggregates player=%d: %w", playerID, err)
	}
	for arows.Next() {
		var d uint32
		var a agg
		if err := arows.Scan(&d, &a.deliveredUnacked, &a.oldestCreatedMs); err != nil {
			arows.Close()
			return nil, err
		}
		aggs[d] = a
	}
	if err := arows.Close(); err != nil {
		return nil, err
	}

	for _, st := range streams {
		a := aggs[st.GetDomain()]
		resp.Streams = append(resp.Streams, &inboxpb.PlayerInboxStreamSummary{
			Domain:                 inboxpb.PlayerInboxDomain(st.GetDomain()),
			OpenCount:              st.GetOpenCount(),
			NextSeq:                st.GetNextSeq(),
			FenceEpoch:             st.GetFenceEpoch(),
			DeliveredUnackedCount:  a.deliveredUnacked,
			OldestPendingCreatedMs: a.oldestCreatedMs,
			NotifyDueMs:            st.GetNotifyDueMs(),
		})
		resp.MaxFenceEpoch = max(resp.MaxFenceEpoch, st.GetFenceEpoch())
	}
	return resp, nil
}

// List 按 player_id + domain(+ 可选 state)分页列条目,seq 升序,after_seq 翻页,limit ≤ 200。
func (s *Store) List(ctx context.Context, req *inboxpb.ListPlayerInboxItemsRequest) (*inboxpb.ListPlayerInboxItemsResponse, error) {
	resp := &inboxpb.ListPlayerInboxItemsResponse{}
	if _, ok := s.registry.Domain(req.GetDomain()); !ok || req.GetPlayerId() == 0 {
		resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
		return resp, nil
	}
	if st := req.GetState(); st != inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_UNSPECIFIED &&
		inboxpb.PlayerInboxItemState_name[int32(st)] == "" {
		resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
		return resp, nil
	}
	limit := clampFetchItems(req.GetLimit(), constants.ListDefaultLimit, constants.ListMaxLimit)
	query := "SELECT " + itemColumns + " FROM " + itemTable +
		" WHERE `shard` = ? AND `player_id` = ? AND `domain` = ? AND `seq` > ?"
	args := []any{RowShard(req.GetPlayerId()), req.GetPlayerId(), uint32(req.GetDomain()), req.GetAfterSeq()}
	if st := req.GetState(); st != inboxpb.PlayerInboxItemState_PLAYER_INBOX_ITEM_UNSPECIFIED {
		query += " AND `state` = ?"
		args = append(args, uint32(st))
	}
	query += " ORDER BY `seq` ASC LIMIT ?"
	args = append(args, limit+1)
	rows, err := queryItems(ctx, s.db, query, args...)
	if err != nil {
		return nil, fmt.Errorf("inbox: list items player=%d: %w", req.GetPlayerId(), err)
	}
	if len(rows) > limit {
		resp.HasMore = true
		rows = rows[:limit]
	}
	for _, row := range rows {
		resp.Items = append(resp.Items, ItemView(row))
	}
	return resp, nil
}

// Lookup 按幂等键 (player_id, producer_kind, request_id) 查条目与结局。
func (s *Store) Lookup(ctx context.Context, req *inboxpb.LookupPlayerInboxRequest) (*inboxpb.LookupPlayerInboxResponse, error) {
	resp := &inboxpb.LookupPlayerInboxResponse{}
	if req.GetPlayerId() == 0 || req.GetRequestId() == 0 || !validProducer(req.GetProducerKind()) {
		resp.ErrorMessage = tipOf(constants.ErrInvalidParameter)
		return resp, nil
	}
	row, err := readItemByRequest(ctx, s.db, RowShard(req.GetPlayerId()), req.GetPlayerId(), req.GetProducerKind(), req.GetRequestId())
	if err != nil {
		return nil, err
	}
	if row == nil {
		resp.ErrorMessage = tipOf(constants.ErrItemNotFound)
		return resp, nil
	}
	resp.Item = ItemView(row)
	return resp, nil
}

// ItemView 把一行折成运维视图(全部列;载荷原样返回,由运维工具按 msg_type 的 payload_message 解码)。
func ItemView(row *inboxpb.PlayerInboxItemRow) *inboxpb.PlayerInboxItemView {
	return &inboxpb.PlayerInboxItemView{
		Domain:             inboxpb.PlayerInboxDomain(row.GetDomain()),
		Seq:                row.GetSeq(),
		MsgType:            inboxpb.PlayerInboxMessageType(row.GetMsgType()),
		ProducerKind:       inboxpb.PlayerInboxProducerKind(row.GetProducerKind()),
		RequestId:          row.GetRequestId(),
		Payload:            row.GetPayload(),
		State:              inboxpb.PlayerInboxItemState(row.GetState()),
		Directive:          inboxpb.PlayerInboxDirective(row.GetDirective()),
		VoidReason:         inboxpb.PlayerInboxVoidReason(row.GetVoidReason()),
		DeliveryCount:      row.GetDeliveryCount(),
		LastDeliveredEpoch: row.GetLastDeliveredEpoch(),
		LastDeliveredMs:    row.GetLastDeliveredMs(),
		OutcomeReason:      row.GetOutcomeReason(),
		FinalizedBy:        inboxpb.PlayerInboxFinalizer(row.GetFinalizedBy()),
		DecidedEpoch:       row.GetDecidedEpoch(),
		IssuedAtMs:         row.GetIssuedAtMs(),
		CreatedMs:          row.GetCreatedMs(),
		UpdatedMs:          row.GetUpdatedMs(),
		OperatorName:       row.GetOperatorName(),
		Note:               row.GetNote(),
	}
}
