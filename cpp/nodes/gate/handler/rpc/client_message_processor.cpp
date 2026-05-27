#include "client_message_processor.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iomanip>
#include <memory>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <string_view>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "gate_codec.h"
#include "message_limiter/illegal_packet_counter.h"
#include "error_reporter/error_reporter.h"
#include "node/system/node/node.h"
#include "grpc_client/login/login_grpc_client.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "rpc/service_metadata/rpc_event_registry.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/player_lifecycle_service_metadata.h"
#include "rpc/service_metadata/login_service_metadata.h"
#include "rpc/service_metadata/client_player_common_service_metadata.h"
#include "proto/common/base/node.pb.h"
#include "node/system/node/node_util.h"
#include "proto/common/event/node_event.pb.h"
#include "thread_context/node_context_manager.h"
#include <session/manager/session_manager.h>
#include <network/node_utils.h>
#include <node_config_manager.h>

namespace
{
	std::string BytesToHex(const unsigned char *data, unsigned int size)
	{
		std::ostringstream stream;
		stream << std::hex << std::setfill('0');
		for (unsigned int index = 0; index < size; ++index)
		{
			stream << std::setw(2) << static_cast<unsigned int>(data[index]);
		}
		return stream.str();
	}

	std::string HmacSha256Hex(std::string_view secret, std::string_view payload)
	{
		unsigned char digest[EVP_MAX_MD_SIZE];
		unsigned int digestLength = 0;
		const auto *result = HMAC(EVP_sha256(),
								  secret.data(),
								  static_cast<int>(secret.size()),
								  reinterpret_cast<const unsigned char *>(payload.data()),
								  payload.size(),
								  digest,
								  &digestLength);
		if (result == nullptr)
		{
			return {};
		}
		return BytesToHex(digest, digestLength);
	}
}

static std::optional<entt::entity> PickRandomNode(uint32_t nodeType)
{
	std::vector<entt::entity> candidates;
	auto &registry = tlsNodeContextManager.GetRegistry(nodeType);
	auto view = registry.view<NodeInfo>();
	for (auto entity : view)
	{
		const auto &node = view.get<NodeInfo>(entity);
		if (node.zone_id() == GetNodeInfo().zone_id())
		{
			candidates.push_back(entity);
		}
	}

	if (candidates.empty())
	{
		return std::nullopt;
	}

	// Pick a random candidate
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, candidates.size() - 1);
	return candidates[dis(gen)];
}

static inline uint64_t GetEffectiveNodeId(
	const SessionInfo &session,
	uint32_t nodeType)
{
	// Despite the name, this is the integral form of an entt::entity, not a
	// business node_id. The handshake layer (registration_manager /
	// gate_event_handler) always stores `entt::to_integral(entity)` here.
	const auto storedEntityInt = session.GetEntityId(nodeType);
	if (storedEntityInt == SessionInfo::kInvalidEntityId)
	{
		return storedEntityInt;
	}

	auto &registry = tlsNodeContextManager.GetRegistry(nodeType);
	const entt::entity storedEntity{storedEntityInt};
	if (registry.valid(storedEntity))
	{
		return storedEntityInt;
	}

	// Post uuid-refactor there is no node_id -> entity fallback: when the
	// stored entity has been destroyed it means the remote node is gone, and
	// any re-registration will be surfaced through event handlers that refresh
	// the session binding via entity integers. Returning kInvalidEntityId lets
	// the caller short-circuit rather than forward to a phantom node.
	return SessionInfo::kInvalidEntityId;
}

RpcClientSessionHandler::RpcClientSessionHandler(ProtobufCodec &codec,
												 ProtobufDispatcher &dispatcherParam)
	: protobufCodec(codec),
	  messageDispatcher(dispatcherParam)
{
	messageDispatcher.registerMessageCallback<ClientRequest>(
		std::bind(&RpcClientSessionHandler::DispatchClientRpcMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	messageDispatcher.registerMessageCallback<ClientTokenVerifyRequest>(
		std::bind(&RpcClientSessionHandler::DispatchTokenVerify, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	tlsEcs.dispatcher.sink<OnNodeRemoveEvent>().connect<&RpcClientSessionHandler::OnNodeRemoveEventHandler>(*this);
}

// Scene node binding must be set explicitly by scene manager (e.g. EnterScene).
// Do NOT pick randomly -- a random scene node has no player entity.
std::optional<entt::entity> ResolveSessionTargetNode(SessionId sessionId, uint32_t nodeType)
{
	const auto sessionIt = tlsSessionManager.sessions().find(sessionId);
	if (sessionIt == tlsSessionManager.sessions().end())
	{
		LOG_ERROR << "Session not found for session id: " << sessionId;
		return std::nullopt;
	}

	auto &session = sessionIt->second;

	if (!session.HasEntityId(nodeType))
	{
		LOG_ERROR << "No node bound for nodeType: " << nodeType << ", session id: " << sessionId;
		return std::nullopt;
	}

	const auto &registry = tlsNodeContextManager.GetRegistry(nodeType);
	const auto effectiveEntityInt = GetEffectiveNodeId(session, nodeType);
	entt::entity nodeEntity = entt::entity{effectiveEntityInt};
	if (!registry.valid(nodeEntity))
	{
		LOG_ERROR << "Bound node is invalid. nodeType: " << nodeType
				  << ", session id: " << sessionId
				  << ", stored_entity_int=" << session.GetEntityId(nodeType);
		session.SetEntityId(nodeType, SessionInfo::kInvalidEntityId);
		return std::nullopt;
	}

	if (effectiveEntityInt != session.GetEntityId(nodeType))
	{
		session.SetEntityId(nodeType, effectiveEntityInt);
	}

	return nodeEntity;
}

void RpcClientSessionHandler::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
	// Token verification is handled by DispatchTokenVerify (ClientTokenVerifyRequest).
	// Until verified, DispatchClientRpcMessage rejects all ClientRequest messages.
	if (conn->connected())
	{
		HandleConnectionEstablished(conn);
	}
	else
	{
		HandleConnectionDisconnection(conn);
	}
}

void RpcClientSessionHandler::SendMessageToClient(const muduo::net::TcpConnectionPtr &conn, const ::google::protobuf::Message &message) const
{
	protobufCodec.send(conn, message);
}

SessionId RpcClientSessionHandler::GetSessionId(const muduo::net::TcpConnectionPtr &conn)
{
	try
	{
		return boost::any_cast<SessionId>(conn->getContext());
	}
	catch (const boost::bad_any_cast &e)
	{
		LOG_ERROR << "Failed to cast session ID from connection context: " << e.what();
		return kInvalidSessionId;
	}
}

void RpcClientSessionHandler::SendTipToClient(const muduo::net::TcpConnectionPtr &conn, uint32_t tipId)
{
	TipInfoMessage tipMessage;
	tipMessage.set_id(tipId);
	MessageContent message;
	message.set_serialized_message(tipMessage.SerializeAsString());
	message.set_message_id(SceneClientPlayerCommonSendTipToClientMessageId);
	GetGateCodec().send(conn, message);

	LOG_TRACE << "Sent tip message to session id: " << GetSessionId(conn) << ", tip id: " << tipId;
}

bool RpcClientSessionHandler::CheckMessageSize(const RpcClientMessagePtr &request, const muduo::net::TcpConnectionPtr &conn) const
{
	constexpr size_t kMaxClientMessageSize = 1024;
	if (request->ByteSizeLong() > kMaxClientMessageSize)
	{
		LOG_ERROR << "Message size exceeds 1KB. Message ID: " << request->message_id();
		MessageContent errResponse;
		errResponse.set_id(request->id());
		errResponse.set_message_id(request->message_id());
		errResponse.mutable_error_message()->set_id(kMessageSizeExceeded);
		conn->send(errResponse.SerializeAsString());
		return false;
	}
	return true;
}

bool RpcClientSessionHandler::CheckMessageLimit(SessionInfo &session, const RpcClientMessagePtr &request, const muduo::net::TcpConnectionPtr &conn) const
{
	if (const auto err = session.messageLimiter.CanSend(request->message_id()); err != kSuccess)
	{
		LOG_ERROR << "Failed to send message. Message ID: " << request->message_id() << ", Error: " << err;
		MessageContent errResponse;
		errResponse.set_id(request->id());
		errResponse.set_message_id(request->message_id());
		errResponse.mutable_error_message()->set_id(err);
		conn->send(errResponse.SerializeAsString());

		// todo.md #236: count this rejection toward the per-session illegal-
		// packet kill switch. A misbehaving / hostile client that keeps
		// hammering past the rate limit gets dropped at the threshold
		// (GATE_ILLEGAL_PACKET_THRESHOLD env, default 50) so we stop
		// burning CPU sending error replies it ignores.
		const bool shouldKill = IllegalPacketCounter::RegisterAndShouldKill(session.illegalPacketCount);

		// todo.md #250 slice A — record into process-wide buffer. Carries
		// the running illegal-packet count so an attack pattern shows up
		// as a step function in the buffer dump, not just per-event noise.
		{
			std::ostringstream m;
			m << "message_id=" << request->message_id()
			  << " illegal_count=" << session.illegalPacketCount
			  << " player_id=" << session.playerId;
			error_reporter::Record(err, "illegal_packet", m.str());
		}

		if (shouldKill)
		{
			LOG_WARN << "Session illegal-packet threshold exceeded — forceClose."
					 << " count=" << session.illegalPacketCount
					 << " message_id=" << request->message_id();
			conn->forceClose();
		}
		return false;
	}
	return true;
}

template <typename Message, typename Request>
void ParseMessageFromRequestBody(Message &message, const Request &request, const SessionId sessionId)
{
	const std::string &requestBody = request->body();
	if (requestBody.empty())
	{
		return;
	}

	if (!message.ParseFromString(requestBody))
	{
		LOG_ERROR << "Failed to parse client message body for session id: " << sessionId;
		return;
	}
}

void RpcClientSessionHandler::HandleConnectionDisconnection(const muduo::net::TcpConnectionPtr &conn)
{
	const auto sessionId = GetSessionId(conn);
	const std::string peer = conn ? conn->peerAddress().toIpPort() : std::string{"<null>"};

	// Retrieve session info before erasing so we can notify both Login and Scene.
	auto &sessions = tlsSessionManager.sessions();
	auto sessionIt = sessions.find(sessionId);

	const bool sessionFound = (sessionIt != sessions.end());
	bool sceneNotified = false;
	uint32_t sceneNodeId = 0;

	if (sessionFound)
	{
		// Notify Scene to exit the player immediately (saves state and stops broadcasting).
		if (sessionIt->second.HasEntityId(SceneNodeService))
		{
			const auto sceneEntityId = sessionIt->second.GetEntityId(SceneNodeService);
			sceneNodeId = sceneEntityId;
			auto &sceneRegistry = tlsNodeContextManager.GetRegistry(SceneNodeService);
			entt::entity sceneEntity{sceneEntityId};
			if (sceneRegistry.valid(sceneEntity))
			{
				const auto *rpcClient = sceneRegistry.try_get<RpcClientPtr>(sceneEntity);
				if (rpcClient && *rpcClient)
				{
					ProcessClientPlayerMessageRequest exitMsg;
					exitMsg.set_session_id(sessionId);
					exitMsg.mutable_message_content()->set_message_id(ScenePlayerExitGameMessageId);
					(*rpcClient)->CallRemoteMethod(SceneProcessClientPlayerMessageMessageId, exitMsg);
					sceneNotified = true;
				}
			}
		}
	}

	// Disconnect notification goes to Login; its session manager owns the disconnect lease.
	// Login nodes are stateless -- pick any available node, no session affinity needed.
	//
	// IMPORTANT: carry SessionInfo.playerId through SessionDetails. Login's
	// markPlayerSessionDisconnecting() early-returns when playerId == 0,
	// which meant a TCP close without explicit Logout would leave the
	// player_locator session in ONLINE forever — later tripping EnterGame
	// into ReplaceLogin against a dead gate. See
	// docs/design/stress-test-2026-05-http-login.md §四 #B-1.
	const auto loginNode = PickRandomNode(eNodeType::LoginNodeService);
	if (loginNode)
	{
		loginpb::LoginNodeDisconnectRequest request;
		request.set_session_id(sessionId);
		SessionDetails sessionDetails;
		sessionDetails.set_session_id(sessionId);
		sessionDetails.set_gate_node_id(gNode->GetNodeId());
		sessionDetails.set_gate_instance_id(gNode->GetNodeInfo().node_uuid());
		if (sessionFound)
		{
			sessionDetails.set_player_id(sessionIt->second.playerId);
		}
		loginpb::SendClientPlayerLoginDisconnect(tlsNodeContextManager.GetRegistry(eNodeType::LoginNodeService), *loginNode, request, {kSessionBinMetaKey}, SerializeSessionDetails(sessionDetails));
	}

	sessions.erase(sessionId);

	LOG_INFO << "Client disconnected, session_id=" << sessionId
			 << ", peer=" << peer
			 << ", session_found=" << sessionFound
			 << ", scene_node_id=" << sceneNodeId
			 << ", scene_notified=" << sceneNotified
			 << ", remaining_sessions=" << sessions.size();
}

// High-water-mark: output buffer exceeded threshold — client not consuming (disconnect/cheat/slow), force close.
static constexpr size_t kClientHighWaterMark = 2 * 1024 * 1024; // 2MB

static void OnClientHighWaterMark(const muduo::net::TcpConnectionPtr &conn, size_t oldLen)
{
	const auto sessionId = RpcClientSessionHandler::GetSessionId(conn);
	LOG_WARN << "Client high water mark triggered, session_id=" << sessionId
			 << ", buffered=" << oldLen
			 << " bytes, threshold=" << kClientHighWaterMark
			 << " bytes, forcing close";
	conn->forceClose();
}

void RpcClientSessionHandler::HandleConnectionEstablished(const muduo::net::TcpConnectionPtr &conn)
{
	auto sessionId = tlsSessionManager.session_id_gen().Generate();
	while (tlsSessionManager.sessions().find(sessionId) != tlsSessionManager.sessions().end())
	{
		sessionId = tlsSessionManager.session_id_gen().Generate();
	}

	// Session ID prevents packet tampering / message misdirection

	conn->setContext(sessionId);
	conn->setHighWaterMarkCallback(OnClientHighWaterMark, kClientHighWaterMark);

	SessionInfo session;
	session.conn = conn;

	// Dev mode: if no gate_token_secret configured, auto-verify all connections
	const auto &secret = gNodeConfigManager.GetBaseDeployConfig().gate_token_secret();
	if (secret.empty())
	{
		session.verified = true;
	}

	tlsSessionManager.sessions().emplace(sessionId, std::move(session));

	const std::string peer = conn ? conn->peerAddress().toIpPort() : std::string{"<null>"};
	LOG_INFO << "Client connected, session_id=" << sessionId
			 << ", peer=" << peer
			 << ", total_sessions=" << tlsSessionManager.sessions().size();
}

// Handle messages related to the game node
void HandleTcpNodeMessage(const SessionInfo &session, const RpcClientMessagePtr &request, SessionId sessionId, const muduo::net::TcpConnectionPtr &conn)
{
	assert(request->message_id() < gRpcMethodRegistry.size());
	auto &handlerMeta = gRpcMethodRegistry[request->message_id()];

	// Player sent message without being logged in — invalid node binding
	entt::entity targetNodeEntity = entt::entity{GetEffectiveNodeId(session, handlerMeta.targetNodeType)};
	auto &registry = tlsNodeContextManager.GetRegistry(handlerMeta.targetNodeType);
	if (!registry.valid(targetNodeEntity))
	{
		LOG_WARN << "[TCP Node] Scene not ready, dropping message_id: " << request->message_id()
				 << ", session_id: " << sessionId
				 << ", node_type: " << handlerMeta.targetNodeType;

		RpcClientSessionHandler::SendTipToClient(conn, kServiceUnavailable);
		return;
	}

	auto &tcpNode = registry.get<RpcClientPtr>(targetNodeEntity);
	ProcessClientPlayerMessageRequest message;
	message.mutable_message_content()->set_serialized_message(request->body());
	message.set_session_id(sessionId);
	message.mutable_message_content()->set_id(request->id());
	message.mutable_message_content()->set_message_id(request->message_id());
	tcpNode->CallRemoteMethod(SceneProcessClientPlayerMessageMessageId, message);

	LOG_TRACE << "Sent message to game node, session id: " << sessionId << ", message id: " << request->message_id();
}

void HandleGrpcNodeMessage(SessionId sessionId, const RpcClientMessagePtr &request, const muduo::net::TcpConnectionPtr &conn)
{
	assert(request->message_id() < gRpcMethodRegistry.size());
	auto &rpcHandlerMeta = gRpcMethodRegistry[request->message_id()];
	ParseMessageFromRequestBody(*rpcHandlerMeta.requestProto, request, sessionId);

	SessionDetails sessionDetails;
	sessionDetails.set_session_id(sessionId);
	const auto sessionIt = tlsSessionManager.sessions().find(sessionId);
	if (sessionIt == tlsSessionManager.sessions().end())
	{
		LOG_ERROR << "Session not found for session id: " << sessionId;
		return;
	}
	sessionDetails.set_player_id(sessionIt->second.playerId);
	sessionDetails.set_gate_node_id(gNode->GetNodeId());
	sessionDetails.set_gate_instance_id(gNode->GetNodeInfo().node_uuid());

	// Stress diagnostic 2026-05-24: scene_manager observed gate_id="0" in
	// EnterScene requests during 3-zone × 15000 round 2, even though cpp
	// gate had long since completed NodeId CAS. Logging the actual values
	// at the SessionDetails build site to confirm whether (a) the cast
	// drops the high bits, (b) gNode->GetNodeId() is genuinely 0 here, or
	// (c) login overwrites the field somewhere downstream.
	LOG_INFO << "[diag] SessionDetails to login: session_id=" << sessionId
			 << " player_id=" << sessionIt->second.playerId
			 << " gate_node_id=" << gNode->GetNodeId()
			 << " message_id=" << request->message_id();

	if (rpcHandlerMeta.sender)
	{
		// Scene nodes hold player entities in memory -- require session affinity binding.
		// All Go microservices (login, guild, friend, chat, etc.) are stateless -- pick any available node.
		std::optional<entt::entity> node;
		if (rpcHandlerMeta.targetNodeType == eNodeType::SceneNodeService)
		{
			node = ResolveSessionTargetNode(sessionId, rpcHandlerMeta.targetNodeType);
		}
		else
		{
			node = PickRandomNode(rpcHandlerMeta.targetNodeType);
		}
		if (!node)
		{
			LOG_ERROR << "Node not found for session id: " << sessionId << ", message id: " << request->message_id();
			RpcClientSessionHandler::SendTipToClient(conn, kServiceUnavailable);
			return;
		}

		rpcHandlerMeta.sender(tlsNodeContextManager.GetRegistry(rpcHandlerMeta.targetNodeType),
							  *node,
							  *rpcHandlerMeta.requestProto,
							  {kSessionBinMetaKey},
							  SerializeSessionDetails(sessionDetails));
	}
}

bool RpcClientSessionHandler::ValidateClientMessage(SessionInfo &session, const RpcClientMessagePtr &request, const muduo::net::TcpConnectionPtr &conn) const
{
	if (!CheckMessageSize(request, conn))
		return false;
	if (!CheckMessageLimit(session, request, conn))
		return false;
	return true;
}

// Main request handler, forwards the request to the appropriate service
void RpcClientSessionHandler::DispatchClientRpcMessage(const muduo::net::TcpConnectionPtr &conn,
													   const RpcClientMessagePtr &request,
													   muduo::Timestamp)
{
	auto sessionId = GetSessionId(conn);
	const auto sessionIt = tlsSessionManager.sessions().find(sessionId);
	if (sessionIt == tlsSessionManager.sessions().end())
	{
		LOG_ERROR << "[Invalid Session] No session found for conn session_id: " << sessionId
				  << ", message_id: " << request->message_id();
		return;
	}

	if (request->message_id() >= gRpcMethodRegistry.size() || !IsClientMessageId(request->message_id()))
	{
		LOG_ERROR << "Invalid or unauthorized message ID: " << request->message_id();
		return;
	}

	auto &session = sessionIt->second;

	// Reject all game messages until the client passes token verification.
	// If gate_token_secret is empty (dev mode), all sessions are auto-verified on connect.
	if (!session.verified)
	{
		LOG_WARN << "[Token] Unverified session rejected message_id: " << request->message_id()
				 << ", session_id: " << sessionId;
		conn->shutdown();
		return;
	}

	if (!ValidateClientMessage(session, request, conn))
		return;

	auto &messageInfo = gRpcMethodRegistry[request->message_id()];
	if (messageInfo.protocol == PROTOCOL_TCP)
	{
		HandleTcpNodeMessage(session, request, sessionId, conn);
	}
	else if (messageInfo.protocol == PROTOCOL_GRPC)
	{
		HandleGrpcNodeMessage(sessionId, request, conn);
	}
}

void RpcClientSessionHandler::DispatchTokenVerify(const muduo::net::TcpConnectionPtr &conn,
												  const ClientTokenVerifyRequestPtr &message,
												  muduo::Timestamp)
{
	auto sessionId = GetSessionId(conn);
	const auto sessionIt = tlsSessionManager.sessions().find(sessionId);
	if (sessionIt == tlsSessionManager.sessions().end())
	{
		LOG_ERROR << "[Token] No session found for session_id: " << sessionId;
		return;
	}

	auto &session = sessionIt->second;

	auto sendReply = [&](bool success, const std::string &error)
	{
		ClientTokenVerifyResponse resp;
		resp.set_success(success);
		if (!error.empty())
			resp.set_error(error);
		protobufCodec.send(conn, resp);
	};

	if (session.verified)
	{
		sendReply(true, "");
		return;
	}

	const auto &secret = gNodeConfigManager.GetBaseDeployConfig().gate_token_secret();
	if (secret.empty())
	{
		// Dev mode: no secret configured, accept all
		session.verified = true;
		sendReply(true, "");
		return;
	}

	// Recompute HMAC-SHA256(secret, payload) and compare with client-provided signature (hex)
	const auto &payloadBytes = message->payload();
	const auto &clientSig = message->signature();

	auto expectedHex = HmacSha256Hex(secret, payloadBytes);
	std::string clientSigStr(clientSig.begin(), clientSig.end());

	if (expectedHex != clientSigStr)
	{
		LOG_WARN << "[Token] HMAC mismatch for session_id: " << sessionId;
		sendReply(false, "invalid token signature");
		conn->shutdown();
		return;
	}

	// Deserialize payload and validate fields
	GateTokenPayload payload;
	if (!payload.ParseFromString(payloadBytes))
	{
		LOG_WARN << "[Token] Failed to parse token payload for session_id: " << sessionId;
		sendReply(false, "malformed token payload");
		conn->shutdown();
		return;
	}

	// Check gate_node_id matches this gate
	if (payload.gate_node_id() != gNode->GetNodeId())
	{
		LOG_WARN << "[Token] gate_node_id mismatch: token=" << payload.gate_node_id()
				 << " self=" << gNode->GetNodeId() << " session_id: " << sessionId;
		sendReply(false, "token not for this gate");
		conn->shutdown();
		return;
	}

	// Check expiry
	auto now = static_cast<int64_t>(std::time(nullptr));
	if (payload.expire_timestamp() <= now)
	{
		LOG_WARN << "[Token] Expired token for session_id: " << sessionId
				 << " expire=" << payload.expire_timestamp() << " now=" << now;
		sendReply(false, "token expired");
		conn->shutdown();
		return;
	}

	// todo.md #76 slice A — install the per-session HMAC key carried in
	// the verified token payload. Empty bytes mean "client/server pair
	// hasn't rolled out the signed-message path yet" — we accept the
	// verification anyway and fall back to the adler32-only path in
	// codec.cpp. Once the wire-format slot for `hmac_tag` lands
	// (slice B), absent key here flags the connection as "unsigned-
	// allowed" so the gate's per-message verifier knows whether to
	// require a tag.
	//
	// Resetting the illegal-packet counter on a verified handshake is
	// intentional: a fresh ClientTokenVerify means the client has just
	// completed AssignGate, so any pre-handshake packet noise is
	// behind us. If illegal packets resume after this point, that's a
	// real attack signal and the threshold counter starts again from
	// zero.
	session.hmacSessionKey.assign(payload.hmac_session_key());
	IllegalPacketCounter::Reset(session.illegalPacketCount);

	session.verified = true;
	sendReply(true, "");
	LOG_DEBUG << "[Token] Session verified, session_id: " << sessionId
			  << " hmac_key_len=" << session.hmacSessionKey.size();
}

void RpcClientSessionHandler::OnNodeRemoveEventHandler(const OnNodeRemoveEvent &pb)
{
	auto &registry = tlsNodeContextManager.GetRegistry(pb.node_type());
	for (auto &session : tlsSessionManager.sessions())
	{
		if (session.second.GetEntityId(pb.node_type()) != pb.entity())
			continue;
		session.second.SetEntityId(pb.node_type(), SessionInfo::kInvalidEntityId);
	}
}
