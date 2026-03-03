#pragma once

#include <entt/src/entt/entity/entity.hpp>

#include "modules/scene/comp/player_scene.h"

class SceneInfoPBComponent;

// PlayerChangeSceneSystem 类定义
class PlayerChangeSceneUtil {
public:
	// 添加切换场景信息到队列
	static uint32_t PushChangeSceneInfo(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo);

	// 移除队列中首个切换场景信息
	static void PopFrontChangeSceneQueue(entt::entity player);

	// 设置当前切换场景信息的切换状态
	static void SetCurrentChangeSceneState(entt::entity player, ChangeSceneInfoPBComponent::eChangeSceneState s);

	// 将场景信息复制到切换场景信息中
	static void CopySceneInfoToChangeInfo(ChangeSceneInfoPBComponent& changeInfo, const SceneInfoPBComponent& sceneInfo);

	// 处理玩家的场景切换队列
	static void ProgressSceneChangeState(entt::entity player);

	static void OnTargetSceneNodeEnterComplete(entt::entity player);
private:

	// 确认玩家成功进入场景后的操作
	static void OnEnterSceneOk(entt::entity player);


private:
	// 玩家场景切换队列结构
};