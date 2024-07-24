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

void AoiSystem::Update(double delta)
{
    GridSet enter_grid_set;
    GridSet leave_grid_set;
    GridSet copy_leaver_grid_set;
    GridSet copy_enter_grid_set;
    EntitySet player_entrant_observer_list;
    EntitySet observer_leave_player_set;

    for (auto&& [mover, transform, player_scene] : tls.registry.view<Transform, SceneEntity>().each())
    {
        if (!tls.scene_registry.valid(player_scene.sceneEntity))
        {
            LOG_ERROR << "scene not found " << tls.registry.get<Guid>(mover);
            continue;
        }
        enter_grid_set.clear();
        leave_grid_set.clear();
        player_entrant_observer_list.clear();
        observer_leave_player_set.clear();
        
        auto& grid_list = tls.scene_registry.get<SceneGridList>(player_scene.sceneEntity);
        const auto hex =
            hex_round(pixel_to_hex(KFlat, Point(transform.location().x(), transform.location().y())));
        const auto grid_id = GetGridId(hex);
       
        // 计算格子差
        if (!tls.registry.any_of<Hex>(mover))
        {
            // 新进入
            ScanNeighborGridId(hex, enter_grid_set);
            grid_list[grid_id].entity_list.emplace(mover);
            tls.registry.emplace<Hex>(mover, hex);
        }
        else
        {
            const auto& hex_old = tls.registry.get<Hex>(mover);
            if (const auto distance = hex_distance(hex_old, hex);
                distance == 0)
            {
                continue;
            }
            else if (distance < 3)
            {
                // 如果相邻格子移动,则计算差值
                ScanNeighborGridId(hex_old, leave_grid_set);
                ScanNeighborGridId(hex, enter_grid_set);

                copy_leaver_grid_set = leave_grid_set;
                copy_enter_grid_set = enter_grid_set;
                for (auto& it : copy_enter_grid_set)
                {
                    leave_grid_set.erase(it);
                }
                for (auto& it : copy_leaver_grid_set)
                {
                    enter_grid_set.erase(it);
                }
            }
            else
            {
                // 通知旧的格子离开
                //通知新的格子进入
                ScanNeighborGridId(hex_old, leave_grid_set);
                ScanNeighborGridId(hex, enter_grid_set);
            }
            LeaveGrid(hex_old, grid_list, mover);

            grid_list[grid_id].entity_list.emplace(mover);
            tls.registry.remove<Hex>(mover);
            tls.registry.emplace<Hex>(mover, hex);
        }

        // 计算可以看到的人，并填充网络包
        tls_actor_create_s2c.Clear();
        for (const auto& grid : enter_grid_set)
        {
            //npc
            const auto& observer_list = grid_list[grid].entity_list;
            for (auto& observer : observer_list)
            {
                if (!tls.registry.any_of<Npc>(mover))
                {
                    continue;
                }
                if (!ViewSystem::CheckSendNpcEnterMessage(observer, mover))
                {
                    continue;
                }
                ViewSystem::FillActorCreateS2CInfo(mover);
                player_entrant_observer_list.emplace(observer);
            }
            //玩家进入视野
            for (auto& observer : observer_list)
            {
                if (tls.registry.any_of<Npc>(mover))
                {
                    continue;
                }
                if (!ViewSystem::CheckSendPlayerEnterMessage(observer, mover))
                {
                    continue;
                }
                ViewSystem::FillActorCreateS2CInfo(mover);
                player_entrant_observer_list.emplace(observer);
            }
        }
        BroadCast2Player(player_entrant_observer_list, ClientPlayerSceneServicePushActorCreateS2CMsgId, tls_actor_create_s2c);

        BroadCastLeaveGridMessage(grid_list, mover, leave_grid_set);
    }
}

absl::uint128 AoiSystem::GetGridId(const Location& l)
{
    const auto hex =
           hex_round(pixel_to_hex(KFlat, Point(l.x(), l.y())));
    const absl::uint128 index = 
        static_cast<absl::uint128>(hex.q) << 64 & static_cast<uint64_t>(hex.r);
    return index;
}

absl::uint128 AoiSystem::GetGridId(const Hex& hex)
{
    return static_cast<absl::uint128>(hex.q) << 64 & static_cast<uint64_t>(hex.r);
}

void AoiSystem::ScanNeighborGridId(const Hex& hex, GridSet& grid_set)
{
    grid_set.emplace(GetGridId(hex_neighbor(hex, 0)));
    grid_set.emplace(GetGridId(hex_neighbor(hex, 1)));
    grid_set.emplace(GetGridId(hex_neighbor(hex, 2)));
    grid_set.emplace(GetGridId(hex_neighbor(hex, 3)));
    grid_set.emplace(GetGridId(hex_neighbor(hex, 4)));
    grid_set.emplace(GetGridId(hex_neighbor(hex, 5)));
}

void  AoiSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
    const auto player = entt::to_entity(message.entity());
    if ( !tls.registry.valid(player))
    {
        LOG_ERROR << "scene not found" ;
        return;
    }
    const auto scene_entity = tls.registry.try_get<SceneEntity>(player);
    if (nullptr == scene_entity)
    {
        LOG_ERROR << "scene not found";
        return;
    }
    const auto hex = tls.registry.try_get<Hex>(player);
    if (nullptr == hex)
    {
        return;
    }
    auto& grid_list = tls.scene_registry.get<SceneGridList>(scene_entity->sceneEntity);

    const auto grid_id = GetGridId(*hex);
    GridSet leave_grid_set;
    ScanNeighborGridId(*hex, leave_grid_set);

    BroadCastLeaveGridMessage(grid_list, player, leave_grid_set);

    LeaveGrid(*hex, grid_list, player);
}

void AoiSystem::UpdateLogGridSize(double delta)
{
    for (auto&& [scene, grid_list] : tls.scene_registry.view<SceneGridList>().each())
    {
        for (const auto& entity_list : grid_list)
        {
            if (entity_list.second.entity_list.empty())
            {
                LOG_ERROR << "grid empty not remove" << tls.scene_registry.get<SceneInfo>(scene).guid();
                continue;
            }
        }
    }
}

void AoiSystem::LeaveGrid(const Hex& hex, SceneGridList& grid_list, entt::entity player)
{
    const auto grid_id_old = GetGridId(hex);
    auto& leave_grid = grid_list[grid_id_old];
    leave_grid.entity_list.erase(player);
    if (leave_grid.entity_list.empty())
    {
        grid_list.erase(grid_id_old);
    }
}

void AoiSystem::BroadCastLeaveGridMessage(const SceneGridList& grid_list, entt::entity player, const GridSet& leave_grid_set)
{
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
