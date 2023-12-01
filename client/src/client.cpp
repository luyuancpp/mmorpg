#include "client.h"

#include <cstdint>

#include "src/thread_local/thread_local_storage.h"

entt::registry::entity_type gAllFinish;

void PlayerClient::onDisConenction()
{
    client_.stop();
    auto& c = tls.registry.get<uint32_t>(gAllFinish);
    --c;
    if (c == 0)
    {
        EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&EventLoop::quit, EventLoop::getEventLoopOfCurrentThread()));
    }
}

