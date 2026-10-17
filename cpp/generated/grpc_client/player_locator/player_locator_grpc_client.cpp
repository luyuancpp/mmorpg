#include "muduo/base/Logging.h"

#include "player_locator_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetPlayerLocatorCallDeadline 按目标节点类型写入
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

namespace playerlocator {
#pragma region PlayerLocatorSetLocation
AsyncPlayerLocatorSetLocationHandlerFunctionType AsyncPlayerLocatorSetLocationHandler;
AsyncPlayerLocatorSetLocationFailedHandlerFunctionType AsyncPlayerLocatorSetLocationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerLocatorSetLocationCall {
    ::playerlocator::PlayerLocation request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerLocatorSetLocationHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompletePlayerLocatorSetLocation(const PlayerLocatorSetLocationCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerLocatorSetLocationHandler) {
            AsyncPlayerLocatorSetLocationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerLocator.SetLocation reply dropped: AsyncPlayerLocatorSetLocationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerLocatorSetLocationFailedHandler) {
        const GrpcCallFailure failure{ PlayerLocatorSetLocationMessageId, "PlayerLocator.SetLocation", outcome, call.sentMetadata};
        AsyncPlayerLocatorSetLocationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerLocator.SetLocation failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerLocatorSetLocationFailure(std::shared_ptr<PlayerLocatorSetLocationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerLocatorSetLocation(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerLocatorSetLocationCall(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<PlayerLocatorSetLocationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerLocatorSetLocationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerLocatorSetLocationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerLocatorSetLocationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerLocatorSetLocation(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerLocator.SetLocation response";
            CompletePlayerLocatorSetLocation(*call, outcome, nullptr);
            return;
        }
        CompletePlayerLocatorSetLocation(*call, outcome, &reply);
    });
}

void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartPlayerLocatorSetLocationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerLocatorSetLocation(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerLocatorSetLocationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::playerlocator::PlayerLocation& derived = static_cast<const ::playerlocator::PlayerLocation&>(message);
    SendPlayerLocatorSetLocation(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorGetLocation
AsyncPlayerLocatorGetLocationHandlerFunctionType AsyncPlayerLocatorGetLocationHandler;
AsyncPlayerLocatorGetLocationFailedHandlerFunctionType AsyncPlayerLocatorGetLocationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerLocatorGetLocationCall {
    ::playerlocator::PlayerId request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerLocatorGetLocationHandler / FailedHandler。
    GrpcDone<::playerlocator::PlayerLocation> done;
};

void CompletePlayerLocatorGetLocation(const PlayerLocatorGetLocationCall& call, const GrpcCallOutcome& outcome,
                                                const ::playerlocator::PlayerLocation* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerLocatorGetLocationHandler) {
            AsyncPlayerLocatorGetLocationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerLocator.GetLocation reply dropped: AsyncPlayerLocatorGetLocationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerLocatorGetLocationFailedHandler) {
        const GrpcCallFailure failure{ PlayerLocatorGetLocationMessageId, "PlayerLocator.GetLocation", outcome, call.sentMetadata};
        AsyncPlayerLocatorGetLocationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerLocator.GetLocation failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerLocatorGetLocationFailure(std::shared_ptr<PlayerLocatorGetLocationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerLocatorGetLocation(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerLocatorGetLocationCall(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::playerlocator::PlayerLocation> done) {
    auto call = std::make_shared<PlayerLocatorGetLocationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerLocatorGetLocationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerLocatorGetLocationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerLocatorGetLocationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerLocatorGetLocation(*call, outcome, nullptr);
            return;
        }
        ::playerlocator::PlayerLocation reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerLocator.GetLocation response";
            CompletePlayerLocatorGetLocation(*call, outcome, nullptr);
            return;
        }
        CompletePlayerLocatorGetLocation(*call, outcome, &reply);
    });
}

void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request,
                                            GrpcDone<::playerlocator::PlayerLocation> done, const GrpcCallOptions& options) {
    StartPlayerLocatorGetLocationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerLocatorGetLocation(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerLocatorGetLocationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::playerlocator::PlayerId& derived = static_cast<const ::playerlocator::PlayerId&>(message);
    SendPlayerLocatorGetLocation(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorMarkOffline
AsyncPlayerLocatorMarkOfflineHandlerFunctionType AsyncPlayerLocatorMarkOfflineHandler;
AsyncPlayerLocatorMarkOfflineFailedHandlerFunctionType AsyncPlayerLocatorMarkOfflineFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerLocatorMarkOfflineCall {
    ::playerlocator::MarkOfflineRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerLocatorMarkOfflineHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompletePlayerLocatorMarkOffline(const PlayerLocatorMarkOfflineCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerLocatorMarkOfflineHandler) {
            AsyncPlayerLocatorMarkOfflineHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerLocator.MarkOffline reply dropped: AsyncPlayerLocatorMarkOfflineHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerLocatorMarkOfflineFailedHandler) {
        const GrpcCallFailure failure{ PlayerLocatorMarkOfflineMessageId, "PlayerLocator.MarkOffline", outcome, call.sentMetadata};
        AsyncPlayerLocatorMarkOfflineFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerLocator.MarkOffline failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerLocatorMarkOfflineFailure(std::shared_ptr<PlayerLocatorMarkOfflineCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerLocatorMarkOffline(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerLocatorMarkOfflineCall(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<PlayerLocatorMarkOfflineCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerLocatorMarkOfflineFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerLocatorMarkOfflineGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerLocatorMarkOfflineFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerLocatorMarkOffline(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerLocator.MarkOffline response";
            CompletePlayerLocatorMarkOffline(*call, outcome, nullptr);
            return;
        }
        CompletePlayerLocatorMarkOffline(*call, outcome, &reply);
    });
}

void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartPlayerLocatorMarkOfflineCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerLocatorMarkOffline(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::MarkOfflineRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerLocatorMarkOfflineCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::playerlocator::MarkOfflineRequest& derived = static_cast<const ::playerlocator::MarkOfflineRequest&>(message);
    SendPlayerLocatorMarkOffline(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorSetSession
AsyncPlayerLocatorSetSessionHandlerFunctionType AsyncPlayerLocatorSetSessionHandler;
AsyncPlayerLocatorSetSessionFailedHandlerFunctionType AsyncPlayerLocatorSetSessionFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerLocatorSetSessionCall {
    ::playerlocator::SetSessionRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerLocatorSetSessionHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompletePlayerLocatorSetSession(const PlayerLocatorSetSessionCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerLocatorSetSessionHandler) {
            AsyncPlayerLocatorSetSessionHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerLocator.SetSession reply dropped: AsyncPlayerLocatorSetSessionHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerLocatorSetSessionFailedHandler) {
        const GrpcCallFailure failure{ PlayerLocatorSetSessionMessageId, "PlayerLocator.SetSession", outcome, call.sentMetadata};
        AsyncPlayerLocatorSetSessionFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerLocator.SetSession failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerLocatorSetSessionFailure(std::shared_ptr<PlayerLocatorSetSessionCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerLocatorSetSession(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerLocatorSetSessionCall(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<PlayerLocatorSetSessionCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerLocatorSetSessionFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerLocatorSetSessionGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerLocatorSetSessionFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerLocatorSetSession(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerLocator.SetSession response";
            CompletePlayerLocatorSetSession(*call, outcome, nullptr);
            return;
        }
        CompletePlayerLocatorSetSession(*call, outcome, &reply);
    });
}

void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartPlayerLocatorSetSessionCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerLocatorSetSession(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerLocatorSetSessionCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerLocatorSetSession(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::playerlocator::SetSessionRequest& derived = static_cast<const ::playerlocator::SetSessionRequest&>(message);
    SendPlayerLocatorSetSession(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorGetSession
AsyncPlayerLocatorGetSessionHandlerFunctionType AsyncPlayerLocatorGetSessionHandler;
AsyncPlayerLocatorGetSessionFailedHandlerFunctionType AsyncPlayerLocatorGetSessionFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerLocatorGetSessionCall {
    ::playerlocator::GetSessionRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerLocatorGetSessionHandler / FailedHandler。
    GrpcDone<::playerlocator::GetSessionResponse> done;
};

void CompletePlayerLocatorGetSession(const PlayerLocatorGetSessionCall& call, const GrpcCallOutcome& outcome,
                                                const ::playerlocator::GetSessionResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerLocatorGetSessionHandler) {
            AsyncPlayerLocatorGetSessionHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerLocator.GetSession reply dropped: AsyncPlayerLocatorGetSessionHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerLocatorGetSessionFailedHandler) {
        const GrpcCallFailure failure{ PlayerLocatorGetSessionMessageId, "PlayerLocator.GetSession", outcome, call.sentMetadata};
        AsyncPlayerLocatorGetSessionFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerLocator.GetSession failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerLocatorGetSessionFailure(std::shared_ptr<PlayerLocatorGetSessionCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerLocatorGetSession(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerLocatorGetSessionCall(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::playerlocator::GetSessionResponse> done) {
    auto call = std::make_shared<PlayerLocatorGetSessionCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerLocatorGetSessionFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerLocatorGetSessionGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerLocatorGetSessionFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerLocatorGetSession(*call, outcome, nullptr);
            return;
        }
        ::playerlocator::GetSessionResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerLocator.GetSession response";
            CompletePlayerLocatorGetSession(*call, outcome, nullptr);
            return;
        }
        CompletePlayerLocatorGetSession(*call, outcome, &reply);
    });
}

void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request,
                                            GrpcDone<::playerlocator::GetSessionResponse> done, const GrpcCallOptions& options) {
    StartPlayerLocatorGetSessionCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerLocatorGetSession(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::GetSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerLocatorGetSessionCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerLocatorGetSession(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::playerlocator::GetSessionRequest& derived = static_cast<const ::playerlocator::GetSessionRequest&>(message);
    SendPlayerLocatorGetSession(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorSetDisconnecting
AsyncPlayerLocatorSetDisconnectingHandlerFunctionType AsyncPlayerLocatorSetDisconnectingHandler;
AsyncPlayerLocatorSetDisconnectingFailedHandlerFunctionType AsyncPlayerLocatorSetDisconnectingFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerLocatorSetDisconnectingCall {
    ::playerlocator::SetDisconnectingRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerLocatorSetDisconnectingHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompletePlayerLocatorSetDisconnecting(const PlayerLocatorSetDisconnectingCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerLocatorSetDisconnectingHandler) {
            AsyncPlayerLocatorSetDisconnectingHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerLocator.SetDisconnecting reply dropped: AsyncPlayerLocatorSetDisconnectingHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerLocatorSetDisconnectingFailedHandler) {
        const GrpcCallFailure failure{ PlayerLocatorSetDisconnectingMessageId, "PlayerLocator.SetDisconnecting", outcome, call.sentMetadata};
        AsyncPlayerLocatorSetDisconnectingFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerLocator.SetDisconnecting failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerLocatorSetDisconnectingFailure(std::shared_ptr<PlayerLocatorSetDisconnectingCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerLocatorSetDisconnecting(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerLocatorSetDisconnectingCall(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<PlayerLocatorSetDisconnectingCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerLocatorSetDisconnectingFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerLocatorSetDisconnectingGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerLocatorSetDisconnectingFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerLocatorSetDisconnecting(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerLocator.SetDisconnecting response";
            CompletePlayerLocatorSetDisconnecting(*call, outcome, nullptr);
            return;
        }
        CompletePlayerLocatorSetDisconnecting(*call, outcome, &reply);
    });
}

void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartPlayerLocatorSetDisconnectingCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerLocatorSetDisconnecting(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::SetDisconnectingRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerLocatorSetDisconnectingCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerLocatorSetDisconnecting(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::playerlocator::SetDisconnectingRequest& derived = static_cast<const ::playerlocator::SetDisconnectingRequest&>(message);
    SendPlayerLocatorSetDisconnecting(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerLocatorReconnect
AsyncPlayerLocatorReconnectHandlerFunctionType AsyncPlayerLocatorReconnectHandler;
AsyncPlayerLocatorReconnectFailedHandlerFunctionType AsyncPlayerLocatorReconnectFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerLocatorReconnectCall {
    ::playerlocator::ReconnectRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerLocatorReconnectHandler / FailedHandler。
    GrpcDone<::playerlocator::ReconnectResponse> done;
};

void CompletePlayerLocatorReconnect(const PlayerLocatorReconnectCall& call, const GrpcCallOutcome& outcome,
                                                const ::playerlocator::ReconnectResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerLocatorReconnectHandler) {
            AsyncPlayerLocatorReconnectHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerLocator.Reconnect reply dropped: AsyncPlayerLocatorReconnectHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerLocatorReconnectFailedHandler) {
        const GrpcCallFailure failure{ PlayerLocatorReconnectMessageId, "PlayerLocator.Reconnect", outcome, call.sentMetadata};
        AsyncPlayerLocatorReconnectFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerLocator.Reconnect failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerLocatorReconnectFailure(std::shared_ptr<PlayerLocatorReconnectCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerLocatorReconnect(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerLocatorReconnectCall(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::playerlocator::ReconnectResponse> done) {
    auto call = std::make_shared<PlayerLocatorReconnectCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerLocatorReconnectFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerLocatorReconnectGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerLocatorReconnectFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerLocatorReconnect(*call, outcome, nullptr);
            return;
        }
        ::playerlocator::ReconnectResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerLocator.Reconnect response";
            CompletePlayerLocatorReconnect(*call, outcome, nullptr);
            return;
        }
        CompletePlayerLocatorReconnect(*call, outcome, &reply);
    });
}

void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request,
                                            GrpcDone<::playerlocator::ReconnectResponse> done, const GrpcCallOptions& options) {
    StartPlayerLocatorReconnectCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerLocatorReconnect(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::ReconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerLocatorReconnectCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerLocatorReconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::playerlocator::ReconnectRequest& derived = static_cast<const ::playerlocator::ReconnectRequest&>(message);
    SendPlayerLocatorReconnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetPlayerLocatorHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncPlayerLocatorSetLocationHandler = handler;
    AsyncPlayerLocatorGetLocationHandler = handler;
    AsyncPlayerLocatorMarkOfflineHandler = handler;
    AsyncPlayerLocatorSetSessionHandler = handler;
    AsyncPlayerLocatorGetSessionHandler = handler;
    AsyncPlayerLocatorSetDisconnectingHandler = handler;
    AsyncPlayerLocatorReconnectHandler = handler;
    (void)handler;
}

void SetPlayerLocatorIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncPlayerLocatorSetLocationHandler) {
        AsyncPlayerLocatorSetLocationHandler = handler;
    }
    if (!AsyncPlayerLocatorGetLocationHandler) {
        AsyncPlayerLocatorGetLocationHandler = handler;
    }
    if (!AsyncPlayerLocatorMarkOfflineHandler) {
        AsyncPlayerLocatorMarkOfflineHandler = handler;
    }
    if (!AsyncPlayerLocatorSetSessionHandler) {
        AsyncPlayerLocatorSetSessionHandler = handler;
    }
    if (!AsyncPlayerLocatorGetSessionHandler) {
        AsyncPlayerLocatorGetSessionHandler = handler;
    }
    if (!AsyncPlayerLocatorSetDisconnectingHandler) {
        AsyncPlayerLocatorSetDisconnectingHandler = handler;
    }
    if (!AsyncPlayerLocatorReconnectHandler) {
        AsyncPlayerLocatorReconnectHandler = handler;
    }
    (void)handler;
}

void SetPlayerLocatorFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncPlayerLocatorSetLocationFailedHandler = handler;
    AsyncPlayerLocatorGetLocationFailedHandler = handler;
    AsyncPlayerLocatorMarkOfflineFailedHandler = handler;
    AsyncPlayerLocatorSetSessionFailedHandler = handler;
    AsyncPlayerLocatorGetSessionFailedHandler = handler;
    AsyncPlayerLocatorSetDisconnectingFailedHandler = handler;
    AsyncPlayerLocatorReconnectFailedHandler = handler;
    (void)handler;
}

void SetPlayerLocatorIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncPlayerLocatorSetLocationFailedHandler) {
        AsyncPlayerLocatorSetLocationFailedHandler = handler;
    }
    if (!AsyncPlayerLocatorGetLocationFailedHandler) {
        AsyncPlayerLocatorGetLocationFailedHandler = handler;
    }
    if (!AsyncPlayerLocatorMarkOfflineFailedHandler) {
        AsyncPlayerLocatorMarkOfflineFailedHandler = handler;
    }
    if (!AsyncPlayerLocatorSetSessionFailedHandler) {
        AsyncPlayerLocatorSetSessionFailedHandler = handler;
    }
    if (!AsyncPlayerLocatorGetSessionFailedHandler) {
        AsyncPlayerLocatorGetSessionFailedHandler = handler;
    }
    if (!AsyncPlayerLocatorSetDisconnectingFailedHandler) {
        AsyncPlayerLocatorSetDisconnectingFailedHandler = handler;
    }
    if (!AsyncPlayerLocatorReconnectFailedHandler) {
        AsyncPlayerLocatorReconnectFailedHandler = handler;
    }
    (void)handler;
}

void SetPlayerLocatorCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace playerlocator
