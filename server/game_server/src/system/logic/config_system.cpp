#include "config_system.h"

#include "src/system/gs_scene_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	GsSceneSystem::LoadAllMainSceneNavBin();
}

