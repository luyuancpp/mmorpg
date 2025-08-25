#pragma once

#include <entt/src/entt/entity/entity.hpp>

class InterestSystem {
public:
    static void InitializeActorComponents(entt::entity entity);
    
    static void AddWatcher(const entt::entity watcher, const entt::entity target);

    static void RemoveWatcher(const entt::entity watcher, const entt::entity target);

    static bool IsInAoiList(const entt::entity watcher, const entt::entity target);
    
    static void AddAoiEntity(const entt::entity watcher, const entt::entity target);

    static void RemoveAoiEntity(const entt::entity watcher, const entt::entity target);

    // 通知所有关注某个实体的观察者
    static void NotifyWatchers(const entt::entity target);

private:
    // 更新观察者的逻辑
    static void NotifyWatcher(entt::entity watcher, entt::entity target);
};

