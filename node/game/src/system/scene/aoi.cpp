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

void AoiSystem::Update(double delta_time) {
    GridSet grids_to_enter, grids_to_leave;
    EntitySet entities_to_notify_entry, entities_to_notify_exit;

    for (auto&& [entity, transform, scene_component] : tls.registry.view<Transform, SceneEntityComp>().each()) {

        if (!tls.sceneRegistry.valid(scene_component.sceneEntity)) {
            LOG_ERROR << "Scene not found for entity " << tls.registry.get<Guid>(entity);
            continue;
        }

        grids_to_enter.clear();
        grids_to_leave.clear();
        entities_to_notify_entry.clear();
        entities_to_notify_exit.clear();

        auto& grid_list = tls.sceneRegistry.get<SceneGridList>(scene_component.sceneEntity);
        const auto current_hex_position = hex_round(pixel_to_hex(kHexLayout, Point(transform.location().x(), transform.location().y())));
        const auto current_grid_id = GetGridId(current_hex_position);

        // Calculate grid changes
        if (!tls.registry.any_of<Hex>(entity)) {
            // Entity is entering the scene
            ScanNeighborGridIds(current_hex_position, grids_to_enter);
            grid_list[current_grid_id].entity_list.emplace(entity);
            tls.registry.emplace<Hex>(entity, current_hex_position);
        }
        else {
            const auto& previous_hex_position = tls.registry.get<Hex>(entity);
            const auto distance = hex_distance(previous_hex_position, current_hex_position);
            if (distance == 0) {
                return;
            }

            ScanNeighborGridIds(previous_hex_position, grids_to_leave);
            ScanNeighborGridIds(current_hex_position, grids_to_enter);

            if (distance < 3) {
                for (const auto& grid : grids_to_enter) {
                    grids_to_leave.erase(grid);
                }
                for (const auto& grid : grids_to_leave) {
                    grids_to_enter.erase(grid);
                }
            }

            LeaveGrid(previous_hex_position, grid_list, entity);
            grid_list[current_grid_id].entity_list.emplace(entity);
            tls.registry.remove<Hex>(entity);
            tls.registry.emplace<Hex>(entity, current_hex_position);
        }

        // Calculate visible entities and fill network packets
        tls_actor_create_s2c.Clear();
        for (const auto& grid_id : grids_to_enter) {
            if (!grid_list.contains(grid_id)) {
                continue;
            }

            const auto& observers = grid_list[grid_id].entity_list;
            for (const auto& observer : observers) {
                if (tls.registry.any_of<Npc>(entity) && ViewSystem::CheckSendNpcEnterMessage(observer, entity)) {
                    ViewSystem::FillActorCreateS2CInfo(entity);
                    entities_to_notify_entry.emplace(observer);
                }
                else if (!tls.registry.any_of<Npc>(entity) && ViewSystem::CheckSendPlayerEnterMessage(observer, entity)) {
                    ViewSystem::FillActorCreateS2CInfo(entity);
                    entities_to_notify_entry.emplace(observer);
                }
            }
        }

        BroadCastToPlayer(entities_to_notify_entry, ClientPlayerSceneServicePushActorCreateS2CMsgId, tls_actor_create_s2c);
        BroadCastLeaveGridMessage(grid_list, entity, grids_to_leave);
    }
}

void AoiSystem::HandlePlayerMovement(entt::entity entity, const Transform& transform, SceneEntityComp& scene_component) {
    // Function implementation here
}

absl::uint128 AoiSystem::GetGridId(const Location& location) {
    return GetGridId(hex_round(pixel_to_hex(kHexLayout, Point(location.x(), location.y()))));
}

absl::uint128 AoiSystem::GetGridId(const Hex& hex) {
    return static_cast<absl::uint128>(hex.q) << 64 | static_cast<uint64_t>(hex.r);
}

void AoiSystem::ScanNeighborGridIds(const Hex& hex, GridSet& grid_set) {
    for (int i = 0; i < 6; ++i) {
        grid_set.emplace(GetGridId(hex_neighbor(hex, i)));
    }
}

void AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message) {
    const auto entity = entt::to_entity(message.entity());
    if (!tls.registry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }

    const auto scene_component = tls.registry.try_get<SceneEntityComp>(entity);
    if (!scene_component) {
        return;
    }

    const auto hex_position = tls.registry.try_get<Hex>(entity);
    if (!hex_position) {
        return;
    }

    auto& grid_list = tls.sceneRegistry.get<SceneGridList>(scene_component->sceneEntity);
    GridSet grids_to_leave;
    ScanNeighborGridIds(*hex_position, grids_to_leave);

    BroadCastLeaveGridMessage(grid_list, entity, grids_to_leave);
    LeaveGrid(*hex_position, grid_list, entity);
}

void AoiSystem::UpdateLogGridSize(double delta_time) {
    for (auto&& [scene_entity, grid_list] : tls.sceneRegistry.view<SceneGridList>().each()) {
        for (const auto& [grid_id, entity_list] : grid_list) {
            if (entity_list.entity_list.empty()) {
                LOG_ERROR << "Grid is empty but not removed";
            }
        }
    }
}

void AoiSystem::LeaveGrid(const Hex& hex, SceneGridList& grid_list, entt::entity entity) {
    const auto previous_grid_id = GetGridId(hex);
    auto& previous_grid = grid_list[previous_grid_id];
    previous_grid.entity_list.erase(entity);
    if (previous_grid.entity_list.empty()) {
        grid_list.erase(previous_grid_id);
    }
}

void AoiSystem::BroadCastLeaveGridMessage(const SceneGridList& grid_list, entt::entity entity, const GridSet& grids_to_leave) {
    EntitySet observers_to_notify_exit;
    tls_actor_destroy_s2c.Clear();
    tls_actor_destroy_s2c.set_entity(entt::to_integral(entity));
    for (const auto& grid_id : grids_to_leave) {
        auto it = grid_list.find(grid_id);
        if (it == grid_list.end()) {
            continue;
        }
        for (const auto& observer : it->second.entity_list) {
            observers_to_notify_exit.emplace(observer);
        }
    }
    BroadCastToPlayer(observers_to_notify_exit, ClientPlayerSceneServicePushActorDestroyS2CMsgId, tls_actor_destroy_s2c);
}
