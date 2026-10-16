#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <muduo/net/TcpConnection.h>

#include <entt/src/entt/entity/registry.hpp>

#include "network/codec/dispatcher.h"   // MessagePtr

class NodeHandshakeRequest;
class NodeHandshakeResponse;
class RpcClient;
class RpcController;   // network/rpc_controller.h —— 本项目的 per-call ctx,不是 ::google::protobuf::RpcController
class RpcStatus;

// 节点间 RPC0 链路的握手:出站侧(gate → scene)在 TCP 连上后发 NodeHandshake,入站侧收到后为对端挂 RpcSession。
// 握手走每次调用回调(docs/design/muduo-rpc-channel.md §8.3):成功 / 对端拒绝 / 超时都在调用点的 lambda 里处理,
// 不再依赖按 message_id 的全局回包表。只在 EventLoop 线程使用。
class NodeHandshakeManager {
public:
	// 握手成功钩子:节点在启动时装入(gate 装 gate_scene_entry::OnSceneLinkHandshaken,给这条出站连接盖「可交付进场」的章)。
	// conn 是发出握手的那条出站连接;在途调用在 DOWN 时已全部失败,所以成功回调里它必然就是回包所走的连接。
	// engine/core 不反向依赖节点代码,所以经钩子注入。只在 loop 线程调用。
	using HandshakeSucceededHook = std::function<void(uint32_t nodeType,
	                                                  const muduo::net::TcpConnectionPtr& conn,
	                                                  const NodeHandshakeResponse& response)>;
	void SetHandshakeSucceededHook(HandshakeSucceededHook hook) { handshakeSucceededHook_ = std::move(hook); }

	// 出站连接 UP 后调用:0.5s 后发起握手。
	void TryRegisterNodeSession(uint32_t nodeType, const muduo::net::TcpConnectionPtr& conn) const;

	// ctx:本次 RPC 调用的上下文(所在连接等),由派发层按次构造、显式传入;勿存储。
	void OnNodeHandshake(const RpcController& ctx, const NodeHandshakeRequest& request, NodeHandshakeResponse& response) const;

private:
	void ScheduleHandshake(entt::registry& registry, entt::entity entity, uint32_t nodeType,
	                       const std::weak_ptr<RpcClient>& weakClient) const;
	static void SendHandshake(uint32_t nodeType, const std::weak_ptr<RpcClient>& weakClient);
	void OnHandshakeCallDone(uint32_t nodeType, const std::weak_ptr<RpcClient>& weakClient,
	                         const std::weak_ptr<muduo::net::TcpConnection>& weakConn,
	                         const RpcStatus& status, const MessagePtr& response) const;
	void OnHandshakeSucceeded(entt::registry& registry, entt::entity entity, uint32_t nodeType,
	                          const NodeHandshakeResponse& response, const muduo::net::TcpConnectionPtr& conn) const;

	HandshakeSucceededHook handshakeSucceededHook_;
};
