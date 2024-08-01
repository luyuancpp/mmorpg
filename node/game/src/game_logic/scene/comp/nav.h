#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Detour/DetourNavMeshQuery.h"

struct SceneNav
{
    dtNavMesh navMesh;
    dtNavMeshQuery navQuery;
};

using SceneNavs = std::unordered_map<uint32_t, SceneNav>;