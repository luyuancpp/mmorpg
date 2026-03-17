#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoopThreadPool.h"

#include "node/system/node/simple_node.h"
#include "node/system/node/node.h"
#include "handler/rpc/gate_service_handler.h"
#include "handler/rpc/client_message_processor.h"
#include "handler/event/event_handler.h"
#include "handler/event/gate_kafka_command_router.h"
#include "grpc_client/grpc_init_client.h"
#include "node/system/grpc_channel_cache.h"
#include "session/system/session.h"
#include "session/manager/session_manager.h"
#include "node/system/node/node_kafka_command_handler.h"
#include <node_config_manager.h>
#include "proto/scene_manager/scene_manager_service.pb.h"
#include "proto/contracts/kafka/gate_command.pb.h"
#include "gate_globals.h"

ProtobufCodec* gGateCodec = nullptr;

namespace {

void startGateNode(EventLoop& loop)
{
    // ── TCP 层基础设施 ──────────────────────────────────────────────────────
    ProtobufDispatcher protobufDispatcher([](const TcpConnectionPtr& conn, const MessagePtr& msg, Timestamp) {
        LOG_ERROR << "Unknown message: " << std::string(msg->GetTypeName());
        conn->shutdown();
    });
    ProtobufCodec codec([&protobufDispatcher](const TcpConnectionPtr& conn, const MessagePtr& msg, Timestamp ts) {
        protobufDispatcher.onProtobufMessage(conn, msg, ts);
    });
    RpcClientSessionHandler rpcClientHandler(codec, protobufDispatcher);
    gGateCodec = &codec;

    LOG_INFO << "gRPC client config: ResourceQuota max threads=" << grpc_channel_cache::ConfiguredMaxThreads()
        << ", backup poll interval ms=" << grpc_channel_cache::ConfiguredBackupPollIntervalMs()
        << ", EventEngine pool reserve=" << (grpc_channel_cache::ConfiguredThreadPoolReserveThreads() > 0
            ? std::to_string(grpc_channel_cache::ConfiguredThreadPoolReserveThreads())
            : std::string("default"));

    // ── gRPC 响应 → 客户端 TCP 桥接 ────────────────────────────────────────
    SetIfEmptyHandler([&rpcClientHandler](const ClientContext& ctx, const ::google::protobuf::Message& reply) {
        auto sd = GetSessionDetailsByClientContext(ctx);
        if (!sd) return;
        auto it = tlsSessionManager.sessions().find(sd->session_id());
        if (it == tlsSessionManager.sessions().end()) return;
        rpcClientHandler.SendMessageToClient(it->second.conn, reply);
    });

    // ── 节点 ────────────────────────────────────────────────────────────────
    SimpleNode<GateHandler> node(&loop, "logs/gate", GateNodeService,
        Node::CanConnectNodeTypeList{ SceneNodeService, LoginNodeService });
    EventHandler::Register();

    // 启动后：挂载客户端 TCP 回调 + session ID 初始化
    node.SetAfterStart([&codec, &rpcClientHandler](SimpleNode<GateHandler>& n) {
        n.GetTcpServer().setConnectionCallback(
            [&rpcClientHandler](const TcpConnectionPtr& conn) {
                rpcClientHandler.OnConnection(conn);
            });
        n.GetTcpServer().setMessageCallback(
            [&codec](const TcpConnectionPtr& conn, muduo::net::Buffer* buf, Timestamp ts) {
                codec.onMessage(conn, buf, ts);
            });
        tlsSessionManager.session_id_gen().set_node_id(n.GetNodeId());
    });

    // Kafka: unified registration path for all node command-consumers.
    node.SetKafkaHandlers([](SimpleNode<GateHandler>& n) {
        node::kafka::KafkaCommandHandlerOptions options;
        options.topicPrefix = "gate";
        options.groupPrefix = "gate-group";
        options.nodeIdFieldNames = { "target_gate_id", "target_node_id" };
        options.instanceIdFieldNames = { "target_instance_id" };

        return node::kafka::RegisterKafkaCommandHandler<contracts::kafka::GateCommand>(
            n,
            options,
            [](const std::string& topic, const contracts::kafka::GateCommand& command) {
                DispatchGateKafkaCommand(topic, command);
            });
    });

    loop.runEvery(10.0, [] {
        LOG_INFO << "gRPC channel cache: active targets=" << grpc_channel_cache::CachedTargetCount();
    });

    loop.loop();
}

} // namespace

int main(int argc, char* argv[])
{
    absl::InitializeLog();
    EventLoop loop;
    startGateNode(loop);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}