#pragma once
#include "proto/gate/gate_service.pb.h"

class ProtobufCodec;

class GateHandler : public ::Gate
{
public:
	void SetCodec(ProtobufCodec* codec) { codec_ = codec; }
	ProtobufCodec& Codec() const { return *codec_; }

	void PlayerEnterGameNode(::google::protobuf::RpcController* controller,  const ::RegisterGameNodeSessionRequest* request, ::RegisterGameNodeSessionResponse* response, ::google::protobuf::Closure* done) override;

	void SendMessageToPlayer(::google::protobuf::RpcController* controller,  const ::NodeRouteMessageRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

	void RouteNodeMessage(::google::protobuf::RpcController* controller,  const ::RouteMessageRequest* request, ::RouteMessageResponse* response, ::google::protobuf::Closure* done) override;

	void RoutePlayerMessage(::google::protobuf::RpcController* controller,  const ::RoutePlayerMessageRequest* request, ::RoutePlayerMessageResponse* response, ::google::protobuf::Closure* done) override;

	void BroadcastToPlayers(::google::protobuf::RpcController* controller,  const ::BroadcastToPlayersRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

	void NodeHandshake(::google::protobuf::RpcController* controller,  const ::NodeHandshakeRequest* request, ::NodeHandshakeResponse* response, ::google::protobuf::Closure* done) override;

	void BindSessionToGate(::google::protobuf::RpcController* controller,  const ::BindSessionToGateRequest* request, ::BindSessionToGateResponse* response, ::google::protobuf::Closure* done) override;

private:
	ProtobufCodec* codec_ = nullptr;
};