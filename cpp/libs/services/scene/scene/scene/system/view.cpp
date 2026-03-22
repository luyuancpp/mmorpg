#include "view.h"

#include "grid.h"
#include "proto/common/component/comp.pb.h"
#include "Detour/DetourCommon.h"
#include "core/network/message_system.h"
#include "scene/scene/constants/view.h"
#include "proto/scene/player_scene.pb.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/component/npc_comp.pb.h"

#include "engine/core/type_define/type_define.h"
#include "network/player_message_utils.h"
#include <thread_context/registry_manager.h>
#include <thread_context/entity_manager.h>

void ViewSystem::Initialize()
{
	// Initialize actor creation and destruction messages in the global registry

}

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
{
	if (BothAreNpcs(observer, entrant)) {
		return false;
	}

	if (EntrantIsNpc(entrant)) {
		return true;
	}

	// Handle cases where sudden loss of visibility requires refreshing view
	return ShouldRefreshView();
}

bool ViewSystem::BothAreNpcs(entt::entity observer, entt::entity entrant)
{
	return tlsRegistryManager.actorRegistry.any_of<Npc>(observer) && tlsRegistryManager.actorRegistry.any_of<Npc>(entrant);
}

bool ViewSystem::EntrantIsNpc(entt::entity entrant)
{
	return tlsRegistryManager.actorRegistry.any_of<Npc>(entrant);
}

bool ViewSystem::ShouldRefreshView()
{
	// TODO: Implement logic for when view needs refreshing
	return true;
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

	// Get observer and target entity positions
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

	// Get positions of both entities
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

	// Compute and return the distance between entities
	return dtVdist(location1, location2);
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entrant, ActorCreateS2C& createMessage)
{
	createMessage.set_entity(entt::to_integral(entrant));

	auto& entrantTransform = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(entrant);
	createMessage.mutable_transform()->CopyFrom(entrantTransform);

	/*if (const auto guid = tlsThreadLocalEntityContainer.registry.try_get<Guid>(entrant)) {
		createMessage.set_guid(*guid);
	}*/
}

void ViewSystem::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
	// Placeholder for handling player leave message
	// Specific logic can be added based on requirements
}

void ViewSystem::BroadcastToNearbyEntities(entt::entity entity, const uint32_t message_id,
const google::protobuf::Message& message, bool excludingSel)
{
	EntityUnorderedSet entites;
	GridSystem::GetEntitiesInGridAndNeighbors(entity, entites, excludingSel);
	BroadcastMessageToPlayers(message_id, message, entites);
}

void ViewSystem::BroadcastMessageToVisiblePlayers(entt::entity entity, const uint32_t message_id,
	const google::protobuf::Message& message)
{
	EntityUnorderedSet entites;
	GridSystem::GetEntitiesInViewAndNearby(entity, entites);
	BroadcastMessageToPlayers(message_id, message, entites);
}

void ViewSystem::LookAtPosition(entt::entity entity, const Vector3& pos) {
    auto& transform = tlsRegistryManager.actorRegistry.get_or_emplace<Transform>(entity);

    // Compute direction towards target
    dtReal targetLocation[] = { pos.x(), pos.y(), pos.z() };
    dtReal location[] = { transform.location().x(), transform.location().y(), transform.location().z() };
    dtReal direction[3] = { 0, 0, 0 };
    dtVsub(direction, targetLocation, location);

    // Normalize the direction vector
    dtVnormalize(direction);

    // Validate direction vector
    if (direction[0] == 0.0 && direction[1] == 0.0 && direction[2] == 0.0) {
        return; // Zero-length direction; can't determine facing
    }

    // Compute rotation Euler angles (radians)
    float yaw = atan2(direction[0], direction[2]);   // Rotation around Y axis
    float pitch = asin(direction[1]);                 // Rotation around X axis

    // Update transform rotation
    transform.mutable_rotation()->set_x(pitch);
    transform.mutable_rotation()->set_y(yaw);
    transform.mutable_rotation()->set_z(0); // Z轴旋转保持为0
}

