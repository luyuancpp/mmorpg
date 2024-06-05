#include "lobby_scene_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/thread_local/controller_thread_local_storage.h"
#include "src/system/player_scene_system.h"
#include "src/system/scene/scene_system.h"
#include "src/constants/tips_id.h"
#include "src/system/player_change_scene.h"
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitLobbyServiceStartCrossGsRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<StartCrossGsResponse>(std::bind(&OnLobbyServiceStartCrossGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnLobbyServiceStartControllerNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<EnterCrossMainSceneResponese>(std::bind(&OnLobbyServiceEnterCrossMainSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<EnterCrossRoomSceneSceneWeightRoundRobinResponse>(std::bind(&OnLobbyServiceEnterCrossMainSceneWeightRoundRobinRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnLobbyServiceLeaveCrossMainSceneRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<Empty>(std::bind(&OnLobbyServiceGameConnectToControllerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnLobbyServiceStartCrossGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartCrossGsResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLobbyServiceStartControllerNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLobbyServiceEnterCrossMainSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterCrossMainSceneResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
     //切跨到b服过程中，跨服没返回又切到c，跨服回来再到c目前就不考虑这种情况了，考虑的话写代码麻烦
    //todo 异步跨服返回来之前又去切换场景，导致已经切换到别的场景了，再切的话可能就不对了，不考虑这种情况了，正常人不会切那么快
    // todo 跨服切换不行，return error
    
	const auto player = ControllerPlayerSystem::GetPlayer(replied->player_id());
    if (entt::null == player)
    {
        LOG_ERROR << "player not found" << replied->player_id();
        return;
    }
    if (const auto scene = ScenesSystem::GetSceneByGuid(replied->scene_id());
	    entt::null == scene)
    {
        LOG_ERROR << "scene not found" << replied->scene_id();
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }
    PlayerChangeSceneSystem::SetChangeCrossServerSatus(player, ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
///<<< END WRITING YOUR CODE
}

void OnLobbyServiceEnterCrossMainSceneWeightRoundRobinRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterCrossRoomSceneSceneWeightRoundRobinResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLobbyServiceLeaveCrossMainSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLobbyServiceGameConnectToControllerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

