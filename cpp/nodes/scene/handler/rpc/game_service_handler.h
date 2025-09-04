#pragma once
#include "proto/scene/game_service.pb.h"


class SceneHandler : public ::Scene
{
public:


	void PlayerEnterGameNode(::google::protobuf::RpcController* controller,  const ::PlayerEnterGameNodeRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void SendMessageToPlayer(::google::protobuf::RpcController* controller,  const ::NodeRouteMessageRequest* request, ::NodeRouteMessageResponse* response, ::google::protobuf::Closure* done) override;



	void ClientSendMessageToPlayer(::google::protobuf::RpcController* controller,  const ::ClientSendMessageToPlayerRequest* request, ::ClientSendMessageToPlayerResponse* response, ::google::protobuf::Closure* done) override;



	void CentreSendToPlayerViaGameNode(::google::protobuf::RpcController* controller,  const ::NodeRouteMessageRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void InvokePlayerService(::google::protobuf::RpcController* controller,  const ::NodeRouteMessageRequest* request, ::NodeRouteMessageResponse* response, ::google::protobuf::Closure* done) override;



	void RouteNodeStringMsg(::google::protobuf::RpcController* controller,  const ::RouteMessageRequest* request, ::RouteMessageResponse* response, ::google::protobuf::Closure* done) override;



	void RoutePlayerStringMsg(::google::protobuf::RpcController* controller,  const ::RoutePlayerMessageRequest* request, ::RoutePlayerMessageResponse* response, ::google::protobuf::Closure* done) override;



	void UpdateSessionDetail(::google::protobuf::RpcController* controller,  const ::RegisterPlayerSessionRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void EnterScene(::google::protobuf::RpcController* controller,  const ::Centre2GsEnterSceneRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void CreateScene(::google::protobuf::RpcController* controller,  const ::CreateSceneRequest* request, ::CreateSceneResponse* response, ::google::protobuf::Closure* done) override;



	void NodeHandshake(::google::protobuf::RpcController* controller,  const ::NodeHandshakeRequest* request, ::NodeHandshakeResponse* response, ::google::protobuf::Closure* done) override;


};