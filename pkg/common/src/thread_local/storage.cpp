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

std::string ThreadLocalStorage::GetRegistryName(const entt::registry& registry) const
{
	if (&registry == &globalRegistry) return "GlobalRegistry";
	if (&registry == &this->registry) return "Registry";
	if (&registry == &sceneRegistry) return "SceneRegistry";
	if (&registry == &sceneNodeRegistry) return "SceneNodeRegistry";
	if (&registry == &gateNodeRegistry) return "GateNodeRegistry";
	if (&registry == &centreNodeRegistry) return "CentreNodeRegistry";
	if (&registry == &itemRegistry) return "ItemRegistry";
	if (&registry == &networkRegistry) return "NetworkRegistry";
	if (&registry == &globalNodeRegistry) return "GlobalNodeRegistry";

	return "UnknownRegistry";
}
