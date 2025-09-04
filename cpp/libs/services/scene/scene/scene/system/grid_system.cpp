#include "grid_system.h"

#include <muduo/base/Logging.h>

#include "hexagons_grid.h"
#include "view_system.h"
#include "proto/logic/component/actor_comp.pb.h"

#include <threading/registry_manager.h>
#include <modules/scene/comp/scene_comp.h>

const Point kDefaultSize(20.0, 20.0);
const Point kOrigin(0.0, 0.0);
const auto kHexLayout = Layout(layout_flat, kDefaultSize, kOrigin);

absl::uint128 GridSystem::GetGridId(const Vector3& pos) {
    return GetGridId(hex_round(pixel_to_hex(kHexLayout, Point(pos.x(), pos.y()))));
}

absl::uint128 GridSystem::GetGridId(const Hex& hex) {
    return static_cast<absl::uint128>(hex.q) << 64 | static_cast<uint64_t>(hex.r);
}

Hex GridSystem::CalculateHexPosition(const Transform& transform) {
    return hex_round(pixel_to_hex(kHexLayout, Point(transform.location().x(), transform.location().y())));
}

void GridSystem::GetNeighborGridIds(const Hex& hex, GridSet& gridSet) {
    for (int i = 0; i < 6; ++i) {
        gridSet.emplace(GridSystem::GetGridId(hex_neighbor(hex, i)));
    }
}

void GridSystem::GetCurrentAndNeighborGridIds(const Hex& hex, GridSet& gridSet) {
    auto currentGridId = GridSystem::GetGridId(hex);
    gridSet.emplace(currentGridId);
    GetNeighborGridIds(hex, gridSet);
}

void GridSystem::GetEntitiesInGridAndNeighbors(entt::entity entity, EntityUnorderedSet& entites, bool excludingSel)
{
    // 检查实体是否有效
    if (!tlsRegistryManager.actorRegistry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }
  
    // 获取实体所在的 Hex 位置
    const auto hexPosition = tlsRegistryManager.actorRegistry.try_get<Hex>(entity);
    if (!hexPosition) {
        return;
    }

    // 获取实体的场景组件
    const auto sceneComponent = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(entity);
    if (!sceneComponent) {
        return;
    }

    // 获取场景中的网格列表
    auto& gridList = tlsRegistryManager.roomRegistry.get<SceneGridListComp>(sceneComponent->roomEntity);
    
    // 存储要扫描的网格 ID
    GridSet grids;
    GetCurrentAndNeighborGridIds(*hexPosition, grids);

    // 遍历扫描到的网格 ID
    for (const auto& gridId : grids)
    {
        // 检查网格 ID 是否在网格列表中
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end())
        {
            continue;
        }

        // 遍历网格中的所有实体并将其添加到集合中
        for (const auto& gridEntity : gridIt->second.entities)
        {
            entites.emplace(gridEntity);
        }
    }

    if (excludingSel)
    {
        entites.erase(entity);
    }
}

void GridSystem::GetEntitiesInViewAndNearby(entt::entity entity, EntityUnorderedSet& entites)
{
    // 检查实体是否有效
    if (!tlsRegistryManager.actorRegistry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }
  
    // 获取实体所在的 Hex 位置
    const auto hexPosition = tlsRegistryManager.actorRegistry.try_get<Hex>(entity);
    if (!hexPosition) {
        return;
    }

    // 获取实体的场景组件
    const auto sceneComponent = tlsRegistryManager.actorRegistry.try_get<RoomEntityComp>(entity);
    if (!sceneComponent) {
        return;
    }

    // 获取场景中的网格列表
    auto& gridList = tlsRegistryManager.roomRegistry.get<SceneGridListComp>(sceneComponent->roomEntity);
    
    // 存储要扫描的网格 ID
    GridSet inViewGrids;
    GetCurrentAndNeighborGridIds(*hexPosition, inViewGrids);

    // 遍历扫描到的网格 ID
    for (const auto& gridId : inViewGrids)
    {
        // 检查网格 ID 是否在网格列表中
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end())
        {
            continue;
        }

        // 遍历网格中的所有实体并将其添加到集合中
        for (const auto& gridEntity : gridIt->second.entities)
        {
            if (gridEntity == entity)
            {
                continue;
            }

            double viewRadius = ViewSystem::GetMaxViewRadius(gridEntity);
            
            if (!ViewSystem::IsWithinViewRadius(gridEntity, entity, viewRadius))
            {
                continue;
            }
            
            entites.emplace(gridEntity);
        }
    }
}

void GridSystem::UpdateLogGridSize(double deltaTime) {
    for (auto&& [sceneEntity, gridList] : tlsRegistryManager.roomRegistry.view<SceneGridListComp>().each()) {
        for (const auto& [gridId, entityList] : gridList) {
            if (entityList.entities.empty()) {
                LOG_ERROR << "Grid is empty but not removed";
            }
        }
    }
}

void GridSystem::ClearEmptyGrids() {
    std::vector<absl::uint128> destroyEntities;
    for (auto&& [_, gridList] : tlsRegistryManager.actorRegistry.view<SceneGridListComp>().each()) {
        destroyEntities.clear();
        for (auto& it : gridList) {
            if (it.second.entities.empty()) {
                destroyEntities.emplace_back(it.first);
            }
        }

        for (auto&& it : destroyEntities) {
            gridList.erase(it);
        }
    }
}