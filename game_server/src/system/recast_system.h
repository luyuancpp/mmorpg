#pragma once

#include <memory>

#include "Detour/DetourNavMeshQuery.h"

class RecstSystem
{
public:
	static dtNavMesh* LoadNavMesh(const char* path);
};
