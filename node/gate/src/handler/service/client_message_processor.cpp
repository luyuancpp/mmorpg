#include "client_message_processor.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <ranges>

#include "gate_node.h"
#include "grpc/request/login_grpc_request.h"
#include "network/gate_session.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/player_common_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/login_service_service_info.h"
#include "thread_local/storage_gate.h"
#include "pbc/common_error_tip.pb.h"
#include "util/random.h"
#include "util/snow_flake.h"


extern std::unordered_set<uint32_t> g_c2s_service_id;

RpcClientHandler::RpcClientHandler(ProtobufCodec& codec,
	ProtobufDispatcher& dispatcher)
	: protobufCodec(codec),
	messageDispatcher(dispatcher)
{
	messageDispatcher.registerMessageCallback<ClientRequest>(
		std::bind(&RpcClientHandler::ProcessRpcRequest, this, _1, _2, _3));
}

//todo 考虑中间一个login服务关了，原来的login服务器处理到一半，新的login处理不了
entt::entity RpcClientHandler::GetLoginNode(uint64_t sessionId)
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
			LOG_ERROR << "Player login server not found for session id: " << sessionId;
			return entt::null;
		}
		session.login_node_id_ = entt::to_integral(loginNodeIt->second);
	}

	const auto loginNodeIt = tls_gate.login_consistent_node().get_node_value(session.login_node_id_);
	if (tls_gate.login_consistent_node().end() == loginNodeIt)
	{
		LOG_ERROR << "Player found login server crash: " << session.login_node_id_;
		session.login_node_id_ = kInvalidNodeId;
		return entt::null;
	}
	return loginNodeIt->second;
}

void RpcClientHandler::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
	// todo 改包把消息发给其他玩家怎么办
    // todo 玩家没登录直接发其他消息，乱发消息
	// todo如果我没登录就发送其他协议到controller game server 怎么办
	// Handle disconnection
	if (conn->connected())
	{
		HandleConnectionEstablished(conn);
	}
	else
	{
		HandleDisconnection(conn);
	}
}

void RpcClientHandler::SendMessageToClient(const muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& message) const
{
	protobufCodec.send(conn, message);
}

void RpcClientHandler::ProcessRpcRequest(const muduo::net::TcpConnectionPtr& conn,
	const RpcClientMessagePtr& request,
	muduo::Timestamp)
{
	auto sessionId = GetSessionId(conn);
	const auto sessionIt = tls_gate.sessions().find(sessionId);
	if (sessionIt == tls_gate.sessions().end())
	{
		LOG_ERROR << "Could not find session id: " << sessionId << " for RPC client message.";
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
			LOG_ERROR << "Invalid game node id " << sessionIt->second.game_node_id_ << " for session id: " << sessionId;
			SendTipToClient(conn, kServerCrashed);
			return;
		}

		auto gameNode = tls.gameNodeRegistry.get<RpcClientPtr>(gameNodeId);
		ClientSendMessageToPlayerRequest message;
		message.mutable_message_body()->set_body(request->body());
		message.set_session_id(sessionId);
		message.mutable_message_body()->set_id(request->id());
		message.mutable_message_body()->set_message_id(request->message_id());
		gameNode->CallMethod(GameServiceClientSendMessageToPlayerMessageId, message);

		LOG_TRACE << "Sent message to game node, session id: " << sessionId << ", message id: " << request->message_id();
	}
	else
	{
		auto loginNode = GetLoginNode(sessionId);
		if (entt::null == loginNode)
		{
			LOG_ERROR << "Login node not found for session id: " << sessionId << ", message id: " << request->message_id();
			//todo close connection
			return;
		}

		if (request->message_id() == LoginServiceLoginMessageId)
		{
			LoginC2LRequest message;
			message.mutable_session_info()->set_session_id(sessionId);
			message.mutable_client_msg_body()->ParseFromArray(
				request->body().data(), request->body().size());
			SendLoginC2LRequest(loginNode, message);

			LOG_TRACE << "Sent LoginC2LRequest, session id: " << sessionId;
		}
		else if (request->message_id() == LoginServiceCreatePlayerMessageId)
		{
			CreatePlayerC2LRequest message;
			message.mutable_session_info()->set_session_id(sessionId);
			message.mutable_client_msg_body()->ParseFromArray(
				request->body().data(), request->body().size());
			SendCreatePlayerC2LRequest(loginNode, message);

			LOG_TRACE << "Sent CreatePlayerC2LRequest, session id: " << sessionId;
		}
		else if (request->message_id() == LoginServiceEnterGameMessageId)
		{
			EnterGameC2LRequest message;
			message.mutable_session_info()->set_session_id(sessionId);
			message.mutable_client_msg_body()->ParseFromArray(
				request->body().data(), request->body().size());
			SendEnterGameC2LRequest(loginNode, message);

			LOG_TRACE << "Sent EnterGameC2LRequest, session id: " << sessionId;
		}
		else
		{
			LOG_ERROR << "Unhandled message id: " << request->message_id() << " for session id: " << sessionId;
		}
	}
}

Guid RpcClientHandler::GetSessionId(const muduo::net::TcpConnectionPtr& conn)
{
    try {
        return boost::any_cast<Guid>(conn->getContext());
    }
    catch (const boost::bad_any_cast& e) {
        // 处理类型转换失败的情况
        // 日志记录异常或采取其他措施
		LOG_ERROR << e.what();
        return kInvalidGuid;  // 返回默认值或特殊值
    }
}

void RpcClientHandler::SendTipToClient(const muduo::net::TcpConnectionPtr& conn, uint32_t tipId)
{
	TipInfoMessage tipMessage;
	tipMessage.set_id(tipId);
	MessageBody message;
	message.set_body(tipMessage.SerializeAsString());
	message.set_message_id(PlayerClientCommonServiceSendTipToClientMessageId);
	g_gate_node->Codec().send(conn, message);

	LOG_ERROR << "Sent tip message to session id: " << GetSessionId(conn) << ", tip id: " << tipId;
}

void RpcClientHandler::HandleConnectionEstablished(const muduo::net::TcpConnectionPtr& conn)
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

	LOG_TRACE << "New connection, assigned session id: " << sessionId;
}

void RpcClientHandler::HandleDisconnection(const muduo::net::TcpConnectionPtr& conn)
{
	const auto sessionId = entt::to_integral(GetSessionId(conn));

	{
		// 重要: 此消息一定要发，不能只通过controller 的gw disconnect去发
		// 重要: 比如:登录还没到controller,gw的disconnect 先到，登录后到，那么controller server 永远删除不了这个sessionid了
		// todo 登录时候断开应该登录服务器也告诉centre
		const auto& loginNode = GetLoginNode(sessionId);
		if (entt::null != loginNode)
		{
			LoginNodeDisconnectRequest request;
			request.set_session_id(sessionId);
			SendDisconnectC2LRequest(loginNode, request);
		}
	}

	// Notify centre server about disconnection
	{
		GateSessionDisconnectRequest request;
		request.mutable_session_info()->set_session_id(sessionId);
		g_gate_node->GetZoneCentreNode()->CallMethod(CentreServiceGateSessionDisconnectMessageId, request);
	}

	// Remove session from registry
	tls_gate.sessions().erase(sessionId);

	LOG_TRACE << "Disconnected session id: " << sessionId;
}
