// Code generated by protoc-gen-go-grpc. DO NOT EDIT.
// versions:
// - protoc-gen-go-grpc v1.5.1
// - protoc             v3.19.4
// source: common/login_service.proto

package game

import (
	context "context"
	grpc "google.golang.org/grpc"
	codes "google.golang.org/grpc/codes"
	status "google.golang.org/grpc/status"
)

// This is a compile-time assertion to ensure that this generated file
// is compatible with the grpc package it is being compiled against.
// Requires gRPC-Go v1.64.0 or later.
const _ = grpc.SupportPackageIsVersion9

const (
	LoginService_Login_FullMethodName        = "/LoginService/Login"
	LoginService_CreatePlayer_FullMethodName = "/LoginService/CreatePlayer"
	LoginService_EnterGame_FullMethodName    = "/LoginService/EnterGame"
	LoginService_LeaveGame_FullMethodName    = "/LoginService/LeaveGame"
	LoginService_Disconnect_FullMethodName   = "/LoginService/Disconnect"
)

// LoginServiceClient is the client API for LoginService service.
//
// For semantics around ctx use and closing/ending streaming RPCs, please refer to https://pkg.go.dev/google.golang.org/grpc/?tab=doc#ClientConn.NewStream.
type LoginServiceClient interface {
	Login(ctx context.Context, in *LoginC2LRequest, opts ...grpc.CallOption) (*LoginC2LResponse, error)
	CreatePlayer(ctx context.Context, in *CreatePlayerC2LRequest, opts ...grpc.CallOption) (*CreatePlayerC2LResponse, error)
	EnterGame(ctx context.Context, in *EnterGameC2LRequest, opts ...grpc.CallOption) (*EnterGameC2LResponse, error)
	LeaveGame(ctx context.Context, in *LeaveGameC2LRequest, opts ...grpc.CallOption) (*Empty, error)
	Disconnect(ctx context.Context, in *LoginNodeDisconnectRequest, opts ...grpc.CallOption) (*Empty, error)
}

type loginServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewLoginServiceClient(cc grpc.ClientConnInterface) LoginServiceClient {
	return &loginServiceClient{cc}
}

func (c *loginServiceClient) Login(ctx context.Context, in *LoginC2LRequest, opts ...grpc.CallOption) (*LoginC2LResponse, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(LoginC2LResponse)
	err := c.cc.Invoke(ctx, LoginService_Login_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *loginServiceClient) CreatePlayer(ctx context.Context, in *CreatePlayerC2LRequest, opts ...grpc.CallOption) (*CreatePlayerC2LResponse, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(CreatePlayerC2LResponse)
	err := c.cc.Invoke(ctx, LoginService_CreatePlayer_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *loginServiceClient) EnterGame(ctx context.Context, in *EnterGameC2LRequest, opts ...grpc.CallOption) (*EnterGameC2LResponse, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(EnterGameC2LResponse)
	err := c.cc.Invoke(ctx, LoginService_EnterGame_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *loginServiceClient) LeaveGame(ctx context.Context, in *LeaveGameC2LRequest, opts ...grpc.CallOption) (*Empty, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(Empty)
	err := c.cc.Invoke(ctx, LoginService_LeaveGame_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

func (c *loginServiceClient) Disconnect(ctx context.Context, in *LoginNodeDisconnectRequest, opts ...grpc.CallOption) (*Empty, error) {
	cOpts := append([]grpc.CallOption{grpc.StaticMethod()}, opts...)
	out := new(Empty)
	err := c.cc.Invoke(ctx, LoginService_Disconnect_FullMethodName, in, out, cOpts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

// LoginServiceServer is the server API for LoginService service.
// All implementations must embed UnimplementedLoginServiceServer
// for forward compatibility.
type LoginServiceServer interface {
	Login(context.Context, *LoginC2LRequest) (*LoginC2LResponse, error)
	CreatePlayer(context.Context, *CreatePlayerC2LRequest) (*CreatePlayerC2LResponse, error)
	EnterGame(context.Context, *EnterGameC2LRequest) (*EnterGameC2LResponse, error)
	LeaveGame(context.Context, *LeaveGameC2LRequest) (*Empty, error)
	Disconnect(context.Context, *LoginNodeDisconnectRequest) (*Empty, error)
	mustEmbedUnimplementedLoginServiceServer()
}

// UnimplementedLoginServiceServer must be embedded to have
// forward compatible implementations.
//
// NOTE: this should be embedded by value instead of pointer to avoid a nil
// pointer dereference when methods are called.
type UnimplementedLoginServiceServer struct{}

func (UnimplementedLoginServiceServer) Login(context.Context, *LoginC2LRequest) (*LoginC2LResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Login not implemented")
}
func (UnimplementedLoginServiceServer) CreatePlayer(context.Context, *CreatePlayerC2LRequest) (*CreatePlayerC2LResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method CreatePlayer not implemented")
}
func (UnimplementedLoginServiceServer) EnterGame(context.Context, *EnterGameC2LRequest) (*EnterGameC2LResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method EnterGame not implemented")
}
func (UnimplementedLoginServiceServer) LeaveGame(context.Context, *LeaveGameC2LRequest) (*Empty, error) {
	return nil, status.Errorf(codes.Unimplemented, "method LeaveGame not implemented")
}
func (UnimplementedLoginServiceServer) Disconnect(context.Context, *LoginNodeDisconnectRequest) (*Empty, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Disconnect not implemented")
}
func (UnimplementedLoginServiceServer) mustEmbedUnimplementedLoginServiceServer() {}
func (UnimplementedLoginServiceServer) testEmbeddedByValue()                      {}

// UnsafeLoginServiceServer may be embedded to opt out of forward compatibility for this service.
// Use of this interface is not recommended, as added methods to LoginServiceServer will
// result in compilation errors.
type UnsafeLoginServiceServer interface {
	mustEmbedUnimplementedLoginServiceServer()
}

func RegisterLoginServiceServer(s grpc.ServiceRegistrar, srv LoginServiceServer) {
	// If the following call pancis, it indicates UnimplementedLoginServiceServer was
	// embedded by pointer and is nil.  This will cause panics if an
	// unimplemented method is ever invoked, so we test this at initialization
	// time to prevent it from happening at runtime later due to I/O.
	if t, ok := srv.(interface{ testEmbeddedByValue() }); ok {
		t.testEmbeddedByValue()
	}
	s.RegisterService(&LoginService_ServiceDesc, srv)
}

func _LoginService_Login_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(LoginC2LRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(LoginServiceServer).Login(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: LoginService_Login_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(LoginServiceServer).Login(ctx, req.(*LoginC2LRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _LoginService_CreatePlayer_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(CreatePlayerC2LRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(LoginServiceServer).CreatePlayer(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: LoginService_CreatePlayer_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(LoginServiceServer).CreatePlayer(ctx, req.(*CreatePlayerC2LRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _LoginService_EnterGame_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(EnterGameC2LRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(LoginServiceServer).EnterGame(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: LoginService_EnterGame_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(LoginServiceServer).EnterGame(ctx, req.(*EnterGameC2LRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _LoginService_LeaveGame_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(LeaveGameC2LRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(LoginServiceServer).LeaveGame(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: LoginService_LeaveGame_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(LoginServiceServer).LeaveGame(ctx, req.(*LeaveGameC2LRequest))
	}
	return interceptor(ctx, in, info, handler)
}

func _LoginService_Disconnect_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(LoginNodeDisconnectRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(LoginServiceServer).Disconnect(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: LoginService_Disconnect_FullMethodName,
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(LoginServiceServer).Disconnect(ctx, req.(*LoginNodeDisconnectRequest))
	}
	return interceptor(ctx, in, info, handler)
}

// LoginService_ServiceDesc is the grpc.ServiceDesc for LoginService service.
// It's only intended for direct use with grpc.RegisterService,
// and not to be introspected or modified (even as a copy)
var LoginService_ServiceDesc = grpc.ServiceDesc{
	ServiceName: "LoginService",
	HandlerType: (*LoginServiceServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "Login",
			Handler:    _LoginService_Login_Handler,
		},
		{
			MethodName: "CreatePlayer",
			Handler:    _LoginService_CreatePlayer_Handler,
		},
		{
			MethodName: "EnterGame",
			Handler:    _LoginService_EnterGame_Handler,
		},
		{
			MethodName: "LeaveGame",
			Handler:    _LoginService_LeaveGame_Handler,
		},
		{
			MethodName: "Disconnect",
			Handler:    _LoginService_Disconnect_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "common/login_service.proto",
}
