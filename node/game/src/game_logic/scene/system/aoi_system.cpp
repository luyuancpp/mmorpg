#include "aoi_system.h"

#include "hexagons_grid.h"
#include "game_logic/core/network/message_util.h"
#include "game_logic/entity/util/entity_util.h"
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
	GridSet gridIntersection;
	EntityUnorderedSet entitiesEnteringCurrentView, observersNotifiedOfMyEntry;
	EntityUnorderedSet entitiesLeavingCurrentView, observersNotifiedOfMyLeave;

	for (auto&& [currentEntity, transform, sceneComponent] : tls.registry.view<Transform, SceneEntityComp>().each()) {

		if (!tls.sceneRegistry.valid(sceneComponent.sceneEntity)) {
			LOG_ERROR << "Scene not found for entity " << tls.registry.get<Guid>(currentEntity);
			continue;
		}

		auto& gridList = tls.sceneRegistry.get<SceneGridListComp>(sceneComponent.sceneEntity);
		
		neighborGridsToEnter.clear();
		neighborGridsToLeave.clear();
		entitiesEnteringCurrentView.clear();
		observersNotifiedOfMyEntry.clear();
		entitiesLeavingCurrentView.clear();
		observersNotifiedOfMyLeave.clear();

		const auto currentHexPosition = GridUtil::CalculateHexPosition(transform);
		const auto currentGridId = GridUtil::GetGridId(currentHexPosition);

		// 如果当前实体没有 Hex 位置，添加它
		if (!tls.registry.any_of<Hex>(currentEntity)) {
			gridList[currentGridId].entityCollection.emplace(currentEntity);
			tls.registry.emplace<Hex>(currentEntity, currentHexPosition);

			GridUtil::GetCurrentAndNeighborGridIds(currentHexPosition, neighborGridsToEnter);
		} else {
			const auto previousHexPosition = tls.registry.get<Hex>(currentEntity);
			if (hex_distance(previousHexPosition, currentHexPosition) <= 0) {
				continue;
			}

			GridUtil::GetCurrentAndNeighborGridIds(previousHexPosition, neighborGridsToLeave);
			GridUtil::GetCurrentAndNeighborGridIds(currentHexPosition, neighborGridsToEnter);

			gridIntersection.clear();
			// 找到交集
			std::ranges::set_intersection(neighborGridsToLeave, neighborGridsToEnter,
			                              std::inserter(gridIntersection, gridIntersection.begin()));

			// 从两个集合中删除交集元素
			for (const auto& elem : gridIntersection) {
				neighborGridsToLeave.erase(elem);
				neighborGridsToEnter.erase(elem);
			}

			const auto previousGridId = GridUtil::GetGridId(previousHexPosition);
			gridList[previousGridId].entityCollection.erase(currentEntity);

			gridList[currentGridId].entityCollection.emplace(currentEntity);

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
			for (const auto& otherEntity : gridIt->second.entityCollection) {
				if (otherEntity == currentEntity || 
					EntityUtil::IsNotNpc(otherEntity) || 
					!ViewUtil::ShouldSendNpcEnterMessage(currentEntity, otherEntity)) {
					continue;
				}

				ViewUtil::FillActorCreateMessageInfo(currentEntity, otherEntity, actorCreateMessage);
				InterestUtil::AddAoiEntity(currentEntity, otherEntity);
			}

			// 处理当前实体进入其他实体视野或其他实体进入当前实体视野
			for (const auto& otherEntity : gridIt->second.entityCollection) {
				if (otherEntity == currentEntity || EntityUtil::IsNotPlayer(otherEntity)) {
					continue;
				}

				// 当前实体进入其他实体的视野
				if (ViewUtil::IsWithinViewRadius(otherEntity, currentEntity)) {
					observersNotifiedOfMyEntry.emplace(otherEntity);
					InterestUtil::AddAoiEntity(otherEntity, currentEntity);
				}else{
					observersNotifiedOfMyLeave.emplace(otherEntity);
					InterestUtil::RemoveAoiEntity(otherEntity, currentEntity);
				}

				// 其他实体进入当前实体的视野
				if (ViewUtil::IsWithinViewRadius(currentEntity, otherEntity)) {
					entitiesEnteringCurrentView.emplace(otherEntity);
					InterestUtil::AddAoiEntity(currentEntity, otherEntity);
				}else{
					entitiesLeavingCurrentView.emplace(otherEntity);
					InterestUtil::RemoveAoiEntity(currentEntity, otherEntity);
				}
			}
		}

		//处理我进入别人的视野，添加感兴趣列表
		for (auto& otherEntity: observersNotifiedOfMyEntry)
		{
			ViewUtil::FillActorCreateMessageInfo(otherEntity, currentEntity, actorCreateMessage);
		}

		// 通知其他实体当前实体进入了他们的视野
		if (actorCreateMessage.entity() > 0) {
			BroadCastToPlayer(ClientPlayerSceneServiceNotifyActorCreateMessageId, actorCreateMessage, observersNotifiedOfMyEntry);
		}
		
		//处理别人进入我的视野，添加感兴趣列表
		for (auto& otherEntity : entitiesEnteringCurrentView){
			ViewUtil::FillActorCreateMessageInfo(currentEntity, otherEntity, *actorListCreateMessage.add_actor_list());
		}
		
		// 发送消息给我进入我的视野的实体
		if (!actorListCreateMessage.actor_list().empty()) {
			SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListCreateMessageId, actorListCreateMessage, currentEntity);
		}

		// 处理离开网格的情况
		BroadCastToGridActorLeaveMessage(gridList, currentEntity, neighborGridsToLeave);

		//发送消息给我，告诉我离开我视野的人
		if (!entitiesLeavingCurrentView.empty())
		{
			actorListDestroyMessage.Clear();
			for (auto& entityLeavingCurrentView : entitiesLeavingCurrentView){
				actorListDestroyMessage.add_entity(entt::to_integral(entityLeavingCurrentView));
			}
			SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListDestroyMessageId, actorListDestroyMessage, currentEntity);
		}

		//发送消息给我离开别人视野的人，告诉别人我离开了他们视野
		if (!observersNotifiedOfMyLeave.empty())
		{
			actorDestroyMessage.Clear();
			actorDestroyMessage.set_entity(entt::to_integral(currentEntity));
			BroadCastToPlayer(ClientPlayerSceneServiceNotifyActorDestroyMessageId, actorDestroyMessage, observersNotifiedOfMyLeave);
		}
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
	BroadCastToGridActorLeaveMessage(gridList, entity, gridsToLeave);
}


void AoiSystem::LeaveGrid(const Hex& hex, SceneGridListComp& gridList, entt::entity entity) {
	const auto previousGridId = GridUtil::GetGridId(hex);
	auto previousGridIt = gridList.find(previousGridId);
	if (previousGridIt == gridList.end()) {
		return;
	}

	auto& previousGrid = previousGridIt->second;
	previousGrid.entityCollection.erase(entity);
	if (previousGrid.entityCollection.empty()) {
		gridList.erase(previousGridIt);
	}
}


void AoiSystem::BroadCastToGridActorLeaveMessage(const SceneGridListComp& gridList, entt::entity entity, const GridSet& gridsToLeave) {
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
		for (const auto& viewLeaver : it->second.entityCollection) {
			observersToNotifyExit.emplace(viewLeaver);
			actorListDestroyMessage.add_entity(entt::to_integral(viewLeaver));

			ViewUtil::HandlePlayerLeaveMessage(viewLeaver, entity);
			ViewUtil::HandlePlayerLeaveMessage(entity, viewLeaver);

			InterestUtil::RemoveAoiEntity(entity, viewLeaver);
			InterestUtil::RemoveAoiEntity(viewLeaver, entity);
		}
	}

	SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListDestroyMessageId, actorListDestroyMessage, entity);
	BroadCastToPlayer(ClientPlayerSceneServiceNotifyActorDestroyMessageId, actorDestroyMessage, observersToNotifyExit);
}
