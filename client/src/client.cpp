#include "client.h"

#include <cstdint>

#include "src/game_logic/game_registry.h"

entt::registry::entity_type gAllFinish;

void PlayerClient::onDisConenction()
{
    auto& c = registry.get<uint32_t>(gAllFinish);
    --c;
    if (c == 0)
    {
        EventLoop::getEventLoopOfCurrentThread()->runInLoop(std::bind(&EventLoop::quit, EventLoop::getEventLoopOfCurrentThread()));
    }
}

