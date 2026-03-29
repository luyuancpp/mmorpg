#include "muduo/base/CrossPlatformAdapterFunction.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoopThreadPool.h"

#include "node/system/node/simple_node.h"
#include "node/system/node/node.h"
#include "node/system/node/node_entry.h"
#include "handler/rpc/gate_service_handler.h"
#include "handler/rpc/client_message_processor.h"
#include "handler/event/event_handler.h"
#include "handler/event/gate_kafka_command_router.h"
#include "gate_codec.h"
#include "grpc_client/grpc_init_client.h"
#include "node/system/grpc_channel_cache.h"
#include "session/system/session.h"
#include "session/manager/session_manager.h"
#include "node/system/node/node_kafka_command_handler.h"
#include <node_config_manager.h>
#include "proto/scene_manager/scene_manager_service.pb.h"
#include "proto/contracts/kafka/gate_command.pb.h"

#include <utility>

namespace
{

    struct GateRuntimeContext
    {
        ProtobufDispatcher protobufDispatcher;
        ProtobufCodec codec;
        RpcClientSessionHandler rpcClientHandler;
        DependencyGate dependencyGate;
        TimerTaskComp playerCountReportTimer;

        GateRuntimeContext()
            : protobufDispatcher([](const TcpConnectionPtr &conn, const MessagePtr &msg, Timestamp)
                                 {
            LOG_ERROR << "Unknown message: " << std::string(msg->GetTypeName());
            conn->shutdown(); }),
              codec([this](const TcpConnectionPtr &conn, const MessagePtr &msg, Timestamp ts)
                    { protobufDispatcher.onProtobufMessage(conn, msg, ts); }),
              rpcClientHandler(codec, protobufDispatcher)
        {
        }
    };

    void LogGrpcThreadConfig()
    {
        LOG_INFO << "gRPC client config: ResourceQuota max threads=" << grpc_channel_cache::ConfiguredMaxThreads()
                 << ", backup poll interval ms=" << grpc_channel_cache::ConfiguredBackupPollIntervalMs()
                 << ", EventEngine pool reserve=" << (grpc_channel_cache::ConfiguredThreadPoolReserveThreads() > 0 ? std::to_string(grpc_channel_cache::ConfiguredThreadPoolReserveThreads()) : std::string("default"))
                 << ", EventEngine pool max=" << (grpc_channel_cache::ConfiguredThreadPoolMaxThreads() > 0 ? std::to_string(grpc_channel_cache::ConfiguredThreadPoolMaxThreads()) : std::string("unlimited"));
    }

} // namespace

int main(int argc, char *argv[])
{
    return node::entry::RunSimpleNodeMainWithOwnedContext<GateHandler, GateRuntimeContext>(
        "logs/gate",
        GateNodeService,
        Node::CanConnectNodeTypeList{SceneNodeService, LoginNodeService},
        [](EventLoop &, GateRuntimeContext &context)
        {
            LogGrpcThreadConfig();
        },
        [](SimpleNode<GateHandler> &node, GateRuntimeContext &context)
        {
            InitGateCodec(context.codec);
            // gRPC response -> client TCP bridge
            SetIfEmptyHandler([&context](const ClientContext &ctx, const ::google::protobuf::Message &reply)
                              {
                auto sd = GetSessionDetailsByClientContext(ctx);
                if (!sd) return;
                auto it = tlsSessionManager.sessions().find(sd->session_id());
                if (it == tlsSessionManager.sessions().end()) return;
                context.rpcClientHandler.SendMessageToClient(it->second.conn, reply); });

            EventHandler::Register();

            // Post-startup: attach client TCP callbacks + initialize session ID generator
            node.SetAfterStart([&context](SimpleNode<GateHandler> &n)
                               { context.dependencyGate.WaitAndRun(n, {LoginNodeService, SceneNodeService}, [&context](SimpleNode<GateHandler> &n)
                                                                   {
                        n.GetTcpServer().setConnectionCallback(
                            [&context](const TcpConnectionPtr& conn) {
                                context.rpcClientHandler.OnConnection(conn);
                            });
                        n.GetTcpServer().setMessageCallback(
                            [&context](const TcpConnectionPtr& conn, muduo::net::Buffer* buf, Timestamp ts) {
                                context.codec.onMessage(conn, buf, ts);
                            });
                        tlsSessionManager.session_id_gen().set_node_id(n.GetNodeId());

                        // Report player_count to etcd every 10 seconds for load balancing
                        context.playerCountReportTimer.RunEvery(10.0, [&n] {
                            auto count = static_cast<uint32_t>(tlsSessionManager.sessions().size());
                            n.GetNodeInfo().set_player_count(count);
                            n.GetEtcdManager().UpdateNodeInfo();
                        }); }, "Gate"); });

            // Kafka: unified registration path for all node command-consumers.
            node.SetKafkaHandlers([](SimpleNode<GateHandler> &n)
                                  {
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
                    }); });
        });
}