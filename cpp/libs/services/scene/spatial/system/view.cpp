#include "view.h"

#include "grid.h"
#include "proto/common/component/comp.pb.h"
#include "Detour/DetourCommon.h"
#include "core/network/message_system.h"
#include "spatial/constants/view.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/npc_comp.pb.h"

#include "engine/core/type_define/type_define.h"
#include "network/player_message_utils.h"
#include <thread_context/registry_manager.h>

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
{
	return !BothAreNpcs(observer, entrant);
}

bool ViewSystem::BothAreNpcs(entt::entity observer, entt::entity entrant)
{
	return tlsRegistryManager.actorRegistry.any_of<Npc>(observer) && tlsRegistryManager.actorRegistry.any_of<Npc>(entrant);
}

double ViewSystem::GetMaxViewRadius(entt::entity observer)
{
	double viewRadius = kMaxViewRadius;

	if (const auto observerViewRadius = tlsRegistryManager.actorRegistry.try_get<ViewRadius>(observer)) {
		viewRadius = observerViewRadius->radius();
	}

	return viewRadius;
}

bool ViewSystem::IsWithinViewRadius(entt::entity viewer, entt::entity targetEntity, double visionRadius)
{
	const auto& viewerTransform = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(viewer);
	const auto& targetTransform = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(targetEntity);

	const dtReal viewerLocation[] = {
		viewerTransform.location().x(),
		viewerTransform.location().y(),
		viewerTransform.location().z()
	};
	const dtReal targetLocation[] = {
		targetTransform.location().x(),
		targetTransform.location().y(),
		targetTransform.location().z()
	};

	// Compute distance and check whether it's within view range
	return dtVdist(viewerLocation, targetLocation) <= visionRadius;
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer, entt::entity entrant)
{
	const double viewRadius = GetMaxViewRadius(observer);

	return IsWithinViewRadius(observer, entrant, viewRadius);
}

double ViewSystem::GetDistanceBetweenEntities(entt::entity entity1, entt::entity entity2)
{
	auto& transform1 = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(entity1);
	auto& transform2 = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(entity2);

	const dtReal location1[] = {
		transform1.location().x(),
		transform1.location().y(),
		transform1.location().z()
	};
	const dtReal location2[] = {
		transform2.location().x(),
		transform2.location().y(),
		transform2.location().z()
	};

	return dtVdist(location1, location2);
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entrant, ActorCreateS2C& createMessage)
{
	createMessage.set_entity(entt::to_integral(entrant));

	auto& entrantTransform = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(entrant);
	createMessage.mutable_transform()->CopyFrom(entrantTransform);
}

void ViewSystem::BroadcastMessageToVisiblePlayers(entt::entity entity, const uint32_t message_id,
	const google::protobuf::Message& message)
{
	EntityUnorderedSet entities;
	GridSystem::GetEntitiesInViewAndNearby(entity, entities);
	BroadcastMessageToPlayers(message_id, message, entities);
}

void ViewSystem::LookAtPosition(entt::entity entity, const Vector3& pos) {
    auto& transform = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(entity);

    // Compute direction towards target
    dtReal targetLocation[] = { pos.x(), pos.y(), pos.z() };
    dtReal location[] = { transform.location().x(), transform.location().y(), transform.location().z() };
    dtReal direction[3] = { 0, 0, 0 };
    dtVsub(direction, targetLocation, location);

    // Zero-length direction means entity is already at target — nothing to face
    if (dtVlenSqr(direction) < 1e-12f) {
        return;
    }

    dtVnormalize(direction);

    // Compute rotation Euler angles (radians)
    float yaw = atan2(direction[0], direction[2]);   // Rotation around Y axis
    float pitch = asin(direction[1]);                 // Rotation around X axis

    // Update transform rotation
    transform.mutable_rotation()->set_x(pitch);
    transform.mutable_rotation()->set_y(yaw);
    transform.mutable_rotation()->set_z(0); // Keep Z rotation at 0
}

