#include "muduo/base/Logging.h"

#include "login_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetLoginCallDeadline 按目标节点类型写入
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

namespace loginpb {
#pragma region ClientPlayerLoginLogin
AsyncClientPlayerLoginLoginHandlerFunctionType AsyncClientPlayerLoginLoginHandler;
AsyncClientPlayerLoginLoginFailedHandlerFunctionType AsyncClientPlayerLoginLoginFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerLoginLoginCall {
    ::loginpb::LoginRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerLoginLoginHandler / FailedHandler。
    GrpcDone<::loginpb::LoginResponse> done;
};

void CompleteClientPlayerLoginLogin(const ClientPlayerLoginLoginCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::LoginResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerLoginLoginHandler) {
            AsyncClientPlayerLoginLoginHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerLogin.Login reply dropped: AsyncClientPlayerLoginLoginHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerLoginLoginFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerLoginLoginMessageId, "ClientPlayerLogin.Login", outcome, call.sentMetadata};
        AsyncClientPlayerLoginLoginFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerLogin.Login failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerLoginLoginFailure(std::shared_ptr<ClientPlayerLoginLoginCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerLoginLogin(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerLoginLoginCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::LoginResponse> done) {
    auto call = std::make_shared<ClientPlayerLoginLoginCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerLoginLoginFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerLoginLoginGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerLoginLoginFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerLoginLogin(*call, outcome, nullptr);
            return;
        }
        ::loginpb::LoginResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerLogin.Login response";
            CompleteClientPlayerLoginLogin(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerLoginLogin(*call, outcome, &reply);
    });
}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request,
                                            GrpcDone<::loginpb::LoginResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerLoginLoginCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerLoginLogin(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerLoginLoginCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerLoginLogin(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::LoginRequest& derived = static_cast<const ::loginpb::LoginRequest&>(message);
    SendClientPlayerLoginLogin(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginCreatePlayer
AsyncClientPlayerLoginCreatePlayerHandlerFunctionType AsyncClientPlayerLoginCreatePlayerHandler;
AsyncClientPlayerLoginCreatePlayerFailedHandlerFunctionType AsyncClientPlayerLoginCreatePlayerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerLoginCreatePlayerCall {
    ::loginpb::CreatePlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerLoginCreatePlayerHandler / FailedHandler。
    GrpcDone<::loginpb::CreatePlayerResponse> done;
};

void CompleteClientPlayerLoginCreatePlayer(const ClientPlayerLoginCreatePlayerCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::CreatePlayerResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerLoginCreatePlayerHandler) {
            AsyncClientPlayerLoginCreatePlayerHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerLogin.CreatePlayer reply dropped: AsyncClientPlayerLoginCreatePlayerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerLoginCreatePlayerFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerLoginCreatePlayerMessageId, "ClientPlayerLogin.CreatePlayer", outcome, call.sentMetadata};
        AsyncClientPlayerLoginCreatePlayerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerLogin.CreatePlayer failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerLoginCreatePlayerFailure(std::shared_ptr<ClientPlayerLoginCreatePlayerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerLoginCreatePlayer(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerLoginCreatePlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::CreatePlayerResponse> done) {
    auto call = std::make_shared<ClientPlayerLoginCreatePlayerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerLoginCreatePlayerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerLoginCreatePlayerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerLoginCreatePlayerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerLoginCreatePlayer(*call, outcome, nullptr);
            return;
        }
        ::loginpb::CreatePlayerResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerLogin.CreatePlayer response";
            CompleteClientPlayerLoginCreatePlayer(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerLoginCreatePlayer(*call, outcome, &reply);
    });
}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request,
                                            GrpcDone<::loginpb::CreatePlayerResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerLoginCreatePlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerLoginCreatePlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::CreatePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerLoginCreatePlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerLoginCreatePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::CreatePlayerRequest& derived = static_cast<const ::loginpb::CreatePlayerRequest&>(message);
    SendClientPlayerLoginCreatePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginEnterGame
AsyncClientPlayerLoginEnterGameHandlerFunctionType AsyncClientPlayerLoginEnterGameHandler;
AsyncClientPlayerLoginEnterGameFailedHandlerFunctionType AsyncClientPlayerLoginEnterGameFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerLoginEnterGameCall {
    ::loginpb::EnterGameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerLoginEnterGameHandler / FailedHandler。
    GrpcDone<::loginpb::EnterGameResponse> done;
};

void CompleteClientPlayerLoginEnterGame(const ClientPlayerLoginEnterGameCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::EnterGameResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerLoginEnterGameHandler) {
            AsyncClientPlayerLoginEnterGameHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerLogin.EnterGame reply dropped: AsyncClientPlayerLoginEnterGameHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerLoginEnterGameFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerLoginEnterGameMessageId, "ClientPlayerLogin.EnterGame", outcome, call.sentMetadata};
        AsyncClientPlayerLoginEnterGameFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerLogin.EnterGame failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerLoginEnterGameFailure(std::shared_ptr<ClientPlayerLoginEnterGameCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerLoginEnterGame(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerLoginEnterGameCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::EnterGameResponse> done) {
    auto call = std::make_shared<ClientPlayerLoginEnterGameCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerLoginEnterGameFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerLoginEnterGameGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerLoginEnterGameFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerLoginEnterGame(*call, outcome, nullptr);
            return;
        }
        ::loginpb::EnterGameResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerLogin.EnterGame response";
            CompleteClientPlayerLoginEnterGame(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerLoginEnterGame(*call, outcome, &reply);
    });
}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request,
                                            GrpcDone<::loginpb::EnterGameResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerLoginEnterGameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerLoginEnterGame(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::EnterGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerLoginEnterGameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerLoginEnterGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::EnterGameRequest& derived = static_cast<const ::loginpb::EnterGameRequest&>(message);
    SendClientPlayerLoginEnterGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginLeaveGame
AsyncClientPlayerLoginLeaveGameHandlerFunctionType AsyncClientPlayerLoginLeaveGameHandler;
AsyncClientPlayerLoginLeaveGameFailedHandlerFunctionType AsyncClientPlayerLoginLeaveGameFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerLoginLeaveGameCall {
    ::loginpb::LeaveGameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerLoginLeaveGameHandler / FailedHandler。
    GrpcDone<::loginpb::LoginEmptyResponse> done;
};

void CompleteClientPlayerLoginLeaveGame(const ClientPlayerLoginLeaveGameCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::LoginEmptyResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerLoginLeaveGameHandler) {
            AsyncClientPlayerLoginLeaveGameHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerLogin.LeaveGame reply dropped: AsyncClientPlayerLoginLeaveGameHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerLoginLeaveGameFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerLoginLeaveGameMessageId, "ClientPlayerLogin.LeaveGame", outcome, call.sentMetadata};
        AsyncClientPlayerLoginLeaveGameFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerLogin.LeaveGame failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerLoginLeaveGameFailure(std::shared_ptr<ClientPlayerLoginLeaveGameCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerLoginLeaveGame(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerLoginLeaveGameCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::LoginEmptyResponse> done) {
    auto call = std::make_shared<ClientPlayerLoginLeaveGameCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerLoginLeaveGameFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerLoginLeaveGameGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerLoginLeaveGameFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerLoginLeaveGame(*call, outcome, nullptr);
            return;
        }
        ::loginpb::LoginEmptyResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerLogin.LeaveGame response";
            CompleteClientPlayerLoginLeaveGame(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerLoginLeaveGame(*call, outcome, &reply);
    });
}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request,
                                            GrpcDone<::loginpb::LoginEmptyResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerLoginLeaveGameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerLoginLeaveGame(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LeaveGameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerLoginLeaveGameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerLoginLeaveGame(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::LeaveGameRequest& derived = static_cast<const ::loginpb::LeaveGameRequest&>(message);
    SendClientPlayerLoginLeaveGame(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginDisconnect
AsyncClientPlayerLoginDisconnectHandlerFunctionType AsyncClientPlayerLoginDisconnectHandler;
AsyncClientPlayerLoginDisconnectFailedHandlerFunctionType AsyncClientPlayerLoginDisconnectFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerLoginDisconnectCall {
    ::loginpb::LoginNodeDisconnectRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerLoginDisconnectHandler / FailedHandler。
    GrpcDone<::loginpb::LoginEmptyResponse> done;
};

void CompleteClientPlayerLoginDisconnect(const ClientPlayerLoginDisconnectCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::LoginEmptyResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerLoginDisconnectHandler) {
            AsyncClientPlayerLoginDisconnectHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerLogin.Disconnect reply dropped: AsyncClientPlayerLoginDisconnectHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerLoginDisconnectFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerLoginDisconnectMessageId, "ClientPlayerLogin.Disconnect", outcome, call.sentMetadata};
        AsyncClientPlayerLoginDisconnectFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerLogin.Disconnect failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerLoginDisconnectFailure(std::shared_ptr<ClientPlayerLoginDisconnectCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerLoginDisconnect(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerLoginDisconnectCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::LoginEmptyResponse> done) {
    auto call = std::make_shared<ClientPlayerLoginDisconnectCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerLoginDisconnectFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerLoginDisconnectGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerLoginDisconnectFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerLoginDisconnect(*call, outcome, nullptr);
            return;
        }
        ::loginpb::LoginEmptyResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerLogin.Disconnect response";
            CompleteClientPlayerLoginDisconnect(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerLoginDisconnect(*call, outcome, &reply);
    });
}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request,
                                            GrpcDone<::loginpb::LoginEmptyResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerLoginDisconnectCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerLoginDisconnect(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::LoginNodeDisconnectRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerLoginDisconnectCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerLoginDisconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::LoginNodeDisconnectRequest& derived = static_cast<const ::loginpb::LoginNodeDisconnectRequest&>(message);
    SendClientPlayerLoginDisconnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerLoginRefreshToken
AsyncClientPlayerLoginRefreshTokenHandlerFunctionType AsyncClientPlayerLoginRefreshTokenHandler;
AsyncClientPlayerLoginRefreshTokenFailedHandlerFunctionType AsyncClientPlayerLoginRefreshTokenFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerLoginRefreshTokenCall {
    ::loginpb::RefreshTokenRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerLoginRefreshTokenHandler / FailedHandler。
    GrpcDone<::loginpb::RefreshTokenResponse> done;
};

void CompleteClientPlayerLoginRefreshToken(const ClientPlayerLoginRefreshTokenCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::RefreshTokenResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerLoginRefreshTokenHandler) {
            AsyncClientPlayerLoginRefreshTokenHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerLogin.RefreshToken reply dropped: AsyncClientPlayerLoginRefreshTokenHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerLoginRefreshTokenFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerLoginRefreshTokenMessageId, "ClientPlayerLogin.RefreshToken", outcome, call.sentMetadata};
        AsyncClientPlayerLoginRefreshTokenFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerLogin.RefreshToken failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerLoginRefreshTokenFailure(std::shared_ptr<ClientPlayerLoginRefreshTokenCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerLoginRefreshToken(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerLoginRefreshTokenCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RefreshTokenRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::RefreshTokenResponse> done) {
    auto call = std::make_shared<ClientPlayerLoginRefreshTokenCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerLoginRefreshTokenFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerLoginRefreshTokenGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerLoginRefreshTokenFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerLoginRefreshToken(*call, outcome, nullptr);
            return;
        }
        ::loginpb::RefreshTokenResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerLogin.RefreshToken response";
            CompleteClientPlayerLoginRefreshToken(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerLoginRefreshToken(*call, outcome, &reply);
    });
}

void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RefreshTokenRequest& request,
                                            GrpcDone<::loginpb::RefreshTokenResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerLoginRefreshTokenCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RefreshTokenRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerLoginRefreshToken(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RefreshTokenRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerLoginRefreshTokenCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerLoginRefreshToken(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::RefreshTokenRequest& derived = static_cast<const ::loginpb::RefreshTokenRequest&>(message);
    SendClientPlayerLoginRefreshToken(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LoginPreGateAssignGate
AsyncLoginPreGateAssignGateHandlerFunctionType AsyncLoginPreGateAssignGateHandler;
AsyncLoginPreGateAssignGateFailedHandlerFunctionType AsyncLoginPreGateAssignGateFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct LoginPreGateAssignGateCall {
    ::loginpb::AssignGateRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncLoginPreGateAssignGateHandler / FailedHandler。
    GrpcDone<::loginpb::AssignGateResponse> done;
};

void CompleteLoginPreGateAssignGate(const LoginPreGateAssignGateCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::AssignGateResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncLoginPreGateAssignGateHandler) {
            AsyncLoginPreGateAssignGateHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC LoginPreGate.AssignGate reply dropped: AsyncLoginPreGateAssignGateHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncLoginPreGateAssignGateFailedHandler) {
        const GrpcCallFailure failure{ LoginPreGateAssignGateMessageId, "LoginPreGate.AssignGate", outcome, call.sentMetadata};
        AsyncLoginPreGateAssignGateFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC LoginPreGate.AssignGate failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalLoginPreGateAssignGateFailure(std::shared_ptr<LoginPreGateAssignGateCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteLoginPreGateAssignGate(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLoginPreGateAssignGateCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::AssignGateRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::AssignGateResponse> done) {
    auto call = std::make_shared<LoginPreGateAssignGateCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalLoginPreGateAssignGateFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kLoginPreGateAssignGateGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalLoginPreGateAssignGateFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteLoginPreGateAssignGate(*call, outcome, nullptr);
            return;
        }
        ::loginpb::AssignGateResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the LoginPreGate.AssignGate response";
            CompleteLoginPreGateAssignGate(*call, outcome, nullptr);
            return;
        }
        CompleteLoginPreGateAssignGate(*call, outcome, &reply);
    });
}

void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::AssignGateRequest& request,
                                            GrpcDone<::loginpb::AssignGateResponse> done, const GrpcCallOptions& options) {
    StartLoginPreGateAssignGateCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::AssignGateRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLoginPreGateAssignGate(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::AssignGateRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLoginPreGateAssignGateCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendLoginPreGateAssignGate(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::AssignGateRequest& derived = static_cast<const ::loginpb::AssignGateRequest&>(message);
    SendLoginPreGateAssignGate(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LoginPreGateQueryQueueStatus
AsyncLoginPreGateQueryQueueStatusHandlerFunctionType AsyncLoginPreGateQueryQueueStatusHandler;
AsyncLoginPreGateQueryQueueStatusFailedHandlerFunctionType AsyncLoginPreGateQueryQueueStatusFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct LoginPreGateQueryQueueStatusCall {
    ::loginpb::QueryQueueStatusRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncLoginPreGateQueryQueueStatusHandler / FailedHandler。
    GrpcDone<::loginpb::QueryQueueStatusResponse> done;
};

void CompleteLoginPreGateQueryQueueStatus(const LoginPreGateQueryQueueStatusCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::QueryQueueStatusResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncLoginPreGateQueryQueueStatusHandler) {
            AsyncLoginPreGateQueryQueueStatusHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC LoginPreGate.QueryQueueStatus reply dropped: AsyncLoginPreGateQueryQueueStatusHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncLoginPreGateQueryQueueStatusFailedHandler) {
        const GrpcCallFailure failure{ LoginPreGateQueryQueueStatusMessageId, "LoginPreGate.QueryQueueStatus", outcome, call.sentMetadata};
        AsyncLoginPreGateQueryQueueStatusFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC LoginPreGate.QueryQueueStatus failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalLoginPreGateQueryQueueStatusFailure(std::shared_ptr<LoginPreGateQueryQueueStatusCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteLoginPreGateQueryQueueStatus(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLoginPreGateQueryQueueStatusCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::QueryQueueStatusRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::QueryQueueStatusResponse> done) {
    auto call = std::make_shared<LoginPreGateQueryQueueStatusCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalLoginPreGateQueryQueueStatusFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kLoginPreGateQueryQueueStatusGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalLoginPreGateQueryQueueStatusFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteLoginPreGateQueryQueueStatus(*call, outcome, nullptr);
            return;
        }
        ::loginpb::QueryQueueStatusResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the LoginPreGate.QueryQueueStatus response";
            CompleteLoginPreGateQueryQueueStatus(*call, outcome, nullptr);
            return;
        }
        CompleteLoginPreGateQueryQueueStatus(*call, outcome, &reply);
    });
}

void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::QueryQueueStatusRequest& request,
                                            GrpcDone<::loginpb::QueryQueueStatusResponse> done, const GrpcCallOptions& options) {
    StartLoginPreGateQueryQueueStatusCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::QueryQueueStatusRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLoginPreGateQueryQueueStatus(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::QueryQueueStatusRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLoginPreGateQueryQueueStatusCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendLoginPreGateQueryQueueStatus(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::QueryQueueStatusRequest& derived = static_cast<const ::loginpb::QueryQueueStatusRequest&>(message);
    SendLoginPreGateQueryQueueStatus(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LoginAdminRemovePlayersFromAccounts
AsyncLoginAdminRemovePlayersFromAccountsHandlerFunctionType AsyncLoginAdminRemovePlayersFromAccountsHandler;
AsyncLoginAdminRemovePlayersFromAccountsFailedHandlerFunctionType AsyncLoginAdminRemovePlayersFromAccountsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct LoginAdminRemovePlayersFromAccountsCall {
    ::loginpb::RemovePlayersFromAccountsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncLoginAdminRemovePlayersFromAccountsHandler / FailedHandler。
    GrpcDone<::loginpb::RemovePlayersFromAccountsResponse> done;
};

void CompleteLoginAdminRemovePlayersFromAccounts(const LoginAdminRemovePlayersFromAccountsCall& call, const GrpcCallOutcome& outcome,
                                                const ::loginpb::RemovePlayersFromAccountsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncLoginAdminRemovePlayersFromAccountsHandler) {
            AsyncLoginAdminRemovePlayersFromAccountsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC LoginAdmin.RemovePlayersFromAccounts reply dropped: AsyncLoginAdminRemovePlayersFromAccountsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncLoginAdminRemovePlayersFromAccountsFailedHandler) {
        const GrpcCallFailure failure{ LoginAdminRemovePlayersFromAccountsMessageId, "LoginAdmin.RemovePlayersFromAccounts", outcome, call.sentMetadata};
        AsyncLoginAdminRemovePlayersFromAccountsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC LoginAdmin.RemovePlayersFromAccounts failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalLoginAdminRemovePlayersFromAccountsFailure(std::shared_ptr<LoginAdminRemovePlayersFromAccountsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteLoginAdminRemovePlayersFromAccounts(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLoginAdminRemovePlayersFromAccountsCall(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RemovePlayersFromAccountsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::loginpb::RemovePlayersFromAccountsResponse> done) {
    auto call = std::make_shared<LoginAdminRemovePlayersFromAccountsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalLoginAdminRemovePlayersFromAccountsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kLoginAdminRemovePlayersFromAccountsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalLoginAdminRemovePlayersFromAccountsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteLoginAdminRemovePlayersFromAccounts(*call, outcome, nullptr);
            return;
        }
        ::loginpb::RemovePlayersFromAccountsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the LoginAdmin.RemovePlayersFromAccounts response";
            CompleteLoginAdminRemovePlayersFromAccounts(*call, outcome, nullptr);
            return;
        }
        CompleteLoginAdminRemovePlayersFromAccounts(*call, outcome, &reply);
    });
}

void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RemovePlayersFromAccountsRequest& request,
                                            GrpcDone<::loginpb::RemovePlayersFromAccountsResponse> done, const GrpcCallOptions& options) {
    StartLoginAdminRemovePlayersFromAccountsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RemovePlayersFromAccountsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLoginAdminRemovePlayersFromAccounts(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const ::loginpb::RemovePlayersFromAccountsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLoginAdminRemovePlayersFromAccountsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendLoginAdminRemovePlayersFromAccounts(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::loginpb::RemovePlayersFromAccountsRequest& derived = static_cast<const ::loginpb::RemovePlayersFromAccountsRequest&>(message);
    SendLoginAdminRemovePlayersFromAccounts(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetLoginHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerLoginLoginHandler = handler;
    AsyncClientPlayerLoginCreatePlayerHandler = handler;
    AsyncClientPlayerLoginEnterGameHandler = handler;
    AsyncClientPlayerLoginLeaveGameHandler = handler;
    AsyncClientPlayerLoginDisconnectHandler = handler;
    AsyncClientPlayerLoginRefreshTokenHandler = handler;
    AsyncLoginPreGateAssignGateHandler = handler;
    AsyncLoginPreGateQueryQueueStatusHandler = handler;
    AsyncLoginAdminRemovePlayersFromAccountsHandler = handler;
    (void)handler;
}

void SetLoginIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncClientPlayerLoginLoginHandler) {
        AsyncClientPlayerLoginLoginHandler = handler;
    }
    if (!AsyncClientPlayerLoginCreatePlayerHandler) {
        AsyncClientPlayerLoginCreatePlayerHandler = handler;
    }
    if (!AsyncClientPlayerLoginEnterGameHandler) {
        AsyncClientPlayerLoginEnterGameHandler = handler;
    }
    if (!AsyncClientPlayerLoginLeaveGameHandler) {
        AsyncClientPlayerLoginLeaveGameHandler = handler;
    }
    if (!AsyncClientPlayerLoginDisconnectHandler) {
        AsyncClientPlayerLoginDisconnectHandler = handler;
    }
    if (!AsyncClientPlayerLoginRefreshTokenHandler) {
        AsyncClientPlayerLoginRefreshTokenHandler = handler;
    }
    if (!AsyncLoginPreGateAssignGateHandler) {
        AsyncLoginPreGateAssignGateHandler = handler;
    }
    if (!AsyncLoginPreGateQueryQueueStatusHandler) {
        AsyncLoginPreGateQueryQueueStatusHandler = handler;
    }
    if (!AsyncLoginAdminRemovePlayersFromAccountsHandler) {
        AsyncLoginAdminRemovePlayersFromAccountsHandler = handler;
    }
    (void)handler;
}

void SetLoginFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerLoginLoginFailedHandler = handler;
    AsyncClientPlayerLoginCreatePlayerFailedHandler = handler;
    AsyncClientPlayerLoginEnterGameFailedHandler = handler;
    AsyncClientPlayerLoginLeaveGameFailedHandler = handler;
    AsyncClientPlayerLoginDisconnectFailedHandler = handler;
    AsyncClientPlayerLoginRefreshTokenFailedHandler = handler;
    AsyncLoginPreGateAssignGateFailedHandler = handler;
    AsyncLoginPreGateQueryQueueStatusFailedHandler = handler;
    AsyncLoginAdminRemovePlayersFromAccountsFailedHandler = handler;
    (void)handler;
}

void SetLoginIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerLoginLoginFailedHandler) {
        AsyncClientPlayerLoginLoginFailedHandler = handler;
    }
    if (!AsyncClientPlayerLoginCreatePlayerFailedHandler) {
        AsyncClientPlayerLoginCreatePlayerFailedHandler = handler;
    }
    if (!AsyncClientPlayerLoginEnterGameFailedHandler) {
        AsyncClientPlayerLoginEnterGameFailedHandler = handler;
    }
    if (!AsyncClientPlayerLoginLeaveGameFailedHandler) {
        AsyncClientPlayerLoginLeaveGameFailedHandler = handler;
    }
    if (!AsyncClientPlayerLoginDisconnectFailedHandler) {
        AsyncClientPlayerLoginDisconnectFailedHandler = handler;
    }
    if (!AsyncClientPlayerLoginRefreshTokenFailedHandler) {
        AsyncClientPlayerLoginRefreshTokenFailedHandler = handler;
    }
    if (!AsyncLoginPreGateAssignGateFailedHandler) {
        AsyncLoginPreGateAssignGateFailedHandler = handler;
    }
    if (!AsyncLoginPreGateQueryQueueStatusFailedHandler) {
        AsyncLoginPreGateQueryQueueStatusFailedHandler = handler;
    }
    if (!AsyncLoginAdminRemovePlayersFromAccountsFailedHandler) {
        AsyncLoginAdminRemovePlayersFromAccountsFailedHandler = handler;
    }
    (void)handler;
}

void SetLoginCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace loginpb
