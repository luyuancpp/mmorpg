#include "muduo/base/Logging.h"

#include "rank_admin_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetRankAdminCallDeadline 按目标节点类型写入
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

namespace rankpb {
#pragma region RankAdminOpenRankGeneration
AsyncRankAdminOpenRankGenerationHandlerFunctionType AsyncRankAdminOpenRankGenerationHandler;
AsyncRankAdminOpenRankGenerationFailedHandlerFunctionType AsyncRankAdminOpenRankGenerationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct RankAdminOpenRankGenerationCall {
    ::rankpb::OpenRankGenerationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncRankAdminOpenRankGenerationHandler / FailedHandler。
    GrpcDone<::rankpb::OpenRankGenerationResponse> done;
};

void CompleteRankAdminOpenRankGeneration(const RankAdminOpenRankGenerationCall& call, const GrpcCallOutcome& outcome,
                                                const ::rankpb::OpenRankGenerationResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncRankAdminOpenRankGenerationHandler) {
            AsyncRankAdminOpenRankGenerationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC RankAdmin.OpenRankGeneration reply dropped: AsyncRankAdminOpenRankGenerationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncRankAdminOpenRankGenerationFailedHandler) {
        const GrpcCallFailure failure{ RankAdminOpenRankGenerationMessageId, "RankAdmin.OpenRankGeneration", outcome, call.sentMetadata};
        AsyncRankAdminOpenRankGenerationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC RankAdmin.OpenRankGeneration failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalRankAdminOpenRankGenerationFailure(std::shared_ptr<RankAdminOpenRankGenerationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteRankAdminOpenRankGeneration(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartRankAdminOpenRankGenerationCall(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::rankpb::OpenRankGenerationResponse> done) {
    auto call = std::make_shared<RankAdminOpenRankGenerationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalRankAdminOpenRankGenerationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kRankAdminOpenRankGenerationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalRankAdminOpenRankGenerationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteRankAdminOpenRankGeneration(*call, outcome, nullptr);
            return;
        }
        ::rankpb::OpenRankGenerationResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the RankAdmin.OpenRankGeneration response";
            CompleteRankAdminOpenRankGeneration(*call, outcome, nullptr);
            return;
        }
        CompleteRankAdminOpenRankGeneration(*call, outcome, &reply);
    });
}

void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request,
                                            GrpcDone<::rankpb::OpenRankGenerationResponse> done, const GrpcCallOptions& options) {
    StartRankAdminOpenRankGenerationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendRankAdminOpenRankGeneration(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::OpenRankGenerationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartRankAdminOpenRankGenerationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendRankAdminOpenRankGeneration(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::rankpb::OpenRankGenerationRequest& derived = static_cast<const ::rankpb::OpenRankGenerationRequest&>(message);
    SendRankAdminOpenRankGeneration(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region RankAdminSubmitRankScores
AsyncRankAdminSubmitRankScoresHandlerFunctionType AsyncRankAdminSubmitRankScoresHandler;
AsyncRankAdminSubmitRankScoresFailedHandlerFunctionType AsyncRankAdminSubmitRankScoresFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct RankAdminSubmitRankScoresCall {
    ::rankpb::SubmitRankScoresRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncRankAdminSubmitRankScoresHandler / FailedHandler。
    GrpcDone<::rankpb::SubmitRankScoresResponse> done;
};

void CompleteRankAdminSubmitRankScores(const RankAdminSubmitRankScoresCall& call, const GrpcCallOutcome& outcome,
                                                const ::rankpb::SubmitRankScoresResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncRankAdminSubmitRankScoresHandler) {
            AsyncRankAdminSubmitRankScoresHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC RankAdmin.SubmitRankScores reply dropped: AsyncRankAdminSubmitRankScoresHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncRankAdminSubmitRankScoresFailedHandler) {
        const GrpcCallFailure failure{ RankAdminSubmitRankScoresMessageId, "RankAdmin.SubmitRankScores", outcome, call.sentMetadata};
        AsyncRankAdminSubmitRankScoresFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC RankAdmin.SubmitRankScores failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalRankAdminSubmitRankScoresFailure(std::shared_ptr<RankAdminSubmitRankScoresCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteRankAdminSubmitRankScores(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartRankAdminSubmitRankScoresCall(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::rankpb::SubmitRankScoresResponse> done) {
    auto call = std::make_shared<RankAdminSubmitRankScoresCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalRankAdminSubmitRankScoresFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kRankAdminSubmitRankScoresGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalRankAdminSubmitRankScoresFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteRankAdminSubmitRankScores(*call, outcome, nullptr);
            return;
        }
        ::rankpb::SubmitRankScoresResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the RankAdmin.SubmitRankScores response";
            CompleteRankAdminSubmitRankScores(*call, outcome, nullptr);
            return;
        }
        CompleteRankAdminSubmitRankScores(*call, outcome, &reply);
    });
}

void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request,
                                            GrpcDone<::rankpb::SubmitRankScoresResponse> done, const GrpcCallOptions& options) {
    StartRankAdminSubmitRankScoresCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendRankAdminSubmitRankScores(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::SubmitRankScoresRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartRankAdminSubmitRankScoresCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendRankAdminSubmitRankScores(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::rankpb::SubmitRankScoresRequest& derived = static_cast<const ::rankpb::SubmitRankScoresRequest&>(message);
    SendRankAdminSubmitRankScores(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region RankAdminFreezeRankGeneration
AsyncRankAdminFreezeRankGenerationHandlerFunctionType AsyncRankAdminFreezeRankGenerationHandler;
AsyncRankAdminFreezeRankGenerationFailedHandlerFunctionType AsyncRankAdminFreezeRankGenerationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct RankAdminFreezeRankGenerationCall {
    ::rankpb::FreezeRankGenerationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncRankAdminFreezeRankGenerationHandler / FailedHandler。
    GrpcDone<::rankpb::FreezeRankGenerationResponse> done;
};

void CompleteRankAdminFreezeRankGeneration(const RankAdminFreezeRankGenerationCall& call, const GrpcCallOutcome& outcome,
                                                const ::rankpb::FreezeRankGenerationResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncRankAdminFreezeRankGenerationHandler) {
            AsyncRankAdminFreezeRankGenerationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC RankAdmin.FreezeRankGeneration reply dropped: AsyncRankAdminFreezeRankGenerationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncRankAdminFreezeRankGenerationFailedHandler) {
        const GrpcCallFailure failure{ RankAdminFreezeRankGenerationMessageId, "RankAdmin.FreezeRankGeneration", outcome, call.sentMetadata};
        AsyncRankAdminFreezeRankGenerationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC RankAdmin.FreezeRankGeneration failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalRankAdminFreezeRankGenerationFailure(std::shared_ptr<RankAdminFreezeRankGenerationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteRankAdminFreezeRankGeneration(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartRankAdminFreezeRankGenerationCall(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::rankpb::FreezeRankGenerationResponse> done) {
    auto call = std::make_shared<RankAdminFreezeRankGenerationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalRankAdminFreezeRankGenerationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kRankAdminFreezeRankGenerationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalRankAdminFreezeRankGenerationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteRankAdminFreezeRankGeneration(*call, outcome, nullptr);
            return;
        }
        ::rankpb::FreezeRankGenerationResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the RankAdmin.FreezeRankGeneration response";
            CompleteRankAdminFreezeRankGeneration(*call, outcome, nullptr);
            return;
        }
        CompleteRankAdminFreezeRankGeneration(*call, outcome, &reply);
    });
}

void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request,
                                            GrpcDone<::rankpb::FreezeRankGenerationResponse> done, const GrpcCallOptions& options) {
    StartRankAdminFreezeRankGenerationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendRankAdminFreezeRankGeneration(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::FreezeRankGenerationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartRankAdminFreezeRankGenerationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendRankAdminFreezeRankGeneration(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::rankpb::FreezeRankGenerationRequest& derived = static_cast<const ::rankpb::FreezeRankGenerationRequest&>(message);
    SendRankAdminFreezeRankGeneration(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region RankAdminRemoveRankEntry
AsyncRankAdminRemoveRankEntryHandlerFunctionType AsyncRankAdminRemoveRankEntryHandler;
AsyncRankAdminRemoveRankEntryFailedHandlerFunctionType AsyncRankAdminRemoveRankEntryFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct RankAdminRemoveRankEntryCall {
    ::rankpb::RemoveRankEntryRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncRankAdminRemoveRankEntryHandler / FailedHandler。
    GrpcDone<::rankpb::RankAdminResponse> done;
};

void CompleteRankAdminRemoveRankEntry(const RankAdminRemoveRankEntryCall& call, const GrpcCallOutcome& outcome,
                                                const ::rankpb::RankAdminResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncRankAdminRemoveRankEntryHandler) {
            AsyncRankAdminRemoveRankEntryHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC RankAdmin.RemoveRankEntry reply dropped: AsyncRankAdminRemoveRankEntryHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncRankAdminRemoveRankEntryFailedHandler) {
        const GrpcCallFailure failure{ RankAdminRemoveRankEntryMessageId, "RankAdmin.RemoveRankEntry", outcome, call.sentMetadata};
        AsyncRankAdminRemoveRankEntryFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC RankAdmin.RemoveRankEntry failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalRankAdminRemoveRankEntryFailure(std::shared_ptr<RankAdminRemoveRankEntryCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteRankAdminRemoveRankEntry(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartRankAdminRemoveRankEntryCall(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::rankpb::RankAdminResponse> done) {
    auto call = std::make_shared<RankAdminRemoveRankEntryCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalRankAdminRemoveRankEntryFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kRankAdminRemoveRankEntryGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalRankAdminRemoveRankEntryFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteRankAdminRemoveRankEntry(*call, outcome, nullptr);
            return;
        }
        ::rankpb::RankAdminResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the RankAdmin.RemoveRankEntry response";
            CompleteRankAdminRemoveRankEntry(*call, outcome, nullptr);
            return;
        }
        CompleteRankAdminRemoveRankEntry(*call, outcome, &reply);
    });
}

void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request,
                                            GrpcDone<::rankpb::RankAdminResponse> done, const GrpcCallOptions& options) {
    StartRankAdminRemoveRankEntryCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendRankAdminRemoveRankEntry(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartRankAdminRemoveRankEntryCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendRankAdminRemoveRankEntry(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::rankpb::RemoveRankEntryRequest& derived = static_cast<const ::rankpb::RemoveRankEntryRequest&>(message);
    SendRankAdminRemoveRankEntry(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region RankAdminDeleteRankGeneration
AsyncRankAdminDeleteRankGenerationHandlerFunctionType AsyncRankAdminDeleteRankGenerationHandler;
AsyncRankAdminDeleteRankGenerationFailedHandlerFunctionType AsyncRankAdminDeleteRankGenerationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct RankAdminDeleteRankGenerationCall {
    ::rankpb::RemoveRankEntryRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncRankAdminDeleteRankGenerationHandler / FailedHandler。
    GrpcDone<::rankpb::RankAdminResponse> done;
};

void CompleteRankAdminDeleteRankGeneration(const RankAdminDeleteRankGenerationCall& call, const GrpcCallOutcome& outcome,
                                                const ::rankpb::RankAdminResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncRankAdminDeleteRankGenerationHandler) {
            AsyncRankAdminDeleteRankGenerationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC RankAdmin.DeleteRankGeneration reply dropped: AsyncRankAdminDeleteRankGenerationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncRankAdminDeleteRankGenerationFailedHandler) {
        const GrpcCallFailure failure{ RankAdminDeleteRankGenerationMessageId, "RankAdmin.DeleteRankGeneration", outcome, call.sentMetadata};
        AsyncRankAdminDeleteRankGenerationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC RankAdmin.DeleteRankGeneration failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalRankAdminDeleteRankGenerationFailure(std::shared_ptr<RankAdminDeleteRankGenerationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteRankAdminDeleteRankGeneration(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartRankAdminDeleteRankGenerationCall(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::rankpb::RankAdminResponse> done) {
    auto call = std::make_shared<RankAdminDeleteRankGenerationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalRankAdminDeleteRankGenerationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kRankAdminDeleteRankGenerationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalRankAdminDeleteRankGenerationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteRankAdminDeleteRankGeneration(*call, outcome, nullptr);
            return;
        }
        ::rankpb::RankAdminResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the RankAdmin.DeleteRankGeneration response";
            CompleteRankAdminDeleteRankGeneration(*call, outcome, nullptr);
            return;
        }
        CompleteRankAdminDeleteRankGeneration(*call, outcome, &reply);
    });
}

void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request,
                                            GrpcDone<::rankpb::RankAdminResponse> done, const GrpcCallOptions& options) {
    StartRankAdminDeleteRankGenerationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendRankAdminDeleteRankGeneration(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const ::rankpb::RemoveRankEntryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartRankAdminDeleteRankGenerationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendRankAdminDeleteRankGeneration(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::rankpb::RemoveRankEntryRequest& derived = static_cast<const ::rankpb::RemoveRankEntryRequest&>(message);
    SendRankAdminDeleteRankGeneration(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetRankAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncRankAdminOpenRankGenerationHandler = handler;
    AsyncRankAdminSubmitRankScoresHandler = handler;
    AsyncRankAdminFreezeRankGenerationHandler = handler;
    AsyncRankAdminRemoveRankEntryHandler = handler;
    AsyncRankAdminDeleteRankGenerationHandler = handler;
    (void)handler;
}

void SetRankAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncRankAdminOpenRankGenerationHandler) {
        AsyncRankAdminOpenRankGenerationHandler = handler;
    }
    if (!AsyncRankAdminSubmitRankScoresHandler) {
        AsyncRankAdminSubmitRankScoresHandler = handler;
    }
    if (!AsyncRankAdminFreezeRankGenerationHandler) {
        AsyncRankAdminFreezeRankGenerationHandler = handler;
    }
    if (!AsyncRankAdminRemoveRankEntryHandler) {
        AsyncRankAdminRemoveRankEntryHandler = handler;
    }
    if (!AsyncRankAdminDeleteRankGenerationHandler) {
        AsyncRankAdminDeleteRankGenerationHandler = handler;
    }
    (void)handler;
}

void SetRankAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncRankAdminOpenRankGenerationFailedHandler = handler;
    AsyncRankAdminSubmitRankScoresFailedHandler = handler;
    AsyncRankAdminFreezeRankGenerationFailedHandler = handler;
    AsyncRankAdminRemoveRankEntryFailedHandler = handler;
    AsyncRankAdminDeleteRankGenerationFailedHandler = handler;
    (void)handler;
}

void SetRankAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncRankAdminOpenRankGenerationFailedHandler) {
        AsyncRankAdminOpenRankGenerationFailedHandler = handler;
    }
    if (!AsyncRankAdminSubmitRankScoresFailedHandler) {
        AsyncRankAdminSubmitRankScoresFailedHandler = handler;
    }
    if (!AsyncRankAdminFreezeRankGenerationFailedHandler) {
        AsyncRankAdminFreezeRankGenerationFailedHandler = handler;
    }
    if (!AsyncRankAdminRemoveRankEntryFailedHandler) {
        AsyncRankAdminRemoveRankEntryFailedHandler = handler;
    }
    if (!AsyncRankAdminDeleteRankGenerationFailedHandler) {
        AsyncRankAdminDeleteRankGenerationFailedHandler = handler;
    }
    (void)handler;
}

void SetRankAdminCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace rankpb
