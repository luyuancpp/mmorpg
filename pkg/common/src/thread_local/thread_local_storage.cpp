#include "thread_local_storage.h"

thread_local ThreadLocalStorage tls;

void ThreadLocalStorage::ClearForTest()
{
	registry.clear();
	session_registry.clear();
	scene_registry.clear();
	op_entity = entt::null;
	error_entity = entt::null;
	global_entity = entt::null;
}
