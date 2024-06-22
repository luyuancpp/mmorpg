// Code generated by protoc-gen-go-grpc. DO NOT EDIT.
// versions:
// - protoc-gen-go-grpc v1.3.0
// - protoc             v4.25.1
// source: common_proto/db_service.proto

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
	AccountDBService_Load2Redis_FullMethodName = "/AccountDBService/Load2Redis"
	AccountDBService_Save2Redis_FullMethodName = "/AccountDBService/Save2Redis"
)

// AccountDBServiceClient is the client API for AccountDBService service.
//
// For semantics around ctx use and closing/ending streaming RPCs, please refer to https://pkg.go.dev/google.golang.org/grpc/?tab=doc#ClientConn.NewStream.
type AccountDBServiceClient interface {
	Load2Redis(ctx context.Context, in *LoadAccountRequest, opts ...grpc.CallOption) (*LoadAccountResponse, error)
	Save2Redis(ctx context.Context, in *SaveAccountRequest, opts ...grpc.CallOption) (*SaveAccountResponse, error)
}

type accountDBServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewAccountDBServiceClient(cc grpc.ClientConnInterface) AccountDBServiceClient {
	return &accountDBServiceClient{cc}
}

func (c *accountDBServiceClient) Load2Redis(ctx context.Context, in *LoadAccountRequest, opts ...grpc.CallOption) (*LoadAccountResponse, error) {
	out := new(LoadAccountResponse)
	err := c.cc.Invoke(ctx, AccountDBService_Load2Redis_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *accountDBServiceClient) Save2Redis(ctx context.Context, in *SaveAccountRequest, opts ...grpc.CallOption) (*SaveAccountResponse, error) {
	out := new(SaveAccountResponse)
	err := c.cc.Invoke(ctx, AccountDBService_Save2Redis_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

// AccountDBServiceServer is the server API for AccountDBService service.
// All implementations must embed UnimplementedAccountDBServiceServer
// for forward compatibility
type AccountDBServiceServer interface {
	Load2Redis(context.Context, *LoadAccountRequest) (*LoadAccountResponse, error)
	Save2Redis(context.Context, *SaveAccountRequest) (*SaveAccountResponse, error)
	mustEmbedUnimplementedAccountDBServiceServer()
}

// UnimplementedAccountDBServiceServer must be embedded to have forward compatible implementations.
type UnimplementedAccountDBServiceServer struct {
}

func (UnimplementedAccountDBServiceServer) Load2Redis(context.Context, *LoadAccountRequest) (*LoadAccountResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Load2Redis not implemented")
}
func (UnimplementedAccountDBServiceServer) Save2Redis(context.Context, *SaveAccountRequest) (*SaveAccountResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Save2Redis not implemented")
}
func (UnimplementedAccountDBServiceServer) mustEmbedUnimplementedAccountDBServiceServer() {}

// UnsafeAccountDBServiceServer may be embedded to opt out of forward compatibility for this service.
// Use of this interface is not recommended, as added methods to AccountDBServiceServer will
// result in compilation errors.
type UnsafeAccountDBServiceServer interface {
	mustEmbedUnimplementedAccountDBServiceServer()
}

func RegisterAccountDBServiceServer(s grpc.ServiceRegistrar, srv AccountDBServiceServer) {
	s.RegisterService(&AccountDBService_ServiceDesc, srv)
}

func _AccountDBService_Load2Redis_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(LoadAccountRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(AccountDBServiceServer).Load2Redis(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: AccountDBService_Load2Redis_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(AccountDBServiceServer).Load2Redis(ctx, req.(*LoadAccountRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _AccountDBService_Save2Redis_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(SaveAccountRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(AccountDBServiceServer).Save2Redis(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: AccountDBService_Save2Redis_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(AccountDBServiceServer).Save2Redis(ctx, req.(*SaveAccountRequest))
	}
	return interceptor(ctx, in, info, handler)
}

// AccountDBService_ServiceDesc is the grpc.ServiceDesc for AccountDBService service.
// It's only intended for direct use with grpc.RegisterService,
// and not to be introspected or modified (even as a copy)
var AccountDBService_ServiceDesc = grpc.ServiceDesc{
	ServiceName: "AccountDBService",
	HandlerType: (*AccountDBServiceServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "Load2Redis",
			Handler:    _AccountDBService_Load2Redis_Handler,
		},
		{
			MethodName: "Save2Redis",
			Handler:    _AccountDBService_Save2Redis_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "common_proto/db_service.proto",
}

const (
	PlayerDBService_Load2Redis_FullMethodName = "/PlayerDBService/Load2Redis"
	PlayerDBService_Save2Redis_FullMethodName = "/PlayerDBService/Save2Redis"
)

// PlayerDBServiceClient is the client API for PlayerDBService service.
//
// For semantics around ctx use and closing/ending streaming RPCs, please refer to https://pkg.go.dev/google.golang.org/grpc/?tab=doc#ClientConn.NewStream.
type PlayerDBServiceClient interface {
	Load2Redis(ctx context.Context, in *LoadPlayerRequest, opts ...grpc.CallOption) (*LoadPlayerResponse, error)
	Save2Redis(ctx context.Context, in *SavePlayerRequest, opts ...grpc.CallOption) (*SavePlayerResponse, error)
}

type playerDBServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewPlayerDBServiceClient(cc grpc.ClientConnInterface) PlayerDBServiceClient {
	return &playerDBServiceClient{cc}
}

func (c *playerDBServiceClient) Load2Redis(ctx context.Context, in *LoadPlayerRequest, opts ...grpc.CallOption) (*LoadPlayerResponse, error) {
	out := new(LoadPlayerResponse)
	err := c.cc.Invoke(ctx, PlayerDBService_Load2Redis_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *playerDBServiceClient) Save2Redis(ctx context.Context, in *SavePlayerRequest, opts ...grpc.CallOption) (*SavePlayerResponse, error) {
	out := new(SavePlayerResponse)
	err := c.cc.Invoke(ctx, PlayerDBService_Save2Redis_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

// PlayerDBServiceServer is the server API for PlayerDBService service.
// All implementations must embed UnimplementedPlayerDBServiceServer
// for forward compatibility
type PlayerDBServiceServer interface {
	Load2Redis(context.Context, *LoadPlayerRequest) (*LoadPlayerResponse, error)
	Save2Redis(context.Context, *SavePlayerRequest) (*SavePlayerResponse, error)
	mustEmbedUnimplementedPlayerDBServiceServer()
}

// UnimplementedPlayerDBServiceServer must be embedded to have forward compatible implementations.
type UnimplementedPlayerDBServiceServer struct {
}

func (UnimplementedPlayerDBServiceServer) Load2Redis(context.Context, *LoadPlayerRequest) (*LoadPlayerResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Load2Redis not implemented")
}
func (UnimplementedPlayerDBServiceServer) Save2Redis(context.Context, *SavePlayerRequest) (*SavePlayerResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Save2Redis not implemented")
}
func (UnimplementedPlayerDBServiceServer) mustEmbedUnimplementedPlayerDBServiceServer() {}

// UnsafePlayerDBServiceServer may be embedded to opt out of forward compatibility for this service.
// Use of this interface is not recommended, as added methods to PlayerDBServiceServer will
// result in compilation errors.
type UnsafePlayerDBServiceServer interface {
	mustEmbedUnimplementedPlayerDBServiceServer()
}

func RegisterPlayerDBServiceServer(s grpc.ServiceRegistrar, srv PlayerDBServiceServer) {
	s.RegisterService(&PlayerDBService_ServiceDesc, srv)
}

func _PlayerDBService_Load2Redis_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(LoadPlayerRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(PlayerDBServiceServer).Load2Redis(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: PlayerDBService_Load2Redis_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(PlayerDBServiceServer).Load2Redis(ctx, req.(*LoadPlayerRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _PlayerDBService_Save2Redis_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(SavePlayerRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(PlayerDBServiceServer).Save2Redis(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: PlayerDBService_Save2Redis_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(PlayerDBServiceServer).Save2Redis(ctx, req.(*SavePlayerRequest))
	}
	return interceptor(ctx, in, info, handler)
}

// PlayerDBService_ServiceDesc is the grpc.ServiceDesc for PlayerDBService service.
// It's only intended for direct use with grpc.RegisterService,
// and not to be introspected or modified (even as a copy)
var PlayerDBService_ServiceDesc = grpc.ServiceDesc{
	ServiceName: "PlayerDBService",
	HandlerType: (*PlayerDBServiceServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "Load2Redis",
			Handler:    _PlayerDBService_Load2Redis_Handler,
		},
		{
			MethodName: "Save2Redis",
			Handler:    _PlayerDBService_Save2Redis_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "common_proto/db_service.proto",
}

const (
	PlayerCentreDBService_Load2Redis_FullMethodName = "/PlayerCentreDBService/Load2Redis"
	PlayerCentreDBService_Save2Redis_FullMethodName = "/PlayerCentreDBService/Save2Redis"
)

// PlayerCentreDBServiceClient is the client API for PlayerCentreDBService service.
//
// For semantics around ctx use and closing/ending streaming RPCs, please refer to https://pkg.go.dev/google.golang.org/grpc/?tab=doc#ClientConn.NewStream.
type PlayerCentreDBServiceClient interface {
	Load2Redis(ctx context.Context, in *LoadPlayerCentreRequest, opts ...grpc.CallOption) (*LoadPlayerCentreResponse, error)
	Save2Redis(ctx context.Context, in *SavePlayerCentreRequest, opts ...grpc.CallOption) (*SavePlayerCentreResponse, error)
}

type playerCentreDBServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewPlayerCentreDBServiceClient(cc grpc.ClientConnInterface) PlayerCentreDBServiceClient {
	return &playerCentreDBServiceClient{cc}
}

func (c *playerCentreDBServiceClient) Load2Redis(ctx context.Context, in *LoadPlayerCentreRequest, opts ...grpc.CallOption) (*LoadPlayerCentreResponse, error) {
	out := new(LoadPlayerCentreResponse)
	err := c.cc.Invoke(ctx, PlayerCentreDBService_Load2Redis_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *playerCentreDBServiceClient) Save2Redis(ctx context.Context, in *SavePlayerCentreRequest, opts ...grpc.CallOption) (*SavePlayerCentreResponse, error) {
	out := new(SavePlayerCentreResponse)
	err := c.cc.Invoke(ctx, PlayerCentreDBService_Save2Redis_FullMethodName, in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

// PlayerCentreDBServiceServer is the server API for PlayerCentreDBService service.
// All implementations must embed UnimplementedPlayerCentreDBServiceServer
// for forward compatibility
type PlayerCentreDBServiceServer interface {
	Load2Redis(context.Context, *LoadPlayerCentreRequest) (*LoadPlayerCentreResponse, error)
	Save2Redis(context.Context, *SavePlayerCentreRequest) (*SavePlayerCentreResponse, error)
	mustEmbedUnimplementedPlayerCentreDBServiceServer()
}

// UnimplementedPlayerCentreDBServiceServer must be embedded to have forward compatible implementations.
type UnimplementedPlayerCentreDBServiceServer struct {
}

func (UnimplementedPlayerCentreDBServiceServer) Load2Redis(context.Context, *LoadPlayerCentreRequest) (*LoadPlayerCentreResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Load2Redis not implemented")
}
func (UnimplementedPlayerCentreDBServiceServer) Save2Redis(context.Context, *SavePlayerCentreRequest) (*SavePlayerCentreResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Save2Redis not implemented")
}
func (UnimplementedPlayerCentreDBServiceServer) mustEmbedUnimplementedPlayerCentreDBServiceServer() {}

// UnsafePlayerCentreDBServiceServer may be embedded to opt out of forward compatibility for this service.
// Use of this interface is not recommended, as added methods to PlayerCentreDBServiceServer will
// result in compilation errors.
type UnsafePlayerCentreDBServiceServer interface {
	mustEmbedUnimplementedPlayerCentreDBServiceServer()
}

func RegisterPlayerCentreDBServiceServer(s grpc.ServiceRegistrar, srv PlayerCentreDBServiceServer) {
	s.RegisterService(&PlayerCentreDBService_ServiceDesc, srv)
}

func _PlayerCentreDBService_Load2Redis_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(LoadPlayerCentreRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(PlayerCentreDBServiceServer).Load2Redis(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: PlayerCentreDBService_Load2Redis_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(PlayerCentreDBServiceServer).Load2Redis(ctx, req.(*LoadPlayerCentreRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _PlayerCentreDBService_Save2Redis_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(SavePlayerCentreRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(PlayerCentreDBServiceServer).Save2Redis(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: PlayerCentreDBService_Save2Redis_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(PlayerCentreDBServiceServer).Save2Redis(ctx, req.(*SavePlayerCentreRequest))
	}
	return interceptor(ctx, in, info, handler)
}

// PlayerCentreDBService_ServiceDesc is the grpc.ServiceDesc for PlayerCentreDBService service.
// It's only intended for direct use with grpc.RegisterService,
// and not to be introspected or modified (even as a copy)
var PlayerCentreDBService_ServiceDesc = grpc.ServiceDesc{
	ServiceName: "PlayerCentreDBService",
	HandlerType: (*PlayerCentreDBServiceServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "Load2Redis",
			Handler:    _PlayerCentreDBService_Load2Redis_Handler,
		},
		{
			MethodName: "Save2Redis",
			Handler:    _PlayerCentreDBService_Save2Redis_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "common_proto/db_service.proto",
}
