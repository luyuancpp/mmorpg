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

	op_entity = entt::null;
	error_entity = entt::null;
	global_entity = entt::null;
}


//to check 
void Destroy(entt::registry& registry, entt::entity entity)
{
    if (!registry.valid(entity))
    {
        return;
    }
    registry.destroy(entity);
}
