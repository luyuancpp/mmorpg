#include "muduo/base/Logging.h"

#include "battle_node_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetBattleNodeCallDeadline 按目标节点类型写入
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

#pragma region BattleNodeQueryBattleCreation
AsyncBattleNodeQueryBattleCreationHandlerFunctionType AsyncBattleNodeQueryBattleCreationHandler;
AsyncBattleNodeQueryBattleCreationFailedHandlerFunctionType AsyncBattleNodeQueryBattleCreationFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleNodeQueryBattleCreationCall {
    ::QueryBattleCreationRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleNodeQueryBattleCreationHandler / FailedHandler。
    GrpcDone<::QueryBattleCreationResponse> done;
};

void CompleteBattleNodeQueryBattleCreation(const BattleNodeQueryBattleCreationCall& call, const GrpcCallOutcome& outcome,
                                                const ::QueryBattleCreationResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleNodeQueryBattleCreationHandler) {
            AsyncBattleNodeQueryBattleCreationHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.QueryBattleCreation reply dropped: AsyncBattleNodeQueryBattleCreationHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeQueryBattleCreationFailedHandler) {
        const GrpcCallFailure failure{ BattleNodeQueryBattleCreationMessageId, "BattleNode.QueryBattleCreation", outcome, call.sentMetadata};
        AsyncBattleNodeQueryBattleCreationFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleNode.QueryBattleCreation failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleNodeQueryBattleCreationFailure(std::shared_ptr<BattleNodeQueryBattleCreationCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleNodeQueryBattleCreation(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeQueryBattleCreationCall(entt::registry& registry, entt::entity nodeEntity, const ::QueryBattleCreationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::QueryBattleCreationResponse> done) {
    auto call = std::make_shared<BattleNodeQueryBattleCreationCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleNodeQueryBattleCreationFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleNodeQueryBattleCreationGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleNodeQueryBattleCreationFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleNodeQueryBattleCreation(*call, outcome, nullptr);
            return;
        }
        ::QueryBattleCreationResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleNode.QueryBattleCreation response";
            CompleteBattleNodeQueryBattleCreation(*call, outcome, nullptr);
            return;
        }
        CompleteBattleNodeQueryBattleCreation(*call, outcome, &reply);
    });
}

void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const ::QueryBattleCreationRequest& request,
                                            GrpcDone<::QueryBattleCreationResponse> done, const GrpcCallOptions& options) {
    StartBattleNodeQueryBattleCreationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const ::QueryBattleCreationRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeQueryBattleCreation(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const ::QueryBattleCreationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeQueryBattleCreationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleNodeQueryBattleCreation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::QueryBattleCreationRequest& derived = static_cast<const ::QueryBattleCreationRequest&>(message);
    SendBattleNodeQueryBattleCreation(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeCreateBattle
AsyncBattleNodeCreateBattleHandlerFunctionType AsyncBattleNodeCreateBattleHandler;
AsyncBattleNodeCreateBattleFailedHandlerFunctionType AsyncBattleNodeCreateBattleFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleNodeCreateBattleCall {
    ::CreateBattleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleNodeCreateBattleHandler / FailedHandler。
    GrpcDone<::CreateBattleResponse> done;
};

void CompleteBattleNodeCreateBattle(const BattleNodeCreateBattleCall& call, const GrpcCallOutcome& outcome,
                                                const ::CreateBattleResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleNodeCreateBattleHandler) {
            AsyncBattleNodeCreateBattleHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.CreateBattle reply dropped: AsyncBattleNodeCreateBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeCreateBattleFailedHandler) {
        const GrpcCallFailure failure{ BattleNodeCreateBattleMessageId, "BattleNode.CreateBattle", outcome, call.sentMetadata};
        AsyncBattleNodeCreateBattleFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleNode.CreateBattle failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleNodeCreateBattleFailure(std::shared_ptr<BattleNodeCreateBattleCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleNodeCreateBattle(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeCreateBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::CreateBattleResponse> done) {
    auto call = std::make_shared<BattleNodeCreateBattleCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleNodeCreateBattleFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleNodeCreateBattleGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleNodeCreateBattleFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleNodeCreateBattle(*call, outcome, nullptr);
            return;
        }
        ::CreateBattleResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleNode.CreateBattle response";
            CompleteBattleNodeCreateBattle(*call, outcome, nullptr);
            return;
        }
        CompleteBattleNodeCreateBattle(*call, outcome, &reply);
    });
}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request,
                                            GrpcDone<::CreateBattleResponse> done, const GrpcCallOptions& options) {
    StartBattleNodeCreateBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeCreateBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const ::CreateBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeCreateBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleNodeCreateBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::CreateBattleRequest& derived = static_cast<const ::CreateBattleRequest&>(message);
    SendBattleNodeCreateBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeDestroyBattle
AsyncBattleNodeDestroyBattleHandlerFunctionType AsyncBattleNodeDestroyBattleHandler;
AsyncBattleNodeDestroyBattleFailedHandlerFunctionType AsyncBattleNodeDestroyBattleFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleNodeDestroyBattleCall {
    ::DestroyBattleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleNodeDestroyBattleHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleNodeDestroyBattle(const BattleNodeDestroyBattleCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleNodeDestroyBattleHandler) {
            AsyncBattleNodeDestroyBattleHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.DestroyBattle reply dropped: AsyncBattleNodeDestroyBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeDestroyBattleFailedHandler) {
        const GrpcCallFailure failure{ BattleNodeDestroyBattleMessageId, "BattleNode.DestroyBattle", outcome, call.sentMetadata};
        AsyncBattleNodeDestroyBattleFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleNode.DestroyBattle failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleNodeDestroyBattleFailure(std::shared_ptr<BattleNodeDestroyBattleCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleNodeDestroyBattle(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeDestroyBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleNodeDestroyBattleCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleNodeDestroyBattleFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleNodeDestroyBattleGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleNodeDestroyBattleFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleNodeDestroyBattle(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleNode.DestroyBattle response";
            CompleteBattleNodeDestroyBattle(*call, outcome, nullptr);
            return;
        }
        CompleteBattleNodeDestroyBattle(*call, outcome, &reply);
    });
}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleNodeDestroyBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeDestroyBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const ::DestroyBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeDestroyBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleNodeDestroyBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::DestroyBattleRequest& derived = static_cast<const ::DestroyBattleRequest&>(message);
    SendBattleNodeDestroyBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeAddObserver
AsyncBattleNodeAddObserverHandlerFunctionType AsyncBattleNodeAddObserverHandler;
AsyncBattleNodeAddObserverFailedHandlerFunctionType AsyncBattleNodeAddObserverFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleNodeAddObserverCall {
    ::AddObserverRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleNodeAddObserverHandler / FailedHandler。
    GrpcDone<::AddObserverResponse> done;
};

void CompleteBattleNodeAddObserver(const BattleNodeAddObserverCall& call, const GrpcCallOutcome& outcome,
                                                const ::AddObserverResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleNodeAddObserverHandler) {
            AsyncBattleNodeAddObserverHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.AddObserver reply dropped: AsyncBattleNodeAddObserverHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeAddObserverFailedHandler) {
        const GrpcCallFailure failure{ BattleNodeAddObserverMessageId, "BattleNode.AddObserver", outcome, call.sentMetadata};
        AsyncBattleNodeAddObserverFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleNode.AddObserver failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleNodeAddObserverFailure(std::shared_ptr<BattleNodeAddObserverCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleNodeAddObserver(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeAddObserverCall(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AddObserverResponse> done) {
    auto call = std::make_shared<BattleNodeAddObserverCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleNodeAddObserverFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleNodeAddObserverGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleNodeAddObserverFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleNodeAddObserver(*call, outcome, nullptr);
            return;
        }
        ::AddObserverResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleNode.AddObserver response";
            CompleteBattleNodeAddObserver(*call, outcome, nullptr);
            return;
        }
        CompleteBattleNodeAddObserver(*call, outcome, &reply);
    });
}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request,
                                            GrpcDone<::AddObserverResponse> done, const GrpcCallOptions& options) {
    StartBattleNodeAddObserverCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeAddObserver(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const ::AddObserverRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeAddObserverCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleNodeAddObserver(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::AddObserverRequest& derived = static_cast<const ::AddObserverRequest&>(message);
    SendBattleNodeAddObserver(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeRemoveObserver
AsyncBattleNodeRemoveObserverHandlerFunctionType AsyncBattleNodeRemoveObserverHandler;
AsyncBattleNodeRemoveObserverFailedHandlerFunctionType AsyncBattleNodeRemoveObserverFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleNodeRemoveObserverCall {
    ::RemoveObserverRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleNodeRemoveObserverHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteBattleNodeRemoveObserver(const BattleNodeRemoveObserverCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleNodeRemoveObserverHandler) {
            AsyncBattleNodeRemoveObserverHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.RemoveObserver reply dropped: AsyncBattleNodeRemoveObserverHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeRemoveObserverFailedHandler) {
        const GrpcCallFailure failure{ BattleNodeRemoveObserverMessageId, "BattleNode.RemoveObserver", outcome, call.sentMetadata};
        AsyncBattleNodeRemoveObserverFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleNode.RemoveObserver failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleNodeRemoveObserverFailure(std::shared_ptr<BattleNodeRemoveObserverCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleNodeRemoveObserver(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeRemoveObserverCall(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<BattleNodeRemoveObserverCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleNodeRemoveObserverFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleNodeRemoveObserverGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleNodeRemoveObserverFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleNodeRemoveObserver(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleNode.RemoveObserver response";
            CompleteBattleNodeRemoveObserver(*call, outcome, nullptr);
            return;
        }
        CompleteBattleNodeRemoveObserver(*call, outcome, &reply);
    });
}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartBattleNodeRemoveObserverCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeRemoveObserver(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const ::RemoveObserverRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeRemoveObserverCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleNodeRemoveObserver(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::RemoveObserverRequest& derived = static_cast<const ::RemoveObserverRequest&>(message);
    SendBattleNodeRemoveObserver(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region BattleNodeIssueBattleTicket
AsyncBattleNodeIssueBattleTicketHandlerFunctionType AsyncBattleNodeIssueBattleTicketHandler;
AsyncBattleNodeIssueBattleTicketFailedHandlerFunctionType AsyncBattleNodeIssueBattleTicketFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct BattleNodeIssueBattleTicketCall {
    ::IssueBattleTicketRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncBattleNodeIssueBattleTicketHandler / FailedHandler。
    GrpcDone<::IssueBattleTicketResponse> done;
};

void CompleteBattleNodeIssueBattleTicket(const BattleNodeIssueBattleTicketCall& call, const GrpcCallOutcome& outcome,
                                                const ::IssueBattleTicketResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncBattleNodeIssueBattleTicketHandler) {
            AsyncBattleNodeIssueBattleTicketHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC BattleNode.IssueBattleTicket reply dropped: AsyncBattleNodeIssueBattleTicketHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncBattleNodeIssueBattleTicketFailedHandler) {
        const GrpcCallFailure failure{ BattleNodeIssueBattleTicketMessageId, "BattleNode.IssueBattleTicket", outcome, call.sentMetadata};
        AsyncBattleNodeIssueBattleTicketFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC BattleNode.IssueBattleTicket failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalBattleNodeIssueBattleTicketFailure(std::shared_ptr<BattleNodeIssueBattleTicketCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteBattleNodeIssueBattleTicket(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartBattleNodeIssueBattleTicketCall(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::IssueBattleTicketResponse> done) {
    auto call = std::make_shared<BattleNodeIssueBattleTicketCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalBattleNodeIssueBattleTicketFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kBattleNodeIssueBattleTicketGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalBattleNodeIssueBattleTicketFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteBattleNodeIssueBattleTicket(*call, outcome, nullptr);
            return;
        }
        ::IssueBattleTicketResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the BattleNode.IssueBattleTicket response";
            CompleteBattleNodeIssueBattleTicket(*call, outcome, nullptr);
            return;
        }
        CompleteBattleNodeIssueBattleTicket(*call, outcome, &reply);
    });
}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request,
                                            GrpcDone<::IssueBattleTicketResponse> done, const GrpcCallOptions& options) {
    StartBattleNodeIssueBattleTicketCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendBattleNodeIssueBattleTicket(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const ::IssueBattleTicketRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartBattleNodeIssueBattleTicketCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendBattleNodeIssueBattleTicket(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::IssueBattleTicketRequest& derived = static_cast<const ::IssueBattleTicketRequest&>(message);
    SendBattleNodeIssueBattleTicket(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetBattleNodeHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncBattleNodeQueryBattleCreationHandler = handler;
    AsyncBattleNodeCreateBattleHandler = handler;
    AsyncBattleNodeDestroyBattleHandler = handler;
    AsyncBattleNodeAddObserverHandler = handler;
    AsyncBattleNodeRemoveObserverHandler = handler;
    AsyncBattleNodeIssueBattleTicketHandler = handler;
    (void)handler;
}

void SetBattleNodeIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncBattleNodeQueryBattleCreationHandler) {
        AsyncBattleNodeQueryBattleCreationHandler = handler;
    }
    if (!AsyncBattleNodeCreateBattleHandler) {
        AsyncBattleNodeCreateBattleHandler = handler;
    }
    if (!AsyncBattleNodeDestroyBattleHandler) {
        AsyncBattleNodeDestroyBattleHandler = handler;
    }
    if (!AsyncBattleNodeAddObserverHandler) {
        AsyncBattleNodeAddObserverHandler = handler;
    }
    if (!AsyncBattleNodeRemoveObserverHandler) {
        AsyncBattleNodeRemoveObserverHandler = handler;
    }
    if (!AsyncBattleNodeIssueBattleTicketHandler) {
        AsyncBattleNodeIssueBattleTicketHandler = handler;
    }
    (void)handler;
}

void SetBattleNodeFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncBattleNodeQueryBattleCreationFailedHandler = handler;
    AsyncBattleNodeCreateBattleFailedHandler = handler;
    AsyncBattleNodeDestroyBattleFailedHandler = handler;
    AsyncBattleNodeAddObserverFailedHandler = handler;
    AsyncBattleNodeRemoveObserverFailedHandler = handler;
    AsyncBattleNodeIssueBattleTicketFailedHandler = handler;
    (void)handler;
}

void SetBattleNodeIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncBattleNodeQueryBattleCreationFailedHandler) {
        AsyncBattleNodeQueryBattleCreationFailedHandler = handler;
    }
    if (!AsyncBattleNodeCreateBattleFailedHandler) {
        AsyncBattleNodeCreateBattleFailedHandler = handler;
    }
    if (!AsyncBattleNodeDestroyBattleFailedHandler) {
        AsyncBattleNodeDestroyBattleFailedHandler = handler;
    }
    if (!AsyncBattleNodeAddObserverFailedHandler) {
        AsyncBattleNodeAddObserverFailedHandler = handler;
    }
    if (!AsyncBattleNodeRemoveObserverFailedHandler) {
        AsyncBattleNodeRemoveObserverFailedHandler = handler;
    }
    if (!AsyncBattleNodeIssueBattleTicketFailedHandler) {
        AsyncBattleNodeIssueBattleTicketFailedHandler = handler;
    }
    (void)handler;
}

void SetBattleNodeCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}
