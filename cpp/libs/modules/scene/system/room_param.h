#pragma once

#include <entt/src/entt/entity/entity.hpp>
#include "proto/common/common.pb.h"

struct EnterDefaultRoomParam
{
	inline bool CheckValid() const
	{
		return enter != entt::null;
	}

	entt::entity enter{ entt::null };
};

struct CreateRoomOnNodeRoomParam
{
	inline bool CheckValid() const
	{
		return node != entt::null;
	}

	entt::entity node{ entt::null };
	RoomInfoPBComponent roomInfo;
};

struct CompelChangeRoomParam
{
	inline bool IsNull() const
	{
		return player == entt::null || destNode == entt::null;
	}

	entt::entity player{ entt::null };
	entt::entity destNode{ entt::null };
	uint32_t roomConfId{ 0 };
};