
#include "gate_service_replied_handler.h"

#include "service_info//gate_service_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "type_define/type_define.h"
#include "thread_local/storage.h"
#include "scene/system/player_change_scene_system.h"
#include "util/game_registry.h"
#include "player/system/player_node_system.h"
#include "proto/logic/component/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE



void InitGateServiceRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::RegisterGameNodeSessionResponse>(GateServicePlayerEnterGameNodeMessageId,
        std::bind(&OnGateServicePlayerEnterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GateServiceSendMessageToPlayerMessageId,
        std::bind(&OnGateServiceSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GateServiceKickSessionByCentreMessageId,
        std::bind(&OnGateServiceKickSessionByCentreRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(GateServiceRouteNodeMessageMessageId,
        std::bind(&OnGateServiceRouteNodeMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(GateServiceRoutePlayerMessageMessageId,
        std::bind(&OnGateServiceRoutePlayerMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GateServiceBroadcastToPlayersMessageId,
        std::bind(&OnGateServiceBroadcastToPlayersRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RegisterNodeSessionResponse>(GateServiceRegisterNodeSessionMessageId,
        std::bind(&OnGateServiceRegisterNodeSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGateServicePlayerEnterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterGameNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	///gate 更新gs,相应的gs可以往那个gate上发消息了
	///todo 中间返回是断开了
	entt::entity GetPlayerEntityBySessionId(uint64_t session_id);
	const auto player = GetPlayerEntityBySessionId(replied->session_info().session_id());
	if (entt::null == player)
	{
		LOG_TRACE << "session player not found " << replied->session_info().session_id();
		return;
	}
	
	PlayerNodeSystem::HandleGameNodePlayerRegisteredAtGateNode(player);

	PlayerNodeSystem::ProcessPlayerSessionState(player);

	PlayerChangeSceneUtil::SetChangeSceneNodeStatus(player, ChangeSceneInfoPBComponent::eGateEnterGsSceneSucceed);
	PlayerChangeSceneUtil::ProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE

}

void OnGateServiceSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceKickSessionByCentreRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceRouteNodeMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE



///<<< END WRITING YOUR CODE

}

void OnGateServiceRoutePlayerMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceBroadcastToPlayersRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceRegisterNodeSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
