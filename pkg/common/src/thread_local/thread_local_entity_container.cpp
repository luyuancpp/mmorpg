#include "thread_local_entity_container.h"

thread_local ThreadLocalEntityContainer tlsThreadLocalEntityContainer;

ThreadLocalEntityContainer::ThreadLocalEntityContainer()
{
	Clear();
}

void ThreadLocalEntityContainer::Clear()
{
	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}


