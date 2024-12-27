﻿#include "message_system.h"

#include "game_common_logic/system/session_system.h"
#include "muduo/base/Logging.h"
#include "network/rpc_session.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "scene/comp/scene_comp.h"
#include "service_info/game_service_service_info.h"
#include "service_info/gate_service_service_info.h"
#include "thread_local/storage.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_define/type_define.h"

void SendToGs(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
	entt::entity gameNodeId{ nodeId };
	if (!tls.gameNodeRegistry.valid(gameNodeId))
	{
		LOG_ERROR << "Game node not found -> " << entt::to_integral(nodeId);
		return;
	}

	const auto gameSessionPtr = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeId);
	if (!gameSessionPtr)
	{
		LOG_ERROR << "RpcSession not found for game node -> " << entt::to_integral(nodeId);
		return;
	}

	(*gameSessionPtr)->SendRequest(messageId, message);
}

void SendToGsPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
    // 检查玩家实体是否有效
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "Invalid player entity -> " << entt::to_integral(player);
        return;
    }

    // 获取玩家的节点信息
    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "PlayerNodeInfo not found for player -> " << entt::to_integral(player);
        return;
    }

    // 获取游戏节点实体
    entt::entity gameNodeEntity{ playerNodeInfo->game_node_id() };

    // 检查游戏节点是否有效
    if (!tls.gameNodeRegistry.valid(gameNodeEntity))
    {
        LOG_ERROR << "Game node not found for player -> " << playerNodeInfo->game_node_id();
        return;
    }

    // 获取游戏节点的 RPC 会话
    auto gameSessionPtr = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeEntity);
    if (!gameSessionPtr)
    {
        LOG_ERROR << "RpcSession not found for game node -> " << playerNodeInfo->game_node_id()
            << " with message ID -> " << messageId;
        return;
    }

    // 准备请求消息
    NodeRouteMessageRequest request;
    const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
    request.mutable_body()->mutable_body()->resize(byteSize);
    message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize);
    request.mutable_body()->set_message_id(messageId);
    request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());

    // 发送请求到游戏节点
    (*gameSessionPtr)->SendRequest(GameServiceSendMessageToPlayerMessageId, request);
}


void SendToGsPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToGsPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToPlayerViaGs(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToPlayerViaGs(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToPlayerViaGs(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
    // 检查玩家实体是否有效
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "Invalid player entity -> " << entt::to_integral(player);
        return;
    }

    // 获取玩家的节点信息
    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "PlayerNodeInfo not found for player -> " << entt::to_integral(player);
        return;
    }

    // 获取游戏节点实体
    entt::entity gameNodeEntity{ playerNodeInfo->game_node_id() };

    // 检查游戏节点是否有效
    if (!tls.gameNodeRegistry.valid(gameNodeEntity))
    {
        LOG_ERROR << "Game node not found for player -> " << playerNodeInfo->game_node_id();
        return;
    }

    // 获取游戏节点的 RPC 会话
    auto rpcSession = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeEntity);
    if (!rpcSession)
    {
        LOG_ERROR << "RpcSession not found for game node -> " << playerNodeInfo->game_node_id();
        return;
    }

    // 准备请求消息
    NodeRouteMessageRequest request;
    const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
    request.mutable_body()->mutable_body()->resize(byteSize);
    message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize);
    request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());

    // 发送请求到游戏节点
    (*rpcSession)->SendRequest(messageId, request);
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
    // Check if player entity is valid
    if (!tls.registry.valid(player))
    {
        LOG_WARN << "Invalid player entity.";
        return;
    }

    // Retrieve player node info from the registry
    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
    if (!playerNodeInfo)
    {
        LOG_WARN << "Player node info not found for player entity " << entt::to_integral(player);
        return;
    }

    // Retrieve the gate session ID associated with the player
    entt::entity gateSessionId{ GetGateNodeId(playerNodeInfo->gate_session_id()) };
    if (!tls.gateNodeRegistry.valid(gateSessionId))
    {
        LOG_ERROR << "Gate session not found for player with session ID " << playerNodeInfo->gate_session_id();
        return;
    }

    // Retrieve the RpcSession for the gate node (rename to gateSessionPtr for clarity)
    const auto gateSessionPtr = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateSessionId);
    if (!gateSessionPtr)
    {
        LOG_ERROR << "RpcSession not found for gate with session ID " << playerNodeInfo->gate_session_id();
        return;
    }

    // Send the message to the player via the associated gate session
    SendMessageToPlayer(messageId, message, *gateSessionPtr, playerNodeInfo->gate_session_id());
}


void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, RpcSessionPtr& gate, uint64_t sessionId)
{
	NodeRouteMessageRequest request;
	const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
	request.mutable_body()->mutable_body()->resize(byteSize);
	message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize);
	request.mutable_head()->set_session_id(sessionId);
	request.mutable_body()->set_message_id(messageId);
	gate->SendRequest(GateServiceSendMessageToPlayerMessageId, request);
}

void SendMessageToPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendMessageToPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendMessageToGateById(const uint32_t messageId, const google::protobuf::Message& message, NodeId gateNodeId)
{
    // 尝试获取 gateNodeId 对应的 gate 实体
    entt::entity gateEntity{ gateNodeId };

    if (!tls.gateNodeRegistry.valid(gateEntity))
    {
        LOG_ERROR << "Gate not found for NodeId -> " << gateNodeId;
        return;
    }

    // 尝试从注册表中获取 gate 的会话对象
    const auto gateSessionPtr = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gateEntity);

    if (!gateSessionPtr)
    {
        LOG_ERROR << "RpcSession not found for Gate NodeId -> " << gateNodeId;
        return;
    }

    // 发送消息到指定的 gate 会话
    (*gateSessionPtr)->SendRequest(messageId, message);
}


void CallGamePlayerMethod(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
    // 检查玩家是否有效
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "Player entity is not valid.";
        return;
    }

    // 获取玩家节点信息
    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "PlayerNodeInfo not found for player -> " << entt::to_integral(player);
        return;
    }

    // 获取对应的游戏节点 ID
    entt::entity gameNodeEntity{ playerNodeInfo->game_node_id() };

    // 如果游戏节点无效，返回
    if (!tls.gameNodeRegistry.valid(gameNodeEntity))
    {
        LOG_ERROR << "Game node not valid for player -> " << entt::to_integral(player) << ", game_node_id: " << playerNodeInfo->game_node_id();
        return;
    }

    // 获取游戏节点的 RPC 会话
    const auto gameSession = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeEntity);
    if (!gameSession)
    {
        LOG_ERROR << "RpcSession not found for game node -> " << playerNodeInfo->game_node_id();
        return;
    }

    // 构造消息
    NodeRouteMessageRequest request;
    const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
    request.mutable_body()->mutable_body()->resize(byteSize);

    if (!message.SerializePartialToArray(request.mutable_body()->mutable_body()->data(), byteSize))
    {
        LOG_ERROR << "Failed to serialize message.";
        return;
    }

    // 设置消息 ID 和会话 ID
    request.mutable_body()->set_message_id(messageId);
    request.mutable_head()->set_session_id(playerNodeInfo->gate_session_id());

    // 发送消息
    (*gameSession)->CallRemoteMethod(GameServiceInvokePlayerServiceMessageId, request);
}

void CallGameNodeMethod(uint32_t messageId, const google::protobuf::Message& message, NodeId nodeId)
{
    // 获取对应的游戏节点实体
    entt::entity gameNodeEntity{ nodeId };

    // 如果游戏节点无效，直接返回
    if (!tls.gameNodeRegistry.valid(gameNodeEntity))
    {
        LOG_ERROR << "Game node entity is not valid for nodeId -> " << nodeId;
        return;
    }

    // 获取游戏节点的 RPC 会话
    const auto gameNodeSession = tls.gameNodeRegistry.try_get<RpcSessionPtr>(gameNodeEntity);
    if (!gameNodeSession)
    {
        LOG_ERROR << "RpcSession not found for game node -> " << nodeId;
        return;
    }

    // 调用游戏节点的远程方法
    (*gameNodeSession)->CallRemoteMethod(messageId, message);
}

void BroadCastToGame(uint32_t messageId, const google::protobuf::Message& message)
{

}
