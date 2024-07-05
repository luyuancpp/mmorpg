#include "config_system.h"

#include "system/scene/node_scene.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	GameNodeSceneSystem::LoadAllMainSceneNavBin();
}

