#pragma once

#include "entt/src/entt/entity/registry.hpp"

extern thread_local entt::registry registry;
entt::entity& error_entity();
entt::entity& op_enitty();

using EntityWeakPtr = std::weak_ptr<entt::entity>;

class EntityPtr
{
public:
	EntityPtr();
	inline  operator entt::entity() { return *entity_.get(); }
	EntityWeakPtr to_weak() { return entity_; }
private:
	std::shared_ptr<entt::entity> entity_;
};

namespace entt
{
	[[nodiscard]] static constexpr entt::entity to_entity(uint64_t value) ENTT_NOEXCEPT {
		return static_cast<entt::entity>(value);
	}
}//namespace entt

entt::entity& global_entity();

