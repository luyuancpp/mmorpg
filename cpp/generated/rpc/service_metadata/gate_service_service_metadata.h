#pragma once
#include <cstdint>

#include "proto/gate/gate_service.pb.h"

constexpr uint32_t GatePlayerEnterGameNodeMessageId = 42;
constexpr uint32_t GatePlayerEnterGameNodeIndex = 0;
#define GatePlayerEnterGameNodeMethod  ::Gate_Stub::descriptor()->method(0)

constexpr uint32_t GateSendMessageToPlayerMessageId = 72;
constexpr uint32_t GateSendMessageToPlayerIndex = 1;
#define GateSendMessageToPlayerMethod  ::Gate_Stub::descriptor()->method(1)

constexpr uint32_t GateRouteNodeMessageMessageId = 41;
constexpr uint32_t GateRouteNodeMessageIndex = 2;
#define GateRouteNodeMessageMethod  ::Gate_Stub::descriptor()->method(2)

constexpr uint32_t GateRoutePlayerMessageMessageId = 74;
constexpr uint32_t GateRoutePlayerMessageIndex = 3;
#define GateRoutePlayerMessageMethod  ::Gate_Stub::descriptor()->method(3)

constexpr uint32_t GateBroadcastToPlayersMessageId = 32;
constexpr uint32_t GateBroadcastToPlayersIndex = 4;
#define GateBroadcastToPlayersMethod  ::Gate_Stub::descriptor()->method(4)

constexpr uint32_t GateBroadcastToSceneMessageId = 57;
constexpr uint32_t GateBroadcastToSceneIndex = 5;
#define GateBroadcastToSceneMethod  ::Gate_Stub::descriptor()->method(5)

constexpr uint32_t GateBroadcastToAllMessageId = 71;
constexpr uint32_t GateBroadcastToAllIndex = 6;
#define GateBroadcastToAllMethod  ::Gate_Stub::descriptor()->method(6)

constexpr uint32_t GateNodeHandshakeMessageId = 45;
constexpr uint32_t GateNodeHandshakeIndex = 7;
#define GateNodeHandshakeMethod  ::Gate_Stub::descriptor()->method(7)

constexpr uint32_t GateBindSessionToGateMessageId = 56;
constexpr uint32_t GateBindSessionToGateIndex = 8;
#define GateBindSessionToGateMethod  ::Gate_Stub::descriptor()->method(8)

constexpr uint32_t GateGmGracefulShutdownMessageId = 125;
constexpr uint32_t GateGmGracefulShutdownIndex = 9;
#define GateGmGracefulShutdownMethod  ::Gate_Stub::descriptor()->method(9)

// 节点间 RPC0 的方法 tag(docs/design/muduo-rpc-channel.md §10):RpcChannel / RpcClient / RpcSession 的
// CallMethod<Tag> / SendRequest<Tag> 按它取消息号与请求 / 响应类型。只含类型信息、不依赖 core;
// 请求 / 响应类型在此前向声明,tag 本身不要求完整类型。
class BindSessionToGateRequest;
class BindSessionToGateResponse;
class BroadcastToAllRequest;
class BroadcastToPlayersRequest;
class BroadcastToSceneRequest;
class Empty;
class GmGracefulShutdownRequest;
class GmGracefulShutdownResponse;
class NodeHandshakeRequest;
class NodeHandshakeResponse;
class NodeRouteMessageRequest;
class RegisterGameNodeSessionRequest;
class RegisterGameNodeSessionResponse;
class RouteMessageRequest;
class RouteMessageResponse;
class RoutePlayerMessageRequest;
class RoutePlayerMessageResponse;

struct GatePlayerEnterGameNodeRpc
{
    static constexpr uint32_t kMessageId = GatePlayerEnterGameNodeMessageId;
    using Request = ::RegisterGameNodeSessionRequest;
    using Response = ::RegisterGameNodeSessionResponse;
};

struct GateSendMessageToPlayerRpc
{
    static constexpr uint32_t kMessageId = GateSendMessageToPlayerMessageId;
    using Request = ::NodeRouteMessageRequest;
    using Response = ::Empty;
};

struct GateRouteNodeMessageRpc
{
    static constexpr uint32_t kMessageId = GateRouteNodeMessageMessageId;
    using Request = ::RouteMessageRequest;
    using Response = ::RouteMessageResponse;
};

struct GateRoutePlayerMessageRpc
{
    static constexpr uint32_t kMessageId = GateRoutePlayerMessageMessageId;
    using Request = ::RoutePlayerMessageRequest;
    using Response = ::RoutePlayerMessageResponse;
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

struct GateBindSessionToGateRpc
{
    static constexpr uint32_t kMessageId = GateBindSessionToGateMessageId;
    using Request = ::BindSessionToGateRequest;
    using Response = ::BindSessionToGateResponse;
};

struct GateGmGracefulShutdownRpc
{
    static constexpr uint32_t kMessageId = GateGmGracefulShutdownMessageId;
    using Request = ::GmGracefulShutdownRequest;
    using Response = ::GmGracefulShutdownResponse;
};
