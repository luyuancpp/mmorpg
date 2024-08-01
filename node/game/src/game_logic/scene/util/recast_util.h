#pragma once

#include <memory>

#include "Detour/DetourNavMeshQuery.h"

class RecastUtil
{
public:
	static void LoadNavMesh(const char* path, dtNavMesh* mesh);
};
