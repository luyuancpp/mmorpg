#include "config_system.h"

#include "system/game_scene_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	GameNodeSceneSystem::LoadAllMainSceneNavBin();
}

