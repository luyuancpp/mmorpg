#pragma once
#include "entt/src/entt/entity/entity.hpp"
#include "engine/core/utils/registry/scene_entity.h"

// 场景生命周期的 scene 属于 sceneRegistry;离场通知的 actor 属于 actorRegistry。
struct OnSceneCreated { SceneEntity scene{entt::null}; };
struct OnSceneDestroyed { SceneEntity scene{entt::null}; };
struct BeforeLeaveScene { entt::entity actor{entt::null}; };
