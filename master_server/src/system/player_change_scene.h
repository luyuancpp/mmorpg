#pragma once

#include "src/game_logic/comp/player_scene_comp.h"

class PlayerChangeSceneSystem
{
public:
	static void InitChangeSceneQueue(entt::entity player);
	static uint32_t ChangeScene(entt::entity player, MsChangeSceneInfo&& change_info);
	static uint32_t TryChangeSameGsScene(entt::entity player);
	static uint32_t Gs1SceneToGs2SceneInMyServer(entt::entity player);//自己的服务器改变gs
	static uint32_t TryPopFrontMsg(entt::entity player);	
	static void TryProcessChangeSceneQueue(entt::entity player);
};