#pragma once

#ifdef __linux__
#include <absl/log/initialize.h>
#else
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#endif
#include "muduo/net/EventLoop.h"
#include "node/system/node/simple_node.h"
#include "thread_context/ecs_context.h"

#include <google/protobuf/stubs/common.h>

#include <memory>
#include <utility>

namespace node {
namespace entry {

template <typename StartNodeFn>
int RunNodeMain(StartNodeFn&& startNode)
{
    absl::InitializeLog();
    muduo::net::EventLoop loop;
    startNode(loop);
    // Clear all registries while the EventLoop is still alive so that
    // TimerTaskComp destructors can safely cancel their timers.
    // After ~EventLoop, getEventLoopOfCurrentThread() returns nullptr.
    tlsEcs.Clear();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

template <typename THandler, typename ConfigureFn>
int RunSimpleNodeMain(const std::string& logDir,
                      uint32_t nodeType,
                      Node::CanConnectNodeTypeList connectTo,
                      ConfigureFn&& configure)
{
    return RunNodeMain([
        logDir,
        nodeType,
        connectTo = std::move(connectTo),
        configure = std::forward<ConfigureFn>(configure)
    ](muduo::net::EventLoop& loop) mutable {
        SimpleNode<THandler> node(&loop, logDir, nodeType, std::move(connectTo));
        configure(node);
        loop.loop();
    });
}

template <typename THandler, typename MakeContextFn, typename ConfigureFn>
int RunSimpleNodeMainWithContext(const std::string& logDir,
                                 uint32_t nodeType,
                                 Node::CanConnectNodeTypeList connectTo,
                                 MakeContextFn&& makeContext,
                                 ConfigureFn&& configure)
{
    return RunNodeMain([
        logDir,
        nodeType,
        connectTo = std::move(connectTo),
        makeContext = std::forward<MakeContextFn>(makeContext),
        configure = std::forward<ConfigureFn>(configure)
    ](muduo::net::EventLoop& loop) mutable {
        auto context = makeContext(loop);
        SimpleNode<THandler> node(&loop, logDir, nodeType, std::move(connectTo));
        configure(node, context);
        loop.loop();
    });
}

template <typename THandler, typename TContext, typename InitContextFn, typename ConfigureFn>
int RunSimpleNodeMainWithOwnedContext(const std::string& logDir,
                                      uint32_t nodeType,
                                      Node::CanConnectNodeTypeList connectTo,
                                      InitContextFn&& initContext,
                                      ConfigureFn&& configure)
{
    return RunSimpleNodeMainWithContext<THandler>(
        logDir,
        nodeType,
        std::move(connectTo),
        [initContext = std::forward<InitContextFn>(initContext)](muduo::net::EventLoop& loop) mutable {
            auto context = std::make_unique<TContext>();
            initContext(loop, *context);
            return context;
        },
        [configure = std::forward<ConfigureFn>(configure)](SimpleNode<THandler>& node,
                                                           std::unique_ptr<TContext>& context) mutable {
            configure(node, *context);
        });
}

} // namespace entry
} // namespace node
