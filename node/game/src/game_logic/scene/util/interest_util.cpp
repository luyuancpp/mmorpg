#include "interest_util.h"

#include <entt/src/entt/entity/entity.hpp>

#include "game_logic/scene/comp/scene_comp.h"
#include "thread_local/storage.h"

void InterestManager::InitializeActorComponents(const entt::entity entity)
{
    tls.registry.emplace<FollowerListComp>(entity);
    tls.registry.emplace<FollowingListComp>(entity);
    tls.registry.emplace<AoiListComp>(entity);
}

void InterestManager::AddWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tls.registry.get<FollowerListComp>(target).followerList.insert(watcher);
    tls.registry.get<FollowingListComp>(watcher).followingList.insert(target);
}

void InterestManager::RemoveWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tls.registry.get<FollowerListComp>(target).followerList.erase(watcher);
    tls.registry.get<FollowingListComp>(watcher).followingList.erase(target);
}

void InterestManager::AddAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null || !tls.registry.any_of<AoiListComp>(watcher)) return;

    tls.registry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestManager::RemoveAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null || !tls.registry.any_of<AoiListComp>(watcher)) return;

    tls.registry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestManager::NotifyWatchers(const entt::entity target) {
}

void InterestManager::NotifyWatcher(entt::entity watcher, entt::entity target) {
}
