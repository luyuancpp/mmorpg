
#include "gate_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "network/network_constants.h"
#include "thread_local/storage_gate.h"

#include "proto/logic/component/player_network_comp.pb.h"

bool shouldLogProtocolErrorForDisconnectedPlayer(int message_id)
{
	// 在这里定义所有需要记录错误日志的有效 message_id
	return (message_id != 47 && message_id != 21 && message_id != 37);
}

///<<< END WRITING YOUR CODE


void GateHandler::PlayerEnterGameNode(::google::protobuf::RpcController* controller,const ::RegisterGameNodeSessionRequest* request,
	::RegisterGameNodeSessionResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void GateHandler::SendMessageToPlayer(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void GateHandler::KickSessionByCentre(::google::protobuf::RpcController* controller,const ::KickSessionRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void GateHandler::RouteNodeMessage(::google::protobuf::RpcController* controller,const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void GateHandler::RoutePlayerMessage(::google::protobuf::RpcController* controller,const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void GateHandler::BroadcastToPlayers(::google::protobuf::RpcController* controller,const ::BroadcastToPlayersRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void GateHandler::RegisterNodeSession(::google::protobuf::RpcController* controller,const ::RegisterNodeSessionRequest* request,
	::RegisterNodeSessionResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}



