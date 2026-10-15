#include "muduo/base/Logging.h"

#include "team_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetTeamCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace teampb {
struct TeamCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region ClientPlayerTeamCreateTeam
boost::object_pool<AsyncClientPlayerTeamCreateTeamGrpcClient> ClientPlayerTeamCreateTeamPool;
using AsyncClientPlayerTeamCreateTeamHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamCreateTeamHandlerFunctionType AsyncClientPlayerTeamCreateTeamHandler;
AsyncClientPlayerTeamCreateTeamFailedHandlerFunctionType AsyncClientPlayerTeamCreateTeamFailedHandler;

void CompleteClientPlayerTeamCreateTeam(AsyncClientPlayerTeamCreateTeamGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamCreateTeamGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamCreateTeamPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamCreateTeamGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamCreateTeamHandler) {
            AsyncClientPlayerTeamCreateTeamHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.CreateTeam", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamCreateTeamFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.CreateTeam failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamCreateTeam(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamCreateTeamGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamCreateTeam(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamCreateTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamCreateTeamPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamCreateTeam(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCreateTeam(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamCreateTeamMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamCreateTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamCreateTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamCreateTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::CreateTeamRequest& derived = static_cast<const ::teampb::CreateTeamRequest&>(message);
    SendClientPlayerTeamCreateTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamGetMyTeam
boost::object_pool<AsyncClientPlayerTeamGetMyTeamGrpcClient> ClientPlayerTeamGetMyTeamPool;
using AsyncClientPlayerTeamGetMyTeamHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamGetMyTeamHandlerFunctionType AsyncClientPlayerTeamGetMyTeamHandler;
AsyncClientPlayerTeamGetMyTeamFailedHandlerFunctionType AsyncClientPlayerTeamGetMyTeamFailedHandler;

void CompleteClientPlayerTeamGetMyTeam(AsyncClientPlayerTeamGetMyTeamGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamGetMyTeamGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamGetMyTeamPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamGetMyTeamGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamGetMyTeamHandler) {
            AsyncClientPlayerTeamGetMyTeamHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.GetMyTeam", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamGetMyTeamFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.GetMyTeam failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamGetMyTeam(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamGetMyTeamGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamGetMyTeam(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamGetMyTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamGetMyTeamPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamGetMyTeam(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetMyTeam(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamGetMyTeamMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamGetMyTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamGetMyTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamGetMyTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::GetMyTeamRequest& derived = static_cast<const ::teampb::GetMyTeamRequest&>(message);
    SendClientPlayerTeamGetMyTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamApplyJoinTeam
boost::object_pool<AsyncClientPlayerTeamApplyJoinTeamGrpcClient> ClientPlayerTeamApplyJoinTeamPool;
using AsyncClientPlayerTeamApplyJoinTeamHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamApplyJoinTeamHandlerFunctionType AsyncClientPlayerTeamApplyJoinTeamHandler;
AsyncClientPlayerTeamApplyJoinTeamFailedHandlerFunctionType AsyncClientPlayerTeamApplyJoinTeamFailedHandler;

void CompleteClientPlayerTeamApplyJoinTeam(AsyncClientPlayerTeamApplyJoinTeamGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamApplyJoinTeamGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamApplyJoinTeamPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamApplyJoinTeamGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamApplyJoinTeamHandler) {
            AsyncClientPlayerTeamApplyJoinTeamHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.ApplyJoinTeam", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamApplyJoinTeamFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.ApplyJoinTeam failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamApplyJoinTeam(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamApplyJoinTeamGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamApplyJoinTeam(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamApplyJoinTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamApplyJoinTeamPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamApplyJoinTeam(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncApplyJoinTeam(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamApplyJoinTeamMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamApplyJoinTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamApplyJoinTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamApplyJoinTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::ApplyJoinTeamRequest& derived = static_cast<const ::teampb::ApplyJoinTeamRequest&>(message);
    SendClientPlayerTeamApplyJoinTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamHandleApplication
boost::object_pool<AsyncClientPlayerTeamHandleApplicationGrpcClient> ClientPlayerTeamHandleApplicationPool;
using AsyncClientPlayerTeamHandleApplicationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamHandleApplicationHandlerFunctionType AsyncClientPlayerTeamHandleApplicationHandler;
AsyncClientPlayerTeamHandleApplicationFailedHandlerFunctionType AsyncClientPlayerTeamHandleApplicationFailedHandler;

void CompleteClientPlayerTeamHandleApplication(AsyncClientPlayerTeamHandleApplicationGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamHandleApplicationGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamHandleApplicationPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamHandleApplicationGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamHandleApplicationHandler) {
            AsyncClientPlayerTeamHandleApplicationHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.HandleApplication", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamHandleApplicationFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.HandleApplication failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamHandleApplication(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamHandleApplicationGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamHandleApplication(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamHandleApplicationCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamHandleApplicationPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamHandleApplication(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncHandleApplication(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamHandleApplicationMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamHandleApplicationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamHandleApplication(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamHandleApplicationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::HandleApplicationRequest& derived = static_cast<const ::teampb::HandleApplicationRequest&>(message);
    SendClientPlayerTeamHandleApplication(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamInviteToTeam
boost::object_pool<AsyncClientPlayerTeamInviteToTeamGrpcClient> ClientPlayerTeamInviteToTeamPool;
using AsyncClientPlayerTeamInviteToTeamHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamInviteToTeamHandlerFunctionType AsyncClientPlayerTeamInviteToTeamHandler;
AsyncClientPlayerTeamInviteToTeamFailedHandlerFunctionType AsyncClientPlayerTeamInviteToTeamFailedHandler;

void CompleteClientPlayerTeamInviteToTeam(AsyncClientPlayerTeamInviteToTeamGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamInviteToTeamGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamInviteToTeamPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamInviteToTeamGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamInviteToTeamHandler) {
            AsyncClientPlayerTeamInviteToTeamHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.InviteToTeam", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamInviteToTeamFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.InviteToTeam failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamInviteToTeam(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamInviteToTeamGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamInviteToTeam(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamInviteToTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamInviteToTeamPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamInviteToTeam(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncInviteToTeam(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamInviteToTeamMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamInviteToTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamInviteToTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamInviteToTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::InviteToTeamRequest& derived = static_cast<const ::teampb::InviteToTeamRequest&>(message);
    SendClientPlayerTeamInviteToTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamRespondInvite
boost::object_pool<AsyncClientPlayerTeamRespondInviteGrpcClient> ClientPlayerTeamRespondInvitePool;
using AsyncClientPlayerTeamRespondInviteHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamRespondInviteHandlerFunctionType AsyncClientPlayerTeamRespondInviteHandler;
AsyncClientPlayerTeamRespondInviteFailedHandlerFunctionType AsyncClientPlayerTeamRespondInviteFailedHandler;

void CompleteClientPlayerTeamRespondInvite(AsyncClientPlayerTeamRespondInviteGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamRespondInviteGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamRespondInvitePool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamRespondInviteGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamRespondInviteHandler) {
            AsyncClientPlayerTeamRespondInviteHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.RespondInvite", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamRespondInviteFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.RespondInvite failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamRespondInvite(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamRespondInviteGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamRespondInvite(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamRespondInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamRespondInvitePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamRespondInvite(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRespondInvite(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamRespondInviteMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamRespondInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamRespondInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamRespondInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::RespondInviteRequest& derived = static_cast<const ::teampb::RespondInviteRequest&>(message);
    SendClientPlayerTeamRespondInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamListMyInvites
boost::object_pool<AsyncClientPlayerTeamListMyInvitesGrpcClient> ClientPlayerTeamListMyInvitesPool;
using AsyncClientPlayerTeamListMyInvitesHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::ListMyInvitesResponse&)>;
AsyncClientPlayerTeamListMyInvitesHandlerFunctionType AsyncClientPlayerTeamListMyInvitesHandler;
AsyncClientPlayerTeamListMyInvitesFailedHandlerFunctionType AsyncClientPlayerTeamListMyInvitesFailedHandler;

void CompleteClientPlayerTeamListMyInvites(AsyncClientPlayerTeamListMyInvitesGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamListMyInvitesGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamListMyInvitesPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamListMyInvitesGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamListMyInvitesHandler) {
            AsyncClientPlayerTeamListMyInvitesHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.ListMyInvites", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamListMyInvitesFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.ListMyInvites failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamListMyInvites(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamListMyInvitesGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamListMyInvites(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamListMyInvitesCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::ListMyInvitesResponse> done) {
    auto call(ClientPlayerTeamListMyInvitesPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamListMyInvites(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListMyInvites(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamListMyInvitesMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request,
                                            GrpcDone<::teampb::ListMyInvitesResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamListMyInvitesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamListMyInvites(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamListMyInvitesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::ListMyInvitesRequest& derived = static_cast<const ::teampb::ListMyInvitesRequest&>(message);
    SendClientPlayerTeamListMyInvites(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamLeaveTeam
boost::object_pool<AsyncClientPlayerTeamLeaveTeamGrpcClient> ClientPlayerTeamLeaveTeamPool;
using AsyncClientPlayerTeamLeaveTeamHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamLeaveTeamHandlerFunctionType AsyncClientPlayerTeamLeaveTeamHandler;
AsyncClientPlayerTeamLeaveTeamFailedHandlerFunctionType AsyncClientPlayerTeamLeaveTeamFailedHandler;

void CompleteClientPlayerTeamLeaveTeam(AsyncClientPlayerTeamLeaveTeamGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamLeaveTeamGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamLeaveTeamPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamLeaveTeamGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamLeaveTeamHandler) {
            AsyncClientPlayerTeamLeaveTeamHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.LeaveTeam", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamLeaveTeamFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.LeaveTeam failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamLeaveTeam(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamLeaveTeamGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamLeaveTeam(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamLeaveTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamLeaveTeamPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamLeaveTeam(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLeaveTeam(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamLeaveTeamMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamLeaveTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamLeaveTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamLeaveTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::LeaveTeamRequest& derived = static_cast<const ::teampb::LeaveTeamRequest&>(message);
    SendClientPlayerTeamLeaveTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamKickMember
boost::object_pool<AsyncClientPlayerTeamKickMemberGrpcClient> ClientPlayerTeamKickMemberPool;
using AsyncClientPlayerTeamKickMemberHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamKickMemberHandlerFunctionType AsyncClientPlayerTeamKickMemberHandler;
AsyncClientPlayerTeamKickMemberFailedHandlerFunctionType AsyncClientPlayerTeamKickMemberFailedHandler;

void CompleteClientPlayerTeamKickMember(AsyncClientPlayerTeamKickMemberGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamKickMemberGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamKickMemberPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamKickMemberGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamKickMemberHandler) {
            AsyncClientPlayerTeamKickMemberHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.KickMember", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamKickMemberFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.KickMember failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamKickMember(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamKickMemberGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamKickMember(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamKickMemberCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamKickMemberPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamKickMember(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncKickMember(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamKickMemberMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamKickMemberCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamKickMember(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamKickMemberCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::KickMemberRequest& derived = static_cast<const ::teampb::KickMemberRequest&>(message);
    SendClientPlayerTeamKickMember(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamTransferLeader
boost::object_pool<AsyncClientPlayerTeamTransferLeaderGrpcClient> ClientPlayerTeamTransferLeaderPool;
using AsyncClientPlayerTeamTransferLeaderHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamTransferLeaderHandlerFunctionType AsyncClientPlayerTeamTransferLeaderHandler;
AsyncClientPlayerTeamTransferLeaderFailedHandlerFunctionType AsyncClientPlayerTeamTransferLeaderFailedHandler;

void CompleteClientPlayerTeamTransferLeader(AsyncClientPlayerTeamTransferLeaderGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamTransferLeaderGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamTransferLeaderPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamTransferLeaderGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamTransferLeaderHandler) {
            AsyncClientPlayerTeamTransferLeaderHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.TransferLeader", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamTransferLeaderFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.TransferLeader failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamTransferLeader(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamTransferLeaderGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamTransferLeader(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamTransferLeaderCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamTransferLeaderPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamTransferLeader(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncTransferLeader(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamTransferLeaderMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamTransferLeaderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamTransferLeader(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamTransferLeaderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::TransferLeaderRequest& derived = static_cast<const ::teampb::TransferLeaderRequest&>(message);
    SendClientPlayerTeamTransferLeader(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamDisbandTeam
boost::object_pool<AsyncClientPlayerTeamDisbandTeamGrpcClient> ClientPlayerTeamDisbandTeamPool;
using AsyncClientPlayerTeamDisbandTeamHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamDisbandTeamHandlerFunctionType AsyncClientPlayerTeamDisbandTeamHandler;
AsyncClientPlayerTeamDisbandTeamFailedHandlerFunctionType AsyncClientPlayerTeamDisbandTeamFailedHandler;

void CompleteClientPlayerTeamDisbandTeam(AsyncClientPlayerTeamDisbandTeamGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamDisbandTeamGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamDisbandTeamPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamDisbandTeamGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamDisbandTeamHandler) {
            AsyncClientPlayerTeamDisbandTeamHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.DisbandTeam", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamDisbandTeamFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.DisbandTeam failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamDisbandTeam(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamDisbandTeamGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamDisbandTeam(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamDisbandTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamDisbandTeamPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamDisbandTeam(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDisbandTeam(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamDisbandTeamMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamDisbandTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamDisbandTeam(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamDisbandTeamCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::DisbandTeamRequest& derived = static_cast<const ::teampb::DisbandTeamRequest&>(message);
    SendClientPlayerTeamDisbandTeam(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamStartTeamMatch
boost::object_pool<AsyncClientPlayerTeamStartTeamMatchGrpcClient> ClientPlayerTeamStartTeamMatchPool;
using AsyncClientPlayerTeamStartTeamMatchHandlerFunctionType =
    std::function<void(const ClientContext&, const ::teampb::TeamResponse&)>;
AsyncClientPlayerTeamStartTeamMatchHandlerFunctionType AsyncClientPlayerTeamStartTeamMatchHandler;
AsyncClientPlayerTeamStartTeamMatchFailedHandlerFunctionType AsyncClientPlayerTeamStartTeamMatchFailedHandler;

void CompleteClientPlayerTeamStartTeamMatch(AsyncClientPlayerTeamStartTeamMatchGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamStartTeamMatchGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamStartTeamMatchPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamStartTeamMatchGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamStartTeamMatchHandler) {
            AsyncClientPlayerTeamStartTeamMatchHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.StartTeamMatch", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamStartTeamMatchFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.StartTeamMatch failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamStartTeamMatch(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamStartTeamMatchGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamStartTeamMatch(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamStartTeamMatchCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    auto call(ClientPlayerTeamStartTeamMatchPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamStartTeamMatch(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncStartTeamMatch(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamStartTeamMatchMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamStartTeamMatchCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamStartTeamMatch(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamStartTeamMatchCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::StartTeamMatchRequest& derived = static_cast<const ::teampb::StartTeamMatchRequest&>(message);
    SendClientPlayerTeamStartTeamMatch(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamSnapshot
boost::object_pool<AsyncClientPlayerTeamNotifyTeamSnapshotGrpcClient> ClientPlayerTeamNotifyTeamSnapshotPool;
using AsyncClientPlayerTeamNotifyTeamSnapshotHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncClientPlayerTeamNotifyTeamSnapshotHandlerFunctionType AsyncClientPlayerTeamNotifyTeamSnapshotHandler;
AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler;

void CompleteClientPlayerTeamNotifyTeamSnapshot(AsyncClientPlayerTeamNotifyTeamSnapshotGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamNotifyTeamSnapshotGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamNotifyTeamSnapshotPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamNotifyTeamSnapshotGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamNotifyTeamSnapshotHandler) {
            AsyncClientPlayerTeamNotifyTeamSnapshotHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.NotifyTeamSnapshot", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamNotifyTeamSnapshotFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamSnapshot failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamNotifyTeamSnapshot(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamNotifyTeamSnapshotGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamNotifyTeamSnapshot(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamNotifyTeamSnapshotCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(ClientPlayerTeamNotifyTeamSnapshotPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamNotifyTeamSnapshot(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyTeamSnapshot(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamNotifyTeamSnapshotMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamSnapshotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamNotifyTeamSnapshot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamNotifyTeamSnapshotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::TeamSnapshotS2C& derived = static_cast<const ::teampb::TeamSnapshotS2C&>(message);
    SendClientPlayerTeamNotifyTeamSnapshot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamInvite
boost::object_pool<AsyncClientPlayerTeamNotifyTeamInviteGrpcClient> ClientPlayerTeamNotifyTeamInvitePool;
using AsyncClientPlayerTeamNotifyTeamInviteHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncClientPlayerTeamNotifyTeamInviteHandlerFunctionType AsyncClientPlayerTeamNotifyTeamInviteHandler;
AsyncClientPlayerTeamNotifyTeamInviteFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamInviteFailedHandler;

void CompleteClientPlayerTeamNotifyTeamInvite(AsyncClientPlayerTeamNotifyTeamInviteGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamNotifyTeamInviteGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamNotifyTeamInvitePool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamNotifyTeamInviteGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamNotifyTeamInviteHandler) {
            AsyncClientPlayerTeamNotifyTeamInviteHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.NotifyTeamInvite", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamNotifyTeamInviteFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamInvite failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamNotifyTeamInvite(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamNotifyTeamInviteGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamNotifyTeamInvite(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamNotifyTeamInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(ClientPlayerTeamNotifyTeamInvitePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamNotifyTeamInvite(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyTeamInvite(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamNotifyTeamInviteMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamNotifyTeamInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamNotifyTeamInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::TeamInviteS2C& derived = static_cast<const ::teampb::TeamInviteS2C&>(message);
    SendClientPlayerTeamNotifyTeamInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamEvent
boost::object_pool<AsyncClientPlayerTeamNotifyTeamEventGrpcClient> ClientPlayerTeamNotifyTeamEventPool;
using AsyncClientPlayerTeamNotifyTeamEventHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncClientPlayerTeamNotifyTeamEventHandlerFunctionType AsyncClientPlayerTeamNotifyTeamEventHandler;
AsyncClientPlayerTeamNotifyTeamEventFailedHandlerFunctionType AsyncClientPlayerTeamNotifyTeamEventFailedHandler;

void CompleteClientPlayerTeamNotifyTeamEvent(AsyncClientPlayerTeamNotifyTeamEventGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerTeamNotifyTeamEventGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerTeamNotifyTeamEventPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerTeamNotifyTeamEventGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerTeamNotifyTeamEventHandler) {
            AsyncClientPlayerTeamNotifyTeamEventHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "ClientPlayerTeam.NotifyTeamEvent", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerTeamNotifyTeamEventFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamEvent failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerTeamNotifyTeamEvent(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerTeamNotifyTeamEventGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerTeamNotifyTeamEvent(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerTeamNotifyTeamEventCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(ClientPlayerTeamNotifyTeamEventPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerTeamStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerTeamNotifyTeamEvent(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyTeamEvent(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerTeamNotifyTeamEventMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamEventCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerTeamNotifyTeamEvent(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerTeamNotifyTeamEventCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::teampb::TeamEventS2C& derived = static_cast<const ::teampb::TeamEventS2C&>(message);
    SendClientPlayerTeamNotifyTeamEvent(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleTeamCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case ClientPlayerTeamCreateTeamMessageId:
            AsyncCompleteGrpcClientPlayerTeamCreateTeam(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamGetMyTeamMessageId:
            AsyncCompleteGrpcClientPlayerTeamGetMyTeam(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamApplyJoinTeamMessageId:
            AsyncCompleteGrpcClientPlayerTeamApplyJoinTeam(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamHandleApplicationMessageId:
            AsyncCompleteGrpcClientPlayerTeamHandleApplication(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamInviteToTeamMessageId:
            AsyncCompleteGrpcClientPlayerTeamInviteToTeam(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamRespondInviteMessageId:
            AsyncCompleteGrpcClientPlayerTeamRespondInvite(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamListMyInvitesMessageId:
            AsyncCompleteGrpcClientPlayerTeamListMyInvites(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamLeaveTeamMessageId:
            AsyncCompleteGrpcClientPlayerTeamLeaveTeam(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamKickMemberMessageId:
            AsyncCompleteGrpcClientPlayerTeamKickMember(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamTransferLeaderMessageId:
            AsyncCompleteGrpcClientPlayerTeamTransferLeader(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamDisbandTeamMessageId:
            AsyncCompleteGrpcClientPlayerTeamDisbandTeam(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamStartTeamMatchMessageId:
            AsyncCompleteGrpcClientPlayerTeamStartTeamMatch(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamNotifyTeamSnapshotMessageId:
            AsyncCompleteGrpcClientPlayerTeamNotifyTeamSnapshot(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamNotifyTeamInviteMessageId:
            AsyncCompleteGrpcClientPlayerTeamNotifyTeamInvite(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerTeamNotifyTeamEventMessageId:
            AsyncCompleteGrpcClientPlayerTeamNotifyTeamEvent(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetTeamHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

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
}

void SetTeamIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

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
}

void SetTeamCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitTeamGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<ClientPlayerTeamStubPtr>(nodeEntity, ClientPlayerTeam::NewStub(channel));

}

}// namespace teampb
