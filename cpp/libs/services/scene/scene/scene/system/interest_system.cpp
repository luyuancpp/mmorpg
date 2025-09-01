#include "interest_system.h"

#include <entt/src/entt/entity/entity.hpp>

#include "scene/scene/comp/scene_node_scene_comp.h"

#include <threading/registry_manager.h>

void InterestSystem::InitializeActorComponents(const entt::entity entity)
{
    tlsRegistryManager.actorRegistry.emplace<FollowerListComp>(entity);
    tlsRegistryManager.actorRegistry.emplace<FollowingListComp>(entity);
    tlsRegistryManager.actorRegistry.emplace<AoiListComp>(entity);
}

void InterestSystem::AddWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tlsRegistryManager.actorRegistry.get<FollowerListComp>(target).followerList.insert(watcher);
    tlsRegistryManager.actorRegistry.get<FollowingListComp>(watcher).followingList.insert(target);
}

void InterestSystem::RemoveWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tlsRegistryManager.actorRegistry.get<FollowerListComp>(target).followerList.erase(watcher);
    tlsRegistryManager.actorRegistry.get<FollowingListComp>(watcher).followingList.erase(target);
}

bool InterestSystem::IsInAoiList(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return false;

    return tlsRegistryManager.actorRegistry.get<AoiListComp>(watcher).aoiList.contains(target);
}

void InterestSystem::AddAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return;

    tlsRegistryManager.actorRegistry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestSystem::RemoveAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null ) return;

    tlsRegistryManager.actorRegistry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestSystem::NotifyWatchers(const entt::entity target) {
}

void InterestSystem::NotifyWatcher(entt::entity watcher, entt::entity target) {
}
