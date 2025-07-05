#include "interest_system.h"

#include <entt/src/entt/entity/entity.hpp>

#include "scene/comp/scene_comp.h"
#include "scene/comp/scene_node_scene_comp.h"
#include "thread_local/storage.h"

void InterestSystem::InitializeActorComponents(const entt::entity entity)
{
    tls.actorRegistry.emplace<FollowerListComp>(entity);
    tls.actorRegistry.emplace<FollowingListComp>(entity);
    tls.actorRegistry.emplace<AoiListComp>(entity);
}

void InterestSystem::AddWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tls.actorRegistry.get<FollowerListComp>(target).followerList.insert(watcher);
    tls.actorRegistry.get<FollowingListComp>(watcher).followingList.insert(target);
}

void InterestSystem::RemoveWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tls.actorRegistry.get<FollowerListComp>(target).followerList.erase(watcher);
    tls.actorRegistry.get<FollowingListComp>(watcher).followingList.erase(target);
}

bool InterestSystem::IsInAoiList(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return false;

    return tls.actorRegistry.get<AoiListComp>(watcher).aoiList.contains(target);
}

void InterestSystem::AddAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return;

    tls.actorRegistry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestSystem::RemoveAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null ) return;

    tls.actorRegistry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestSystem::NotifyWatchers(const entt::entity target) {
}

void InterestSystem::NotifyWatcher(entt::entity watcher, entt::entity target) {
}
