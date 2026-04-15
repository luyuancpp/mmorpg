#include "navigation.h"

#include "ue5navmesh/Public/Detour/DetourNavMesh.h"
#include "muduo/base/Logging.h"
#include "config/config.h"
#include "table/code/basescene_table.h"
#include "spatial/system/recast.h"
#include "spatial/constants/nav.h"
#include "spatial/manager/scene_nav.h"

void NavigationSystem::LoadNavBins()
{
	auto &configAll = FindAllBaseSceneTable();
	for (auto &item : configAll.data())
	{
		NavComp nav;
		const std::string navPath = GetDataRootDir() + item.nav_bin_file();
		RecastSystem::LoadNavMesh(navPath.c_str(), &nav.navMesh);
		nav.navQuery.init(&nav.navMesh, kMaxMeshQueryNodes);
		SceneNavManager::Instance().AddNav(item.id(), std::move(nav));
	}
}
