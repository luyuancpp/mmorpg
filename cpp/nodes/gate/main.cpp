#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

#include "node/system/node/simple_node.h"
#include "handler/rpc/gate_service_handler.h"
#include "handler/rpc/client_message_processor.h"
#include "grpc_client/grpc_init_client.h"
#include "session/manager/session_manager.h"
#include "node/system/node/node_config_manager.h"
#include "proto/scene_manager/scene_manager_service.pb.h"
#include "gate_globals.h"

ProtobufCodec* gGateCodec = nullptr;

namespace {

using GateCommandPtr     = std::shared_ptr<const scene_manager::GateCommand>;
using GateCommandHandler = std::function<void(SessionInfo&, const GateCommandPtr&)>;

void startGateNode(EventLoop& loop)
{
    // ── TCP 层基础设施 ──────────────────────────────────────────────────────
    ProtobufDispatcher dispatcher([](const TcpConnectionPtr& conn, const MessagePtr& msg, Timestamp) {
        LOG_ERROR << "Unknown message: " << msg->GetTypeName();
        conn->shutdown();
    });
    ProtobufCodec codec([&dispatcher](const TcpConnectionPtr& conn, const MessagePtr& msg, Timestamp ts) {
        dispatcher.onProtobufMessage(conn, msg, ts);
    });
    RpcClientSessionHandler rpcClientHandler(codec, dispatcher);
    gGateCodec = &codec;

    // ── gRPC 响应 → 客户端 TCP 桥接 ────────────────────────────────────────
    SetIfEmptyHandler([&rpcClientHandler](const ClientContext& ctx, const ::google::protobuf::Message& reply) {
        auto sd = GetSessionDetailsByClientContext(ctx);
        if (!sd) return;
        auto it = tlsSessionManager.sessions().find(sd->session_id());
        if (it == tlsSessionManager.sessions().end()) return;
        rpcClientHandler.SendMessageToClient(it->second.conn, reply);
    });

    // ── SceneManager → Gate Kafka 命令处理器 ────────────────────────────────
    std::unordered_map<int, GateCommandHandler> commandHandlers;
    commandHandlers[scene_manager::GateCommand::RoutePlayer] =
        [](SessionInfo& session, const GateCommandPtr& cmd) {
            if (cmd->target_node_id() == 0) return;
            session.SetNodeId(SceneNodeService, cmd->target_node_id());
            LOG_INFO << "RoutePlayer: session " << cmd->session_id()
                     << " -> SceneNode " << cmd->target_node_id();
        };
    commandHandlers[scene_manager::GateCommand::KickPlayer] =
        [](SessionInfo& session, const GateCommandPtr& cmd) {
            if (!session.conn) return;
            session.conn->forceClose();
            LOG_INFO << "KickPlayer: session " << cmd->session_id();
        };

    // ── 节点 ────────────────────────────────────────────────────────────────
    SimpleNode<GateHandler> node(&loop, "logs/gate", GateNodeService,
        CanConnectNodeTypeList{ CentreNodeService, SceneNodeService, LoginNodeService });

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
    node.SetKafkaHandlers([&commandHandlers](SimpleNode<GateHandler>& n) {
        auto& kafkaConfig = tlsNodeConfigManager.GetBaseDeployConfig().kafka();
        std::string groupId = kafkaConfig.group_id();
        if (groupId.empty()) {
            groupId = "gate-group-" + std::to_string(n.GetNodeId());
        }
        const std::string topic = "gate-" + std::to_string(n.GetNodeId());
        LOG_INFO << "Registering gate Kafka handlers for node_id=" << n.GetNodeId();

        return n.RegisterKafkaMessageHandler({ topic }, groupId,
            [&n, &commandHandlers](const std::string& t, const std::string& payload) {
                auto command = std::make_shared<scene_manager::GateCommand>();
                if (!command->ParseFromString(payload)) {
                    LOG_ERROR << "Failed to parse GateCommand from topic " << t;
                    return;
                }
                const auto& targetId = command->target_instance_id();
                if (!targetId.empty() && targetId != n.GetNodeInfo().node_uuid()) {
                    LOG_WARN << "Stale GateCommand ignored. target=" << targetId;
                    return;
                }
                for (auto* evloop : n.GetTcpServer().threadPool()->getAllLoops()) {
                    evloop->runInLoop([&commandHandlers, command] {
                        const auto it = tlsSessionManager.sessions().find(command->session_id());
                        if (it == tlsSessionManager.sessions().end()) return;
                        SessionInfo& session = it->second;
                        const auto handlerIt = commandHandlers.find(command->command_type());
                        if (handlerIt == commandHandlers.end()) {
                            LOG_WARN << "Unknown GateCommand type: " << command->command_type();
                            return;
                        }
                        handlerIt->second(session, command);
                    });
                }
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