#pragma once
#include "common_proto/centre_service.pb.h"
class CentreServiceHandler : public ::CentreService
{
public:
	void RegisterGame(::google::protobuf::RpcController* controller,
		const ::RegisterGameRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void RegisterGate(::google::protobuf::RpcController* controller,
		const ::RegisterGateRequest* request,
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

	void LsLoginAccount(::google::protobuf::RpcController* controller,
		const ::LoginRequest* request,
		::LoginResponse* response,
		 ::google::protobuf::Closure* done)override;

	void LsEnterGame(::google::protobuf::RpcController* controller,
		const ::EnterGameRequest* request,
		::EnterGameResponse* response,
		 ::google::protobuf::Closure* done)override;

	void LsLeaveGame(::google::protobuf::RpcController* controller,
		const ::CtrlLsLeaveGameRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void LsDisconnect(::google::protobuf::RpcController* controller,
		const ::CtrlLsDisconnectRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void GsPlayerService(::google::protobuf::RpcController* controller,
		const ::NodeRouteMessageRequest* request,
		::NodeRouteMessageResponse* response,
		 ::google::protobuf::Closure* done)override;

	void EnterGsSucceed(::google::protobuf::RpcController* controller,
		const ::EnterGameNodeSucceedRequest* request,
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

	void UnRegisterGame(::google::protobuf::RpcController* controller,
		const ::UnRegisterGameRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

};

