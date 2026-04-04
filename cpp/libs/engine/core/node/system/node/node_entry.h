#pragma once

#ifdef __linux__
#include <absl/log/initialize.h>
#else
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#endif
#include "muduo/net/EventLoop.h"
#include "node/system/node/simple_node.h"
#include "node/system/node/node_kafka_command_handler.h"
#include "table/code/all_table.h"
#include "thread_context/ecs_context.h"

#include <google/protobuf/stubs/common.h>

#include <memory>
#include <type_traits>
#include <utility>

// ── Linker-level convention ────────────────────────────────────────
// Every node executable must define this function (typically in
// handler/event/event_handler.cpp).  It is called automatically
// after SimpleNode construction.
// ────────────────────────────────────────────────────────────────────
void RegisterNodeEvents();

namespace node {
namespace entry {

// ── THooks convention ──────────────────────────────────────────────
// A plain struct with optional nested types.  Only declare what you need.
//
//   struct MyNodeHooks {
//       struct TableLoadHandler {
//           static void OnLoaded();                    // called after tables load
//       };
//       using KafkaCommandType = contracts::kafka::MyNodeCommand;
//   };
//
// Omit any member to skip that hook.  Pass void (the default) to skip all.
// ────────────────────────────────────────────────────────────────────

namespace detail {

template <typename T, typename = void>
struct has_table_load_handler : std::false_type {};
template <typename T>
struct has_table_load_handler<T, std::void_t<typename T::TableLoadHandler>> : std::true_type {};

template <typename T, typename = void>
struct has_kafka_command_type : std::false_type {};
template <typename T>
struct has_kafka_command_type<T, std::void_t<typename T::KafkaCommandType>> : std::true_type {};

} // namespace detail

// Applied BEFORE SimpleNode construction (table callback must be registered
// before Initialize() triggers LoadTablesAsync()).
template <typename THooks>
void ApplyPreConstructionHooks()
{
    if constexpr (!std::is_void_v<THooks> && detail::has_table_load_handler<THooks>::value) {
        OnTablesLoadSuccess([] { THooks::TableLoadHandler::OnLoaded(); });
    }
}

// Applied AFTER SimpleNode construction but before the user configure lambda.
template <typename THooks, typename THandler>
void ApplyPostConstructionHooks(SimpleNode<THandler>& node)
{
    ::RegisterNodeEvents();

    if constexpr (!std::is_void_v<THooks>) {
        if constexpr (detail::has_kafka_command_type<THooks>::value) {
            node.SetKafkaHandlers([](SimpleNode<THandler>& n) {
                return kafka::RegisterKafkaCommandHandler<typename THooks::KafkaCommandType>(n);
            });
        }
    }
}

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

template <typename THandler, typename THooks = void, typename ConfigureFn>
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
        ApplyPreConstructionHooks<THooks>();
        SimpleNode<THandler> node(&loop, logDir, nodeType, std::move(connectTo));
        ApplyPostConstructionHooks<THooks>(node);
        configure(node);
        loop.loop();
    });
}

template <typename THandler, typename THooks = void, typename MakeContextFn, typename ConfigureFn>
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
        ApplyPreConstructionHooks<THooks>();
        SimpleNode<THandler> node(&loop, logDir, nodeType, std::move(connectTo));
        ApplyPostConstructionHooks<THooks>(node);
        configure(node, context);
        loop.loop();
    });
}

template <typename THandler, typename TContext, typename THooks = void, typename InitContextFn, typename ConfigureFn>
int RunSimpleNodeMainWithOwnedContext(const std::string& logDir,
                                      uint32_t nodeType,
                                      Node::CanConnectNodeTypeList connectTo,
                                      InitContextFn&& initContext,
                                      ConfigureFn&& configure)
{
    return RunSimpleNodeMainWithContext<THandler, THooks>(
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
