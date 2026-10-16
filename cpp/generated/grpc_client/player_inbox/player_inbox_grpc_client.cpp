#include "muduo/base/Logging.h"

#include "player_inbox_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetPlayerInboxCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace player_inbox {
struct PlayerInboxCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region PlayerInboxFetchItems
boost::object_pool<AsyncPlayerInboxFetchItemsGrpcClient> PlayerInboxFetchItemsPool;
using AsyncPlayerInboxFetchItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::FetchPlayerInboxResponse&)>;
AsyncPlayerInboxFetchItemsHandlerFunctionType AsyncPlayerInboxFetchItemsHandler;
AsyncPlayerInboxFetchItemsFailedHandlerFunctionType AsyncPlayerInboxFetchItemsFailedHandler;

void CompletePlayerInboxFetchItems(AsyncPlayerInboxFetchItemsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxFetchItemsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxFetchItemsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxFetchItemsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxFetchItemsHandler) {
            AsyncPlayerInboxFetchItemsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInbox.FetchItems reply dropped: AsyncPlayerInboxFetchItemsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxFetchItemsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInbox.FetchItems", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxFetchItemsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInbox.FetchItems failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxFetchItems(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxFetchItemsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxFetchItems(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxFetchItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::FetchPlayerInboxResponse> done) {
    auto call(PlayerInboxFetchItemsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxFetchItems(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncFetchItems(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxFetchItemsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::FetchPlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxFetchItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxFetchItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxFetchItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::FetchPlayerInboxRequest& derived = static_cast<const ::player_inbox::FetchPlayerInboxRequest&>(message);
    SendPlayerInboxFetchItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAckItems
boost::object_pool<AsyncPlayerInboxAckItemsGrpcClient> PlayerInboxAckItemsPool;
using AsyncPlayerInboxAckItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::AckPlayerInboxResponse&)>;
AsyncPlayerInboxAckItemsHandlerFunctionType AsyncPlayerInboxAckItemsHandler;
AsyncPlayerInboxAckItemsFailedHandlerFunctionType AsyncPlayerInboxAckItemsFailedHandler;

void CompletePlayerInboxAckItems(AsyncPlayerInboxAckItemsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxAckItemsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxAckItemsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxAckItemsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxAckItemsHandler) {
            AsyncPlayerInboxAckItemsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInbox.AckItems reply dropped: AsyncPlayerInboxAckItemsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxAckItemsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInbox.AckItems", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxAckItemsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInbox.AckItems failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxAckItems(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxAckItemsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxAckItems(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAckItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::AckPlayerInboxResponse> done) {
    auto call(PlayerInboxAckItemsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxAckItems(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAckItems(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxAckItemsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::AckPlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAckItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAckItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAckItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::AckPlayerInboxRequest& derived = static_cast<const ::player_inbox::AckPlayerInboxRequest&>(message);
    SendPlayerInboxAckItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandlePlayerInboxCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case PlayerInboxFetchItemsMessageId:
            AsyncCompleteGrpcPlayerInboxFetchItems(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case PlayerInboxAckItemsMessageId:
            AsyncCompleteGrpcPlayerInboxAckItems(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetPlayerInboxHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncPlayerInboxFetchItemsHandler = handler;
    AsyncPlayerInboxAckItemsHandler = handler;
}

void SetPlayerInboxIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncPlayerInboxFetchItemsHandler) {
        AsyncPlayerInboxFetchItemsHandler = handler;
    }
    if (!AsyncPlayerInboxAckItemsHandler) {
        AsyncPlayerInboxAckItemsHandler = handler;
    }
}

void SetPlayerInboxFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncPlayerInboxFetchItemsFailedHandler = handler;
    AsyncPlayerInboxAckItemsFailedHandler = handler;
}

void SetPlayerInboxIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncPlayerInboxFetchItemsFailedHandler) {
        AsyncPlayerInboxFetchItemsFailedHandler = handler;
    }
    if (!AsyncPlayerInboxAckItemsFailedHandler) {
        AsyncPlayerInboxAckItemsFailedHandler = handler;
    }
}

void SetPlayerInboxCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitPlayerInboxGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<PlayerInboxStubPtr>(nodeEntity, PlayerInbox::NewStub(channel));

}

}// namespace player_inbox
