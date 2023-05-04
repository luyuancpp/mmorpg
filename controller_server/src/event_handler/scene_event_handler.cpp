#include "scene_event_handler.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE 

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/pb/pbc/serviceid/serverplayersceneservice_service_method_id.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_change_scene.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"

#include "component_proto/scene_comp.pb.h"
#include "logic_proto/scene_server_player.pb.h"
///<<< END WRITING YOUR CODE
void SceneEventHandler::Register(entt::dispatcher& dispatcher)
	{
		dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
		dispatcher.sink<OnEnterScene>().connect<&SceneEventHandler::OnEnterSceneHandler>();
		dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		dispatcher.sink<OnLeaveScene>().connect<&SceneEventHandler::OnLeaveSceneHandler>();
		dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
	}

void SceneEventHandler::UnRegister(entt::dispatcher& dispatcher)
	{
		dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
		dispatcher.sink<OnEnterScene>().disconnect<&SceneEventHandler::OnEnterSceneHandler>();
		dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventHandler::OnLeaveSceneHandler>();
		dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
	}

void SceneEventHandler::BeforeEnterSceneHandler(const BeforeEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::OnEnterSceneHandler(const OnEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE
    entt::entity player = entt::to_entity(event_obj.entity());
    auto try_player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    PlayerSceneSystem::Send2GsEnterScene(player);
    LOG_INFO << "player enter scene " << *try_player_id << " "
        << tls.registry.get<SceneInfo>(tls.registry.get<SceneEntity>(player).scene_entity_).scene_id();
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
///<<< BEGIN WRITING YOUR CODE
    entt::entity player = entt::to_entity(event_obj.entity());
    auto try_player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    //LOG_INFO << "player leave scene " << *try_player_id << " " << tls.registry.get<SceneInfo>(tls.registry.get<SceneEntity>(player).scene_entity_).scene_id();
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
	auto& change_scene_info = change_scene_queue.front();
	auto to_scene = ScenesSystem::get_scene(change_scene_info.scene_info().scene_id());
    Controller2GsLeaveSceneRequest leave_scene_message;
	auto try_to_scene_gs = tls.registry.try_get<GsNodePtr>(to_scene);
	auto p_player_gs = tls.registry.try_get<PlayerSession>(player);
	if (nullptr == try_to_scene_gs || nullptr == p_player_gs)
	{
		LOG_ERROR << " scene null : " << (nullptr == try_to_scene_gs) << " " << (nullptr == p_player_gs);
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return ;
	}
    leave_scene_message.set_change_gs(p_player_gs->gs_node_id() != (*try_to_scene_gs)->node_id());
    Send2GsPlayer(ServerPlayerSceneService_Id_LeaveSceneController2Gs, leave_scene_message, player);
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
    entt::entity player = entt::to_entity(event_obj.entity());
    auto try_player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    PlayerSceneSystem::EnterSceneS2C(player);
///<<< END WRITING YOUR CODE
}

