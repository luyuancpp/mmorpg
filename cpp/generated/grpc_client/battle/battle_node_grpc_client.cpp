#include "muduo/base/Logging.h"

#include "battle_node_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetBattleNodeCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

struct BattleNodeCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region BattleNodeCreateBattle
boost::object_pool<AsyncBattleNodeCreateBattleGrpcClient> BattleNodeCreateBattlePool;
using AsyncBattleNodeCreateBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::CreateBattleResponse&)>;
AsyncBattleNodeCreateBattleHandlerFunctionType AsyncBattleNodeCreateBattleHandler;
AsyncBattleNodeCreateBattleFailedHandlerFunctionType AsyncBattleNodeCreateBattleFailedHandler;

void CompleteBattleNodeCreateBattle(AsyncBattleNodeCreateBattleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleNodeCreateBattleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleNodeCreateBattlePool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleNodeCreateBattleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleNodeCreateBattleHandler) {
            AsyncBattleNodeCreateBattleHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.CreateBattle reply dropped: AsyncBattleNodeCreateBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeCreateBattleFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleNode.CreateBattle", call->context, call->status, call->sentMetadata};
        AsyncBattleNodeCreateBattleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleNode.CreateBattle failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleNodeCreateBattle(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleNodeCreateBattleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleNodeCreateBattle(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeCreateBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::CreateBattleResponse> done) {
    auto call(BattleNodeCreateBattlePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleNodeStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleNodeCreateBattle(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCreateBattle(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleNodeCreateBattleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request,
                                            GrpcDone<::CreateBattleResponse> done, const GrpcCallOptions& options) {
    StartBattleNodeCreateBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeCreateBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeCreateBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::CreateBattleRequest& derived = static_cast<const ::CreateBattleRequest&>(message);
    SendBattleNodeCreateBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeDestroyBattle
boost::object_pool<AsyncBattleNodeDestroyBattleGrpcClient> BattleNodeDestroyBattlePool;
using AsyncBattleNodeDestroyBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleNodeDestroyBattleHandlerFunctionType AsyncBattleNodeDestroyBattleHandler;
AsyncBattleNodeDestroyBattleFailedHandlerFunctionType AsyncBattleNodeDestroyBattleFailedHandler;

void CompleteBattleNodeDestroyBattle(AsyncBattleNodeDestroyBattleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleNodeDestroyBattleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleNodeDestroyBattlePool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleNodeDestroyBattleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleNodeDestroyBattleHandler) {
            AsyncBattleNodeDestroyBattleHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.DestroyBattle reply dropped: AsyncBattleNodeDestroyBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeDestroyBattleFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleNode.DestroyBattle", call->context, call->status, call->sentMetadata};
        AsyncBattleNodeDestroyBattleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleNode.DestroyBattle failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleNodeDestroyBattle(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleNodeDestroyBattleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleNodeDestroyBattle(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeDestroyBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleNodeDestroyBattlePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleNodeStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleNodeDestroyBattle(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDestroyBattle(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleNodeDestroyBattleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleNodeDestroyBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeDestroyBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeDestroyBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::DestroyBattleRequest& derived = static_cast<const ::DestroyBattleRequest&>(message);
    SendBattleNodeDestroyBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeAddObserver
boost::object_pool<AsyncBattleNodeAddObserverGrpcClient> BattleNodeAddObserverPool;
using AsyncBattleNodeAddObserverHandlerFunctionType =
    std::function<void(const ClientContext&, const ::AddObserverResponse&)>;
AsyncBattleNodeAddObserverHandlerFunctionType AsyncBattleNodeAddObserverHandler;
AsyncBattleNodeAddObserverFailedHandlerFunctionType AsyncBattleNodeAddObserverFailedHandler;

void CompleteBattleNodeAddObserver(AsyncBattleNodeAddObserverGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleNodeAddObserverGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleNodeAddObserverPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleNodeAddObserverGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleNodeAddObserverHandler) {
            AsyncBattleNodeAddObserverHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.AddObserver reply dropped: AsyncBattleNodeAddObserverHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeAddObserverFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleNode.AddObserver", call->context, call->status, call->sentMetadata};
        AsyncBattleNodeAddObserverFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleNode.AddObserver failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleNodeAddObserver(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleNodeAddObserverGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleNodeAddObserver(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeAddObserverCall(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AddObserverResponse> done) {
    auto call(BattleNodeAddObserverPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleNodeStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleNodeAddObserver(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAddObserver(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleNodeAddObserverMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request,
                                            GrpcDone<::AddObserverResponse> done, const GrpcCallOptions& options) {
    StartBattleNodeAddObserverCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeAddObserver(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeAddObserverCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::AddObserverRequest& derived = static_cast<const ::AddObserverRequest&>(message);
    SendBattleNodeAddObserver(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeRemoveObserver
boost::object_pool<AsyncBattleNodeRemoveObserverGrpcClient> BattleNodeRemoveObserverPool;
using AsyncBattleNodeRemoveObserverHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleNodeRemoveObserverHandlerFunctionType AsyncBattleNodeRemoveObserverHandler;
AsyncBattleNodeRemoveObserverFailedHandlerFunctionType AsyncBattleNodeRemoveObserverFailedHandler;

void CompleteBattleNodeRemoveObserver(AsyncBattleNodeRemoveObserverGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleNodeRemoveObserverGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleNodeRemoveObserverPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleNodeRemoveObserverGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleNodeRemoveObserverHandler) {
            AsyncBattleNodeRemoveObserverHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.RemoveObserver reply dropped: AsyncBattleNodeRemoveObserverHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeRemoveObserverFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleNode.RemoveObserver", call->context, call->status, call->sentMetadata};
        AsyncBattleNodeRemoveObserverFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleNode.RemoveObserver failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleNodeRemoveObserver(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleNodeRemoveObserverGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleNodeRemoveObserver(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeRemoveObserverCall(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleNodeRemoveObserverPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleNodeStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleNodeRemoveObserver(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRemoveObserver(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleNodeRemoveObserverMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleNodeRemoveObserverCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeRemoveObserver(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeRemoveObserverCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::RemoveObserverRequest& derived = static_cast<const ::RemoveObserverRequest&>(message);
    SendBattleNodeRemoveObserver(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeIssueBattleTicket
boost::object_pool<AsyncBattleNodeIssueBattleTicketGrpcClient> BattleNodeIssueBattleTicketPool;
using AsyncBattleNodeIssueBattleTicketHandlerFunctionType =
    std::function<void(const ClientContext&, const ::IssueBattleTicketResponse&)>;
AsyncBattleNodeIssueBattleTicketHandlerFunctionType AsyncBattleNodeIssueBattleTicketHandler;
AsyncBattleNodeIssueBattleTicketFailedHandlerFunctionType AsyncBattleNodeIssueBattleTicketFailedHandler;

void CompleteBattleNodeIssueBattleTicket(AsyncBattleNodeIssueBattleTicketGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleNodeIssueBattleTicketGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleNodeIssueBattleTicketPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleNodeIssueBattleTicketGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleNodeIssueBattleTicketHandler) {
            AsyncBattleNodeIssueBattleTicketHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.IssueBattleTicket reply dropped: AsyncBattleNodeIssueBattleTicketHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeIssueBattleTicketFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleNode.IssueBattleTicket", call->context, call->status, call->sentMetadata};
        AsyncBattleNodeIssueBattleTicketFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleNode.IssueBattleTicket failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleNodeIssueBattleTicket(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleNodeIssueBattleTicketGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleNodeIssueBattleTicket(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeIssueBattleTicketCall(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::IssueBattleTicketResponse> done) {
    auto call(BattleNodeIssueBattleTicketPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleNodeStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleNodeIssueBattleTicket(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncIssueBattleTicket(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleNodeIssueBattleTicketMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request,
                                            GrpcDone<::IssueBattleTicketResponse> done, const GrpcCallOptions& options) {
    StartBattleNodeIssueBattleTicketCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeIssueBattleTicket(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeIssueBattleTicketCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::IssueBattleTicketRequest& derived = static_cast<const ::IssueBattleTicketRequest&>(message);
    SendBattleNodeIssueBattleTicket(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleBattleNodeCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case BattleNodeCreateBattleMessageId:
            AsyncCompleteGrpcBattleNodeCreateBattle(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleNodeDestroyBattleMessageId:
            AsyncCompleteGrpcBattleNodeDestroyBattle(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleNodeAddObserverMessageId:
            AsyncCompleteGrpcBattleNodeAddObserver(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleNodeRemoveObserverMessageId:
            AsyncCompleteGrpcBattleNodeRemoveObserver(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleNodeIssueBattleTicketMessageId:
            AsyncCompleteGrpcBattleNodeIssueBattleTicket(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetBattleNodeHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncBattleNodeCreateBattleHandler = handler;
    AsyncBattleNodeDestroyBattleHandler = handler;
    AsyncBattleNodeAddObserverHandler = handler;
    AsyncBattleNodeRemoveObserverHandler = handler;
    AsyncBattleNodeIssueBattleTicketHandler = handler;
}

void SetBattleNodeIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncBattleNodeCreateBattleHandler) {
        AsyncBattleNodeCreateBattleHandler = handler;
    }
    if (!AsyncBattleNodeDestroyBattleHandler) {
        AsyncBattleNodeDestroyBattleHandler = handler;
    }
    if (!AsyncBattleNodeAddObserverHandler) {
        AsyncBattleNodeAddObserverHandler = handler;
    }
    if (!AsyncBattleNodeRemoveObserverHandler) {
        AsyncBattleNodeRemoveObserverHandler = handler;
    }
    if (!AsyncBattleNodeIssueBattleTicketHandler) {
        AsyncBattleNodeIssueBattleTicketHandler = handler;
    }
}

void SetBattleNodeFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncBattleNodeCreateBattleFailedHandler = handler;
    AsyncBattleNodeDestroyBattleFailedHandler = handler;
    AsyncBattleNodeAddObserverFailedHandler = handler;
    AsyncBattleNodeRemoveObserverFailedHandler = handler;
    AsyncBattleNodeIssueBattleTicketFailedHandler = handler;
}

void SetBattleNodeIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncBattleNodeCreateBattleFailedHandler) {
        AsyncBattleNodeCreateBattleFailedHandler = handler;
    }
    if (!AsyncBattleNodeDestroyBattleFailedHandler) {
        AsyncBattleNodeDestroyBattleFailedHandler = handler;
    }
    if (!AsyncBattleNodeAddObserverFailedHandler) {
        AsyncBattleNodeAddObserverFailedHandler = handler;
    }
    if (!AsyncBattleNodeRemoveObserverFailedHandler) {
        AsyncBattleNodeRemoveObserverFailedHandler = handler;
    }
    if (!AsyncBattleNodeIssueBattleTicketFailedHandler) {
        AsyncBattleNodeIssueBattleTicketFailedHandler = handler;
    }
}

void SetBattleNodeCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitBattleNodeGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<BattleNodeStubPtr>(nodeEntity, BattleNode::NewStub(channel));

}
