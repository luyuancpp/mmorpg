#ifndef MASTER_SERVER_GAME_GAME_CLIENT_H_
#define MASTER_SERVER_GAME_GAME_CLIENT_H_

#include "entt/src/entt/entity/registry.hpp"

namespace master
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
}//namespace master

#endif//MASTER_SERVER_GAME_GAME_CLIENT_H_
