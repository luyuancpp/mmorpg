#include "config_system.h"

#include "game_logic/scene/util/navigation_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	NavigationSystem::LoadMainSceneNavBins();
}

