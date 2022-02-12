#ifndef MASTER_SERVER_GAME_GAME_CLIENT_H_
#define MASTER_SERVER_GAME_GAME_CLIENT_H_

#include "entt/src/entt/entity/registry.hpp"

namespace master
{
    class SessionReg : public entt::registry
    {
    public:
        static SessionReg& GetSingleton() {
            thread_local SessionReg singleton;
            return singleton;
        }
    private:

    };
}//namespace master

#endif//MASTER_SERVER_GAME_GAME_CLIENT_H_
