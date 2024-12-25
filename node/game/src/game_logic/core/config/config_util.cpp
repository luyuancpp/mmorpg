#include "config_util.h"

#include "game_logic/scene/util/navigation_util.h"

void ConfigUtil::OnConfigLoadSuccessful()
{
	NavigationSystem::LoadMainSceneNavBins();
}

