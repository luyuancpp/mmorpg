#pragma once

#include "scene/comp/player_scene.h"

// PlayerChangeSceneSystem 类定义
class PlayerChangeSceneUtil {
public:
	// 初始化场景切换队列
	static void InitChangeSceneQueue(entt::entity player);

	// 添加切换场景信息到队列
	static uint32_t PushChangeSceneInfo(entt::entity player, const CentreChangeSceneInfoPBComp& changeInfo);

	// 移除队列中首个切换场景信息
	static void PopFrontChangeSceneQueue(entt::entity player);

	// 设置当前切换场景信息的切换状态
	static void SetChangeGsStatus(entt::entity player, CentreChangeSceneInfoPBComp::eChangeGsStatus s);

	// 将场景信息复制到切换场景信息中
	static void CopySceneInfoToChangeInfo(CentreChangeSceneInfoPBComp& changeInfo, const SceneInfoPBComp& sceneInfo);

	// 处理玩家的场景切换队列
	static void ProcessChangeSceneQueue(entt::entity player);

private:
	// 处理同一游戏服务器内的场景切换
	static void ProcessSameGsChangeScene(entt::entity player, const CentreChangeSceneInfoPBComp& changeInfo);

	// 处理不同游戏服务器间的场景切换
	static void ProcessDifferentGsChangeScene(entt::entity player, const CentreChangeSceneInfoPBComp& changeInfo);

	// 确认玩家成功进入场景后的操作
	static void OnEnterSceneOk(entt::entity player);

private:
	// 玩家场景切换队列结构
};