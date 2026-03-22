#include "aoi.h"

#include "hexagons_grid.h"
#include "core/network/message_system.h"
#include "entity/system/entity.h"
#include "scene/scene/comp/grid.h"
#include "scene/scene/system/grid.h"
#include "scene/scene/system/interest.h"
#include "scene/scene/system/view.h"
#include "muduo/base/Logging.h"
#include "proto/common/component/actor_comp.pb.h"
#include "proto/common/event/scene_event.pb.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "core/utils/stat/stat.h"
#include "type_alias/actor.h"
#include "network/player_message_utils.h"
#include <thread_context/registry_manager.h>
#include <modules/scene/comp/scene_comp.h>


void AoiSystem::Update(double delta) {
    for (auto&& [entity, transform, sceneComp] : tlsRegistryManager.actorRegistry.view<Transform, SceneEntityComp>().each()) {
        // Skip invalid scenes
        if (!tlsRegistryManager.sceneRegistry.valid(sceneComp.sceneEntity)) {
            LOG_ERROR << "Scene not found for entity " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(entity);
            continue;
        }

        auto& gridList = tlsRegistryManager.sceneRegistry.get_or_emplace<SceneGridListComp>(sceneComp.sceneEntity);

        // Get current hex grid position
        const auto currentHex = GridSystem::CalculateHexPosition(transform);
        const auto currentGridId = GridSystem::GetGridId(currentHex);

        GridSet gridsToEnter, gridsToLeave;
        UpdateGridState(entity, gridList, currentHex, currentGridId, gridsToEnter, gridsToLeave);

        // Handle entity enter/leave visibility
        HandleEntityVisibility(entity, gridList, gridsToEnter, gridsToLeave);
    }
}

void AoiSystem::UpdateGridState(const entt::entity entity, SceneGridListComp& gridList, const Hex& currentHex,
                                const GridId currentGridId, GridSet& gridsToEnter, GridSet& gridsToLeave) {
    if (!tlsRegistryManager.actorRegistry.any_of<Hex>(entity)) {
        // First time entering scene
        gridList[currentGridId].entities.insert(entity);
        tlsRegistryManager.actorRegistry.emplace<Hex>(entity, currentHex);
        GridSystem::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);
    } else {
        // Position update
        const auto previousHex = tlsRegistryManager.actorRegistry.get<Hex>(entity);
        if (hex_distance(previousHex, currentHex) == 0) {
            return;
        }

        GridSystem::GetCurrentAndNeighborGridIds(previousHex, gridsToLeave);
        GridSystem::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);

        // Remove intersection grids
        GridSet gridIntersection;
        std::ranges::set_intersection(gridsToLeave, gridsToEnter, std::inserter(gridIntersection, gridIntersection.begin()));
        for (const auto& gridId : gridIntersection) {
            gridsToLeave.erase(gridId);
            gridsToEnter.erase(gridId);
        }

        const auto previousGridId = GridSystem::GetGridId(previousHex);
        gridList[previousGridId].entities.erase(entity);
        gridList[currentGridId].entities.insert(entity);

        tlsRegistryManager.actorRegistry.remove<Hex>(entity);
        tlsRegistryManager.actorRegistry.emplace<Hex>(entity, currentHex);
    }
}

void AoiSystem::HandleEntityVisibility(entt::entity entity, SceneGridListComp& gridList, 
                                       const GridSet& gridsToEnter, const GridSet& gridsToLeave) {
    EntityUnorderedSet entitiesEnteringView, entitiesLeavingView;

    // Entities entering grids
    for (const auto& gridId : gridsToEnter) {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end()) continue;

        for (const auto& otherEntity : gridIt->second.entities) {
            if (otherEntity == entity) continue;

            if (ViewSystem::IsWithinViewRadius(entity, otherEntity)) {
                entitiesEnteringView.insert(otherEntity);
                InterestSystem::AddAoiEntity(entity, otherEntity);
            }

            if (ViewSystem::IsWithinViewRadius(otherEntity, entity)) {
                InterestSystem::AddAoiEntity(otherEntity, entity);
            }
        }
    }

    // Entities leaving grids
    for (const auto& gridId : gridsToLeave) {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end()) continue;

        for (const auto& otherEntity : gridIt->second.entities) {
            if (ViewSystem::IsWithinViewRadius(entity, otherEntity)) {
                entitiesLeavingView.insert(otherEntity);
                InterestSystem::RemoveAoiEntity(entity, otherEntity);
            }

            if (ViewSystem::IsWithinViewRadius(otherEntity, entity)) {
                InterestSystem::RemoveAoiEntity(otherEntity, entity);
            }
        }
    }

    NotifyEntityVisibilityChanges(entity, entitiesEnteringView, entitiesLeavingView);
}

void AoiSystem::NotifyEntityVisibilityChanges(entt::entity entity, 
                                              const EntityUnorderedSet& enteringEntities, 
                                              const EntityUnorderedSet& leavingEntities) {
    // Notify entities entering view
    if (!enteringEntities.empty()) {
        actorListCreateMessage.Clear();
        for (auto& otherEntity : enteringEntities) {
            ViewSystem::FillActorCreateMessageInfo(entity, otherEntity, *actorListCreateMessage.add_actor_list());
        }
        SendMessageToClientViaGate(SceneSceneClientPlayerNotifyActorListCreateMessageId, actorListCreateMessage, entity);
    }

    // Notify entities leaving view
    if (!leavingEntities.empty()) {
        actorListDestroyMessage.Clear();
        for (auto& otherEntity : leavingEntities) {
            actorListDestroyMessage.add_entity(entt::to_integral(otherEntity));
        }
        SendMessageToClientViaGate(SceneSceneClientPlayerNotifyActorListDestroyMessageId, actorListDestroyMessage, entity);
    }
}

void AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message) {
    const auto entity = entt::to_entity(message.entity());
    if (!tlsRegistryManager.actorRegistry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }

    const auto hex = tlsRegistryManager.actorRegistry.try_get<Hex>(entity);
    if (!hex) return;

	auto sceneEntity = tlsRegistryManager.actorRegistry.get_or_emplace<SceneEntityComp>(entity).sceneEntity;
    if (!tlsRegistryManager.sceneRegistry.valid(sceneEntity))
    {
        LOG_ERROR << "Scene entity not found for entity " << entt::to_integral(entity);
		return;
    }

    auto& gridList = tlsRegistryManager.sceneRegistry.get_or_emplace<SceneGridListComp>(sceneEntity);
    GridSet gridsToLeave;
    GridSystem::GetCurrentAndNeighborGridIds(*hex, gridsToLeave);

    RemoveEntityFromGrid(*hex, gridList, entity);
    BroadcastEntityLeave(gridList, entity, gridsToLeave);
}

void AoiSystem::RemoveEntityFromGrid(const Hex& hex, SceneGridListComp& gridList, entt::entity entity) {
    const auto gridId = GridSystem::GetGridId(hex);
    auto gridIt = gridList.find(gridId);
    if (gridIt == gridList.end()) return;

    auto& grid = gridIt->second;
    grid.entities.erase(entity);

    if (gFeatureSwitches[kTestClearEmptyTiles])
    {
        if (grid.entities.empty()) {
            gridList.erase(gridIt);
        }
    }
}

void AoiSystem::BroadcastEntityLeave(const SceneGridListComp& gridList, entt::entity entity, const GridSet& gridsToLeave) {
    if (gridsToLeave.empty()) return;

    actorDestroyMessage.Clear();
    actorDestroyMessage.set_entity(entt::to_integral(entity));

    EntityUnorderedSet observersToNotify;
    for (const auto& gridId : gridsToLeave) {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end()) continue;

        for (const auto& observer : gridIt->second.entities) {
            observersToNotify.insert(observer);
            InterestSystem::RemoveAoiEntity(observer, entity);
        }
    }

    BroadcastMessageToPlayers(SceneSceneClientPlayerNotifyActorDestroyMessageId, actorDestroyMessage, observersToNotify);
}

