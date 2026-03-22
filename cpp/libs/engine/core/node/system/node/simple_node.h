#pragma once

#include "node/system/node/node.h"
#include "node/system/node/thread_observability.h"

// SimpleNode<THandler> — eliminates boilerplate for standard node creation.
//
// ┌─ Usage scenarios ──────────────────────────────────────────────────┐
// │ Case A: Minimal node, no post-startup work needed                 │
// │   → Instantiate SimpleNode<MyHandler> in main.cpp directly.       │
// │     No separate *_node.h / *_node.cpp files needed.               │
// │                                                                   │
// │ Case B: Small post-startup work (timers, subsystem init, etc.)    │
// │   → Subclass SimpleNode<MyHandler>, override OnAfterStartRpcServer│
// │                                                                   │
// │ Case C: Custom TCP stack or entirely different startup (Gate/Scene)│
// │   → Inherit Node directly, skip SimpleNode                        │
// └───────────────────────────────────────────────────────────────────┘
//
// Required steps for adding a new node (cannot be skipped):
//   1. proto/common/base/node.proto      add enum value
//   2. node_util.cpp                     add nodeTypeNameMap entry
//   3. proto/<node>/<node>.proto         define RPC service
//   4. handler/rpc/<node>_handler.h/.cpp implement RPC methods (business logic here)
//   5. handler/rpc/register_handler.cpp  populate InitServiceHandler()
//   6. main.cpp                          see Case A example
//
// ── Case A: main.cpp example (Chat node, no *_node files) ──────────────
//
//   #include "engine/core/node/system/node/simple_node.h"
//   #include "handler/rpc/chat_handler.h"
//
//   int main() {
//       absl::InitializeLog();
//       muduo::net::EventLoop loop;
//       SimpleNode<ChatHandler> node(&loop, "logs/chat",
//                                    ChatNodeService,
//                                    {});
//       loop.loop();
//       google::protobuf::ShutdownProtobufLibrary();
//   }
//
// ── Case B: example with post-startup logic ──────────────────────────────────
//
//   class InstanceNode : public SimpleNode<InstanceHandler> {
//   public:
//       InstanceNode(muduo::net::EventLoop* loop)
//           : SimpleNode(loop, "logs/instance", InstanceNodeService, {}) {}
//   protected:
//       void OnAfterStartRpcServer() override {
//           cleanupTimer_.RunEvery(60s, &InstanceSystem::CleanupExpiredInstances);
//       }
//   private:
//       TimerTaskComp cleanupTimer_;
//   };

template <typename THandler>
class SimpleNode : public Node
{
public:
    using AfterStartFn   = std::function<void(SimpleNode<THandler>&)>;
    using KafkaHandlersFn = std::function<bool(SimpleNode<THandler>&)>;

    SimpleNode(muduo::net::EventLoop* loop,
               const std::string& logDir,
               uint32_t nodeType,
               CanConnectNodeTypeList connectTo = {})
        : Node(loop, logDir)
    {
        GetNodeInfo().set_node_type(nodeType);
        targetNodeTypeWhitelist = std::move(connectTo);
        Initialize();
        node::observability::RegisterThreadObservability(*loop, logDir);
    }

    ::google::protobuf::Service* GetNodeReplyService() override { return &handler_; }

    THandler& GetHandler() { return handler_; }

    // Only valid after StartRpcServer()
    muduo::net::TcpServer& GetTcpServer() { return rpcServer->GetTcpServer(); }

    // Go-style: register hooks before start instead of subclassing
    SimpleNode& SetAfterStart(AfterStartFn fn)    { afterStartFn_    = std::move(fn); return *this; }
    SimpleNode& SetKafkaHandlers(KafkaHandlersFn fn) { kafkaHandlersFn_ = std::move(fn); return *this; }

    void StartRpcServer() override
    {
        Node::StartRpcServer();
        if (afterStartFn_) afterStartFn_(*this);
    }

protected:
    bool RegisterKafkaHandlers() override
    {
        return kafkaHandlersFn_ ? kafkaHandlersFn_(*this) : true;
    }

    THandler handler_;

private:
    AfterStartFn    afterStartFn_;
    KafkaHandlersFn kafkaHandlersFn_;
};
