#pragma once

#include <entt/src/entt/entity/entity.hpp>

#include "modules/scene/comp/player_scene.h"

class SceneInfoPBComponent;

class PlayerChangeSceneUtil {
public:
	static uint32_t PushChangeSceneInfo(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo);

	static void PopFrontChangeSceneQueue(entt::entity player);

	static void SetCurrentChangeSceneState(entt::entity player, ChangeSceneInfoPBComponent::eChangeSceneState s);

	static void CopySceneInfoToChangeInfo(ChangeSceneInfoPBComponent& changeInfo, const SceneInfoPBComponent& sceneInfo);

	static void ProgressSceneChangeState(entt::entity player);

	static void OnTargetSceneNodeEnterComplete(entt::entity player);
private:

	static void OnEnterSceneOk(entt::entity player);


private:
};