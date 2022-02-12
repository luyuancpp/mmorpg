#ifndef COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_
#define COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_

#include "entt/src/entt/entity/fwd.hpp"
 
namespace common
{
    using EntityPtr = std::shared_ptr<entt::entity>;
    using EntityWeakPtr = std::weak_ptr<entt::entity>;
    class EntityHandle
    {
    public:
        EntityHandle();
        inline entt::entity entity()const { return *entity_.get(); }
    private:
        EntityPtr entity_;
    };
}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_
