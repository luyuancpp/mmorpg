#pragma once
#include "common/gate_service.pb.h"
class GateServiceHandler : public ::GateService
{
public:
	void RegisterGame(::google::protobuf::RpcController* controller,
		const ::RegisterGameRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void UnRegisterGame(::google::protobuf::RpcController* controller,
		const ::UnRegisterGameRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void PlayerEnterGs(::google::protobuf::RpcController* controller,
		const ::RegisterSessionGameNodeRequest* request,
		::RegisterSessionGameNodeResponse* response,
		 ::google::protobuf::Closure* done)override;

	void PlayerMessage(::google::protobuf::RpcController* controller,
		const ::NodeRouteMessageRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void KickConnByCentre(::google::protobuf::RpcController* controller,
		const ::GateNodeKickConnRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void RouteNodeStringMsg(::google::protobuf::RpcController* controller,
		const ::RouteMsgStringRequest* request,
		::RouteMsgStringResponse* response,
		 ::google::protobuf::Closure* done)override;

	void RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
		const ::RoutePlayerMsgStringRequest* request,
		::RoutePlayerMsgStringResponse* response,
		 ::google::protobuf::Closure* done)override;

	void BroadCast2PlayerMessage(::google::protobuf::RpcController* controller,
		const ::BroadCast2PlayerRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

};

