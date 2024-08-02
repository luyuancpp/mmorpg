#include "config_util.h"

#include "game_logic/scene/system/game_node_scene_system.h"

void ConfigUtil::OnConfigLoadSuccessful()
{
	GameNodeSceneUtil::LoadAllMainSceneNavBin();
}

