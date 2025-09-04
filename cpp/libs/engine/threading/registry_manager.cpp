#include "registry_manager.h"

thread_local RegistryManager tlsRegistryManager;

RegistryManager::RegistryManager()
{
	Clear();
}

void RegistryManager::Clear()
{
	globalRegistry.clear();
	actorRegistry.clear();
	roomRegistry.clear();
	itemRegistry.clear();
	nodeGlobalRegistry.clear();
}
