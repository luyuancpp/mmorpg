#include "registration_manager.h"
#include <chrono>
#include <table/proto/tip/common_error_tip.pb.h>
#include <time/comp/timer_task_comp.h>
#include "node/system/node/node_util.h"
#include <rpc/service_metadata/scene_service_metadata.h>
#include <rpc/service_metadata/gate_service_service_metadata.h>
#include <network/rpc_client.h>
#include <network/rpc_session.h>
#include <network/rpc_status.h>
#include "node/system/node/node.h"
#include <network/node_utils.h>
#include <proto_helpers/proto_util.h>
#include "thread_context/node_context_manager.h"
#include "proto/common/base/message.pb.h"
#include <network/rpc_controller.h>

namespace
{
uint32_t kNodeTypeToMessageId[eNodeType_ARRAYSIZE] = {
	0,
	0,
	0,
	SceneNodeHandshakeMessageId,
	GateNodeHandshakeMessageId
};

// CPP-2:TCP 连上 0.5s 后才发握手,失败 / 超时同样按 0.5s 节奏重试(约 2 次/s)。
constexpr double kHandshakeRetryDelaySeconds = 0.5;
// 握手是对端同步处理的轻量调用;超时只为兜住「应答丢失」——以前只有收到失败应答才重试,应答丢了只能等重连。
constexpr std::chrono::milliseconds kNodeHandshakeCallTimeout{3000};

bool IsHandshakeNodeType(uint32_t nodeType)
{
	return nodeType < eNodeType_ARRAYSIZE && kNodeTypeToMessageId[nodeType] != 0;
}
} // namespace

void NodeHandshakeManager::TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const {
	if (!IsHandshakeNodeType(nodeType)) {
		return;
	}
	entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
	for (const auto& [entity, client, nodeInfo] : registry.view<RpcClientPtr, NodeInfo>().each()) {
		if (client->GetConnection() == nullptr || client->GetConnection().get() != conn.get()) continue;
		LOG_INFO << "Peer address match in " << NodeUtils::GetRegistryName(registry)
			<< ": " << conn->peerAddress().toIpPort();
		ScheduleHandshake(registry, entity, nodeType, client);
		return;
	}
}

void NodeHandshakeManager::ScheduleHandshake(entt::registry& registry, entt::entity entity, uint32_t nodeType,
                                             const std::weak_ptr<RpcClient>& weakClient) const {
	// 弱捕获,不能按值捕获 TcpConnectionPtr / RpcClientPtr。这个闭包会在 EventLoop 的定时器队列里活最长 0.5s;
	// 若节点恰在这个窗口内被摘除(etcd DELETE → DestroyEntity → ~RpcClient → ~TcpClient),闭包里的强引用会把
	// muduo TcpClient::~TcpClient 的 `unique = connection_.use_count() == 1` 带成 false,forceClose() 被跳过,
	// 连接在 kConnected 态一直悬到闭包销毁,最后在 TcpConnection.cc:71 的 assert 上炸
	// (docs/ops/incident-gate-tcpconnection-dtor-assert-2026-09-13.md §7)。触发时 lock() 失败就说明节点已经走了。
	// RunAfter 先取消同一实体上尚未触发的那一次,所以任何时刻每个节点只有一个待发的握手。
	registry.get_or_emplace<TimerTaskComp>(entity).RunAfter(kHandshakeRetryDelaySeconds, [nodeType, weakClient]() {
		SendHandshake(nodeType, weakClient);
	});
}

void NodeHandshakeManager::SendHandshake(uint32_t nodeType, const std::weak_ptr<RpcClient>& weakClient) {
	const auto client = weakClient.lock();
	if (!client || !client->connected()) {
		return;   // 等下一次 UP:OnConnected2TcpServerEvent 会重新 TryRegisterNodeSession
	}
	NodeHandshakeRequest req;
	*req.mutable_self_node() = GetNodeInfo();
	std::weak_ptr<muduo::net::TcpConnection> weakConn = client->GetConnection();
	// 回调只捕获值与 weak;不捕获 this(它可能在关机阶段经 queueInLoop 执行)。
	client->CallMethod(
		kNodeTypeToMessageId[nodeType], req,
		[nodeType, weakClient, weakConn](const RpcStatus& status, const MessagePtr& response) {
			gNode->GetNodeRegistrationManager().OnHandshakeCallDone(nodeType, weakClient, weakConn, status, response);
		},
		RpcCallOptions{kNodeHandshakeCallTimeout});
}

void NodeHandshakeManager::OnHandshakeCallDone(uint32_t nodeType, const std::weak_ptr<RpcClient>& weakClient,
                                               const std::weak_ptr<muduo::net::TcpConnection>& weakConn,
                                               const RpcStatus& status, const MessagePtr& response) const {
	const auto client = weakClient.lock();
	if (!client) {
		return;   // 节点已摘除
	}
	// nodeType 是本端发起时自己的,不是对端自报的;仍走 TryGetRegistry,与「对端来的 node_type 一律校验」口径一致。
	entt::registry* const registryPtr = tlsNodeContextManager.TryGetRegistry(nodeType);
	if (registryPtr == nullptr) {
		LOG_ERROR << "Node handshake reply for invalid node_type=" << nodeType;
		return;
	}
	entt::registry& registry = *registryPtr;
	// 实体可能已被同 uuid 重注册替换:只认仍持有这个 RpcClient 的实体。
	entt::entity entity = entt::null;
	for (const auto& [candidate, candidateClient] : registry.view<RpcClientPtr>().each()) {
		if (candidateClient == client) {
			entity = candidate;
			break;
		}
	}
	if (entity == entt::null) {
		return;
	}

	if (!status.ok()) {
		switch (status.code()) {
		case RpcStatusCode::kDisconnected:
		case RpcStatusCode::kChannelClosed:
		case RpcStatusCode::kNotSent:
			// 连接没了 / 关机 / 根本没发出去:不重试。重连 UP 会重新握手,这里再排一次会握手两次。
			LOG_INFO << "Node handshake not completed: status=" << status.name()
				<< ", peer=" << client->peer_addr().toIpPort();
			return;
		default:
			// 超时(应答丢失)、对端拒绝、回包坏了:按原节奏重试。
			LOG_WARN << "Node handshake failed, retrying: status=" << status.name()
				<< ", remote_error=" << status.remoteError() << ", peer=" << client->peer_addr().toIpPort();
			ScheduleHandshake(registry, entity, nodeType, client);
			return;
		}
	}

	if (response == nullptr || response->GetDescriptor() != NodeHandshakeResponse::descriptor()) {
		LOG_ERROR << "Node handshake reply has unexpected type, retrying: peer=" << client->peer_addr().toIpPort();
		ScheduleHandshake(registry, entity, nodeType, client);
		return;
	}
	const auto& reply = static_cast<const NodeHandshakeResponse&>(*response);
	LOG_TRACE << "Node registration response: " << reply.DebugString();
	if (reply.error_message().id() != kCommon_errorOK) {
		LOG_TRACE << "Registration failed: " << reply.DebugString();
		ScheduleHandshake(registry, entity, nodeType, client);
		return;
	}
	OnHandshakeSucceeded(registry, entity, nodeType, reply, weakConn.lock());
}

void NodeHandshakeManager::OnHandshakeSucceeded(entt::registry& registry, entt::entity entity, uint32_t nodeType,
                                                const NodeHandshakeResponse& response,
                                                const muduo::net::TcpConnectionPtr& conn) const {
	// 对端自报的身份只用来核对,不用来选 registry / 找实体(那是本端发起时就定下的)。不一致就不认这次握手(fail-closed):
	// 说明同一地址上已经换了一个节点(端口复用),或对端数据有误。
	const auto* nodeInfo = registry.try_get<NodeInfo>(entity);
	if (response.peer_node().node_type() != nodeType || nodeInfo == nullptr ||
		!NodeUtils::IsSameNode(nodeInfo->node_uuid(), response.peer_node().node_uuid())) {
		LOG_ERROR << "Node handshake reply identity mismatch: expected node_type=" << nodeType
			<< " uuid=" << (nodeInfo ? nodeInfo->node_uuid() : std::string("<none>"))
			<< ", got node_type=" << response.peer_node().node_type()
			<< " uuid=" << response.peer_node().node_uuid();
		return;
	}

	registry.remove<TimerTaskComp>(entity);

	if (handshakeSucceededHook_ && conn) {
		handshakeSucceededHook_(nodeType, conn, response);
	}
	LOG_INFO << "Node registration success.";
}

void NodeHandshakeManager::OnNodeHandshake(
	const RpcController& ctx,
	const NodeHandshakeRequest& request,
	NodeHandshakeResponse& response
) const {
	// 连接随 ctx 显式传入(Go 的 ctx 语义),不再从 thread_local 取。
	const TcpConnectionPtr& rpcConn = ctx.conn();
	if (!rpcConn)
	{
		response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
		LOG_ERROR << "No connection on the RPC context.";
		return;
	}

	auto& peerNode = request.self_node();
	// 生成的 IsTcpNodeType 包含可注册的全部服务，不等于拥有 RPC0 握手入口。
	// 先拒绝纯 gRPC 服务，避免把只有 NodeInfo/RpcSession 的实体混进其出站通道池。
	if (!IsHandshakeNodeType(peerNode.node_type()))
	{
		response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
		LOG_ERROR << "Invalid node type for registration: " << peerNode.node_type();
		return;
	}
	response.mutable_peer_node()->CopyFrom(gNode->GetNodeInfo());
	LOG_TRACE << "Node registration request: " << request.DebugString();

	auto tryRegister = [&, this](const TcpConnectionPtr& conn, uint32_t nodeType) -> bool {
		entt::registry& registry = tlsNodeContextManager.GetRegistry(nodeType);
		const auto& nodesOfType = gNode->GetServiceDiscoveryManager().ServiceNodes().NodesOfType(nodeType);
		const NodeInfo* nodeToRegister = &peerNode;
		for (const auto& serverNode : nodesOfType) {
			if (!NodeUtils::IsSameNode(serverNode.node_uuid(), peerNode.node_uuid())) {
				continue;
			}
			nodeToRegister = &serverNode;
			break;
		}

		if (NodeUtils::IsZoneScopedNodeType(nodeType) &&
			nodeToRegister->zone_id() != gNode->GetNodeInfo().zone_id()) {
			LOG_WARN << "Reject node registration from foreign zone. type=" << nodeType
				<< ", node_zone=" << nodeToRegister->zone_id()
				<< ", self_zone=" << gNode->GetNodeInfo().zone_id()
				<< ", uuid=" << nodeToRegister->node_uuid();
			return false;
		}

		// uuid is the only globally-unique handle for a remote node, so we
		// look up (or freshly allocate) the entity by uuid. Previously the
		// entity slot was keyed by node_id, which silently aliased nodes
		// from different zones onto the same slot.
		entt::entity nodeEntity = entt::null;
		if (const auto found = NodeUtils::FindNodeEntityByUuid(nodeType, nodeToRegister->node_uuid()); found) {
			nodeEntity = *found;
			// Reset session-bound components on re-registration so any
			// stale RpcSession from a previous connection is cleared.
			registry.remove<RpcSession>(nodeEntity);
		} else {
			nodeEntity = registry.create();
			LOG_TRACE << "Allocated new entity " << entt::to_integral(nodeEntity)
				<< " for handshaking peer uuid=" << nodeToRegister->node_uuid();
		}

		registry.emplace_or_replace<NodeInfo>(nodeEntity, *nodeToRegister);
		registry.emplace<RpcSession>(nodeEntity, RpcSession{ conn , nodeToRegister->node_uuid()});
		LOG_INFO << "Node registered, id: " << nodeToRegister->node_id()
			<< " uuid: " << nodeToRegister->node_uuid()
			<< " entity: " << entt::to_integral(nodeEntity)
			<< " in " << NodeUtils::GetRegistryName(registry);
		return true;
		};

	if (tryRegister(rpcConn, peerNode.node_type())) {
		response.mutable_error_message()->set_id(kCommon_errorOK);
		LOG_INFO << "Node registration succeeded: " << peerNode.DebugString();
		return;
	}
	response.mutable_error_message()->set_id(kFailedToRegisterTheNode);
}
