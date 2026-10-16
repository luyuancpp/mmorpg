#include "muduo/base/Logging.h"

#include "scene_node_service_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetSceneNodeServiceCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace scene_node {
struct SceneNodeServiceCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region SceneNodeGrpcCreateScene
boost::object_pool<AsyncSceneNodeGrpcCreateSceneGrpcClient> SceneNodeGrpcCreateScenePool;
using AsyncSceneNodeGrpcCreateSceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::CreateSceneResponse&)>;
AsyncSceneNodeGrpcCreateSceneHandlerFunctionType AsyncSceneNodeGrpcCreateSceneHandler;
AsyncSceneNodeGrpcCreateSceneFailedHandlerFunctionType AsyncSceneNodeGrpcCreateSceneFailedHandler;

void CompleteSceneNodeGrpcCreateScene(AsyncSceneNodeGrpcCreateSceneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcCreateSceneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcCreateScenePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcCreateSceneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcCreateSceneHandler) {
            AsyncSceneNodeGrpcCreateSceneHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.CreateScene", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcCreateSceneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.CreateScene failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcCreateScene(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcCreateSceneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcCreateScene(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcCreateSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::CreateSceneResponse> done) {
    auto call(SceneNodeGrpcCreateScenePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcCreateScene(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCreateScene(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcCreateSceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request,
                                            GrpcDone<::CreateSceneResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcCreateSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcCreateScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcCreateSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::CreateSceneRequest& derived = static_cast<const ::CreateSceneRequest&>(message);
    SendSceneNodeGrpcCreateScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcDestroyScene
boost::object_pool<AsyncSceneNodeGrpcDestroySceneGrpcClient> SceneNodeGrpcDestroyScenePool;
using AsyncSceneNodeGrpcDestroySceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncSceneNodeGrpcDestroySceneHandlerFunctionType AsyncSceneNodeGrpcDestroySceneHandler;
AsyncSceneNodeGrpcDestroySceneFailedHandlerFunctionType AsyncSceneNodeGrpcDestroySceneFailedHandler;

void CompleteSceneNodeGrpcDestroyScene(AsyncSceneNodeGrpcDestroySceneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcDestroySceneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcDestroyScenePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcDestroySceneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcDestroySceneHandler) {
            AsyncSceneNodeGrpcDestroySceneHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.DestroyScene", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcDestroySceneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.DestroyScene failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcDestroyScene(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcDestroySceneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcDestroyScene(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcDestroySceneCall(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(SceneNodeGrpcDestroyScenePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcDestroyScene(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDestroyScene(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcDestroySceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcDestroySceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcDestroyScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcDestroySceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::DestroySceneRequest& derived = static_cast<const ::DestroySceneRequest&>(message);
    SendSceneNodeGrpcDestroyScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcReleasePlayer
boost::object_pool<AsyncSceneNodeGrpcReleasePlayerGrpcClient> SceneNodeGrpcReleasePlayerPool;
using AsyncSceneNodeGrpcReleasePlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncSceneNodeGrpcReleasePlayerHandlerFunctionType AsyncSceneNodeGrpcReleasePlayerHandler;
AsyncSceneNodeGrpcReleasePlayerFailedHandlerFunctionType AsyncSceneNodeGrpcReleasePlayerFailedHandler;

void CompleteSceneNodeGrpcReleasePlayer(AsyncSceneNodeGrpcReleasePlayerGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcReleasePlayerGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcReleasePlayerPool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcReleasePlayerGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcReleasePlayerHandler) {
            AsyncSceneNodeGrpcReleasePlayerHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.ReleasePlayer", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcReleasePlayerFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.ReleasePlayer failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcReleasePlayer(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcReleasePlayerGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcReleasePlayer(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcReleasePlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(SceneNodeGrpcReleasePlayerPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcReleasePlayer(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncReleasePlayer(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcReleasePlayerMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcReleasePlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcReleasePlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const ::scene_node::ReleasePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcReleasePlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcReleasePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_node::ReleasePlayerRequest& derived = static_cast<const ::scene_node::ReleasePlayerRequest&>(message);
    SendSceneNodeGrpcReleasePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcPrepareBattle
boost::object_pool<AsyncSceneNodeGrpcPrepareBattleGrpcClient> SceneNodeGrpcPrepareBattlePool;
using AsyncSceneNodeGrpcPrepareBattleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::PrepareBattleResponse&)>;
AsyncSceneNodeGrpcPrepareBattleHandlerFunctionType AsyncSceneNodeGrpcPrepareBattleHandler;
AsyncSceneNodeGrpcPrepareBattleFailedHandlerFunctionType AsyncSceneNodeGrpcPrepareBattleFailedHandler;

void CompleteSceneNodeGrpcPrepareBattle(AsyncSceneNodeGrpcPrepareBattleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcPrepareBattleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcPrepareBattlePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcPrepareBattleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcPrepareBattleHandler) {
            AsyncSceneNodeGrpcPrepareBattleHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.PrepareBattle", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcPrepareBattleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.PrepareBattle failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcPrepareBattle(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcPrepareBattleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcPrepareBattle(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcPrepareBattleCall(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::PrepareBattleResponse> done) {
    auto call(SceneNodeGrpcPrepareBattlePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcPrepareBattle(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncPrepareBattle(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcPrepareBattleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request,
                                            GrpcDone<::PrepareBattleResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcPrepareBattleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcPrepareBattle(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const ::PrepareBattleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcPrepareBattleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcPrepareBattle(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::PrepareBattleRequest& derived = static_cast<const ::PrepareBattleRequest&>(message);
    SendSceneNodeGrpcPrepareBattle(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcCancelBattlePrepare
boost::object_pool<AsyncSceneNodeGrpcCancelBattlePrepareGrpcClient> SceneNodeGrpcCancelBattlePreparePool;
using AsyncSceneNodeGrpcCancelBattlePrepareHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncSceneNodeGrpcCancelBattlePrepareHandlerFunctionType AsyncSceneNodeGrpcCancelBattlePrepareHandler;
AsyncSceneNodeGrpcCancelBattlePrepareFailedHandlerFunctionType AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler;

void CompleteSceneNodeGrpcCancelBattlePrepare(AsyncSceneNodeGrpcCancelBattlePrepareGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcCancelBattlePrepareGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcCancelBattlePreparePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcCancelBattlePrepareGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcCancelBattlePrepareHandler) {
            AsyncSceneNodeGrpcCancelBattlePrepareHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.CancelBattlePrepare", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcCancelBattlePrepareFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.CancelBattlePrepare failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcCancelBattlePrepare(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcCancelBattlePrepareGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcCancelBattlePrepare(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcCancelBattlePrepareCall(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(SceneNodeGrpcCancelBattlePreparePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcCancelBattlePrepare(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCancelBattlePrepare(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcCancelBattlePrepareMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcCancelBattlePrepareCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcCancelBattlePrepare(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const ::CancelBattlePrepareRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcCancelBattlePrepareCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcCancelBattlePrepare(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::CancelBattlePrepareRequest& derived = static_cast<const ::CancelBattlePrepareRequest&>(message);
    SendSceneNodeGrpcCancelBattlePrepare(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcAssetDebit
boost::object_pool<AsyncSceneNodeGrpcAssetDebitGrpcClient> SceneNodeGrpcAssetDebitPool;
using AsyncSceneNodeGrpcAssetDebitHandlerFunctionType =
    std::function<void(const ClientContext&, const ::AssetOpResponse&)>;
AsyncSceneNodeGrpcAssetDebitHandlerFunctionType AsyncSceneNodeGrpcAssetDebitHandler;
AsyncSceneNodeGrpcAssetDebitFailedHandlerFunctionType AsyncSceneNodeGrpcAssetDebitFailedHandler;

void CompleteSceneNodeGrpcAssetDebit(AsyncSceneNodeGrpcAssetDebitGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcAssetDebitGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcAssetDebitPool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcAssetDebitGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcAssetDebitHandler) {
            AsyncSceneNodeGrpcAssetDebitHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.AssetDebit", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcAssetDebitFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.AssetDebit failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcAssetDebit(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcAssetDebitGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcAssetDebit(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcAssetDebitCall(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AssetOpResponse> done) {
    auto call(SceneNodeGrpcAssetDebitPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcAssetDebit(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAssetDebit(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcAssetDebitMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                            GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcAssetDebitCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcAssetDebit(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcAssetDebitCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcAssetDebit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::AssetOpRequest& derived = static_cast<const ::AssetOpRequest&>(message);
    SendSceneNodeGrpcAssetDebit(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcAssetAbortDebit
boost::object_pool<AsyncSceneNodeGrpcAssetAbortDebitGrpcClient> SceneNodeGrpcAssetAbortDebitPool;
using AsyncSceneNodeGrpcAssetAbortDebitHandlerFunctionType =
    std::function<void(const ClientContext&, const ::AssetOpResponse&)>;
AsyncSceneNodeGrpcAssetAbortDebitHandlerFunctionType AsyncSceneNodeGrpcAssetAbortDebitHandler;
AsyncSceneNodeGrpcAssetAbortDebitFailedHandlerFunctionType AsyncSceneNodeGrpcAssetAbortDebitFailedHandler;

void CompleteSceneNodeGrpcAssetAbortDebit(AsyncSceneNodeGrpcAssetAbortDebitGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcAssetAbortDebitGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcAssetAbortDebitPool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcAssetAbortDebitGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcAssetAbortDebitHandler) {
            AsyncSceneNodeGrpcAssetAbortDebitHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.AssetAbortDebit", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcAssetAbortDebitFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.AssetAbortDebit failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcAssetAbortDebit(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcAssetAbortDebitGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcAssetAbortDebit(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcAssetAbortDebitCall(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AssetOpResponse> done) {
    auto call(SceneNodeGrpcAssetAbortDebitPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcAssetAbortDebit(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAssetAbortDebit(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcAssetAbortDebitMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                            GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcAssetAbortDebitCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcAssetAbortDebit(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcAssetAbortDebitCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcAssetAbortDebit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::AssetOpRequest& derived = static_cast<const ::AssetOpRequest&>(message);
    SendSceneNodeGrpcAssetAbortDebit(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneNodeGrpcAssetCredit
boost::object_pool<AsyncSceneNodeGrpcAssetCreditGrpcClient> SceneNodeGrpcAssetCreditPool;
using AsyncSceneNodeGrpcAssetCreditHandlerFunctionType =
    std::function<void(const ClientContext&, const ::AssetOpResponse&)>;
AsyncSceneNodeGrpcAssetCreditHandlerFunctionType AsyncSceneNodeGrpcAssetCreditHandler;
AsyncSceneNodeGrpcAssetCreditFailedHandlerFunctionType AsyncSceneNodeGrpcAssetCreditFailedHandler;

void CompleteSceneNodeGrpcAssetCredit(AsyncSceneNodeGrpcAssetCreditGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneNodeGrpcAssetCreditGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneNodeGrpcAssetCreditPool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneNodeGrpcAssetCreditGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneNodeGrpcAssetCreditHandler) {
            AsyncSceneNodeGrpcAssetCreditHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneNodeGrpc.AssetCredit", call->context, call->status, call->sentMetadata};
        AsyncSceneNodeGrpcAssetCreditFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneNodeGrpc.AssetCredit failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneNodeGrpcAssetCredit(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneNodeGrpcAssetCreditGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneNodeGrpcAssetCredit(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneNodeGrpcAssetCreditCall(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::AssetOpResponse> done) {
    auto call(SceneNodeGrpcAssetCreditPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneNodeGrpcStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneNodeGrpcAssetCredit(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAssetCredit(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneNodeGrpcAssetCreditMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request,
                                            GrpcDone<::AssetOpResponse> done, const GrpcCallOptions& options) {
    StartSceneNodeGrpcAssetCreditCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneNodeGrpcAssetCredit(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const ::AssetOpRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneNodeGrpcAssetCreditCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneNodeGrpcAssetCredit(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::AssetOpRequest& derived = static_cast<const ::AssetOpRequest&>(message);
    SendSceneNodeGrpcAssetCredit(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleSceneNodeServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case SceneNodeGrpcCreateSceneMessageId:
            AsyncCompleteGrpcSceneNodeGrpcCreateScene(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneNodeGrpcDestroySceneMessageId:
            AsyncCompleteGrpcSceneNodeGrpcDestroyScene(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneNodeGrpcReleasePlayerMessageId:
            AsyncCompleteGrpcSceneNodeGrpcReleasePlayer(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneNodeGrpcPrepareBattleMessageId:
            AsyncCompleteGrpcSceneNodeGrpcPrepareBattle(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneNodeGrpcCancelBattlePrepareMessageId:
            AsyncCompleteGrpcSceneNodeGrpcCancelBattlePrepare(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneNodeGrpcAssetDebitMessageId:
            AsyncCompleteGrpcSceneNodeGrpcAssetDebit(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneNodeGrpcAssetAbortDebitMessageId:
            AsyncCompleteGrpcSceneNodeGrpcAssetAbortDebit(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneNodeGrpcAssetCreditMessageId:
            AsyncCompleteGrpcSceneNodeGrpcAssetCredit(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetSceneNodeServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncSceneNodeGrpcCreateSceneHandler = handler;
    AsyncSceneNodeGrpcDestroySceneHandler = handler;
    AsyncSceneNodeGrpcReleasePlayerHandler = handler;
    AsyncSceneNodeGrpcPrepareBattleHandler = handler;
    AsyncSceneNodeGrpcCancelBattlePrepareHandler = handler;
    AsyncSceneNodeGrpcAssetDebitHandler = handler;
    AsyncSceneNodeGrpcAssetAbortDebitHandler = handler;
    AsyncSceneNodeGrpcAssetCreditHandler = handler;
}

void SetSceneNodeServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

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
}

void SetSceneNodeServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitSceneNodeServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<SceneNodeGrpcStubPtr>(nodeEntity, SceneNodeGrpc::NewStub(channel));

}

}// namespace scene_node
