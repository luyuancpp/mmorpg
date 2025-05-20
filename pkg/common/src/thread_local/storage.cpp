#include "storage.h"

thread_local ThreadLocalStorage tls;

void ThreadLocalStorage::Clear()
{
	globalRegistry.clear();
	registry.clear();
	sceneRegistry.clear();
	sceneNodeRegistry.clear();
	gateNodeRegistry.clear();
	centreNodeRegistry.clear();
	itemRegistry.clear();
	sessionRegistry.clear();
	globalNodeRegistry.clear();
	nodeRegistries.clear();

	dispatcher.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}
