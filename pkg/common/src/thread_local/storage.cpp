#include "storage.h"

thread_local ThreadLocalStorage tls;

ThreadLocalStorage::ThreadLocalStorage()
{
	Clear();
}

void ThreadLocalStorage::Clear()
{
	globalRegistry.clear();
	actorRegistry.clear();
	sceneRegistry.clear();
	itemRegistry.clear();
	sessionRegistry.clear();
	nodeGlobalRegistry.clear();

	dispatcher.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}


void ThreadLocalStorage::OnNodeStart(uint32_t nodeId)
{
	itemIdGenerator.set_node_id(nodeId);
}
