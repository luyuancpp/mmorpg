#pragma once

#include "scene/comp/nav_comp.h"

using SceneNavMapComp = std::unordered_map<uint32_t, NavComp>;

class SceneNavManager
{
public:
    SceneNavManager() = default;

    // 禁止拷贝和移动，确保单例唯一性
    SceneNavManager(const SceneNavManager&) = delete;
    SceneNavManager& operator=(const SceneNavManager&) = delete;

    /**
     * 获取线程局部的单例实例
     * @return 线程唯一的 ThreadLocalNodeContext 实例
     */
    static SceneNavManager& Instance() {
        thread_local SceneNavManager instance;
        return instance;
    }

    void AddNav(uint32_t id, NavComp&& nav) { sceneNav.emplace(id, std::move(nav)); }

    bool Contains(uint32_t id) { return sceneNav.contains(id); }

	SceneNavMapComp sceneNav;
};