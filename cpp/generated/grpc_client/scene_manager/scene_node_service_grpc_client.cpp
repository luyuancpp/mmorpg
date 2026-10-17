#include "muduo/base/Logging.h"

#include "scene_node_service_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetSceneNodeServiceCallDeadline 按目标节点类型写入
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

namespace scene_node {
#pragma region SceneNodeGrpcCreateScene
AsyncSceneNodeGrpcCreateSceneHandlerFunctionType AsyncSceneNodeGrpcCreateSceneHandler;
AsyncSceneNodeGrpcCreateSceneFailedHandlerFunctionType AsyncSceneNodeGrpcCreateSceneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcCreateSceneCall {
    ::CreateSceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcCreateSceneHandler / FailedHandler。
    GrpcDone<::CreateSceneResponse> done;
};

void CompleteSceneNodeGrpcCreateScene(const SceneNodeGrpcCreateSceneCall& call, const GrpcCallOutcome& outcome,
                                                const ::CreateSceneResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcCreateSceneHandler) {
            AsyncSceneNodeGrpcCreateSceneHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.CreateScene reply dropped: AsyncSceneNodeGrpcCreateSceneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcCreateSceneFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcCreateSceneMessageId, "SceneNodeGrpc.CreateScene", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcCreateSceneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.CreateScene failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcCreateSceneFailure(std::shared_ptr<SceneNodeGrpcCreateSceneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcCreateScene(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcCreateSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::CreateSceneResponse> done) {
    auto call = std::make_shared<SceneNodeGrpcCreateSceneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcCreateSceneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcCreateSceneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcCreateSceneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcCreateScene(*call, outcome, nullptr);
            return;
        }
        ::CreateSceneResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.CreateScene response";
            CompleteSceneNodeGrpcCreateScene(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcCreateScene(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request,
                                            GrpcDone<::CreateSceneResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcCreateSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcCreateScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcCreateSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::CreateSceneRequest& derived = static_cast<const ::CreateSceneRequest&>(message);
    SendSceneNodeGrpcCreateScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcDestroyScene
AsyncSceneNodeGrpcDestroySceneHandlerFunctionType AsyncSceneNodeGrpcDestroySceneHandler;
AsyncSceneNodeGrpcDestroySceneFailedHandlerFunctionType AsyncSceneNodeGrpcDestroySceneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcDestroySceneCall {
    ::DestroySceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcDestroySceneHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteSceneNodeGrpcDestroyScene(const SceneNodeGrpcDestroySceneCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcDestroySceneHandler) {
            AsyncSceneNodeGrpcDestroySceneHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.DestroyScene reply dropped: AsyncSceneNodeGrpcDestroySceneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcDestroySceneFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcDestroySceneMessageId, "SceneNodeGrpc.DestroyScene", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcDestroySceneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.DestroyScene failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcDestroySceneFailure(std::shared_ptr<SceneNodeGrpcDestroySceneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcDestroyScene(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcDestroySceneCall(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<SceneNodeGrpcDestroySceneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcDestroySceneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcDestroySceneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcDestroySceneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcDestroyScene(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.DestroyScene response";
            CompleteSceneNodeGrpcDestroyScene(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcDestroyScene(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcDestroySceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcDestroyScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcDestroySceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::DestroySceneRequest& derived = static_cast<const ::DestroySceneRequest&>(message);
    SendSceneNodeGrpcDestroyScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcReleasePlayer
AsyncSceneNodeGrpcReleasePlayerHandlerFunctionType AsyncSceneNodeGrpcReleasePlayerHandler;
AsyncSceneNodeGrpcReleasePlayerFailedHandlerFunctionType AsyncSceneNodeGrpcReleasePlayerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcReleasePlayerCall {
    ::scene_node::ReleasePlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcReleasePlayerHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteSceneNodeGrpcReleasePlayer(const SceneNodeGrpcReleasePlayerCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcReleasePlayerHandler) {
            AsyncSceneNodeGrpcReleasePlayerHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.ReleasePlayer reply dropped: AsyncSceneNodeGrpcReleasePlayerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcReleasePlayerFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcReleasePlayerMessageId, "SceneNodeGrpc.ReleasePlayer", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcReleasePlayerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.ReleasePlayer failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcReleasePlayerFailure(std::shared_ptr<SceneNodeGrpcReleasePlayerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcReleasePlayer(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcReleasePlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<SceneNodeGrpcReleasePlayerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcReleasePlayerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcReleasePlayerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcReleasePlayerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcReleasePlayer(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.ReleasePlayer response";
            CompleteSceneNodeGrpcReleasePlayer(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcReleasePlayer(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcReleasePlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcReleasePlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcReleasePlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::scene_node::ReleasePlayerRequest& derived = static_cast<const ::scene_node::ReleasePlayerRequest&>(message);
    SendSceneNodeGrpcReleasePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcPrepareBattle
AsyncSceneNodeGrpcPrepareBattleHandlerFunctionType AsyncSceneNodeGrpcPrepareBattleHandler;
AsyncSceneNodeGrpcPrepareBattleFailedHandlerFunctionType AsyncSceneNodeGrpcPrepareBattleFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcPrepareBattleCall {
    ::PrepareBattleRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcPrepareBattleHandler / FailedHandler。
    GrpcDone<::PrepareBattleResponse> done;
};

void CompleteSceneNodeGrpcPrepareBattle(const SceneNodeGrpcPrepareBattleCall& call, const GrpcCallOutcome& outcome,
                                                const ::PrepareBattleResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcPrepareBattleHandler) {
            AsyncSceneNodeGrpcPrepareBattleHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.PrepareBattle reply dropped: AsyncSceneNodeGrpcPrepareBattleHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcPrepareBattleFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcPrepareBattleMessageId, "SceneNodeGrpc.PrepareBattle", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcPrepareBattleFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.PrepareBattle failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcPrepareBattleFailure(std::shared_ptr<SceneNodeGrpcPrepareBattleCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcPrepareBattle(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcPrepareBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::PrepareBattleResponse> done) {
    auto call = std::make_shared<SceneNodeGrpcPrepareBattleCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcPrepareBattleFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcPrepareBattleGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcPrepareBattleFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcPrepareBattle(*call, outcome, nullptr);
            return;
        }
        ::PrepareBattleResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.PrepareBattle response";
            CompleteSceneNodeGrpcPrepareBattle(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcPrepareBattle(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request,
                                            GrpcDone<::PrepareBattleResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcPrepareBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcPrepareBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcPrepareBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::PrepareBattleRequest& derived = static_cast<const ::PrepareBattleRequest&>(message);
    SendSceneNodeGrpcPrepareBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcCancelBattlePrepare
AsyncSceneNodeGrpcCancelBattlePrepareHandlerFunctionType AsyncSceneNodeGrpcCancelBattlePrepareHandler;
AsyncSceneNodeGrpcCancelBattlePrepareFailedHandlerFunctionType AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcCancelBattlePrepareCall {
    ::CancelBattlePrepareRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcCancelBattlePrepareHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteSceneNodeGrpcCancelBattlePrepare(const SceneNodeGrpcCancelBattlePrepareCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcCancelBattlePrepareHandler) {
            AsyncSceneNodeGrpcCancelBattlePrepareHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.CancelBattlePrepare reply dropped: AsyncSceneNodeGrpcCancelBattlePrepareHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcCancelBattlePrepareMessageId, "SceneNodeGrpc.CancelBattlePrepare", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.CancelBattlePrepare failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcCancelBattlePrepareFailure(std::shared_ptr<SceneNodeGrpcCancelBattlePrepareCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcCancelBattlePrepare(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcCancelBattlePrepareCall(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<SceneNodeGrpcCancelBattlePrepareCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcCancelBattlePrepareFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcCancelBattlePrepareGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcCancelBattlePrepareFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcCancelBattlePrepare(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.CancelBattlePrepare response";
            CompleteSceneNodeGrpcCancelBattlePrepare(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcCancelBattlePrepare(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcCancelBattlePrepareCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcCancelBattlePrepare(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcCancelBattlePrepareCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::CancelBattlePrepareRequest& derived = static_cast<const ::CancelBattlePrepareRequest&>(message);
    SendSceneNodeGrpcCancelBattlePrepare(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcAssetDebit
AsyncSceneNodeGrpcAssetDebitHandlerFunctionType AsyncSceneNodeGrpcAssetDebitHandler;
AsyncSceneNodeGrpcAssetDebitFailedHandlerFunctionType AsyncSceneNodeGrpcAssetDebitFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcAssetDebitCall {
    ::AssetOpRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcAssetDebitHandler / FailedHandler。
    GrpcDone<::AssetOpResponse> done;
};

void CompleteSceneNodeGrpcAssetDebit(const SceneNodeGrpcAssetDebitCall& call, const GrpcCallOutcome& outcome,
                                                const ::AssetOpResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcAssetDebitHandler) {
            AsyncSceneNodeGrpcAssetDebitHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.AssetDebit reply dropped: AsyncSceneNodeGrpcAssetDebitHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcAssetDebitFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcAssetDebitMessageId, "SceneNodeGrpc.AssetDebit", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcAssetDebitFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.AssetDebit failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcAssetDebitFailure(std::shared_ptr<SceneNodeGrpcAssetDebitCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcAssetDebit(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcAssetDebitCall(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AssetOpResponse> done) {
    auto call = std::make_shared<SceneNodeGrpcAssetDebitCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcAssetDebitFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcAssetDebitGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcAssetDebitFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcAssetDebit(*call, outcome, nullptr);
            return;
        }
        ::AssetOpResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.AssetDebit response";
            CompleteSceneNodeGrpcAssetDebit(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcAssetDebit(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                            GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcAssetDebitCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcAssetDebit(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcAssetDebitCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::AssetOpRequest& derived = static_cast<const ::AssetOpRequest&>(message);
    SendSceneNodeGrpcAssetDebit(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcAssetAbortDebit
AsyncSceneNodeGrpcAssetAbortDebitHandlerFunctionType AsyncSceneNodeGrpcAssetAbortDebitHandler;
AsyncSceneNodeGrpcAssetAbortDebitFailedHandlerFunctionType AsyncSceneNodeGrpcAssetAbortDebitFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcAssetAbortDebitCall {
    ::AssetOpRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcAssetAbortDebitHandler / FailedHandler。
    GrpcDone<::AssetOpResponse> done;
};

void CompleteSceneNodeGrpcAssetAbortDebit(const SceneNodeGrpcAssetAbortDebitCall& call, const GrpcCallOutcome& outcome,
                                                const ::AssetOpResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcAssetAbortDebitHandler) {
            AsyncSceneNodeGrpcAssetAbortDebitHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.AssetAbortDebit reply dropped: AsyncSceneNodeGrpcAssetAbortDebitHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcAssetAbortDebitFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcAssetAbortDebitMessageId, "SceneNodeGrpc.AssetAbortDebit", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcAssetAbortDebitFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.AssetAbortDebit failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcAssetAbortDebitFailure(std::shared_ptr<SceneNodeGrpcAssetAbortDebitCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcAssetAbortDebit(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcAssetAbortDebitCall(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AssetOpResponse> done) {
    auto call = std::make_shared<SceneNodeGrpcAssetAbortDebitCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcAssetAbortDebitFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcAssetAbortDebitGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcAssetAbortDebitFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcAssetAbortDebit(*call, outcome, nullptr);
            return;
        }
        ::AssetOpResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.AssetAbortDebit response";
            CompleteSceneNodeGrpcAssetAbortDebit(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcAssetAbortDebit(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                            GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcAssetAbortDebitCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcAssetAbortDebit(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcAssetAbortDebitCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::AssetOpRequest& derived = static_cast<const ::AssetOpRequest&>(message);
    SendSceneNodeGrpcAssetAbortDebit(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcAssetCredit
AsyncSceneNodeGrpcAssetCreditHandlerFunctionType AsyncSceneNodeGrpcAssetCreditHandler;
AsyncSceneNodeGrpcAssetCreditFailedHandlerFunctionType AsyncSceneNodeGrpcAssetCreditFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcAssetCreditCall {
    ::AssetOpRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcAssetCreditHandler / FailedHandler。
    GrpcDone<::AssetOpResponse> done;
};

void CompleteSceneNodeGrpcAssetCredit(const SceneNodeGrpcAssetCreditCall& call, const GrpcCallOutcome& outcome,
                                                const ::AssetOpResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcAssetCreditHandler) {
            AsyncSceneNodeGrpcAssetCreditHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.AssetCredit reply dropped: AsyncSceneNodeGrpcAssetCreditHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcAssetCreditFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcAssetCreditMessageId, "SceneNodeGrpc.AssetCredit", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcAssetCreditFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.AssetCredit failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcAssetCreditFailure(std::shared_ptr<SceneNodeGrpcAssetCreditCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcAssetCredit(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcAssetCreditCall(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AssetOpResponse> done) {
    auto call = std::make_shared<SceneNodeGrpcAssetCreditCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcAssetCreditFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcAssetCreditGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcAssetCreditFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcAssetCredit(*call, outcome, nullptr);
            return;
        }
        ::AssetOpResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.AssetCredit response";
            CompleteSceneNodeGrpcAssetCredit(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcAssetCredit(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                            GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcAssetCreditCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcAssetCredit(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcAssetCreditCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::AssetOpRequest& derived = static_cast<const ::AssetOpRequest&>(message);
    SendSceneNodeGrpcAssetCredit(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcAssetAckReceipt
AsyncSceneNodeGrpcAssetAckReceiptHandlerFunctionType AsyncSceneNodeGrpcAssetAckReceiptHandler;
AsyncSceneNodeGrpcAssetAckReceiptFailedHandlerFunctionType AsyncSceneNodeGrpcAssetAckReceiptFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneNodeGrpcAssetAckReceiptCall {
    ::AssetOpRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneNodeGrpcAssetAckReceiptHandler / FailedHandler。
    GrpcDone<::AssetOpResponse> done;
};

void CompleteSceneNodeGrpcAssetAckReceipt(const SceneNodeGrpcAssetAckReceiptCall& call, const GrpcCallOutcome& outcome,
                                                const ::AssetOpResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneNodeGrpcAssetAckReceiptHandler) {
            AsyncSceneNodeGrpcAssetAckReceiptHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneNodeGrpc.AssetAckReceipt reply dropped: AsyncSceneNodeGrpcAssetAckReceiptHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneNodeGrpcAssetAckReceiptFailedHandler) {
        const GrpcCallFailure failure{ SceneNodeGrpcAssetAckReceiptMessageId, "SceneNodeGrpc.AssetAckReceipt", outcome, call.sentMetadata};
        AsyncSceneNodeGrpcAssetAckReceiptFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.AssetAckReceipt failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneNodeGrpcAssetAckReceiptFailure(std::shared_ptr<SceneNodeGrpcAssetAckReceiptCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneNodeGrpcAssetAckReceipt(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcAssetAckReceiptCall(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AssetOpResponse> done) {
    auto call = std::make_shared<SceneNodeGrpcAssetAckReceiptCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneNodeGrpcAssetAckReceiptFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneNodeGrpcAssetAckReceiptGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneNodeGrpcAssetAckReceiptFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneNodeGrpcAssetAckReceipt(*call, outcome, nullptr);
            return;
        }
        ::AssetOpResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneNodeGrpc.AssetAckReceipt response";
            CompleteSceneNodeGrpcAssetAckReceipt(*call, outcome, nullptr);
            return;
        }
        CompleteSceneNodeGrpcAssetAckReceipt(*call, outcome, &reply);
    });
}

void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                            GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcAssetAckReceiptCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcAssetAckReceipt(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcAssetAckReceiptCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneNodeGrpcAssetAckReceipt(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::AssetOpRequest& derived = static_cast<const ::AssetOpRequest&>(message);
    SendSceneNodeGrpcAssetAckReceipt(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetSceneNodeServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncSceneNodeGrpcCreateSceneHandler = handler;
    AsyncSceneNodeGrpcDestroySceneHandler = handler;
    AsyncSceneNodeGrpcReleasePlayerHandler = handler;
    AsyncSceneNodeGrpcPrepareBattleHandler = handler;
    AsyncSceneNodeGrpcCancelBattlePrepareHandler = handler;
    AsyncSceneNodeGrpcAssetDebitHandler = handler;
    AsyncSceneNodeGrpcAssetAbortDebitHandler = handler;
    AsyncSceneNodeGrpcAssetCreditHandler = handler;
    AsyncSceneNodeGrpcAssetAckReceiptHandler = handler;
    (void)handler;
}

void SetSceneNodeServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncSceneNodeGrpcCreateSceneHandler) {
        AsyncSceneNodeGrpcCreateSceneHandler = handler;
    }
    if (!AsyncSceneNodeGrpcDestroySceneHandler) {
        AsyncSceneNodeGrpcDestroySceneHandler = handler;
    }
    if (!AsyncSceneNodeGrpcReleasePlayerHandler) {
        AsyncSceneNodeGrpcReleasePlayerHandler = handler;
    }
    if (!AsyncSceneNodeGrpcPrepareBattleHandler) {
        AsyncSceneNodeGrpcPrepareBattleHandler = handler;
    }
    if (!AsyncSceneNodeGrpcCancelBattlePrepareHandler) {
        AsyncSceneNodeGrpcCancelBattlePrepareHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetDebitHandler) {
        AsyncSceneNodeGrpcAssetDebitHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetAbortDebitHandler) {
        AsyncSceneNodeGrpcAssetAbortDebitHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetCreditHandler) {
        AsyncSceneNodeGrpcAssetCreditHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetAckReceiptHandler) {
        AsyncSceneNodeGrpcAssetAckReceiptHandler = handler;
    }
    (void)handler;
}

void SetSceneNodeServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncSceneNodeGrpcCreateSceneFailedHandler = handler;
    AsyncSceneNodeGrpcDestroySceneFailedHandler = handler;
    AsyncSceneNodeGrpcReleasePlayerFailedHandler = handler;
    AsyncSceneNodeGrpcPrepareBattleFailedHandler = handler;
    AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler = handler;
    AsyncSceneNodeGrpcAssetDebitFailedHandler = handler;
    AsyncSceneNodeGrpcAssetAbortDebitFailedHandler = handler;
    AsyncSceneNodeGrpcAssetCreditFailedHandler = handler;
    AsyncSceneNodeGrpcAssetAckReceiptFailedHandler = handler;
    (void)handler;
}

void SetSceneNodeServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncSceneNodeGrpcCreateSceneFailedHandler) {
        AsyncSceneNodeGrpcCreateSceneFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcDestroySceneFailedHandler) {
        AsyncSceneNodeGrpcDestroySceneFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcReleasePlayerFailedHandler) {
        AsyncSceneNodeGrpcReleasePlayerFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcPrepareBattleFailedHandler) {
        AsyncSceneNodeGrpcPrepareBattleFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler) {
        AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetDebitFailedHandler) {
        AsyncSceneNodeGrpcAssetDebitFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetAbortDebitFailedHandler) {
        AsyncSceneNodeGrpcAssetAbortDebitFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetCreditFailedHandler) {
        AsyncSceneNodeGrpcAssetCreditFailedHandler = handler;
    }
    if (!AsyncSceneNodeGrpcAssetAckReceiptFailedHandler) {
        AsyncSceneNodeGrpcAssetAckReceiptFailedHandler = handler;
    }
    (void)handler;
}

void SetSceneNodeServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace scene_node
