
#include "game_service_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "common_error_tip.pb.h"
#include "muduo/net/InetAddress.h"

#include "scene_node.h"
#include "game_common_logic/system/session_system.h"
#include "core/network/message_system.h"
#include "player/system/player_node_system.h"
#include "player/system/player_scene_system.h"
#include "scene/system/game_node_scene_system.h"
#include "service/player_service.h"
#include "network/rpc_session.h"
#include "network/constants/network_constants.h"
#include "network/system/error_handling_system.h"
#include "proto/logic/component/player_async_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "scene/system/scene_system.h"
#include "service_info/service_info.h"
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage_game.h"
#include "type_alias/player_session_type_alias.h"
#include "util/proto_field_checker.h"

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

using namespace muduo::net;

///<<< END WRITING YOUR CODE


void SceneHandler::PlayerEnterGameNode(::google::protobuf::RpcController* controller,const ::PlayerEnterGameNodeRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::SendMessageToPlayer(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::ClientSendMessageToPlayer(::google::protobuf::RpcController* controller,const ::ClientSendMessageToPlayerRequest* request,
	::ClientSendMessageToPlayerResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::CentreSendToPlayerViaGameNode(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::InvokePlayerService(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::UpdateSessionDetail(::google::protobuf::RpcController* controller,const ::RegisterPlayerSessionRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::EnterScene(::google::protobuf::RpcController* controller,const ::Centre2GsEnterSceneRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::CreateScene(::google::protobuf::RpcController* controller,const ::CreateSceneRequest* request,
	::CreateSceneResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void SceneHandler::RegisterNodeSession(::google::protobuf::RpcController* controller,const ::RegisterNodeSessionRequest* request,
	::RegisterNodeSessionResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}



