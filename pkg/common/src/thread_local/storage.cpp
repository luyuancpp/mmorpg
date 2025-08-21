#include "storage.h"

thread_local ThreadLocalStorage tls;

ThreadLocalStorage::ThreadLocalStorage()
{
	Clear();
}

void ThreadLocalStorage::Clear()
{
	operatorEntity = entt::null;
	errorEntity = entt::null;
	globalEntity = entt::null;
}


