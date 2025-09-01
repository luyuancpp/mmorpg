#include "aoi_system.h"

#include "hexagons_grid.h"
#include "core/network/message_system.h"
#include "entity/system/entity_system.h"
#include "scene/scene/comp/grid_comp.h"
#include "scene/scene/system/grid_system.h"
#include "scene/scene/system/interest_system.h"
#include "scene/scene/system/view_system.h"
#include "muduo/base/Logging.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"
#include "scene/comp/scene_comp.h"
#include "rpc/service_info/player_scene_service_info.h"
#include "core/utils/stat/stat.h"
#include "type_alias/actor.h"
#include "network/player_message_utils.h"
#include <threading/registry_manager.h>


void AoiSystem::Update(double delta) {
    for (auto&& [entity, transform, sceneComp] : tlsRegistryManager.actorRegistry.view<Transform, SceneEntityComp>().each()) {
        // 跳过无效场景
        if (!tlsRegistryManager.sceneRegistry.valid(sceneComp.sceneEntity)) {
            LOG_ERROR << "Scene not found for entity " << tlsRegistryManager.actorRegistry.get<Guid>(entity);
            continue;
        }

        auto& gridList = tlsRegistryManager.sceneRegistry.get<SceneGridListComp>(sceneComp.sceneEntity);

        // 获取当前实体所在网格
        const auto currentHex = GridSystem::CalculateHexPosition(transform);
        const auto currentGridId = GridSystem::GetGridId(currentHex);

        // 初始化网格变化信息
        GridSet gridsToEnter, gridsToLeave;
        UpdateGridState(entity, gridList, currentHex, currentGridId, gridsToEnter, gridsToLeave);

        // 处理实体进入/离开视野
        HandleEntityVisibility(entity, gridList, gridsToEnter, gridsToLeave);
    }
}

void AoiSystem::UpdateGridState(const entt::entity entity, SceneGridListComp& gridList, const Hex& currentHex,
                                const GridId currentGridId, GridSet& gridsToEnter, GridSet& gridsToLeave) {
    if (!tlsRegistryManager.actorRegistry.any_of<Hex>(entity)) {
        // 首次加入场景
        gridList[currentGridId].entities.insert(entity);
        tlsRegistryManager.actorRegistry.emplace<Hex>(entity, currentHex);
        GridSystem::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);
    } else {
        // 更新位置
        const auto previousHex = tlsRegistryManager.actorRegistry.get<Hex>(entity);
        if (hex_distance(previousHex, currentHex) == 0) {
            return;
        }

        GridSystem::GetCurrentAndNeighborGridIds(previousHex, gridsToLeave);
        GridSystem::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);

        // 移除交集的网格
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

    // 处理进入新网格的实体
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

    // 处理离开旧网格的实体
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
    // 通知进入视野的实体
    if (!enteringEntities.empty()) {
        actorListCreateMessage.Clear();
        for (auto& otherEntity : enteringEntities) {
            ViewSystem::FillActorCreateMessageInfo(entity, otherEntity, *actorListCreateMessage.add_actor_list());
        }
        SendMessageToClientViaGate(SceneSceneClientPlayerNotifyActorListCreateMessageId, actorListCreateMessage, entity);
    }

    // 通知离开视野的实体
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

    auto& gridList = tlsRegistryManager.sceneRegistry.get<SceneGridListComp>(tlsRegistryManager.actorRegistry.get<SceneEntityComp>(entity).sceneEntity);
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
