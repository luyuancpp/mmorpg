#include "config_system.h"

#include "system/gs_scene_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	GameNodeSceneSystem::LoadAllMainSceneNavBin();
}

