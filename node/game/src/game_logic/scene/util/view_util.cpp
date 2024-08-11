#include "view_util.h"

#include "grid_util.h"
#include "game_logic/scene/constants/view_constants.h"
#include "proto/logic/client_player/scene_player.pb.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "Detour/DetourCommon.h"
#include "thread_local/storage.h"
#include "type_define/type_define.h"
#include "game_logic/network/message_util.h"

void ViewUtil::Initialize()
{
	// Initialize actor creation and destruction messages in the global registry
	InitializeActorMessages();
}

void ViewUtil::InitializeActorMessages()
{
	tls.globalRegistry.emplace<ActorCreateS2C>(GlobalEntity());
	tls.globalRegistry.emplace<ActorDestroyS2C>(GlobalEntity());
	tls.globalRegistry.emplace<ActorListCreateS2C>(GlobalEntity());
	tls.globalRegistry.emplace<ActorListDestroyS2C>(GlobalEntity());
}

bool ViewUtil::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
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

bool ViewUtil::BothAreNpcs(entt::entity observer, entt::entity entrant)
{
	return tls.registry.any_of<Npc>(observer) && tls.registry.any_of<Npc>(entrant);
}

bool ViewUtil::EntrantIsNpc(entt::entity entrant)
{
	return tls.registry.any_of<Npc>(entrant);
}

bool ViewUtil::ShouldRefreshView()
{
	// TODO: Implement logic for when view needs refreshing
	return true;
}

bool ViewUtil::ShouldUpdateView(entt::entity observer, entt::entity entrant)
{
	if (BothAreNpcs(observer, entrant)) {
		return false;
	}

	if (EntrantIsNpc(entrant)) {
		return true;
	}

	// 如果需要刷新视图，返回false
	if (ShouldRefreshView()) {
		return false;
	}

	double viewRadius = GetMaxViewRadius(observer);

	if (IsWithinViewRadius(observer, entrant, viewRadius)) {
		return false;
	}

	// TODO: Check priority focus list

	return true;
}

double ViewUtil::GetMaxViewRadius(entt::entity observer)
{
	double viewRadius = kMaxViewRadius;

	if (const auto observerViewRadius = tls.registry.try_get<ViewRadius>(observer)) {
		viewRadius = observerViewRadius->radius();
	}

	return viewRadius;
}

bool ViewUtil::IsWithinViewRadius(entt::entity observer, entt::entity entrant, double viewRadius)
{
	const auto observerTransform = tls.registry.try_get<Transform>(observer);
	const auto entrantTransform = tls.registry.try_get<Transform>(entrant);

	if (!observerTransform || !entrantTransform) {
		return true; // Consider beyond radius if position information is missing
	}

	const dtReal observerLocation[] = {
		observerTransform->location().x(),
		observerTransform->location().y(),
		observerTransform->location().z()
	};
	const dtReal entrantLocation[] = {
		entrantTransform->location().x(),
		entrantTransform->location().y(),
		entrantTransform->location().z()
	};

	return dtVdist(observerLocation, entrantLocation) > viewRadius;
}

void ViewUtil::FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& createMessage)
{
	createMessage.set_entity(entt::to_integral(entity));

	if (const auto entrantTransform = tls.registry.try_get<Transform>(entity)) {
		createMessage.mutable_transform()->CopyFrom(*entrantTransform);
	}

	if (const auto guid = tls.registry.try_get<Guid>(entity)) {
		createMessage.set_guid(*guid);
	}
}

void ViewUtil::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
	// Placeholder for handling player leave message
	// Specific logic can be added based on requirements
}

void ViewUtil::BroadcastToNearbyEntities(entt::entity entity, const uint32_t message_id,
const google::protobuf::Message& message, bool excludingSel)
{
	EntityUnorderedSet entites;
	GridUtil::GetEntitiesInGridAndNeighbors(entity, entites, excludingSel);
	BroadCastToPlayer(entites, message_id, message);
}

void ViewUtil::BroadcastMessageToVisiblePlayers(entt::entity entity, const uint32_t message_id,
	const google::protobuf::Message& message)
{
	EntityUnorderedSet entites;
	GridUtil::GetEntitiesInViewAndNearby(entity, entites);
	BroadCastToPlayer(entites, message_id, message);
}
