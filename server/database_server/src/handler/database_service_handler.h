#pragma once
#include "common_proto/database_service.pb.h"
class DbServiceHandler : public ::DbService
{
public:
	void Login(::google::protobuf::RpcController* controller,
		const ::DatabaseNodeLoginRequest* request,
		::DatabaseNodeLoginResponse* response,
		 ::google::protobuf::Closure* done)override;

	void CreatePlayer(::google::protobuf::RpcController* controller,
		const ::DatabaseNodeCreatePlayerRequest* request,
		::DatabaseNodeCreatePlayerResponse* response,
		 ::google::protobuf::Closure* done)override;

	void EnterGame(::google::protobuf::RpcController* controller,
		const ::DatabaseNodeEnterGameRequest* request,
		::DatabaseNodeEnterGameResponse* response,
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

