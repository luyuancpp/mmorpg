#ifndef GATEWAY_SERVER_GAME_GAME_CLIENT_H_
#define GATEWAY_SERVER_GAME_GAME_CLIENT_H_

#include "entt/src/entt/entity/registry.hpp"

namespace gateway
{
    class GameClient : public entt::registry
    {
    public:
        static GameClient& GetSingleton() {
            static GameClient singleton;
            return singleton;
        }
    private:

    };
}//namespace gateway

#endif//GATEWAY_SERVER_GAME_GAME_CLIENT_H_
