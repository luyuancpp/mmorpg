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
    networkRegistry.clear();
    globalNodeRegistry.clear();

    dispatcher.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}

