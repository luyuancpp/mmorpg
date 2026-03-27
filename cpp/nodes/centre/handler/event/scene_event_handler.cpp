#include "scene_event_handler.h"
#include "thread_context/dispatcher_manager.h"

void SceneEventHandler::Register()
{
    tlsEcs.dispatcher.sink<OnSceneCreated>().connect<&SceneEventHandler::OnSceneCreatedHandler>();
    tlsEcs.dispatcher.sink<OnSceneDestroyed>().connect<&SceneEventHandler::OnSceneDestroyedHandler>();
    tlsEcs.dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
    tlsEcs.dispatcher.sink<AfterEnterScene>().connect<&SceneEventHandler::AfterEnterSceneHandler>();
    tlsEcs.dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    tlsEcs.dispatcher.sink<AfterLeaveScene>().connect<&SceneEventHandler::AfterLeaveSceneHandler>();
    tlsEcs.dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<OnSceneCreated>().disconnect<&SceneEventHandler::OnSceneCreatedHandler>();
    tlsEcs.dispatcher.sink<OnSceneDestroyed>().disconnect<&SceneEventHandler::OnSceneDestroyedHandler>();
    tlsEcs.dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
    tlsEcs.dispatcher.sink<AfterEnterScene>().disconnect<&SceneEventHandler::AfterEnterSceneHandler>();
    tlsEcs.dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    tlsEcs.dispatcher.sink<AfterLeaveScene>().disconnect<&SceneEventHandler::AfterLeaveSceneHandler>();
    tlsEcs.dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
}
void SceneEventHandler::OnSceneCreatedHandler(const OnSceneCreated& event)
{

}
void SceneEventHandler::OnSceneDestroyedHandler(const OnSceneDestroyed& event)
{

}
void SceneEventHandler::BeforeEnterSceneHandler(const BeforeEnterScene& event)
{

}
void SceneEventHandler::AfterEnterSceneHandler(const AfterEnterScene& event)
{

}
void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& event)
{

}
void SceneEventHandler::AfterLeaveSceneHandler(const AfterLeaveScene& event)
{

}
void SceneEventHandler::S2CEnterSceneHandler(const S2CEnterScene& event)
{

}

