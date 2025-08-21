#include "storage.h"

thread_local ThreadLocalStorage tls;

ThreadLocalStorage::ThreadLocalStorage()
{
	Clear();
}

void ThreadLocalStorage::Clear()
{
	itemRegistry.clear();
	sessionRegistry.clear();
	nodeGlobalRegistry.clear();

	dispatcher.clear();

	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}


