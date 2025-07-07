#include "message_system.h"

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
#include "proto/common/node.pb.h"
#include "util/network_utils.h"

void SendToGsPlayer(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
    // 检查玩家实体是否有效
    if (!tls.actorRegistry.valid(player))
    {
        LOG_ERROR << "Invalid player entity -> " << entt::to_integral(player);
        return;
    }

    // 获取玩家的节点信息
    const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
    if (!playerSessionSnapshotPB)
    {
        LOG_ERROR << "PlayerNodeInfo not found for player -> " << entt::to_integral(player);
        return;
    }

    // 获取游戏节点实体
    entt::entity gameNodeEntity{ playerSessionSnapshotPB->scene_node_id() };

	auto& registry = tls.GetNodeRegistry(eNodeType::SceneNodeService);

    // 检查游戏节点是否有效
    if (!registry.valid(gameNodeEntity))
    {
        LOG_ERROR << "Game node not found for player -> " << playerSessionSnapshotPB->scene_node_id();
        return;
    }

    // 获取游戏节点的 RPC 会话
    auto sceneSession = registry.try_get<RpcSession>(gameNodeEntity);
    if (!sceneSession)
    {
        LOG_ERROR << "RpcSession not found for game node -> " << playerSessionSnapshotPB->scene_node_id()
            << " with message ID -> " << messageId;
        return;
    }

    // 准备请求消息
    NodeRouteMessageRequest request;
    const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
    request.mutable_message_content()->mutable_serialized_message()->resize(byteSize);
    message.SerializePartialToArray(request.mutable_message_content()->mutable_serialized_message()->data(), byteSize);
    request.mutable_message_content()->set_message_id(messageId);
    request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());

    // 发送请求到游戏节点
    sceneSession->SendRequest(SceneSendMessageToPlayerMessageId, request);
}


void SendToGsPlayer(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToGsPlayer(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToPlayerViaSceneNode(uint32_t messageId, const google::protobuf::Message& message, Guid playerId)
{
	SendToPlayerViaSceneNode(messageId, message, tlsCommonLogic.GetPlayer(playerId));
}

void SendToPlayerViaSceneNode(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
    // 检查玩家实体是否有效
    if (!tls.actorRegistry.valid(player))
    {
        LOG_ERROR << "Invalid player entity -> " << entt::to_integral(player);
        return;
    }

    // 获取玩家的节点信息
    const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
    if (!playerSessionSnapshotPB)
    {
        LOG_ERROR << "PlayerNodeInfo not found for player -> " << entt::to_integral(player);
        return;
    }

    // 获取游戏节点实体
    entt::entity gameNodeEntity{ playerSessionSnapshotPB->scene_node_id() };
	auto& registry = tls.GetNodeRegistry(eNodeType::SceneNodeService);

    // 检查游戏节点是否有效
    if (!registry.valid(gameNodeEntity))
    {
        LOG_ERROR << "Game node not found for player -> " << playerSessionSnapshotPB->scene_node_id();
        return;
    }

    // 获取游戏节点的 RPC 会话
    auto sceneSession = registry.try_get<RpcSession>(gameNodeEntity);
    if (!sceneSession)
    {
        LOG_ERROR << "RpcSession not found for game node -> " << playerSessionSnapshotPB->scene_node_id();
        return;
    }

    // 准备请求消息
    NodeRouteMessageRequest request;
    const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
    request.mutable_message_content()->mutable_serialized_message()->resize(byteSize);
    message.SerializePartialToArray(request.mutable_message_content()->mutable_serialized_message()->data(), byteSize);
    request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());

    // 发送请求到游戏节点
    sceneSession->SendRequest(messageId, request);
}

void CallScenePlayerMethod(uint32_t messageId, const google::protobuf::Message& message, entt::entity player)
{
    // 检查玩家是否有效
    if (!tls.actorRegistry.valid(player))
    {
        LOG_ERROR << "Player entity is not valid.";
        return;
    }

    // 获取玩家节点信息
    const auto* playerSessionSnapshotPB = tls.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
    if (!playerSessionSnapshotPB)
    {
        LOG_ERROR << "PlayerNodeInfo not found for player -> " << entt::to_integral(player);
        return;
    }

    // 获取对应的游戏节点 ID
    entt::entity sceneNodeEntity{ playerSessionSnapshotPB->scene_node_id() };
	auto& registry = tls.GetNodeRegistry(eNodeType::SceneNodeService);

    // 如果游戏节点无效，返回
    if (!registry.valid(sceneNodeEntity))
    {
        LOG_ERROR << "Game node not valid for player -> " << entt::to_integral(player) << ", game_node_id: " << playerSessionSnapshotPB->scene_node_id();
        return;
    }

    // 获取游戏节点的 RPC 会话
    const auto sceneSession = registry.try_get<RpcSession>(sceneNodeEntity);
    if (!sceneSession)
    {
        LOG_ERROR << "RpcSession not found for game node -> " << playerSessionSnapshotPB->scene_node_id();
        return;
    }

    // 构造消息
    NodeRouteMessageRequest request;
    const int32_t byteSize = static_cast<int32_t>(message.ByteSizeLong());
    request.mutable_message_content()->mutable_serialized_message()->resize(byteSize);

    if (!message.SerializePartialToArray(request.mutable_message_content()->mutable_serialized_message()->data(), byteSize))
    {
        LOG_ERROR << "Failed to serialize message.";
        return;
    }

    // 设置消息 ID 和会话 ID
    request.mutable_message_content()->set_message_id(messageId);
    request.mutable_header()->set_session_id(playerSessionSnapshotPB->gate_session_id());

    // 发送消息
    sceneSession->CallRemoteMethod(SceneInvokePlayerServiceMessageId, request);
}
