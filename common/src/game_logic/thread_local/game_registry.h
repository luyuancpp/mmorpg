#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "entt/src/entt/signal/dispatcher.hpp"

extern entt::registry registry;
extern entt::dispatcher dispatcher;
entt::entity& error_entity();
entt::entity& op_enitty();

using EntityWeakPtr = std::weak_ptr<entt::entity>;

class EntityPtr
{
public:
	EntityPtr();
	inline  operator entt::entity() { return *entity_.get(); }
	EntityWeakPtr to_weak() { return entity_; }
    bool is_null()const {return nullptr == entity_; }
private:
	std::shared_ptr<entt::entity> entity_;
};

class EventOwner
{
public:
	inline  operator entt::entity() { return event_owner_; }
	inline operator entt::id_type() { return entt::to_integral(event_owner_); }
	void set_event_owner(entt::entity event_owner) { event_owner_ = event_owner; }
	inline entt::entity event_owner() const { return event_owner_; };
private:
	entt::entity event_owner_{ entt::null };
};

namespace entt
{
	[[nodiscard]] static constexpr entt::entity to_entity(uint64_t value)  {
		return static_cast<entt::entity>(value);
	}
}//namespace entt

entt::entity& global_entity();

