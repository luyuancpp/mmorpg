#include "gate_service_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void GateServiceHandler::StartGS(::google::protobuf::RpcController* controller,
	const ::GateNodeStartGSRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GateServiceHandler::StopGS(::google::protobuf::RpcController* controller,
	const ::GateNodeStopGSRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GateServiceHandler::PlayerEnterGs(::google::protobuf::RpcController* controller,
	const ::GateNodePlayerEnterGsRequest* request,
	::GateNodePlayerEnterGsResponese* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GateServiceHandler::PlayerMessage(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GateServiceHandler::KickConnByController(::google::protobuf::RpcController* controller,
	const ::GateNodeKickConnRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GateServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void GateServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

