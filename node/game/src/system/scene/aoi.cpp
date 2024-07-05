#include "aoi.h"

#include <component_proto/actor_comp.pb.h>

#include "muduo/base/Logging.h"
#include "hexagons_grid.h"
#include "comp/scene/grid.h"
#include "comp/scene.h"

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
        const auto hex =
            pixel_to_hex(KFlat, Point(transform.localtion().x(), transform.localtion().y()));
        absl::uint128 key = static_cast<absl::uint128>(hex.q) << 64 & static_cast<uint64_t>(hex.r);
        //to test
        (*grid_list)[key].entity_list_.emplace(player);
    }
}
