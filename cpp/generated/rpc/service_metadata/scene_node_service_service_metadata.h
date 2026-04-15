#pragma once
#include <cstdint>

#include "proto/scene_manager/scene_node_service.pb.h"

constexpr uint32_t SceneNodeGrpcCreateSceneMessageId = 122;
constexpr uint32_t SceneNodeGrpcCreateSceneIndex = 0;
#define SceneNodeGrpcCreateSceneMethod  ::SceneNodeGrpc_Stub::descriptor()->method(0)

constexpr uint32_t SceneNodeGrpcDestroySceneMessageId = 123;
constexpr uint32_t SceneNodeGrpcDestroySceneIndex = 1;
#define SceneNodeGrpcDestroySceneMethod  ::SceneNodeGrpc_Stub::descriptor()->method(1)
