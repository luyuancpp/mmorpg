#include "muduo/base/Logging.h"

#include "friend_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetFriendCallDeadline 按目标节点类型写入
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

namespace friendpb {
#pragma region ClientPlayerFriendAddFriend
AsyncClientPlayerFriendAddFriendHandlerFunctionType AsyncClientPlayerFriendAddFriendHandler;
AsyncClientPlayerFriendAddFriendFailedHandlerFunctionType AsyncClientPlayerFriendAddFriendFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendAddFriendCall {
    ::friendpb::AddFriendRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendAddFriendHandler / FailedHandler。
    GrpcDone<::friendpb::AddFriendResponse> done;
};

void CompleteClientPlayerFriendAddFriend(const ClientPlayerFriendAddFriendCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::AddFriendResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendAddFriendHandler) {
            AsyncClientPlayerFriendAddFriendHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.AddFriend reply dropped: AsyncClientPlayerFriendAddFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendAddFriendFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendAddFriendMessageId, "ClientPlayerFriend.AddFriend", outcome, call.sentMetadata};
        AsyncClientPlayerFriendAddFriendFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.AddFriend failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendAddFriendFailure(std::shared_ptr<ClientPlayerFriendAddFriendCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendAddFriend(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendAddFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::AddFriendResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendAddFriendCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendAddFriendFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendAddFriendGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendAddFriendFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendAddFriend(*call, outcome, nullptr);
            return;
        }
        ::friendpb::AddFriendResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.AddFriend response";
            CompleteClientPlayerFriendAddFriend(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendAddFriend(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request,
                                            GrpcDone<::friendpb::AddFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendAddFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendAddFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendAddFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::AddFriendRequest& derived = static_cast<const ::friendpb::AddFriendRequest&>(message);
    SendClientPlayerFriendAddFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendAcceptFriend
AsyncClientPlayerFriendAcceptFriendHandlerFunctionType AsyncClientPlayerFriendAcceptFriendHandler;
AsyncClientPlayerFriendAcceptFriendFailedHandlerFunctionType AsyncClientPlayerFriendAcceptFriendFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendAcceptFriendCall {
    ::friendpb::AcceptFriendRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendAcceptFriendHandler / FailedHandler。
    GrpcDone<::friendpb::AcceptFriendResponse> done;
};

void CompleteClientPlayerFriendAcceptFriend(const ClientPlayerFriendAcceptFriendCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::AcceptFriendResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendAcceptFriendHandler) {
            AsyncClientPlayerFriendAcceptFriendHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.AcceptFriend reply dropped: AsyncClientPlayerFriendAcceptFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendAcceptFriendFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendAcceptFriendMessageId, "ClientPlayerFriend.AcceptFriend", outcome, call.sentMetadata};
        AsyncClientPlayerFriendAcceptFriendFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.AcceptFriend failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendAcceptFriendFailure(std::shared_ptr<ClientPlayerFriendAcceptFriendCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendAcceptFriend(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendAcceptFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::AcceptFriendResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendAcceptFriendCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendAcceptFriendFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendAcceptFriendGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendAcceptFriendFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendAcceptFriend(*call, outcome, nullptr);
            return;
        }
        ::friendpb::AcceptFriendResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.AcceptFriend response";
            CompleteClientPlayerFriendAcceptFriend(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendAcceptFriend(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request,
                                            GrpcDone<::friendpb::AcceptFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendAcceptFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendAcceptFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendAcceptFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::AcceptFriendRequest& derived = static_cast<const ::friendpb::AcceptFriendRequest&>(message);
    SendClientPlayerFriendAcceptFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendRejectFriend
AsyncClientPlayerFriendRejectFriendHandlerFunctionType AsyncClientPlayerFriendRejectFriendHandler;
AsyncClientPlayerFriendRejectFriendFailedHandlerFunctionType AsyncClientPlayerFriendRejectFriendFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendRejectFriendCall {
    ::friendpb::RejectFriendRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendRejectFriendHandler / FailedHandler。
    GrpcDone<::friendpb::RejectFriendResponse> done;
};

void CompleteClientPlayerFriendRejectFriend(const ClientPlayerFriendRejectFriendCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::RejectFriendResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendRejectFriendHandler) {
            AsyncClientPlayerFriendRejectFriendHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.RejectFriend reply dropped: AsyncClientPlayerFriendRejectFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendRejectFriendFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendRejectFriendMessageId, "ClientPlayerFriend.RejectFriend", outcome, call.sentMetadata};
        AsyncClientPlayerFriendRejectFriendFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.RejectFriend failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendRejectFriendFailure(std::shared_ptr<ClientPlayerFriendRejectFriendCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendRejectFriend(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendRejectFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::RejectFriendResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendRejectFriendCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendRejectFriendFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendRejectFriendGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendRejectFriendFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendRejectFriend(*call, outcome, nullptr);
            return;
        }
        ::friendpb::RejectFriendResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.RejectFriend response";
            CompleteClientPlayerFriendRejectFriend(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendRejectFriend(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request,
                                            GrpcDone<::friendpb::RejectFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendRejectFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendRejectFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendRejectFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::RejectFriendRequest& derived = static_cast<const ::friendpb::RejectFriendRequest&>(message);
    SendClientPlayerFriendRejectFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendRemoveFriend
AsyncClientPlayerFriendRemoveFriendHandlerFunctionType AsyncClientPlayerFriendRemoveFriendHandler;
AsyncClientPlayerFriendRemoveFriendFailedHandlerFunctionType AsyncClientPlayerFriendRemoveFriendFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendRemoveFriendCall {
    ::friendpb::RemoveFriendRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendRemoveFriendHandler / FailedHandler。
    GrpcDone<::friendpb::RemoveFriendResponse> done;
};

void CompleteClientPlayerFriendRemoveFriend(const ClientPlayerFriendRemoveFriendCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::RemoveFriendResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendRemoveFriendHandler) {
            AsyncClientPlayerFriendRemoveFriendHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.RemoveFriend reply dropped: AsyncClientPlayerFriendRemoveFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendRemoveFriendFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendRemoveFriendMessageId, "ClientPlayerFriend.RemoveFriend", outcome, call.sentMetadata};
        AsyncClientPlayerFriendRemoveFriendFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.RemoveFriend failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendRemoveFriendFailure(std::shared_ptr<ClientPlayerFriendRemoveFriendCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendRemoveFriend(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendRemoveFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::RemoveFriendResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendRemoveFriendCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendRemoveFriendFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendRemoveFriendGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendRemoveFriendFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendRemoveFriend(*call, outcome, nullptr);
            return;
        }
        ::friendpb::RemoveFriendResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.RemoveFriend response";
            CompleteClientPlayerFriendRemoveFriend(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendRemoveFriend(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request,
                                            GrpcDone<::friendpb::RemoveFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendRemoveFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendRemoveFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendRemoveFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::RemoveFriendRequest& derived = static_cast<const ::friendpb::RemoveFriendRequest&>(message);
    SendClientPlayerFriendRemoveFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendGetFriendList
AsyncClientPlayerFriendGetFriendListHandlerFunctionType AsyncClientPlayerFriendGetFriendListHandler;
AsyncClientPlayerFriendGetFriendListFailedHandlerFunctionType AsyncClientPlayerFriendGetFriendListFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendGetFriendListCall {
    ::friendpb::GetFriendListRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendGetFriendListHandler / FailedHandler。
    GrpcDone<::friendpb::GetFriendListResponse> done;
};

void CompleteClientPlayerFriendGetFriendList(const ClientPlayerFriendGetFriendListCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::GetFriendListResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendGetFriendListHandler) {
            AsyncClientPlayerFriendGetFriendListHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.GetFriendList reply dropped: AsyncClientPlayerFriendGetFriendListHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendGetFriendListFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendGetFriendListMessageId, "ClientPlayerFriend.GetFriendList", outcome, call.sentMetadata};
        AsyncClientPlayerFriendGetFriendListFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.GetFriendList failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendGetFriendListFailure(std::shared_ptr<ClientPlayerFriendGetFriendListCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendGetFriendList(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendGetFriendListCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::GetFriendListResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendGetFriendListCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendGetFriendListFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendGetFriendListGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendGetFriendListFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendGetFriendList(*call, outcome, nullptr);
            return;
        }
        ::friendpb::GetFriendListResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.GetFriendList response";
            CompleteClientPlayerFriendGetFriendList(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendGetFriendList(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request,
                                            GrpcDone<::friendpb::GetFriendListResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendGetFriendListCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendGetFriendList(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendGetFriendListCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::GetFriendListRequest& derived = static_cast<const ::friendpb::GetFriendListRequest&>(message);
    SendClientPlayerFriendGetFriendList(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendGetPendingRequests
AsyncClientPlayerFriendGetPendingRequestsHandlerFunctionType AsyncClientPlayerFriendGetPendingRequestsHandler;
AsyncClientPlayerFriendGetPendingRequestsFailedHandlerFunctionType AsyncClientPlayerFriendGetPendingRequestsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendGetPendingRequestsCall {
    ::friendpb::GetPendingRequestsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendGetPendingRequestsHandler / FailedHandler。
    GrpcDone<::friendpb::GetPendingRequestsResponse> done;
};

void CompleteClientPlayerFriendGetPendingRequests(const ClientPlayerFriendGetPendingRequestsCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::GetPendingRequestsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendGetPendingRequestsHandler) {
            AsyncClientPlayerFriendGetPendingRequestsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.GetPendingRequests reply dropped: AsyncClientPlayerFriendGetPendingRequestsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendGetPendingRequestsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendGetPendingRequestsMessageId, "ClientPlayerFriend.GetPendingRequests", outcome, call.sentMetadata};
        AsyncClientPlayerFriendGetPendingRequestsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.GetPendingRequests failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendGetPendingRequestsFailure(std::shared_ptr<ClientPlayerFriendGetPendingRequestsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendGetPendingRequests(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendGetPendingRequestsCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::GetPendingRequestsResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendGetPendingRequestsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendGetPendingRequestsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendGetPendingRequestsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendGetPendingRequestsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendGetPendingRequests(*call, outcome, nullptr);
            return;
        }
        ::friendpb::GetPendingRequestsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.GetPendingRequests response";
            CompleteClientPlayerFriendGetPendingRequests(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendGetPendingRequests(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request,
                                            GrpcDone<::friendpb::GetPendingRequestsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendGetPendingRequestsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendGetPendingRequests(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendGetPendingRequestsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::GetPendingRequestsRequest& derived = static_cast<const ::friendpb::GetPendingRequestsRequest&>(message);
    SendClientPlayerFriendGetPendingRequests(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendBlock
AsyncClientPlayerFriendBlockHandlerFunctionType AsyncClientPlayerFriendBlockHandler;
AsyncClientPlayerFriendBlockFailedHandlerFunctionType AsyncClientPlayerFriendBlockFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendBlockCall {
    ::friendpb::BlockRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendBlockHandler / FailedHandler。
    GrpcDone<::friendpb::BlockResponse> done;
};

void CompleteClientPlayerFriendBlock(const ClientPlayerFriendBlockCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::BlockResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendBlockHandler) {
            AsyncClientPlayerFriendBlockHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.Block reply dropped: AsyncClientPlayerFriendBlockHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendBlockFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendBlockMessageId, "ClientPlayerFriend.Block", outcome, call.sentMetadata};
        AsyncClientPlayerFriendBlockFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.Block failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendBlockFailure(std::shared_ptr<ClientPlayerFriendBlockCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendBlock(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendBlockCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::BlockResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendBlockCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendBlockFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendBlockGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendBlockFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendBlock(*call, outcome, nullptr);
            return;
        }
        ::friendpb::BlockResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.Block response";
            CompleteClientPlayerFriendBlock(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendBlock(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request,
                                            GrpcDone<::friendpb::BlockResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendBlockCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendBlock(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendBlockCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::BlockRequest& derived = static_cast<const ::friendpb::BlockRequest&>(message);
    SendClientPlayerFriendBlock(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendUnblock
AsyncClientPlayerFriendUnblockHandlerFunctionType AsyncClientPlayerFriendUnblockHandler;
AsyncClientPlayerFriendUnblockFailedHandlerFunctionType AsyncClientPlayerFriendUnblockFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendUnblockCall {
    ::friendpb::UnblockRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendUnblockHandler / FailedHandler。
    GrpcDone<::friendpb::UnblockResponse> done;
};

void CompleteClientPlayerFriendUnblock(const ClientPlayerFriendUnblockCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::UnblockResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendUnblockHandler) {
            AsyncClientPlayerFriendUnblockHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.Unblock reply dropped: AsyncClientPlayerFriendUnblockHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendUnblockFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendUnblockMessageId, "ClientPlayerFriend.Unblock", outcome, call.sentMetadata};
        AsyncClientPlayerFriendUnblockFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.Unblock failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendUnblockFailure(std::shared_ptr<ClientPlayerFriendUnblockCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendUnblock(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendUnblockCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::UnblockResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendUnblockCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendUnblockFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendUnblockGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendUnblockFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendUnblock(*call, outcome, nullptr);
            return;
        }
        ::friendpb::UnblockResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.Unblock response";
            CompleteClientPlayerFriendUnblock(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendUnblock(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request,
                                            GrpcDone<::friendpb::UnblockResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendUnblockCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendUnblock(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendUnblockCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::UnblockRequest& derived = static_cast<const ::friendpb::UnblockRequest&>(message);
    SendClientPlayerFriendUnblock(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendListBlocks
AsyncClientPlayerFriendListBlocksHandlerFunctionType AsyncClientPlayerFriendListBlocksHandler;
AsyncClientPlayerFriendListBlocksFailedHandlerFunctionType AsyncClientPlayerFriendListBlocksFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendListBlocksCall {
    ::friendpb::ListBlocksRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendListBlocksHandler / FailedHandler。
    GrpcDone<::friendpb::ListBlocksResponse> done;
};

void CompleteClientPlayerFriendListBlocks(const ClientPlayerFriendListBlocksCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::ListBlocksResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendListBlocksHandler) {
            AsyncClientPlayerFriendListBlocksHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.ListBlocks reply dropped: AsyncClientPlayerFriendListBlocksHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendListBlocksFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendListBlocksMessageId, "ClientPlayerFriend.ListBlocks", outcome, call.sentMetadata};
        AsyncClientPlayerFriendListBlocksFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.ListBlocks failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendListBlocksFailure(std::shared_ptr<ClientPlayerFriendListBlocksCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendListBlocks(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendListBlocksCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::ListBlocksResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendListBlocksCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendListBlocksFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendListBlocksGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendListBlocksFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendListBlocks(*call, outcome, nullptr);
            return;
        }
        ::friendpb::ListBlocksResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.ListBlocks response";
            CompleteClientPlayerFriendListBlocks(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendListBlocks(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request,
                                            GrpcDone<::friendpb::ListBlocksResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendListBlocksCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendListBlocks(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendListBlocksCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::ListBlocksRequest& derived = static_cast<const ::friendpb::ListBlocksRequest&>(message);
    SendClientPlayerFriendListBlocks(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendRecommendFriends
AsyncClientPlayerFriendRecommendFriendsHandlerFunctionType AsyncClientPlayerFriendRecommendFriendsHandler;
AsyncClientPlayerFriendRecommendFriendsFailedHandlerFunctionType AsyncClientPlayerFriendRecommendFriendsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendRecommendFriendsCall {
    ::friendpb::RecommendFriendsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendRecommendFriendsHandler / FailedHandler。
    GrpcDone<::friendpb::RecommendFriendsResponse> done;
};

void CompleteClientPlayerFriendRecommendFriends(const ClientPlayerFriendRecommendFriendsCall& call, const GrpcCallOutcome& outcome,
                                                const ::friendpb::RecommendFriendsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendRecommendFriendsHandler) {
            AsyncClientPlayerFriendRecommendFriendsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.RecommendFriends reply dropped: AsyncClientPlayerFriendRecommendFriendsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendRecommendFriendsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendRecommendFriendsMessageId, "ClientPlayerFriend.RecommendFriends", outcome, call.sentMetadata};
        AsyncClientPlayerFriendRecommendFriendsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.RecommendFriends failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendRecommendFriendsFailure(std::shared_ptr<ClientPlayerFriendRecommendFriendsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendRecommendFriends(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendRecommendFriendsCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::RecommendFriendsResponse> done) {
    auto call = std::make_shared<ClientPlayerFriendRecommendFriendsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendRecommendFriendsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendRecommendFriendsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendRecommendFriendsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendRecommendFriends(*call, outcome, nullptr);
            return;
        }
        ::friendpb::RecommendFriendsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.RecommendFriends response";
            CompleteClientPlayerFriendRecommendFriends(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendRecommendFriends(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request,
                                            GrpcDone<::friendpb::RecommendFriendsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendRecommendFriendsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendRecommendFriends(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendRecommendFriendsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::RecommendFriendsRequest& derived = static_cast<const ::friendpb::RecommendFriendsRequest&>(message);
    SendClientPlayerFriendRecommendFriends(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendNotifyFriendEvent
AsyncClientPlayerFriendNotifyFriendEventHandlerFunctionType AsyncClientPlayerFriendNotifyFriendEventHandler;
AsyncClientPlayerFriendNotifyFriendEventFailedHandlerFunctionType AsyncClientPlayerFriendNotifyFriendEventFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerFriendNotifyFriendEventCall {
    ::friendpb::FriendEventS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerFriendNotifyFriendEventHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteClientPlayerFriendNotifyFriendEvent(const ClientPlayerFriendNotifyFriendEventCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerFriendNotifyFriendEventHandler) {
            AsyncClientPlayerFriendNotifyFriendEventHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.NotifyFriendEvent reply dropped: AsyncClientPlayerFriendNotifyFriendEventHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendNotifyFriendEventFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerFriendNotifyFriendEventMessageId, "ClientPlayerFriend.NotifyFriendEvent", outcome, call.sentMetadata};
        AsyncClientPlayerFriendNotifyFriendEventFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.NotifyFriendEvent failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerFriendNotifyFriendEventFailure(std::shared_ptr<ClientPlayerFriendNotifyFriendEventCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerFriendNotifyFriendEvent(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendNotifyFriendEventCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<ClientPlayerFriendNotifyFriendEventCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerFriendNotifyFriendEventFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerFriendNotifyFriendEventGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerFriendNotifyFriendEventFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerFriendNotifyFriendEvent(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerFriend.NotifyFriendEvent response";
            CompleteClientPlayerFriendNotifyFriendEvent(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerFriendNotifyFriendEvent(*call, outcome, &reply);
    });
}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendNotifyFriendEventCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendNotifyFriendEvent(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendNotifyFriendEventCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::friendpb::FriendEventS2C& derived = static_cast<const ::friendpb::FriendEventS2C&>(message);
    SendClientPlayerFriendNotifyFriendEvent(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetFriendHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerFriendAddFriendHandler = handler;
    AsyncClientPlayerFriendAcceptFriendHandler = handler;
    AsyncClientPlayerFriendRejectFriendHandler = handler;
    AsyncClientPlayerFriendRemoveFriendHandler = handler;
    AsyncClientPlayerFriendGetFriendListHandler = handler;
    AsyncClientPlayerFriendGetPendingRequestsHandler = handler;
    AsyncClientPlayerFriendBlockHandler = handler;
    AsyncClientPlayerFriendUnblockHandler = handler;
    AsyncClientPlayerFriendListBlocksHandler = handler;
    AsyncClientPlayerFriendRecommendFriendsHandler = handler;
    AsyncClientPlayerFriendNotifyFriendEventHandler = handler;
    (void)handler;
}

void SetFriendIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncClientPlayerFriendAddFriendHandler) {
        AsyncClientPlayerFriendAddFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendAcceptFriendHandler) {
        AsyncClientPlayerFriendAcceptFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendRejectFriendHandler) {
        AsyncClientPlayerFriendRejectFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendRemoveFriendHandler) {
        AsyncClientPlayerFriendRemoveFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetFriendListHandler) {
        AsyncClientPlayerFriendGetFriendListHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetPendingRequestsHandler) {
        AsyncClientPlayerFriendGetPendingRequestsHandler = handler;
    }
    if (!AsyncClientPlayerFriendBlockHandler) {
        AsyncClientPlayerFriendBlockHandler = handler;
    }
    if (!AsyncClientPlayerFriendUnblockHandler) {
        AsyncClientPlayerFriendUnblockHandler = handler;
    }
    if (!AsyncClientPlayerFriendListBlocksHandler) {
        AsyncClientPlayerFriendListBlocksHandler = handler;
    }
    if (!AsyncClientPlayerFriendRecommendFriendsHandler) {
        AsyncClientPlayerFriendRecommendFriendsHandler = handler;
    }
    if (!AsyncClientPlayerFriendNotifyFriendEventHandler) {
        AsyncClientPlayerFriendNotifyFriendEventHandler = handler;
    }
    (void)handler;
}

void SetFriendFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerFriendAddFriendFailedHandler = handler;
    AsyncClientPlayerFriendAcceptFriendFailedHandler = handler;
    AsyncClientPlayerFriendRejectFriendFailedHandler = handler;
    AsyncClientPlayerFriendRemoveFriendFailedHandler = handler;
    AsyncClientPlayerFriendGetFriendListFailedHandler = handler;
    AsyncClientPlayerFriendGetPendingRequestsFailedHandler = handler;
    AsyncClientPlayerFriendBlockFailedHandler = handler;
    AsyncClientPlayerFriendUnblockFailedHandler = handler;
    AsyncClientPlayerFriendListBlocksFailedHandler = handler;
    AsyncClientPlayerFriendRecommendFriendsFailedHandler = handler;
    AsyncClientPlayerFriendNotifyFriendEventFailedHandler = handler;
    (void)handler;
}

void SetFriendIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerFriendAddFriendFailedHandler) {
        AsyncClientPlayerFriendAddFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendAcceptFriendFailedHandler) {
        AsyncClientPlayerFriendAcceptFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendRejectFriendFailedHandler) {
        AsyncClientPlayerFriendRejectFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendRemoveFriendFailedHandler) {
        AsyncClientPlayerFriendRemoveFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetFriendListFailedHandler) {
        AsyncClientPlayerFriendGetFriendListFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetPendingRequestsFailedHandler) {
        AsyncClientPlayerFriendGetPendingRequestsFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendBlockFailedHandler) {
        AsyncClientPlayerFriendBlockFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendUnblockFailedHandler) {
        AsyncClientPlayerFriendUnblockFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendListBlocksFailedHandler) {
        AsyncClientPlayerFriendListBlocksFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendRecommendFriendsFailedHandler) {
        AsyncClientPlayerFriendRecommendFriendsFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendNotifyFriendEventFailedHandler) {
        AsyncClientPlayerFriendNotifyFriendEventFailedHandler = handler;
    }
    (void)handler;
}

void SetFriendCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace friendpb
