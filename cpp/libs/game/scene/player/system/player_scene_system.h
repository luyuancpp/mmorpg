#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "base/common/type_define/type_define.h"

//可以丢弃时间的派发器，比如提示,服务器压力大的时候可以把提示扔了

class PlayerSceneSystem
{
public:
	static void HandleEnterScene(entt::entity player, entt::entity scene);
	static void HandleBeforeLeaveScene(entt::entity player);
};
