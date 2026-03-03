#pragma once

#include <entt/src/entt/entity/entity.hpp>
#include "proto/common/base/common.pb.h"

struct EnterDefaultSceneParam
{
	inline bool CheckValid() const
	{
		return enter != entt::null;
	}

	entt::entity enter{ entt::null };
};

struct CreateSceneOnNodeSceneParam
{
	inline bool CheckValid() const
	{
		return node != entt::null;
	}

	entt::entity node{ entt::null };
	SceneInfoPBComponent sceneInfo;
};

struct CompelChangeSceneParam
{
	inline bool IsNull() const
	{
		return player == entt::null || destNode == entt::null;
	}

	entt::entity player{ entt::null };
	entt::entity destNode{ entt::null };
	uint32_t sceneConfId{ 0 };
};