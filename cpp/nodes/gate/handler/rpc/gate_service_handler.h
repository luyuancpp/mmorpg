#pragma once
#include "proto/gate/gate_service.pb.h"

// done 由框架传入且非空:不碰它,handler 返回后框架立即回包;要等下游再回复,在返回前 RpcDoneCallback::Adopt(done)(network/rpc_done_callback.h)。
class GateHandler : public ::Gate
{
public:

	void SendMessageToPlayer(::google::protobuf::RpcController* controller,  const ::NodeRouteMessageRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

	void BroadcastToPlayers(::google::protobuf::RpcController* controller,  const ::BroadcastToPlayersRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

	void BroadcastToScene(::google::protobuf::RpcController* controller,  const ::BroadcastToSceneRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

	void BroadcastToAll(::google::protobuf::RpcController* controller,  const ::BroadcastToAllRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

	void NodeHandshake(::google::protobuf::RpcController* controller,  const ::NodeHandshakeRequest* request, ::NodeHandshakeResponse* response, ::google::protobuf::Closure* done) override;

	void GmGracefulShutdown(::google::protobuf::RpcController* controller,  const ::GmGracefulShutdownRequest* request, ::GmGracefulShutdownResponse* response, ::google::protobuf::Closure* done) override;

};