package server

import (
	"context"
	tradepb "proto/trade"
	"trade/internal/logic"
)

// 手写薄入口延续既有JubaozhaiServer；不修改生成树。
func (s *JubaozhaiServer) CreateRmbConsignment(ctx context.Context, in *tradepb.CreateRmbConsignmentRequest) (*tradepb.RmbConsignmentResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).CreateRmbConsignment(in)
}
func (s *JubaozhaiServer) GetRmbConsignment(ctx context.Context, in *tradepb.GetRmbConsignmentRequest) (*tradepb.RmbConsignmentResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).GetRmbConsignment(in)
}
func (s *JubaozhaiServer) ListRmbConsignments(ctx context.Context, in *tradepb.ListRmbConsignmentsRequest) (*tradepb.ListRmbConsignmentsResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).ListRmbConsignments(in)
}
func (s *JubaozhaiServer) BuyRmbConsignment(ctx context.Context, in *tradepb.BuyRmbConsignmentRequest) (*tradepb.CommerceOrderResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).BuyRmbConsignment(in)
}
func (s *JubaozhaiServer) CancelRmbConsignment(ctx context.Context, in *tradepb.CancelRmbConsignmentRequest) (*tradepb.RmbConsignmentResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).CancelRmbConsignment(in)
}
func (s *JubaozhaiServer) GetRmbOrder(ctx context.Context, in *tradepb.GetCommerceOrderRequest) (*tradepb.CommerceOrderResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).GetRmbOrder(in)
}
func (s *JubaozhaiServer) ListRmbOrders(ctx context.Context, in *tradepb.ListCommerceOrdersRequest) (*tradepb.ListCommerceOrdersResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).ListRmbOrders(in)
}
func (s *JubaozhaiServer) CancelRmbOrder(ctx context.Context, in *tradepb.CancelCommerceOrderRequest) (*tradepb.CommerceOrderResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).CancelRmbOrder(in)
}
func (s *JubaozhaiServer) ListSellerIncomes(ctx context.Context, in *tradepb.ListSellerIncomesRequest) (*tradepb.ListSellerIncomesResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).ListSellerIncomes(in)
}
func (s *JubaozhaiServer) CreateWithdrawal(ctx context.Context, in *tradepb.CreateWithdrawalRequest) (*tradepb.WithdrawalResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).CreateWithdrawal(in)
}
func (s *JubaozhaiServer) GetWithdrawal(ctx context.Context, in *tradepb.GetWithdrawalRequest) (*tradepb.WithdrawalResponse, error) {
	return logic.NewJubaozhaiLogic(ctx, s.deps).GetWithdrawal(in)
}
