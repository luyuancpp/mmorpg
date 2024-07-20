#pragma once
#include "common/game_service.pb.h"
class GameServiceHandler : public ::GameService
{
public:
	void EnterGs(::google::protobuf::RpcController* controller,
		const ::GameNodeEnterGsRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void Send2Player(::google::protobuf::RpcController* controller,
		const ::NodeRouteMessageRequest* request,
		::NodeRouteMessageResponse* response,
		 ::google::protobuf::Closure* done)override;

	void ClientSend2Player(::google::protobuf::RpcController* controller,
		const ::GameNodeRpcClientRequest* request,
		::GameNodeRpcClientResponse* response,
		 ::google::protobuf::Closure* done)override;

	void Disconnect(::google::protobuf::RpcController* controller,
		const ::GameNodeDisconnectRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void RegisterGate(::google::protobuf::RpcController* controller,
		const ::RegisterGateRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void CentreSend2PlayerViaGs(::google::protobuf::RpcController* controller,
		const ::NodeRouteMessageRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void CallPlayer(::google::protobuf::RpcController* controller,
		const ::NodeRouteMessageRequest* request,
		::NodeRouteMessageResponse* response,
		 ::google::protobuf::Closure* done)override;

	void RouteNodeStringMsg(::google::protobuf::RpcController* controller,
		const ::RouteMsgStringRequest* request,
		::RouteMsgStringResponse* response,
		 ::google::protobuf::Closure* done)override;

	void RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
		const ::RoutePlayerMsgStringRequest* request,
		::RoutePlayerMsgStringResponse* response,
		 ::google::protobuf::Closure* done)override;

	void UpdateSession(::google::protobuf::RpcController* controller,
		const ::RegisterPlayerSessionRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void EnterScene(::google::protobuf::RpcController* controller,
		const ::Centre2GsEnterSceneRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void CreateScene(::google::protobuf::RpcController* controller,
		const ::CreateSceneRequest* request,
		::CreateSceneResponse* response,
		 ::google::protobuf::Closure* done)override;

};

