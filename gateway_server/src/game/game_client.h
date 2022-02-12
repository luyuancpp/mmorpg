#ifndef GATEWAY_SERVER_GAME_GAME_CLIENT_H_
#define GATEWAY_SERVER_GAME_GAME_CLIENT_H_

#include "entt/src/entt/entity/registry.hpp"

namespace gateway
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
}//namespace gateway

#endif//GATEWAY_SERVER_GAME_GAME_CLIENT_H_
