#pragma once
#include <cstdint>

#include "proto/gate/gate_service.pb.h"

constexpr uint32_t GateSendMessageToPlayerMessageId = 72;
constexpr uint32_t GateSendMessageToPlayerIndex = 0;
#define GateSendMessageToPlayerMethod  ::Gate_Stub::descriptor()->method(0)

constexpr uint32_t GateBroadcastToPlayersMessageId = 32;
constexpr uint32_t GateBroadcastToPlayersIndex = 1;
#define GateBroadcastToPlayersMethod  ::Gate_Stub::descriptor()->method(1)

constexpr uint32_t GateBroadcastToSceneMessageId = 57;
constexpr uint32_t GateBroadcastToSceneIndex = 2;
#define GateBroadcastToSceneMethod  ::Gate_Stub::descriptor()->method(2)

constexpr uint32_t GateBroadcastToAllMessageId = 71;
constexpr uint32_t GateBroadcastToAllIndex = 3;
#define GateBroadcastToAllMethod  ::Gate_Stub::descriptor()->method(3)

constexpr uint32_t GateNodeHandshakeMessageId = 45;
constexpr uint32_t GateNodeHandshakeIndex = 4;
#define GateNodeHandshakeMethod  ::Gate_Stub::descriptor()->method(4)

constexpr uint32_t GateGmGracefulShutdownMessageId = 125;
constexpr uint32_t GateGmGracefulShutdownIndex = 5;
#define GateGmGracefulShutdownMethod  ::Gate_Stub::descriptor()->method(5)

// 节点间 RPC0 的方法 tag(docs/design/muduo-rpc-channel.md §10):RpcChannel / RpcClient / RpcSession 的
// CallMethod<Tag> / SendRequest<Tag> 按它取消息号与请求 / 响应类型。只含类型信息、不依赖 core;
// 请求 / 响应类型在此前向声明,tag 本身不要求完整类型。
class BroadcastToAllRequest;
class BroadcastToPlayersRequest;
class BroadcastToSceneRequest;
class Empty;
class GmGracefulShutdownRequest;
class GmGracefulShutdownResponse;
class NodeHandshakeRequest;
class NodeHandshakeResponse;
class NodeRouteMessageRequest;

struct GateSendMessageToPlayerRpc
{
    static constexpr uint32_t kMessageId = GateSendMessageToPlayerMessageId;
    using Request = ::NodeRouteMessageRequest;
    using Response = ::Empty;
};

struct GateBroadcastToPlayersRpc
{
    static constexpr uint32_t kMessageId = GateBroadcastToPlayersMessageId;
    using Request = ::BroadcastToPlayersRequest;
    using Response = ::Empty;
};

struct GateBroadcastToSceneRpc
{
    static constexpr uint32_t kMessageId = GateBroadcastToSceneMessageId;
    using Request = ::BroadcastToSceneRequest;
    using Response = ::Empty;
};

struct GateBroadcastToAllRpc
{
    static constexpr uint32_t kMessageId = GateBroadcastToAllMessageId;
    using Request = ::BroadcastToAllRequest;
    using Response = ::Empty;
};

struct GateNodeHandshakeRpc
{
    static constexpr uint32_t kMessageId = GateNodeHandshakeMessageId;
    using Request = ::NodeHandshakeRequest;
    using Response = ::NodeHandshakeResponse;
};

struct GateGmGracefulShutdownRpc
{
    static constexpr uint32_t kMessageId = GateGmGracefulShutdownMessageId;
    using Request = ::GmGracefulShutdownRequest;
    using Response = ::GmGracefulShutdownResponse;
};
