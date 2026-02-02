
#include "gate_service_response_handler.h"

#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "engine/core/type_define/type_define.h"

#include "scene/system/player_change_room.h"
#include "core/utils/registry/game_registry.h"
#include "player/system/player_lifecycle.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include <threading/player_manager.h>
#include <threading/registry_manager.h>
#include <type_alias/player_session_type_alias.h>
///<<< END WRITING YOUR CODE



void InitGateReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::RegisterGameNodeSessionResponse>(GatePlayerEnterGameNodeMessageId,
        std::bind(&OnGatePlayerEnterGameNodeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(GateSendMessageToPlayerMessageId,
        std::bind(&OnGateSendMessageToPlayerReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(GateKickSessionByCentreMessageId,
        std::bind(&OnGateKickSessionByCentreReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(GateRouteNodeMessageMessageId,
        std::bind(&OnGateRouteNodeMessageReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(GateRoutePlayerMessageMessageId,
        std::bind(&OnGateRoutePlayerMessageReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(GateBroadcastToPlayersMessageId,
        std::bind(&OnGateBroadcastToPlayersReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(GateNodeHandshakeMessageId,
        std::bind(&OnGateNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::BindSessionToGateResponse>(GateBindSessionToGateMessageId,
        std::bind(&OnGateBindSessionToGateReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGatePlayerEnterGameNodeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterGameNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	///gate 更新gs,相应的gs可以往那个gate上发消息了
	///todo 中间返回是断开了
	entt::entity GetPlayerEntityBySessionId(uint64_t session_id);
	const auto player = GetPlayerEntityBySessionId(replied->session_info().session_id());
	if (entt::null == player)
	{
		LOG_TRACE << "session player not found " << replied->session_info().session_id();
		return;
	}
	
	PlayerLifecycleSystem::HandleBindPlayerToGateOK(player);
	PlayerLifecycleSystem::ProcessPlayerSessionState(player);
	PlayerChangeRoomUtil::OnTargetSceneNodeEnterComplete(player);
	PlayerChangeRoomUtil::ProgressSceneChangeState(player);
///<<< END WRITING YOUR CODE
}

void OnGateSendMessageToPlayerReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateKickSessionByCentreReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateRouteNodeMessageReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateRoutePlayerMessageReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateBroadcastToPlayersReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateNodeHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateBindSessionToGateReply(const TcpConnectionPtr& conn, const std::shared_ptr<::BindSessionToGateResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	const uint64_t sessionId = replied->session_id();
	const Guid playerId = replied->player_id();
	const uint64_t respVersion = replied->session_version();

	// 1️⃣ 玩家是否还在内存
	auto it = tlsPlayerList.find(playerId);
	if (it == tlsPlayerList.end()) {
		LOG_INFO << "BindResp ignored: player not exist, player=" << playerId;
		return;
	}

	entt::entity playerEntity = it->second;
	auto* sessionPB =
		tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!sessionPB) {
		LOG_INFO << "BindResp ignored: no snapshot, player=" << playerId;
		return;
	}

	// 2️⃣ 版本校验（核心）
	if (sessionPB->session_version() != respVersion) {
		LOG_INFO << "BindResp ignored: version mismatch "
			<< "resp=" << respVersion
			<< " cur=" << sessionPB->session_version()
			<< " player=" << playerId;
		return;
	}

	// 3️⃣ session_id 校验（防旧 ACK）
	if (sessionPB->gate_session_id() != sessionId) {
		LOG_INFO << "BindResp ignored: session mismatch "
			<< "resp=" << sessionId
			<< " cur=" << sessionPB->gate_session_id()
			<< " player=" << playerId;
		return;
	}

	// 4 一切匹配 → 现在才安全写 SessionMap
	SessionMap()[sessionId] = playerId;

	LOG_INFO << "BindResp accepted: session=" << sessionId
		<< " player=" << playerId
		<< " version=" << respVersion;
///<<< END WRITING YOUR CODE
}
