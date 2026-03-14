#include "gate_node.h"
#include "node_config_manager.h"

#include <thread>
#include <chrono>

#include "grpc_client/login/login_grpc_client.h"
#include "core/network/rpc_session.h"
#include "proto/login/login.grpc.pb.h"
#include "proto/common/base/node.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "node/system/node/node_util.h"
#include "session/system/session.h"
#include "grpc_client/grpc_init_client.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "session/manager/session_manager.h"
#include "proto/scene_manager/scene_manager_service.pb.h"

GateNode* gGateNode = nullptr;

GateNode::GateNode(EventLoop* loop)
    : Node(loop, "logs/gate"),
    dispatcher_(std::bind(&GateNode::OnUnknownMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
    rpcClientHandler(codec_, dispatcher_)
{
	gGateNode = this;
	GetNodeInfo().set_node_type(GateNodeService);
	targetNodeTypeWhitelist = { CentreNodeService, SceneNodeService, LoginNodeService };
    Initialize();

	auto sendGrpcResponseToClientSession = [](const ClientContext& context, const ::google::protobuf::Message& reply) {
		auto sessionDetails = GetSessionDetailsByClientContext(context);
		if (nullptr == sessionDetails) {
			return;
		}
		auto it = tlsSessionManager.sessions().find(sessionDetails->session_id());
		if (it == tlsSessionManager.sessions().end()){
			return;
		}
		gGateNode->SendMessageToClient(it->second.conn, reply);
		};
	SetIfEmptyHandler(sendGrpcResponseToClientSession);
}

GateNode::~GateNode() {
}

void GateNode::StartRpcServer()
{
	Node::StartRpcServer();

	rpcServer->GetTcpServer().setConnectionCallback(
		std::bind(&GateNode::OnConnection, this, std::placeholders::_1));
	rpcServer->GetTcpServer().setMessageCallback(
		std::bind(&ProtobufCodec::onMessage, &codec_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	tlsSessionManager.session_id_gen().set_node_id(GetNodeId());

    if (!ConnectToSceneManager()) {
        LOG_FATAL << "Failed to initialize Gate Kafka consumer for gate " << GetNodeId();
    }
}

bool GateNode::ConnectToSceneManager() {
    LOG_INFO << "Initializing Kafka manager for Gate " << GetNodeId();

    auto& kafkaConfig = tlsNodeConfigManager.GetBaseDeployConfig().kafka();

    // Group ID
    // Use a unique group ID for this gate node to ensure independent consumption if topics are shared,
    // or standard group ID if topics are unique.
    // Given topic is gate-{id}, standard group ID works too, but let's be safe.
    std::string groupId = kafkaConfig.group_id();
    if (groupId.empty()) {
        groupId = "gate-group-" + std::to_string(GetNodeId());
    }

    // Topic: gate-{gate_id}
    std::string topic = "gate-" + std::to_string(GetNodeId());
    std::vector<std::string> topics = { topic };

    // Partitions: empty vector for automatic assignment/subscription
    std::vector<int32_t> partitions;

    if (!GetKafkaManager().Subscribe(kafkaConfig, topics, groupId, partitions,
        std::bind(&GateNode::HandleGateCommand, this, std::placeholders::_1, std::placeholders::_2))) {
        LOG_ERROR << "Failed to subscribe Gate Kafka consumer on topic: " << topic;
        return false;
    }

    LOG_INFO << "Kafka Consumer started on topic: " << topic;

    StartKafkaPolling();
    return true;
}

void GateNode::HandleGateCommand(const std::string& topic, const std::string& payload) {
    auto command = std::make_shared<scene_manager::GateCommand>();
    if (!command->ParseFromString(payload)) {
        LOG_ERROR << "Failed to parse GateCommand from topic " << topic;
        return;
    }

    const auto& targetInstanceId = command->target_instance_id();
    if (!targetInstanceId.empty() && targetInstanceId != GetNodeInfo().node_uuid()) {
        LOG_WARN << "Ignoring GateCommand for stale gate instance. topic: " << topic
                 << " target_instance_id: " << targetInstanceId
                 << " local_instance_id: " << GetNodeInfo().node_uuid();
        return;
    }

    for (auto* loop : rpcServer->GetTcpServer().threadPool()->getAllLoops()) {
        loop->runInLoop([command]() {
            HandleGateCommandInLoop(command);
        });
    }
}

void GateNode::HandleGateCommandInLoop(const GateCommandPtr& command) {
    auto session_id = command->session_id();
    // Check if this thread manages the session
    auto& sessions = tlsSessionManager.sessions();
    auto it = sessions.find(session_id);
    if (it == sessions.end()) {
        // Not on this thread
        return;
    }

    SessionInfo& session = it->second;
    LOG_INFO << "Processing GateCommand type: " << command->command_type() << " for session: " << session_id;

    switch (command->command_type()) {
    case scene_manager::GateCommand::RoutePlayer: {
        if (command->target_node_id() != 0) {
            session.SetNodeId(SceneNodeService, command->target_node_id());
            LOG_INFO << "Updated route for session " << session_id << " to SceneNode " << command->target_node_id();
        }
        break;
    }
    case scene_manager::GateCommand::KickPlayer: {
        if (session.conn) {
            session.conn->forceClose(); // Close connection
            LOG_INFO << "Kicked session " << session_id;
        }
        break;
    }
    default:
        LOG_WARN << "Unknown GateCommand type: " << command->command_type();
        break;
    }
}
