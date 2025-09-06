#pragma once

#include "engine/core/type_define/type_define.h"
#include "engine/core/node/constants/node_constants.h"

constexpr uint32_t kDefaultSceneId = 1;

struct GetRoomParams
{
	uint32_t roomConfigurationId{ kDefaultSceneId };
};

struct GetRoomFilterParam
{
	NodePressureState nodePressureState{ NodePressureState::kNoPressure };
};

class RoomNodeSelector
{
public:
	// Find a scene with the minimum player count among servers, less efficient
	static entt::entity SelectLeastLoadedRoom(const GetRoomParams& param);

	// Find a server with the scene where player count is not full, more efficient than the above function
	static entt::entity SelectAvailableRoom(const GetRoomParams& param);

};

