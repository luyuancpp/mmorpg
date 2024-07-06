#include "aoi.h"

#include "comp/scene.h"
#include "comp/scene/grid.h"
#include "component_proto/actor_comp.pb.h"
#include "event_proto/scene_event.pb.h"
#include "muduo/base/Logging.h"
#include "system/scene/hexagons_grid.h"
#include "thread_local/storage.h"

const Point kDefaultSize(20.0, 20.0);
const Point kOrigin(0.0, 0.0);
const auto KFlat = Layout(layout_flat, kDefaultSize, kOrigin);

void AoiSystem::Update(double delta)
{
    GridSet enter_grid_set;
    GridSet leave_grid_set;
    GridSet copy_leaver_grid_set;
    GridSet copy_enter_grid_set;
    for (auto&& [player, transform, player_scene] : tls.registry.view<Transform, SceneEntity>().each())
    {
        if (!tls.scene_registry.valid(player_scene.scene_entity_))
        {
            LOG_ERROR << "scene not found " << tls.registry.get<Guid>(player);
            continue;
        }
        enter_grid_set.clear();
        leave_grid_set.clear();
        
        auto& grid_list = tls.scene_registry.get<SceneGridList>(player_scene.scene_entity_);
        const auto hex =
            hex_round(pixel_to_hex(KFlat, Point(transform.localtion().x(), transform.localtion().y())));
        const auto grid_id = GetGridId(hex);
        // 新进入
        if (!tls.registry.any_of<Hex>(player))
        {
            ScanNeighborGridId(hex,enter_grid_set);
            grid_list[grid_id].entity_list_.emplace(player);
            tls.registry.emplace<Hex>(player, hex);
        }
        else
        {
            const auto& hex_old = tls.registry.get<Hex>(player);
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
           
            grid_list[grid_id].entity_list_.emplace(player);
            tls.registry.remove<Hex>(player);
            tls.registry.emplace<Hex>(player, hex);

            LeaveGrid(hex_old, grid_list, player);
        }
    }
}

absl::uint128 AoiSystem::GetGridId(const Location& l)
{
    const auto hex =
           hex_round(pixel_to_hex(KFlat, Point(l.x(), l.y())));
    const absl::uint128 index = static_cast<absl::uint128>(hex.q) << 64 & static_cast<uint64_t>(hex.r);
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
    const auto grid_id = GetGridId(*hex);
    GridSet leave_grid_set;
    ScanNeighborGridId(*hex, leave_grid_set);

    
    
    auto& grid_list =
        tls.scene_registry.get<SceneGridList>(scene_entity->scene_entity_);
    LeaveGrid(*hex, grid_list, player);
}

void AoiSystem::LeaveGrid(const Hex& hex, SceneGridList& grid_list, entt::entity player)
{
    const auto grid_id_old = GetGridId(hex);
    grid_list[grid_id_old].entity_list_.erase(player);
    if (grid_list[grid_id_old].entity_list_.empty())
    {
        grid_list.erase(grid_id_old);
    }
}
