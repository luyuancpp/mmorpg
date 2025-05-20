#include "storage.h"

thread_local ThreadLocalStorage tls;

void ThreadLocalStorage::Clear()
{
	globalRegistry.clear();
	registry.clear();
	sceneRegistry.clear();
	sceneNodeRegistry.clear();
	gateNodeRegistry.clear();
	itemRegistry.clear();
	sessionRegistry.clear();
	globalNodeRegistry.clear();
	
	for (auto& registry : nodeRegistries)
	{
		registry.clear();
	}

	dispatcher.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}
