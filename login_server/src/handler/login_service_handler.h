#pragma once
#include "common_proto/login_service.pb.h"
class LoginServiceHandler : public ::LoginService
{
public:
	void Login(::google::protobuf::RpcController* controller,
		const ::LoginRequest* request,
		::LoginResponse* response,
		 ::google::protobuf::Closure* done)override;

	void CreatPlayer(::google::protobuf::RpcController* controller,
		const ::CreatePlayerC2lRequest* request,
		::LoginNodeCreatePlayerResponse* response,
		 ::google::protobuf::Closure* done)override;

	void EnterGame(::google::protobuf::RpcController* controller,
		const ::EnterGameC2LRequest* request,
		::LoginNodeEnterGameResponse* response,
		 ::google::protobuf::Closure* done)override;

	void LeaveGame(::google::protobuf::RpcController* controller,
		const ::LeaveGameC2LRequest* request,
		::google::protobuf::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void Disconnect(::google::protobuf::RpcController* controller,
		const ::LoginNodeDisconnectRequest* request,
		::google::protobuf::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void RouteNodeStringMsg(::google::protobuf::RpcController* controller,
		const ::RouteMsgStringRequest* request,
		::RouteMsgStringResponse* response,
		 ::google::protobuf::Closure* done)override;

	void RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
		const ::RoutePlayerMsgStringRequest* request,
		::RoutePlayerMsgStringResponse* response,
		 ::google::protobuf::Closure* done)override;

};

