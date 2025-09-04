#pragma once
#include "proto/gate/gate_service.pb.h"


class GateHandler : public ::Gate
{
public:


	void PlayerEnterGameNode(::google::protobuf::RpcController* controller,  const ::RegisterGameNodeSessionRequest* request, ::RegisterGameNodeSessionResponse* response, ::google::protobuf::Closure* done) override;



	void SendMessageToPlayer(::google::protobuf::RpcController* controller,  const ::NodeRouteMessageRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void KickSessionByCentre(::google::protobuf::RpcController* controller,  const ::KickSessionRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void RouteNodeMessage(::google::protobuf::RpcController* controller,  const ::RouteMessageRequest* request, ::RouteMessageResponse* response, ::google::protobuf::Closure* done) override;



	void RoutePlayerMessage(::google::protobuf::RpcController* controller,  const ::RoutePlayerMessageRequest* request, ::RoutePlayerMessageResponse* response, ::google::protobuf::Closure* done) override;



	void BroadcastToPlayers(::google::protobuf::RpcController* controller,  const ::BroadcastToPlayersRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;





	void Handshake(::google::protobuf::RpcController* controller,  const ::HandshakeRequest* request, ::HandshakeResponse* response, ::google::protobuf::Closure* done) override;


};