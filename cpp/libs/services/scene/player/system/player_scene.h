#pragma once

#include "entt/src/entt/entity/entity.hpp"

#include "engine/core/type_define/type_define.h"

class PlayerSceneSystem
{
public:
	static void HandleEnterScene(entt::entity player, entt::entity scene);

private:
	static void OnGetTeamInfo(entt::entity player, void* reply);
	static void OnGetLeaderLocation(entt::entity player, void* reply);
};
