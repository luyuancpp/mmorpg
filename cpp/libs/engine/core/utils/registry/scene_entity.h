#pragma once

#include <cstdint>
#include "entt/src/entt/entity/registry.hpp"

// sceneRegistry 专用句柄，与 actorRegistry 的 entt::entity 在类型上隔离。
// EnTT 按枚举底层类型推导 index/version 位宽，句柄仅在所属进程有效。
enum class SceneEntity : std::uint64_t {};
using SceneRegistry = entt::basic_registry<SceneEntity>;
