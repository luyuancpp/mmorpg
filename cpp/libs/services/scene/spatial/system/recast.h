#pragma once

#include "Detour/DetourNavMeshQuery.h"

class RecastSystem
{
public:
	static void LoadNavMesh(const char* path, dtNavMesh* mesh);
};
