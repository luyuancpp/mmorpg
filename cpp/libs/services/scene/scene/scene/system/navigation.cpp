#include "navigation.h"


#include "ue5navmesh/Public/Detour/DetourNavMesh.h"
#include "muduo/base/Logging.h"
#include "table/code/mainscene_table.h"
#include "DetourCrowd/DetourCrowd.h"
#include "core/network/message_system.h"
#include "scene/scene/system/recast.h"
#include "proto/common/event/scene_event.pb.h"
#include "scene/scene/constants/nav.h"
#include "scene/scene/mananger/scene_nav.h"

void NavigationSystem::LoadMainSceneNavBins() {
	auto& configAll = GetMainSceneAllTable();
	for (auto& item : configAll.data()) {
		NavComp nav;
		RecastSystem::LoadNavMesh(item.nav_bin_file().c_str(), &nav.navMesh);
		nav.navQuery.init(&nav.navMesh, kMaxMeshQueryNodes);
		SceneNavManager::Instance().AddNav(item.id(), NavComp{});
	}
}
