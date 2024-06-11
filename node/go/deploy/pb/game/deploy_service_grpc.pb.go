// Code generated by protoc-gen-go-grpc. DO NOT EDIT.
// versions:
// - protoc-gen-go-grpc v1.3.0
// - protoc             v4.25.1
// source: common_proto/deploy_service.proto

package game

import (
	context "context"
	grpc "google.golang.org/grpc"
	codes "google.golang.org/grpc/codes"
	status "google.golang.org/grpc/status"
)

// This is a compile-time assertion to ensure that this generated file
// is compatible with the grpc package it is being compiled against.
// Requires gRPC-Go v1.32.0 or later.
const _ = grpc.SupportPackageIsVersion7

const (
	DeployService_GetNodeInfo_FullMethodName     = "/DeployService/GetNodeInfo"
	DeployService_NewGameNode_FullMethodName     = "/DeployService/NewGameNode"
	DeployService_GetGameNodeInfo_FullMethodName = "/DeployService/GetGameNodeInfo"
)

// DeployServiceClient is the client API for DeployService service.
//
// For semantics around ctx use and closing/ending streaming RPCs, please refer to https://pkg.go.dev/google.golang.org/grpc/?tab=doc#ClientConn.NewStream.
type DeployServiceClient interface {
	GetNodeInfo(ctx context.Context, in *NodeInfoRequest, opts ...grpc.CallOption) (*NodeInfoResponse, error)
	NewGameNode(ctx context.Context, in *NewGameNodeRequest, opts ...grpc.CallOption) (*NewGameNodeResponse, error)
	GetGameNodeInfo(ctx context.Context, in *GetGameNodeInfoRequest, opts ...grpc.CallOption) (*GetGameNodeInfoResponse, error)
}

type deployServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewDeployServiceClient(cc grpc.ClientConnInterface) DeployServiceClient {
	return &deployServiceClient{cc}
}

func (c *deployServiceClient) GetNodeInfo(ctx context.Context, in *NodeInfoRequest, opts ...grpc.CallOption) (*NodeInfoResponse, error) {
	out := new(NodeInfoResponse)
	err := c.cc.Invoke(ctx, DeployService_GetNodeInfo_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *deployServiceClient) NewGameNode(ctx context.Context, in *NewGameNodeRequest, opts ...grpc.CallOption) (*NewGameNodeResponse, error) {
	out := new(NewGameNodeResponse)
	err := c.cc.Invoke(ctx, DeployService_NewGameNode_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *deployServiceClient) GetGameNodeInfo(ctx context.Context, in *GetGameNodeInfoRequest, opts ...grpc.CallOption) (*GetGameNodeInfoResponse, error) {
	out := new(GetGameNodeInfoResponse)
	err := c.cc.Invoke(ctx, DeployService_GetGameNodeInfo_FullMethodName, in, out, opts...)
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
	NewGameNode(context.Context, *NewGameNodeRequest) (*NewGameNodeResponse, error)
	GetGameNodeInfo(context.Context, *GetGameNodeInfoRequest) (*GetGameNodeInfoResponse, error)
	mustEmbedUnimplementedDeployServiceServer()
}

// UnimplementedDeployServiceServer must be embedded to have forward compatible implementations.
type UnimplementedDeployServiceServer struct {
}

func (UnimplementedDeployServiceServer) GetNodeInfo(context.Context, *NodeInfoRequest) (*NodeInfoResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method GetNodeInfo not implemented")
}
func (UnimplementedDeployServiceServer) NewGameNode(context.Context, *NewGameNodeRequest) (*NewGameNodeResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method NewGameNode not implemented")
}
func (UnimplementedDeployServiceServer) GetGameNodeInfo(context.Context, *GetGameNodeInfoRequest) (*GetGameNodeInfoResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method GetGameNodeInfo not implemented")
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

func _DeployService_NewGameNode_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(NewGameNodeRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(DeployServiceServer).NewGameNode(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: DeployService_NewGameNode_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(DeployServiceServer).NewGameNode(ctx, req.(*NewGameNodeRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _DeployService_GetGameNodeInfo_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(GetGameNodeInfoRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(DeployServiceServer).GetGameNodeInfo(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: DeployService_GetGameNodeInfo_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(DeployServiceServer).GetGameNodeInfo(ctx, req.(*GetGameNodeInfoRequest))
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
			MethodName: "NewGameNode",
			Handler:    _DeployService_NewGameNode_Handler,
		},
		{
			MethodName: "GetGameNodeInfo",
			Handler:    _DeployService_GetGameNodeInfo_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "common_proto/deploy_service.proto",
}
