#include "aoi_system.h"

#include "game_logic/scene/util/view_util.h"
#include "comp/scene_comp.h"
#include "game_logic/scene/comp/grid.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/event/scene_event.pb.h"
#include "muduo/base/Logging.h"
#include "system/network/message_system.h"
#include "service/scene_client_player_service.h"
#include "hexagons_grid.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"

const Point kDefaultSize(20.0, 20.0);
const Point kOrigin(0.0, 0.0);
const auto kHexLayout = Layout(layout_flat, kDefaultSize, kOrigin);

void AoiSystem::Update(double deltaTime) {
    GridSet gridsToEnter, gridsToLeave;
    EntityUnorderedSet entitiesToNotifyEntry, entitiesToNotifyExit;

    for (auto&& [entity, transform, sceneComponent] : tls.registry.view<Transform, SceneEntityComp>().each()) {

        if (!tls.sceneRegistry.valid(sceneComponent.sceneEntity)) {
            LOG_ERROR << "Scene not found for entity " << tls.registry.get<Guid>(entity);
            continue;
        }

        gridsToEnter.clear();
        gridsToLeave.clear();
        entitiesToNotifyEntry.clear();
        entitiesToNotifyExit.clear();

        auto& gridList = tls.sceneRegistry.get<SceneGridList>(sceneComponent.sceneEntity);
        const auto currentHexPosition = CalculateHexPosition(transform);
        const auto currentGridId = GetGridId(currentHexPosition);

        // Calculate grid changes
        if (!tls.registry.any_of<Hex>(entity)) {
            gridList[currentGridId].entity_list.emplace(entity);
            tls.registry.emplace<Hex>(entity, currentHexPosition);

            ScanCurrentAndNeighborGridIds(currentHexPosition, gridsToEnter);
        }
        else {
            const auto previousHexPosition = tls.registry.get<Hex>(entity);
            const auto distance = hex_distance(previousHexPosition, currentHexPosition);
            if (distance == 0) {
                continue;
            }

            ScanCurrentAndNeighborGridIds(previousHexPosition, gridsToLeave);
            ScanCurrentAndNeighborGridIds(currentHexPosition, gridsToEnter);

            for (const auto& grid : gridsToEnter) {
                gridsToLeave.erase(grid);
            }
            for (const auto& grid : gridsToLeave) {
                gridsToEnter.erase(grid);
            }

            const auto previousGridId = GetGridId(previousHexPosition);
            gridList[previousGridId].entity_list.erase(entity);

            gridList[currentGridId].entity_list.emplace(entity);

            tls.registry.remove<Hex>(entity);
            tls.registry.emplace<Hex>(entity, currentHexPosition);
        }

        // Calculate visible entities and fill network packets
        actorCreateMessage.Clear();
        actorListCreateMessage.Clear();

        for (const auto& gridId : gridsToEnter) {

            auto gridIt = gridList.find(gridId);
            if (gridIt == gridList.end()) {
                continue;
            }

            for (const auto& observer : gridIt->second.entity_list) {

                // 处理npc进入我的视野
                if (observer == entity || 
                    !tls.registry.any_of<Npc>(observer) ||
                    !ViewUtil::ShouldSendNpcEnterMessage(entity, observer)){
                    continue;
                }

                ViewUtil::FillActorCreateMessageInfo(entity, observer, actorCreateMessage);
            }

            for (const auto& observer : gridIt->second.entity_list) {

                if (observer == entity || 
                    tls.registry.any_of<Npc>(entity))
                {
                    continue;
                }

                // 我进入别人视野
                if (ViewUtil::ShouldSendPlayerEnterMessage(observer, entity)) {
                    ViewUtil::FillActorCreateMessageInfo(observer, entity, actorCreateMessage);
                    entitiesToNotifyEntry.emplace(observer);
                }

                //别人进入我的视野
                if (ViewUtil::ShouldSendPlayerEnterMessage(entity, observer)) {
                    ViewUtil::FillActorCreateMessageInfo(entity, observer, *actorListCreateMessage.add_actor_list());
                }
            }
        }
        SendMessageToPlayer(ClientPlayerSceneServicePushActorListCreateS2CMsgId, actorListCreateMessage, entity);

        BroadCastToPlayer(entitiesToNotifyEntry, ClientPlayerSceneServicePushActorCreateS2CMsgId, actorCreateMessage);
        BroadCastLeaveGridMessage(gridList, entity, gridsToLeave);
    }
}

Hex AoiSystem::CalculateHexPosition(const Transform& transform) {
    return hex_round(pixel_to_hex(kHexLayout, Point(transform.location().x(), transform.location().y())));
}

absl::uint128 AoiSystem::GetGridId(const Location& location) {
    return GetGridId(hex_round(pixel_to_hex(kHexLayout, Point(location.x(), location.y()))));
}

absl::uint128 AoiSystem::GetGridId(const Hex& hex) {
    return static_cast<absl::uint128>(hex.q) << 64 | static_cast<uint64_t>(hex.r);
}

void AoiSystem::ScanNeighborGridIds(const Hex& hex, GridSet& gridSet) {
    for (int i = 0; i < 6; ++i) {
        gridSet.emplace(GetGridId(hex_neighbor(hex, i)));
    }
}

void AoiSystem::ScanCurrentAndNeighborGridIds(const Hex& hex, GridSet& grid_set) {
    auto currentGridId = GetGridId(hex);
    grid_set.emplace(currentGridId);
    ScanNeighborGridIds(hex, grid_set);
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

    auto& gridList = tls.sceneRegistry.get<SceneGridList>(sceneComponent->sceneEntity);
    GridSet gridsToLeave;
    ScanCurrentAndNeighborGridIds(*hexPosition, gridsToLeave);

    LeaveGrid(*hexPosition, gridList, entity);
    BroadCastLeaveGridMessage(gridList, entity, gridsToLeave);
}

void AoiSystem::UpdateLogGridSize(double deltaTime) {
    for (auto&& [sceneEntity, gridList] : tls.sceneRegistry.view<SceneGridList>().each()) {
        for (const auto& [gridId, entityList] : gridList) {
            if (entityList.entity_list.empty()) {
                LOG_ERROR << "Grid is empty but not removed";
            }
        }
    }
}

void AoiSystem::LeaveGrid(const Hex& hex, SceneGridList& gridList, entt::entity entity) {
    const auto previousGridId = GetGridId(hex);
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

void AoiSystem::ClearEmptyGrids() {
    std::vector<absl::uint128> destroyEntities;
    for (auto&& [_, gridList] : tls.registry.view<SceneGridList>().each()) {
        destroyEntities.clear();
        for (auto& it : gridList) {
            if (it.second.entity_list.empty()) {
                destroyEntities.emplace_back(it.first);
            }
        }

        for (auto&& it : destroyEntities) {
            gridList.erase(it);
        }
    }
}

void AoiSystem::BroadCastLeaveGridMessage(const SceneGridList& gridList, entt::entity entity, const GridSet& gridsToLeave) {
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
        for (const auto& observer : it->second.entity_list) {
            observersToNotifyExit.emplace(observer);
            actorListDestroyMessage.add_entity(entt::to_entity(observer));

            ViewUtil::HandlePlayerLeaveMessage(observer, entity);
            ViewUtil::HandlePlayerLeaveMessage(entity, observer);
        }
    }
    SendMessageToPlayer(ClientPlayerSceneServicePushActorListDestroyS2CMsgId, actorListDestroyMessage, entity);
    BroadCastToPlayer(observersToNotifyExit, ClientPlayerSceneServicePushActorDestroyS2CMsgId, actorDestroyMessage);
}
