#pragma once

#include "Detour/DetourNavMeshQuery.h"

class RecstSystem
{
public:
	dtNavMesh* LoadNavMesh(const char* path);
};
