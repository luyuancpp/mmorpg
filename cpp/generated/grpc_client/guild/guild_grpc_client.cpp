#include "muduo/base/Logging.h"

#include "guild_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetGuildCallDeadline 按目标节点类型写入
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

namespace guildpb {
#pragma region GuildServiceCreateGuild
AsyncGuildServiceCreateGuildHandlerFunctionType AsyncGuildServiceCreateGuildHandler;
AsyncGuildServiceCreateGuildFailedHandlerFunctionType AsyncGuildServiceCreateGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceCreateGuildCall {
    ::guildpb::CreateGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceCreateGuildHandler / FailedHandler。
    GrpcDone<::guildpb::CreateGuildResponse> done;
};

void CompleteGuildServiceCreateGuild(const GuildServiceCreateGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::CreateGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceCreateGuildHandler) {
            AsyncGuildServiceCreateGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.CreateGuild reply dropped: AsyncGuildServiceCreateGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceCreateGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceCreateGuildMessageId, "GuildService.CreateGuild", outcome, call.sentMetadata};
        AsyncGuildServiceCreateGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.CreateGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceCreateGuildFailure(std::shared_ptr<GuildServiceCreateGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceCreateGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceCreateGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::CreateGuildResponse> done) {
    auto call = std::make_shared<GuildServiceCreateGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceCreateGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceCreateGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceCreateGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceCreateGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::CreateGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.CreateGuild response";
            CompleteGuildServiceCreateGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceCreateGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request,
                                            GrpcDone<::guildpb::CreateGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceCreateGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceCreateGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceCreateGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::CreateGuildRequest& derived = static_cast<const ::guildpb::CreateGuildRequest&>(message);
    SendGuildServiceCreateGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuild
AsyncGuildServiceGetGuildHandlerFunctionType AsyncGuildServiceGetGuildHandler;
AsyncGuildServiceGetGuildFailedHandlerFunctionType AsyncGuildServiceGetGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceGetGuildCall {
    ::guildpb::GetGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildHandler / FailedHandler。
    GrpcDone<::guildpb::GetGuildResponse> done;
};

void CompleteGuildServiceGetGuild(const GuildServiceGetGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::GetGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceGetGuildHandler) {
            AsyncGuildServiceGetGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.GetGuild reply dropped: AsyncGuildServiceGetGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceGetGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceGetGuildMessageId, "GuildService.GetGuild", outcome, call.sentMetadata};
        AsyncGuildServiceGetGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceGetGuildFailure(std::shared_ptr<GuildServiceGetGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceGetGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildResponse> done) {
    auto call = std::make_shared<GuildServiceGetGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceGetGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceGetGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceGetGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceGetGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::GetGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.GetGuild response";
            CompleteGuildServiceGetGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceGetGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request,
                                            GrpcDone<::guildpb::GetGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GetGuildRequest& derived = static_cast<const ::guildpb::GetGuildRequest&>(message);
    SendGuildServiceGetGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetPlayerGuild
AsyncGuildServiceGetPlayerGuildHandlerFunctionType AsyncGuildServiceGetPlayerGuildHandler;
AsyncGuildServiceGetPlayerGuildFailedHandlerFunctionType AsyncGuildServiceGetPlayerGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceGetPlayerGuildCall {
    ::guildpb::GetPlayerGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetPlayerGuildHandler / FailedHandler。
    GrpcDone<::guildpb::GetPlayerGuildResponse> done;
};

void CompleteGuildServiceGetPlayerGuild(const GuildServiceGetPlayerGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::GetPlayerGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceGetPlayerGuildHandler) {
            AsyncGuildServiceGetPlayerGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.GetPlayerGuild reply dropped: AsyncGuildServiceGetPlayerGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceGetPlayerGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceGetPlayerGuildMessageId, "GuildService.GetPlayerGuild", outcome, call.sentMetadata};
        AsyncGuildServiceGetPlayerGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetPlayerGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceGetPlayerGuildFailure(std::shared_ptr<GuildServiceGetPlayerGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceGetPlayerGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetPlayerGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetPlayerGuildResponse> done) {
    auto call = std::make_shared<GuildServiceGetPlayerGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceGetPlayerGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceGetPlayerGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceGetPlayerGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceGetPlayerGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::GetPlayerGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.GetPlayerGuild response";
            CompleteGuildServiceGetPlayerGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceGetPlayerGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request,
                                            GrpcDone<::guildpb::GetPlayerGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetPlayerGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetPlayerGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetPlayerGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GetPlayerGuildRequest& derived = static_cast<const ::guildpb::GetPlayerGuildRequest&>(message);
    SendGuildServiceGetPlayerGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceLeaveGuild
AsyncGuildServiceLeaveGuildHandlerFunctionType AsyncGuildServiceLeaveGuildHandler;
AsyncGuildServiceLeaveGuildFailedHandlerFunctionType AsyncGuildServiceLeaveGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceLeaveGuildCall {
    ::guildpb::LeaveGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceLeaveGuildHandler / FailedHandler。
    GrpcDone<::guildpb::LeaveGuildResponse> done;
};

void CompleteGuildServiceLeaveGuild(const GuildServiceLeaveGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::LeaveGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceLeaveGuildHandler) {
            AsyncGuildServiceLeaveGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.LeaveGuild reply dropped: AsyncGuildServiceLeaveGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceLeaveGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceLeaveGuildMessageId, "GuildService.LeaveGuild", outcome, call.sentMetadata};
        AsyncGuildServiceLeaveGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.LeaveGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceLeaveGuildFailure(std::shared_ptr<GuildServiceLeaveGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceLeaveGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceLeaveGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::LeaveGuildResponse> done) {
    auto call = std::make_shared<GuildServiceLeaveGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceLeaveGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceLeaveGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceLeaveGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceLeaveGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::LeaveGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.LeaveGuild response";
            CompleteGuildServiceLeaveGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceLeaveGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request,
                                            GrpcDone<::guildpb::LeaveGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceLeaveGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceLeaveGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceLeaveGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::LeaveGuildRequest& derived = static_cast<const ::guildpb::LeaveGuildRequest&>(message);
    SendGuildServiceLeaveGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceDisbandGuild
AsyncGuildServiceDisbandGuildHandlerFunctionType AsyncGuildServiceDisbandGuildHandler;
AsyncGuildServiceDisbandGuildFailedHandlerFunctionType AsyncGuildServiceDisbandGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceDisbandGuildCall {
    ::guildpb::DisbandGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceDisbandGuildHandler / FailedHandler。
    GrpcDone<::guildpb::DisbandGuildResponse> done;
};

void CompleteGuildServiceDisbandGuild(const GuildServiceDisbandGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::DisbandGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceDisbandGuildHandler) {
            AsyncGuildServiceDisbandGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.DisbandGuild reply dropped: AsyncGuildServiceDisbandGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceDisbandGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceDisbandGuildMessageId, "GuildService.DisbandGuild", outcome, call.sentMetadata};
        AsyncGuildServiceDisbandGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.DisbandGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceDisbandGuildFailure(std::shared_ptr<GuildServiceDisbandGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceDisbandGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceDisbandGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::DisbandGuildResponse> done) {
    auto call = std::make_shared<GuildServiceDisbandGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceDisbandGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceDisbandGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceDisbandGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceDisbandGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::DisbandGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.DisbandGuild response";
            CompleteGuildServiceDisbandGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceDisbandGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request,
                                            GrpcDone<::guildpb::DisbandGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceDisbandGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceDisbandGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceDisbandGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::DisbandGuildRequest& derived = static_cast<const ::guildpb::DisbandGuildRequest&>(message);
    SendGuildServiceDisbandGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceSetAnnouncement
AsyncGuildServiceSetAnnouncementHandlerFunctionType AsyncGuildServiceSetAnnouncementHandler;
AsyncGuildServiceSetAnnouncementFailedHandlerFunctionType AsyncGuildServiceSetAnnouncementFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceSetAnnouncementCall {
    ::guildpb::SetAnnouncementRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceSetAnnouncementHandler / FailedHandler。
    GrpcDone<::guildpb::SetAnnouncementResponse> done;
};

void CompleteGuildServiceSetAnnouncement(const GuildServiceSetAnnouncementCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::SetAnnouncementResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceSetAnnouncementHandler) {
            AsyncGuildServiceSetAnnouncementHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.SetAnnouncement reply dropped: AsyncGuildServiceSetAnnouncementHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceSetAnnouncementFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceSetAnnouncementMessageId, "GuildService.SetAnnouncement", outcome, call.sentMetadata};
        AsyncGuildServiceSetAnnouncementFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.SetAnnouncement failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceSetAnnouncementFailure(std::shared_ptr<GuildServiceSetAnnouncementCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceSetAnnouncement(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceSetAnnouncementCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::SetAnnouncementResponse> done) {
    auto call = std::make_shared<GuildServiceSetAnnouncementCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceSetAnnouncementFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceSetAnnouncementGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceSetAnnouncementFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceSetAnnouncement(*call, outcome, nullptr);
            return;
        }
        ::guildpb::SetAnnouncementResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.SetAnnouncement response";
            CompleteGuildServiceSetAnnouncement(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceSetAnnouncement(*call, outcome, &reply);
    });
}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request,
                                            GrpcDone<::guildpb::SetAnnouncementResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceSetAnnouncementCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceSetAnnouncement(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceSetAnnouncementCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::SetAnnouncementRequest& derived = static_cast<const ::guildpb::SetAnnouncementRequest&>(message);
    SendGuildServiceSetAnnouncement(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceSetGuildMemberRole
AsyncGuildServiceSetGuildMemberRoleHandlerFunctionType AsyncGuildServiceSetGuildMemberRoleHandler;
AsyncGuildServiceSetGuildMemberRoleFailedHandlerFunctionType AsyncGuildServiceSetGuildMemberRoleFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceSetGuildMemberRoleCall {
    ::guildpb::SetGuildMemberRoleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceSetGuildMemberRoleHandler / FailedHandler。
    GrpcDone<::guildpb::SetGuildMemberRoleResponse> done;
};

void CompleteGuildServiceSetGuildMemberRole(const GuildServiceSetGuildMemberRoleCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::SetGuildMemberRoleResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceSetGuildMemberRoleHandler) {
            AsyncGuildServiceSetGuildMemberRoleHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.SetGuildMemberRole reply dropped: AsyncGuildServiceSetGuildMemberRoleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceSetGuildMemberRoleFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceSetGuildMemberRoleMessageId, "GuildService.SetGuildMemberRole", outcome, call.sentMetadata};
        AsyncGuildServiceSetGuildMemberRoleFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.SetGuildMemberRole failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceSetGuildMemberRoleFailure(std::shared_ptr<GuildServiceSetGuildMemberRoleCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceSetGuildMemberRole(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceSetGuildMemberRoleCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::SetGuildMemberRoleResponse> done) {
    auto call = std::make_shared<GuildServiceSetGuildMemberRoleCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceSetGuildMemberRoleFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceSetGuildMemberRoleGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceSetGuildMemberRoleFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceSetGuildMemberRole(*call, outcome, nullptr);
            return;
        }
        ::guildpb::SetGuildMemberRoleResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.SetGuildMemberRole response";
            CompleteGuildServiceSetGuildMemberRole(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceSetGuildMemberRole(*call, outcome, &reply);
    });
}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request,
                                            GrpcDone<::guildpb::SetGuildMemberRoleResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceSetGuildMemberRoleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceSetGuildMemberRole(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceSetGuildMemberRoleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::SetGuildMemberRoleRequest& derived = static_cast<const ::guildpb::SetGuildMemberRoleRequest&>(message);
    SendGuildServiceSetGuildMemberRole(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceKickGuildMember
AsyncGuildServiceKickGuildMemberHandlerFunctionType AsyncGuildServiceKickGuildMemberHandler;
AsyncGuildServiceKickGuildMemberFailedHandlerFunctionType AsyncGuildServiceKickGuildMemberFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceKickGuildMemberCall {
    ::guildpb::KickGuildMemberRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceKickGuildMemberHandler / FailedHandler。
    GrpcDone<::guildpb::KickGuildMemberResponse> done;
};

void CompleteGuildServiceKickGuildMember(const GuildServiceKickGuildMemberCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::KickGuildMemberResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceKickGuildMemberHandler) {
            AsyncGuildServiceKickGuildMemberHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.KickGuildMember reply dropped: AsyncGuildServiceKickGuildMemberHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceKickGuildMemberFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceKickGuildMemberMessageId, "GuildService.KickGuildMember", outcome, call.sentMetadata};
        AsyncGuildServiceKickGuildMemberFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.KickGuildMember failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceKickGuildMemberFailure(std::shared_ptr<GuildServiceKickGuildMemberCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceKickGuildMember(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceKickGuildMemberCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::KickGuildMemberResponse> done) {
    auto call = std::make_shared<GuildServiceKickGuildMemberCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceKickGuildMemberFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceKickGuildMemberGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceKickGuildMemberFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceKickGuildMember(*call, outcome, nullptr);
            return;
        }
        ::guildpb::KickGuildMemberResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.KickGuildMember response";
            CompleteGuildServiceKickGuildMember(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceKickGuildMember(*call, outcome, &reply);
    });
}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request,
                                            GrpcDone<::guildpb::KickGuildMemberResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceKickGuildMemberCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceKickGuildMember(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceKickGuildMemberCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::KickGuildMemberRequest& derived = static_cast<const ::guildpb::KickGuildMemberRequest&>(message);
    SendGuildServiceKickGuildMember(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceTransferGuildLeader
AsyncGuildServiceTransferGuildLeaderHandlerFunctionType AsyncGuildServiceTransferGuildLeaderHandler;
AsyncGuildServiceTransferGuildLeaderFailedHandlerFunctionType AsyncGuildServiceTransferGuildLeaderFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceTransferGuildLeaderCall {
    ::guildpb::TransferGuildLeaderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceTransferGuildLeaderHandler / FailedHandler。
    GrpcDone<::guildpb::TransferGuildLeaderResponse> done;
};

void CompleteGuildServiceTransferGuildLeader(const GuildServiceTransferGuildLeaderCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::TransferGuildLeaderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceTransferGuildLeaderHandler) {
            AsyncGuildServiceTransferGuildLeaderHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.TransferGuildLeader reply dropped: AsyncGuildServiceTransferGuildLeaderHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceTransferGuildLeaderFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceTransferGuildLeaderMessageId, "GuildService.TransferGuildLeader", outcome, call.sentMetadata};
        AsyncGuildServiceTransferGuildLeaderFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.TransferGuildLeader failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceTransferGuildLeaderFailure(std::shared_ptr<GuildServiceTransferGuildLeaderCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceTransferGuildLeader(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceTransferGuildLeaderCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::TransferGuildLeaderResponse> done) {
    auto call = std::make_shared<GuildServiceTransferGuildLeaderCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceTransferGuildLeaderFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceTransferGuildLeaderGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceTransferGuildLeaderFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceTransferGuildLeader(*call, outcome, nullptr);
            return;
        }
        ::guildpb::TransferGuildLeaderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.TransferGuildLeader response";
            CompleteGuildServiceTransferGuildLeader(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceTransferGuildLeader(*call, outcome, &reply);
    });
}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request,
                                            GrpcDone<::guildpb::TransferGuildLeaderResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceTransferGuildLeaderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceTransferGuildLeader(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceTransferGuildLeaderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::TransferGuildLeaderRequest& derived = static_cast<const ::guildpb::TransferGuildLeaderRequest&>(message);
    SendGuildServiceTransferGuildLeader(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceApplyJoinGuild
AsyncGuildServiceApplyJoinGuildHandlerFunctionType AsyncGuildServiceApplyJoinGuildHandler;
AsyncGuildServiceApplyJoinGuildFailedHandlerFunctionType AsyncGuildServiceApplyJoinGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceApplyJoinGuildCall {
    ::guildpb::ApplyJoinGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceApplyJoinGuildHandler / FailedHandler。
    GrpcDone<::guildpb::ApplyJoinGuildResponse> done;
};

void CompleteGuildServiceApplyJoinGuild(const GuildServiceApplyJoinGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::ApplyJoinGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceApplyJoinGuildHandler) {
            AsyncGuildServiceApplyJoinGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.ApplyJoinGuild reply dropped: AsyncGuildServiceApplyJoinGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceApplyJoinGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceApplyJoinGuildMessageId, "GuildService.ApplyJoinGuild", outcome, call.sentMetadata};
        AsyncGuildServiceApplyJoinGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.ApplyJoinGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceApplyJoinGuildFailure(std::shared_ptr<GuildServiceApplyJoinGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceApplyJoinGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceApplyJoinGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ApplyJoinGuildResponse> done) {
    auto call = std::make_shared<GuildServiceApplyJoinGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceApplyJoinGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceApplyJoinGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceApplyJoinGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceApplyJoinGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::ApplyJoinGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.ApplyJoinGuild response";
            CompleteGuildServiceApplyJoinGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceApplyJoinGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request,
                                            GrpcDone<::guildpb::ApplyJoinGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceApplyJoinGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceApplyJoinGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceApplyJoinGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::ApplyJoinGuildRequest& derived = static_cast<const ::guildpb::ApplyJoinGuildRequest&>(message);
    SendGuildServiceApplyJoinGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceCancelGuildApplication
AsyncGuildServiceCancelGuildApplicationHandlerFunctionType AsyncGuildServiceCancelGuildApplicationHandler;
AsyncGuildServiceCancelGuildApplicationFailedHandlerFunctionType AsyncGuildServiceCancelGuildApplicationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceCancelGuildApplicationCall {
    ::guildpb::CancelGuildApplicationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceCancelGuildApplicationHandler / FailedHandler。
    GrpcDone<::guildpb::CancelGuildApplicationResponse> done;
};

void CompleteGuildServiceCancelGuildApplication(const GuildServiceCancelGuildApplicationCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::CancelGuildApplicationResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceCancelGuildApplicationHandler) {
            AsyncGuildServiceCancelGuildApplicationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.CancelGuildApplication reply dropped: AsyncGuildServiceCancelGuildApplicationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceCancelGuildApplicationFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceCancelGuildApplicationMessageId, "GuildService.CancelGuildApplication", outcome, call.sentMetadata};
        AsyncGuildServiceCancelGuildApplicationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.CancelGuildApplication failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceCancelGuildApplicationFailure(std::shared_ptr<GuildServiceCancelGuildApplicationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceCancelGuildApplication(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceCancelGuildApplicationCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::CancelGuildApplicationResponse> done) {
    auto call = std::make_shared<GuildServiceCancelGuildApplicationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceCancelGuildApplicationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceCancelGuildApplicationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceCancelGuildApplicationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceCancelGuildApplication(*call, outcome, nullptr);
            return;
        }
        ::guildpb::CancelGuildApplicationResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.CancelGuildApplication response";
            CompleteGuildServiceCancelGuildApplication(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceCancelGuildApplication(*call, outcome, &reply);
    });
}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request,
                                            GrpcDone<::guildpb::CancelGuildApplicationResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceCancelGuildApplicationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceCancelGuildApplication(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceCancelGuildApplicationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::CancelGuildApplicationRequest& derived = static_cast<const ::guildpb::CancelGuildApplicationRequest&>(message);
    SendGuildServiceCancelGuildApplication(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceListMyGuildApplications
AsyncGuildServiceListMyGuildApplicationsHandlerFunctionType AsyncGuildServiceListMyGuildApplicationsHandler;
AsyncGuildServiceListMyGuildApplicationsFailedHandlerFunctionType AsyncGuildServiceListMyGuildApplicationsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceListMyGuildApplicationsCall {
    ::guildpb::ListMyGuildApplicationsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceListMyGuildApplicationsHandler / FailedHandler。
    GrpcDone<::guildpb::ListMyGuildApplicationsResponse> done;
};

void CompleteGuildServiceListMyGuildApplications(const GuildServiceListMyGuildApplicationsCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::ListMyGuildApplicationsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceListMyGuildApplicationsHandler) {
            AsyncGuildServiceListMyGuildApplicationsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.ListMyGuildApplications reply dropped: AsyncGuildServiceListMyGuildApplicationsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceListMyGuildApplicationsFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceListMyGuildApplicationsMessageId, "GuildService.ListMyGuildApplications", outcome, call.sentMetadata};
        AsyncGuildServiceListMyGuildApplicationsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.ListMyGuildApplications failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceListMyGuildApplicationsFailure(std::shared_ptr<GuildServiceListMyGuildApplicationsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceListMyGuildApplications(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceListMyGuildApplicationsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ListMyGuildApplicationsResponse> done) {
    auto call = std::make_shared<GuildServiceListMyGuildApplicationsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceListMyGuildApplicationsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceListMyGuildApplicationsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceListMyGuildApplicationsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceListMyGuildApplications(*call, outcome, nullptr);
            return;
        }
        ::guildpb::ListMyGuildApplicationsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.ListMyGuildApplications response";
            CompleteGuildServiceListMyGuildApplications(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceListMyGuildApplications(*call, outcome, &reply);
    });
}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request,
                                            GrpcDone<::guildpb::ListMyGuildApplicationsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceListMyGuildApplicationsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceListMyGuildApplications(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceListMyGuildApplicationsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::ListMyGuildApplicationsRequest& derived = static_cast<const ::guildpb::ListMyGuildApplicationsRequest&>(message);
    SendGuildServiceListMyGuildApplications(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceListGuildApplications
AsyncGuildServiceListGuildApplicationsHandlerFunctionType AsyncGuildServiceListGuildApplicationsHandler;
AsyncGuildServiceListGuildApplicationsFailedHandlerFunctionType AsyncGuildServiceListGuildApplicationsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceListGuildApplicationsCall {
    ::guildpb::ListGuildApplicationsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceListGuildApplicationsHandler / FailedHandler。
    GrpcDone<::guildpb::ListGuildApplicationsResponse> done;
};

void CompleteGuildServiceListGuildApplications(const GuildServiceListGuildApplicationsCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::ListGuildApplicationsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceListGuildApplicationsHandler) {
            AsyncGuildServiceListGuildApplicationsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.ListGuildApplications reply dropped: AsyncGuildServiceListGuildApplicationsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceListGuildApplicationsFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceListGuildApplicationsMessageId, "GuildService.ListGuildApplications", outcome, call.sentMetadata};
        AsyncGuildServiceListGuildApplicationsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.ListGuildApplications failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceListGuildApplicationsFailure(std::shared_ptr<GuildServiceListGuildApplicationsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceListGuildApplications(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceListGuildApplicationsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ListGuildApplicationsResponse> done) {
    auto call = std::make_shared<GuildServiceListGuildApplicationsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceListGuildApplicationsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceListGuildApplicationsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceListGuildApplicationsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceListGuildApplications(*call, outcome, nullptr);
            return;
        }
        ::guildpb::ListGuildApplicationsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.ListGuildApplications response";
            CompleteGuildServiceListGuildApplications(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceListGuildApplications(*call, outcome, &reply);
    });
}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request,
                                            GrpcDone<::guildpb::ListGuildApplicationsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceListGuildApplicationsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceListGuildApplications(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceListGuildApplicationsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::ListGuildApplicationsRequest& derived = static_cast<const ::guildpb::ListGuildApplicationsRequest&>(message);
    SendGuildServiceListGuildApplications(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceReviewGuildApplication
AsyncGuildServiceReviewGuildApplicationHandlerFunctionType AsyncGuildServiceReviewGuildApplicationHandler;
AsyncGuildServiceReviewGuildApplicationFailedHandlerFunctionType AsyncGuildServiceReviewGuildApplicationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceReviewGuildApplicationCall {
    ::guildpb::ReviewGuildApplicationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceReviewGuildApplicationHandler / FailedHandler。
    GrpcDone<::guildpb::ReviewGuildApplicationResponse> done;
};

void CompleteGuildServiceReviewGuildApplication(const GuildServiceReviewGuildApplicationCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::ReviewGuildApplicationResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceReviewGuildApplicationHandler) {
            AsyncGuildServiceReviewGuildApplicationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.ReviewGuildApplication reply dropped: AsyncGuildServiceReviewGuildApplicationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceReviewGuildApplicationFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceReviewGuildApplicationMessageId, "GuildService.ReviewGuildApplication", outcome, call.sentMetadata};
        AsyncGuildServiceReviewGuildApplicationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.ReviewGuildApplication failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceReviewGuildApplicationFailure(std::shared_ptr<GuildServiceReviewGuildApplicationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceReviewGuildApplication(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceReviewGuildApplicationCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ReviewGuildApplicationResponse> done) {
    auto call = std::make_shared<GuildServiceReviewGuildApplicationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceReviewGuildApplicationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceReviewGuildApplicationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceReviewGuildApplicationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceReviewGuildApplication(*call, outcome, nullptr);
            return;
        }
        ::guildpb::ReviewGuildApplicationResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.ReviewGuildApplication response";
            CompleteGuildServiceReviewGuildApplication(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceReviewGuildApplication(*call, outcome, &reply);
    });
}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request,
                                            GrpcDone<::guildpb::ReviewGuildApplicationResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceReviewGuildApplicationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceReviewGuildApplication(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceReviewGuildApplicationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::ReviewGuildApplicationRequest& derived = static_cast<const ::guildpb::ReviewGuildApplicationRequest&>(message);
    SendGuildServiceReviewGuildApplication(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceNotifyGuildChanged
AsyncGuildServiceNotifyGuildChangedHandlerFunctionType AsyncGuildServiceNotifyGuildChangedHandler;
AsyncGuildServiceNotifyGuildChangedFailedHandlerFunctionType AsyncGuildServiceNotifyGuildChangedFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceNotifyGuildChangedCall {
    ::guildpb::GuildChangedS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceNotifyGuildChangedHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteGuildServiceNotifyGuildChanged(const GuildServiceNotifyGuildChangedCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceNotifyGuildChangedHandler) {
            AsyncGuildServiceNotifyGuildChangedHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.NotifyGuildChanged reply dropped: AsyncGuildServiceNotifyGuildChangedHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceNotifyGuildChangedFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceNotifyGuildChangedMessageId, "GuildService.NotifyGuildChanged", outcome, call.sentMetadata};
        AsyncGuildServiceNotifyGuildChangedFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.NotifyGuildChanged failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceNotifyGuildChangedFailure(std::shared_ptr<GuildServiceNotifyGuildChangedCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceNotifyGuildChanged(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceNotifyGuildChangedCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<GuildServiceNotifyGuildChangedCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceNotifyGuildChangedFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceNotifyGuildChangedGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceNotifyGuildChangedFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceNotifyGuildChanged(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.NotifyGuildChanged response";
            CompleteGuildServiceNotifyGuildChanged(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceNotifyGuildChanged(*call, outcome, &reply);
    });
}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartGuildServiceNotifyGuildChangedCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceNotifyGuildChanged(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceNotifyGuildChangedCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GuildChangedS2C& derived = static_cast<const ::guildpb::GuildChangedS2C&>(message);
    SendGuildServiceNotifyGuildChanged(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceUpdateGuildScore
AsyncGuildServiceUpdateGuildScoreHandlerFunctionType AsyncGuildServiceUpdateGuildScoreHandler;
AsyncGuildServiceUpdateGuildScoreFailedHandlerFunctionType AsyncGuildServiceUpdateGuildScoreFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceUpdateGuildScoreCall {
    ::guildpb::UpdateGuildScoreRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceUpdateGuildScoreHandler / FailedHandler。
    GrpcDone<::guildpb::UpdateGuildScoreResponse> done;
};

void CompleteGuildServiceUpdateGuildScore(const GuildServiceUpdateGuildScoreCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::UpdateGuildScoreResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceUpdateGuildScoreHandler) {
            AsyncGuildServiceUpdateGuildScoreHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.UpdateGuildScore reply dropped: AsyncGuildServiceUpdateGuildScoreHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceUpdateGuildScoreFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceUpdateGuildScoreMessageId, "GuildService.UpdateGuildScore", outcome, call.sentMetadata};
        AsyncGuildServiceUpdateGuildScoreFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.UpdateGuildScore failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceUpdateGuildScoreFailure(std::shared_ptr<GuildServiceUpdateGuildScoreCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceUpdateGuildScore(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceUpdateGuildScoreCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::UpdateGuildScoreResponse> done) {
    auto call = std::make_shared<GuildServiceUpdateGuildScoreCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceUpdateGuildScoreFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceUpdateGuildScoreGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceUpdateGuildScoreFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceUpdateGuildScore(*call, outcome, nullptr);
            return;
        }
        ::guildpb::UpdateGuildScoreResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.UpdateGuildScore response";
            CompleteGuildServiceUpdateGuildScore(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceUpdateGuildScore(*call, outcome, &reply);
    });
}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request,
                                            GrpcDone<::guildpb::UpdateGuildScoreResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceUpdateGuildScoreCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceUpdateGuildScore(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceUpdateGuildScoreCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::UpdateGuildScoreRequest& derived = static_cast<const ::guildpb::UpdateGuildScoreRequest&>(message);
    SendGuildServiceUpdateGuildScore(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildRank
AsyncGuildServiceGetGuildRankHandlerFunctionType AsyncGuildServiceGetGuildRankHandler;
AsyncGuildServiceGetGuildRankFailedHandlerFunctionType AsyncGuildServiceGetGuildRankFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceGetGuildRankCall {
    ::guildpb::GetGuildRankRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildRankHandler / FailedHandler。
    GrpcDone<::guildpb::GetGuildRankResponse> done;
};

void CompleteGuildServiceGetGuildRank(const GuildServiceGetGuildRankCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::GetGuildRankResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceGetGuildRankHandler) {
            AsyncGuildServiceGetGuildRankHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.GetGuildRank reply dropped: AsyncGuildServiceGetGuildRankHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceGetGuildRankFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceGetGuildRankMessageId, "GuildService.GetGuildRank", outcome, call.sentMetadata};
        AsyncGuildServiceGetGuildRankFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildRank failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceGetGuildRankFailure(std::shared_ptr<GuildServiceGetGuildRankCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceGetGuildRank(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildRankCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildRankResponse> done) {
    auto call = std::make_shared<GuildServiceGetGuildRankCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceGetGuildRankFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceGetGuildRankGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceGetGuildRankFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceGetGuildRank(*call, outcome, nullptr);
            return;
        }
        ::guildpb::GetGuildRankResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.GetGuildRank response";
            CompleteGuildServiceGetGuildRank(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceGetGuildRank(*call, outcome, &reply);
    });
}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request,
                                            GrpcDone<::guildpb::GetGuildRankResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildRankCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildRank(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildRankCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GetGuildRankRequest& derived = static_cast<const ::guildpb::GetGuildRankRequest&>(message);
    SendGuildServiceGetGuildRank(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildRankByGuild
AsyncGuildServiceGetGuildRankByGuildHandlerFunctionType AsyncGuildServiceGetGuildRankByGuildHandler;
AsyncGuildServiceGetGuildRankByGuildFailedHandlerFunctionType AsyncGuildServiceGetGuildRankByGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceGetGuildRankByGuildCall {
    ::guildpb::GetGuildRankByGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildRankByGuildHandler / FailedHandler。
    GrpcDone<::guildpb::GetGuildRankByGuildResponse> done;
};

void CompleteGuildServiceGetGuildRankByGuild(const GuildServiceGetGuildRankByGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::GetGuildRankByGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceGetGuildRankByGuildHandler) {
            AsyncGuildServiceGetGuildRankByGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.GetGuildRankByGuild reply dropped: AsyncGuildServiceGetGuildRankByGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceGetGuildRankByGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceGetGuildRankByGuildMessageId, "GuildService.GetGuildRankByGuild", outcome, call.sentMetadata};
        AsyncGuildServiceGetGuildRankByGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildRankByGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceGetGuildRankByGuildFailure(std::shared_ptr<GuildServiceGetGuildRankByGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceGetGuildRankByGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildRankByGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildRankByGuildResponse> done) {
    auto call = std::make_shared<GuildServiceGetGuildRankByGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceGetGuildRankByGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceGetGuildRankByGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceGetGuildRankByGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceGetGuildRankByGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::GetGuildRankByGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.GetGuildRankByGuild response";
            CompleteGuildServiceGetGuildRankByGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceGetGuildRankByGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request,
                                            GrpcDone<::guildpb::GetGuildRankByGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildRankByGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildRankByGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildRankByGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GetGuildRankByGuildRequest& derived = static_cast<const ::guildpb::GetGuildRankByGuildRequest&>(message);
    SendGuildServiceGetGuildRankByGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildDonateOptions
AsyncGuildServiceGetGuildDonateOptionsHandlerFunctionType AsyncGuildServiceGetGuildDonateOptionsHandler;
AsyncGuildServiceGetGuildDonateOptionsFailedHandlerFunctionType AsyncGuildServiceGetGuildDonateOptionsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceGetGuildDonateOptionsCall {
    ::guildpb::GetGuildDonateOptionsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildDonateOptionsHandler / FailedHandler。
    GrpcDone<::guildpb::GetGuildDonateOptionsResponse> done;
};

void CompleteGuildServiceGetGuildDonateOptions(const GuildServiceGetGuildDonateOptionsCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::GetGuildDonateOptionsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceGetGuildDonateOptionsHandler) {
            AsyncGuildServiceGetGuildDonateOptionsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.GetGuildDonateOptions reply dropped: AsyncGuildServiceGetGuildDonateOptionsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceGetGuildDonateOptionsFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceGetGuildDonateOptionsMessageId, "GuildService.GetGuildDonateOptions", outcome, call.sentMetadata};
        AsyncGuildServiceGetGuildDonateOptionsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildDonateOptions failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceGetGuildDonateOptionsFailure(std::shared_ptr<GuildServiceGetGuildDonateOptionsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceGetGuildDonateOptions(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildDonateOptionsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildDonateOptionsResponse> done) {
    auto call = std::make_shared<GuildServiceGetGuildDonateOptionsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceGetGuildDonateOptionsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceGetGuildDonateOptionsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceGetGuildDonateOptionsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceGetGuildDonateOptions(*call, outcome, nullptr);
            return;
        }
        ::guildpb::GetGuildDonateOptionsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.GetGuildDonateOptions response";
            CompleteGuildServiceGetGuildDonateOptions(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceGetGuildDonateOptions(*call, outcome, &reply);
    });
}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request,
                                            GrpcDone<::guildpb::GetGuildDonateOptionsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildDonateOptionsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildDonateOptions(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildDonateOptionsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GetGuildDonateOptionsRequest& derived = static_cast<const ::guildpb::GetGuildDonateOptionsRequest&>(message);
    SendGuildServiceGetGuildDonateOptions(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceDonateToGuild
AsyncGuildServiceDonateToGuildHandlerFunctionType AsyncGuildServiceDonateToGuildHandler;
AsyncGuildServiceDonateToGuildFailedHandlerFunctionType AsyncGuildServiceDonateToGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceDonateToGuildCall {
    ::guildpb::DonateToGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceDonateToGuildHandler / FailedHandler。
    GrpcDone<::guildpb::DonateToGuildResponse> done;
};

void CompleteGuildServiceDonateToGuild(const GuildServiceDonateToGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::DonateToGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceDonateToGuildHandler) {
            AsyncGuildServiceDonateToGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.DonateToGuild reply dropped: AsyncGuildServiceDonateToGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceDonateToGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceDonateToGuildMessageId, "GuildService.DonateToGuild", outcome, call.sentMetadata};
        AsyncGuildServiceDonateToGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.DonateToGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceDonateToGuildFailure(std::shared_ptr<GuildServiceDonateToGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceDonateToGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceDonateToGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::DonateToGuildResponse> done) {
    auto call = std::make_shared<GuildServiceDonateToGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceDonateToGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceDonateToGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceDonateToGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceDonateToGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::DonateToGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.DonateToGuild response";
            CompleteGuildServiceDonateToGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceDonateToGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request,
                                            GrpcDone<::guildpb::DonateToGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceDonateToGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceDonateToGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceDonateToGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::DonateToGuildRequest& derived = static_cast<const ::guildpb::DonateToGuildRequest&>(message);
    SendGuildServiceDonateToGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceUpgradeGuild
AsyncGuildServiceUpgradeGuildHandlerFunctionType AsyncGuildServiceUpgradeGuildHandler;
AsyncGuildServiceUpgradeGuildFailedHandlerFunctionType AsyncGuildServiceUpgradeGuildFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceUpgradeGuildCall {
    ::guildpb::UpgradeGuildRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceUpgradeGuildHandler / FailedHandler。
    GrpcDone<::guildpb::UpgradeGuildResponse> done;
};

void CompleteGuildServiceUpgradeGuild(const GuildServiceUpgradeGuildCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::UpgradeGuildResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceUpgradeGuildHandler) {
            AsyncGuildServiceUpgradeGuildHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.UpgradeGuild reply dropped: AsyncGuildServiceUpgradeGuildHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceUpgradeGuildFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceUpgradeGuildMessageId, "GuildService.UpgradeGuild", outcome, call.sentMetadata};
        AsyncGuildServiceUpgradeGuildFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.UpgradeGuild failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceUpgradeGuildFailure(std::shared_ptr<GuildServiceUpgradeGuildCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceUpgradeGuild(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceUpgradeGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::UpgradeGuildResponse> done) {
    auto call = std::make_shared<GuildServiceUpgradeGuildCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceUpgradeGuildFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceUpgradeGuildGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceUpgradeGuildFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceUpgradeGuild(*call, outcome, nullptr);
            return;
        }
        ::guildpb::UpgradeGuildResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.UpgradeGuild response";
            CompleteGuildServiceUpgradeGuild(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceUpgradeGuild(*call, outcome, &reply);
    });
}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request,
                                            GrpcDone<::guildpb::UpgradeGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceUpgradeGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceUpgradeGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceUpgradeGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::UpgradeGuildRequest& derived = static_cast<const ::guildpb::UpgradeGuildRequest&>(message);
    SendGuildServiceUpgradeGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildShop
AsyncGuildServiceGetGuildShopHandlerFunctionType AsyncGuildServiceGetGuildShopHandler;
AsyncGuildServiceGetGuildShopFailedHandlerFunctionType AsyncGuildServiceGetGuildShopFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceGetGuildShopCall {
    ::guildpb::GetGuildShopRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildShopHandler / FailedHandler。
    GrpcDone<::guildpb::GetGuildShopResponse> done;
};

void CompleteGuildServiceGetGuildShop(const GuildServiceGetGuildShopCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::GetGuildShopResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceGetGuildShopHandler) {
            AsyncGuildServiceGetGuildShopHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.GetGuildShop reply dropped: AsyncGuildServiceGetGuildShopHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceGetGuildShopFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceGetGuildShopMessageId, "GuildService.GetGuildShop", outcome, call.sentMetadata};
        AsyncGuildServiceGetGuildShopFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildShop failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceGetGuildShopFailure(std::shared_ptr<GuildServiceGetGuildShopCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceGetGuildShop(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildShopCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildShopResponse> done) {
    auto call = std::make_shared<GuildServiceGetGuildShopCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceGetGuildShopFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceGetGuildShopGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceGetGuildShopFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceGetGuildShop(*call, outcome, nullptr);
            return;
        }
        ::guildpb::GetGuildShopResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.GetGuildShop response";
            CompleteGuildServiceGetGuildShop(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceGetGuildShop(*call, outcome, &reply);
    });
}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request,
                                            GrpcDone<::guildpb::GetGuildShopResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildShopCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildShop(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildShopCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GetGuildShopRequest& derived = static_cast<const ::guildpb::GetGuildShopRequest&>(message);
    SendGuildServiceGetGuildShop(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceBuyGuildShopGoods
AsyncGuildServiceBuyGuildShopGoodsHandlerFunctionType AsyncGuildServiceBuyGuildShopGoodsHandler;
AsyncGuildServiceBuyGuildShopGoodsFailedHandlerFunctionType AsyncGuildServiceBuyGuildShopGoodsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceBuyGuildShopGoodsCall {
    ::guildpb::BuyGuildShopGoodsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceBuyGuildShopGoodsHandler / FailedHandler。
    GrpcDone<::guildpb::BuyGuildShopGoodsResponse> done;
};

void CompleteGuildServiceBuyGuildShopGoods(const GuildServiceBuyGuildShopGoodsCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::BuyGuildShopGoodsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceBuyGuildShopGoodsHandler) {
            AsyncGuildServiceBuyGuildShopGoodsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.BuyGuildShopGoods reply dropped: AsyncGuildServiceBuyGuildShopGoodsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceBuyGuildShopGoodsFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceBuyGuildShopGoodsMessageId, "GuildService.BuyGuildShopGoods", outcome, call.sentMetadata};
        AsyncGuildServiceBuyGuildShopGoodsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.BuyGuildShopGoods failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceBuyGuildShopGoodsFailure(std::shared_ptr<GuildServiceBuyGuildShopGoodsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceBuyGuildShopGoods(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceBuyGuildShopGoodsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::BuyGuildShopGoodsResponse> done) {
    auto call = std::make_shared<GuildServiceBuyGuildShopGoodsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceBuyGuildShopGoodsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceBuyGuildShopGoodsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceBuyGuildShopGoodsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceBuyGuildShopGoods(*call, outcome, nullptr);
            return;
        }
        ::guildpb::BuyGuildShopGoodsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.BuyGuildShopGoods response";
            CompleteGuildServiceBuyGuildShopGoods(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceBuyGuildShopGoods(*call, outcome, &reply);
    });
}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request,
                                            GrpcDone<::guildpb::BuyGuildShopGoodsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceBuyGuildShopGoodsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceBuyGuildShopGoods(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceBuyGuildShopGoodsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::BuyGuildShopGoodsRequest& derived = static_cast<const ::guildpb::BuyGuildShopGoodsRequest&>(message);
    SendGuildServiceBuyGuildShopGoods(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildActivities
AsyncGuildServiceGetGuildActivitiesHandlerFunctionType AsyncGuildServiceGetGuildActivitiesHandler;
AsyncGuildServiceGetGuildActivitiesFailedHandlerFunctionType AsyncGuildServiceGetGuildActivitiesFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceGetGuildActivitiesCall {
    ::guildpb::GetGuildActivitiesRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceGetGuildActivitiesHandler / FailedHandler。
    GrpcDone<::guildpb::GetGuildActivitiesResponse> done;
};

void CompleteGuildServiceGetGuildActivities(const GuildServiceGetGuildActivitiesCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::GetGuildActivitiesResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceGetGuildActivitiesHandler) {
            AsyncGuildServiceGetGuildActivitiesHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.GetGuildActivities reply dropped: AsyncGuildServiceGetGuildActivitiesHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceGetGuildActivitiesFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceGetGuildActivitiesMessageId, "GuildService.GetGuildActivities", outcome, call.sentMetadata};
        AsyncGuildServiceGetGuildActivitiesFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildActivities failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceGetGuildActivitiesFailure(std::shared_ptr<GuildServiceGetGuildActivitiesCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceGetGuildActivities(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildActivitiesCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildActivitiesResponse> done) {
    auto call = std::make_shared<GuildServiceGetGuildActivitiesCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceGetGuildActivitiesFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceGetGuildActivitiesGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceGetGuildActivitiesFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceGetGuildActivities(*call, outcome, nullptr);
            return;
        }
        ::guildpb::GetGuildActivitiesResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.GetGuildActivities response";
            CompleteGuildServiceGetGuildActivities(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceGetGuildActivities(*call, outcome, &reply);
    });
}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request,
                                            GrpcDone<::guildpb::GetGuildActivitiesResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildActivitiesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildActivities(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildActivitiesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::GetGuildActivitiesRequest& derived = static_cast<const ::guildpb::GetGuildActivitiesRequest&>(message);
    SendGuildServiceGetGuildActivities(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceLightGuildLantern
AsyncGuildServiceLightGuildLanternHandlerFunctionType AsyncGuildServiceLightGuildLanternHandler;
AsyncGuildServiceLightGuildLanternFailedHandlerFunctionType AsyncGuildServiceLightGuildLanternFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceLightGuildLanternCall {
    ::guildpb::LightGuildLanternRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceLightGuildLanternHandler / FailedHandler。
    GrpcDone<::guildpb::LightGuildLanternResponse> done;
};

void CompleteGuildServiceLightGuildLantern(const GuildServiceLightGuildLanternCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::LightGuildLanternResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceLightGuildLanternHandler) {
            AsyncGuildServiceLightGuildLanternHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.LightGuildLantern reply dropped: AsyncGuildServiceLightGuildLanternHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceLightGuildLanternFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceLightGuildLanternMessageId, "GuildService.LightGuildLantern", outcome, call.sentMetadata};
        AsyncGuildServiceLightGuildLanternFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.LightGuildLantern failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceLightGuildLanternFailure(std::shared_ptr<GuildServiceLightGuildLanternCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceLightGuildLantern(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceLightGuildLanternCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::LightGuildLanternResponse> done) {
    auto call = std::make_shared<GuildServiceLightGuildLanternCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceLightGuildLanternFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceLightGuildLanternGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceLightGuildLanternFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceLightGuildLantern(*call, outcome, nullptr);
            return;
        }
        ::guildpb::LightGuildLanternResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.LightGuildLantern response";
            CompleteGuildServiceLightGuildLantern(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceLightGuildLantern(*call, outcome, &reply);
    });
}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request,
                                            GrpcDone<::guildpb::LightGuildLanternResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceLightGuildLanternCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceLightGuildLantern(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceLightGuildLanternCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::LightGuildLanternRequest& derived = static_cast<const ::guildpb::LightGuildLanternRequest&>(message);
    SendGuildServiceLightGuildLantern(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceClaimGuildReunion
AsyncGuildServiceClaimGuildReunionHandlerFunctionType AsyncGuildServiceClaimGuildReunionHandler;
AsyncGuildServiceClaimGuildReunionFailedHandlerFunctionType AsyncGuildServiceClaimGuildReunionFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceClaimGuildReunionCall {
    ::guildpb::ClaimGuildReunionRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceClaimGuildReunionHandler / FailedHandler。
    GrpcDone<::guildpb::ClaimGuildReunionResponse> done;
};

void CompleteGuildServiceClaimGuildReunion(const GuildServiceClaimGuildReunionCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::ClaimGuildReunionResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceClaimGuildReunionHandler) {
            AsyncGuildServiceClaimGuildReunionHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.ClaimGuildReunion reply dropped: AsyncGuildServiceClaimGuildReunionHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceClaimGuildReunionFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceClaimGuildReunionMessageId, "GuildService.ClaimGuildReunion", outcome, call.sentMetadata};
        AsyncGuildServiceClaimGuildReunionFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.ClaimGuildReunion failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceClaimGuildReunionFailure(std::shared_ptr<GuildServiceClaimGuildReunionCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceClaimGuildReunion(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceClaimGuildReunionCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ClaimGuildReunionResponse> done) {
    auto call = std::make_shared<GuildServiceClaimGuildReunionCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceClaimGuildReunionFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceClaimGuildReunionGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceClaimGuildReunionFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceClaimGuildReunion(*call, outcome, nullptr);
            return;
        }
        ::guildpb::ClaimGuildReunionResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.ClaimGuildReunion response";
            CompleteGuildServiceClaimGuildReunion(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceClaimGuildReunion(*call, outcome, &reply);
    });
}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request,
                                            GrpcDone<::guildpb::ClaimGuildReunionResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceClaimGuildReunionCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceClaimGuildReunion(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceClaimGuildReunionCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::ClaimGuildReunionRequest& derived = static_cast<const ::guildpb::ClaimGuildReunionRequest&>(message);
    SendGuildServiceClaimGuildReunion(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceStartGuildTrial
AsyncGuildServiceStartGuildTrialHandlerFunctionType AsyncGuildServiceStartGuildTrialHandler;
AsyncGuildServiceStartGuildTrialFailedHandlerFunctionType AsyncGuildServiceStartGuildTrialFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceStartGuildTrialCall {
    ::guildpb::StartGuildTrialRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceStartGuildTrialHandler / FailedHandler。
    GrpcDone<::guildpb::StartGuildTrialResponse> done;
};

void CompleteGuildServiceStartGuildTrial(const GuildServiceStartGuildTrialCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::StartGuildTrialResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceStartGuildTrialHandler) {
            AsyncGuildServiceStartGuildTrialHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.StartGuildTrial reply dropped: AsyncGuildServiceStartGuildTrialHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceStartGuildTrialFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceStartGuildTrialMessageId, "GuildService.StartGuildTrial", outcome, call.sentMetadata};
        AsyncGuildServiceStartGuildTrialFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.StartGuildTrial failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceStartGuildTrialFailure(std::shared_ptr<GuildServiceStartGuildTrialCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceStartGuildTrial(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceStartGuildTrialCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::StartGuildTrialResponse> done) {
    auto call = std::make_shared<GuildServiceStartGuildTrialCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceStartGuildTrialFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceStartGuildTrialGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceStartGuildTrialFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceStartGuildTrial(*call, outcome, nullptr);
            return;
        }
        ::guildpb::StartGuildTrialResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.StartGuildTrial response";
            CompleteGuildServiceStartGuildTrial(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceStartGuildTrial(*call, outcome, &reply);
    });
}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request,
                                            GrpcDone<::guildpb::StartGuildTrialResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceStartGuildTrialCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceStartGuildTrial(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceStartGuildTrialCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::StartGuildTrialRequest& derived = static_cast<const ::guildpb::StartGuildTrialRequest&>(message);
    SendGuildServiceStartGuildTrial(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceRespondGuildTrialInvite
AsyncGuildServiceRespondGuildTrialInviteHandlerFunctionType AsyncGuildServiceRespondGuildTrialInviteHandler;
AsyncGuildServiceRespondGuildTrialInviteFailedHandlerFunctionType AsyncGuildServiceRespondGuildTrialInviteFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct GuildServiceRespondGuildTrialInviteCall {
    ::guildpb::RespondGuildTrialInviteRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncGuildServiceRespondGuildTrialInviteHandler / FailedHandler。
    GrpcDone<::guildpb::RespondGuildTrialInviteResponse> done;
};

void CompleteGuildServiceRespondGuildTrialInvite(const GuildServiceRespondGuildTrialInviteCall& call, const GrpcCallOutcome& outcome,
                                                const ::guildpb::RespondGuildTrialInviteResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncGuildServiceRespondGuildTrialInviteHandler) {
            AsyncGuildServiceRespondGuildTrialInviteHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC GuildService.RespondGuildTrialInvite reply dropped: AsyncGuildServiceRespondGuildTrialInviteHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncGuildServiceRespondGuildTrialInviteFailedHandler) {
        const GrpcCallFailure failure{ GuildServiceRespondGuildTrialInviteMessageId, "GuildService.RespondGuildTrialInvite", outcome, call.sentMetadata};
        AsyncGuildServiceRespondGuildTrialInviteFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC GuildService.RespondGuildTrialInvite failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalGuildServiceRespondGuildTrialInviteFailure(std::shared_ptr<GuildServiceRespondGuildTrialInviteCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteGuildServiceRespondGuildTrialInvite(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceRespondGuildTrialInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::RespondGuildTrialInviteResponse> done) {
    auto call = std::make_shared<GuildServiceRespondGuildTrialInviteCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalGuildServiceRespondGuildTrialInviteFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kGuildServiceRespondGuildTrialInviteGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalGuildServiceRespondGuildTrialInviteFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteGuildServiceRespondGuildTrialInvite(*call, outcome, nullptr);
            return;
        }
        ::guildpb::RespondGuildTrialInviteResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the GuildService.RespondGuildTrialInvite response";
            CompleteGuildServiceRespondGuildTrialInvite(*call, outcome, nullptr);
            return;
        }
        CompleteGuildServiceRespondGuildTrialInvite(*call, outcome, &reply);
    });
}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request,
                                            GrpcDone<::guildpb::RespondGuildTrialInviteResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceRespondGuildTrialInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceRespondGuildTrialInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceRespondGuildTrialInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::guildpb::RespondGuildTrialInviteRequest& derived = static_cast<const ::guildpb::RespondGuildTrialInviteRequest&>(message);
    SendGuildServiceRespondGuildTrialInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetGuildHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncGuildServiceCreateGuildHandler = handler;
    AsyncGuildServiceGetGuildHandler = handler;
    AsyncGuildServiceGetPlayerGuildHandler = handler;
    AsyncGuildServiceLeaveGuildHandler = handler;
    AsyncGuildServiceDisbandGuildHandler = handler;
    AsyncGuildServiceSetAnnouncementHandler = handler;
    AsyncGuildServiceSetGuildMemberRoleHandler = handler;
    AsyncGuildServiceKickGuildMemberHandler = handler;
    AsyncGuildServiceTransferGuildLeaderHandler = handler;
    AsyncGuildServiceApplyJoinGuildHandler = handler;
    AsyncGuildServiceCancelGuildApplicationHandler = handler;
    AsyncGuildServiceListMyGuildApplicationsHandler = handler;
    AsyncGuildServiceListGuildApplicationsHandler = handler;
    AsyncGuildServiceReviewGuildApplicationHandler = handler;
    AsyncGuildServiceNotifyGuildChangedHandler = handler;
    AsyncGuildServiceUpdateGuildScoreHandler = handler;
    AsyncGuildServiceGetGuildRankHandler = handler;
    AsyncGuildServiceGetGuildRankByGuildHandler = handler;
    AsyncGuildServiceGetGuildDonateOptionsHandler = handler;
    AsyncGuildServiceDonateToGuildHandler = handler;
    AsyncGuildServiceUpgradeGuildHandler = handler;
    AsyncGuildServiceGetGuildShopHandler = handler;
    AsyncGuildServiceBuyGuildShopGoodsHandler = handler;
    AsyncGuildServiceGetGuildActivitiesHandler = handler;
    AsyncGuildServiceLightGuildLanternHandler = handler;
    AsyncGuildServiceClaimGuildReunionHandler = handler;
    AsyncGuildServiceStartGuildTrialHandler = handler;
    AsyncGuildServiceRespondGuildTrialInviteHandler = handler;
    (void)handler;
}

void SetGuildIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncGuildServiceCreateGuildHandler) {
        AsyncGuildServiceCreateGuildHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildHandler) {
        AsyncGuildServiceGetGuildHandler = handler;
    }
    if (!AsyncGuildServiceGetPlayerGuildHandler) {
        AsyncGuildServiceGetPlayerGuildHandler = handler;
    }
    if (!AsyncGuildServiceLeaveGuildHandler) {
        AsyncGuildServiceLeaveGuildHandler = handler;
    }
    if (!AsyncGuildServiceDisbandGuildHandler) {
        AsyncGuildServiceDisbandGuildHandler = handler;
    }
    if (!AsyncGuildServiceSetAnnouncementHandler) {
        AsyncGuildServiceSetAnnouncementHandler = handler;
    }
    if (!AsyncGuildServiceSetGuildMemberRoleHandler) {
        AsyncGuildServiceSetGuildMemberRoleHandler = handler;
    }
    if (!AsyncGuildServiceKickGuildMemberHandler) {
        AsyncGuildServiceKickGuildMemberHandler = handler;
    }
    if (!AsyncGuildServiceTransferGuildLeaderHandler) {
        AsyncGuildServiceTransferGuildLeaderHandler = handler;
    }
    if (!AsyncGuildServiceApplyJoinGuildHandler) {
        AsyncGuildServiceApplyJoinGuildHandler = handler;
    }
    if (!AsyncGuildServiceCancelGuildApplicationHandler) {
        AsyncGuildServiceCancelGuildApplicationHandler = handler;
    }
    if (!AsyncGuildServiceListMyGuildApplicationsHandler) {
        AsyncGuildServiceListMyGuildApplicationsHandler = handler;
    }
    if (!AsyncGuildServiceListGuildApplicationsHandler) {
        AsyncGuildServiceListGuildApplicationsHandler = handler;
    }
    if (!AsyncGuildServiceReviewGuildApplicationHandler) {
        AsyncGuildServiceReviewGuildApplicationHandler = handler;
    }
    if (!AsyncGuildServiceNotifyGuildChangedHandler) {
        AsyncGuildServiceNotifyGuildChangedHandler = handler;
    }
    if (!AsyncGuildServiceUpdateGuildScoreHandler) {
        AsyncGuildServiceUpdateGuildScoreHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildRankHandler) {
        AsyncGuildServiceGetGuildRankHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildRankByGuildHandler) {
        AsyncGuildServiceGetGuildRankByGuildHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildDonateOptionsHandler) {
        AsyncGuildServiceGetGuildDonateOptionsHandler = handler;
    }
    if (!AsyncGuildServiceDonateToGuildHandler) {
        AsyncGuildServiceDonateToGuildHandler = handler;
    }
    if (!AsyncGuildServiceUpgradeGuildHandler) {
        AsyncGuildServiceUpgradeGuildHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildShopHandler) {
        AsyncGuildServiceGetGuildShopHandler = handler;
    }
    if (!AsyncGuildServiceBuyGuildShopGoodsHandler) {
        AsyncGuildServiceBuyGuildShopGoodsHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildActivitiesHandler) {
        AsyncGuildServiceGetGuildActivitiesHandler = handler;
    }
    if (!AsyncGuildServiceLightGuildLanternHandler) {
        AsyncGuildServiceLightGuildLanternHandler = handler;
    }
    if (!AsyncGuildServiceClaimGuildReunionHandler) {
        AsyncGuildServiceClaimGuildReunionHandler = handler;
    }
    if (!AsyncGuildServiceStartGuildTrialHandler) {
        AsyncGuildServiceStartGuildTrialHandler = handler;
    }
    if (!AsyncGuildServiceRespondGuildTrialInviteHandler) {
        AsyncGuildServiceRespondGuildTrialInviteHandler = handler;
    }
    (void)handler;
}

void SetGuildFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncGuildServiceCreateGuildFailedHandler = handler;
    AsyncGuildServiceGetGuildFailedHandler = handler;
    AsyncGuildServiceGetPlayerGuildFailedHandler = handler;
    AsyncGuildServiceLeaveGuildFailedHandler = handler;
    AsyncGuildServiceDisbandGuildFailedHandler = handler;
    AsyncGuildServiceSetAnnouncementFailedHandler = handler;
    AsyncGuildServiceSetGuildMemberRoleFailedHandler = handler;
    AsyncGuildServiceKickGuildMemberFailedHandler = handler;
    AsyncGuildServiceTransferGuildLeaderFailedHandler = handler;
    AsyncGuildServiceApplyJoinGuildFailedHandler = handler;
    AsyncGuildServiceCancelGuildApplicationFailedHandler = handler;
    AsyncGuildServiceListMyGuildApplicationsFailedHandler = handler;
    AsyncGuildServiceListGuildApplicationsFailedHandler = handler;
    AsyncGuildServiceReviewGuildApplicationFailedHandler = handler;
    AsyncGuildServiceNotifyGuildChangedFailedHandler = handler;
    AsyncGuildServiceUpdateGuildScoreFailedHandler = handler;
    AsyncGuildServiceGetGuildRankFailedHandler = handler;
    AsyncGuildServiceGetGuildRankByGuildFailedHandler = handler;
    AsyncGuildServiceGetGuildDonateOptionsFailedHandler = handler;
    AsyncGuildServiceDonateToGuildFailedHandler = handler;
    AsyncGuildServiceUpgradeGuildFailedHandler = handler;
    AsyncGuildServiceGetGuildShopFailedHandler = handler;
    AsyncGuildServiceBuyGuildShopGoodsFailedHandler = handler;
    AsyncGuildServiceGetGuildActivitiesFailedHandler = handler;
    AsyncGuildServiceLightGuildLanternFailedHandler = handler;
    AsyncGuildServiceClaimGuildReunionFailedHandler = handler;
    AsyncGuildServiceStartGuildTrialFailedHandler = handler;
    AsyncGuildServiceRespondGuildTrialInviteFailedHandler = handler;
    (void)handler;
}

void SetGuildIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncGuildServiceCreateGuildFailedHandler) {
        AsyncGuildServiceCreateGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildFailedHandler) {
        AsyncGuildServiceGetGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceGetPlayerGuildFailedHandler) {
        AsyncGuildServiceGetPlayerGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceLeaveGuildFailedHandler) {
        AsyncGuildServiceLeaveGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceDisbandGuildFailedHandler) {
        AsyncGuildServiceDisbandGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceSetAnnouncementFailedHandler) {
        AsyncGuildServiceSetAnnouncementFailedHandler = handler;
    }
    if (!AsyncGuildServiceSetGuildMemberRoleFailedHandler) {
        AsyncGuildServiceSetGuildMemberRoleFailedHandler = handler;
    }
    if (!AsyncGuildServiceKickGuildMemberFailedHandler) {
        AsyncGuildServiceKickGuildMemberFailedHandler = handler;
    }
    if (!AsyncGuildServiceTransferGuildLeaderFailedHandler) {
        AsyncGuildServiceTransferGuildLeaderFailedHandler = handler;
    }
    if (!AsyncGuildServiceApplyJoinGuildFailedHandler) {
        AsyncGuildServiceApplyJoinGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceCancelGuildApplicationFailedHandler) {
        AsyncGuildServiceCancelGuildApplicationFailedHandler = handler;
    }
    if (!AsyncGuildServiceListMyGuildApplicationsFailedHandler) {
        AsyncGuildServiceListMyGuildApplicationsFailedHandler = handler;
    }
    if (!AsyncGuildServiceListGuildApplicationsFailedHandler) {
        AsyncGuildServiceListGuildApplicationsFailedHandler = handler;
    }
    if (!AsyncGuildServiceReviewGuildApplicationFailedHandler) {
        AsyncGuildServiceReviewGuildApplicationFailedHandler = handler;
    }
    if (!AsyncGuildServiceNotifyGuildChangedFailedHandler) {
        AsyncGuildServiceNotifyGuildChangedFailedHandler = handler;
    }
    if (!AsyncGuildServiceUpdateGuildScoreFailedHandler) {
        AsyncGuildServiceUpdateGuildScoreFailedHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildRankFailedHandler) {
        AsyncGuildServiceGetGuildRankFailedHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildRankByGuildFailedHandler) {
        AsyncGuildServiceGetGuildRankByGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildDonateOptionsFailedHandler) {
        AsyncGuildServiceGetGuildDonateOptionsFailedHandler = handler;
    }
    if (!AsyncGuildServiceDonateToGuildFailedHandler) {
        AsyncGuildServiceDonateToGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceUpgradeGuildFailedHandler) {
        AsyncGuildServiceUpgradeGuildFailedHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildShopFailedHandler) {
        AsyncGuildServiceGetGuildShopFailedHandler = handler;
    }
    if (!AsyncGuildServiceBuyGuildShopGoodsFailedHandler) {
        AsyncGuildServiceBuyGuildShopGoodsFailedHandler = handler;
    }
    if (!AsyncGuildServiceGetGuildActivitiesFailedHandler) {
        AsyncGuildServiceGetGuildActivitiesFailedHandler = handler;
    }
    if (!AsyncGuildServiceLightGuildLanternFailedHandler) {
        AsyncGuildServiceLightGuildLanternFailedHandler = handler;
    }
    if (!AsyncGuildServiceClaimGuildReunionFailedHandler) {
        AsyncGuildServiceClaimGuildReunionFailedHandler = handler;
    }
    if (!AsyncGuildServiceStartGuildTrialFailedHandler) {
        AsyncGuildServiceStartGuildTrialFailedHandler = handler;
    }
    if (!AsyncGuildServiceRespondGuildTrialInviteFailedHandler) {
        AsyncGuildServiceRespondGuildTrialInviteFailedHandler = handler;
    }
    (void)handler;
}

void SetGuildCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace guildpb
