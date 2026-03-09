#pragma once

#include <stdio.h>

#include "muduo/base/AsyncLogging.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "handler/rpc/client_message_processor.h"
#include "handler/rpc/gate_service_handler.h"
#include "network/rpc_connection_event.h"
#include "network/codec/codec.h"
#include "network/codec/dispatcher.h"
#include "time/comp/timer_task_comp.h"
#include "node/system/node/node.h"

#include <grpcpp/grpcpp.h>
#include "proto/scene_manager/scene_manager_service.grpc.pb.h"
#include <thread>
#include <atomic>
#include <mutex>

class GateNode : public  Node
{
public:
    using TcpServerPtr = std::unique_ptr<TcpServer>;

    explicit GateNode(EventLoop* loop);
    ~GateNode() override; // Changed to non-inline to handle member destruction
    inline ProtobufCodec& Codec() { return codec_; }
	::google::protobuf::Service* GetNodeReplyService() override{ return &nodeReplyService ; }

    inline void SendMessageToClient(const muduo::net::TcpConnectionPtr& conn,
                            const ::google::protobuf::Message& message) const { rpcClientHandler.SendMessageToClient(conn, message); }
    void StartRpcServer()override;

    // Connect to SceneManager
    void ConnectToSceneManager();

private:
    void OnConnection(const TcpConnectionPtr& conn)
    {
        rpcClientHandler.OnConnection(conn);
    }

    void OnUnknownMessage(const TcpConnectionPtr& conn,
                                 const MessagePtr& message,
                                 Timestamp)
    {
        LOG_ERROR << "onUnknownMessage: " << message->GetTypeName().data();
        conn->shutdown();
    }

    void HandleStream();

private:
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    RpcClientSessionHandler rpcClientHandler;
    GateHandler nodeReplyService;

    // SceneManager Control Stream
    std::unique_ptr<scene_manager::SceneManager::Stub> sceneManagerStub_;
    std::shared_ptr<grpc::Channel> sceneManagerChannel_;
    std::unique_ptr<grpc::ClientReaderWriter<scene_manager::GateHeartbeat, scene_manager::GateCommand>> gateStream_;
    std::unique_ptr<std::thread> streamThread_;
    std::unique_ptr<grpc::ClientContext> streamContext_;
    std::atomic<bool> isRunning_{false};
    std::mutex streamMutex_;
};

extern GateNode* gGateNode;
