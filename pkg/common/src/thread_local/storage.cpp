#include "storage.h"

thread_local ThreadLocalStorage tls;

void ThreadLocalStorage::ClearForTest()
{
	registry.clear();
	sceneRegistry.clear();
    gameNodeRegistry.clear();
    gateNodeRegistry.clear();
    centreNodeRegistry.clear();
    itemRegistry.clear();
    networkRegistry.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}

