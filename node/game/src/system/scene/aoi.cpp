#include "aoi.h"

#include "comp/scene.h"
#include "comp/scene/grid.h"
#include "component_proto/actor_comp.pb.h"
#include "muduo/base/Logging.h"
#include "system/scene/hexagons_grid.h"
#include "thread_local/storage.h"

const Point kDefaultSize(10.0, 10.0);
const Point kOrigin(10.0, 10.0);
const auto KFlat = Layout(layout_flat, kDefaultSize, kOrigin);

void AoiSystem::Update(double delta)
{
    for (auto&& [player, transform, player_scene] : tls.registry.view<Transform, SceneEntity>().each())
    {
        if (!tls.scene_registry.valid(player_scene.scene_entity_))
        {
            LOG_ERROR << "scene not found " << tls.registry.get<Guid>(player);
            continue;
        }
        const auto grid_list = tls.scene_registry.try_get<SceneGridList>(player_scene.scene_entity_);
        if (nullptr == grid_list)
        {
            LOG_ERROR << "scene list not found " << tls.registry.get<Guid>(player);
            continue;
        }
        const auto index = GetGridIndex(transform.localtion());
        //to test
        (*grid_list)[index].entity_list_.emplace(player);
    }
}

absl::uint128 AoiSystem::GetGridIndex(const Location& l)
{
    const auto hex =
           pixel_to_hex(KFlat, Point(l.x(), l.y()));
    const absl::uint128 index = static_cast<absl::uint128>(hex.q) << 64 & static_cast<uint64_t>(hex.r);
    return index;
}
