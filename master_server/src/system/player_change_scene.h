#pragma once

#include "src/game_logic/comp/player_scene_comp.h"

class PlayerChangeSceneSystem
{
public:
	static void InitChangeSceneQueue(entt::entity player);
	static uint32_t ChangeScene(entt::entity player, const MsChangeSceneInfo&& change_info);

};