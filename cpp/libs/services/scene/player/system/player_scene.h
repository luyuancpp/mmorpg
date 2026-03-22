#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "engine/core/type_define/type_define.h"

// Dispatcher that can drop messages under load (e.g. tips)

class PlayerSceneSystem
{
public:
	static void HandleEnterScene(entt::entity player, entt::entity scene);
	static void HandleBeforeLeaveScene(entt::entity player);

private:
	static void OnGetTeamInfo(entt::entity player, void* reply);
	static void OnGetLeaderLocation(entt::entity player, void* reply);
};
