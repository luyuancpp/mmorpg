#include "interest_util.h"

#include <entt/src/entt/entity/entity.hpp>

#include "game_logic/scene/comp/scene_comp.h"
#include "thread_local/storage.h"

void InterestUtil::InitializeActorComponents(const entt::entity entity)
{
    tls.registry.emplace<FollowerListComp>(entity);
    tls.registry.emplace<FollowingListComp>(entity);
    tls.registry.emplace<AoiListComp>(entity);
}

void InterestUtil::AddWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tls.registry.get<FollowerListComp>(target).followerList.insert(watcher);
    tls.registry.get<FollowingListComp>(watcher).followingList.insert(target);
}

void InterestUtil::RemoveWatcher(const entt::entity watcher, const entt::entity target) {
    if (watcher == entt::null || target == entt::null) return;

    tls.registry.get<FollowerListComp>(target).followerList.erase(watcher);
    tls.registry.get<FollowingListComp>(watcher).followingList.erase(target);
}

bool InterestUtil::IsInAoiList(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return false;

    return tls.registry.get<AoiListComp>(watcher).aoiList.contains(target);
}

void InterestUtil::AddAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return;

    tls.registry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestUtil::RemoveAoiEntity(const entt::entity watcher, const entt::entity target)
{
    if (watcher == entt::null || target == entt::null ) return;

    tls.registry.get<AoiListComp>(watcher).aoiList.erase(target);
}

void InterestUtil::NotifyWatchers(const entt::entity target) {
}

void InterestUtil::NotifyWatcher(entt::entity watcher, entt::entity target) {
}
