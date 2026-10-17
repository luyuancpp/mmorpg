package logic

import (
	"context"
	"errors"

	"data_service/internal/svc"
	goredis "github.com/redis/go-redis/v9"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
	componentpb "proto/common/component"
	"shared/assetop"
)

// GetSQLCommittedAssetLedger 只接受 go/db 在同一玩家 SQL 行提交后发布的证据。
// 缺证据与旧版服务没有证据均等待重投，不能用异步存盘前的 Redis 玩家 blob 代替。
func GetSQLCommittedAssetLedger(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64) (*componentpb.PlayerAssetOpLedgerComp, bool, error) {
	if ctx.Err() != nil {
		return nil, false, status.FromContextError(ctx.Err()).Err()
	}
	if playerID == 0 {
		return nil, false, status.Error(codes.InvalidArgument, "player_id is required")
	}
	if svcCtx == nil || svcCtx.Router == nil {
		return nil, false, status.Error(codes.Unavailable, "player router not configured")
	}
	client, err := svcCtx.Router.ClientForPlayer(ctx, playerID)
	if err != nil {
		if ctx.Err() != nil {
			return nil, false, status.FromContextError(ctx.Err()).Err()
		}
		return nil, false, status.Error(codes.Unavailable, "asset SQL commit route unavailable")
	}
	raw, err := client.HGet(ctx, assetop.SQLCommitKey(playerID), "data").Bytes()
	if errors.Is(err, goredis.Nil) {
		return nil, false, nil
	}
	if err != nil {
		if ctx.Err() != nil {
			return nil, false, status.FromContextError(ctx.Err()).Err()
		}
		return nil, false, status.Error(codes.Unavailable, "asset SQL commit proof unavailable")
	}
	commit := &assetpb.PlayerAssetSQLCommit{}
	if err := proto.Unmarshal(raw, commit); err != nil || commit.PlayerId != playerID || commit.OwnerEpoch == 0 || commit.SaveSeq == 0 {
		return nil, false, status.Error(codes.Internal, "asset SQL commit proof invalid")
	}
	ledger := &componentpb.PlayerAssetOpLedgerComp{}
	if err := proto.Unmarshal(commit.LedgerProto, ledger); err != nil {
		return nil, false, status.Error(codes.Internal, "asset SQL commit ledger invalid")
	}
	return ledger, true, nil
}
