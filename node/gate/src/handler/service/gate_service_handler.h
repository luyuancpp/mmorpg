#pragma once
#include "proto/common/gate_service.pb.h"
class GateServiceHandler : public ::GateService
{
public:
	void RegisterGame(::google::protobuf::RpcController* controller,
		const ::RegisterGameNodeRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void UnRegisterGame(::google::protobuf::RpcController* controller,
		const ::UnRegisterGameNodeRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void PlayerEnterGameNode(::google::protobuf::RpcController* controller,
		const ::RegisterGameNodeSessionRequest* request,
		::RegisterGameNodeSessionResponse* response,
		::google::protobuf::Closure* done)override;


	void SendMessageToPlayer(::google::protobuf::RpcController* controller,
		const ::NodeRouteMessageRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void KickSessionByCentre(::google::protobuf::RpcController* controller,
		const ::KickSessionRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void RouteNodeMessage(::google::protobuf::RpcController* controller,
		const ::RouteMsgStringRequest* request,
		::RouteMsgStringResponse* response,
		::google::protobuf::Closure* done)override;


	void RoutePlayerMessage(::google::protobuf::RpcController* controller,
		const ::RoutePlayerMsgStringRequest* request,
		::RoutePlayerMsgStringResponse* response,
		::google::protobuf::Closure* done)override;


	void BroadcastToPlayers(::google::protobuf::RpcController* controller,
		const ::BroadcastToPlayersRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


};

