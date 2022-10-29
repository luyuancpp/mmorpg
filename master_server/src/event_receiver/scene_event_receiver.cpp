#include "scene_event_receiver.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE 

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/system/player_scene_system.h"
#include "src/network/message_system.h"

#include "component_proto/scene_comp.pb.h"
#include "logic_proto/scene_server_player.pb.h"
///<<< END WRITING YOUR CODE
void SceneEventReceiver::Register(entt::dispatcher& dispatcher)
{
    dispatcher.sink<BeforeEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
    dispatcher.sink<OnEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
    dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
    dispatcher.sink<OnLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
}

void SceneEventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
    dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
    dispatcher.sink<OnEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
    dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
    dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
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
    LOG_INFO << "player leave scene " << *try_player_id << " "
        << registry.get<SceneInfo>(registry.get<SceneEntity>(player).scene_entity_).scene_id();
    Ms2GsLeaveSceneRequest leave_scene_message;
    leave_scene_message.set_change_gs(false);//error
    Send2GsPlayer(leave_scene_message, player);
    ///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive3(const OnLeaveScene& event_obj)
{
    ///<<< BEGIN WRITING YOUR CODE 
    ///<<< END WRITING YOUR CODE 
}

