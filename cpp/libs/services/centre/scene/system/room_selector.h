#pragma once
#include <cstdint>

#include "entt/src/entt/entity/entity.hpp"

class RoomRegistryComp;

class RoomSelector {
public:
	static entt::entity SelectRoomWithMinPlayers(const RoomRegistryComp& comp, uint32_t configId);
};
