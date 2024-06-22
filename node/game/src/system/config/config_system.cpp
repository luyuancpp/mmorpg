#include "config_system.h"

#include "system/game_node_scene_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	GameNodeSceneSystem::LoadAllMainSceneNavBin();
	GameNodeSceneSystem::InitNodeScene();
}

