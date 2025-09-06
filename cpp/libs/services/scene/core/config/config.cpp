#include "config.h"

#include "scene/scene/system/navigation_system.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	NavigationSystem::LoadMainSceneNavBins();
}

