#include "gate_node.h"

#include <grpcpp/grpcpp.h>
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
    isRunning_ = false;
    {
        std::lock_guard<std::mutex> lock(streamMutex_);
        if (streamContext_) {
            streamContext_->TryCancel();
        }
    }
    if (streamThread_ && streamThread_->joinable()) {
        streamThread_->join();
    }
}

void GateNode::StartRpcServer()
{
	Node::StartRpcServer();

	rpcServer->GetTcpServer().setConnectionCallback(
		std::bind(&GateNode::OnConnection, this, std::placeholders::_1));
	rpcServer->GetTcpServer().setMessageCallback(
		std::bind(&ProtobufCodec::onMessage, &codec_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	tlsSessionManager.session_id_gen().set_node_id(GetNodeId());

    // Connect to SceneManager (Go-Zero)
    ConnectToSceneManager();
}

void GateNode::ConnectToSceneManager() {
    // TODO: Read address from config or discovery
    std::string address = "127.0.0.1:8080"; 
    LOG_INFO << "Connecting to SceneManager at " << address;

    sceneManagerChannel_ = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    sceneManagerStub_ = scene_manager::SceneManager::NewStub(sceneManagerChannel_);

    isRunning_ = true;
    // Start handling stream in a separate thread
    streamThread_ = std::make_unique<std::thread>(&GateNode::HandleStream, this);
}

void GateNode::HandleStream() {
    // Retry loop
    while (isRunning_) {
        // Reset stream first to ensure it doesn't access destroyed context
        gateStream_.reset();
        
        {
            std::lock_guard<std::mutex> lock(streamMutex_);
            if (!isRunning_) break;
            streamContext_ = std::make_unique<grpc::ClientContext>();
        }

        gateStream_ = sceneManagerStub_->GateConnect(streamContext_.get());
        
        // Send initial heartbeat
        scene_manager::GateHeartbeat heartbeat;
        heartbeat.set_gate_id(std::to_string(GetNodeId()));
        heartbeat.set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        heartbeat.set_connection_count(0); // TODO: Get actual count

        if (!gateStream_->Write(heartbeat)) {
            LOG_ERROR << "Failed to send initial heartbeat to SceneManager";
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        LOG_INFO << "Connected to SceneManager stream";

        // Read loop
        scene_manager::GateCommand command;
        while (gateStream_->Read(&command)) {
            LOG_INFO << "Received GateCommand: type=" << command.command_type() 
                     << " player=" << command.player_id()
                     << " target=" << command.target_node_id();
            
            // Handle command here
            if (command.command_type() == scene_manager::GateCommand::RoutePlayer) {
                 uint64_t sessionId = command.session_id();
                 uint32_t targetNodeId = 0;
                 try {
                     targetNodeId = (uint32_t)std::stoul(command.target_node_id());
                 } catch(...) {
                     LOG_ERROR << "Invalid target node id: " << command.target_node_id();
                     continue;
                 }

                 auto threadPool = rpcServer->GetTcpServer().threadPool();
                 if (threadPool) {
                     auto loops = threadPool->getAllLoops();
                     if (loops.empty()) {
                         // Single thread mode
                         if (GetLoop()) {
                             GetLoop()->runInLoop([sessionId, targetNodeId]() {
                                 auto it = tlsSessionManager.sessions().find(sessionId);
                                 if (it != tlsSessionManager.sessions().end()) {
                                     it->second.SetNodeId(SceneNodeService, targetNodeId);
                                     LOG_INFO << "Routed session " << sessionId << " to node " << targetNodeId;
                                 }
                             });
                         }
                     } else {
                         for (auto* loop : loops) {
                             loop->runInLoop([sessionId, targetNodeId]() {
                                 auto it = tlsSessionManager.sessions().find(sessionId);
                                 if (it != tlsSessionManager.sessions().end()) {
                                     it->second.SetNodeId(SceneNodeService, targetNodeId);
                                     LOG_INFO << "Routed session " << sessionId << " to node " << targetNodeId;
                                 }
                             });
                         }
                     }
                 }
            }
        }

        Status status = gateStream_->Finish();
        if (!status.ok()) {
            LOG_ERROR << "SceneManager stream failed: " << status.error_message();
        } else {
            LOG_INFO << "SceneManager stream closed";
        }
        
        if (isRunning_) {
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Reconnect delay
        }
    }
}
