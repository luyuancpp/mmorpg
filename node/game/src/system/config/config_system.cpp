#include "config_system.h"

#include "system/game_node_scene.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	GameNodeSceneSystem::LoadAllMainSceneNavBin();
	
}

