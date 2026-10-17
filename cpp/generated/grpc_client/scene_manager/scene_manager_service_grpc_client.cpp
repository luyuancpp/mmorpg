#include "muduo/base/Logging.h"

#include "scene_manager_service_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetSceneManagerServiceCallDeadline 按目标节点类型写入
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

namespace scene_manager {
#pragma region SceneManagerCreateScene
AsyncSceneManagerCreateSceneHandlerFunctionType AsyncSceneManagerCreateSceneHandler;
AsyncSceneManagerCreateSceneFailedHandlerFunctionType AsyncSceneManagerCreateSceneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneManagerCreateSceneCall {
    ::scene_manager::CreateSceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneManagerCreateSceneHandler / FailedHandler。
    GrpcDone<::scene_manager::CreateSceneResponse> done;
};

void CompleteSceneManagerCreateScene(const SceneManagerCreateSceneCall& call, const GrpcCallOutcome& outcome,
                                                const ::scene_manager::CreateSceneResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneManagerCreateSceneHandler) {
            AsyncSceneManagerCreateSceneHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneManager.CreateScene reply dropped: AsyncSceneManagerCreateSceneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneManagerCreateSceneFailedHandler) {
        const GrpcCallFailure failure{ SceneManagerCreateSceneMessageId, "SceneManager.CreateScene", outcome, call.sentMetadata};
        AsyncSceneManagerCreateSceneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneManager.CreateScene failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneManagerCreateSceneFailure(std::shared_ptr<SceneManagerCreateSceneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneManagerCreateScene(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerCreateSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::scene_manager::CreateSceneResponse> done) {
    auto call = std::make_shared<SceneManagerCreateSceneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneManagerCreateSceneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneManagerCreateSceneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneManagerCreateSceneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneManagerCreateScene(*call, outcome, nullptr);
            return;
        }
        ::scene_manager::CreateSceneResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneManager.CreateScene response";
            CompleteSceneManagerCreateScene(*call, outcome, nullptr);
            return;
        }
        CompleteSceneManagerCreateScene(*call, outcome, &reply);
    });
}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request,
                                            GrpcDone<::scene_manager::CreateSceneResponse> done, const GrpcCallOptions& options) {
    StartSceneManagerCreateSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerCreateScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerCreateSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::scene_manager::CreateSceneRequest& derived = static_cast<const ::scene_manager::CreateSceneRequest&>(message);
    SendSceneManagerCreateScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerDestroyScene
AsyncSceneManagerDestroySceneHandlerFunctionType AsyncSceneManagerDestroySceneHandler;
AsyncSceneManagerDestroySceneFailedHandlerFunctionType AsyncSceneManagerDestroySceneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneManagerDestroySceneCall {
    ::scene_manager::DestroySceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneManagerDestroySceneHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteSceneManagerDestroyScene(const SceneManagerDestroySceneCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneManagerDestroySceneHandler) {
            AsyncSceneManagerDestroySceneHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneManager.DestroyScene reply dropped: AsyncSceneManagerDestroySceneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneManagerDestroySceneFailedHandler) {
        const GrpcCallFailure failure{ SceneManagerDestroySceneMessageId, "SceneManager.DestroyScene", outcome, call.sentMetadata};
        AsyncSceneManagerDestroySceneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneManager.DestroyScene failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneManagerDestroySceneFailure(std::shared_ptr<SceneManagerDestroySceneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneManagerDestroyScene(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerDestroySceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<SceneManagerDestroySceneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneManagerDestroySceneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneManagerDestroySceneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneManagerDestroySceneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneManagerDestroyScene(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneManager.DestroyScene response";
            CompleteSceneManagerDestroyScene(*call, outcome, nullptr);
            return;
        }
        CompleteSceneManagerDestroyScene(*call, outcome, &reply);
    });
}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneManagerDestroySceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerDestroyScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerDestroySceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::scene_manager::DestroySceneRequest& derived = static_cast<const ::scene_manager::DestroySceneRequest&>(message);
    SendSceneManagerDestroyScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerEnterScene
AsyncSceneManagerEnterSceneHandlerFunctionType AsyncSceneManagerEnterSceneHandler;
AsyncSceneManagerEnterSceneFailedHandlerFunctionType AsyncSceneManagerEnterSceneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneManagerEnterSceneCall {
    ::scene_manager::EnterSceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneManagerEnterSceneHandler / FailedHandler。
    GrpcDone<::scene_manager::EnterSceneResponse> done;
};

void CompleteSceneManagerEnterScene(const SceneManagerEnterSceneCall& call, const GrpcCallOutcome& outcome,
                                                const ::scene_manager::EnterSceneResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneManagerEnterSceneHandler) {
            AsyncSceneManagerEnterSceneHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneManager.EnterScene reply dropped: AsyncSceneManagerEnterSceneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneManagerEnterSceneFailedHandler) {
        const GrpcCallFailure failure{ SceneManagerEnterSceneMessageId, "SceneManager.EnterScene", outcome, call.sentMetadata};
        AsyncSceneManagerEnterSceneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneManager.EnterScene failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneManagerEnterSceneFailure(std::shared_ptr<SceneManagerEnterSceneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneManagerEnterScene(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerEnterSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::scene_manager::EnterSceneResponse> done) {
    auto call = std::make_shared<SceneManagerEnterSceneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneManagerEnterSceneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneManagerEnterSceneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneManagerEnterSceneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneManagerEnterScene(*call, outcome, nullptr);
            return;
        }
        ::scene_manager::EnterSceneResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneManager.EnterScene response";
            CompleteSceneManagerEnterScene(*call, outcome, nullptr);
            return;
        }
        CompleteSceneManagerEnterScene(*call, outcome, &reply);
    });
}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request,
                                            GrpcDone<::scene_manager::EnterSceneResponse> done, const GrpcCallOptions& options) {
    StartSceneManagerEnterSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerEnterScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerEnterSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::scene_manager::EnterSceneRequest& derived = static_cast<const ::scene_manager::EnterSceneRequest&>(message);
    SendSceneManagerEnterScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerLeaveScene
AsyncSceneManagerLeaveSceneHandlerFunctionType AsyncSceneManagerLeaveSceneHandler;
AsyncSceneManagerLeaveSceneFailedHandlerFunctionType AsyncSceneManagerLeaveSceneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct SceneManagerLeaveSceneCall {
    ::scene_manager::LeaveSceneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncSceneManagerLeaveSceneHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteSceneManagerLeaveScene(const SceneManagerLeaveSceneCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncSceneManagerLeaveSceneHandler) {
            AsyncSceneManagerLeaveSceneHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC SceneManager.LeaveScene reply dropped: AsyncSceneManagerLeaveSceneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncSceneManagerLeaveSceneFailedHandler) {
        const GrpcCallFailure failure{ SceneManagerLeaveSceneMessageId, "SceneManager.LeaveScene", outcome, call.sentMetadata};
        AsyncSceneManagerLeaveSceneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC SceneManager.LeaveScene failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalSceneManagerLeaveSceneFailure(std::shared_ptr<SceneManagerLeaveSceneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteSceneManagerLeaveScene(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerLeaveSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<SceneManagerLeaveSceneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalSceneManagerLeaveSceneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kSceneManagerLeaveSceneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalSceneManagerLeaveSceneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteSceneManagerLeaveScene(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the SceneManager.LeaveScene response";
            CompleteSceneManagerLeaveScene(*call, outcome, nullptr);
            return;
        }
        CompleteSceneManagerLeaveScene(*call, outcome, &reply);
    });
}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneManagerLeaveSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerLeaveScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerLeaveSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::scene_manager::LeaveSceneRequest& derived = static_cast<const ::scene_manager::LeaveSceneRequest&>(message);
    SendSceneManagerLeaveScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetSceneManagerServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncSceneManagerCreateSceneHandler = handler;
    AsyncSceneManagerDestroySceneHandler = handler;
    AsyncSceneManagerEnterSceneHandler = handler;
    AsyncSceneManagerLeaveSceneHandler = handler;
    (void)handler;
}

void SetSceneManagerServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncSceneManagerCreateSceneHandler) {
        AsyncSceneManagerCreateSceneHandler = handler;
    }
    if (!AsyncSceneManagerDestroySceneHandler) {
        AsyncSceneManagerDestroySceneHandler = handler;
    }
    if (!AsyncSceneManagerEnterSceneHandler) {
        AsyncSceneManagerEnterSceneHandler = handler;
    }
    if (!AsyncSceneManagerLeaveSceneHandler) {
        AsyncSceneManagerLeaveSceneHandler = handler;
    }
    (void)handler;
}

void SetSceneManagerServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncSceneManagerCreateSceneFailedHandler = handler;
    AsyncSceneManagerDestroySceneFailedHandler = handler;
    AsyncSceneManagerEnterSceneFailedHandler = handler;
    AsyncSceneManagerLeaveSceneFailedHandler = handler;
    (void)handler;
}

void SetSceneManagerServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncSceneManagerCreateSceneFailedHandler) {
        AsyncSceneManagerCreateSceneFailedHandler = handler;
    }
    if (!AsyncSceneManagerDestroySceneFailedHandler) {
        AsyncSceneManagerDestroySceneFailedHandler = handler;
    }
    if (!AsyncSceneManagerEnterSceneFailedHandler) {
        AsyncSceneManagerEnterSceneFailedHandler = handler;
    }
    if (!AsyncSceneManagerLeaveSceneFailedHandler) {
        AsyncSceneManagerLeaveSceneFailedHandler = handler;
    }
    (void)handler;
}

void SetSceneManagerServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace scene_manager
