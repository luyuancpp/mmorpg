// Package server 是 rank 的 gRPC 入口薄包装(照 friend / mail 的 internal/server):每个 RPC 委托给 logic,不写业务。
package server

import (
	"context"

	"rank/internal/logic"

	rankpb "proto/rank"
)

// RankServer 实现 rankpb.ClientPlayerRankServer(标了 OptionIsClientProtocolService,只接 gate → 路由服转来的客户端请求)。
//
// 内嵌 UnimplementedClientPlayerRankServer 满足生成代码的 mustEmbed 约束。**内部 / 写方法不许加到这个 service 上**:
// 服务级客户端开关会把同 service 的所有方法带进 gate 白名单与路由表(设计 L-12);要加就另建不带开关的 service。
type RankServer struct {
	rankpb.UnimplementedClientPlayerRankServer
	logic *logic.RankLogic
}

// NewRankServer 返回接口类型:调用方(main)只需要能注册进 gRPC。
func NewRankServer(l *logic.RankLogic) rankpb.ClientPlayerRankServer {
	return &RankServer{logic: l}
}

// GetRankPage 拉一页 + 调用者自己的名次。
func (s *RankServer) GetRankPage(ctx context.Context, in *rankpb.GetRankPageRequest) (*rankpb.GetRankPageResponse, error) {
	return s.logic.GetRankPage(ctx, in)
}
