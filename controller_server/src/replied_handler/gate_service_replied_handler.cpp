#include "gate_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

extern ProtobufDispatcher g_response_dispatcher;

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/session.h"
#include "src/system/player_common_system.h"
#include "src/system/player_change_scene.h"

extern entt::entity GetPlayerByConnId(uint64_t session_id);
///<<< END WRITING YOUR CODE

void InitGateServiceStartGSRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<GateNodePlayerEnterGsResponese>(std::bind(&OnGateServicePlayerEnterGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RouteMsgStringResponse>(std::bind(&OnGateServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<RoutePlayerMsgStringResponse>(std::bind(&OnGateServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGateServicePlayerEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GateNodePlayerEnterGsResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	 //gate 更新完gs，相应的gs可以往那个gate上发送消息了
    //todo 中间返回是断开了
    entt::entity player = GetPlayerByConnId(tcp_session_id(conn));
    if (entt::null == player)
    {
        LOG_ERROR << "player not found " << tls.registry.get<Guid>(player);
        return;
    }

    PlayerCommonSystem::OnEnterGateSucceed(player);

    PlayerChangeSceneSystem::SetChangeGsStatus(player, ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
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

