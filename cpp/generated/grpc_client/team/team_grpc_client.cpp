#include "muduo/base/Logging.h"

#include "team_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetTeamCallDeadline 按目标节点类型写入
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

namespace teampb {
#pragma region ClientPlayerTeamCreateTeam
AsyncClientPlayerTeamCreateTeamHandlerFunctionType AsyncClientPlayerTeamCreateTeamHandler;
AsyncClientPlayerTeamCreateTeamFailedHandlerFunctionType AsyncClientPlayerTeamCreateTeamFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamCreateTeamCall {
    ::teampb::CreateTeamRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamCreateTeamHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamCreateTeam(const ClientPlayerTeamCreateTeamCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamCreateTeamHandler) {
            AsyncClientPlayerTeamCreateTeamHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.CreateTeam reply dropped: AsyncClientPlayerTeamCreateTeamHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamCreateTeamFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamCreateTeamMessageId, "ClientPlayerTeam.CreateTeam", outcome, call.sentMetadata};
        AsyncClientPlayerTeamCreateTeamFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.CreateTeam failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamCreateTeamFailure(std::shared_ptr<ClientPlayerTeamCreateTeamCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamCreateTeam(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamCreateTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamCreateTeamCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamCreateTeamFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamCreateTeamGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamCreateTeamFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamCreateTeam(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.CreateTeam response";
            CompleteClientPlayerTeamCreateTeam(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamCreateTeam(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamCreateTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamCreateTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamCreateTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::CreateTeamRequest& derived = static_cast<const ::teampb::CreateTeamRequest&>(message);
    SendClientPlayerTeamCreateTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamGetMyTeam
AsyncClientPlayerTeamGetMyTeamHandlerFunctionType AsyncClientPlayerTeamGetMyTeamHandler;
AsyncClientPlayerTeamGetMyTeamFailedHandlerFunctionType AsyncClientPlayerTeamGetMyTeamFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamGetMyTeamCall {
    ::teampb::GetMyTeamRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamGetMyTeamHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamGetMyTeam(const ClientPlayerTeamGetMyTeamCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamGetMyTeamHandler) {
            AsyncClientPlayerTeamGetMyTeamHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.GetMyTeam reply dropped: AsyncClientPlayerTeamGetMyTeamHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamGetMyTeamFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamGetMyTeamMessageId, "ClientPlayerTeam.GetMyTeam", outcome, call.sentMetadata};
        AsyncClientPlayerTeamGetMyTeamFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.GetMyTeam failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamGetMyTeamFailure(std::shared_ptr<ClientPlayerTeamGetMyTeamCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamGetMyTeam(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamGetMyTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamGetMyTeamCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamGetMyTeamFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamGetMyTeamGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamGetMyTeamFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamGetMyTeam(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.GetMyTeam response";
            CompleteClientPlayerTeamGetMyTeam(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamGetMyTeam(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamGetMyTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamGetMyTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamGetMyTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::GetMyTeamRequest& derived = static_cast<const ::teampb::GetMyTeamRequest&>(message);
    SendClientPlayerTeamGetMyTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamApplyJoinTeam
AsyncClientPlayerTeamApplyJoinTeamHandlerFunctionType AsyncClientPlayerTeamApplyJoinTeamHandler;
AsyncClientPlayerTeamApplyJoinTeamFailedHandlerFunctionType AsyncClientPlayerTeamApplyJoinTeamFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamApplyJoinTeamCall {
    ::teampb::ApplyJoinTeamRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamApplyJoinTeamHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamApplyJoinTeam(const ClientPlayerTeamApplyJoinTeamCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamApplyJoinTeamHandler) {
            AsyncClientPlayerTeamApplyJoinTeamHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.ApplyJoinTeam reply dropped: AsyncClientPlayerTeamApplyJoinTeamHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamApplyJoinTeamFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamApplyJoinTeamMessageId, "ClientPlayerTeam.ApplyJoinTeam", outcome, call.sentMetadata};
        AsyncClientPlayerTeamApplyJoinTeamFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.ApplyJoinTeam failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamApplyJoinTeamFailure(std::shared_ptr<ClientPlayerTeamApplyJoinTeamCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamApplyJoinTeam(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamApplyJoinTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamApplyJoinTeamCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamApplyJoinTeamFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamApplyJoinTeamGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamApplyJoinTeamFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamApplyJoinTeam(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.ApplyJoinTeam response";
            CompleteClientPlayerTeamApplyJoinTeam(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamApplyJoinTeam(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamApplyJoinTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamApplyJoinTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamApplyJoinTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::ApplyJoinTeamRequest& derived = static_cast<const ::teampb::ApplyJoinTeamRequest&>(message);
    SendClientPlayerTeamApplyJoinTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamHandleApplication
AsyncClientPlayerTeamHandleApplicationHandlerFunctionType AsyncClientPlayerTeamHandleApplicationHandler;
AsyncClientPlayerTeamHandleApplicationFailedHandlerFunctionType AsyncClientPlayerTeamHandleApplicationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamHandleApplicationCall {
    ::teampb::HandleApplicationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamHandleApplicationHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamHandleApplication(const ClientPlayerTeamHandleApplicationCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamHandleApplicationHandler) {
            AsyncClientPlayerTeamHandleApplicationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.HandleApplication reply dropped: AsyncClientPlayerTeamHandleApplicationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamHandleApplicationFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamHandleApplicationMessageId, "ClientPlayerTeam.HandleApplication", outcome, call.sentMetadata};
        AsyncClientPlayerTeamHandleApplicationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.HandleApplication failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamHandleApplicationFailure(std::shared_ptr<ClientPlayerTeamHandleApplicationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamHandleApplication(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamHandleApplicationCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamHandleApplicationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamHandleApplicationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamHandleApplicationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamHandleApplicationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamHandleApplication(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.HandleApplication response";
            CompleteClientPlayerTeamHandleApplication(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamHandleApplication(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamHandleApplicationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamHandleApplication(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamHandleApplicationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::HandleApplicationRequest& derived = static_cast<const ::teampb::HandleApplicationRequest&>(message);
    SendClientPlayerTeamHandleApplication(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamInviteToTeam
AsyncClientPlayerTeamInviteToTeamHandlerFunctionType AsyncClientPlayerTeamInviteToTeamHandler;
AsyncClientPlayerTeamInviteToTeamFailedHandlerFunctionType AsyncClientPlayerTeamInviteToTeamFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamInviteToTeamCall {
    ::teampb::InviteToTeamRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamInviteToTeamHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamInviteToTeam(const ClientPlayerTeamInviteToTeamCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamInviteToTeamHandler) {
            AsyncClientPlayerTeamInviteToTeamHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.InviteToTeam reply dropped: AsyncClientPlayerTeamInviteToTeamHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamInviteToTeamFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamInviteToTeamMessageId, "ClientPlayerTeam.InviteToTeam", outcome, call.sentMetadata};
        AsyncClientPlayerTeamInviteToTeamFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.InviteToTeam failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamInviteToTeamFailure(std::shared_ptr<ClientPlayerTeamInviteToTeamCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamInviteToTeam(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamInviteToTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamInviteToTeamCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamInviteToTeamFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamInviteToTeamGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamInviteToTeamFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamInviteToTeam(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.InviteToTeam response";
            CompleteClientPlayerTeamInviteToTeam(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamInviteToTeam(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamInviteToTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamInviteToTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamInviteToTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::InviteToTeamRequest& derived = static_cast<const ::teampb::InviteToTeamRequest&>(message);
    SendClientPlayerTeamInviteToTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamRespondInvite
AsyncClientPlayerTeamRespondInviteHandlerFunctionType AsyncClientPlayerTeamRespondInviteHandler;
AsyncClientPlayerTeamRespondInviteFailedHandlerFunctionType AsyncClientPlayerTeamRespondInviteFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamRespondInviteCall {
    ::teampb::RespondInviteRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamRespondInviteHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamRespondInvite(const ClientPlayerTeamRespondInviteCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamRespondInviteHandler) {
            AsyncClientPlayerTeamRespondInviteHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.RespondInvite reply dropped: AsyncClientPlayerTeamRespondInviteHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamRespondInviteFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamRespondInviteMessageId, "ClientPlayerTeam.RespondInvite", outcome, call.sentMetadata};
        AsyncClientPlayerTeamRespondInviteFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.RespondInvite failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamRespondInviteFailure(std::shared_ptr<ClientPlayerTeamRespondInviteCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamRespondInvite(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamRespondInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamRespondInviteCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamRespondInviteFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamRespondInviteGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamRespondInviteFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamRespondInvite(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.RespondInvite response";
            CompleteClientPlayerTeamRespondInvite(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamRespondInvite(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamRespondInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamRespondInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamRespondInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::RespondInviteRequest& derived = static_cast<const ::teampb::RespondInviteRequest&>(message);
    SendClientPlayerTeamRespondInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamListMyInvites
AsyncClientPlayerTeamListMyInvitesHandlerFunctionType AsyncClientPlayerTeamListMyInvitesHandler;
AsyncClientPlayerTeamListMyInvitesFailedHandlerFunctionType AsyncClientPlayerTeamListMyInvitesFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamListMyInvitesCall {
    ::teampb::ListMyInvitesRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamListMyInvitesHandler / FailedHandler。
    GrpcDone<::teampb::ListMyInvitesResponse> done;
};

void CompleteClientPlayerTeamListMyInvites(const ClientPlayerTeamListMyInvitesCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::ListMyInvitesResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamListMyInvitesHandler) {
            AsyncClientPlayerTeamListMyInvitesHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.ListMyInvites reply dropped: AsyncClientPlayerTeamListMyInvitesHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamListMyInvitesFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamListMyInvitesMessageId, "ClientPlayerTeam.ListMyInvites", outcome, call.sentMetadata};
        AsyncClientPlayerTeamListMyInvitesFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.ListMyInvites failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamListMyInvitesFailure(std::shared_ptr<ClientPlayerTeamListMyInvitesCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamListMyInvites(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamListMyInvitesCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::ListMyInvitesResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamListMyInvitesCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamListMyInvitesFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamListMyInvitesGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamListMyInvitesFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamListMyInvites(*call, outcome, nullptr);
            return;
        }
        ::teampb::ListMyInvitesResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.ListMyInvites response";
            CompleteClientPlayerTeamListMyInvites(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamListMyInvites(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request,
                                            GrpcDone<::teampb::ListMyInvitesResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamListMyInvitesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamListMyInvites(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamListMyInvitesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::ListMyInvitesRequest& derived = static_cast<const ::teampb::ListMyInvitesRequest&>(message);
    SendClientPlayerTeamListMyInvites(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamLeaveTeam
AsyncClientPlayerTeamLeaveTeamHandlerFunctionType AsyncClientPlayerTeamLeaveTeamHandler;
AsyncClientPlayerTeamLeaveTeamFailedHandlerFunctionType AsyncClientPlayerTeamLeaveTeamFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamLeaveTeamCall {
    ::teampb::LeaveTeamRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamLeaveTeamHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamLeaveTeam(const ClientPlayerTeamLeaveTeamCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamLeaveTeamHandler) {
            AsyncClientPlayerTeamLeaveTeamHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.LeaveTeam reply dropped: AsyncClientPlayerTeamLeaveTeamHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamLeaveTeamFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamLeaveTeamMessageId, "ClientPlayerTeam.LeaveTeam", outcome, call.sentMetadata};
        AsyncClientPlayerTeamLeaveTeamFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.LeaveTeam failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamLeaveTeamFailure(std::shared_ptr<ClientPlayerTeamLeaveTeamCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamLeaveTeam(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamLeaveTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamLeaveTeamCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamLeaveTeamFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamLeaveTeamGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamLeaveTeamFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamLeaveTeam(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.LeaveTeam response";
            CompleteClientPlayerTeamLeaveTeam(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamLeaveTeam(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamLeaveTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamLeaveTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamLeaveTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::LeaveTeamRequest& derived = static_cast<const ::teampb::LeaveTeamRequest&>(message);
    SendClientPlayerTeamLeaveTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamKickMember
AsyncClientPlayerTeamKickMemberHandlerFunctionType AsyncClientPlayerTeamKickMemberHandler;
AsyncClientPlayerTeamKickMemberFailedHandlerFunctionType AsyncClientPlayerTeamKickMemberFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamKickMemberCall {
    ::teampb::KickMemberRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamKickMemberHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamKickMember(const ClientPlayerTeamKickMemberCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamKickMemberHandler) {
            AsyncClientPlayerTeamKickMemberHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.KickMember reply dropped: AsyncClientPlayerTeamKickMemberHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamKickMemberFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamKickMemberMessageId, "ClientPlayerTeam.KickMember", outcome, call.sentMetadata};
        AsyncClientPlayerTeamKickMemberFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.KickMember failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamKickMemberFailure(std::shared_ptr<ClientPlayerTeamKickMemberCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamKickMember(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamKickMemberCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamKickMemberCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamKickMemberFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamKickMemberGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamKickMemberFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamKickMember(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.KickMember response";
            CompleteClientPlayerTeamKickMember(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamKickMember(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamKickMemberCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamKickMember(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamKickMemberCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::KickMemberRequest& derived = static_cast<const ::teampb::KickMemberRequest&>(message);
    SendClientPlayerTeamKickMember(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamTransferLeader
AsyncClientPlayerTeamTransferLeaderHandlerFunctionType AsyncClientPlayerTeamTransferLeaderHandler;
AsyncClientPlayerTeamTransferLeaderFailedHandlerFunctionType AsyncClientPlayerTeamTransferLeaderFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamTransferLeaderCall {
    ::teampb::TransferLeaderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamTransferLeaderHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamTransferLeader(const ClientPlayerTeamTransferLeaderCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamTransferLeaderHandler) {
            AsyncClientPlayerTeamTransferLeaderHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.TransferLeader reply dropped: AsyncClientPlayerTeamTransferLeaderHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamTransferLeaderFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamTransferLeaderMessageId, "ClientPlayerTeam.TransferLeader", outcome, call.sentMetadata};
        AsyncClientPlayerTeamTransferLeaderFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.TransferLeader failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamTransferLeaderFailure(std::shared_ptr<ClientPlayerTeamTransferLeaderCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamTransferLeader(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamTransferLeaderCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamTransferLeaderCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamTransferLeaderFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamTransferLeaderGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamTransferLeaderFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamTransferLeader(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.TransferLeader response";
            CompleteClientPlayerTeamTransferLeader(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamTransferLeader(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamTransferLeaderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamTransferLeader(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamTransferLeaderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::TransferLeaderRequest& derived = static_cast<const ::teampb::TransferLeaderRequest&>(message);
    SendClientPlayerTeamTransferLeader(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamDisbandTeam
AsyncClientPlayerTeamDisbandTeamHandlerFunctionType AsyncClientPlayerTeamDisbandTeamHandler;
AsyncClientPlayerTeamDisbandTeamFailedHandlerFunctionType AsyncClientPlayerTeamDisbandTeamFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamDisbandTeamCall {
    ::teampb::DisbandTeamRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamDisbandTeamHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamDisbandTeam(const ClientPlayerTeamDisbandTeamCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamDisbandTeamHandler) {
            AsyncClientPlayerTeamDisbandTeamHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.DisbandTeam reply dropped: AsyncClientPlayerTeamDisbandTeamHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamDisbandTeamFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamDisbandTeamMessageId, "ClientPlayerTeam.DisbandTeam", outcome, call.sentMetadata};
        AsyncClientPlayerTeamDisbandTeamFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.DisbandTeam failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamDisbandTeamFailure(std::shared_ptr<ClientPlayerTeamDisbandTeamCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamDisbandTeam(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamDisbandTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamDisbandTeamCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamDisbandTeamFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamDisbandTeamGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamDisbandTeamFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamDisbandTeam(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.DisbandTeam response";
            CompleteClientPlayerTeamDisbandTeam(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamDisbandTeam(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamDisbandTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamDisbandTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamDisbandTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::DisbandTeamRequest& derived = static_cast<const ::teampb::DisbandTeamRequest&>(message);
    SendClientPlayerTeamDisbandTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamStartTeamMatch
AsyncClientPlayerTeamStartTeamMatchHandlerFunctionType AsyncClientPlayerTeamStartTeamMatchHandler;
AsyncClientPlayerTeamStartTeamMatchFailedHandlerFunctionType AsyncClientPlayerTeamStartTeamMatchFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamStartTeamMatchCall {
    ::teampb::StartTeamMatchRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamStartTeamMatchHandler / FailedHandler。
    GrpcDone<::teampb::TeamResponse> done;
};

void CompleteClientPlayerTeamStartTeamMatch(const ClientPlayerTeamStartTeamMatchCall& call, const GrpcCallOutcome& outcome,
                                                const ::teampb::TeamResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamStartTeamMatchHandler) {
            AsyncClientPlayerTeamStartTeamMatchHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.StartTeamMatch reply dropped: AsyncClientPlayerTeamStartTeamMatchHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamStartTeamMatchFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamStartTeamMatchMessageId, "ClientPlayerTeam.StartTeamMatch", outcome, call.sentMetadata};
        AsyncClientPlayerTeamStartTeamMatchFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.StartTeamMatch failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamStartTeamMatchFailure(std::shared_ptr<ClientPlayerTeamStartTeamMatchCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamStartTeamMatch(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamStartTeamMatchCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call = std::make_shared<ClientPlayerTeamStartTeamMatchCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamStartTeamMatchFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamStartTeamMatchGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamStartTeamMatchFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamStartTeamMatch(*call, outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.StartTeamMatch response";
            CompleteClientPlayerTeamStartTeamMatch(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamStartTeamMatch(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamStartTeamMatchCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamStartTeamMatch(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamStartTeamMatchCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::StartTeamMatchRequest& derived = static_cast<const ::teampb::StartTeamMatchRequest&>(message);
    SendClientPlayerTeamStartTeamMatch(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamSnapshot
AsyncClientPlayerTeamNotifyTeamSnapshotHandlerFunctionType AsyncClientPlayerTeamNotifyTeamSnapshotHandler;
AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamNotifyTeamSnapshotCall {
    ::teampb::TeamSnapshotS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamNotifyTeamSnapshotHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteClientPlayerTeamNotifyTeamSnapshot(const ClientPlayerTeamNotifyTeamSnapshotCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamNotifyTeamSnapshotHandler) {
            AsyncClientPlayerTeamNotifyTeamSnapshotHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamSnapshot reply dropped: AsyncClientPlayerTeamNotifyTeamSnapshotHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamNotifyTeamSnapshotMessageId, "ClientPlayerTeam.NotifyTeamSnapshot", outcome, call.sentMetadata};
        AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamSnapshot failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamNotifyTeamSnapshotFailure(std::shared_ptr<ClientPlayerTeamNotifyTeamSnapshotCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamNotifyTeamSnapshot(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamNotifyTeamSnapshotCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<ClientPlayerTeamNotifyTeamSnapshotCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamNotifyTeamSnapshotFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamNotifyTeamSnapshotGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamNotifyTeamSnapshotFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamNotifyTeamSnapshot(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.NotifyTeamSnapshot response";
            CompleteClientPlayerTeamNotifyTeamSnapshot(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamNotifyTeamSnapshot(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamSnapshotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamNotifyTeamSnapshot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamNotifyTeamSnapshotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::TeamSnapshotS2C& derived = static_cast<const ::teampb::TeamSnapshotS2C&>(message);
    SendClientPlayerTeamNotifyTeamSnapshot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamInvite
AsyncClientPlayerTeamNotifyTeamInviteHandlerFunctionType AsyncClientPlayerTeamNotifyTeamInviteHandler;
AsyncClientPlayerTeamNotifyTeamInviteFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamInviteFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamNotifyTeamInviteCall {
    ::teampb::TeamInviteS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamNotifyTeamInviteHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteClientPlayerTeamNotifyTeamInvite(const ClientPlayerTeamNotifyTeamInviteCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamNotifyTeamInviteHandler) {
            AsyncClientPlayerTeamNotifyTeamInviteHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamInvite reply dropped: AsyncClientPlayerTeamNotifyTeamInviteHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamNotifyTeamInviteFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamNotifyTeamInviteMessageId, "ClientPlayerTeam.NotifyTeamInvite", outcome, call.sentMetadata};
        AsyncClientPlayerTeamNotifyTeamInviteFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamInvite failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamNotifyTeamInviteFailure(std::shared_ptr<ClientPlayerTeamNotifyTeamInviteCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamNotifyTeamInvite(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamNotifyTeamInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<ClientPlayerTeamNotifyTeamInviteCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamNotifyTeamInviteFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamNotifyTeamInviteGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamNotifyTeamInviteFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamNotifyTeamInvite(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.NotifyTeamInvite response";
            CompleteClientPlayerTeamNotifyTeamInvite(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamNotifyTeamInvite(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamNotifyTeamInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamNotifyTeamInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::TeamInviteS2C& derived = static_cast<const ::teampb::TeamInviteS2C&>(message);
    SendClientPlayerTeamNotifyTeamInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamEvent
AsyncClientPlayerTeamNotifyTeamEventHandlerFunctionType AsyncClientPlayerTeamNotifyTeamEventHandler;
AsyncClientPlayerTeamNotifyTeamEventFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamEventFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerTeamNotifyTeamEventCall {
    ::teampb::TeamEventS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerTeamNotifyTeamEventHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteClientPlayerTeamNotifyTeamEvent(const ClientPlayerTeamNotifyTeamEventCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerTeamNotifyTeamEventHandler) {
            AsyncClientPlayerTeamNotifyTeamEventHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamEvent reply dropped: AsyncClientPlayerTeamNotifyTeamEventHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerTeamNotifyTeamEventFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerTeamNotifyTeamEventMessageId, "ClientPlayerTeam.NotifyTeamEvent", outcome, call.sentMetadata};
        AsyncClientPlayerTeamNotifyTeamEventFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamEvent failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerTeamNotifyTeamEventFailure(std::shared_ptr<ClientPlayerTeamNotifyTeamEventCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerTeamNotifyTeamEvent(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamNotifyTeamEventCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<ClientPlayerTeamNotifyTeamEventCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamNotifyTeamEventFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamNotifyTeamEventGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamNotifyTeamEventFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerTeamNotifyTeamEvent(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.NotifyTeamEvent response";
            CompleteClientPlayerTeamNotifyTeamEvent(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerTeamNotifyTeamEvent(*call, outcome, &reply);
    });
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamEventCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamNotifyTeamEvent(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamNotifyTeamEventCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::teampb::TeamEventS2C& derived = static_cast<const ::teampb::TeamEventS2C&>(message);
    SendClientPlayerTeamNotifyTeamEvent(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetTeamHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerTeamCreateTeamHandler = handler;
    AsyncClientPlayerTeamGetMyTeamHandler = handler;
    AsyncClientPlayerTeamApplyJoinTeamHandler = handler;
    AsyncClientPlayerTeamHandleApplicationHandler = handler;
    AsyncClientPlayerTeamInviteToTeamHandler = handler;
    AsyncClientPlayerTeamRespondInviteHandler = handler;
    AsyncClientPlayerTeamListMyInvitesHandler = handler;
    AsyncClientPlayerTeamLeaveTeamHandler = handler;
    AsyncClientPlayerTeamKickMemberHandler = handler;
    AsyncClientPlayerTeamTransferLeaderHandler = handler;
    AsyncClientPlayerTeamDisbandTeamHandler = handler;
    AsyncClientPlayerTeamStartTeamMatchHandler = handler;
    AsyncClientPlayerTeamNotifyTeamSnapshotHandler = handler;
    AsyncClientPlayerTeamNotifyTeamInviteHandler = handler;
    AsyncClientPlayerTeamNotifyTeamEventHandler = handler;
    (void)handler;
}

void SetTeamIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncClientPlayerTeamCreateTeamHandler) {
        AsyncClientPlayerTeamCreateTeamHandler = handler;
    }
    if (!AsyncClientPlayerTeamGetMyTeamHandler) {
        AsyncClientPlayerTeamGetMyTeamHandler = handler;
    }
    if (!AsyncClientPlayerTeamApplyJoinTeamHandler) {
        AsyncClientPlayerTeamApplyJoinTeamHandler = handler;
    }
    if (!AsyncClientPlayerTeamHandleApplicationHandler) {
        AsyncClientPlayerTeamHandleApplicationHandler = handler;
    }
    if (!AsyncClientPlayerTeamInviteToTeamHandler) {
        AsyncClientPlayerTeamInviteToTeamHandler = handler;
    }
    if (!AsyncClientPlayerTeamRespondInviteHandler) {
        AsyncClientPlayerTeamRespondInviteHandler = handler;
    }
    if (!AsyncClientPlayerTeamListMyInvitesHandler) {
        AsyncClientPlayerTeamListMyInvitesHandler = handler;
    }
    if (!AsyncClientPlayerTeamLeaveTeamHandler) {
        AsyncClientPlayerTeamLeaveTeamHandler = handler;
    }
    if (!AsyncClientPlayerTeamKickMemberHandler) {
        AsyncClientPlayerTeamKickMemberHandler = handler;
    }
    if (!AsyncClientPlayerTeamTransferLeaderHandler) {
        AsyncClientPlayerTeamTransferLeaderHandler = handler;
    }
    if (!AsyncClientPlayerTeamDisbandTeamHandler) {
        AsyncClientPlayerTeamDisbandTeamHandler = handler;
    }
    if (!AsyncClientPlayerTeamStartTeamMatchHandler) {
        AsyncClientPlayerTeamStartTeamMatchHandler = handler;
    }
    if (!AsyncClientPlayerTeamNotifyTeamSnapshotHandler) {
        AsyncClientPlayerTeamNotifyTeamSnapshotHandler = handler;
    }
    if (!AsyncClientPlayerTeamNotifyTeamInviteHandler) {
        AsyncClientPlayerTeamNotifyTeamInviteHandler = handler;
    }
    if (!AsyncClientPlayerTeamNotifyTeamEventHandler) {
        AsyncClientPlayerTeamNotifyTeamEventHandler = handler;
    }
    (void)handler;
}

void SetTeamFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerTeamCreateTeamFailedHandler = handler;
    AsyncClientPlayerTeamGetMyTeamFailedHandler = handler;
    AsyncClientPlayerTeamApplyJoinTeamFailedHandler = handler;
    AsyncClientPlayerTeamHandleApplicationFailedHandler = handler;
    AsyncClientPlayerTeamInviteToTeamFailedHandler = handler;
    AsyncClientPlayerTeamRespondInviteFailedHandler = handler;
    AsyncClientPlayerTeamListMyInvitesFailedHandler = handler;
    AsyncClientPlayerTeamLeaveTeamFailedHandler = handler;
    AsyncClientPlayerTeamKickMemberFailedHandler = handler;
    AsyncClientPlayerTeamTransferLeaderFailedHandler = handler;
    AsyncClientPlayerTeamDisbandTeamFailedHandler = handler;
    AsyncClientPlayerTeamStartTeamMatchFailedHandler = handler;
    AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler = handler;
    AsyncClientPlayerTeamNotifyTeamInviteFailedHandler = handler;
    AsyncClientPlayerTeamNotifyTeamEventFailedHandler = handler;
    (void)handler;
}

void SetTeamIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerTeamCreateTeamFailedHandler) {
        AsyncClientPlayerTeamCreateTeamFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamGetMyTeamFailedHandler) {
        AsyncClientPlayerTeamGetMyTeamFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamApplyJoinTeamFailedHandler) {
        AsyncClientPlayerTeamApplyJoinTeamFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamHandleApplicationFailedHandler) {
        AsyncClientPlayerTeamHandleApplicationFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamInviteToTeamFailedHandler) {
        AsyncClientPlayerTeamInviteToTeamFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamRespondInviteFailedHandler) {
        AsyncClientPlayerTeamRespondInviteFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamListMyInvitesFailedHandler) {
        AsyncClientPlayerTeamListMyInvitesFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamLeaveTeamFailedHandler) {
        AsyncClientPlayerTeamLeaveTeamFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamKickMemberFailedHandler) {
        AsyncClientPlayerTeamKickMemberFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamTransferLeaderFailedHandler) {
        AsyncClientPlayerTeamTransferLeaderFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamDisbandTeamFailedHandler) {
        AsyncClientPlayerTeamDisbandTeamFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamStartTeamMatchFailedHandler) {
        AsyncClientPlayerTeamStartTeamMatchFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler) {
        AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamNotifyTeamInviteFailedHandler) {
        AsyncClientPlayerTeamNotifyTeamInviteFailedHandler = handler;
    }
    if (!AsyncClientPlayerTeamNotifyTeamEventFailedHandler) {
        AsyncClientPlayerTeamNotifyTeamEventFailedHandler = handler;
    }
    (void)handler;
}

void SetTeamCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace teampb
