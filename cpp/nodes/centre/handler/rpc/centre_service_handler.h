#pragma once
#include "proto/service/cpp/rpc/centre/centre_service.pb.h"


class CentreHandler : public ::Centre
{
public:


	void GatePlayerService(::google::protobuf::RpcController* controller,  const ::GateClientMessageRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void GateSessionDisconnect(::google::protobuf::RpcController* controller,  const ::GateSessionDisconnectRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void LoginNodeAccountLogin(::google::protobuf::RpcController* controller,  const ::CentreLoginRequest* request, ::CentreLoginResponse* response, ::google::protobuf::Closure* done) override;



	void LoginNodeEnterGame(::google::protobuf::RpcController* controller,  const ::CentrePlayerGameNodeEntryRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void LoginNodeLeaveGame(::google::protobuf::RpcController* controller,  const ::LoginNodeLeaveGameRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void LoginNodeSessionDisconnect(::google::protobuf::RpcController* controller,  const ::GateSessionDisconnectRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void PlayerService(::google::protobuf::RpcController* controller,  const ::NodeRouteMessageRequest* request, ::NodeRouteMessageResponse* response, ::google::protobuf::Closure* done) override;



	void EnterGsSucceed(::google::protobuf::RpcController* controller,  const ::EnterGameNodeSuccessRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void RouteNodeStringMsg(::google::protobuf::RpcController* controller,  const ::RouteMessageRequest* request, ::RouteMessageResponse* response, ::google::protobuf::Closure* done) override;



	void RoutePlayerStringMsg(::google::protobuf::RpcController* controller,  const ::RoutePlayerMessageRequest* request, ::RoutePlayerMessageResponse* response, ::google::protobuf::Closure* done) override;



	void InitSceneNode(::google::protobuf::RpcController* controller,  const ::InitSceneNodeRequest* request, ::Empty* response, ::google::protobuf::Closure* done) override;



	void NodeHandshake(::google::protobuf::RpcController* controller,  const ::NodeHandshakeRequest* request, ::NodeHandshakeResponse* response, ::google::protobuf::Closure* done) override;


};