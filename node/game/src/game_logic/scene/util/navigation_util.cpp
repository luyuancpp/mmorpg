#include "navigation_util.h"


#include "ue5navmesh/Public/Detour/DetourNavMesh.h"
#include "muduo/base/Logging.h"

#include "mainscene_config.h"

#include "DetourCrowd/DetourCrowd.h"
#include "game_node.h"
#include "game_logic/core/network/message_system.h"
#include "service_info/centre_scene_service_info.h"
#include "game_logic/scene/util/recast_util.h"
#include "scene/util/scene_util.h"
#include "thread_local/storage_game.h"
#include "logic/event/scene_event.pb.h"
#include "game_logic/scene/constants/nav_constants.h"

void NavigationSystem::LoadMainSceneNavBins() {
	auto& configAll = GetMainSceneAllTable();
	for (auto& item : configAll.data()) {
		auto navIt = tlsGame.sceneNav.emplace(item.id(), NavComp{});
		if (!navIt.second) {
			LOG_ERROR << "Failed to load scene navigation: " << item.id();
			continue;
		}
		auto& nav = navIt.first->second;
		RecastSystem::LoadNavMesh(item.nav_bin_file().c_str(), &nav.navMesh);
		nav.navQuery.init(&nav.navMesh, kMaxMeshQueryNodes);
	}
}
