#include "room_selector.h"
#include <threading/registry_manager.h>
#include <node/constants/node_constants.h>
#include "modules/scene/comp/room_node_comp.h"

entt::entity RoomSelector::SelectRoomWithMinPlayers(const RoomRegistryComp& comp, uint32_t configId) {
	const auto& rooms = comp.GetRoomsByConfig(configId);
	if (rooms.empty()) return entt::null;

	entt::entity best = entt::null;
	std::size_t minPlayers = UINT64_MAX;

	for (auto room : rooms) {
		std::size_t playerCount = tlsRegistryManager.roomRegistry.get<RoomPlayers>(room).size();
		if (playerCount >= kMaxScenePlayerSize) continue;
		if (playerCount < minPlayers) {
			minPlayers = playerCount;
			best = room;
			if (playerCount == 0) return best;
		}
	}
	return best;
}
