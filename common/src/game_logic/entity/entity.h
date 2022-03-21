#ifndef COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_
#define COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_

#include "entt/src/entt/entity/fwd.hpp"
 
namespace common
{
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
}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_
