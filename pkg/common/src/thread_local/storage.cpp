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

	for (auto& registry : nodeRegistries)
	{
		registry.clear();
	}

	for (auto& e : nodeGlobalEntities)
	{
		e = entt::null;
	}

	dispatcher.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}

entt::entity ThreadLocalStorage::GetNodeGlobalEntity(uint32_t nodeType)
{
	auto& registry = GetNodeRegistry(nodeType);
	if (nodeGlobalEntities[nodeType] == entt::null)
	{
		nodeGlobalEntities[nodeType] = registry.create();
	}
	return nodeGlobalEntities[nodeType];
}

void ThreadLocalStorage::OnNodeStart(uint32_t nodeId)
{
	itemIdGenerator.set_node_id(nodeId);
}
