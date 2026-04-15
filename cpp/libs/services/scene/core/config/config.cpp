#include "config.h"

#include "spatial/system/navigation.h"

void ConfigSystem::OnConfigLoadSuccessful()
{
	NavigationSystem::LoadNavBins();
}
