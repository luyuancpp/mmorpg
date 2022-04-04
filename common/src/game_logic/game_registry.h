#ifndef COMMON_SRC_GAME_ECS_GAME_REGISTRY_H_
#define COMMON_SRC_GAME_ECS_GAME_REGISTRY_H_

#include "entt/src/entt/entity/registry.hpp"

namespace common
{
extern thread_local entt::registry reg;
entt::entity& error_entity();
entt::entity& op_enitty();

using EntityWeakPtr = std::weak_ptr<entt::entity>;
class EntityPtr
{
public:
	EntityPtr();
	inline entt::entity entity()const { return *entity_.get(); }
	EntityWeakPtr to_weak() { return entity_; }
private:
	std::shared_ptr<entt::entity> entity_;
};
}//namespace common

namespace entt
{
	[[nodiscard]] static constexpr entt::entity to_entity(uint64_t value) ENTT_NOEXCEPT {
		return static_cast<entt::entity>(value);
	}
}//namespace entt

#endif//COMMON_SRC_GAME_ECS_GAME_REGISTRY_H_
