#pragma once

#include <entt/src/entt/entity/entity.hpp>

#include "scene/comp/player_scene.h"

class SceneInfoPBComponent;

// PlayerChangeSceneSystem 类定义
class PlayerChangeSceneUtil {
public:
	// 添加切换场景信息到队列
	static uint32_t PushChangeSceneInfo(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo);

	// 移除队列中首个切换场景信息
	static void PopFrontChangeSceneQueue(entt::entity player);

	// 设置当前切换场景信息的切换状态
	static void SetChangeSceneNodeStatus(entt::entity player, ChangeSceneInfoPBComponent::eChangeGsStatus s);

	// 将场景信息复制到切换场景信息中
	static void CopySceneInfoToChangeInfo(ChangeSceneInfoPBComponent& changeInfo, const SceneInfoPBComponent& sceneInfo);

	// 处理玩家的场景切换队列
	static void ProcessChangeSceneQueue(entt::entity player);

private:
	// 处理同一游戏服务器内的场景切换
	static void ProcessSameGsChangeScene(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo);

	// 处理不同游戏服务器间的场景切换
	static void ProcessDifferentGsChangeScene(entt::entity player, const ChangeSceneInfoPBComponent& changeInfo);

	// 确认玩家成功进入场景后的操作
	static void OnEnterSceneOk(entt::entity player);

private:
	// 玩家场景切换队列结构
};