#include "scene_event_receiver.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE 

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/tips_id.h"
#include "src/game_logic/scene/scene.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_change_scene.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"

#include "component_proto/scene_comp.pb.h"
#include "logic_proto/scene_server_player.pb.h"
///<<< END WRITING YOUR CODE
void SceneEventReceiver::Register(entt::dispatcher& dispatcher)
{
    dispatcher.sink<BeforeEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
    dispatcher.sink<OnEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
    dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
    dispatcher.sink<OnLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
    dispatcher.sink<S2CEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive4>();
}

void SceneEventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
    dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
    dispatcher.sink<OnEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
    dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
    dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
    dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive4>();
}

void SceneEventReceiver::Receive0(const BeforeEnterScene& event_obj)
{
    ///<<< BEGIN WRITING YOUR CODE 
    ///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive1(const OnEnterScene& event_obj)
{
    ///<<< BEGIN WRITING YOUR CODE 
    entt::entity player = entt::to_entity(event_obj.entity());
    auto try_player_id = registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    PlayerSceneSystem::Send2GsEnterScene(player);
    LOG_INFO << "player enter scene " << *try_player_id << " "
        << registry.get<SceneInfo>(registry.get<SceneEntity>(player).scene_entity_).scene_id();
    ///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive2(const BeforeLeaveScene& event_obj)
{
    ///<<< BEGIN WRITING YOUR CODE 
    entt::entity player = entt::to_entity(event_obj.entity());
    auto try_player_id = registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    //LOG_INFO << "player leave scene " << *try_player_id << " " << registry.get<SceneInfo>(registry.get<SceneEntity>(player).scene_entity_).scene_id();
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
	auto& change_scene_info = change_scene_queue.front();
	auto to_scene = ScenesSystem::get_scene(change_scene_info.scene_info().scene_id());
    Controller2GsLeaveSceneRequest leave_scene_message;
	auto try_to_scene_gs = registry.try_get<GsNodePtr>(to_scene);
	auto p_player_gs = registry.try_get<PlayerSession>(player);
	if (nullptr == try_to_scene_gs || nullptr == p_player_gs)
	{
		LOG_ERROR << " scene null : " << (nullptr == try_to_scene_gs) << " " << (nullptr == p_player_gs);
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return ;
	}
    leave_scene_message.set_change_gs(p_player_gs->gs_node_id() != (*try_to_scene_gs)->node_id());
    Send2GsPlayer(leave_scene_message, player);
    ///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive3(const OnLeaveScene& event_obj)
{
    ///<<< BEGIN WRITING YOUR CODE 
    ///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive4(const S2CEnterScene& event_obj)
{
    ///<<< BEGIN WRITING YOUR CODE 
    entt::entity player = entt::to_entity(event_obj.entity());
    auto try_player_id = registry.try_get<Guid>(player);
    if (nullptr == try_player_id)
    {
        return;
    }
    PlayerSceneSystem::EnterSceneS2C(player);
    PlayerSceneSystem::TryEnterNextScene(player);
    ///<<< END WRITING YOUR CODE 
}

