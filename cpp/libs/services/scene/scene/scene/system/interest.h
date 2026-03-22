#pragma once

#include <entt/src/entt/entity/entity.hpp>

class InterestSystem {
public:
    
    static void AddWatcher(const entt::entity watcher, const entt::entity target);

    static void RemoveWatcher(const entt::entity watcher, const entt::entity target);

    static bool IsInAoiList(const entt::entity watcher, const entt::entity target);
    
    static void AddAoiEntity(const entt::entity watcher, const entt::entity target);

    static void RemoveAoiEntity(const entt::entity watcher, const entt::entity target);

    // Notify all watchers observing a target entity
    static void NotifyWatchers(const entt::entity target);

private:
    // Watcher notification logic
    static void NotifyWatcher(entt::entity watcher, entt::entity target);
};

