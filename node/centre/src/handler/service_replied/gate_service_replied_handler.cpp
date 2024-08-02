#include "gate_service_replied_handler.h"
#include "network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "game_logic/scene/util/player_change_scene.h"
#include "util/game_registry.h"
#include "game_logic/player/util/player_node_util.h"
#include "proto/logic/component/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitGateServiceRegisterGameRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceRegisterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceUnRegisterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RegisterSessionGameNodeResponse>(std::bind(&OnGateServicePlayerEnterGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServicePlayerMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceKickConnByCentreRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGateServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGateServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceBroadCast2PlayerMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGateServiceRegisterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceUnRegisterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServicePlayerEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RegisterSessionGameNodeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	///gate 更新gs,相应的gs可以往那个gate上发消息了
	///todo 中间返回是断开了
	entt::entity GetPlayerEntityBySessionId(uint64_t session_id);
	const auto player = GetPlayerEntityBySessionId(replied->session_info().session_id());
	if (entt::null == player)
	{
		LOG_ERROR << "session player not found " << replied->session_info().session_id();
		return;
	}
	PlayerNodeUtil::OnPlayerRegisteredToGateNode(player);
	PlayerChangeSceneSystem::SetChangeGsStatus(player, CentreChangeSceneInfo::eGateEnterGsSceneSucceed);
	PlayerChangeSceneSystem::ProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE
}

void OnGateServicePlayerMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceKickConnByCentreRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE



///<<< END WRITING YOUR CODE
}

void OnGateServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceBroadCast2PlayerMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

