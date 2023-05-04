#pragma once
#include "src/game_logic/thread_local/thread_local_storage.h"

class BeforeEnterScene;
class OnEnterScene;
class BeforeLeaveScene;
class OnLeaveScene;
class S2CEnterScene;

class SceneEventHandler
{
	static void Register(entt::dispatcher& dispatcher);
	static void UnRegister(entt::dispatcher& dispatcher);

	static void BeforeEnterSceneHandler(const BeforeEnterScene& message);
	static void OnEnterSceneHandler(const OnEnterScene& message);
	static void BeforeLeaveSceneHandler(const BeforeLeaveScene& message);
	static void OnLeaveSceneHandler(const OnLeaveScene& message);
	static void S2CEnterSceneHandler(const S2CEnterScene& message);
};
