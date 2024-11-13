#include "aoi_system.h"

#include "hexagons_grid.h"
#include "game_logic/core/network/message_util.h"
#include "game_logic/scene/comp/grid_comp.h"
#include "game_logic/scene/util/grid_util.h"
#include "game_logic/scene/util/interest_util.h"
#include "game_logic/scene/util/view_util.h"
#include "muduo/base/Logging.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"
#include "scene/comp/scene_comp.h"
#include "service_info/player_scene_service_info.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"

void AoiSystem::Update(double delta) {
	GridSet neighborGridsToEnter, neighborGridsToLeave;
	EntityUnorderedSet entitiesEnteringCurrentView, observersNotifiedOfMyEntry;

	for (auto&& [currentEntity, transform, sceneComponent] : tls.registry.view<Transform, SceneEntityComp>().each()) {

		if (!tls.sceneRegistry.valid(sceneComponent.sceneEntity)) {
			LOG_ERROR << "Scene not found for entity " << tls.registry.get<Guid>(currentEntity);
			continue;
		}

		neighborGridsToEnter.clear();
		neighborGridsToLeave.clear();
		entitiesEnteringCurrentView.clear();
		observersNotifiedOfMyEntry.clear();

		auto& gridList = tls.sceneRegistry.get<SceneGridListComp>(sceneComponent.sceneEntity);
		const auto currentHexPosition = GridUtil::CalculateHexPosition(transform);
		const auto currentGridId = GridUtil::GetGridId(currentHexPosition);

		// 如果当前实体没有 Hex 位置，添加它
		if (!tls.registry.any_of<Hex>(currentEntity)) {
			gridList[currentGridId].entity_list.emplace(currentEntity);
			tls.registry.emplace<Hex>(currentEntity, currentHexPosition);

			GridUtil::GetCurrentAndNeighborGridIds(currentHexPosition, neighborGridsToEnter);
		} else {
			const auto previousHexPosition = tls.registry.get<Hex>(currentEntity);
			if (hex_distance(previousHexPosition, currentHexPosition) <= 0) {
				continue;
			}

			GridUtil::GetCurrentAndNeighborGridIds(previousHexPosition, neighborGridsToLeave);
			GridUtil::GetCurrentAndNeighborGridIds(currentHexPosition, neighborGridsToEnter);

			for (const auto& grid : neighborGridsToEnter) {
				neighborGridsToLeave.erase(grid);
			}
			for (const auto& grid : neighborGridsToLeave) {
				neighborGridsToEnter.erase(grid);
			}

			const auto previousGridId = GridUtil::GetGridId(previousHexPosition);
			gridList[previousGridId].entity_list.erase(currentEntity);

			gridList[currentGridId].entity_list.emplace(currentEntity);

			tls.registry.remove<Hex>(currentEntity);
			tls.registry.emplace<Hex>(currentEntity, currentHexPosition);
		}

		actorCreateMessage.Clear();
		actorListCreateMessage.Clear();

		for (const auto& gridId : neighborGridsToEnter) {
			auto gridIt = gridList.find(gridId);
			if (gridIt == gridList.end()) {
				continue;
			}

			// 处理进入当前实体视野的 NPC
			for (const auto& otherEntity : gridIt->second.entity_list) {
				if (otherEntity == currentEntity || 
					!tls.registry.any_of<Npc>(otherEntity) || 
					!ViewUtil::ShouldSendNpcEnterMessage(currentEntity, otherEntity)) {
					continue;
				}

				ViewUtil::FillActorCreateMessageInfo(currentEntity, otherEntity, actorCreateMessage);
				InterestUtil::AddAoiEntity(currentEntity, otherEntity);
				InterestUtil::AddAoiEntity(otherEntity, currentEntity);
			}

			// 处理当前实体进入其他实体视野或其他实体进入当前实体视野
			for (const auto& otherEntity : gridIt->second.entity_list) {
				if (otherEntity == currentEntity || tls.registry.any_of<Npc>(currentEntity)) {
					continue;
				}

				// 当前实体进入其他实体的视野
				if (ViewUtil::ShouldUpdateView(otherEntity, currentEntity)) {
					ViewUtil::FillActorCreateMessageInfo(otherEntity, currentEntity, actorCreateMessage);
					observersNotifiedOfMyEntry.emplace(otherEntity);
					InterestUtil::AddAoiEntity(otherEntity, currentEntity);
				}

				// 其他实体进入当前实体的视野
				if (ViewUtil::ShouldUpdateView(currentEntity, otherEntity)) {
					ViewUtil::FillActorCreateMessageInfo(currentEntity, otherEntity, *actorListCreateMessage.add_actor_list());
					entitiesEnteringCurrentView.emplace(otherEntity);
					InterestUtil::AddAoiEntity(currentEntity, otherEntity);
				}
			}
		}

		// 发送消息给进入视野的实体
		if (!actorListCreateMessage.actor_list().empty()) {
			SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListCreateMessageId, actorListCreateMessage, currentEntity);
		}

		// 通知其他实体当前实体进入了他们的视野
		if (actorCreateMessage.entity() > 0) {
			BroadCastToPlayer(ClientPlayerSceneServiceNotifyActorCreateMessageId, actorCreateMessage, observersNotifiedOfMyEntry);
		}

		// 处理离开网格的情况
		BroadCastLeaveGridMessage(gridList, currentEntity, neighborGridsToLeave);
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
