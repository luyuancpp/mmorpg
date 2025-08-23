#include "config_system.h"

#include "scene/system/navigation_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	NavigationSystem::LoadMainSceneNavBins();
}

