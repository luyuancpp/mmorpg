#include "navigation_util.h"


#include "ue5navmesh/Public/Detour/DetourNavMesh.h"
#include "muduo/base/Logging.h"

#include "mainscene_config.h"
#include "scene_config.h"

#include "DetourCrowd/DetourCrowd.h"
#include "game_node.h"
#include "game_logic/network/message_util.h"
#include "service/centre_scene_service.h"
#include "game_logic/player/util/player_util.h"
#include "game_logic/scene/util/recast_util.h"
#include "scene/util/scene_util.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "logic/event/scene_event.pb.h"
#include "game_logic/scene/comp/grid_comp.h"
#include "game_logic/scene/constants/nav_constants.h"
#include "scene/comp/scene_comp.h"

#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/constants/node.pb.h"

void NavigationUtil::LoadMainSceneNavBins() {
	auto& configAll = GetMainSceneAllTable();
	for (auto& item : configAll.data()) {
		auto navIt = tlsGame.sceneNav.emplace(item.id(), NavComp{});
		if (!navIt.second) {
			LOG_ERROR << "Failed to load scene navigation: " << item.id();
			continue;
		}
		auto& nav = navIt.first->second;
		RecastUtil::LoadNavMesh(item.nav_bin_file().c_str(), &nav.navMesh);
		nav.navQuery.init(&nav.navMesh, kMaxMeshQueryNodes);
	}
}
