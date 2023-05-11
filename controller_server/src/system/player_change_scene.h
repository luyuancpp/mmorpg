#pragma once

#include "src/game_logic/comp/player_scene_comp.h"

//todo 测试换场景的时候服务器崩溃，队列首个换场景删除以后还能正常换场景
class PlayerChangeSceneSystem
{
public:
	static void InitChangeSceneQueue(entt::entity player);
	//队长拉人也得等待，不然太复杂了,队长拉进副本得等玩家继续切换次数切完了
	static uint32_t PushChangeSceneInfo(entt::entity player, const ControllerChangeSceneInfo& change_info);
	static void TryProcessChangeSceneQueue(entt::entity player);
	static void PopFrontChangeSceneQueue(entt::entity player);
	static void SetChangeGsStatus(entt::entity player, ControllerChangeSceneInfo::eChangeGsStatus s);
private:
	static void TryProcessZoneServerChangeScene(entt::entity player);//处理本服务器的切换,不跨服
	static void TryProcessViaCrossServerChangeScene(entt::entity player);//处理通过跨服消息的换场景
	static uint32_t TryChangeSameGsScene(entt::entity player);
	static uint32_t ChangeDiffGsScene(entt::entity player);//自己的服务器改变gs gs1的场景到gs2的场景
	static void OnEnterSceneOk(entt::entity player);
};