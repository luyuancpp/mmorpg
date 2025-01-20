// Code generated by protoc-gen-go-grpc. DO NOT EDIT.
// versions:
// - protoc-gen-go-grpc v1.4.0
// - protoc             v5.26.1
// source: proto/common/deploy_service.proto

package game

import (
	context "context"
	grpc "google.golang.org/grpc"
	codes "google.golang.org/grpc/codes"
	status "google.golang.org/grpc/status"
)

// This is a compile-time assertion to ensure that this generated file
// is compatible with the grpc package it is being compiled against.
// Requires gRPC-Go v1.62.0 or later.
const _ = grpc.SupportPackageIsVersion8

const (
	DeployService_GetNodeInfo_FullMethodName = "/DeployService/GetNodeInfo"
	DeployService_GetID_FullMethodName       = "/DeployService/GetID"
	DeployService_ReleaseID_FullMethodName   = "/DeployService/ReleaseID"
	DeployService_RenewLease_FullMethodName  = "/DeployService/RenewLease"
)

// DeployServiceClient is the client API for DeployService service.
//
// For semantics around ctx use and closing/ending streaming RPCs, please refer to https://pkg.go.dev/google.golang.org/grpc/?tab=doc#ClientConn.NewStream.
type DeployServiceClient interface {
	GetNodeInfo(ctx context.Context, in *NodeInfoRequest, opts ...grpc.CallOption) (*NodeInfoResponse, error)
	GetID(ctx context.Context, in *GetIDRequest, opts ...grpc.CallOption) (*GetIDResponse, error)
	ReleaseID(ctx context.Context, in *ReleaseIDRequest, opts ...grpc.CallOption) (*ReleaseIDResponse, error)
	RenewLease(ctx context.Context, in *RenewLeaseIDRequest, opts ...grpc.CallOption) (*RenewLeaseIDResponse, error)
}

type deployServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewDeployServiceClient(cc grpc.ClientConnInterface) DeployServiceClient {
	return &deployServiceClient{cc}
}

func (c *deployServiceClient) GetNodeInfo(ctx context.Context, in *NodeInfoRequest, opts ...grpc.CallOption) (*NodeInfoResponse, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(NodeInfoResponse)
	err := c.cc.Invoke(ctx, DeployService_GetNodeInfo_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *deployServiceClient) GetID(ctx context.Context, in *GetIDRequest, opts ...grpc.CallOption) (*GetIDResponse, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(GetIDResponse)
	err := c.cc.Invoke(ctx, DeployService_GetID_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *deployServiceClient) ReleaseID(ctx context.Context, in *ReleaseIDRequest, opts ...grpc.CallOption) (*ReleaseIDResponse, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(ReleaseIDResponse)
	err := c.cc.Invoke(ctx, DeployService_ReleaseID_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *deployServiceClient) RenewLease(ctx context.Context, in *RenewLeaseIDRequest, opts ...grpc.CallOption) (*RenewLeaseIDResponse, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(RenewLeaseIDResponse)
	err := c.cc.Invoke(ctx, DeployService_RenewLease_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

// DeployServiceServer is the server API for DeployService service.
// All implementations must embed UnimplementedDeployServiceServer
// for forward compatibility
type DeployServiceServer interface {
	GetNodeInfo(context.Context, *NodeInfoRequest) (*NodeInfoResponse, error)
	GetID(context.Context, *GetIDRequest) (*GetIDResponse, error)
	ReleaseID(context.Context, *ReleaseIDRequest) (*ReleaseIDResponse, error)
	RenewLease(context.Context, *RenewLeaseIDRequest) (*RenewLeaseIDResponse, error)
	mustEmbedUnimplementedDeployServiceServer()
}

// UnimplementedDeployServiceServer must be embedded to have forward compatible implementations.
type UnimplementedDeployServiceServer struct {
}

func (UnimplementedDeployServiceServer) GetNodeInfo(context.Context, *NodeInfoRequest) (*NodeInfoResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method GetNodeInfo not implemented")
}
func (UnimplementedDeployServiceServer) GetID(context.Context, *GetIDRequest) (*GetIDResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method GetID not implemented")
}
func (UnimplementedDeployServiceServer) ReleaseID(context.Context, *ReleaseIDRequest) (*ReleaseIDResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method ReleaseID not implemented")
}
func (UnimplementedDeployServiceServer) RenewLease(context.Context, *RenewLeaseIDRequest) (*RenewLeaseIDResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method RenewLease not implemented")
}
func (UnimplementedDeployServiceServer) mustEmbedUnimplementedDeployServiceServer() {}

// UnsafeDeployServiceServer may be embedded to opt out of forward compatibility for this service.
// Use of this interface is not recommended, as added methods to DeployServiceServer will
// result in compilation errors.
type UnsafeDeployServiceServer interface {
	mustEmbedUnimplementedDeployServiceServer()
}

func RegisterDeployServiceServer(s grpc.ServiceRegistrar, srv DeployServiceServer) {
	s.RegisterService(&DeployService_ServiceDesc, srv)
}

func _DeployService_GetNodeInfo_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(NodeInfoRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(DeployServiceServer).GetNodeInfo(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: DeployService_GetNodeInfo_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(DeployServiceServer).GetNodeInfo(ctx, req.(*NodeInfoRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _DeployService_GetID_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(GetIDRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(DeployServiceServer).GetID(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: DeployService_GetID_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(DeployServiceServer).GetID(ctx, req.(*GetIDRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _DeployService_ReleaseID_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(ReleaseIDRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(DeployServiceServer).ReleaseID(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: DeployService_ReleaseID_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(DeployServiceServer).ReleaseID(ctx, req.(*ReleaseIDRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _DeployService_RenewLease_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(RenewLeaseIDRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(DeployServiceServer).RenewLease(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: DeployService_RenewLease_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(DeployServiceServer).RenewLease(ctx, req.(*RenewLeaseIDRequest))
	}
	return interceptor(ctx, in, info, handler)
}

// DeployService_ServiceDesc is the grpc.ServiceDesc for DeployService service.
// It's only intended for direct use with grpc.RegisterService,
// and not to be introspected or modified (even as a copy)
var DeployService_ServiceDesc = grpc.ServiceDesc{
	ServiceName: "DeployService",
	HandlerType: (*DeployServiceServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "GetNodeInfo",
			Handler:    _DeployService_GetNodeInfo_Handler,
		},
		{
			MethodName: "GetID",
			Handler:    _DeployService_GetID_Handler,
		},
		{
			MethodName: "ReleaseID",
			Handler:    _DeployService_ReleaseID_Handler,
		},
		{
			MethodName: "RenewLease",
			Handler:    _DeployService_RenewLease_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "proto/common/deploy_service.proto",
}