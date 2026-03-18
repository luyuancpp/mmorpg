#include "entity_manager.h"
#include <thread_local_entity_container.h>
#include <registry_manager.h>

entt::entity ErrorEntity()
{
	if (tlsThreadLocalEntityContainer.errorEntity == entt::null)
	{
		tlsThreadLocalEntityContainer.errorEntity = tlsRegistryManager.globalRegistry.create();
	}
	return tlsThreadLocalEntityContainer.errorEntity;
}

entt::entity OperatorEntity()
{
	if (tlsThreadLocalEntityContainer.operatorEntity == entt::null)
	{
		tlsThreadLocalEntityContainer.operatorEntity = tlsRegistryManager.globalRegistry.create();
	}
	return tlsThreadLocalEntityContainer.operatorEntity;
}

entt::entity GlobalEntity()
{
	if (tlsThreadLocalEntityContainer.globalEntity == entt::null)
	{
		tlsThreadLocalEntityContainer.globalEntity = tlsRegistryManager.globalRegistry.create();
	}
	return tlsThreadLocalEntityContainer.globalEntity;
}

entt::entity GetGlobalGrpcNodeEntity()
{
	if (tlsThreadLocalEntityContainer.globalGrpcNodeEntity == entt::null)
	{
		tlsThreadLocalEntityContainer.globalGrpcNodeEntity = tlsRegistryManager.nodeGlobalRegistry.create();
	}
	return tlsThreadLocalEntityContainer.globalGrpcNodeEntity;
}