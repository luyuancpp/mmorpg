#include "grid_util.h"

#include <muduo/base/Logging.h>

#include "proto/logic/component/actor_comp.pb.h"
#include "hexagons_grid.h"
#include "scene/comp/scene_comp.h"
#include "thread_local/storage.h"

const Point kDefaultSize(20.0, 20.0);
const Point kOrigin(0.0, 0.0);
const auto kHexLayout = Layout(layout_flat, kDefaultSize, kOrigin);

absl::uint128 GridUtil::GetGridId(const Location& location) {
    return GetGridId(hex_round(pixel_to_hex(kHexLayout, Point(location.x(), location.y()))));
}

absl::uint128 GridUtil::GetGridId(const Hex& hex) {
    return static_cast<absl::uint128>(hex.q) << 64 | static_cast<uint64_t>(hex.r);
}

Hex GridUtil::CalculateHexPosition(const Transform& transform) {
    return hex_round(pixel_to_hex(kHexLayout, Point(transform.location().x(), transform.location().y())));
}

void GridUtil::ScanNeighborGridIds(const Hex& hex, GridSet& gridSet) {
    for (int i = 0; i < 6; ++i) {
        gridSet.emplace(GridUtil::GetGridId(hex_neighbor(hex, i)));
    }
}

void GridUtil::ScanCurrentAndNeighborGridIds(const Hex& hex, GridSet& grid_set) {
    auto currentGridId = GridUtil::GetGridId(hex);
    grid_set.emplace(currentGridId);
    ScanNeighborGridIds(hex, grid_set);
}

void GridUtil::ScanCurrentAndNeighborGridEntities(entt::entity entity, EntityUnorderedSet& entities)
{
    // 检查实体是否有效
    if (!tls.registry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }
  
    // 获取实体所在的 Hex 位置
    const auto hexPosition = tls.registry.try_get<Hex>(entity);
    if (!hexPosition) {
        return;
    }

    // 获取实体的场景组件
    const auto sceneComponent = tls.registry.try_get<SceneEntityComp>(entity);
    if (!sceneComponent) {
        return;
    }

    // 获取场景中的网格列表
    auto& gridList = tls.sceneRegistry.get<SceneGridListComp>(sceneComponent->sceneEntity);
    
    // 存储要扫描的网格 ID
    GridSet scanGrid;
    ScanCurrentAndNeighborGridIds(*hexPosition, scanGrid);

    // 遍历扫描到的网格 ID
    for (const auto& gridId : scanGrid)
    {
        // 检查网格 ID 是否在网格列表中
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end())
        {
            continue;
        }

        // 遍历网格中的所有实体并将其添加到集合中
        for (const auto& gridEntity : gridIt->second.entity_list)
        {
            entities.emplace(gridEntity);
        }
    }
}