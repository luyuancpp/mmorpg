#include "scene_event_handler.h"
#include "thread_context/dispatcher_manager.h"

void SceneEventHandler::Register()
{
    dispatcher.sink<OnSceneCreated>().connect<&SceneEventHandler::OnSceneCreatedHandler>();
    dispatcher.sink<OnSceneDestroyed>().connect<&SceneEventHandler::OnSceneDestroyedHandler>();
    dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
    dispatcher.sink<AfterEnterScene>().connect<&SceneEventHandler::AfterEnterSceneHandler>();
    dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    dispatcher.sink<AfterLeaveScene>().connect<&SceneEventHandler::AfterLeaveSceneHandler>();
    dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::UnRegister()
{
    dispatcher.sink<OnSceneCreated>().disconnect<&SceneEventHandler::OnSceneCreatedHandler>();
    dispatcher.sink<OnSceneDestroyed>().disconnect<&SceneEventHandler::OnSceneDestroyedHandler>();
    dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
    dispatcher.sink<AfterEnterScene>().disconnect<&SceneEventHandler::AfterEnterSceneHandler>();
    dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    dispatcher.sink<AfterLeaveScene>().disconnect<&SceneEventHandler::AfterLeaveSceneHandler>();
    dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
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

