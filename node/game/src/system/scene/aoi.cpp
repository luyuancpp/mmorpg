#include "aoi.h"

#include "comp/actor.h"
#include "comp/scene.h"
#include "comp/scene/grid.h"
#include "component_proto/actor_comp.pb.h"
#include "muduo/base/Logging.h"
#include "system/scene/hexagons_grid.h"
#include "thread_local/storage.h"

const Point kDefaultSize(20.0, 20.0);
const Point kOrigin(0.0, 0.0);
const auto KFlat = Layout(layout_flat, kDefaultSize, kOrigin);

using GridSet = std::unordered_set<absl::uint128>;

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
            grid_list[grid_id].entity_list_.emplace(player);
            enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 0)));
            enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 1)));
            enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 2)));
            enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 3)));
            enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 4)));
            enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 5)));
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

                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 0)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 1)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 2)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 3)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 4)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 5)));
                
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 0)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 1)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 2)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 3)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 4)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 5)));

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
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 0)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 1)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 2)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 3)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 4)));
                leave_grid_set.emplace(GetGridId(hex_neighbor(hex_old, 5)));

                //通知新的格子进入
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 0)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 1)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 2)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 3)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 4)));
                enter_grid_set.emplace(GetGridId(hex_neighbor(hex, 5)));
            }
           
            grid_list[grid_id].entity_list_.emplace(player);
            tls.registry.emplace_or_replace<Hex>(player, hex);

            const auto grid_id_old = GetGridId(hex_old);
            grid_list[grid_id_old].entity_list_.erase(player);
            if (grid_list[grid_id_old].entity_list_.empty())
            {
                grid_list.erase(grid_id_old);
            }
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
