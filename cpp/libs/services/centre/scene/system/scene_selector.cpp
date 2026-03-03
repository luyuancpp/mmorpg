#include "scene_selector.h"
#include <limits>
#include <threading/registry_manager.h>
#include "engine/core/node/constants/node_constants.h"
#include "modules/scene/comp/scene_node_comp.h"

entt::entity SceneSelectorSystem::SelectSceneWithMinPlayers(const SceneRegistryComp& comp, uint32_t configId) {
	const auto& scenes = comp.GetScenesByConfig(configId);
	if (scenes.empty()) return entt::null;

	entt::entity best = entt::null;
	std::size_t minPlayers = std::numeric_limits<std::size_t>::max();

	for (auto scene : scenes) {
		const auto& players = tlsRegistryManager.sceneRegistry.get<ScenePlayers>(scene);
		std::size_t playerCount = players.size();
		if (playerCount >= kMaxPlayersPerScene) continue;
		if (playerCount < minPlayers) {
			minPlayers = playerCount;
			best = scene;
			if (playerCount == 0) return best;
		}
	}
	return best;
}
