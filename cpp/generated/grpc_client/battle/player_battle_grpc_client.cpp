#include "muduo/base/Logging.h"

#include "player_battle_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetPlayerBattleCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

struct PlayerBattleCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region BattleClientPlayerSubmitBattleAction
boost::object_pool<AsyncBattleClientPlayerSubmitBattleActionGrpcClient> BattleClientPlayerSubmitBattleActionPool;
using AsyncBattleClientPlayerSubmitBattleActionHandlerFunctionType =
    std::function<void(const ClientContext&, const ::SubmitBattleActionResponse&)>;
AsyncBattleClientPlayerSubmitBattleActionHandlerFunctionType AsyncBattleClientPlayerSubmitBattleActionHandler;
AsyncBattleClientPlayerSubmitBattleActionFailedHandlerFunctionType AsyncBattleClientPlayerSubmitBattleActionFailedHandler;

void CompleteBattleClientPlayerSubmitBattleAction(AsyncBattleClientPlayerSubmitBattleActionGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerSubmitBattleActionGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerSubmitBattleActionPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerSubmitBattleActionGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerSubmitBattleActionHandler) {
            AsyncBattleClientPlayerSubmitBattleActionHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.SubmitBattleAction reply dropped: AsyncBattleClientPlayerSubmitBattleActionHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerSubmitBattleActionFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.SubmitBattleAction", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerSubmitBattleActionFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.SubmitBattleAction failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerSubmitBattleAction(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerSubmitBattleActionGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerSubmitBattleAction(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerSubmitBattleActionCall(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::SubmitBattleActionResponse> done) {
    auto call(BattleClientPlayerSubmitBattleActionPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerSubmitBattleAction(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSubmitBattleAction(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerSubmitBattleActionMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request,
                                            GrpcDone<::SubmitBattleActionResponse> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerSubmitBattleActionCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerSubmitBattleAction(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerSubmitBattleActionCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::SubmitBattleActionRequest& derived = static_cast<const ::SubmitBattleActionRequest&>(message);
    SendBattleClientPlayerSubmitBattleAction(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerGetBattleState
boost::object_pool<AsyncBattleClientPlayerGetBattleStateGrpcClient> BattleClientPlayerGetBattleStatePool;
using AsyncBattleClientPlayerGetBattleStateHandlerFunctionType =
    std::function<void(const ClientContext&, const ::BattleStateS2C&)>;
AsyncBattleClientPlayerGetBattleStateHandlerFunctionType AsyncBattleClientPlayerGetBattleStateHandler;
AsyncBattleClientPlayerGetBattleStateFailedHandlerFunctionType AsyncBattleClientPlayerGetBattleStateFailedHandler;

void CompleteBattleClientPlayerGetBattleState(AsyncBattleClientPlayerGetBattleStateGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerGetBattleStateGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerGetBattleStatePool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerGetBattleStateGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerGetBattleStateHandler) {
            AsyncBattleClientPlayerGetBattleStateHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.GetBattleState reply dropped: AsyncBattleClientPlayerGetBattleStateHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerGetBattleStateFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.GetBattleState", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerGetBattleStateFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.GetBattleState failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerGetBattleState(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerGetBattleStateGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerGetBattleState(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerGetBattleStateCall(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::BattleStateS2C> done) {
    auto call(BattleClientPlayerGetBattleStatePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerGetBattleState(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetBattleState(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerGetBattleStateMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request,
                                            GrpcDone<::BattleStateS2C> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerGetBattleStateCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerGetBattleState(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerGetBattleStateCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::GetBattleStateRequest& derived = static_cast<const ::GetBattleStateRequest&>(message);
    SendBattleClientPlayerGetBattleState(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleStart
boost::object_pool<AsyncBattleClientPlayerNotifyBattleStartGrpcClient> BattleClientPlayerNotifyBattleStartPool;
using AsyncBattleClientPlayerNotifyBattleStartHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifyBattleStartHandlerFunctionType AsyncBattleClientPlayerNotifyBattleStartHandler;
AsyncBattleClientPlayerNotifyBattleStartFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleStartFailedHandler;

void CompleteBattleClientPlayerNotifyBattleStart(AsyncBattleClientPlayerNotifyBattleStartGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifyBattleStartGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifyBattleStartPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifyBattleStartGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleStartHandler) {
            AsyncBattleClientPlayerNotifyBattleStartHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleStart reply dropped: AsyncBattleClientPlayerNotifyBattleStartHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleStartFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifyBattleStart", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifyBattleStartFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleStart failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifyBattleStart(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifyBattleStartGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifyBattleStart(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleStartCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifyBattleStartPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifyBattleStart(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyBattleStart(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifyBattleStartMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleStartCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleStart(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleStartCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::BattleStartS2C& derived = static_cast<const ::BattleStartS2C&>(message);
    SendBattleClientPlayerNotifyBattleStart(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyTurnResult
boost::object_pool<AsyncBattleClientPlayerNotifyTurnResultGrpcClient> BattleClientPlayerNotifyTurnResultPool;
using AsyncBattleClientPlayerNotifyTurnResultHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifyTurnResultHandlerFunctionType AsyncBattleClientPlayerNotifyTurnResultHandler;
AsyncBattleClientPlayerNotifyTurnResultFailedHandlerFunctionType AsyncBattleClientPlayerNotifyTurnResultFailedHandler;

void CompleteBattleClientPlayerNotifyTurnResult(AsyncBattleClientPlayerNotifyTurnResultGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifyTurnResultGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifyTurnResultPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifyTurnResultGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifyTurnResultHandler) {
            AsyncBattleClientPlayerNotifyTurnResultHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyTurnResult reply dropped: AsyncBattleClientPlayerNotifyTurnResultHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyTurnResultFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifyTurnResult", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifyTurnResultFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyTurnResult failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifyTurnResult(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifyTurnResultGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifyTurnResult(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyTurnResultCall(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifyTurnResultPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifyTurnResult(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyTurnResult(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifyTurnResultMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyTurnResultCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyTurnResult(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyTurnResultCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::TurnResultS2C& derived = static_cast<const ::TurnResultS2C&>(message);
    SendBattleClientPlayerNotifyTurnResult(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleEnd
boost::object_pool<AsyncBattleClientPlayerNotifyBattleEndGrpcClient> BattleClientPlayerNotifyBattleEndPool;
using AsyncBattleClientPlayerNotifyBattleEndHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifyBattleEndHandlerFunctionType AsyncBattleClientPlayerNotifyBattleEndHandler;
AsyncBattleClientPlayerNotifyBattleEndFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleEndFailedHandler;

void CompleteBattleClientPlayerNotifyBattleEnd(AsyncBattleClientPlayerNotifyBattleEndGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifyBattleEndGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifyBattleEndPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifyBattleEndGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleEndHandler) {
            AsyncBattleClientPlayerNotifyBattleEndHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleEnd reply dropped: AsyncBattleClientPlayerNotifyBattleEndHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleEndFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifyBattleEnd", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifyBattleEndFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleEnd failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifyBattleEnd(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifyBattleEndGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifyBattleEnd(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleEndCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifyBattleEndPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifyBattleEnd(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyBattleEnd(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifyBattleEndMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleEndCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleEnd(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleEndCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::BattleEndS2C& derived = static_cast<const ::BattleEndS2C&>(message);
    SendBattleClientPlayerNotifyBattleEnd(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleReconnect
boost::object_pool<AsyncBattleClientPlayerNotifyBattleReconnectGrpcClient> BattleClientPlayerNotifyBattleReconnectPool;
using AsyncBattleClientPlayerNotifyBattleReconnectHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifyBattleReconnectHandlerFunctionType AsyncBattleClientPlayerNotifyBattleReconnectHandler;
AsyncBattleClientPlayerNotifyBattleReconnectFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler;

void CompleteBattleClientPlayerNotifyBattleReconnect(AsyncBattleClientPlayerNotifyBattleReconnectGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifyBattleReconnectGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifyBattleReconnectPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifyBattleReconnectGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleReconnectHandler) {
            AsyncBattleClientPlayerNotifyBattleReconnectHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleReconnect reply dropped: AsyncBattleClientPlayerNotifyBattleReconnectHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifyBattleReconnect", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleReconnect failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifyBattleReconnect(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifyBattleReconnectGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifyBattleReconnect(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleReconnectCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifyBattleReconnectPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifyBattleReconnect(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyBattleReconnect(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifyBattleReconnectMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleReconnectCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleReconnect(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleReconnectCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::BattleReconnectS2C& derived = static_cast<const ::BattleReconnectS2C&>(message);
    SendBattleClientPlayerNotifyBattleReconnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerStopWatchBattle
boost::object_pool<AsyncBattleClientPlayerStopWatchBattleGrpcClient> BattleClientPlayerStopWatchBattlePool;
using AsyncBattleClientPlayerStopWatchBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::StopWatchBattleResponse&)>;
AsyncBattleClientPlayerStopWatchBattleHandlerFunctionType AsyncBattleClientPlayerStopWatchBattleHandler;
AsyncBattleClientPlayerStopWatchBattleFailedHandlerFunctionType AsyncBattleClientPlayerStopWatchBattleFailedHandler;

void CompleteBattleClientPlayerStopWatchBattle(AsyncBattleClientPlayerStopWatchBattleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerStopWatchBattleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerStopWatchBattlePool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerStopWatchBattleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerStopWatchBattleHandler) {
            AsyncBattleClientPlayerStopWatchBattleHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.StopWatchBattle reply dropped: AsyncBattleClientPlayerStopWatchBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerStopWatchBattleFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.StopWatchBattle", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerStopWatchBattleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.StopWatchBattle failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerStopWatchBattle(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerStopWatchBattleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerStopWatchBattle(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerStopWatchBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::StopWatchBattleResponse> done) {
    auto call(BattleClientPlayerStopWatchBattlePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerStopWatchBattle(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncStopWatchBattle(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerStopWatchBattleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request,
                                            GrpcDone<::StopWatchBattleResponse> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerStopWatchBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerStopWatchBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerStopWatchBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::StopWatchBattleRequest& derived = static_cast<const ::StopWatchBattleRequest&>(message);
    SendBattleClientPlayerStopWatchBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerSetAutoBattle
boost::object_pool<AsyncBattleClientPlayerSetAutoBattleGrpcClient> BattleClientPlayerSetAutoBattlePool;
using AsyncBattleClientPlayerSetAutoBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::SetAutoBattleResponse&)>;
AsyncBattleClientPlayerSetAutoBattleHandlerFunctionType AsyncBattleClientPlayerSetAutoBattleHandler;
AsyncBattleClientPlayerSetAutoBattleFailedHandlerFunctionType AsyncBattleClientPlayerSetAutoBattleFailedHandler;

void CompleteBattleClientPlayerSetAutoBattle(AsyncBattleClientPlayerSetAutoBattleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerSetAutoBattleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerSetAutoBattlePool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerSetAutoBattleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerSetAutoBattleHandler) {
            AsyncBattleClientPlayerSetAutoBattleHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.SetAutoBattle reply dropped: AsyncBattleClientPlayerSetAutoBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerSetAutoBattleFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.SetAutoBattle", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerSetAutoBattleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.SetAutoBattle failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerSetAutoBattle(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerSetAutoBattleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerSetAutoBattle(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerSetAutoBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::SetAutoBattleResponse> done) {
    auto call(BattleClientPlayerSetAutoBattlePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerSetAutoBattle(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSetAutoBattle(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerSetAutoBattleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request,
                                            GrpcDone<::SetAutoBattleResponse> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerSetAutoBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerSetAutoBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerSetAutoBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::SetAutoBattleRequest& derived = static_cast<const ::SetAutoBattleRequest&>(message);
    SendBattleClientPlayerSetAutoBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifySpectateState
boost::object_pool<AsyncBattleClientPlayerNotifySpectateStateGrpcClient> BattleClientPlayerNotifySpectateStatePool;
using AsyncBattleClientPlayerNotifySpectateStateHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifySpectateStateHandlerFunctionType AsyncBattleClientPlayerNotifySpectateStateHandler;
AsyncBattleClientPlayerNotifySpectateStateFailedHandlerFunctionType AsyncBattleClientPlayerNotifySpectateStateFailedHandler;

void CompleteBattleClientPlayerNotifySpectateState(AsyncBattleClientPlayerNotifySpectateStateGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifySpectateStateGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifySpectateStatePool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifySpectateStateGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifySpectateStateHandler) {
            AsyncBattleClientPlayerNotifySpectateStateHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateState reply dropped: AsyncBattleClientPlayerNotifySpectateStateHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifySpectateStateFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifySpectateState", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifySpectateStateFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateState failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifySpectateState(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifySpectateStateGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifySpectateState(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifySpectateStateCall(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifySpectateStatePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifySpectateState(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifySpectateState(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifySpectateStateMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifySpectateStateCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifySpectateState(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifySpectateStateCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::SpectateStateS2C& derived = static_cast<const ::SpectateStateS2C&>(message);
    SendBattleClientPlayerNotifySpectateState(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifySpectateTurnResult
boost::object_pool<AsyncBattleClientPlayerNotifySpectateTurnResultGrpcClient> BattleClientPlayerNotifySpectateTurnResultPool;
using AsyncBattleClientPlayerNotifySpectateTurnResultHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifySpectateTurnResultHandlerFunctionType AsyncBattleClientPlayerNotifySpectateTurnResultHandler;
AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandlerFunctionType AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler;

void CompleteBattleClientPlayerNotifySpectateTurnResult(AsyncBattleClientPlayerNotifySpectateTurnResultGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifySpectateTurnResultGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifySpectateTurnResultPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifySpectateTurnResultGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifySpectateTurnResultHandler) {
            AsyncBattleClientPlayerNotifySpectateTurnResultHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateTurnResult reply dropped: AsyncBattleClientPlayerNotifySpectateTurnResultHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifySpectateTurnResult", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateTurnResult failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifySpectateTurnResult(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifySpectateTurnResultGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifySpectateTurnResult(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifySpectateTurnResultCall(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifySpectateTurnResultPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifySpectateTurnResult(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifySpectateTurnResult(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifySpectateTurnResultMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifySpectateTurnResultCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifySpectateTurnResult(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifySpectateTurnResultCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::TurnResultS2C& derived = static_cast<const ::TurnResultS2C&>(message);
    SendBattleClientPlayerNotifySpectateTurnResult(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifySpectateEnd
boost::object_pool<AsyncBattleClientPlayerNotifySpectateEndGrpcClient> BattleClientPlayerNotifySpectateEndPool;
using AsyncBattleClientPlayerNotifySpectateEndHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifySpectateEndHandlerFunctionType AsyncBattleClientPlayerNotifySpectateEndHandler;
AsyncBattleClientPlayerNotifySpectateEndFailedHandlerFunctionType AsyncBattleClientPlayerNotifySpectateEndFailedHandler;

void CompleteBattleClientPlayerNotifySpectateEnd(AsyncBattleClientPlayerNotifySpectateEndGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifySpectateEndGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifySpectateEndPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifySpectateEndGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifySpectateEndHandler) {
            AsyncBattleClientPlayerNotifySpectateEndHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateEnd reply dropped: AsyncBattleClientPlayerNotifySpectateEndHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifySpectateEndFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifySpectateEnd", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifySpectateEndFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateEnd failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifySpectateEnd(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifySpectateEndGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifySpectateEnd(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifySpectateEndCall(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifySpectateEndPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifySpectateEnd(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifySpectateEnd(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifySpectateEndMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifySpectateEndCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifySpectateEnd(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifySpectateEndCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::SpectateEndS2C& derived = static_cast<const ::SpectateEndS2C&>(message);
    SendBattleClientPlayerNotifySpectateEnd(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleAssigned
boost::object_pool<AsyncBattleClientPlayerNotifyBattleAssignedGrpcClient> BattleClientPlayerNotifyBattleAssignedPool;
using AsyncBattleClientPlayerNotifyBattleAssignedHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncBattleClientPlayerNotifyBattleAssignedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleAssignedHandler;
AsyncBattleClientPlayerNotifyBattleAssignedFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler;

void CompleteBattleClientPlayerNotifyBattleAssigned(AsyncBattleClientPlayerNotifyBattleAssignedGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncBattleClientPlayerNotifyBattleAssignedGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        BattleClientPlayerNotifyBattleAssignedPool.destroy(completed);
    };
    const std::unique_ptr<AsyncBattleClientPlayerNotifyBattleAssignedGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleAssignedHandler) {
            AsyncBattleClientPlayerNotifyBattleAssignedHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleAssigned reply dropped: AsyncBattleClientPlayerNotifyBattleAssignedHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "BattleClientPlayer.NotifyBattleAssigned", call->context, call->status, call->sentMetadata};
        AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleAssigned failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcBattleClientPlayerNotifyBattleAssigned(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncBattleClientPlayerNotifyBattleAssignedGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteBattleClientPlayerNotifyBattleAssigned(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleAssignedCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(BattleClientPlayerNotifyBattleAssignedPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<BattleClientPlayerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteBattleClientPlayerNotifyBattleAssigned(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyBattleAssigned(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(BattleClientPlayerNotifyBattleAssignedMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleAssignedCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleAssigned(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleAssignedCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::BattleAssignedS2C& derived = static_cast<const ::BattleAssignedS2C&>(message);
    SendBattleClientPlayerNotifyBattleAssigned(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandlePlayerBattleCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case BattleClientPlayerSubmitBattleActionMessageId:
            AsyncCompleteGrpcBattleClientPlayerSubmitBattleAction(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerGetBattleStateMessageId:
            AsyncCompleteGrpcBattleClientPlayerGetBattleState(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifyBattleStartMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifyBattleStart(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifyTurnResultMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifyTurnResult(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifyBattleEndMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifyBattleEnd(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifyBattleReconnectMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifyBattleReconnect(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerStopWatchBattleMessageId:
            AsyncCompleteGrpcBattleClientPlayerStopWatchBattle(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerSetAutoBattleMessageId:
            AsyncCompleteGrpcBattleClientPlayerSetAutoBattle(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifySpectateStateMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifySpectateState(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifySpectateTurnResultMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifySpectateTurnResult(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifySpectateEndMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifySpectateEnd(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case BattleClientPlayerNotifyBattleAssignedMessageId:
            AsyncCompleteGrpcBattleClientPlayerNotifyBattleAssigned(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetPlayerBattleHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncBattleClientPlayerSubmitBattleActionHandler = handler;
    AsyncBattleClientPlayerGetBattleStateHandler = handler;
    AsyncBattleClientPlayerNotifyBattleStartHandler = handler;
    AsyncBattleClientPlayerNotifyTurnResultHandler = handler;
    AsyncBattleClientPlayerNotifyBattleEndHandler = handler;
    AsyncBattleClientPlayerNotifyBattleReconnectHandler = handler;
    AsyncBattleClientPlayerStopWatchBattleHandler = handler;
    AsyncBattleClientPlayerSetAutoBattleHandler = handler;
    AsyncBattleClientPlayerNotifySpectateStateHandler = handler;
    AsyncBattleClientPlayerNotifySpectateTurnResultHandler = handler;
    AsyncBattleClientPlayerNotifySpectateEndHandler = handler;
    AsyncBattleClientPlayerNotifyBattleAssignedHandler = handler;
}

void SetPlayerBattleIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncBattleClientPlayerSubmitBattleActionHandler) {
        AsyncBattleClientPlayerSubmitBattleActionHandler = handler;
    }
    if (!AsyncBattleClientPlayerGetBattleStateHandler) {
        AsyncBattleClientPlayerGetBattleStateHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleStartHandler) {
        AsyncBattleClientPlayerNotifyBattleStartHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyTurnResultHandler) {
        AsyncBattleClientPlayerNotifyTurnResultHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleEndHandler) {
        AsyncBattleClientPlayerNotifyBattleEndHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleReconnectHandler) {
        AsyncBattleClientPlayerNotifyBattleReconnectHandler = handler;
    }
    if (!AsyncBattleClientPlayerStopWatchBattleHandler) {
        AsyncBattleClientPlayerStopWatchBattleHandler = handler;
    }
    if (!AsyncBattleClientPlayerSetAutoBattleHandler) {
        AsyncBattleClientPlayerSetAutoBattleHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateStateHandler) {
        AsyncBattleClientPlayerNotifySpectateStateHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateTurnResultHandler) {
        AsyncBattleClientPlayerNotifySpectateTurnResultHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateEndHandler) {
        AsyncBattleClientPlayerNotifySpectateEndHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleAssignedHandler) {
        AsyncBattleClientPlayerNotifyBattleAssignedHandler = handler;
    }
}

void SetPlayerBattleFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncBattleClientPlayerSubmitBattleActionFailedHandler = handler;
    AsyncBattleClientPlayerGetBattleStateFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleStartFailedHandler = handler;
    AsyncBattleClientPlayerNotifyTurnResultFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleEndFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler = handler;
    AsyncBattleClientPlayerStopWatchBattleFailedHandler = handler;
    AsyncBattleClientPlayerSetAutoBattleFailedHandler = handler;
    AsyncBattleClientPlayerNotifySpectateStateFailedHandler = handler;
    AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler = handler;
    AsyncBattleClientPlayerNotifySpectateEndFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler = handler;
}

void SetPlayerBattleIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncBattleClientPlayerSubmitBattleActionFailedHandler) {
        AsyncBattleClientPlayerSubmitBattleActionFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerGetBattleStateFailedHandler) {
        AsyncBattleClientPlayerGetBattleStateFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleStartFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleStartFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyTurnResultFailedHandler) {
        AsyncBattleClientPlayerNotifyTurnResultFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleEndFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleEndFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerStopWatchBattleFailedHandler) {
        AsyncBattleClientPlayerStopWatchBattleFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerSetAutoBattleFailedHandler) {
        AsyncBattleClientPlayerSetAutoBattleFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateStateFailedHandler) {
        AsyncBattleClientPlayerNotifySpectateStateFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler) {
        AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateEndFailedHandler) {
        AsyncBattleClientPlayerNotifySpectateEndFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler = handler;
    }
}

void SetPlayerBattleCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitPlayerBattleGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<BattleClientPlayerStubPtr>(nodeEntity, BattleClientPlayer::NewStub(channel));

}
