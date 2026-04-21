#include "grid.h"

#include <muduo/base/Logging.h>

#include "hexagons_grid.h"
#include "view.h"
#include "proto/common/component/actor_comp.pb.h"

#include <modules/scene/comp/scene_comp.h>
#include <thread_context/ecs_context.h>

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

void GridSystem::GetEntitiesInGridAndNeighbors(entt::entity entity, EntityUnorderedSet& entities, bool excludingSelf)
{
    if (!tlsEcs.actorRegistry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }
  
    const auto hexPosition = tlsEcs.actorRegistry.try_get<Hex>(entity);
    if (!hexPosition) {
        return;
    }

    const auto sceneComponent = tlsEcs.actorRegistry.try_get<SceneEntityComp>(entity);
    if (!sceneComponent) {
        return;
    }

    auto& gridList = tlsEcs.sceneRegistry.get_or_emplace<SceneGridListComp>(sceneComponent->sceneEntity);
    
    GridSet grids;
    GetCurrentAndNeighborGridIds(*hexPosition, grids);

    for (const auto& gridId : grids)
    {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end())
        {
            continue;
        }

        for (const auto& gridEntity : gridIt->second.entities)
        {
            entities.emplace(gridEntity);
        }
    }

    if (excludingSelf)
    {
        entities.erase(entity);
    }
}

void GridSystem::GetEntitiesInViewAndNearby(entt::entity entity, EntityUnorderedSet& entities)
{
    if (!tlsEcs.actorRegistry.valid(entity)) {
        LOG_ERROR << "Entity not found in scene";
        return;
    }
  
    const auto hexPosition = tlsEcs.actorRegistry.try_get<Hex>(entity);
    if (!hexPosition) {
        return;
    }

    const auto sceneComponent = tlsEcs.actorRegistry.try_get<SceneEntityComp>(entity);
    if (!sceneComponent) {
        return;
    }

    auto& gridList = tlsEcs.sceneRegistry.get_or_emplace<SceneGridListComp>(sceneComponent->sceneEntity);
    
    GridSet inViewGrids;
    GetCurrentAndNeighborGridIds(*hexPosition, inViewGrids);

    for (const auto& gridId : inViewGrids)
    {
        auto gridIt = gridList.find(gridId);
        if (gridIt == gridList.end())
        {
            continue;
        }

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
            
            entities.emplace(gridEntity);
        }
    }
}

void GridSystem::UpdateLogGridSize() {
    for (auto&& [sceneEntity, gridList] : tlsEcs.sceneRegistry.view<SceneGridListComp>().each()) {
        for (const auto& [gridId, entityList] : gridList) {
            if (entityList.entities.empty()) {
                LOG_ERROR << "Grid is empty but not removed";
            }
        }
    }
}
