#pragma once

#ifdef __linux__
#include <absl/log/initialize.h>
#else
#include "grpc/third_party/abseil-cpp/absl/log/initialize.h"
#endif
#include "muduo/net/EventLoop.h"
#include "node/system/node/node.h"
#include "node/system/node/node_kafka_command_handler.h"
#include "table/code/all_table.h"
#include "thread_context/ecs_context.h"

#include <google/protobuf/stubs/common.h>

#include <memory>
#include <type_traits>
#include <utility>

// Every node executable must define this function (typically generated
// in handler/event/event_handler.cpp by the code generator).
void RegisterNodeEvents();

namespace node {
namespace entry {

// ── THooks convention ──────────────────────────────────────────────
// A plain struct with optional nested types.  Only declare what you need.
//
//   struct MyNodeHooks {
//       struct TableLoadHandler { static void OnLoaded(); };
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

template <typename THooks>
void ApplyPreConstructionHooks()
{
    if constexpr (!std::is_void_v<THooks> && has_table_load_handler<THooks>::value) {
        OnTablesLoadSuccess([] { THooks::TableLoadHandler::OnLoaded(); });
    }
}

template <typename THooks>
void ApplyPostConstructionHooks(Node& node)
{
    ::RegisterNodeEvents();
    if constexpr (!std::is_void_v<THooks> && has_kafka_command_type<THooks>::value) {
        node.SetKafkaHandlers([](Node& n) {
            return kafka::RegisterKafkaCommandHandler<typename THooks::KafkaCommandType>(n);
        });
    }
}

} // namespace detail

// ── Entry helper ──────────────────────────────────────────────────

template <typename StartNodeFn>
int RunNodeMain(StartNodeFn&& startNode)
{
    absl::InitializeLog();
    muduo::net::EventLoop loop;
    startNode(loop);
    tlsEcs.Clear();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}

template <typename THandler, typename TContext, typename THooks = void, typename ConfigureFn>
int RunSimpleNodeMainWithOwnedContext(uint32_t nodeType,
                                      Node::CanConnectNodeTypeList connectTo,
                                      ConfigureFn&& configure)
{
    return RunNodeMain([
        nodeType,
        connectTo = std::move(connectTo),
        configure = std::forward<ConfigureFn>(configure)
    ](muduo::net::EventLoop& loop) mutable {
        auto context = std::make_unique<TContext>();
        detail::ApplyPreConstructionHooks<THooks>();
        THandler handler;
        Node node(&loop, nodeType, std::move(connectTo), &handler);
        detail::ApplyPostConstructionHooks<THooks>(node);
        configure(node, *context);
        loop.loop();
    });
}

} // namespace entry
} // namespace node
