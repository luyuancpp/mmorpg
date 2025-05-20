#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/signal/dispatcher.hpp"

entt::entity ErrorEntity();
entt::entity OperatorEntity();
entt::entity GlobalEntity();
entt::entity GetGlobalGrpcNodeEntity();

class EventOwner
{
public:
	inline  operator entt::entity() { return event_owner_; }
	inline operator entt::id_type() { return entt::to_integral(event_owner_); }
	void set_event_owner(entt::entity event_owner) { event_owner_ = event_owner; }
	inline entt::entity event_owner() const { return event_owner_; }
private:
	entt::entity event_owner_{ entt::null };
};

namespace entt
{
	[[nodiscard]] static constexpr entt::entity to_entity(uint64_t value)  {
		return entt::entity{ value };
	}
}//namespace entt

void Destroy(entt::registry& registry, entt::entity entity);

struct EntityDeleter {
	entt::registry& registry;

	explicit EntityDeleter(entt::registry& registry) : registry(registry) {}

	void operator()(const entt::entity* entity) const {
		if (entity) {
			Destroy(registry, *entity);
			delete entity;
		}
	}
};

using EntityPtr = std::shared_ptr<entt::entity>;

inline EntityPtr CreateEntityPtr(entt::registry& registry) {
	const entt::entity entity = registry.create();
	return EntityPtr(new entt::entity(entity), EntityDeleter(registry));
}