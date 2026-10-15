#include "muduo/base/Logging.h"

#include "scene_manager_service_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetSceneManagerServiceCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace scene_manager {
struct SceneManagerServiceCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region SceneManagerCreateScene
boost::object_pool<AsyncSceneManagerCreateSceneGrpcClient> SceneManagerCreateScenePool;
using AsyncSceneManagerCreateSceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::CreateSceneResponse&)>;
AsyncSceneManagerCreateSceneHandlerFunctionType AsyncSceneManagerCreateSceneHandler;
AsyncSceneManagerCreateSceneFailedHandlerFunctionType AsyncSceneManagerCreateSceneFailedHandler;

void CompleteSceneManagerCreateScene(AsyncSceneManagerCreateSceneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneManagerCreateSceneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneManagerCreateScenePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneManagerCreateSceneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneManagerCreateSceneHandler) {
            AsyncSceneManagerCreateSceneHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneManager.CreateScene", call->context, call->status, call->sentMetadata};
        AsyncSceneManagerCreateSceneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneManager.CreateScene failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneManagerCreateScene(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneManagerCreateSceneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneManagerCreateScene(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerCreateSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::scene_manager::CreateSceneResponse> done) {
    auto call(SceneManagerCreateScenePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneManagerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneManagerCreateScene(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCreateScene(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerCreateSceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request,
                                            GrpcDone<::scene_manager::CreateSceneResponse> done, const GrpcCallOptions& options) {
    StartSceneManagerCreateSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerCreateScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerCreateSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::CreateSceneRequest& derived = static_cast<const ::scene_manager::CreateSceneRequest&>(message);
    SendSceneManagerCreateScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerDestroyScene
boost::object_pool<AsyncSceneManagerDestroySceneGrpcClient> SceneManagerDestroyScenePool;
using AsyncSceneManagerDestroySceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncSceneManagerDestroySceneHandlerFunctionType AsyncSceneManagerDestroySceneHandler;
AsyncSceneManagerDestroySceneFailedHandlerFunctionType AsyncSceneManagerDestroySceneFailedHandler;

void CompleteSceneManagerDestroyScene(AsyncSceneManagerDestroySceneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneManagerDestroySceneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneManagerDestroyScenePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneManagerDestroySceneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneManagerDestroySceneHandler) {
            AsyncSceneManagerDestroySceneHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneManager.DestroyScene", call->context, call->status, call->sentMetadata};
        AsyncSceneManagerDestroySceneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneManager.DestroyScene failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneManagerDestroyScene(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneManagerDestroySceneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneManagerDestroyScene(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerDestroySceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(SceneManagerDestroyScenePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneManagerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneManagerDestroyScene(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDestroyScene(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerDestroySceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneManagerDestroySceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerDestroyScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerDestroySceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::DestroySceneRequest& derived = static_cast<const ::scene_manager::DestroySceneRequest&>(message);
    SendSceneManagerDestroyScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerEnterScene
boost::object_pool<AsyncSceneManagerEnterSceneGrpcClient> SceneManagerEnterScenePool;
using AsyncSceneManagerEnterSceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::EnterSceneResponse&)>;
AsyncSceneManagerEnterSceneHandlerFunctionType AsyncSceneManagerEnterSceneHandler;
AsyncSceneManagerEnterSceneFailedHandlerFunctionType AsyncSceneManagerEnterSceneFailedHandler;

void CompleteSceneManagerEnterScene(AsyncSceneManagerEnterSceneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneManagerEnterSceneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneManagerEnterScenePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneManagerEnterSceneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneManagerEnterSceneHandler) {
            AsyncSceneManagerEnterSceneHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneManager.EnterScene", call->context, call->status, call->sentMetadata};
        AsyncSceneManagerEnterSceneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneManager.EnterScene failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneManagerEnterScene(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneManagerEnterSceneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneManagerEnterScene(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerEnterSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::scene_manager::EnterSceneResponse> done) {
    auto call(SceneManagerEnterScenePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneManagerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneManagerEnterScene(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncEnterScene(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerEnterSceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request,
                                            GrpcDone<::scene_manager::EnterSceneResponse> done, const GrpcCallOptions& options) {
    StartSceneManagerEnterSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerEnterScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerEnterSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneManagerEnterScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::EnterSceneRequest& derived = static_cast<const ::scene_manager::EnterSceneRequest&>(message);
    SendSceneManagerEnterScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerLeaveScene
boost::object_pool<AsyncSceneManagerLeaveSceneGrpcClient> SceneManagerLeaveScenePool;
using AsyncSceneManagerLeaveSceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncSceneManagerLeaveSceneHandlerFunctionType AsyncSceneManagerLeaveSceneHandler;
AsyncSceneManagerLeaveSceneFailedHandlerFunctionType AsyncSceneManagerLeaveSceneFailedHandler;

void CompleteSceneManagerLeaveScene(AsyncSceneManagerLeaveSceneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncSceneManagerLeaveSceneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        SceneManagerLeaveScenePool.destroy(completed);
    };
    const std::unique_ptr<AsyncSceneManagerLeaveSceneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncSceneManagerLeaveSceneHandler) {
            AsyncSceneManagerLeaveSceneHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "SceneManager.LeaveScene", call->context, call->status, call->sentMetadata};
        AsyncSceneManagerLeaveSceneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC SceneManager.LeaveScene failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcSceneManagerLeaveScene(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncSceneManagerLeaveSceneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteSceneManagerLeaveScene(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartSceneManagerLeaveSceneCall(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(SceneManagerLeaveScenePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<SceneManagerStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteSceneManagerLeaveScene(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLeaveScene(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerLeaveSceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartSceneManagerLeaveSceneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendSceneManagerLeaveScene(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartSceneManagerLeaveSceneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendSceneManagerLeaveScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::LeaveSceneRequest& derived = static_cast<const ::scene_manager::LeaveSceneRequest&>(message);
    SendSceneManagerLeaveScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleSceneManagerServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case SceneManagerCreateSceneMessageId:
            AsyncCompleteGrpcSceneManagerCreateScene(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneManagerDestroySceneMessageId:
            AsyncCompleteGrpcSceneManagerDestroyScene(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneManagerEnterSceneMessageId:
            AsyncCompleteGrpcSceneManagerEnterScene(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case SceneManagerLeaveSceneMessageId:
            AsyncCompleteGrpcSceneManagerLeaveScene(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetSceneManagerServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncSceneManagerCreateSceneHandler = handler;
    AsyncSceneManagerDestroySceneHandler = handler;
    AsyncSceneManagerEnterSceneHandler = handler;
    AsyncSceneManagerLeaveSceneHandler = handler;
}

void SetSceneManagerServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

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
}

void SetSceneManagerServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncSceneManagerCreateSceneFailedHandler = handler;
    AsyncSceneManagerDestroySceneFailedHandler = handler;
    AsyncSceneManagerEnterSceneFailedHandler = handler;
    AsyncSceneManagerLeaveSceneFailedHandler = handler;
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
}

void SetSceneManagerServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitSceneManagerServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<SceneManagerStubPtr>(nodeEntity, SceneManager::NewStub(channel));

}

}// namespace scene_manager
