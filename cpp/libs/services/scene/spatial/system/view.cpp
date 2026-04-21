#include "view.h"

#include "grid.h"
#include "proto/common/component/base_comp.pb.h"
#include "Detour/DetourCommon.h"
#include "spatial/constants/view.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/npc_comp.pb.h"
#include "proto/common/component/player_comp.pb.h"
#include "combat/buff/comp/buff_comp.h"
#include "spatial/comp/scene_node_scene_comp.h"

#include "engine/core/type_define/type_define.h"
#include "network/player_message_utils.h"
#include "thread_context/ecs_context.h"

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
{
    return !BothAreNpcs(observer, entrant);
}

bool ViewSystem::BothAreNpcs(entt::entity observer, entt::entity entrant)
{
    return tlsEcs.actorRegistry.any_of<Npc>(observer) && tlsEcs.actorRegistry.any_of<Npc>(entrant);
}

double ViewSystem::GetMaxViewRadius(entt::entity observer)
{
    double viewRadius = kMaxViewRadius;

    if (const auto observerViewRadius = tlsEcs.actorRegistry.try_get<ViewRadius>(observer))
    {
        viewRadius = observerViewRadius->radius();
    }

    return viewRadius;
}

bool ViewSystem::IsWithinViewRadius(entt::entity viewer, entt::entity targetEntity, double visionRadius)
{
    const auto *viewerTransformPtr = tlsEcs.actorRegistry.try_get<Transform>(viewer);
    const auto *targetTransformPtr = tlsEcs.actorRegistry.try_get<Transform>(targetEntity);
    if (!viewerTransformPtr || !targetTransformPtr) return false;

    const auto &viewerTransform = *viewerTransformPtr;
    const auto &targetTransform = *targetTransformPtr;

    const dtReal viewerLocation[] = {
        viewerTransform.location().x(),
        viewerTransform.location().y(),
        viewerTransform.location().z()};
    const dtReal targetLocation[] = {
        targetTransform.location().x(),
        targetTransform.location().y(),
        targetTransform.location().z()};

    // Compute distance and check whether it's within view range
    return dtVdist(viewerLocation, targetLocation) <= visionRadius;
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer, entt::entity entrant)
{
    const double viewRadius = GetMaxViewRadius(observer);

    return IsWithinViewRadius(observer, entrant, viewRadius);
}

std::optional<double> ViewSystem::GetDistanceBetweenEntities(entt::entity entity1, entt::entity entity2)
{
    const auto *transform1 = tlsEcs.actorRegistry.try_get<Transform>(entity1);
    const auto *transform2 = tlsEcs.actorRegistry.try_get<Transform>(entity2);
    if (!transform1 || !transform2)
        return std::nullopt;

    const dtReal location1[] = {
        transform1->location().x(),
        transform1->location().y(),
        transform1->location().z()};
    const dtReal location2[] = {
        transform2->location().x(),
        transform2->location().y(),
        transform2->location().z()};

    return dtVdist(location1, location2);
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entrant, ActorCreateS2C &createMessage)
{
    createMessage.set_entity(entt::to_integral(entrant));

    const auto *entrantTransformPtr = tlsEcs.actorRegistry.try_get<Transform>(entrant);
    if (entrantTransformPtr)
    {
        createMessage.mutable_transform()->mutable_location()->CopyFrom(
            entrantTransformPtr->location());
    }

    if (tlsEcs.actorRegistry.any_of<Player>(entrant))
    {
        createMessage.set_actor_type(ACTOR_TYPE_PLAYER);
    }
    else if (tlsEcs.actorRegistry.any_of<Npc>(entrant))
    {
        createMessage.set_actor_type(ACTOR_TYPE_NPC);
    }

    const auto *guidPtr = tlsEcs.actorRegistry.try_get<Guid>(entrant);
    if (guidPtr)
    {
        createMessage.set_guid(*guidPtr);
    }
}

void ViewSystem::BroadcastMessageToVisiblePlayers(entt::entity entity, const uint32_t message_id,
                                                  const google::protobuf::Message &message)
{
    EntityUnorderedSet entities;
    GridSystem::GetEntitiesInViewAndNearby(entity, entities);
    BroadcastMessageToPlayers(message_id, message, entities);
}

void ViewSystem::LookAtPosition(entt::entity entity, const Vector3 &pos)
{
    auto *transformPtr = tlsEcs.actorRegistry.try_get<Transform>(entity);
    if (!transformPtr) return;
    auto &transform = *transformPtr;

    // Compute direction towards target
    dtReal targetLocation[] = {pos.x(), pos.y(), pos.z()};
    dtReal location[] = {transform.location().x(), transform.location().y(), transform.location().z()};
    dtReal direction[3] = {0, 0, 0};
    dtVsub(direction, targetLocation, location);

    // Zero-length direction means entity is already at target — nothing to face
    if (dtVlenSqr(direction) < 1e-12f)
    {
        return;
    }

    dtVnormalize(direction);

    // Compute rotation Euler angles (radians)
    float yaw = atan2(direction[0], direction[2]); // Rotation around Y axis
    float pitch = asin(direction[1]);              // Rotation around X axis

    // Update transform rotation
    transform.mutable_rotation()->set_x(pitch);
    transform.mutable_rotation()->set_y(yaw);
    transform.mutable_rotation()->set_z(0); // Keep Z rotation at 0
}

bool ViewSystem::IsStealthed(entt::entity entity)
{
    return tlsEcs.actorRegistry.any_of<StealthedTagComp>(entity);
}

bool ViewSystem::CanSee(entt::entity observer, entt::entity target)
{
    // Distance check first (cheapest).
    if (!IsWithinViewRadius(observer, target))
        return false;

    // Stealth check: stealthed targets are invisible unless observer
    // has a pinned interest in them (e.g. skill/buff forced visibility).
    if (IsStealthed(target))
    {
        // Check if observer has the target pinned in its interest list.
        const auto *aoiList = tlsEcs.actorRegistry.try_get<AoiListComp>(observer);
        if (aoiList == nullptr)
            return false;

        auto it = aoiList->entries.find(target);
        if (it == aoiList->entries.end() || it->second.priority < AoiPriority::kPinned)
        {
            return false;
        }
    }

    return true;
}
