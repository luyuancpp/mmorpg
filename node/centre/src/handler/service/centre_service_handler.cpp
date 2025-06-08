
#include "centre_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "centre_node.h"
#include "common_error_tip.pb.h"
#include "mainscene_config.h"
#include "game_common_logic/system/session_system.h"
#include "service/player_service.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/InetAddress.h"
#include "network/message_system.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "network/constants/network_constants.h"
#include "network/system/error_handling_system.h"
#include "node/comp/game_node_comp.h"
#include "pbc/login_error_tip.pb.h"
#include "player/system/player_node_system.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "scene/system/player_change_scene_system.h"
#include "scene/system/scene_system.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "service_info/player_common_service_info.h"
#include "service_info/service_info.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_loading.h"
#include "type_alias/player_redis.h"
#include "type_alias/player_session_type_alias.h"
#include "util/defer.h"
#include "util/proto_field_checker.h"
#include "util/stacktrace_system.h"

using namespace muduo;
using namespace muduo::net;

constexpr std::size_t kMaxPlayerSize{50000};

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

Guid GetPlayerIDBySessionId(const uint64_t session_id)
{
	const auto session_it = tlsSessions.find(session_id);
	if (session_it == tlsSessions.end())
	{
		LOG_DEBUG << "Cannot find session ID " << session_id << GetStackTraceAsString();
		return kInvalidGuid;
	}
	return session_it->second.player_id();
}

entt::entity GetPlayerEntityBySessionId(uint64_t session_id)
{
	auto player_id = GetPlayerIDBySessionId(session_id);

	LOG_TRACE << "Getting player entity for session ID: " << session_id << ", player ID: " << player_id;

	const auto player_it = tlsCommonLogic.GetPlayerList().find(player_id);
	if (player_it == tlsCommonLogic.GetPlayerList().end())
	{
		LOG_ERROR << "Player not found for session ID: " << session_id << ", player ID: " << player_id;
		return entt::null;
	}

	LOG_TRACE << "Player entity found for session ID: " << session_id << ", player ID: " << player_id;

	return player_it->second;
}

///<<< END WRITING YOUR CODE


void CentreHandler::GatePlayerService(::google::protobuf::RpcController* controller,const ::GateClientMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::GateSessionDisconnect(::google::protobuf::RpcController* controller,const ::GateSessionDisconnectRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::LoginNodeAccountLogin(::google::protobuf::RpcController* controller,const ::CentreLoginRequest* request,
	::CentreLoginResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::LoginNodeEnterGame(::google::protobuf::RpcController* controller,const ::CentrePlayerGameNodeEntryRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::LoginNodeLeaveGame(::google::protobuf::RpcController* controller,const ::LoginNodeLeaveGameRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::LoginNodeSessionDisconnect(::google::protobuf::RpcController* controller,const ::GateSessionDisconnectRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::PlayerService(::google::protobuf::RpcController* controller,const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::EnterGsSucceed(::google::protobuf::RpcController* controller,const ::EnterGameNodeSuccessRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller,const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller,const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::InitSceneNode(::google::protobuf::RpcController* controller,const ::InitSceneNodeRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}




void CentreHandler::RegisterNodeSession(::google::protobuf::RpcController* controller,const ::RegisterNodeSessionRequest* request,
	::RegisterNodeSessionResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}



