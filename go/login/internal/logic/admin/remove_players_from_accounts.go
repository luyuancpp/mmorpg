package admin

import (
	"context"
	"errors"
	"github.com/zeromicro/go-zero/core/logx"
	"login/internal/svc"
	dspb "proto/data_service"
	"shared/accountdirectory"
)

type RemovePlayersFromAccountsReq struct{ PlayerIDs []uint64 }
type RemovePlayersFromAccountsResp struct{ RemovedCount, NotFoundCount, FailedCount uint32 }

// 回档孤儿清理只读全局 SQL 归属；陈旧 Redis 反向键不再授权删除。
// 当前 LoginAdmin RPC 仍未接线；本函数是待受控管理调用者接入的目录删除接缝，不授权公开调用。
func RemovePlayersFromAccounts(ctx context.Context, sc *svc.ServiceContext, req *RemovePlayersFromAccountsReq) (*RemovePlayersFromAccountsResp, error) {
	out := &RemovePlayersFromAccountsResp{}
	if sc.AccountDirectory == nil {
		return out, accountdirectory.ErrUnavailable
	}
	if req == nil || len(req.PlayerIDs) > 10000 {
		return out, accountdirectory.ErrInvalid
	}
	groups := map[string][]*dspb.AccountRoleVersion{}
	seen := map[uint64]bool{}
	for _, pid := range req.PlayerIDs {
		if seen[pid] {
			continue
		}
		seen[pid] = true
		owned, err := sc.AccountDirectory.Lookup(ctx, pid)
		if errors.Is(err, accountdirectory.ErrMissing) {
			out.NotFoundCount++
			continue
		}
		if err != nil {
			out.FailedCount++
			logx.Errorf("[account-directory] 孤儿归属查询失败: %v", err)
			continue
		}
		groups[owned.Account] = append(groups[owned.Account], &dspb.AccountRoleVersion{PlayerId: pid, OwnerRevision: owned.OwnerRevision})
	}
	for account, versions := range groups {
		if err := sc.AccountDirectory.Remove(ctx, account, versions); err != nil {
			out.FailedCount += uint32(len(versions))
			logx.Errorf("[account-directory] 孤儿清理失败，保留原归属: %v", err)
			continue
		}
		out.RemovedCount += uint32(len(versions))
	}
	return out, nil
}
