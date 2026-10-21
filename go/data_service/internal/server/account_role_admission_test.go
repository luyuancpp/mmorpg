package server

import (
	"context"
	"errors"
	"testing"
	"time"

	"data_service/internal/svc"
	base "proto/common/base"
	dspb "proto/data_service"
	"shared/accountdirectory"
	"shared/serviceauth"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

type roleAdmissionsFake struct {
	calls int
	in    proto.Message
	ctx   context.Context
	err   error
}

func (f *roleAdmissionsFake) record(ctx context.Context, in proto.Message) {
	f.calls++
	f.in = in
	f.ctx = ctx
}
func (f *roleAdmissionsFake) Register(ctx context.Context, in *dspb.RegisterRoleAdmissionRequest) (*dspb.RegisterRoleAdmissionResponse, error) {
	f.record(ctx, in)
	return &dspb.RegisterRoleAdmissionResponse{Identity: in.Identity, ClosedThrough: 7}, f.err
}
func (f *roleAdmissionsFake) CloseAdmissions(ctx context.Context, in *dspb.CloseRoleAdmissionsRequest) (*dspb.CloseRoleAdmissionsResponse, error) {
	f.record(ctx, in)
	return &dspb.CloseRoleAdmissionsResponse{ClosedThrough: in.ThroughGeneration}, f.err
}
func (f *roleAdmissionsFake) AckDrain(ctx context.Context, in *dspb.AckRoleTradeDrainRequest) error {
	f.record(ctx, in)
	return f.err
}

func roleAdmissionRPCs() []roleTradeRPC {
	return []roleTradeRPC{
		{"RegisterRoleAdmission", func() proto.Message {
			return &dspb.RegisterRoleAdmissionRequest{Identity: &base.RoleAdmissionIdentity{PlayerId: ^uint64(0), SceneInstanceId: "11111111-2222-3333-4444-555555555555", Generation: 9, OwnerRevision: 17, OwnerEpoch: 23, SceneNodeId: "11", SceneId: 111, ZoneId: 1, SessionId: 5, LoginGeneration: 99}}
		}, func(ctx context.Context, s *DataServiceServer, in proto.Message) (proto.Message, error) {
			return s.RegisterRoleAdmission(ctx, in.(*dspb.RegisterRoleAdmissionRequest))
		}},
		{"CloseRoleAdmissions", func() proto.Message {
			return &dspb.CloseRoleAdmissionsRequest{PlayerId: ^uint64(0), SceneInstanceId: "11111111-2222-3333-4444-555555555555", OwnerRevision: 17, ThroughGeneration: 9, RequiredOwnerEpoch: 23, RequiredSaveSeq: 33}
		}, func(ctx context.Context, s *DataServiceServer, in proto.Message) (proto.Message, error) {
			return s.CloseRoleAdmissions(ctx, in.(*dspb.CloseRoleAdmissionsRequest))
		}},
		{"AckRoleTradeDrain", func() proto.Message {
			return &dspb.AckRoleTradeDrainRequest{Seal: &base.RoleTradeSeal{ListingId: 101, PlayerId: ^uint64(0), OwnerRevision: 17, SceneInstanceId: "11111111-2222-3333-4444-555555555555", ThroughGeneration: 9, SceneNodeId: "11", ZoneId: 1}, RequiredOwnerEpoch: 23, RequiredSaveSeq: 33}
		}, func(ctx context.Context, s *DataServiceServer, in proto.Message) (proto.Message, error) {
			return s.AckRoleTradeDrain(ctx, in.(*dspb.AckRoleTradeDrainRequest))
		}},
	}
}
func roleAdmissionVerifier(t *testing.T) *serviceauth.Verifier {
	t.Helper()
	v, err := serviceauth.New(map[string]string{"scene": directoryTestSecret, "login": directoryTestSecret, "trade": directoryTestSecret})
	if err != nil {
		t.Fatal(err)
	}
	return v
}
func TestRoleAdmissionRPCRejectsMissingWrongAndChangedProof(t *testing.T) {
	for _, rpc := range roleAdmissionRPCs() {
		for _, mode := range []string{"unsigned", "login", "trade", "other-method", "body-changed", "expired"} {
			t.Run(rpc.method+"/"+mode, func(t *testing.T) {
				f := &roleAdmissionsFake{}
				s := NewDataServiceServer(&svc.ServiceContext{RoleAdmissions: f, RoleAdmissionAuth: roleAdmissionVerifier(t)})
				in := rpc.request()
				caller, method, stamp := "scene", rpc.method, uint64(time.Now().UnixMilli())
				if mode == "login" || mode == "trade" {
					caller = mode
				}
				if mode == "other-method" {
					method += "Other"
				}
				if mode == "expired" {
					stamp = 1
				}
				if mode != "unsigned" {
					if err := serviceauth.Sign(accountdirectory.MethodPrefix+method, in, caller, directoryTestSecret, stamp); err != nil {
						t.Fatal(err)
					}
				}
				if mode == "body-changed" {
					switch r := in.(type) {
					case *dspb.RegisterRoleAdmissionRequest:
						r.Identity.OwnerRevision++
					case *dspb.CloseRoleAdmissionsRequest:
						r.RequiredSaveSeq++
					case *dspb.AckRoleTradeDrainRequest:
						r.Seal.ThroughGeneration++
					}
				}
				_, err := rpc.call(context.Background(), s, in)
				if status.Code(err) != codes.PermissionDenied || f.calls != 0 {
					t.Fatalf("无效证明产生写入: err=%v calls=%d", err, f.calls)
				}
			})
		}
	}
}
func TestRoleAdmissionRPCUnreadyErrorsAndOriginalFields(t *testing.T) {
	for _, rpc := range roleAdmissionRPCs() {
		for _, missing := range []string{"store", "auth"} {
			t.Run(rpc.method+"/missing-"+missing, func(t *testing.T) {
				sc := &svc.ServiceContext{RoleAdmissions: &roleAdmissionsFake{}, RoleAdmissionAuth: roleAdmissionVerifier(t)}
				if missing == "store" {
					sc.RoleAdmissions = nil
				} else {
					sc.RoleAdmissionAuth = nil
				}
				_, err := rpc.call(context.Background(), NewDataServiceServer(sc), rpc.request())
				if status.Code(err) != codes.Unavailable {
					t.Fatalf("未就绪未拒绝: %v", err)
				}
			})
		}
		for _, tc := range []struct {
			name string
			err  error
			code codes.Code
		}{{"success", nil, codes.OK}, {"invalid", accountdirectory.ErrInvalid, codes.InvalidArgument}, {"conflict", accountdirectory.ErrConflict, codes.FailedPrecondition}, {"missing", accountdirectory.ErrMissing, codes.NotFound}, {"sql", errors.New("SQL dependency failure"), codes.Unavailable}} {
			t.Run(rpc.method+"/"+tc.name, func(t *testing.T) {
				f := &roleAdmissionsFake{err: tc.err}
				s := NewDataServiceServer(&svc.ServiceContext{RoleAdmissions: f, RoleAdmissionAuth: roleAdmissionVerifier(t)})
				in := rpc.request()
				if err := serviceauth.Sign(accountdirectory.MethodPrefix+rpc.method, in, "scene", directoryTestSecret, uint64(time.Now().UnixMilli())); err != nil {
					t.Fatal(err)
				}
				ctx := context.WithValue(context.Background(), struct{}{}, "admission")
				out, err := rpc.call(ctx, s, in)
				if status.Code(err) != tc.code || f.calls != 1 || f.in != in || f.ctx != ctx {
					t.Fatalf("原入参/上下文/错误分类改变: %v/%v", out, err)
				}
				if tc.err != nil && out != nil && out.ProtoReflect().IsValid() {
					t.Fatalf("失败返回部分成功: %v", out)
				}
			})
		}
	}
}
