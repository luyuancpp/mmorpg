#pragma once
#include "common/centre_service.pb.h"
class CentreServiceHandler : public ::CentreService
{
public:
	void RegisterGameNode(::google::protobuf::RpcController* controller,
		const ::RegisterGameNodeRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void RegisterGateNode(::google::protobuf::RpcController* controller,
		const ::RegisterGateNodeRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void GatePlayerService(::google::protobuf::RpcController* controller,
		const ::GateClientMessageRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void GateSessionDisconnect(::google::protobuf::RpcController* controller,
		const ::GateSessionDisconnectRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void LoginNodeLoginAccount(::google::protobuf::RpcController* controller,
		const ::LoginRequest* request,
		::LoginResponse* response,
		::google::protobuf::Closure* done)override;


	void LoginNodeEnterGame(::google::protobuf::RpcController* controller,
		const ::CentrePlayerGameNodeEntryRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void LoginNodeLeaveGame(::google::protobuf::RpcController* controller,
		const ::LoginNodeLeaveGameRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void LoginNodeDisconnect(::google::protobuf::RpcController* controller,
		const ::GateSessionDisconnectRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


	void GsPlayerService(::google::protobuf::RpcController* controller,
		const ::NodeRouteMessageRequest* request,
		::NodeRouteMessageResponse* response,
		::google::protobuf::Closure* done)override;


	void EnterGsSucceed(::google::protobuf::RpcController* controller,
		const ::EnterGameNodeSuccessRequest* request,
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


	void UnRegisterGameNode(::google::protobuf::RpcController* controller,
		const ::UnRegisterGameNodeRequest* request,
		::Empty* response,
		::google::protobuf::Closure* done)override;


};

