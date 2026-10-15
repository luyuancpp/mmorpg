#include "ecs_context.h"

thread_local EcsContext tlsEcs;

EcsContext::EcsContext()
{
    // 让 index 0 / version 0 这个句柄退役,见头文件里的不变量说明。
    actorRegistry.destroy(actorRegistry.create());
}

entt::entity EcsContext::GetPlayer(Guid playerUid)
{
    auto it = playerList.find(playerUid);
    if (it == playerList.end()) {
        return entt::null;
    }
    return it->second;
}

void EcsContext::Clear()
{
    actorRegistry.clear();
    sceneRegistry.clear();
    // 生产代码目前不用 enqueue,这里只是不让排队事件跨过一次 Clear 留到下一轮。
    dispatcher.clear();
    playerList.clear();
    sessionPlayers.clear();
}
