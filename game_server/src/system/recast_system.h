#pragma once

#include <memory>

#include "Detour/DetourNavMeshQuery.h"

class RecstSystem
{
public:
	static void LoadNavMesh(const char* path, dtNavMesh* mesh);
};
