#pragma once
#include <entt/src/entt/entity/entity.hpp>

class ActorCreateS2C;

class ViewSystem
{
public:
    static void Init();
    //检测是否关系npc，比如身上没有npc 任务我就不优先关注npc了,或者多人副本,没有npc的副本
    static bool CheckSendNpcEnterMessage(entt::entity observer, entt::entity entrant);
    static bool CheckSendPlayerEnterMessage(entt::entity observer, entt::entity entrant);
    static void FillActorCreateS2CInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage);
    static void HandlerPlayerLeaveMessage(entt::entity observer, entt::entity leaver);
};
