#include "login_service_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void LoginServiceHandler::Login(::google::protobuf::RpcController* controller,
	const ::LoginRequest* request,
	::LoginResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::CreatPlayer(::google::protobuf::RpcController* controller,
	const ::CreatePlayerC2lRequest* request,
	::LoginNodeCreatePlayerResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::EnterGame(::google::protobuf::RpcController* controller,
	const ::EnterGameC2LRequest* request,
	::LoginNodeEnterGameResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::LeaveGame(::google::protobuf::RpcController* controller,
	const ::LeaveGameC2LRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::Disconnect(::google::protobuf::RpcController* controller,
	const ::LoginNodeDisconnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,
	const ::RouteMsgStringRequest* request,
	::RouteMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void LoginServiceHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,
	const ::RoutePlayerMsgStringRequest* request,
	::RoutePlayerMsgStringResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

