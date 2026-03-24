#pragma once
#include "engine/core/type_define/type_define.h"

namespace google::protobuf
{
    class Message;
}

class ActorCreateS2C;
class Vector3;

class ViewSystem
{
public:
    // Check whether to send NPC-enter message (e.g., skip in multiplayer dungeons without NPC quests)
    static bool ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant);

    static void FillActorCreateMessageInfo(entt::entity observer, entt::entity entrant, ActorCreateS2C& createMessage);

    // Broadcast message to all players who can see this entity (including self)
    static void BroadcastMessageToVisiblePlayers(entt::entity entity, const uint32_t message_id,
    const google::protobuf::Message& message);

    static bool IsWithinViewRadius(entt::entity observer, entt::entity entrant, double view_radius);
    static bool IsWithinViewRadius(entt::entity observer, entt::entity entrant);

    static double GetDistanceBetweenEntities(entt::entity entity1, entt::entity entity2);
    
    static double GetMaxViewRadius(entt::entity observer);

    static void LookAtPosition(entt::entity entity, const Vector3& pos);
private:
    static bool BothAreNpcs(entt::entity observer, entt::entity entrant);
};

