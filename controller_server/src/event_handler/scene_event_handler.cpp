#include "scene_event_handler.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE 

#include "component_proto/player_network_comp.pb.h"
#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene_system.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/pb/pbc/game_scene_server_player_service.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_change_scene.h"
#include "src/network/message_system.h"
#include "src/network/gs_node.h"

#include "component_proto/scene_comp.pb.h"
///<<< END WRITING YOUR CODE
void SceneEventHandler::Register()
{
		tls.dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
		tls.dispatcher.sink<OnEnterScene>().connect<&SceneEventHandler::OnEnterSceneHandler>();
		tls.dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		tls.dispatcher.sink<OnLeaveScene>().connect<&SceneEventHandler::OnLeaveSceneHandler>();
		tls.dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::UnRegister()
{
		tls.dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
		tls.dispatcher.sink<OnEnterScene>().disconnect<&SceneEventHandler::OnEnterSceneHandler>();
		tls.dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		tls.dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventHandler::OnLeaveSceneHandler>();
		tls.dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::BeforeEnterSceneHandler(const BeforeEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::OnEnterSceneHandler(const OnEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE
    entt::entity player = entt::to_entity(message.entity());
    auto try_player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    PlayerSceneSystem::Send2GsEnterScene(player);
    LOG_INFO << "player enter scene " << *try_player_id << " "
        << tls.registry.get<SceneInfo>(tls.registry.get<SceneEntity>(player).scene_entity_).guid();
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
///<<< BEGIN WRITING YOUR CODE
	auto player = entt::to_entity(message.entity());
    //LOG_INFO << "player leave scene " << *try_player_id << " " << tls.registry.get<SceneInfo>(tls.registry.get<SceneEntity>(player).scene_entity_).scene_id();
	auto* const try_change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
	if (nullptr == try_change_scene_queue)
	{
		return;
	}
	auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
	if (change_scene_queue.empty())
	{
		return;
	}
	const auto& change_scene_info = change_scene_queue.front();
	auto to_scene = ScenesSystem::GetSceneByGuid(change_scene_info.scene_info().guid());
	GsLeaveSceneRequest leave_scene_message;
	const auto try_to_scene_gs = tls.registry.try_get<GsNodePtr>(to_scene);
	const auto player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == try_to_scene_gs || nullptr == player_node_info)
	{
		LOG_ERROR << " scene null : " << (nullptr == try_to_scene_gs) << " " << (nullptr == player_node_info);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}
	leave_scene_message.set_change_gs(player_node_info->game_node_id() != (*try_to_scene_gs)->node_id());
	Send2GsPlayer(GamePlayerSceneServiceLeaveSceneMsgId, leave_scene_message, player);
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::OnLeaveSceneHandler(const OnLeaveScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::S2CEnterSceneHandler(const S2CEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE
    entt::entity player = entt::to_entity(message.entity());
    auto try_player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    PlayerSceneSystem::EnterSceneS2C(player);
///<<< END WRITING YOUR CODE
}

