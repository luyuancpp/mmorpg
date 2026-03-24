#pragma once

#include <entt/src/entt/entity/entity.hpp>

#include "modules/scene/comp/player_scene_comp.h"

class SceneInfoComp;

class PlayerChangeSceneUtil {
public:
	static uint32_t PushChangeSceneInfo(entt::entity player, const ChangeSceneInfoComp& changeInfo);

	static void PopFrontChangeSceneQueue(entt::entity player);

	static void SetCurrentChangeSceneState(entt::entity player, ChangeSceneInfoComp::eChangeSceneState s);

	static void CopySceneInfoToChangeInfo(ChangeSceneInfoComp& changeInfo, const SceneInfoComp& sceneInfo);

	static void ProgressSceneChangeState(entt::entity player);

	static void OnTargetSceneNodeEnterComplete(entt::entity player);
private:

	static void OnEnterSceneOk(entt::entity player);


private:
};