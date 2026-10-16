#include "muduo/base/Logging.h"

#include "player_inbox_grpc_client.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include "core/network/grpc_client_runtime.h"
#include "core/utils/encode/base64.h"
#include "muduo_grpc/channel.h"

namespace {
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetPlayerInboxCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::milliseconds DefaultCallDeadline() {
    return std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}

GrpcCallOutcome LocalFailure(uint32_t code, std::string message) {
    GrpcCallOutcome outcome;
    outcome.code = code;
    outcome.message = std::move(message);
    return outcome;
}
}

namespace player_inbox {
#pragma region PlayerInboxFetchItems
AsyncPlayerInboxFetchItemsHandlerFunctionType AsyncPlayerInboxFetchItemsHandler;
AsyncPlayerInboxFetchItemsFailedHandlerFunctionType AsyncPlayerInboxFetchItemsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxFetchItemsCall {
    ::player_inbox::FetchPlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxFetchItemsHandler / FailedHandler。
    GrpcDone<::player_inbox::FetchPlayerInboxResponse> done;
};

void CompletePlayerInboxFetchItems(const PlayerInboxFetchItemsCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::FetchPlayerInboxResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxFetchItemsHandler) {
            AsyncPlayerInboxFetchItemsHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxFetchItemsMessageId, "PlayerInbox.FetchItems", outcome, call.sentMetadata};
        AsyncPlayerInboxFetchItemsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInbox.FetchItems failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxFetchItemsFailure(std::shared_ptr<PlayerInboxFetchItemsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxFetchItems(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxFetchItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::FetchPlayerInboxResponse> done) {
    auto call = std::make_shared<PlayerInboxFetchItemsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxFetchItemsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxFetchItemsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxFetchItemsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    rpc.metadata.reserve(call->sentMetadata.size());
    for (const auto& [key, value] : call->sentMetadata) {
        // 应用层这一层 Base64 不能省:线上字节与改前一致,Go 侧按它解码、按原样回显(§4.2)。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [call](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            CompletePlayerInboxFetchItems(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::FetchPlayerInboxResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInbox.FetchItems response";
            CompletePlayerInboxFetchItems(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxFetchItems(*call, outcome, &reply);
    });
}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::FetchPlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxFetchItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxFetchItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::FetchPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxFetchItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxFetchItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::FetchPlayerInboxRequest& derived = static_cast<const ::player_inbox::FetchPlayerInboxRequest&>(message);
    SendPlayerInboxFetchItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAckItems
AsyncPlayerInboxAckItemsHandlerFunctionType AsyncPlayerInboxAckItemsHandler;
AsyncPlayerInboxAckItemsFailedHandlerFunctionType AsyncPlayerInboxAckItemsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxAckItemsCall {
    ::player_inbox::AckPlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAckItemsHandler / FailedHandler。
    GrpcDone<::player_inbox::AckPlayerInboxResponse> done;
};

void CompletePlayerInboxAckItems(const PlayerInboxAckItemsCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::AckPlayerInboxResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxAckItemsHandler) {
            AsyncPlayerInboxAckItemsHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxAckItemsMessageId, "PlayerInbox.AckItems", outcome, call.sentMetadata};
        AsyncPlayerInboxAckItemsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInbox.AckItems failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxAckItemsFailure(std::shared_ptr<PlayerInboxAckItemsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxAckItems(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAckItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::AckPlayerInboxResponse> done) {
    auto call = std::make_shared<PlayerInboxAckItemsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxAckItemsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxAckItemsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxAckItemsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    rpc.metadata.reserve(call->sentMetadata.size());
    for (const auto& [key, value] : call->sentMetadata) {
        // 应用层这一层 Base64 不能省:线上字节与改前一致,Go 侧按它解码、按原样回显(§4.2)。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [call](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            CompletePlayerInboxAckItems(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::AckPlayerInboxResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInbox.AckItems response";
            CompletePlayerInboxAckItems(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxAckItems(*call, outcome, &reply);
    });
}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::AckPlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAckItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAckItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::AckPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAckItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxAckItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::AckPlayerInboxRequest& derived = static_cast<const ::player_inbox::AckPlayerInboxRequest&>(message);
    SendPlayerInboxAckItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetPlayerInboxHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncPlayerInboxFetchItemsHandler = handler;
    AsyncPlayerInboxAckItemsHandler = handler;
    (void)handler;
}

void SetPlayerInboxIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncPlayerInboxFetchItemsHandler) {
        AsyncPlayerInboxFetchItemsHandler = handler;
    }
    if (!AsyncPlayerInboxAckItemsHandler) {
        AsyncPlayerInboxAckItemsHandler = handler;
    }
    (void)handler;
}

void SetPlayerInboxFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncPlayerInboxFetchItemsFailedHandler = handler;
    AsyncPlayerInboxAckItemsFailedHandler = handler;
    (void)handler;
}

void SetPlayerInboxIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncPlayerInboxFetchItemsFailedHandler) {
        AsyncPlayerInboxFetchItemsFailedHandler = handler;
    }
    if (!AsyncPlayerInboxAckItemsFailedHandler) {
        AsyncPlayerInboxAckItemsFailedHandler = handler;
    }
    (void)handler;
}

void SetPlayerInboxCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace player_inbox
