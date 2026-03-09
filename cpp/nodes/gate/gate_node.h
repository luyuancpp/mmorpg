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
    void ConnectToSceneManager(); // Keeping for now, might repurpose for Kafka init

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

private:
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    RpcClientSessionHandler rpcClientHandler;
    GateHandler nodeReplyService;

    // Kafka Consumer will be added here
    std::unique_ptr<std::thread> kafkaConsumerThread_;
    std::atomic<bool> isRunning_{false};
};

extern GateNode* gGateNode;
