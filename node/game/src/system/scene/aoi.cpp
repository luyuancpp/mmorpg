#include "aoi.h"

#include "view.h"
#include "comp/scene.h"
#include "comp/scene/grid.h"
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
        const auto currentHexPosition = hex_round(pixel_to_hex(kHexLayout, Point(transform.location().x(), transform.location().y())));
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

        for (const auto& gridId : gridsToEnter) {

            auto gridIt = gridList.find(gridId);
            if (gridIt == gridList.end()) {
                continue;
            }

            for (const auto& observer : gridIt->second.entity_list) {

                if (observer == entity)
                {
                    continue;
                }

                // 我进入别人视野
                if (tls.registry.any_of<Npc>(entity) && ViewSystem::CheckSendNpcEnterMessage(observer, entity)) {
                    ViewSystem::FillActorCreateS2CInfo(observer, entity, actorCreateMessage);
                    entitiesToNotifyEntry.emplace(observer);
                }
                else if (!tls.registry.any_of<Npc>(entity) && ViewSystem::CheckSendPlayerEnterMessage(observer, entity)) {
                    ViewSystem::FillActorCreateS2CInfo(observer, entity, actorCreateMessage);
                    entitiesToNotifyEntry.emplace(observer);
                }

                //别人进入我的视野
                if (tls.registry.any_of<Npc>(observer) && ViewSystem::CheckSendNpcEnterMessage(entity, observer)) {
                    ViewSystem::FillActorCreateS2CInfo(entity, observer, actorCreateMessage);
                }
                else if (!tls.registry.any_of<Npc>(observer) && ViewSystem::CheckSendPlayerEnterMessage(entity, observer)) {
                    ViewSystem::FillActorCreateS2CInfo(entity, observer, actorCreateMessage);
                }
            }
        }

        BroadCastToPlayer(entitiesToNotifyEntry, ClientPlayerSceneServicePushActorCreateS2CMsgId, actorCreateMessage);
        BroadCastLeaveGridMessage(gridList, entity, gridsToLeave);
    }
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

void AoiSystem::ScanCurrentAndNeighborGridIds(const Hex& hex, GridSet& grid_set)
{
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
    ScanNeighborGridIds(*hexPosition, gridsToLeave);

    BroadCastLeaveGridMessage(gridList, entity, gridsToLeave);
    LeaveGrid(*hexPosition, gridList, entity);
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
    if (previousGridIt == gridList.end())
    {
        return;
    }
    auto& previousGrid = previousGridIt->second;
    previousGrid.entity_list.erase(entity);
    if (previousGrid.entity_list.empty()) {
        gridList.erase(previousGridIt);
    }
}

void AoiSystem::ClearEmptyGrids()
{
    std::vector<absl::uint128> destroyEntites;
    for (auto&& [_, gridList] : tls.registry.view<SceneGridList>().each())
    {
        destroyEntites.clear();
        for (auto& it : gridList)
        {
            if (!it.second.entity_list.empty())
            {
                continue;
            }
            destroyEntites.emplace_back(it.first);
        }

        for (auto&& it : destroyEntites)
        {
            gridList.erase(it);
        }
    }
}

void AoiSystem::BroadCastLeaveGridMessage(const SceneGridList& gridList, entt::entity entity, const GridSet& gridsToLeave) {
    if (gridsToLeave.empty() || gridList.empty())
    {
        return;
    }

    EntityUnorderedSet observersToNotifyExit;
    actorDestroyMessage.Clear();
    actorDestroyMessage.set_entity(entt::to_integral(entity));
    for (const auto& gridId : gridsToLeave) {
        auto it = gridList.find(gridId);
        if (it == gridList.end()) {
            continue;
        }
        for (const auto& observer : it->second.entity_list) {
            observersToNotifyExit.emplace(observer);
            ViewSystem::HandlerPlayerLeaveMessage(observer, entity);
            ViewSystem::HandlerPlayerLeaveMessage(entity, observer);
        }
    }
    BroadCastToPlayer(observersToNotifyExit, ClientPlayerSceneServicePushActorDestroyS2CMsgId, actorDestroyMessage);
}
