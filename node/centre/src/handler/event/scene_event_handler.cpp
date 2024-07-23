#include "scene_event_handler.h"
#include "logic/event/scene_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE 

#include "proto/logic/component/player_network_comp.pb.h"
#include "muduo/base/Logging.h"

#include "comp/scene.h"
#include "comp/game_node.h"
#include "system/network/message_system.h"
#include "service/game_scene_server_player_service.h"
#include "system/player_change_scene.h"
#include "system/player_scene.h"
#include "system/scene/scene_system.h"
#include "type_define/type_define.h"

#include "proto/logic/component/scene_comp.pb.h"
///<<< END WRITING YOUR CODE
void SceneEventHandler::Register()
{
		tls.dispatcher.sink<OnSceneCreate>().connect<&SceneEventHandler::OnSceneCreateHandler>();
		tls.dispatcher.sink<OnDestroyScene>().connect<&SceneEventHandler::OnDestroySceneHandler>();
		tls.dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
		tls.dispatcher.sink<AfterEnterScene>().connect<&SceneEventHandler::AfterEnterSceneHandler>();
		tls.dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		tls.dispatcher.sink<AfterLeaveScene>().connect<&SceneEventHandler::AfterLeaveSceneHandler>();
		tls.dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::UnRegister()
{
		tls.dispatcher.sink<OnSceneCreate>().disconnect<&SceneEventHandler::OnSceneCreateHandler>();
		tls.dispatcher.sink<OnDestroyScene>().disconnect<&SceneEventHandler::OnDestroySceneHandler>();
		tls.dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
		tls.dispatcher.sink<AfterEnterScene>().disconnect<&SceneEventHandler::AfterEnterSceneHandler>();
		tls.dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		tls.dispatcher.sink<AfterLeaveScene>().disconnect<&SceneEventHandler::AfterLeaveSceneHandler>();
		tls.dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::OnSceneCreateHandler(const OnSceneCreate& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::OnDestroySceneHandler(const OnDestroyScene& event)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
}

void SceneEventHandler::BeforeEnterSceneHandler(const BeforeEnterScene& event)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
}

void SceneEventHandler::AfterEnterSceneHandler(const AfterEnterScene& event)
{
///<<< BEGIN WRITING YOUR CODE
  
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& event)
{
///<<< BEGIN WRITING YOUR CODE
    const auto player = entt::to_entity(event.entity());
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
    GsLeaveSceneRequest request;
    request.set_change_gs(
        change_scene_info.change_gs_type() == CentreChangeSceneInfo::eDifferentGs);
    Send2GsPlayer(GamePlayerSceneServiceLeaveSceneMsgId, request, player);
    LOG_DEBUG << "player leave scene " << tls.registry.get<Guid>(player) 
        << " " << tls.scene_registry.get<SceneInfo>(tls.registry.get<SceneEntity>(player).scene_entity).guid();
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::AfterLeaveSceneHandler(const AfterLeaveScene& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::S2CEnterSceneHandler(const S2CEnterScene& event)
{
///<<< BEGIN WRITING YOUR CODE
    PlayerSceneSystem::Send2GsEnterScene(entt::to_entity(event.entity()));
///<<< END WRITING YOUR CODE
}
