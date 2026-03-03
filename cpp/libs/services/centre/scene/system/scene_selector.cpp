#include "scene_selector.h"
#include <threading/registry_manager.h>
#include <node/constants/node_constants.h>
#include "modules/scene/comp/scene_node_comp.h"

entt::entity SceneSelectorSystem::SelectSceneWithMinPlayers(const SceneRegistryComp& comp, uint32_t configId) {
	const auto& scenes = comp.GetScenesByConfig(configId);
	if (scenes.empty()) return entt::null;

	entt::entity best = entt::null;
	std::size_t minPlayers = UINT64_MAX;

	for (auto scene : scenes) {
		std::size_t playerCount = tlsRegistryManager.sceneRegistry.get<ScenePlayers>(scene).size();
		if (playerCount >= kMaxPlayersPerScene) continue;
		if (playerCount < minPlayers) {
			minPlayers = playerCount;
			best = scene;
			if (playerCount == 0) return best;
		}
	}
	return best;
}
