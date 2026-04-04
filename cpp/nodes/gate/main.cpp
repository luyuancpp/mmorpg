#include "muduo/base/CrossPlatformAdapterFunction.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoopThreadPool.h"

#include "node/system/node/node_entry.h"
#include "handler/rpc/gate_service_handler.h"
#include "handler/rpc/client_message_processor.h"
#include "gate_codec.h"
#include "grpc_client/grpc_init_client.h"
#include "node/system/grpc_channel_cache.h"
#include "session/system/session.h"
#include "session/manager/session_manager.h"
#include <node_config_manager.h>
#include "proto/scene_manager/scene_manager_service.pb.h"
#include "proto/contracts/kafka/gate_command.pb.h"
#include "proto/common/base/message.pb.h"
#include "rpc/service_metadata/rpc_event_registry.h"

#include <unordered_map>
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

    struct GateNodeHooks {
        using KafkaCommandType = contracts::kafka::GateCommand;
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
    return node::entry::RunSimpleNodeMainWithOwnedContext<GateHandler, GateRuntimeContext, GateNodeHooks>(
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

            // Build reverse map: response proto full_name -> message_id
            // Used to wrap gRPC replies in MessageContent for the client.
            static std::unordered_map<std::string, uint32_t> sResponseTypeToMsgId;
            for (uint32_t i = 0; i < kMaxRpcMethodCount; ++i) {
                const auto& meta = gRpcMethodRegistry[i];
                if (meta.responseProto) {
                    sResponseTypeToMsgId[std::string(meta.responseProto->GetDescriptor()->full_name())] = i;
                }
            }

            // gRPC response -> client TCP bridge (wrap in MessageContent)
            SetIfEmptyHandler([&context](const ClientContext &ctx, const ::google::protobuf::Message &reply)
                              {
                auto sd = GetSessionDetailsByClientContext(ctx);
                if (!sd) return;
                auto it = tlsSessionManager.sessions().find(sd->session_id());
                if (it == tlsSessionManager.sessions().end()) return;

                // If reply is already MessageContent, send directly.
                if (reply.GetDescriptor() == MessageContent::descriptor()) {
                    context.rpcClientHandler.SendMessageToClient(it->second.conn, reply);
                    return;
                }

                // Otherwise, wrap in MessageContent envelope for the client.
                std::string typeName(reply.GetDescriptor()->full_name());
                auto typeIt = sResponseTypeToMsgId.find(typeName);
                if (typeIt == sResponseTypeToMsgId.end()) {
                    LOG_ERROR << "No message_id mapping for gRPC response type: " << typeName.c_str();
                    return;
                }
                MessageContent mc;
                mc.set_serialized_message(reply.SerializeAsString());
                mc.set_message_id(typeIt->second);
                context.rpcClientHandler.SendMessageToClient(it->second.conn, mc); });

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
        });
}