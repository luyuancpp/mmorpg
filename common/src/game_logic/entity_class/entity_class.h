#ifndef COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_
#define COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_

#include "entt/src/entt/entity/fwd.hpp"

namespace common
{
    class EntityHandle
    {
    public:
        using EntityType = std::shared_ptr<entt::entity>;
        EntityHandle();
        ~EntityHandle();
        EntityHandle(const EntityHandle& r);
        EntityHandle& operator=(const EntityHandle& r);
        inline entt::entity entity()const { return entity_; }
    private:
        
        entt::entity entity_;
    };
}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_ENTITY_CLASS_H_
