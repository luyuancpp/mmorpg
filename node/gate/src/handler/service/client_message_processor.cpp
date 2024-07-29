#include "client_message_processor.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <ranges>

#include "gate_node.h"
#include "grpc/request/login_grpc_request.h"
#include "network/gate_session.h"
#include "service/centre_service_service.h"
#include "service/common_client_player_service.h"
#include "service/game_service_service.h"
#include "service/login_service_service.h"
#include "thread_local/storage_gate.h"
#include "util/random.h"
#include "util/snow_flake.h"

#include "proto/logic/tip_code/common_tip_code.pb.h"

extern std::unordered_set<uint32_t> g_c2s_service_id;

ClientMessageProcessor::ClientMessageProcessor(ProtobufCodec& codec,
	ProtobufDispatcher& dispatcher)
	: codec_(codec),
	dispatcher_(dispatcher)
{
	dispatcher_.registerMessageCallback<ClientRequest>(
		std::bind(&ClientMessageProcessor::OnRpcClientMessage, this, _1, _2, _3));
}

//todo 考虑中间一个login服务关了，原来的login服务器处理到一半，新的login处理不了
entt::entity ClientMessageProcessor::GetLoginNode(uint64_t sessionId)
{
	const auto sessionIt = tls_gate.sessions().find(sessionId);
	if (sessionIt == tls_gate.sessions().end())
	{
		return entt::null;
	}
	auto& session = sessionIt->second;
	if (!session.HasLoginNodeId())
	{
		const auto loginNodeIt = tls_gate.login_consistent_node().get_by_hash(sessionId);
		if (tls_gate.login_consistent_node().end() == loginNodeIt)
		{
			LOG_ERROR << "player login server not found session id : " << sessionId;
			return entt::null;
		}
		session.login_node_id_ = entt::to_integral(loginNodeIt->second);
	}
	const auto loginNodeIt = tls_gate.login_consistent_node().get_node_value(session.login_node_id_);
	if (tls_gate.login_consistent_node().end() == loginNodeIt)
	{
		LOG_ERROR << "player found login server crash : " << session.login_node_id_;
		session.login_node_id_ = kInvalidNodeId;
		return entt::null;
	}
	return loginNodeIt->second;
}

void ClientMessageProcessor::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
	// todo 改包把消息发给其他玩家怎么办
    // todo 玩家没登录直接发其他消息，乱发消息
	// todo如果我没登录就发送其他协议到controller game server 怎么办
	// Handle disconnection
	if (!conn->connected())
	{
		const auto sessionId = entt::to_integral(SessionId(conn));

		// 重要: 此消息一定要发，不能值通过controller 的gw disconnect去发
		// 重要: 比如:登录还没到controller,gw的disconnect 先到，登录后到，那么controller server 永远删除不了这个sessionid了
		// Disconnect from login server if session exists
		{
			if (const auto& loginNode = GetLoginNode(sessionId);
				entt::null != loginNode)
			{
				LoginNodeDisconnectRequest request;
				request.set_session_id(sessionId);
				SendDisconnectC2LRequest(loginNode, request);
			}
		}

		// Disconnect from centre server
		{
			GateSessionDisconnectRequest request;
			request.set_session_id(sessionId);
			g_gate_node->GetZoneCentreNode()->CallMethod(CentreServiceGateSessionDisconnectMsgId, request);
		}

		// Remove session from registry
		tls_gate.sessions().erase(sessionId);
	}
	else // Handle new connection
	{
		auto sessionId = tls_gate.session_id_gen().Generate();
		while (tls_gate.sessions().contains(sessionId))
		{
			sessionId = tls_gate.session_id_gen().Generate();
		}
		conn->setContext(sessionId);
		Session session;
		session.conn_ = conn;
		tls_gate.sessions().emplace(sessionId, std::move(session));
	}
}

void ClientMessageProcessor::OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
	const RpcClientMessagePtr& request,
	muduo::Timestamp)
{
	auto sessionId = SessionId(conn);
	const auto sessionIt = tls_gate.sessions().find(sessionId);
	if (sessionIt == tls_gate.sessions().end())
	{
		LOG_ERROR << "could not find session  " << conn.get();
		return;
	}

	// todo 发往登录服务器,如果以后可能有其他服务器那么就特写一下,根据协议名字发送的对应服务器,
	// 有没有更好的办法
	// Check if message id is valid
	if (g_c2s_service_id.contains(request->message_id()))
	{
		// Check if player can send this message id
		entt::entity gameNodeId{ sessionIt->second.game_node_id_ };
		if (!tls.gameNodeRegistry.valid(gameNodeId))
		{
			Tip(conn, kRetServerCrush);
			return;
		}

		auto gameNode = tls.gameNodeRegistry.get<RpcClientPtr>(gameNodeId);
		GameNodeRpcClientRequest message;
		message.set_body(request->body());
		message.set_session_id(sessionId);
		message.set_id(request->id());
		message.set_message_id(request->message_id());
		gameNode->CallMethod(GameServiceClientSend2PlayerMsgId, message);
	}
	else // Handle messages to login server
	{
		auto loginNode = GetLoginNode(sessionId);
		if (entt::null == loginNode)
		{
			//todo close connection
			return;
		}

		if (request->message_id() == LoginServiceLoginMsgId)
		{
			LoginC2LRequest message;
			message.mutable_session_info()->set_session_id(sessionId);
			message.mutable_client_msg_body()->ParseFromArray(
				request->body().data(), request->body().size());
			SendLoginC2LRequest(loginNode, message);
		}
		else if (request->message_id() == LoginServiceCreatePlayerMsgId)
		{
			CreatePlayerC2LRequest message;
			message.mutable_session_info()->set_session_id(sessionId);
			message.mutable_client_msg_body()->ParseFromArray(
				request->body().data(), request->body().size());
			SendCreatePlayerC2LRequest(loginNode, message);
		}
		else if (request->message_id() == LoginServiceEnterGameMsgId)
		{
			EnterGameC2LRequest message;
			message.mutable_session_info()->set_session_id(sessionId);
			message.mutable_client_msg_body()->ParseFromArray(
				request->body().data(), request->body().size());
			SendEnterGameC2LRequest(loginNode, message);
		}
	}
}

void ClientMessageProcessor::Tip(const muduo::net::TcpConnectionPtr& conn, uint32_t tipId)
{
	TipMessage tipMessage;
	tipMessage.mutable_tip_info()->set_id(tipId);
	MessageBody message;
	message.set_body(tipMessage.SerializeAsString());
	message.set_message_id(ClientPlayerCommonServicePushTipS2CMsgId);
	g_gate_node->Codec().send(conn, message);
}
