#include "client.h"

#include <cstdint>

#include "src/game_logic/game_registry.h"

entt::registry::entity_type gAllFinish;

void PlayerClient::onDisConenction()
{
    client_.stop();
    auto& c = registry.get<uint32_t>(gAllFinish);
    --c;
    if (c == 0)
    {
        EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&EventLoop::quit, EventLoop::getEventLoopOfCurrentThread()));
    }
}

