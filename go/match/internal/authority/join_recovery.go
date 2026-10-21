package authority

import (
	"context"
	"database/sql"
	"errors"

	"google.golang.org/protobuf/proto"
	pb "proto/match"
)

type JoinCursor struct{ Player, High, Low uint64 }

func (s *Store) PendingJoinHighWater(ctx context.Context) (JoinCursor, error) {
	var cursor JoinCursor
	err := s.DB.QueryRowContext(ctx, "SELECT player_id,request_high,request_low FROM match_join_intent WHERE state=1 ORDER BY player_id DESC,request_high DESC,request_low DESC LIMIT 1").Scan(&cursor.Player, &cursor.High, &cursor.Low)
	if errors.Is(err, sql.ErrNoRows) {
		return JoinCursor{}, nil
	}
	return cursor, err
}
func DecodePendingJoin(intent *pb.MatchJoinIntentRecord) (*pb.MatchAuthorityTicketRecord, *pb.JoinQueueRequest, *pb.MatchSpectatorRecord, error) {
	if intent == nil || intent.State != 1 || intent.Cancelled || !intent.ExitPrepared {
		return nil, nil, nil, ErrOriginalPlayer
	}
	ticket, request := &pb.MatchAuthorityTicketRecord{}, &pb.JoinQueueRequest{}
	if proto.Unmarshal(intent.TicketRecord, ticket) != nil || proto.Unmarshal(intent.RequestPayload, request) != nil || ticket.PlayerId != intent.PlayerId || ticket.State != 0 || ticket.Ticket == "" || ticket.RequestId != intent.RequestId || ticket.OwnerAccount != intent.OwnerAccount || ticket.OwnerRevision != intent.OwnerRevision || request.RequestId != ticket.RequestId || uint32(request.Mode) != ticket.Mode || request.BattleConfigId != ticket.ConfigId {
		return nil, nil, nil, ErrOriginalPlayer
	}
	high, low, err := joinRequestParts(ticket.RequestId)
	if err != nil || high != intent.RequestHigh || low != intent.RequestLow {
		return nil, nil, nil, ErrOriginalPlayer
	}
	var exit *pb.MatchSpectatorRecord
	if len(intent.SpectatorExit) != 0 {
		exit = &pb.MatchSpectatorRecord{}
		if proto.Unmarshal(intent.SpectatorExit, exit) != nil || validateSpectator(exit) != nil || !sameSpectatorOwner(exit, ticket) {
			return nil, nil, nil, ErrOriginalPlayer
		}
	}
	return ticket, request, exit, nil
}
func (s *Store) PendingJoinsThrough(ctx context.Context, after, through JoinCursor) ([]*pb.MatchJoinIntentRecord, error) {
	rows, err := s.DB.QueryContext(ctx, "SELECT player_id,request_id,ticket_record,cancelled,request_high,request_low,owner_account,owner_revision,spectator_exit,exit_prepared,request_payload,state FROM match_join_intent WHERE state=1 AND (player_id,request_high,request_low)>(?,?,?) AND (player_id,request_high,request_low)<=(?,?,?) ORDER BY player_id,request_high,request_low LIMIT 8", after.Player, after.High, after.Low, through.Player, through.High, through.Low)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []*pb.MatchJoinIntentRecord
	for rows.Next() {
		intent := &pb.MatchJoinIntentRecord{}
		if err = rows.Scan(&intent.PlayerId, &intent.RequestId, &intent.TicketRecord, &intent.Cancelled, &intent.RequestHigh, &intent.RequestLow, &intent.OwnerAccount, &intent.OwnerRevision, &intent.SpectatorExit, &intent.ExitPrepared, &intent.RequestPayload, &intent.State); err != nil {
			return nil, err
		}
		if _, _, _, err = DecodePendingJoin(intent); err != nil {
			return nil, err
		}
		out = append(out, intent)
	}
	return out, rows.Err()
}
