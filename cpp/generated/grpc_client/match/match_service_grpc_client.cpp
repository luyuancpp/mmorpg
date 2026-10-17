#include "muduo/base/Logging.h"

#include "match_service_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetMatchServiceCallDeadline 按目标节点类型写入
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

namespace match {
#pragma region MatchServiceGetNetworkProbePlan
AsyncMatchServiceGetNetworkProbePlanHandlerFunctionType AsyncMatchServiceGetNetworkProbePlanHandler;
AsyncMatchServiceGetNetworkProbePlanFailedHandlerFunctionType AsyncMatchServiceGetNetworkProbePlanFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceGetNetworkProbePlanCall {
    ::match::GetNetworkProbePlanRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceGetNetworkProbePlanHandler / FailedHandler。
    GrpcDone<::match::GetNetworkProbePlanResponse> done;
};

void CompleteMatchServiceGetNetworkProbePlan(const MatchServiceGetNetworkProbePlanCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::GetNetworkProbePlanResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceGetNetworkProbePlanHandler) {
            AsyncMatchServiceGetNetworkProbePlanHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.GetNetworkProbePlan reply dropped: AsyncMatchServiceGetNetworkProbePlanHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceGetNetworkProbePlanFailedHandler) {
        const GrpcCallFailure failure{ MatchServiceGetNetworkProbePlanMessageId, "MatchService.GetNetworkProbePlan", outcome, call.sentMetadata};
        AsyncMatchServiceGetNetworkProbePlanFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.GetNetworkProbePlan failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceGetNetworkProbePlanFailure(std::shared_ptr<MatchServiceGetNetworkProbePlanCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceGetNetworkProbePlan(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceGetNetworkProbePlanCall(entt::registry& registry, entt::entity nodeEntity, const ::match::GetNetworkProbePlanRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::GetNetworkProbePlanResponse> done) {
    auto call = std::make_shared<MatchServiceGetNetworkProbePlanCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceGetNetworkProbePlanFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceGetNetworkProbePlanGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceGetNetworkProbePlanFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceGetNetworkProbePlan(*call, outcome, nullptr);
            return;
        }
        ::match::GetNetworkProbePlanResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.GetNetworkProbePlan response";
            CompleteMatchServiceGetNetworkProbePlan(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceGetNetworkProbePlan(*call, outcome, &reply);
    });
}

void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const ::match::GetNetworkProbePlanRequest& request,
                                            GrpcDone<::match::GetNetworkProbePlanResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceGetNetworkProbePlanCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const ::match::GetNetworkProbePlanRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceGetNetworkProbePlan(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const ::match::GetNetworkProbePlanRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceGetNetworkProbePlanCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceGetNetworkProbePlan(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::GetNetworkProbePlanRequest& derived = static_cast<const ::match::GetNetworkProbePlanRequest&>(message);
    SendMatchServiceGetNetworkProbePlan(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceCommitNetworkProbe
AsyncMatchServiceCommitNetworkProbeHandlerFunctionType AsyncMatchServiceCommitNetworkProbeHandler;
AsyncMatchServiceCommitNetworkProbeFailedHandlerFunctionType AsyncMatchServiceCommitNetworkProbeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceCommitNetworkProbeCall {
    ::match::CommitNetworkProbeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceCommitNetworkProbeHandler / FailedHandler。
    GrpcDone<::match::CommitNetworkProbeResponse> done;
};

void CompleteMatchServiceCommitNetworkProbe(const MatchServiceCommitNetworkProbeCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::CommitNetworkProbeResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceCommitNetworkProbeHandler) {
            AsyncMatchServiceCommitNetworkProbeHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MatchService.CommitNetworkProbe reply dropped: AsyncMatchServiceCommitNetworkProbeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMatchServiceCommitNetworkProbeFailedHandler) {
        const GrpcCallFailure failure{ MatchServiceCommitNetworkProbeMessageId, "MatchService.CommitNetworkProbe", outcome, call.sentMetadata};
        AsyncMatchServiceCommitNetworkProbeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.CommitNetworkProbe failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceCommitNetworkProbeFailure(std::shared_ptr<MatchServiceCommitNetworkProbeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceCommitNetworkProbe(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceCommitNetworkProbeCall(entt::registry& registry, entt::entity nodeEntity, const ::match::CommitNetworkProbeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::CommitNetworkProbeResponse> done) {
    auto call = std::make_shared<MatchServiceCommitNetworkProbeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceCommitNetworkProbeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceCommitNetworkProbeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceCommitNetworkProbeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceCommitNetworkProbe(*call, outcome, nullptr);
            return;
        }
        ::match::CommitNetworkProbeResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.CommitNetworkProbe response";
            CompleteMatchServiceCommitNetworkProbe(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceCommitNetworkProbe(*call, outcome, &reply);
    });
}

void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const ::match::CommitNetworkProbeRequest& request,
                                            GrpcDone<::match::CommitNetworkProbeResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceCommitNetworkProbeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const ::match::CommitNetworkProbeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceCommitNetworkProbe(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const ::match::CommitNetworkProbeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceCommitNetworkProbeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceCommitNetworkProbe(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::CommitNetworkProbeRequest& derived = static_cast<const ::match::CommitNetworkProbeRequest&>(message);
    SendMatchServiceCommitNetworkProbe(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceJoinQueue
AsyncMatchServiceJoinQueueHandlerFunctionType AsyncMatchServiceJoinQueueHandler;
AsyncMatchServiceJoinQueueFailedHandlerFunctionType AsyncMatchServiceJoinQueueFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceJoinQueueCall {
    ::match::JoinQueueRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceJoinQueueHandler / FailedHandler。
    GrpcDone<::match::JoinQueueResponse> done;
};

void CompleteMatchServiceJoinQueue(const MatchServiceJoinQueueCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::JoinQueueResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceJoinQueueHandler) {
            AsyncMatchServiceJoinQueueHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceJoinQueueMessageId, "MatchService.JoinQueue", outcome, call.sentMetadata};
        AsyncMatchServiceJoinQueueFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.JoinQueue failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceJoinQueueFailure(std::shared_ptr<MatchServiceJoinQueueCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceJoinQueue(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceJoinQueueCall(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::JoinQueueResponse> done) {
    auto call = std::make_shared<MatchServiceJoinQueueCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceJoinQueueFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceJoinQueueGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceJoinQueueFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceJoinQueue(*call, outcome, nullptr);
            return;
        }
        ::match::JoinQueueResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.JoinQueue response";
            CompleteMatchServiceJoinQueue(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceJoinQueue(*call, outcome, &reply);
    });
}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request,
                                            GrpcDone<::match::JoinQueueResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceJoinQueueCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceJoinQueue(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::JoinQueueRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceJoinQueueCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceJoinQueue(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::JoinQueueRequest& derived = static_cast<const ::match::JoinQueueRequest&>(message);
    SendMatchServiceJoinQueue(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceCancelQueue
AsyncMatchServiceCancelQueueHandlerFunctionType AsyncMatchServiceCancelQueueHandler;
AsyncMatchServiceCancelQueueFailedHandlerFunctionType AsyncMatchServiceCancelQueueFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceCancelQueueCall {
    ::match::CancelQueueRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceCancelQueueHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteMatchServiceCancelQueue(const MatchServiceCancelQueueCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceCancelQueueHandler) {
            AsyncMatchServiceCancelQueueHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceCancelQueueMessageId, "MatchService.CancelQueue", outcome, call.sentMetadata};
        AsyncMatchServiceCancelQueueFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.CancelQueue failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceCancelQueueFailure(std::shared_ptr<MatchServiceCancelQueueCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceCancelQueue(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceCancelQueueCall(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<MatchServiceCancelQueueCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceCancelQueueFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceCancelQueueGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceCancelQueueFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceCancelQueue(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.CancelQueue response";
            CompleteMatchServiceCancelQueue(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceCancelQueue(*call, outcome, &reply);
    });
}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartMatchServiceCancelQueueCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceCancelQueue(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const ::match::CancelQueueRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceCancelQueueCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceCancelQueue(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::CancelQueueRequest& derived = static_cast<const ::match::CancelQueueRequest&>(message);
    SendMatchServiceCancelQueue(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceGetQueueStatus
AsyncMatchServiceGetQueueStatusHandlerFunctionType AsyncMatchServiceGetQueueStatusHandler;
AsyncMatchServiceGetQueueStatusFailedHandlerFunctionType AsyncMatchServiceGetQueueStatusFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceGetQueueStatusCall {
    ::match::GetQueueStatusRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceGetQueueStatusHandler / FailedHandler。
    GrpcDone<::match::GetQueueStatusResponse> done;
};

void CompleteMatchServiceGetQueueStatus(const MatchServiceGetQueueStatusCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::GetQueueStatusResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceGetQueueStatusHandler) {
            AsyncMatchServiceGetQueueStatusHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceGetQueueStatusMessageId, "MatchService.GetQueueStatus", outcome, call.sentMetadata};
        AsyncMatchServiceGetQueueStatusFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.GetQueueStatus failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceGetQueueStatusFailure(std::shared_ptr<MatchServiceGetQueueStatusCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceGetQueueStatus(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceGetQueueStatusCall(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::GetQueueStatusResponse> done) {
    auto call = std::make_shared<MatchServiceGetQueueStatusCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceGetQueueStatusFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceGetQueueStatusGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceGetQueueStatusFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceGetQueueStatus(*call, outcome, nullptr);
            return;
        }
        ::match::GetQueueStatusResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.GetQueueStatus response";
            CompleteMatchServiceGetQueueStatus(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceGetQueueStatus(*call, outcome, &reply);
    });
}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request,
                                            GrpcDone<::match::GetQueueStatusResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceGetQueueStatusCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceGetQueueStatus(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const ::match::GetQueueStatusRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceGetQueueStatusCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceGetQueueStatus(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::GetQueueStatusRequest& derived = static_cast<const ::match::GetQueueStatusRequest&>(message);
    SendMatchServiceGetQueueStatus(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceChallengePlayer
AsyncMatchServiceChallengePlayerHandlerFunctionType AsyncMatchServiceChallengePlayerHandler;
AsyncMatchServiceChallengePlayerFailedHandlerFunctionType AsyncMatchServiceChallengePlayerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceChallengePlayerCall {
    ::match::ChallengePlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceChallengePlayerHandler / FailedHandler。
    GrpcDone<::match::ChallengePlayerResponse> done;
};

void CompleteMatchServiceChallengePlayer(const MatchServiceChallengePlayerCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::ChallengePlayerResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceChallengePlayerHandler) {
            AsyncMatchServiceChallengePlayerHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceChallengePlayerMessageId, "MatchService.ChallengePlayer", outcome, call.sentMetadata};
        AsyncMatchServiceChallengePlayerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.ChallengePlayer failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceChallengePlayerFailure(std::shared_ptr<MatchServiceChallengePlayerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceChallengePlayer(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceChallengePlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::ChallengePlayerResponse> done) {
    auto call = std::make_shared<MatchServiceChallengePlayerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceChallengePlayerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceChallengePlayerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceChallengePlayerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceChallengePlayer(*call, outcome, nullptr);
            return;
        }
        ::match::ChallengePlayerResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.ChallengePlayer response";
            CompleteMatchServiceChallengePlayer(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceChallengePlayer(*call, outcome, &reply);
    });
}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request,
                                            GrpcDone<::match::ChallengePlayerResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceChallengePlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceChallengePlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceChallengePlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceChallengePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::ChallengePlayerRequest& derived = static_cast<const ::match::ChallengePlayerRequest&>(message);
    SendMatchServiceChallengePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceRespondChallenge
AsyncMatchServiceRespondChallengeHandlerFunctionType AsyncMatchServiceRespondChallengeHandler;
AsyncMatchServiceRespondChallengeFailedHandlerFunctionType AsyncMatchServiceRespondChallengeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceRespondChallengeCall {
    ::match::RespondChallengeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceRespondChallengeHandler / FailedHandler。
    GrpcDone<::match::RespondChallengeResponse> done;
};

void CompleteMatchServiceRespondChallenge(const MatchServiceRespondChallengeCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::RespondChallengeResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceRespondChallengeHandler) {
            AsyncMatchServiceRespondChallengeHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceRespondChallengeMessageId, "MatchService.RespondChallenge", outcome, call.sentMetadata};
        AsyncMatchServiceRespondChallengeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.RespondChallenge failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceRespondChallengeFailure(std::shared_ptr<MatchServiceRespondChallengeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceRespondChallenge(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceRespondChallengeCall(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::RespondChallengeResponse> done) {
    auto call = std::make_shared<MatchServiceRespondChallengeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceRespondChallengeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceRespondChallengeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceRespondChallengeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceRespondChallenge(*call, outcome, nullptr);
            return;
        }
        ::match::RespondChallengeResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.RespondChallenge response";
            CompleteMatchServiceRespondChallenge(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceRespondChallenge(*call, outcome, &reply);
    });
}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request,
                                            GrpcDone<::match::RespondChallengeResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceRespondChallengeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceRespondChallenge(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const ::match::RespondChallengeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceRespondChallengeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceRespondChallenge(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::RespondChallengeRequest& derived = static_cast<const ::match::RespondChallengeRequest&>(message);
    SendMatchServiceRespondChallenge(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceNotifyChallengeInvite
AsyncMatchServiceNotifyChallengeInviteHandlerFunctionType AsyncMatchServiceNotifyChallengeInviteHandler;
AsyncMatchServiceNotifyChallengeInviteFailedHandlerFunctionType AsyncMatchServiceNotifyChallengeInviteFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceNotifyChallengeInviteCall {
    ::match::ChallengeInviteS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceNotifyChallengeInviteHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteMatchServiceNotifyChallengeInvite(const MatchServiceNotifyChallengeInviteCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceNotifyChallengeInviteHandler) {
            AsyncMatchServiceNotifyChallengeInviteHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceNotifyChallengeInviteMessageId, "MatchService.NotifyChallengeInvite", outcome, call.sentMetadata};
        AsyncMatchServiceNotifyChallengeInviteFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.NotifyChallengeInvite failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceNotifyChallengeInviteFailure(std::shared_ptr<MatchServiceNotifyChallengeInviteCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceNotifyChallengeInvite(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceNotifyChallengeInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<MatchServiceNotifyChallengeInviteCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceNotifyChallengeInviteFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceNotifyChallengeInviteGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceNotifyChallengeInviteFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceNotifyChallengeInvite(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.NotifyChallengeInvite response";
            CompleteMatchServiceNotifyChallengeInvite(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceNotifyChallengeInvite(*call, outcome, &reply);
    });
}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartMatchServiceNotifyChallengeInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceNotifyChallengeInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeInviteS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceNotifyChallengeInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceNotifyChallengeInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::ChallengeInviteS2C& derived = static_cast<const ::match::ChallengeInviteS2C&>(message);
    SendMatchServiceNotifyChallengeInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceNotifyChallengeResult
AsyncMatchServiceNotifyChallengeResultHandlerFunctionType AsyncMatchServiceNotifyChallengeResultHandler;
AsyncMatchServiceNotifyChallengeResultFailedHandlerFunctionType AsyncMatchServiceNotifyChallengeResultFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceNotifyChallengeResultCall {
    ::match::ChallengeResultS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceNotifyChallengeResultHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteMatchServiceNotifyChallengeResult(const MatchServiceNotifyChallengeResultCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceNotifyChallengeResultHandler) {
            AsyncMatchServiceNotifyChallengeResultHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceNotifyChallengeResultMessageId, "MatchService.NotifyChallengeResult", outcome, call.sentMetadata};
        AsyncMatchServiceNotifyChallengeResultFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.NotifyChallengeResult failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceNotifyChallengeResultFailure(std::shared_ptr<MatchServiceNotifyChallengeResultCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceNotifyChallengeResult(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceNotifyChallengeResultCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<MatchServiceNotifyChallengeResultCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceNotifyChallengeResultFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceNotifyChallengeResultGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceNotifyChallengeResultFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceNotifyChallengeResult(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.NotifyChallengeResult response";
            CompleteMatchServiceNotifyChallengeResult(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceNotifyChallengeResult(*call, outcome, &reply);
    });
}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartMatchServiceNotifyChallengeResultCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceNotifyChallengeResult(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const ::match::ChallengeResultS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceNotifyChallengeResultCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceNotifyChallengeResult(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::ChallengeResultS2C& derived = static_cast<const ::match::ChallengeResultS2C&>(message);
    SendMatchServiceNotifyChallengeResult(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceWatchBattle
AsyncMatchServiceWatchBattleHandlerFunctionType AsyncMatchServiceWatchBattleHandler;
AsyncMatchServiceWatchBattleFailedHandlerFunctionType AsyncMatchServiceWatchBattleFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceWatchBattleCall {
    ::match::WatchBattleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceWatchBattleHandler / FailedHandler。
    GrpcDone<::match::WatchBattleResponse> done;
};

void CompleteMatchServiceWatchBattle(const MatchServiceWatchBattleCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::WatchBattleResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceWatchBattleHandler) {
            AsyncMatchServiceWatchBattleHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceWatchBattleMessageId, "MatchService.WatchBattle", outcome, call.sentMetadata};
        AsyncMatchServiceWatchBattleFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.WatchBattle failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceWatchBattleFailure(std::shared_ptr<MatchServiceWatchBattleCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceWatchBattle(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceWatchBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::WatchBattleResponse> done) {
    auto call = std::make_shared<MatchServiceWatchBattleCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceWatchBattleFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceWatchBattleGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceWatchBattleFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceWatchBattle(*call, outcome, nullptr);
            return;
        }
        ::match::WatchBattleResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.WatchBattle response";
            CompleteMatchServiceWatchBattle(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceWatchBattle(*call, outcome, &reply);
    });
}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request,
                                            GrpcDone<::match::WatchBattleResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceWatchBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceWatchBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::match::WatchBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceWatchBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceWatchBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::WatchBattleRequest& derived = static_cast<const ::match::WatchBattleRequest&>(message);
    SendMatchServiceWatchBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceRequestBattleTicket
AsyncMatchServiceRequestBattleTicketHandlerFunctionType AsyncMatchServiceRequestBattleTicketHandler;
AsyncMatchServiceRequestBattleTicketFailedHandlerFunctionType AsyncMatchServiceRequestBattleTicketFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceRequestBattleTicketCall {
    ::RequestBattleTicketRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceRequestBattleTicketHandler / FailedHandler。
    GrpcDone<::RequestBattleTicketResponse> done;
};

void CompleteMatchServiceRequestBattleTicket(const MatchServiceRequestBattleTicketCall& call, const GrpcCallOutcome& outcome,
                                                const ::RequestBattleTicketResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceRequestBattleTicketHandler) {
            AsyncMatchServiceRequestBattleTicketHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceRequestBattleTicketMessageId, "MatchService.RequestBattleTicket", outcome, call.sentMetadata};
        AsyncMatchServiceRequestBattleTicketFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.RequestBattleTicket failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceRequestBattleTicketFailure(std::shared_ptr<MatchServiceRequestBattleTicketCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceRequestBattleTicket(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceRequestBattleTicketCall(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::RequestBattleTicketResponse> done) {
    auto call = std::make_shared<MatchServiceRequestBattleTicketCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceRequestBattleTicketFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceRequestBattleTicketGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceRequestBattleTicketFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceRequestBattleTicket(*call, outcome, nullptr);
            return;
        }
        ::RequestBattleTicketResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.RequestBattleTicket response";
            CompleteMatchServiceRequestBattleTicket(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceRequestBattleTicket(*call, outcome, &reply);
    });
}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request,
                                            GrpcDone<::RequestBattleTicketResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceRequestBattleTicketCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceRequestBattleTicket(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::RequestBattleTicketRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceRequestBattleTicketCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceRequestBattleTicket(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::RequestBattleTicketRequest& derived = static_cast<const ::RequestBattleTicketRequest&>(message);
    SendMatchServiceRequestBattleTicket(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MatchServiceListWatchableBattles
AsyncMatchServiceListWatchableBattlesHandlerFunctionType AsyncMatchServiceListWatchableBattlesHandler;
AsyncMatchServiceListWatchableBattlesFailedHandlerFunctionType AsyncMatchServiceListWatchableBattlesFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MatchServiceListWatchableBattlesCall {
    ::match::ListWatchableBattlesRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMatchServiceListWatchableBattlesHandler / FailedHandler。
    GrpcDone<::match::ListWatchableBattlesResponse> done;
};

void CompleteMatchServiceListWatchableBattles(const MatchServiceListWatchableBattlesCall& call, const GrpcCallOutcome& outcome,
                                                const ::match::ListWatchableBattlesResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMatchServiceListWatchableBattlesHandler) {
            AsyncMatchServiceListWatchableBattlesHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ MatchServiceListWatchableBattlesMessageId, "MatchService.ListWatchableBattles", outcome, call.sentMetadata};
        AsyncMatchServiceListWatchableBattlesFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MatchService.ListWatchableBattles failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMatchServiceListWatchableBattlesFailure(std::shared_ptr<MatchServiceListWatchableBattlesCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMatchServiceListWatchableBattles(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMatchServiceListWatchableBattlesCall(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::match::ListWatchableBattlesResponse> done) {
    auto call = std::make_shared<MatchServiceListWatchableBattlesCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMatchServiceListWatchableBattlesFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMatchServiceListWatchableBattlesGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMatchServiceListWatchableBattlesFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMatchServiceListWatchableBattles(*call, outcome, nullptr);
            return;
        }
        ::match::ListWatchableBattlesResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MatchService.ListWatchableBattles response";
            CompleteMatchServiceListWatchableBattles(*call, outcome, nullptr);
            return;
        }
        CompleteMatchServiceListWatchableBattles(*call, outcome, &reply);
    });
}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request,
                                            GrpcDone<::match::ListWatchableBattlesResponse> done, const GrpcCallOptions& options) {
    StartMatchServiceListWatchableBattlesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMatchServiceListWatchableBattles(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const ::match::ListWatchableBattlesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMatchServiceListWatchableBattlesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMatchServiceListWatchableBattles(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::match::ListWatchableBattlesRequest& derived = static_cast<const ::match::ListWatchableBattlesRequest&>(message);
    SendMatchServiceListWatchableBattles(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetMatchServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncMatchServiceGetNetworkProbePlanHandler = handler;
    AsyncMatchServiceCommitNetworkProbeHandler = handler;
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
    (void)handler;
}

void SetMatchServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncMatchServiceGetNetworkProbePlanHandler) {
        AsyncMatchServiceGetNetworkProbePlanHandler = handler;
    }
    if (!AsyncMatchServiceCommitNetworkProbeHandler) {
        AsyncMatchServiceCommitNetworkProbeHandler = handler;
    }
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
    (void)handler;
}

void SetMatchServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncMatchServiceGetNetworkProbePlanFailedHandler = handler;
    AsyncMatchServiceCommitNetworkProbeFailedHandler = handler;
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
    (void)handler;
}

void SetMatchServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncMatchServiceGetNetworkProbePlanFailedHandler) {
        AsyncMatchServiceGetNetworkProbePlanFailedHandler = handler;
    }
    if (!AsyncMatchServiceCommitNetworkProbeFailedHandler) {
        AsyncMatchServiceCommitNetworkProbeFailedHandler = handler;
    }
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
    (void)handler;
}

void SetMatchServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace match
