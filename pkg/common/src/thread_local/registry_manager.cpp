#include "registry_manager.h"

thread_local RegistryManager tlsManager;

RegistryManager::RegistryManager()
{
	Clear();
}

void RegistryManager::Clear()
{
	globalRegistry.clear();
	actorRegistry.clear();
	sceneRegistry.clear();
	itemRegistry.clear();
	sessionRegistry.clear();
	nodeGlobalRegistry.clear();
}
