#ifndef MASTER_SERVER_GAME_GAME_CLIENT_H_
#define MASTER_SERVER_GAME_GAME_CLIENT_H_

#include <memory>

#include "entt/src/entt/entity/registry.hpp"

#include "muduo/base/noncopyable.h"

namespace master
{
    class GameClient : muduo::noncopyable
    {
    public:
        using Type = entt::registry;
        using GameClientPtr = std::unique_ptr<Type>;

        static GameClientPtr& GetSingleton()
        {
            return singleton_;
        }

    private:
        static GameClientPtr singleton_;
    };
}//namespace master

#endif//MASTER_SERVER_GAME_GAME_CLIENT_H_
