#pragma once
#include <cstdint>

#include "proto/scene/scene_admin.pb.h"

constexpr uint32_t SceneSceneTestMessageId = 18;
constexpr uint32_t SceneSceneTestIndex = 0;
#define SceneSceneTestMethod  ::SceneScene_Stub::descriptor()->method(0)

constexpr uint32_t SceneSceneGmGracefulShutdownMessageId = 126;
constexpr uint32_t SceneSceneGmGracefulShutdownIndex = 1;
#define SceneSceneGmGracefulShutdownMethod  ::SceneScene_Stub::descriptor()->method(1)

// 节点间 RPC0 的方法 tag(docs/design/muduo-rpc-channel.md §10):RpcChannel / RpcClient / RpcSession 的
// CallMethod<Tag> / SendRequest<Tag> 按它取消息号与请求 / 响应类型。只含类型信息、不依赖 core;
// 请求 / 响应类型在此前向声明,tag 本身不要求完整类型。
class Empty;
class GameSceneTest;
class GmGracefulShutdownRequest;
class GmGracefulShutdownResponse;

struct SceneSceneTestRpc
{
    static constexpr uint32_t kMessageId = SceneSceneTestMessageId;
    using Request = ::GameSceneTest;
    using Response = ::Empty;
};

struct SceneSceneGmGracefulShutdownRpc
{
    static constexpr uint32_t kMessageId = SceneSceneGmGracefulShutdownMessageId;
    using Request = ::GmGracefulShutdownRequest;
    using Response = ::GmGracefulShutdownResponse;
};
