#include "aoi_system.h"

#include "game_logic/scene/util/view_util.h"
#include "scene/comp/scene_comp.h"
#include "game_logic/scene/comp/grid_comp.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"
#include "muduo/base/Logging.h"
#include "game_logic/network/message_util.h"
#include "service_info/player_scene_service_info.h"
#include "hexagons_grid.h"
#include "game_logic/scene/util/grid_util.h"
#include "game_logic/scene/util/interest_util.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"

void AoiSystem::Update(double delta) {
	GridSet gridsToEnter, gridsToLeave;
	EntityUnorderedSet entitiesEnteringMyView, entitiesIEnterViewOf;

	for (auto&& [entity, transform, sceneComponent] : tls.registry.view<Transform, SceneEntityComp>().each()) {

		if (!tls.sceneRegistry.valid(sceneComponent.sceneEntity)) {
			LOG_ERROR << "Scene not found for entity " << tls.registry.get<Guid>(entity);
			continue;
		}

		gridsToEnter.clear();
		gridsToLeave.clear();
		entitiesEnteringMyView.clear();
		entitiesIEnterViewOf.clear();

		auto& gridList = tls.sceneRegistry.get<SceneGridListComp>(sceneComponent.sceneEntity);
		const auto currentHexPosition = GridUtil::CalculateHexPosition(transform);
		const auto currentGridId = GridUtil::GetGridId(currentHexPosition);

		if (!tls.registry.any_of<Hex>(entity)) {
			gridList[currentGridId].entity_list.emplace(entity);
			tls.registry.emplace<Hex>(entity, currentHexPosition);

			GridUtil::GetCurrentAndNeighborGridIds(currentHexPosition, gridsToEnter);
		}
		else {
			const auto previousHexPosition = tls.registry.get<Hex>(entity);
			const auto distance = hex_distance(previousHexPosition, currentHexPosition);
			if (distance == 0) {
				continue;
			}

			GridUtil::GetCurrentAndNeighborGridIds(previousHexPosition, gridsToLeave);
			GridUtil::GetCurrentAndNeighborGridIds(currentHexPosition, gridsToEnter);

			for (const auto& grid : gridsToEnter) {
				gridsToLeave.erase(grid);
			}
			for (const auto& grid : gridsToLeave) {
				gridsToEnter.erase(grid);
			}

			const auto previousGridId = GridUtil::GetGridId(previousHexPosition);
			gridList[previousGridId].entity_list.erase(entity);

			gridList[currentGridId].entity_list.emplace(entity);

			tls.registry.remove<Hex>(entity);
			tls.registry.emplace<Hex>(entity, currentHexPosition);
		}

		actorCreateMessage.Clear();
		actorListCreateMessage.Clear();

		for (const auto& gridId : gridsToEnter) {
			auto gridIt = gridList.find(gridId);
			if (gridIt == gridList.end()) {
				continue;
			}

			for (const auto& viewEntrant : gridIt->second.entity_list) {
				if (viewEntrant == entity || 
					!tls.registry.any_of<Npc>(viewEntrant) || 
					!ViewUtil::ShouldSendNpcEnterMessage(entity, viewEntrant)) {
					continue;
				}

				ViewUtil::FillActorCreateMessageInfo(entity, viewEntrant, actorCreateMessage);
				InterestUtil::AddAoiEntity(entity, viewEntrant);
			}

			for (const auto& viewEntrant : gridIt->second.entity_list) {
				if (viewEntrant == entity || tls.registry.any_of<Npc>(entity)) {
					continue;
				}

				if (ViewUtil::ShouldUpdateView(viewEntrant, entity)) {
					ViewUtil::FillActorCreateMessageInfo(viewEntrant, entity, actorCreateMessage);
					entitiesIEnterViewOf.emplace(viewEntrant);
					InterestUtil::AddAoiEntity(viewEntrant, entity);
				}

				if (ViewUtil::ShouldUpdateView(entity, viewEntrant)) {
					ViewUtil::FillActorCreateMessageInfo(entity, viewEntrant, *actorListCreateMessage.add_actor_list());
					entitiesEnteringMyView.emplace(viewEntrant);
					InterestUtil::AddAoiEntity(entity, viewEntrant);
				}
			}
		}

		if (!actorListCreateMessage.actor_list().empty()) {
			SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListCreateMessageId, actorListCreateMessage, entity);
		}

		if (actorCreateMessage.entity() > 0) {
			BroadCastToPlayer(ClientPlayerSceneServiceNotifyActorCreateMessageId, actorCreateMessage, entitiesIEnterViewOf);
		}

		BroadCastLeaveGridMessage(gridList, entity, gridsToLeave);
	}
}


void AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message) {
	const auto entity = entt::to_entity(message.entity());

	if (!tls.registry.valid(entity)) {
		LOG_ERROR << "Entity not found in scene";
		return;
	}

	const auto sceneComponent = tls.registry.try_get<SceneEntityComp>(entity);
	if (!sceneComponent) {
		return;
	}

	const auto hexPosition = tls.registry.try_get<Hex>(entity);
	if (!hexPosition) {
		return;
	}

	auto& gridList = tls.sceneRegistry.get<SceneGridListComp>(sceneComponent->sceneEntity);
	GridSet gridsToLeave;
	GridUtil::GetCurrentAndNeighborGridIds(*hexPosition, gridsToLeave);

	LeaveGrid(*hexPosition, gridList, entity);
	BroadCastLeaveGridMessage(gridList, entity, gridsToLeave);
}


void AoiSystem::LeaveGrid(const Hex& hex, SceneGridListComp& gridList, entt::entity entity) {
	const auto previousGridId = GridUtil::GetGridId(hex);
	auto previousGridIt = gridList.find(previousGridId);
	if (previousGridIt == gridList.end()) {
		return;
	}

	auto& previousGrid = previousGridIt->second;
	previousGrid.entity_list.erase(entity);
	if (previousGrid.entity_list.empty()) {
		gridList.erase(previousGridIt);
	}
}


void AoiSystem::BroadCastLeaveGridMessage(const SceneGridListComp& gridList, entt::entity entity, const GridSet& gridsToLeave) {
	if (gridsToLeave.empty() || gridList.empty()) {
		return;
	}

	EntityUnorderedSet observersToNotifyExit;
	actorDestroyMessage.Clear();
	actorListDestroyMessage.Clear();
	actorDestroyMessage.set_entity(entt::to_integral(entity));

	for (const auto& gridId : gridsToLeave) {
		auto it = gridList.find(gridId);
		if (it == gridList.end()) {
			continue;
		}
		for (const auto& viewLeaver : it->second.entity_list) {
			observersToNotifyExit.emplace(viewLeaver);
			actorListDestroyMessage.add_entity(entt::to_entity(viewLeaver));

			ViewUtil::HandlePlayerLeaveMessage(viewLeaver, entity);
			ViewUtil::HandlePlayerLeaveMessage(entity, viewLeaver);

			InterestUtil::RemoveAoiEntity(entity, viewLeaver);
			InterestUtil::RemoveAoiEntity(viewLeaver, entity);
		}
	}

	SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListDestroyMessageId, actorListDestroyMessage, entity);
	BroadCastToPlayer(ClientPlayerSceneServiceNotifyActorDestroyMessageId, actorDestroyMessage, observersToNotifyExit);
}
