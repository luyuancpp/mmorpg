#include "config.h"

#include "scene/scene/system/navigation.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	NavigationSystem::LoadMainSceneNavBins();
}

