#include "muduo/base/Logging.h"

#include "match_service_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetMatchServiceCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace match {
struct MatchServiceCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region MatchServiceJoinQueue
boost::object_pool<AsyncMatchServiceJoinQueueGrpcClient> MatchServiceJoinQueuePool;
using AsyncMatchServiceJoinQueueHandlerFunctionType =
    std::function<void(const ClientContext&, const ::match::JoinQueueResponse&)>;
AsyncMatchServiceJoinQueueHandlerFunctionType AsyncMatchServiceJoinQueueHandler;
AsyncMatchServiceJoinQueueFailedHandlerFunctionType AsyncMatchServiceJoinQueueFailedHandler;

void CompleteMatchServiceJoinQueue(AsyncMatchServiceJoinQueueGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceJoinQueueGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceJoinQueuePool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceJoinQueueGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceJoinQueueHandler) {
            AsyncMatchServiceJoinQueueHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.JoinQueue reply dropped: AsyncMatchServiceJoinQueueHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceJoinQueueFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.JoinQueue", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceJoinQueueFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.JoinQueue failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceJoinQueue(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceJoinQueueGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceJoinQueue(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceJoinQueueCall(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::JoinQueueResponse> done) {
    auto call(MatchServiceJoinQueuePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceJoinQueue(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncJoinQueue(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceJoinQueueMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request,
                                            GrpcDone<::match::JoinQueueResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceJoinQueueCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceJoinQueue(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceJoinQueueCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::JoinQueueRequest& derived = static_cast<const ::match::JoinQueueRequest&>(message);
    SendMatchServiceJoinQueue(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceCancelQueue
boost::object_pool<AsyncMatchServiceCancelQueueGrpcClient> MatchServiceCancelQueuePool;
using AsyncMatchServiceCancelQueueHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncMatchServiceCancelQueueHandlerFunctionType AsyncMatchServiceCancelQueueHandler;
AsyncMatchServiceCancelQueueFailedHandlerFunctionType AsyncMatchServiceCancelQueueFailedHandler;

void CompleteMatchServiceCancelQueue(AsyncMatchServiceCancelQueueGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceCancelQueueGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceCancelQueuePool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceCancelQueueGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceCancelQueueHandler) {
            AsyncMatchServiceCancelQueueHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.CancelQueue reply dropped: AsyncMatchServiceCancelQueueHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceCancelQueueFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.CancelQueue", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceCancelQueueFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.CancelQueue failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceCancelQueue(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceCancelQueueGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceCancelQueue(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceCancelQueueCall(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(MatchServiceCancelQueuePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceCancelQueue(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCancelQueue(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceCancelQueueMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartMatchServiceCancelQueueCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceCancelQueue(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceCancelQueueCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::CancelQueueRequest& derived = static_cast<const ::match::CancelQueueRequest&>(message);
    SendMatchServiceCancelQueue(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceGetQueueStatus
boost::object_pool<AsyncMatchServiceGetQueueStatusGrpcClient> MatchServiceGetQueueStatusPool;
using AsyncMatchServiceGetQueueStatusHandlerFunctionType =
    std::function<void(const ClientContext&, const ::match::GetQueueStatusResponse&)>;
AsyncMatchServiceGetQueueStatusHandlerFunctionType AsyncMatchServiceGetQueueStatusHandler;
AsyncMatchServiceGetQueueStatusFailedHandlerFunctionType AsyncMatchServiceGetQueueStatusFailedHandler;

void CompleteMatchServiceGetQueueStatus(AsyncMatchServiceGetQueueStatusGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceGetQueueStatusGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceGetQueueStatusPool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceGetQueueStatusGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceGetQueueStatusHandler) {
            AsyncMatchServiceGetQueueStatusHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.GetQueueStatus reply dropped: AsyncMatchServiceGetQueueStatusHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceGetQueueStatusFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.GetQueueStatus", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceGetQueueStatusFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.GetQueueStatus failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceGetQueueStatus(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceGetQueueStatusGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceGetQueueStatus(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceGetQueueStatusCall(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::GetQueueStatusResponse> done) {
    auto call(MatchServiceGetQueueStatusPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceGetQueueStatus(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetQueueStatus(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceGetQueueStatusMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request,
                                            GrpcDone<::match::GetQueueStatusResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceGetQueueStatusCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceGetQueueStatus(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceGetQueueStatusCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::GetQueueStatusRequest& derived = static_cast<const ::match::GetQueueStatusRequest&>(message);
    SendMatchServiceGetQueueStatus(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceChallengePlayer
boost::object_pool<AsyncMatchServiceChallengePlayerGrpcClient> MatchServiceChallengePlayerPool;
using AsyncMatchServiceChallengePlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::match::ChallengePlayerResponse&)>;
AsyncMatchServiceChallengePlayerHandlerFunctionType AsyncMatchServiceChallengePlayerHandler;
AsyncMatchServiceChallengePlayerFailedHandlerFunctionType AsyncMatchServiceChallengePlayerFailedHandler;

void CompleteMatchServiceChallengePlayer(AsyncMatchServiceChallengePlayerGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceChallengePlayerGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceChallengePlayerPool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceChallengePlayerGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceChallengePlayerHandler) {
            AsyncMatchServiceChallengePlayerHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.ChallengePlayer reply dropped: AsyncMatchServiceChallengePlayerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceChallengePlayerFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.ChallengePlayer", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceChallengePlayerFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.ChallengePlayer failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceChallengePlayer(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceChallengePlayerGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceChallengePlayer(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceChallengePlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::ChallengePlayerResponse> done) {
    auto call(MatchServiceChallengePlayerPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceChallengePlayer(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncChallengePlayer(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceChallengePlayerMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request,
                                            GrpcDone<::match::ChallengePlayerResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceChallengePlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceChallengePlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceChallengePlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::ChallengePlayerRequest& derived = static_cast<const ::match::ChallengePlayerRequest&>(message);
    SendMatchServiceChallengePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceRespondChallenge
boost::object_pool<AsyncMatchServiceRespondChallengeGrpcClient> MatchServiceRespondChallengePool;
using AsyncMatchServiceRespondChallengeHandlerFunctionType =
    std::function<void(const ClientContext&, const ::match::RespondChallengeResponse&)>;
AsyncMatchServiceRespondChallengeHandlerFunctionType AsyncMatchServiceRespondChallengeHandler;
AsyncMatchServiceRespondChallengeFailedHandlerFunctionType AsyncMatchServiceRespondChallengeFailedHandler;

void CompleteMatchServiceRespondChallenge(AsyncMatchServiceRespondChallengeGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceRespondChallengeGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceRespondChallengePool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceRespondChallengeGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceRespondChallengeHandler) {
            AsyncMatchServiceRespondChallengeHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.RespondChallenge reply dropped: AsyncMatchServiceRespondChallengeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceRespondChallengeFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.RespondChallenge", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceRespondChallengeFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.RespondChallenge failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceRespondChallenge(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceRespondChallengeGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceRespondChallenge(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceRespondChallengeCall(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::RespondChallengeResponse> done) {
    auto call(MatchServiceRespondChallengePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceRespondChallenge(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRespondChallenge(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceRespondChallengeMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request,
                                            GrpcDone<::match::RespondChallengeResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceRespondChallengeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceRespondChallenge(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceRespondChallengeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::RespondChallengeRequest& derived = static_cast<const ::match::RespondChallengeRequest&>(message);
    SendMatchServiceRespondChallenge(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceNotifyChallengeInvite
boost::object_pool<AsyncMatchServiceNotifyChallengeInviteGrpcClient> MatchServiceNotifyChallengeInvitePool;
using AsyncMatchServiceNotifyChallengeInviteHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncMatchServiceNotifyChallengeInviteHandlerFunctionType AsyncMatchServiceNotifyChallengeInviteHandler;
AsyncMatchServiceNotifyChallengeInviteFailedHandlerFunctionType AsyncMatchServiceNotifyChallengeInviteFailedHandler;

void CompleteMatchServiceNotifyChallengeInvite(AsyncMatchServiceNotifyChallengeInviteGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceNotifyChallengeInviteGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceNotifyChallengeInvitePool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceNotifyChallengeInviteGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceNotifyChallengeInviteHandler) {
            AsyncMatchServiceNotifyChallengeInviteHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.NotifyChallengeInvite reply dropped: AsyncMatchServiceNotifyChallengeInviteHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceNotifyChallengeInviteFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.NotifyChallengeInvite", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceNotifyChallengeInviteFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.NotifyChallengeInvite failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceNotifyChallengeInvite(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceNotifyChallengeInviteGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceNotifyChallengeInvite(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceNotifyChallengeInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(MatchServiceNotifyChallengeInvitePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceNotifyChallengeInvite(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyChallengeInvite(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceNotifyChallengeInviteMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartMatchServiceNotifyChallengeInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceNotifyChallengeInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceNotifyChallengeInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::ChallengeInviteS2C& derived = static_cast<const ::match::ChallengeInviteS2C&>(message);
    SendMatchServiceNotifyChallengeInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceNotifyChallengeResult
boost::object_pool<AsyncMatchServiceNotifyChallengeResultGrpcClient> MatchServiceNotifyChallengeResultPool;
using AsyncMatchServiceNotifyChallengeResultHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncMatchServiceNotifyChallengeResultHandlerFunctionType AsyncMatchServiceNotifyChallengeResultHandler;
AsyncMatchServiceNotifyChallengeResultFailedHandlerFunctionType AsyncMatchServiceNotifyChallengeResultFailedHandler;

void CompleteMatchServiceNotifyChallengeResult(AsyncMatchServiceNotifyChallengeResultGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceNotifyChallengeResultGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceNotifyChallengeResultPool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceNotifyChallengeResultGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceNotifyChallengeResultHandler) {
            AsyncMatchServiceNotifyChallengeResultHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.NotifyChallengeResult reply dropped: AsyncMatchServiceNotifyChallengeResultHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceNotifyChallengeResultFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.NotifyChallengeResult", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceNotifyChallengeResultFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.NotifyChallengeResult failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceNotifyChallengeResult(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceNotifyChallengeResultGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceNotifyChallengeResult(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceNotifyChallengeResultCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(MatchServiceNotifyChallengeResultPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceNotifyChallengeResult(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyChallengeResult(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceNotifyChallengeResultMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartMatchServiceNotifyChallengeResultCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceNotifyChallengeResult(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceNotifyChallengeResultCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::ChallengeResultS2C& derived = static_cast<const ::match::ChallengeResultS2C&>(message);
    SendMatchServiceNotifyChallengeResult(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceWatchBattle
boost::object_pool<AsyncMatchServiceWatchBattleGrpcClient> MatchServiceWatchBattlePool;
using AsyncMatchServiceWatchBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::match::WatchBattleResponse&)>;
AsyncMatchServiceWatchBattleHandlerFunctionType AsyncMatchServiceWatchBattleHandler;
AsyncMatchServiceWatchBattleFailedHandlerFunctionType AsyncMatchServiceWatchBattleFailedHandler;

void CompleteMatchServiceWatchBattle(AsyncMatchServiceWatchBattleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceWatchBattleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceWatchBattlePool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceWatchBattleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceWatchBattleHandler) {
            AsyncMatchServiceWatchBattleHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.WatchBattle reply dropped: AsyncMatchServiceWatchBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceWatchBattleFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.WatchBattle", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceWatchBattleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.WatchBattle failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceWatchBattle(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceWatchBattleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceWatchBattle(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceWatchBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::WatchBattleResponse> done) {
    auto call(MatchServiceWatchBattlePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceWatchBattle(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncWatchBattle(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceWatchBattleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request,
                                            GrpcDone<::match::WatchBattleResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceWatchBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceWatchBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceWatchBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::WatchBattleRequest& derived = static_cast<const ::match::WatchBattleRequest&>(message);
    SendMatchServiceWatchBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceRequestBattleTicket
boost::object_pool<AsyncMatchServiceRequestBattleTicketGrpcClient> MatchServiceRequestBattleTicketPool;
using AsyncMatchServiceRequestBattleTicketHandlerFunctionType =
    std::function<void(const ClientContext&, const ::RequestBattleTicketResponse&)>;
AsyncMatchServiceRequestBattleTicketHandlerFunctionType AsyncMatchServiceRequestBattleTicketHandler;
AsyncMatchServiceRequestBattleTicketFailedHandlerFunctionType AsyncMatchServiceRequestBattleTicketFailedHandler;

void CompleteMatchServiceRequestBattleTicket(AsyncMatchServiceRequestBattleTicketGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceRequestBattleTicketGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceRequestBattleTicketPool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceRequestBattleTicketGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceRequestBattleTicketHandler) {
            AsyncMatchServiceRequestBattleTicketHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.RequestBattleTicket reply dropped: AsyncMatchServiceRequestBattleTicketHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceRequestBattleTicketFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.RequestBattleTicket", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceRequestBattleTicketFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.RequestBattleTicket failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceRequestBattleTicket(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceRequestBattleTicketGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceRequestBattleTicket(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceRequestBattleTicketCall(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::RequestBattleTicketResponse> done) {
    auto call(MatchServiceRequestBattleTicketPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceRequestBattleTicket(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRequestBattleTicket(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceRequestBattleTicketMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request,
                                            GrpcDone<::RequestBattleTicketResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceRequestBattleTicketCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceRequestBattleTicket(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceRequestBattleTicketCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::RequestBattleTicketRequest& derived = static_cast<const ::RequestBattleTicketRequest&>(message);
    SendMatchServiceRequestBattleTicket(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceListWatchableBattles
boost::object_pool<AsyncMatchServiceListWatchableBattlesGrpcClient> MatchServiceListWatchableBattlesPool;
using AsyncMatchServiceListWatchableBattlesHandlerFunctionType =
    std::function<void(const ClientContext&, const ::match::ListWatchableBattlesResponse&)>;
AsyncMatchServiceListWatchableBattlesHandlerFunctionType AsyncMatchServiceListWatchableBattlesHandler;
AsyncMatchServiceListWatchableBattlesFailedHandlerFunctionType AsyncMatchServiceListWatchableBattlesFailedHandler;

void CompleteMatchServiceListWatchableBattles(AsyncMatchServiceListWatchableBattlesGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncMatchServiceListWatchableBattlesGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        MatchServiceListWatchableBattlesPool.destroy(completed);
    };
    const std::unique_ptr<AsyncMatchServiceListWatchableBattlesGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncMatchServiceListWatchableBattlesHandler) {
            AsyncMatchServiceListWatchableBattlesHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.ListWatchableBattles reply dropped: AsyncMatchServiceListWatchableBattlesHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceListWatchableBattlesFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "MatchService.ListWatchableBattles", call->context, call->status, call->sentMetadata};
        AsyncMatchServiceListWatchableBattlesFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC MatchService.ListWatchableBattles failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcMatchServiceListWatchableBattles(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncMatchServiceListWatchableBattlesGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteMatchServiceListWatchableBattles(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceListWatchableBattlesCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::ListWatchableBattlesResponse> done) {
    auto call(MatchServiceListWatchableBattlesPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<MatchServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteMatchServiceListWatchableBattles(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListWatchableBattles(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(MatchServiceListWatchableBattlesMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request,
                                            GrpcDone<::match::ListWatchableBattlesResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceListWatchableBattlesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceListWatchableBattles(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceListWatchableBattlesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::match::ListWatchableBattlesRequest& derived = static_cast<const ::match::ListWatchableBattlesRequest&>(message);
    SendMatchServiceListWatchableBattles(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleMatchServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case MatchServiceJoinQueueMessageId:
            AsyncCompleteGrpcMatchServiceJoinQueue(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceCancelQueueMessageId:
            AsyncCompleteGrpcMatchServiceCancelQueue(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceGetQueueStatusMessageId:
            AsyncCompleteGrpcMatchServiceGetQueueStatus(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceChallengePlayerMessageId:
            AsyncCompleteGrpcMatchServiceChallengePlayer(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceRespondChallengeMessageId:
            AsyncCompleteGrpcMatchServiceRespondChallenge(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceNotifyChallengeInviteMessageId:
            AsyncCompleteGrpcMatchServiceNotifyChallengeInvite(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceNotifyChallengeResultMessageId:
            AsyncCompleteGrpcMatchServiceNotifyChallengeResult(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceWatchBattleMessageId:
            AsyncCompleteGrpcMatchServiceWatchBattle(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceRequestBattleTicketMessageId:
            AsyncCompleteGrpcMatchServiceRequestBattleTicket(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case MatchServiceListWatchableBattlesMessageId:
            AsyncCompleteGrpcMatchServiceListWatchableBattles(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetMatchServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncMatchServiceJoinQueueHandler = handler;
    AsyncMatchServiceCancelQueueHandler = handler;
    AsyncMatchServiceGetQueueStatusHandler = handler;
    AsyncMatchServiceChallengePlayerHandler = handler;
    AsyncMatchServiceRespondChallengeHandler = handler;
    AsyncMatchServiceNotifyChallengeInviteHandler = handler;
    AsyncMatchServiceNotifyChallengeResultHandler = handler;
    AsyncMatchServiceWatchBattleHandler = handler;
    AsyncMatchServiceRequestBattleTicketHandler = handler;
    AsyncMatchServiceListWatchableBattlesHandler = handler;
}

void SetMatchServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncMatchServiceJoinQueueHandler) {
        AsyncMatchServiceJoinQueueHandler = handler;
    }
    if (!AsyncMatchServiceCancelQueueHandler) {
        AsyncMatchServiceCancelQueueHandler = handler;
    }
    if (!AsyncMatchServiceGetQueueStatusHandler) {
        AsyncMatchServiceGetQueueStatusHandler = handler;
    }
    if (!AsyncMatchServiceChallengePlayerHandler) {
        AsyncMatchServiceChallengePlayerHandler = handler;
    }
    if (!AsyncMatchServiceRespondChallengeHandler) {
        AsyncMatchServiceRespondChallengeHandler = handler;
    }
    if (!AsyncMatchServiceNotifyChallengeInviteHandler) {
        AsyncMatchServiceNotifyChallengeInviteHandler = handler;
    }
    if (!AsyncMatchServiceNotifyChallengeResultHandler) {
        AsyncMatchServiceNotifyChallengeResultHandler = handler;
    }
    if (!AsyncMatchServiceWatchBattleHandler) {
        AsyncMatchServiceWatchBattleHandler = handler;
    }
    if (!AsyncMatchServiceRequestBattleTicketHandler) {
        AsyncMatchServiceRequestBattleTicketHandler = handler;
    }
    if (!AsyncMatchServiceListWatchableBattlesHandler) {
        AsyncMatchServiceListWatchableBattlesHandler = handler;
    }
}

void SetMatchServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncMatchServiceJoinQueueFailedHandler = handler;
    AsyncMatchServiceCancelQueueFailedHandler = handler;
    AsyncMatchServiceGetQueueStatusFailedHandler = handler;
    AsyncMatchServiceChallengePlayerFailedHandler = handler;
    AsyncMatchServiceRespondChallengeFailedHandler = handler;
    AsyncMatchServiceNotifyChallengeInviteFailedHandler = handler;
    AsyncMatchServiceNotifyChallengeResultFailedHandler = handler;
    AsyncMatchServiceWatchBattleFailedHandler = handler;
    AsyncMatchServiceRequestBattleTicketFailedHandler = handler;
    AsyncMatchServiceListWatchableBattlesFailedHandler = handler;
}

void SetMatchServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncMatchServiceJoinQueueFailedHandler) {
        AsyncMatchServiceJoinQueueFailedHandler = handler;
    }
    if (!AsyncMatchServiceCancelQueueFailedHandler) {
        AsyncMatchServiceCancelQueueFailedHandler = handler;
    }
    if (!AsyncMatchServiceGetQueueStatusFailedHandler) {
        AsyncMatchServiceGetQueueStatusFailedHandler = handler;
    }
    if (!AsyncMatchServiceChallengePlayerFailedHandler) {
        AsyncMatchServiceChallengePlayerFailedHandler = handler;
    }
    if (!AsyncMatchServiceRespondChallengeFailedHandler) {
        AsyncMatchServiceRespondChallengeFailedHandler = handler;
    }
    if (!AsyncMatchServiceNotifyChallengeInviteFailedHandler) {
        AsyncMatchServiceNotifyChallengeInviteFailedHandler = handler;
    }
    if (!AsyncMatchServiceNotifyChallengeResultFailedHandler) {
        AsyncMatchServiceNotifyChallengeResultFailedHandler = handler;
    }
    if (!AsyncMatchServiceWatchBattleFailedHandler) {
        AsyncMatchServiceWatchBattleFailedHandler = handler;
    }
    if (!AsyncMatchServiceRequestBattleTicketFailedHandler) {
        AsyncMatchServiceRequestBattleTicketFailedHandler = handler;
    }
    if (!AsyncMatchServiceListWatchableBattlesFailedHandler) {
        AsyncMatchServiceListWatchableBattlesFailedHandler = handler;
    }
}

void SetMatchServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitMatchServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<MatchServiceStubPtr>(nodeEntity, MatchService::NewStub(channel));

}

}// namespace match
