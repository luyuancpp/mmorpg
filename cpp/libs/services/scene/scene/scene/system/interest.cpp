#include "interest.h"

#include <entt/src/entt/entity/entity.hpp>

#include "scene/scene/comp/scene_node_scene.h"

#include <threading/registry_manager.h>

void InterestSystem::AddWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tlsRegistryManager.actorRegistry.get_or_emplace<FollowerListComp>(target).followerList.insert(watcher);
    tlsRegistryManager.actorRegistry.get_or_emplace<FollowingListComp>(watcher).followingList.insert(target);
}

void InterestSystem::RemoveWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tlsRegistryManager.actorRegistry.get_or_emplace<FollowerListComp>(target).followerList.erase(watcher);
    tlsRegistryManager.actorRegistry.get_or_emplace<FollowingListComp>(watcher).followingList.erase(target);
}

bool InterestSystem::IsInAoiList(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return false;

    return tlsRegistryManager.actorRegistry.get_or_emplace<AoiListComp>(watcher).aoiList.contains(target);
}

void InterestSystem::AddAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return;

    tlsRegistryManager.actorRegistry.get_or_emplace<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestSystem::RemoveAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null ) return;

    tlsRegistryManager.actorRegistry.get_or_emplace<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestSystem::NotifyWatchers(const entt::entity target) {
}

void InterestSystem::NotifyWatcher(entt::entity watcher, entt::entity target) {
}
