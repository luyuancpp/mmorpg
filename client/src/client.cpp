#include "client.h"

#include <cstdint>

#include "src/game_logic/game_registry.h"

entt::registry::entity_type gAllFinish;

void PlayerClient::onDisConenction()
{
    auto& c = reg.get<uint32_t>(gAllFinish);
    --c;
    if (c == 0)
    {
        EventLoop::getEventLoopOfCurrentThread()->runAfter(2, std::bind(&EventLoop::quit, EventLoop::getEventLoopOfCurrentThread()));
    }
}

