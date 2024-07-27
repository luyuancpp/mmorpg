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
#include "system/scene/hexagons_grid.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"

const Point kDefaultSize(20.0, 20.0);
const Point kOrigin(0.0, 0.0);
const auto KFlat = Layout(layout_flat, kDefaultSize, kOrigin);

void AoiSystem::Update(double delta) {
    for (auto&& [mover, transform, player_scene] : tls.registry.view<Transform, SceneEntity>().each()) {
        HandlePlayerMovement(mover, transform, player_scene);
    }
}

void AoiSystem::HandlePlayerMovement(entt::entity mover, const Transform& transform, SceneEntity& player_scene) {
    if (!tls.sceneRegistry.valid(player_scene.sceneEntity)) {
        LOG_ERROR << "scene not found " << tls.registry.get<Guid>(mover);
        return;
    }

    GridSet enter_grid_set, leave_grid_set;
    EntitySet player_entrant_observer_list, observer_leave_player_set;

    auto& grid_list = tls.sceneRegistry.get<SceneGridList>(player_scene.sceneEntity);
    const auto hex = hex_round(pixel_to_hex(KFlat, Point(transform.location().x(), transform.location().y())));
    const auto grid_id = GetGridId(hex);

    // 计算格子差
    if (!tls.registry.any_of<Hex>(mover)) {
        // 新进入
        ScanNeighborGridId(hex, enter_grid_set);
        grid_list[grid_id].entity_list.emplace(mover);
        tls.registry.emplace<Hex>(mover, hex);
    }
    else {
        const auto& hex_old = tls.registry.get<Hex>(mover);
        const auto distance = hex_distance(hex_old, hex);
        if (distance == 0) {
            return;
        }

        ScanNeighborGridId(hex_old, leave_grid_set);
        ScanNeighborGridId(hex, enter_grid_set);

        if (distance < 3) {
            for (const auto& it : enter_grid_set) {
                leave_grid_set.erase(it);
            }
            for (const auto& it : leave_grid_set) {
                enter_grid_set.erase(it);
            }
        }

        LeaveGrid(hex_old, grid_list, mover);
        grid_list[grid_id].entity_list.emplace(mover);
        tls.registry.remove<Hex>(mover);
        tls.registry.emplace<Hex>(mover, hex);
    }

    // 计算可以看到的人，并填充网络包
    tls_actor_create_s2c.Clear();
    for (const auto& grid : enter_grid_set) {
        const auto& observer_list = grid_list[grid].entity_list;
        for (const auto& observer : observer_list) {
            if (tls.registry.any_of<Npc>(mover) && ViewSystem::CheckSendNpcEnterMessage(observer, mover)) {
                ViewSystem::FillActorCreateS2CInfo(mover);
                player_entrant_observer_list.emplace(observer);
            }
            else if (!tls.registry.any_of<Npc>(mover) && ViewSystem::CheckSendPlayerEnterMessage(observer, mover)) {
                ViewSystem::FillActorCreateS2CInfo(mover);
                player_entrant_observer_list.emplace(observer);
            }
        }
    }

    BroadCast2Player(player_entrant_observer_list, ClientPlayerSceneServicePushActorCreateS2CMsgId, tls_actor_create_s2c);
    BroadCastLeaveGridMessage(grid_list, mover, leave_grid_set);
}

absl::uint128 AoiSystem::GetGridId(const Location& l) {
    return GetGridId(hex_round(pixel_to_hex(KFlat, Point(l.x(), l.y()))));
}

absl::uint128 AoiSystem::GetGridId(const Hex& hex) {
    return static_cast<absl::uint128>(hex.q) << 64 | static_cast<uint64_t>(hex.r);
}

void AoiSystem::ScanNeighborGridId(const Hex& hex, GridSet& grid_set) {
    for (int i = 0; i < 6; ++i) {
        grid_set.emplace(GetGridId(hex_neighbor(hex, i)));
    }
}

void AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message) {
    const auto player = entt::to_entity(message.entity());
    if (!tls.registry.valid(player)) {
        LOG_ERROR << "scene not found";
        return;
    }

    const auto scene_entity = tls.registry.try_get<SceneEntity>(player);
    if (!scene_entity) {
        return;
    }

    const auto hex = tls.registry.try_get<Hex>(player);
    if (!hex) {
        return;
    }

    auto& grid_list = tls.sceneRegistry.get<SceneGridList>(scene_entity->sceneEntity);
    GridSet leave_grid_set;
    ScanNeighborGridId(*hex, leave_grid_set);

    BroadCastLeaveGridMessage(grid_list, player, leave_grid_set);
    LeaveGrid(*hex, grid_list, player);
}

void AoiSystem::UpdateLogGridSize(double delta) {
    for (auto&& [scene, grid_list] : tls.sceneRegistry.view<SceneGridList>().each()) {
        for (const auto& [_, entity_list] : grid_list) {
            if (entity_list.entity_list.empty()) {
                LOG_ERROR << "grid empty not remove " << tls.sceneRegistry.get<SceneInfo>(scene).guid();
            }
        }
    }
}

void AoiSystem::LeaveGrid(const Hex& hex, SceneGridList& grid_list, entt::entity player) {
    const auto grid_id_old = GetGridId(hex);
    auto& leave_grid = grid_list[grid_id_old];
    leave_grid.entity_list.erase(player);
    if (leave_grid.entity_list.empty()) {
        grid_list.erase(grid_id_old);
    }
}

void AoiSystem::BroadCastLeaveGridMessage(const SceneGridList& grid_list, entt::entity player, const GridSet& leave_grid_set) {
    EntitySet observer_leave_player_set;
    tls_actor_destroy_s2c.Clear();
    tls_actor_destroy_s2c.set_entity(entt::to_integral(player));
    for (auto& grid : leave_grid_set)
    {
        auto it = grid_list.find(grid);
        if (it == grid_list.end())
        {
            continue;
        }
        for (auto & observer : it->second.entity_list)
        {
            observer_leave_player_set.emplace(observer);
        }
    }
    BroadCast2Player(observer_leave_player_set, ClientPlayerSceneServicePushActorDestroyS2CMsgId, tls_actor_destroy_s2c);
}
