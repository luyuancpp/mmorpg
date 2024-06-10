#include "scene_event_handler.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE 

#include "component_proto/player_network_comp.pb.h"
#include "muduo/base/Logging.h"

#include "src/type_define/type_define.h"
#include "src/comp/scene_comp.h"
#include "src/system/scene/scene_system.h"
#include "src/thread_local/thread_local_storage.h"
#include "service/game_scene_server_player_service.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_change_scene.h"
#include "src/network/message_system.h"
#include "src/network/game_node.h"

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
    auto player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == player_id)
    {
        return;
    }
    PlayerSceneSystem::Send2GsEnterScene(player);
    LOG_INFO << "player enter scene " << *player_id << " "
        << tls.registry.get<SceneInfo>(tls.registry.get<SceneEntity>(player).scene_entity_).guid();
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
///<<< BEGIN WRITING YOUR CODE
	const auto player = entt::to_entity(message.entity());
	//LOG_INFO << "player leave scene " << *try_player_id << " " << tls.registry.get<SceneInfo>(tls.registry.get<SceneEntity>(player).scene_entity_).scene_id();
	auto* const change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (nullptr == change_scene_queue)
	{
		return;
	}
	if (change_scene_queue->change_scene_queue_.empty())
	{
		return;
	}
	const auto& change_scene_info = change_scene_queue->change_scene_queue_.front();
	const auto dest_scene = entt::to_entity(change_scene_info.scene_info().guid());
	const auto* const dest_game_node = tls.registry.try_get<GameNodeClient>(dest_scene);
	const auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == dest_game_node || nullptr == player_node_info)
	{
		//todo
		LOG_ERROR << " scene null : " << (nullptr == dest_game_node) << " " << (nullptr == player_node_info);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}
	GsLeaveSceneRequest leave_scene_message;
	leave_scene_message.set_change_gs(player_node_info->game_node_id() != (*dest_game_node)->node_id());
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
    auto player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == player_id)
    {
        return;
    }
    PlayerSceneSystem::EnterSceneS2C(player);
///<<< END WRITING YOUR CODE
}

