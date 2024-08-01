#include "config_system.h"

#include "game_logic/scene/system/game_node_scene_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	GameNodeSceneUtil::LoadAllMainSceneNavBin();
}

