#pragma once
#include "src/game_logic/thread_local/thread_local_storage.h"

class BeforeEnterScene;
class OnEnterScene;
class BeforeLeaveScene;
class OnLeaveScene;
class S2CEnterScene;

class SceneEventHandler
{
public:
	static void Register(entt::dispatcher& dispatcher)
	{
		dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
		dispatcher.sink<OnEnterScene>().connect<&SceneEventHandler::OnEnterSceneHandler>();
		dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		dispatcher.sink<OnLeaveScene>().connect<&SceneEventHandler::OnLeaveSceneHandler>();
		dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
	}

	static void UnRegister(entt::dispatcher& dispatcher)
	{
		dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
		dispatcher.sink<OnEnterScene>().disconnect<&SceneEventHandler::OnEnterSceneHandler>();
		dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventHandler::OnLeaveSceneHandler>();
		dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
	}

	static void BeforeEnterSceneHandler(const BeforeEnterScene& message);
	static void OnEnterSceneHandler(const OnEnterScene& message);
	static void BeforeLeaveSceneHandler(const BeforeLeaveScene& message);
	static void OnLeaveSceneHandler(const OnLeaveScene& message);
	static void S2CEnterSceneHandler(const S2CEnterScene& message);
};
