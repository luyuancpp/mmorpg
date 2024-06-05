#pragma once
#include "common_proto/controller_service.pb.h"
class ControllerServiceHandler : public ::ControllerService
{
public:
	void StartGs(::google::protobuf::RpcController* controller,
		const ::CtrlStartGsRequest* request,
		::CtrlStartGsResponse* response,
		 ::google::protobuf::Closure* done)override;

	void GateConnect(::google::protobuf::RpcController* controller,
		const ::GateConnectRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void GatePlayerService(::google::protobuf::RpcController* controller,
		const ::GateClientMessageRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void GateDisconnect(::google::protobuf::RpcController* controller,
		const ::GateDisconnectRequest* request,
		::Empty* response,
		 ::google::protobuf::Closure* done)override;

	void StartLs(::google::protobuf::RpcController* controller,
		const ::StartLsRequest* request,
		::StartLsResponse* response,
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

	void AddCrossServerScene(::google::protobuf::RpcController* controller,
		const ::AddCrossServerSceneRequest* request,
		::Empty* response,
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

};

