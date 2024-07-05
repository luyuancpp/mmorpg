#include "aoi.h"

#include <component_proto/actor_comp.pb.h>

#include "hexagons_grid.h"
#include "comp/scene/grid.h"

#include "thread_local/storage.h"

const Point kDefaultSize(10.0, 10.0);
const Point kOrigin(10.0, 10.0);
const auto KFlat = Layout(layout_flat, kDefaultSize, kOrigin);

void AoiSystem::Update(double delta)
{
    for (auto&& [e, grid_list] : tls.scene_registry.view<SceneGridList>().each())
    {
        for (auto&& [e, transform] : tls.registry.view<Transform>().each())
        {
            const auto hex = pixel_to_hex(KFlat, Point(transform.localtion().x(), transform.localtion().y()));
            absl::uint128 key = static_cast<absl::uint128>(hex.q) << 64 & static_cast<uint64_t>(hex.r);
            //to test
            grid_list[key].entity_list_.emplace(e);
        }
    }
}
