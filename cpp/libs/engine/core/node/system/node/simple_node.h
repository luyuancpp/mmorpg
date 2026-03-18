#pragma once

#include "node/system/node/node.h"
#include "node/system/node/thread_observability.h"

// SimpleNode<THandler> — 用于快速创建标准 node，消除重复样板代码。
//
// ┌─ 使用场景 ─────────────────────────────────────────────────────────┐
// │ 情况 A：极简 node，启动后什么都不需要做                             │
// │   → 直接在 main.cpp 里实例化 SimpleNode<MyHandler>，不需要单独的  │
// │     *_node.h / *_node.cpp 文件                                    │
// │                                                                   │
// │ 情况 B：需要在"服务启动后"做点小事（加定时器、初始化子系统等）       │
// │   → 继承 SimpleNode<MyHandler>，覆盖 OnAfterStartRpcServer()       │
// │                                                                   │
// │ 情况 C：有客户端 TCP 栈或完全不同的启动流程（Gate/Scene 这类）       │
// │   → 直接继承 Node，不走 SimpleNode                                 │
// └───────────────────────────────────────────────────────────────────┘
//
// 新加一个 node 的必做步骤（不可省略）：
//   1. proto/common/base/node.proto      添加枚举值
//   2. node_util.cpp                     添加 nodeTypeNameMap 条目
//   3. proto/<node>/<node>.proto         定义 RPC service
//   4. handler/rpc/<node>_handler.h/.cpp 实现 RPC 方法（业务逻辑在这里）
//   5. handler/rpc/register_handler.cpp  填充 InitServiceHandler()
//   6. main.cpp                          见情况 A 示例
//
// ── 情况 A：main.cpp 示例（Chat 节点，无任何 *_node 文件）──────────────
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
// ── 情况 B：需要启动后逻辑的示例 ────────────────────────────────────────
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

    // 仅在 StartRpcServer() 后调用
    muduo::net::TcpServer& GetTcpServer() { return rpcServer->GetTcpServer(); }

    // Go-style: 启动前注册钩子，代替继承子类
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
