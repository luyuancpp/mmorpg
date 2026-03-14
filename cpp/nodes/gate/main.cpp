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
#include "session/system/session.h"
#include "session/manager/session_manager.h"
#include <node_config_manager.h>
#include <messaging/kafka/kafka_proto_decoder.h>
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
        Node::CanConnectNodeTypeList{ CentreNodeService, SceneNodeService, LoginNodeService });
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

    // Kafka：订阅 gate-{id} topic，分发 SceneManager 命令
    node.SetKafkaHandlers([](SimpleNode<GateHandler>& n) {
        auto& kafkaConfig = tlsNodeConfigManager.GetBaseDeployConfig().kafka();
        std::string groupId = kafkaConfig.group_id();
        if (groupId.empty()) {
            groupId = "gate-group-" + std::to_string(n.GetNodeId());
        }
        const std::string topic = "gate-" + std::to_string(n.GetNodeId());
        LOG_INFO << "Registering gate Kafka handlers for node_id=" << n.GetNodeId();

        return n.RegisterKafkaMessageHandler({ topic }, groupId,
            [&n](const std::string& t, const std::string& payload) {
                auto command = DecodeKafkaProtoPayload<contracts::kafka::GateCommand>(t, payload);
                if (!command) {
                    return;
                }
                const auto& targetId = command->target_instance_id();
                if (!targetId.empty() && targetId != n.GetNodeInfo().node_uuid()) {
                    LOG_WARN << "Stale GateCommand ignored. target=" << targetId;
                    return;
                }
                DispatchGateKafkaCommand(t, *command);
            });
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