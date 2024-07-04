#include "aoi.h"

#include <component_proto/actor_comp.pb.h>

#include "hexagons_grid.h"
#include "comp/scene/grid.h"

#include "thread_local/storage.h"

Point kDefaultSize(10.0, 10.0);

void AoiSystem::Update(entt::entity scene)
{
    auto&& grid_list = tls.scene_registry.get<SceneGridList>(scene);
    for (auto&&[e, transform, velocity] : tls.registry.view<Transform, Velocity>().each())
    {
        const auto flat = Layout(layout_flat, kDefaultSize, Point(35.0, 71.0));
        const auto hex = pixel_to_hex(flat, Point(transform.localtion().x(), transform.localtion().y()));
        const uint64_t q = static_cast<uint64_t>(hex.q);
        const uint64_t r = static_cast<uint64_t>(hex.r);
        absl::uint128 key = q ;
        key = key << 64 & r;
        if (grid_list.contains(key))
        {
            
        }
        else
        {
            auto it = grid_list.emplace(key, HexagonsGrid{});
            if (it.second)
            {
                
            }
        }
    }
}
