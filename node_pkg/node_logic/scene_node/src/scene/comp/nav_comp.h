#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Detour/DetourNavMeshQuery.h"

struct NavComp
{
    dtNavMesh navMesh;
    dtNavMeshQuery navQuery;
};

using SceneNavMapComp = std::unordered_map<uint32_t, NavComp>;