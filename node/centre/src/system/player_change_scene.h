#pragma once

#include "comp/player_scene.h"

//todo 测试换场景的时候服务器崩溃，队列首个换场景删除以后还能正常换场景
class PlayerChangeSceneSystem
{
public:
	static void InitChangeSceneQueue(entt::entity player);
	//队长拉人也得等待，不然太复杂了,队长拉进副本得等玩家继续切换次数切完了
	static uint32_t PushChangeSceneInfo(entt::entity player, const CentreChangeSceneInfo& change_info);
	static void ProcessChangeSceneQueue(entt::entity player);
	static void PopFrontChangeSceneQueue(entt::entity player);
	static void SetChangeGsStatus(entt::entity player, CentreChangeSceneInfo::eChangeGsStatus s);
	static void CopySceneInfoToChangeInfo(CentreChangeSceneInfo& change_info, const SceneInfo& scene_info);
private:
	static void ProcessSameGsChangeScene(entt::entity player, const CentreChangeSceneInfo& change_info);
	static void ProcessDifferentGsChangeScene(entt::entity player, const CentreChangeSceneInfo& change_info);
	static void OnEnterSceneOk(entt::entity player);
};