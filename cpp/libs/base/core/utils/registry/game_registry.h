#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/signal/dispatcher.hpp"


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

namespace entt{
	[[nodiscard]] static constexpr entt::entity to_entity(uint64_t value)  {
		return entt::entity{ value };
	}
}//namespace entt

void Destroy(entt::registry& registry, entt::entity entity);
entt::entity TryCreateEntity(entt::registry& registry, entt::entity entity);
entt::entity ResetEntity(entt::registry& registry, entt::entity entity);


