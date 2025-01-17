#include "navigation_system.h"


#include "ue5navmesh/Public/Detour/DetourNavMesh.h"
#include "muduo/base/Logging.h"
#include "mainscene_config.h"
#include "DetourCrowd/DetourCrowd.h"
#include "core/network/message_system.h"
#include "scene/system/recast_system.h"
#include "thread_local/storage_game.h"
#include "proto/logic/event/scene_event.pb.h"
#include "scene/constants/nav_constants.h"

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
