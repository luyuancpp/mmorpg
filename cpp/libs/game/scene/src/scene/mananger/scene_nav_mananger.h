#pragma once

#include "scene/comp/nav_comp.h"

using SceneNavMapComp = std::unordered_map<uint32_t, NavComp>;

class SceneNavManager
{
public:
    SceneNavManager() = default;

    SceneNavManager(const SceneNavManager&) = delete;
    SceneNavManager& operator=(const SceneNavManager&) = delete;

    static SceneNavManager& Instance() {
        thread_local SceneNavManager instance;
        return instance;
    }

    void AddNav(uint32_t id, NavComp&& nav) { sceneNav.emplace(id, std::move(nav)); }

    bool Contains(uint32_t id) { return sceneNav.contains(id); }

	SceneNavMapComp sceneNav;
};