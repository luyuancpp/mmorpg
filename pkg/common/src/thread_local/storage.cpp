#include "storage.h"

thread_local ThreadLocalStorage tls;

void ThreadLocalStorage::ClearForTest()
{
	registry.clear();
	sceneRegistry.clear();
    sceneNodeRegistry.clear();
    gateNodeRegistry.clear();
    centreNodeRegistry.clear();
    itemRegistry.clear();
    networkRegistry.clear();
    globalNodeRegistry.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}

