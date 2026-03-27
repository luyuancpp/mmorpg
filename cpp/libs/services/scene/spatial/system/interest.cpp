#include "interest.h"

#include <entt/src/entt/entity/entity.hpp>

#include "spatial/comp/scene_node_scene_comp.h"

#include <thread_context/registry_manager.h>

void InterestSystem::AddAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return;

    tlsEcs.actorRegistry.get_or_emplace<AoiListComp>(watcher).aoiList.insert(target);
}

void InterestSystem::RemoveAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return;

    auto* aoiList = tlsEcs.actorRegistry.try_get<AoiListComp>(watcher);
    if (aoiList == nullptr) return;
    aoiList->aoiList.erase(target);
}

