#include "registration_manager.h"
#include <chrono>
#include <type_traits>
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
// 两类节点的握手方法请求 / 响应同型:SendHandshake 与 OnHandshakeCallDone 只写一份,不按节点类型分叉。
static_assert(std::is_same_v<SceneNodeHandshakeRpc::Request, GateNodeHandshakeRpc::Request> &&
                  std::is_same_v<SceneNodeHandshakeRpc::Response, GateNodeHandshakeRpc::Response>,
              "Scene / Gate NodeHandshake must share request and response types");

// 节点类型 → 该类节点的 RPC0 握手方法 tag(muduo-rpc-channel.md §10):有握手入口就以 fn(Tag{}) 调用并返回 true,
// 其余类型(纯 gRPC 服务、未知值)返回 false、不调用 fn。它也是「哪些节点类型有 RPC0 握手入口」的唯一来源:
//   - 出站:gate 拨 scene,SendHandshake 走 Scene 项(当前没有节点向 gate 拨 TCP,Gate 项在出站侧不可达);
//   - 入站:scene 收 gate 的握手时,OnNodeHandshake 经 IsHandshakeNodeType 靠 Gate 项放行 —— Gate 项不能删。
template <typename Fn>
constexpr bool VisitHandshakeRpc(uint32_t nodeType, Fn&& fn)
{
	switch (static_cast<eNodeType>(nodeType))
	{
	case eNodeType::SceneNodeService:
		fn(SceneNodeHandshakeRpc{});
		return true;
	case eNodeType::GateNodeService:
		fn(GateNodeHandshakeRpc{});
		return true;
	default:
		return false;
	}
}

// 出站握手方法号的编译期回归防护。SendHandshake → typed CallMethod → OnHandshakeCallDone 这条路径单测走不到
// (握手 0.5s 后才由定时器发出,完成回调要经 gNode,单测进程里没有 Node),只有联调看得到。
// 若上面两项的 tag 写反,gate 会向 scene 发 Gate.NodeHandshake:scene 没有 Gate 服务,回 NO_SERVICE(kRemoteRejected),
// gate 落进重试分支每 0.5s 重发、永不成功,而单测全绿。这里拿各服务自己生成的消息号常量核对,在编译期拦下。
// 放行 / 拒绝集合由 rpc_client_lifecycle_test 的 HandshakeTypeWhitelistAdmitsSceneAndGate / InboundHandshakeRejects... 守。
constexpr uint32_t HandshakeMessageIdOf(uint32_t nodeType)
{
	uint32_t messageId = 0;
	VisitHandshakeRpc(nodeType, [&messageId]<typename Method>(Method) { messageId = Method::kMessageId; });
	return messageId;
}
static_assert(HandshakeMessageIdOf(static_cast<uint32_t>(eNodeType::SceneNodeService)) == SceneNodeHandshakeMessageId,
              "a Scene peer must be sent Scene.NodeHandshake");
static_assert(HandshakeMessageIdOf(static_cast<uint32_t>(eNodeType::GateNodeService)) == GateNodeHandshakeMessageId,
              "a Gate peer must be sent Gate.NodeHandshake");

// CPP-2:TCP 连上 0.5s 后才发握手,失败 / 超时同样按 0.5s 节奏重试(约 2 次/s)。
constexpr double kHandshakeRetryDelaySeconds = 0.5;
// 握手是对端同步处理的轻量调用;超时只为兜住「应答丢失」——以前只有收到失败应答才重试,应答丢了只能等重连。
constexpr std::chrono::milliseconds kNodeHandshakeCallTimeout{3000};

bool IsHandshakeNodeType(uint32_t nodeType)
{
	return VisitHandshakeRpc(nodeType, [](auto) {});
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
		return;   // 重连 UP 经 RpcClient 的连接观察者重新 TryRegisterNodeSession
	}
	NodeHandshakeRequest req;
	*req.mutable_self_node() = GetNodeInfo();
	std::weak_ptr<muduo::net::TcpConnection> weakConn = client->GetConnection();
	// 按对端类型取握手方法 tag 做 typed 调用:请求 / 响应类型在编译期钉住,发起时再按注册表核对一次。
	// 回调只捕获值与 weak;不捕获 this(它可能在关机阶段经 queueInLoop 执行)。
	const bool hasHandshakeRpc = VisitHandshakeRpc(nodeType, [&]<typename Method>(Method) {
		client->CallMethod<Method>(
			req,
			[nodeType, weakClient, weakConn](const RpcStatus& status,
			                                 const std::shared_ptr<NodeHandshakeResponse>& response) {
				gNode->GetNodeRegistrationManager().OnHandshakeCallDone(nodeType, weakClient, weakConn, status, response);
			},
			RpcCallOptions{kNodeHandshakeCallTimeout});
	});
	if (!hasHandshakeRpc) {
		// 只有经 IsHandshakeNodeType 放行的类型才会排到这里,正常不可达;不静默吞掉。
		LOG_ERROR << "Node handshake not sent: node_type=" << nodeType << " has no RPC0 handshake method";
	}
}

void NodeHandshakeManager::OnHandshakeCallDone(uint32_t nodeType, const std::weak_ptr<RpcClient>& weakClient,
                                               const std::weak_ptr<muduo::net::TcpConnection>& weakConn,
                                               const RpcStatus& status,
                                               const std::shared_ptr<NodeHandshakeResponse>& response) const {
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

	// status.ok() ⇔ response 非空(rpc_channel.h ClientDoneCallback 契约 (a)),且类型就是 NodeHandshakeResponse:
	// typed 调用发起时已核对注册表的响应类型,应答按它解析;解析失败以 kInvalidResponse 落在上面 default 分支重试。
	const NodeHandshakeResponse& reply = *response;
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
