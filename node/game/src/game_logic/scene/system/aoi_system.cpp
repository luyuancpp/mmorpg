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
#include "proto/logic/event/scene_event.pb.h"
#include "scene/comp/scene_comp.h"
#include "service_info/player_scene_service_info.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"

void AoiSystem::Update(double delta) {
    for (auto&& [entity, transform, sceneComp] : tls.registry.view<Transform, SceneEntityComp>().each()) {
        // 跳过无效场景
        if (!tls.sceneRegistry.valid(sceneComp.sceneEntity)) {
            LOG_ERROR << "Scene not found for entity " << tls.registry.get<Guid>(entity);
            continue;
        }

        auto& gridList = tls.sceneRegistry.get<SceneGridListComp>(sceneComp.sceneEntity);

        // 获取当前实体所在网格
        const auto currentHex = GridUtil::CalculateHexPosition(transform);
        const auto currentGridId = GridUtil::GetGridId(currentHex);

        // 初始化网格变化信息
        GridSet gridsToEnter, gridsToLeave;
        UpdateGridState(entity, gridList, currentHex, currentGridId, gridsToEnter, gridsToLeave);

        // 处理实体进入/离开视野
        HandleEntityVisibility(entity, gridList, gridsToEnter, gridsToLeave);
    }
}

void AoiSystem::UpdateGridState(const entt::entity entity, SceneGridListComp& gridList, const Hex& currentHex,
                                const GridId currentGridId, GridSet& gridsToEnter, GridSet& gridsToLeave) {
    if (!tls.registry.any_of<Hex>(entity)) {
        // 首次加入场景
        gridList[currentGridId].entities.insert(entity);
        tls.registry.emplace<Hex>(entity, currentHex);
        GridUtil::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);
    } else {
        // 更新位置
        const auto previousHex = tls.registry.get<Hex>(entity);
        if (hex_distance(previousHex, currentHex) == 0) {
            return;
        }

        GridUtil::GetCurrentAndNeighborGridIds(previousHex, gridsToLeave);
        GridUtil::GetCurrentAndNeighborGridIds(currentHex, gridsToEnter);

        // 移除交集的网格
        GridSet gridIntersection;
        std::ranges::set_intersection(gridsToLeave, gridsToEnter, std::inserter(gridIntersection, gridIntersection.begin()));
        for (const auto& gridId : gridIntersection) {
            gridsToLeave.erase(gridId);
            gridsToEnter.erase(gridId);
        }

        const auto previousGridId = GridUtil::GetGridId(previousHex);
        gridList[previousGridId].entities.erase(entity);
        gridList[currentGridId].entities.insert(entity);

        tls.registry.remove<Hex>(entity);
        tls.registry.emplace<Hex>(entity, currentHex);
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

            if (ViewUtil::IsWithinViewRadius(entity, otherEntity)) {
                entitiesEnteringView.insert(otherEntity);
                InterestUtil::AddAoiEntity(entity, otherEntity);
            }

            if (ViewUtil::IsWithinViewRadius(otherEntity, entity)) {
                InterestUtil::AddAoiEntity(otherEntity, entity);
            }
        }
    }

    // 处理离开旧网格的实体
    for (const auto& gridId : gridsToLeave) {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end()) continue;

        for (const auto& otherEntity : gridIt->second.entities) {
            if (ViewUtil::IsWithinViewRadius(entity, otherEntity)) {
                entitiesLeavingView.insert(otherEntity);
                InterestUtil::RemoveAoiEntity(entity, otherEntity);
            }

            if (ViewUtil::IsWithinViewRadius(otherEntity, entity)) {
                InterestUtil::RemoveAoiEntity(otherEntity, entity);
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
            ViewUtil::FillActorCreateMessageInfo(entity, otherEntity, *actorListCreateMessage.add_actor_list());
        }
        SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListCreateMessageId, actorListCreateMessage, entity);
    }

    // 通知离开视野的实体
    if (!leavingEntities.empty()) {
        actorListDestroyMessage.Clear();
        for (auto& otherEntity : leavingEntities) {
            actorListDestroyMessage.add_entity(entt::to_integral(otherEntity));
        }
        SendMessageToPlayer(ClientPlayerSceneServiceNotifyActorListDestroyMessageId, actorListDestroyMessage, entity);
    }
}

void AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message) {
    const auto entity = entt::to_entity(message.entity());
    if (!tls.registry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }

    const auto hex = tls.registry.try_get<Hex>(entity);
    if (!hex) return;

    auto& gridList = tls.sceneRegistry.get<SceneGridListComp>(tls.registry.get<SceneEntityComp>(entity).sceneEntity);
    GridSet gridsToLeave;
    GridUtil::GetCurrentAndNeighborGridIds(*hex, gridsToLeave);

    RemoveEntityFromGrid(*hex, gridList, entity);
    BroadcastEntityLeave(gridList, entity, gridsToLeave);
}

void AoiSystem::RemoveEntityFromGrid(const Hex& hex, SceneGridListComp& gridList, entt::entity entity) {
    const auto gridId = GridUtil::GetGridId(hex);
    auto gridIt = gridList.find(gridId);
    if (gridIt == gridList.end()) return;

    auto& grid = gridIt->second;
    grid.entities.erase(entity);
    if (grid.entities.empty()) {
        gridList.erase(gridIt);
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
            InterestUtil::RemoveAoiEntity(observer, entity);
        }
    }

    BroadCastToPlayer(ClientPlayerSceneServiceNotifyActorDestroyMessageId, actorDestroyMessage, observersToNotify);
}
