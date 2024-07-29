#include "view.h"

#include "constants/view.h"
#include "muduo/base/Logging.h"
#include "proto/logic/client_player/scene_client_player.pb.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "Detour/DetourCommon.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"
#include "type_define/type_define.h"

void ViewSystem::Initialize()
{
	// Initialize actor creation and destruction messages in the global registry
	initializeActorMessages();
}

void ViewSystem::initializeActorMessages()
{
	tls.globalRegistry.emplace<ActorCreateS2C>(global_entity());
	tls.globalRegistry.emplace<ActorDestroyS2C>(global_entity());
	tls.globalRegistry.emplace<ActorListCreateS2C>(global_entity());
	tls.globalRegistry.emplace<ActorListDestroyS2C>(global_entity());
}

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
{
	if (bothAreNpcs(observer, entrant)) {
		return false;
	}

	if (entrantIsNpc(entrant)) {
		return true;
	}

	// Handle cases where sudden loss of visibility requires refreshing view
	return shouldRefreshView();
}

bool ViewSystem::bothAreNpcs(entt::entity observer, entt::entity entrant)
{
	return tls.registry.any_of<Npc>(observer) && tls.registry.any_of<Npc>(entrant);
}

bool ViewSystem::entrantIsNpc(entt::entity entrant)
{
	return tls.registry.any_of<Npc>(entrant);
}

bool ViewSystem::shouldRefreshView()
{
	// TODO: Implement logic for when view needs refreshing
	return true;
}

bool ViewSystem::ShouldSendPlayerEnterMessage(entt::entity observer, entt::entity entrant)
{
	if (bothAreNpcs(observer, entrant)) {
		return false;
	}

	if (entrantIsNpc(entrant)) {
		return true;
	}

	if (shouldRefreshView()) {
		return false;
	}

	double view_radius = getMaxViewRadius(observer);

	if (isBeyondViewRadius(observer, entrant, view_radius)) {
		return false;
	}

	// TODO: Check priority focus list

	return true;
}

double ViewSystem::getMaxViewRadius(entt::entity observer)
{
	double view_radius = kMaxViewRadius;

	if (const auto observer_view_radius = tls.registry.try_get<ViewRadius>(observer)) {
		view_radius = observer_view_radius->radius();
	}

	return view_radius;
}

bool ViewSystem::isBeyondViewRadius(entt::entity observer, entt::entity entrant, double view_radius)
{
	const auto observer_transform = tls.registry.try_get<Transform>(observer);
	const auto entrant_transform = tls.registry.try_get<Transform>(entrant);

	if (!observer_transform || !entrant_transform) {
		return true; // Consider beyond radius if position information is missing
	}

	const dtReal observer_location[] = {
		observer_transform->location().x(),
		observer_transform->location().y(),
		observer_transform->location().z()
	};
	const dtReal entrant_location[] = {
		entrant_transform->location().x(),
		entrant_transform->location().y(),
		entrant_transform->location().z()
	};

	return dtVdist(observer_location, entrant_location) > view_radius;
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& createMessage)
{
	createMessage.set_entity(entt::to_integral(entity));

	if (const auto entrant_transform = tls.registry.try_get<Transform>(entity)) {
		createMessage.mutable_transform()->CopyFrom(*entrant_transform);
	}

	if (const auto guid = tls.registry.try_get<Guid>(entity)) {
		createMessage.set_guid(*guid);
	}
}

void ViewSystem::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
	// Placeholder for handling player leave message
	// Specific logic can be added based on requirements
}
