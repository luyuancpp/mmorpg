#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Detour/DetourNavMeshQuery.h"

struct SceneNav
{
	using DtNavMeshPtr = std::unique_ptr<dtNavMesh>;
	using DtNavMeshQueryPtr = std::unique_ptr<dtNavMeshQuery>;
	DtNavMeshPtr p_nav_;
	DtNavMeshQueryPtr p_nav_query_;
};

using SceneNavPtr = std::shared_ptr<SceneNav>;

using SceneNavs = std::unordered_map<uint32_t, SceneNavPtr>;
