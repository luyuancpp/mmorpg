#include "gate_service_replied_handler.h"
#include "network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "game_logic/scene/util/player_change_scene_util.h"
#include "util/game_registry.h"
#include "game_logic/player/util/player_node_util.h"
#include "proto/logic/component/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE

void InitGateServiceRegisterGameRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceRegisterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceUnRegisterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RegisterGameNodeSessionResponse>(std::bind(&OnGateServicePlayerEnterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceKickSessionByCentreRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGateServiceRouteNodeMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGateServiceRoutePlayerMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnGateServiceBroadcastToPlayersRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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

void OnGateServicePlayerEnterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RegisterGameNodeSessionResponse>& replied, Timestamp timestamp)
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
	
	PlayerNodeUtil::HandleGameNodePlayerRegisteredAtGateNode(player);

	PlayerNodeUtil::ProcessPlayerSessionState(player);

	PlayerChangeSceneUtil::SetChangeGsStatus(player, ChangeSceneInfoPBComponent::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE
}

void OnGateServiceSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceKickSessionByCentreRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceRouteNodeMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE



///<<< END WRITING YOUR CODE
}

void OnGateServiceRoutePlayerMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateServiceBroadcastToPlayersRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

