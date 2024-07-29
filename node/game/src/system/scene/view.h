#pragma once
#include <entt/src/entt/entity/entity.hpp>

class ActorCreateS2C;

class ViewSystem
{
public:
    static void Initialize();
    //检测是否关系npc，比如身上没有npc 任务我就不优先关注npc了,或者多人副本,没有npc的副本
    static bool ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant);
    static bool ShouldSendPlayerEnterMessage(entt::entity observer, entt::entity entrant);
    static void FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage);
    static void HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver);
private:
    static void InitializeActorMessages();
	static bool BothAreNpcs(entt::entity observer, entt::entity entrant);
    static bool EntrantIsNpc(entt::entity entrant);
    static bool ShouldRefreshView();
    static double GetMaxViewRadius(entt::entity observer);
    static bool IsBeyondViewRadius(entt::entity observer, entt::entity entrant, double view_radius);
};
