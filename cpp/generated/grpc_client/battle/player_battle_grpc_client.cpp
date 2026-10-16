#include "muduo/base/Logging.h"

#include "player_battle_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetPlayerBattleCallDeadline 按目标节点类型写入
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

#pragma region BattleClientPlayerSubmitBattleAction
AsyncBattleClientPlayerSubmitBattleActionHandlerFunctionType AsyncBattleClientPlayerSubmitBattleActionHandler;
AsyncBattleClientPlayerSubmitBattleActionFailedHandlerFunctionType AsyncBattleClientPlayerSubmitBattleActionFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerSubmitBattleActionCall {
    ::SubmitBattleActionRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerSubmitBattleActionHandler / FailedHandler。
    GrpcDone<::SubmitBattleActionResponse> done;
};

void CompleteBattleClientPlayerSubmitBattleAction(const BattleClientPlayerSubmitBattleActionCall& call, const GrpcCallOutcome& outcome,
                                                const ::SubmitBattleActionResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerSubmitBattleActionHandler) {
            AsyncBattleClientPlayerSubmitBattleActionHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.SubmitBattleAction reply dropped: AsyncBattleClientPlayerSubmitBattleActionHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerSubmitBattleActionFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerSubmitBattleActionMessageId, "BattleClientPlayer.SubmitBattleAction", outcome, call.sentMetadata};
        AsyncBattleClientPlayerSubmitBattleActionFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.SubmitBattleAction failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerSubmitBattleActionFailure(std::shared_ptr<BattleClientPlayerSubmitBattleActionCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerSubmitBattleAction(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerSubmitBattleActionCall(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::SubmitBattleActionResponse> done) {
    auto call = std::make_shared<BattleClientPlayerSubmitBattleActionCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerSubmitBattleActionFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerSubmitBattleActionGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerSubmitBattleActionFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerSubmitBattleAction(*call, outcome, nullptr);
            return;
        }
        ::SubmitBattleActionResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.SubmitBattleAction response";
            CompleteBattleClientPlayerSubmitBattleAction(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerSubmitBattleAction(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request,
                                            GrpcDone<::SubmitBattleActionResponse> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerSubmitBattleActionCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerSubmitBattleAction(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const ::SubmitBattleActionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerSubmitBattleActionCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerSubmitBattleAction(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::SubmitBattleActionRequest& derived = static_cast<const ::SubmitBattleActionRequest&>(message);
    SendBattleClientPlayerSubmitBattleAction(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerGetBattleState
AsyncBattleClientPlayerGetBattleStateHandlerFunctionType AsyncBattleClientPlayerGetBattleStateHandler;
AsyncBattleClientPlayerGetBattleStateFailedHandlerFunctionType AsyncBattleClientPlayerGetBattleStateFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerGetBattleStateCall {
    ::GetBattleStateRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerGetBattleStateHandler / FailedHandler。
    GrpcDone<::BattleStateS2C> done;
};

void CompleteBattleClientPlayerGetBattleState(const BattleClientPlayerGetBattleStateCall& call, const GrpcCallOutcome& outcome,
                                                const ::BattleStateS2C* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerGetBattleStateHandler) {
            AsyncBattleClientPlayerGetBattleStateHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.GetBattleState reply dropped: AsyncBattleClientPlayerGetBattleStateHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerGetBattleStateFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerGetBattleStateMessageId, "BattleClientPlayer.GetBattleState", outcome, call.sentMetadata};
        AsyncBattleClientPlayerGetBattleStateFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.GetBattleState failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerGetBattleStateFailure(std::shared_ptr<BattleClientPlayerGetBattleStateCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerGetBattleState(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerGetBattleStateCall(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::BattleStateS2C> done) {
    auto call = std::make_shared<BattleClientPlayerGetBattleStateCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerGetBattleStateFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerGetBattleStateGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerGetBattleStateFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerGetBattleState(*call, outcome, nullptr);
            return;
        }
        ::BattleStateS2C reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.GetBattleState response";
            CompleteBattleClientPlayerGetBattleState(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerGetBattleState(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request,
                                            GrpcDone<::BattleStateS2C> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerGetBattleStateCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerGetBattleState(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const ::GetBattleStateRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerGetBattleStateCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerGetBattleState(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::GetBattleStateRequest& derived = static_cast<const ::GetBattleStateRequest&>(message);
    SendBattleClientPlayerGetBattleState(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleStart
AsyncBattleClientPlayerNotifyBattleStartHandlerFunctionType AsyncBattleClientPlayerNotifyBattleStartHandler;
AsyncBattleClientPlayerNotifyBattleStartFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleStartFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifyBattleStartCall {
    ::BattleStartS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifyBattleStartHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifyBattleStart(const BattleClientPlayerNotifyBattleStartCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleStartHandler) {
            AsyncBattleClientPlayerNotifyBattleStartHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleStart reply dropped: AsyncBattleClientPlayerNotifyBattleStartHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleStartFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifyBattleStartMessageId, "BattleClientPlayer.NotifyBattleStart", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifyBattleStartFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleStart failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifyBattleStartFailure(std::shared_ptr<BattleClientPlayerNotifyBattleStartCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifyBattleStart(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleStartCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifyBattleStartCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifyBattleStartFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifyBattleStartGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifyBattleStartFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifyBattleStart(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifyBattleStart response";
            CompleteBattleClientPlayerNotifyBattleStart(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifyBattleStart(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleStartCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleStart(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const ::BattleStartS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleStartCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifyBattleStart(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::BattleStartS2C& derived = static_cast<const ::BattleStartS2C&>(message);
    SendBattleClientPlayerNotifyBattleStart(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyTurnResult
AsyncBattleClientPlayerNotifyTurnResultHandlerFunctionType AsyncBattleClientPlayerNotifyTurnResultHandler;
AsyncBattleClientPlayerNotifyTurnResultFailedHandlerFunctionType AsyncBattleClientPlayerNotifyTurnResultFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifyTurnResultCall {
    ::TurnResultS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifyTurnResultHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifyTurnResult(const BattleClientPlayerNotifyTurnResultCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifyTurnResultHandler) {
            AsyncBattleClientPlayerNotifyTurnResultHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyTurnResult reply dropped: AsyncBattleClientPlayerNotifyTurnResultHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyTurnResultFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifyTurnResultMessageId, "BattleClientPlayer.NotifyTurnResult", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifyTurnResultFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyTurnResult failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifyTurnResultFailure(std::shared_ptr<BattleClientPlayerNotifyTurnResultCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifyTurnResult(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyTurnResultCall(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifyTurnResultCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifyTurnResultFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifyTurnResultGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifyTurnResultFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifyTurnResult(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifyTurnResult response";
            CompleteBattleClientPlayerNotifyTurnResult(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifyTurnResult(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyTurnResultCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyTurnResult(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyTurnResultCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifyTurnResult(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::TurnResultS2C& derived = static_cast<const ::TurnResultS2C&>(message);
    SendBattleClientPlayerNotifyTurnResult(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleEnd
AsyncBattleClientPlayerNotifyBattleEndHandlerFunctionType AsyncBattleClientPlayerNotifyBattleEndHandler;
AsyncBattleClientPlayerNotifyBattleEndFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleEndFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifyBattleEndCall {
    ::BattleEndS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifyBattleEndHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifyBattleEnd(const BattleClientPlayerNotifyBattleEndCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleEndHandler) {
            AsyncBattleClientPlayerNotifyBattleEndHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleEnd reply dropped: AsyncBattleClientPlayerNotifyBattleEndHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleEndFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifyBattleEndMessageId, "BattleClientPlayer.NotifyBattleEnd", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifyBattleEndFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleEnd failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifyBattleEndFailure(std::shared_ptr<BattleClientPlayerNotifyBattleEndCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifyBattleEnd(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleEndCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifyBattleEndCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifyBattleEndFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifyBattleEndGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifyBattleEndFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifyBattleEnd(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifyBattleEnd response";
            CompleteBattleClientPlayerNotifyBattleEnd(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifyBattleEnd(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleEndCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleEnd(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const ::BattleEndS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleEndCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifyBattleEnd(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::BattleEndS2C& derived = static_cast<const ::BattleEndS2C&>(message);
    SendBattleClientPlayerNotifyBattleEnd(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleReconnect
AsyncBattleClientPlayerNotifyBattleReconnectHandlerFunctionType AsyncBattleClientPlayerNotifyBattleReconnectHandler;
AsyncBattleClientPlayerNotifyBattleReconnectFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifyBattleReconnectCall {
    ::BattleReconnectS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifyBattleReconnectHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifyBattleReconnect(const BattleClientPlayerNotifyBattleReconnectCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleReconnectHandler) {
            AsyncBattleClientPlayerNotifyBattleReconnectHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleReconnect reply dropped: AsyncBattleClientPlayerNotifyBattleReconnectHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifyBattleReconnectMessageId, "BattleClientPlayer.NotifyBattleReconnect", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleReconnect failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifyBattleReconnectFailure(std::shared_ptr<BattleClientPlayerNotifyBattleReconnectCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifyBattleReconnect(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleReconnectCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifyBattleReconnectCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifyBattleReconnectFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifyBattleReconnectGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifyBattleReconnectFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifyBattleReconnect(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifyBattleReconnect response";
            CompleteBattleClientPlayerNotifyBattleReconnect(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifyBattleReconnect(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleReconnectCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleReconnect(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const ::BattleReconnectS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleReconnectCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifyBattleReconnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::BattleReconnectS2C& derived = static_cast<const ::BattleReconnectS2C&>(message);
    SendBattleClientPlayerNotifyBattleReconnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerStopWatchBattle
AsyncBattleClientPlayerStopWatchBattleHandlerFunctionType AsyncBattleClientPlayerStopWatchBattleHandler;
AsyncBattleClientPlayerStopWatchBattleFailedHandlerFunctionType AsyncBattleClientPlayerStopWatchBattleFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerStopWatchBattleCall {
    ::StopWatchBattleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerStopWatchBattleHandler / FailedHandler。
    GrpcDone<::StopWatchBattleResponse> done;
};

void CompleteBattleClientPlayerStopWatchBattle(const BattleClientPlayerStopWatchBattleCall& call, const GrpcCallOutcome& outcome,
                                                const ::StopWatchBattleResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerStopWatchBattleHandler) {
            AsyncBattleClientPlayerStopWatchBattleHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.StopWatchBattle reply dropped: AsyncBattleClientPlayerStopWatchBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerStopWatchBattleFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerStopWatchBattleMessageId, "BattleClientPlayer.StopWatchBattle", outcome, call.sentMetadata};
        AsyncBattleClientPlayerStopWatchBattleFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.StopWatchBattle failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerStopWatchBattleFailure(std::shared_ptr<BattleClientPlayerStopWatchBattleCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerStopWatchBattle(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerStopWatchBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::StopWatchBattleResponse> done) {
    auto call = std::make_shared<BattleClientPlayerStopWatchBattleCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerStopWatchBattleFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerStopWatchBattleGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerStopWatchBattleFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerStopWatchBattle(*call, outcome, nullptr);
            return;
        }
        ::StopWatchBattleResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.StopWatchBattle response";
            CompleteBattleClientPlayerStopWatchBattle(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerStopWatchBattle(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request,
                                            GrpcDone<::StopWatchBattleResponse> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerStopWatchBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerStopWatchBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const ::StopWatchBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerStopWatchBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerStopWatchBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::StopWatchBattleRequest& derived = static_cast<const ::StopWatchBattleRequest&>(message);
    SendBattleClientPlayerStopWatchBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerSetAutoBattle
AsyncBattleClientPlayerSetAutoBattleHandlerFunctionType AsyncBattleClientPlayerSetAutoBattleHandler;
AsyncBattleClientPlayerSetAutoBattleFailedHandlerFunctionType AsyncBattleClientPlayerSetAutoBattleFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerSetAutoBattleCall {
    ::SetAutoBattleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerSetAutoBattleHandler / FailedHandler。
    GrpcDone<::SetAutoBattleResponse> done;
};

void CompleteBattleClientPlayerSetAutoBattle(const BattleClientPlayerSetAutoBattleCall& call, const GrpcCallOutcome& outcome,
                                                const ::SetAutoBattleResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerSetAutoBattleHandler) {
            AsyncBattleClientPlayerSetAutoBattleHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.SetAutoBattle reply dropped: AsyncBattleClientPlayerSetAutoBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerSetAutoBattleFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerSetAutoBattleMessageId, "BattleClientPlayer.SetAutoBattle", outcome, call.sentMetadata};
        AsyncBattleClientPlayerSetAutoBattleFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.SetAutoBattle failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerSetAutoBattleFailure(std::shared_ptr<BattleClientPlayerSetAutoBattleCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerSetAutoBattle(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerSetAutoBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::SetAutoBattleResponse> done) {
    auto call = std::make_shared<BattleClientPlayerSetAutoBattleCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerSetAutoBattleFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerSetAutoBattleGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerSetAutoBattleFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerSetAutoBattle(*call, outcome, nullptr);
            return;
        }
        ::SetAutoBattleResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.SetAutoBattle response";
            CompleteBattleClientPlayerSetAutoBattle(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerSetAutoBattle(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request,
                                            GrpcDone<::SetAutoBattleResponse> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerSetAutoBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerSetAutoBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const ::SetAutoBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerSetAutoBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerSetAutoBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::SetAutoBattleRequest& derived = static_cast<const ::SetAutoBattleRequest&>(message);
    SendBattleClientPlayerSetAutoBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifySpectateState
AsyncBattleClientPlayerNotifySpectateStateHandlerFunctionType AsyncBattleClientPlayerNotifySpectateStateHandler;
AsyncBattleClientPlayerNotifySpectateStateFailedHandlerFunctionType AsyncBattleClientPlayerNotifySpectateStateFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifySpectateStateCall {
    ::SpectateStateS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifySpectateStateHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifySpectateState(const BattleClientPlayerNotifySpectateStateCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifySpectateStateHandler) {
            AsyncBattleClientPlayerNotifySpectateStateHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateState reply dropped: AsyncBattleClientPlayerNotifySpectateStateHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifySpectateStateFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifySpectateStateMessageId, "BattleClientPlayer.NotifySpectateState", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifySpectateStateFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateState failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifySpectateStateFailure(std::shared_ptr<BattleClientPlayerNotifySpectateStateCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifySpectateState(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifySpectateStateCall(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifySpectateStateCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifySpectateStateFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifySpectateStateGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifySpectateStateFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifySpectateState(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifySpectateState response";
            CompleteBattleClientPlayerNotifySpectateState(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifySpectateState(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifySpectateStateCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifySpectateState(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const ::SpectateStateS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifySpectateStateCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifySpectateState(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::SpectateStateS2C& derived = static_cast<const ::SpectateStateS2C&>(message);
    SendBattleClientPlayerNotifySpectateState(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifySpectateTurnResult
AsyncBattleClientPlayerNotifySpectateTurnResultHandlerFunctionType AsyncBattleClientPlayerNotifySpectateTurnResultHandler;
AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandlerFunctionType AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifySpectateTurnResultCall {
    ::TurnResultS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifySpectateTurnResultHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifySpectateTurnResult(const BattleClientPlayerNotifySpectateTurnResultCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifySpectateTurnResultHandler) {
            AsyncBattleClientPlayerNotifySpectateTurnResultHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateTurnResult reply dropped: AsyncBattleClientPlayerNotifySpectateTurnResultHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifySpectateTurnResultMessageId, "BattleClientPlayer.NotifySpectateTurnResult", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateTurnResult failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifySpectateTurnResultFailure(std::shared_ptr<BattleClientPlayerNotifySpectateTurnResultCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifySpectateTurnResult(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifySpectateTurnResultCall(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifySpectateTurnResultCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifySpectateTurnResultFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifySpectateTurnResultGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifySpectateTurnResultFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifySpectateTurnResult(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifySpectateTurnResult response";
            CompleteBattleClientPlayerNotifySpectateTurnResult(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifySpectateTurnResult(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifySpectateTurnResultCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifySpectateTurnResult(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const ::TurnResultS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifySpectateTurnResultCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifySpectateTurnResult(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::TurnResultS2C& derived = static_cast<const ::TurnResultS2C&>(message);
    SendBattleClientPlayerNotifySpectateTurnResult(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifySpectateEnd
AsyncBattleClientPlayerNotifySpectateEndHandlerFunctionType AsyncBattleClientPlayerNotifySpectateEndHandler;
AsyncBattleClientPlayerNotifySpectateEndFailedHandlerFunctionType AsyncBattleClientPlayerNotifySpectateEndFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifySpectateEndCall {
    ::SpectateEndS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifySpectateEndHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifySpectateEnd(const BattleClientPlayerNotifySpectateEndCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifySpectateEndHandler) {
            AsyncBattleClientPlayerNotifySpectateEndHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateEnd reply dropped: AsyncBattleClientPlayerNotifySpectateEndHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifySpectateEndFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifySpectateEndMessageId, "BattleClientPlayer.NotifySpectateEnd", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifySpectateEndFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifySpectateEnd failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifySpectateEndFailure(std::shared_ptr<BattleClientPlayerNotifySpectateEndCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifySpectateEnd(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifySpectateEndCall(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifySpectateEndCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifySpectateEndFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifySpectateEndGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifySpectateEndFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifySpectateEnd(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifySpectateEnd response";
            CompleteBattleClientPlayerNotifySpectateEnd(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifySpectateEnd(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifySpectateEndCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifySpectateEnd(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const ::SpectateEndS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifySpectateEndCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifySpectateEnd(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::SpectateEndS2C& derived = static_cast<const ::SpectateEndS2C&>(message);
    SendBattleClientPlayerNotifySpectateEnd(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleClientPlayerNotifyBattleAssigned
AsyncBattleClientPlayerNotifyBattleAssignedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleAssignedHandler;
AsyncBattleClientPlayerNotifyBattleAssignedFailedHandlerFunctionType AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleClientPlayerNotifyBattleAssignedCall {
    ::BattleAssignedS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleClientPlayerNotifyBattleAssignedHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleClientPlayerNotifyBattleAssigned(const BattleClientPlayerNotifyBattleAssignedCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleClientPlayerNotifyBattleAssignedHandler) {
            AsyncBattleClientPlayerNotifyBattleAssignedHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleAssigned reply dropped: AsyncBattleClientPlayerNotifyBattleAssignedHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler) {
        const GrpcCallFailure failure{ BattleClientPlayerNotifyBattleAssignedMessageId, "BattleClientPlayer.NotifyBattleAssigned", outcome, call.sentMetadata};
        AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleClientPlayer.NotifyBattleAssigned failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleClientPlayerNotifyBattleAssignedFailure(std::shared_ptr<BattleClientPlayerNotifyBattleAssignedCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleClientPlayerNotifyBattleAssigned(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleClientPlayerNotifyBattleAssignedCall(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleClientPlayerNotifyBattleAssignedCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleClientPlayerNotifyBattleAssignedFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleClientPlayerNotifyBattleAssignedGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleClientPlayerNotifyBattleAssignedFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleClientPlayerNotifyBattleAssigned(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleClientPlayer.NotifyBattleAssigned response";
            CompleteBattleClientPlayerNotifyBattleAssigned(*call, outcome, nullptr);
            return;
        }
        CompleteBattleClientPlayerNotifyBattleAssigned(*call, outcome, &reply);
    });
}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleClientPlayerNotifyBattleAssignedCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleClientPlayerNotifyBattleAssigned(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const ::BattleAssignedS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleClientPlayerNotifyBattleAssignedCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleClientPlayerNotifyBattleAssigned(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::BattleAssignedS2C& derived = static_cast<const ::BattleAssignedS2C&>(message);
    SendBattleClientPlayerNotifyBattleAssigned(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetPlayerBattleHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncBattleClientPlayerSubmitBattleActionHandler = handler;
    AsyncBattleClientPlayerGetBattleStateHandler = handler;
    AsyncBattleClientPlayerNotifyBattleStartHandler = handler;
    AsyncBattleClientPlayerNotifyTurnResultHandler = handler;
    AsyncBattleClientPlayerNotifyBattleEndHandler = handler;
    AsyncBattleClientPlayerNotifyBattleReconnectHandler = handler;
    AsyncBattleClientPlayerStopWatchBattleHandler = handler;
    AsyncBattleClientPlayerSetAutoBattleHandler = handler;
    AsyncBattleClientPlayerNotifySpectateStateHandler = handler;
    AsyncBattleClientPlayerNotifySpectateTurnResultHandler = handler;
    AsyncBattleClientPlayerNotifySpectateEndHandler = handler;
    AsyncBattleClientPlayerNotifyBattleAssignedHandler = handler;
    (void)handler;
}

void SetPlayerBattleIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncBattleClientPlayerSubmitBattleActionHandler) {
        AsyncBattleClientPlayerSubmitBattleActionHandler = handler;
    }
    if (!AsyncBattleClientPlayerGetBattleStateHandler) {
        AsyncBattleClientPlayerGetBattleStateHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleStartHandler) {
        AsyncBattleClientPlayerNotifyBattleStartHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyTurnResultHandler) {
        AsyncBattleClientPlayerNotifyTurnResultHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleEndHandler) {
        AsyncBattleClientPlayerNotifyBattleEndHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleReconnectHandler) {
        AsyncBattleClientPlayerNotifyBattleReconnectHandler = handler;
    }
    if (!AsyncBattleClientPlayerStopWatchBattleHandler) {
        AsyncBattleClientPlayerStopWatchBattleHandler = handler;
    }
    if (!AsyncBattleClientPlayerSetAutoBattleHandler) {
        AsyncBattleClientPlayerSetAutoBattleHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateStateHandler) {
        AsyncBattleClientPlayerNotifySpectateStateHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateTurnResultHandler) {
        AsyncBattleClientPlayerNotifySpectateTurnResultHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateEndHandler) {
        AsyncBattleClientPlayerNotifySpectateEndHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleAssignedHandler) {
        AsyncBattleClientPlayerNotifyBattleAssignedHandler = handler;
    }
    (void)handler;
}

void SetPlayerBattleFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncBattleClientPlayerSubmitBattleActionFailedHandler = handler;
    AsyncBattleClientPlayerGetBattleStateFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleStartFailedHandler = handler;
    AsyncBattleClientPlayerNotifyTurnResultFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleEndFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler = handler;
    AsyncBattleClientPlayerStopWatchBattleFailedHandler = handler;
    AsyncBattleClientPlayerSetAutoBattleFailedHandler = handler;
    AsyncBattleClientPlayerNotifySpectateStateFailedHandler = handler;
    AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler = handler;
    AsyncBattleClientPlayerNotifySpectateEndFailedHandler = handler;
    AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler = handler;
    (void)handler;
}

void SetPlayerBattleIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncBattleClientPlayerSubmitBattleActionFailedHandler) {
        AsyncBattleClientPlayerSubmitBattleActionFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerGetBattleStateFailedHandler) {
        AsyncBattleClientPlayerGetBattleStateFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleStartFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleStartFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyTurnResultFailedHandler) {
        AsyncBattleClientPlayerNotifyTurnResultFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleEndFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleEndFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleReconnectFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerStopWatchBattleFailedHandler) {
        AsyncBattleClientPlayerStopWatchBattleFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerSetAutoBattleFailedHandler) {
        AsyncBattleClientPlayerSetAutoBattleFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateStateFailedHandler) {
        AsyncBattleClientPlayerNotifySpectateStateFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler) {
        AsyncBattleClientPlayerNotifySpectateTurnResultFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifySpectateEndFailedHandler) {
        AsyncBattleClientPlayerNotifySpectateEndFailedHandler = handler;
    }
    if (!AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler) {
        AsyncBattleClientPlayerNotifyBattleAssignedFailedHandler = handler;
    }
    (void)handler;
}

void SetPlayerBattleCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}
