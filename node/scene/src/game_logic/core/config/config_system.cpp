#include "config_system.h"

#include "game_logic/scene/system/navigation_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	NavigationSystem::LoadMainSceneNavBins();
}

