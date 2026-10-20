package loginadminlogic

import (
	"context"

	"login/internal/svc"
	"proto/login"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

type RemovePlayersFromAccountsLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewRemovePlayersFromAccountsLogic(ctx context.Context, svcCtx *svc.ServiceContext) *RemovePlayersFromAccountsLogic {
	return &RemovePlayersFromAccountsLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Remove orphan characters from their accounts' SimplePlayers lists.
//
// 2026-10-18 起明确返回 Unimplemented:这个 gRPC 入口从生成起就是空桩,此前回一个 0/0/0、无错误的空响应,
// 调用方会误以为「没有孤儿要清」而实际什么都没执行。实现在 login/internal/logic/admin.RemovePlayersFromAccounts
// (账号锁内原子摘孤儿并删反向索引),但还没有生产接线;要接上需另开任务:先确认 LoginAdmin 的鉴权拦截,
// 再把三个计数映射到 removed_count / not_found_count / failed_count,并把「账号忙时重跑」的语义写进 proto 注释。
func (l *RemovePlayersFromAccountsLogic) RemovePlayersFromAccounts(in *login.RemovePlayersFromAccountsRequest) (*login.RemovePlayersFromAccountsResponse, error) {
	return nil, status.Error(codes.Unimplemented,
		"RemovePlayersFromAccounts has no production wiring yet; see login/internal/logic/admin.RemovePlayersFromAccounts")
}
