#include "muduo/base/Logging.h"

#include "guild_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetGuildCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace guildpb {
struct GuildCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region GuildServiceCreateGuild
boost::object_pool<AsyncGuildServiceCreateGuildGrpcClient> GuildServiceCreateGuildPool;
using AsyncGuildServiceCreateGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::CreateGuildResponse&)>;
AsyncGuildServiceCreateGuildHandlerFunctionType AsyncGuildServiceCreateGuildHandler;
AsyncGuildServiceCreateGuildFailedHandlerFunctionType AsyncGuildServiceCreateGuildFailedHandler;

void CompleteGuildServiceCreateGuild(AsyncGuildServiceCreateGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceCreateGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceCreateGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceCreateGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceCreateGuildHandler) {
            AsyncGuildServiceCreateGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.CreateGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceCreateGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.CreateGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceCreateGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceCreateGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceCreateGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceCreateGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::CreateGuildResponse> done) {
    auto call(GuildServiceCreateGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceCreateGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCreateGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceCreateGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request,
                                            GrpcDone<::guildpb::CreateGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceCreateGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceCreateGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceCreateGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::CreateGuildRequest& derived = static_cast<const ::guildpb::CreateGuildRequest&>(message);
    SendGuildServiceCreateGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuild
boost::object_pool<AsyncGuildServiceGetGuildGrpcClient> GuildServiceGetGuildPool;
using AsyncGuildServiceGetGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildResponse&)>;
AsyncGuildServiceGetGuildHandlerFunctionType AsyncGuildServiceGetGuildHandler;
AsyncGuildServiceGetGuildFailedHandlerFunctionType AsyncGuildServiceGetGuildFailedHandler;

void CompleteGuildServiceGetGuild(AsyncGuildServiceGetGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceGetGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceGetGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceGetGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceGetGuildHandler) {
            AsyncGuildServiceGetGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.GetGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceGetGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceGetGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceGetGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceGetGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildResponse> done) {
    auto call(GuildServiceGetGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceGetGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceGetGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request,
                                            GrpcDone<::guildpb::GetGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GetGuildRequest& derived = static_cast<const ::guildpb::GetGuildRequest&>(message);
    SendGuildServiceGetGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetPlayerGuild
boost::object_pool<AsyncGuildServiceGetPlayerGuildGrpcClient> GuildServiceGetPlayerGuildPool;
using AsyncGuildServiceGetPlayerGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetPlayerGuildResponse&)>;
AsyncGuildServiceGetPlayerGuildHandlerFunctionType AsyncGuildServiceGetPlayerGuildHandler;
AsyncGuildServiceGetPlayerGuildFailedHandlerFunctionType AsyncGuildServiceGetPlayerGuildFailedHandler;

void CompleteGuildServiceGetPlayerGuild(AsyncGuildServiceGetPlayerGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceGetPlayerGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceGetPlayerGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceGetPlayerGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceGetPlayerGuildHandler) {
            AsyncGuildServiceGetPlayerGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.GetPlayerGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceGetPlayerGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetPlayerGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceGetPlayerGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceGetPlayerGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceGetPlayerGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetPlayerGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetPlayerGuildResponse> done) {
    auto call(GuildServiceGetPlayerGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceGetPlayerGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetPlayerGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceGetPlayerGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request,
                                            GrpcDone<::guildpb::GetPlayerGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetPlayerGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetPlayerGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetPlayerGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GetPlayerGuildRequest& derived = static_cast<const ::guildpb::GetPlayerGuildRequest&>(message);
    SendGuildServiceGetPlayerGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceLeaveGuild
boost::object_pool<AsyncGuildServiceLeaveGuildGrpcClient> GuildServiceLeaveGuildPool;
using AsyncGuildServiceLeaveGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::LeaveGuildResponse&)>;
AsyncGuildServiceLeaveGuildHandlerFunctionType AsyncGuildServiceLeaveGuildHandler;
AsyncGuildServiceLeaveGuildFailedHandlerFunctionType AsyncGuildServiceLeaveGuildFailedHandler;

void CompleteGuildServiceLeaveGuild(AsyncGuildServiceLeaveGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceLeaveGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceLeaveGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceLeaveGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceLeaveGuildHandler) {
            AsyncGuildServiceLeaveGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.LeaveGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceLeaveGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.LeaveGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceLeaveGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceLeaveGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceLeaveGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceLeaveGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::LeaveGuildResponse> done) {
    auto call(GuildServiceLeaveGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceLeaveGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLeaveGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceLeaveGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request,
                                            GrpcDone<::guildpb::LeaveGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceLeaveGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceLeaveGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceLeaveGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::LeaveGuildRequest& derived = static_cast<const ::guildpb::LeaveGuildRequest&>(message);
    SendGuildServiceLeaveGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceDisbandGuild
boost::object_pool<AsyncGuildServiceDisbandGuildGrpcClient> GuildServiceDisbandGuildPool;
using AsyncGuildServiceDisbandGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::DisbandGuildResponse&)>;
AsyncGuildServiceDisbandGuildHandlerFunctionType AsyncGuildServiceDisbandGuildHandler;
AsyncGuildServiceDisbandGuildFailedHandlerFunctionType AsyncGuildServiceDisbandGuildFailedHandler;

void CompleteGuildServiceDisbandGuild(AsyncGuildServiceDisbandGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceDisbandGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceDisbandGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceDisbandGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceDisbandGuildHandler) {
            AsyncGuildServiceDisbandGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.DisbandGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceDisbandGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.DisbandGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceDisbandGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceDisbandGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceDisbandGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceDisbandGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::DisbandGuildResponse> done) {
    auto call(GuildServiceDisbandGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceDisbandGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDisbandGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceDisbandGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request,
                                            GrpcDone<::guildpb::DisbandGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceDisbandGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceDisbandGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceDisbandGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::DisbandGuildRequest& derived = static_cast<const ::guildpb::DisbandGuildRequest&>(message);
    SendGuildServiceDisbandGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceSetAnnouncement
boost::object_pool<AsyncGuildServiceSetAnnouncementGrpcClient> GuildServiceSetAnnouncementPool;
using AsyncGuildServiceSetAnnouncementHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::SetAnnouncementResponse&)>;
AsyncGuildServiceSetAnnouncementHandlerFunctionType AsyncGuildServiceSetAnnouncementHandler;
AsyncGuildServiceSetAnnouncementFailedHandlerFunctionType AsyncGuildServiceSetAnnouncementFailedHandler;

void CompleteGuildServiceSetAnnouncement(AsyncGuildServiceSetAnnouncementGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceSetAnnouncementGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceSetAnnouncementPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceSetAnnouncementGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceSetAnnouncementHandler) {
            AsyncGuildServiceSetAnnouncementHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.SetAnnouncement", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceSetAnnouncementFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.SetAnnouncement failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceSetAnnouncement(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceSetAnnouncementGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceSetAnnouncement(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceSetAnnouncementCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::SetAnnouncementResponse> done) {
    auto call(GuildServiceSetAnnouncementPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceSetAnnouncement(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSetAnnouncement(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceSetAnnouncementMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request,
                                            GrpcDone<::guildpb::SetAnnouncementResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceSetAnnouncementCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceSetAnnouncement(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceSetAnnouncementCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::SetAnnouncementRequest& derived = static_cast<const ::guildpb::SetAnnouncementRequest&>(message);
    SendGuildServiceSetAnnouncement(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceSetGuildMemberRole
boost::object_pool<AsyncGuildServiceSetGuildMemberRoleGrpcClient> GuildServiceSetGuildMemberRolePool;
using AsyncGuildServiceSetGuildMemberRoleHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::SetGuildMemberRoleResponse&)>;
AsyncGuildServiceSetGuildMemberRoleHandlerFunctionType AsyncGuildServiceSetGuildMemberRoleHandler;
AsyncGuildServiceSetGuildMemberRoleFailedHandlerFunctionType AsyncGuildServiceSetGuildMemberRoleFailedHandler;

void CompleteGuildServiceSetGuildMemberRole(AsyncGuildServiceSetGuildMemberRoleGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceSetGuildMemberRoleGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceSetGuildMemberRolePool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceSetGuildMemberRoleGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceSetGuildMemberRoleHandler) {
            AsyncGuildServiceSetGuildMemberRoleHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.SetGuildMemberRole", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceSetGuildMemberRoleFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.SetGuildMemberRole failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceSetGuildMemberRole(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceSetGuildMemberRoleGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceSetGuildMemberRole(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceSetGuildMemberRoleCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::SetGuildMemberRoleResponse> done) {
    auto call(GuildServiceSetGuildMemberRolePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceSetGuildMemberRole(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSetGuildMemberRole(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceSetGuildMemberRoleMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request,
                                            GrpcDone<::guildpb::SetGuildMemberRoleResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceSetGuildMemberRoleCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceSetGuildMemberRole(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetGuildMemberRoleRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceSetGuildMemberRoleCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceSetGuildMemberRole(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::SetGuildMemberRoleRequest& derived = static_cast<const ::guildpb::SetGuildMemberRoleRequest&>(message);
    SendGuildServiceSetGuildMemberRole(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceKickGuildMember
boost::object_pool<AsyncGuildServiceKickGuildMemberGrpcClient> GuildServiceKickGuildMemberPool;
using AsyncGuildServiceKickGuildMemberHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::KickGuildMemberResponse&)>;
AsyncGuildServiceKickGuildMemberHandlerFunctionType AsyncGuildServiceKickGuildMemberHandler;
AsyncGuildServiceKickGuildMemberFailedHandlerFunctionType AsyncGuildServiceKickGuildMemberFailedHandler;

void CompleteGuildServiceKickGuildMember(AsyncGuildServiceKickGuildMemberGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceKickGuildMemberGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceKickGuildMemberPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceKickGuildMemberGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceKickGuildMemberHandler) {
            AsyncGuildServiceKickGuildMemberHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.KickGuildMember", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceKickGuildMemberFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.KickGuildMember failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceKickGuildMember(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceKickGuildMemberGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceKickGuildMember(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceKickGuildMemberCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::KickGuildMemberResponse> done) {
    auto call(GuildServiceKickGuildMemberPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceKickGuildMember(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncKickGuildMember(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceKickGuildMemberMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request,
                                            GrpcDone<::guildpb::KickGuildMemberResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceKickGuildMemberCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceKickGuildMember(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::KickGuildMemberRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceKickGuildMemberCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceKickGuildMember(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::KickGuildMemberRequest& derived = static_cast<const ::guildpb::KickGuildMemberRequest&>(message);
    SendGuildServiceKickGuildMember(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceTransferGuildLeader
boost::object_pool<AsyncGuildServiceTransferGuildLeaderGrpcClient> GuildServiceTransferGuildLeaderPool;
using AsyncGuildServiceTransferGuildLeaderHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::TransferGuildLeaderResponse&)>;
AsyncGuildServiceTransferGuildLeaderHandlerFunctionType AsyncGuildServiceTransferGuildLeaderHandler;
AsyncGuildServiceTransferGuildLeaderFailedHandlerFunctionType AsyncGuildServiceTransferGuildLeaderFailedHandler;

void CompleteGuildServiceTransferGuildLeader(AsyncGuildServiceTransferGuildLeaderGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceTransferGuildLeaderGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceTransferGuildLeaderPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceTransferGuildLeaderGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceTransferGuildLeaderHandler) {
            AsyncGuildServiceTransferGuildLeaderHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.TransferGuildLeader", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceTransferGuildLeaderFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.TransferGuildLeader failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceTransferGuildLeader(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceTransferGuildLeaderGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceTransferGuildLeader(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceTransferGuildLeaderCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::TransferGuildLeaderResponse> done) {
    auto call(GuildServiceTransferGuildLeaderPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceTransferGuildLeader(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncTransferGuildLeader(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceTransferGuildLeaderMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request,
                                            GrpcDone<::guildpb::TransferGuildLeaderResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceTransferGuildLeaderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceTransferGuildLeader(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::TransferGuildLeaderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceTransferGuildLeaderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceTransferGuildLeader(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::TransferGuildLeaderRequest& derived = static_cast<const ::guildpb::TransferGuildLeaderRequest&>(message);
    SendGuildServiceTransferGuildLeader(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceApplyJoinGuild
boost::object_pool<AsyncGuildServiceApplyJoinGuildGrpcClient> GuildServiceApplyJoinGuildPool;
using AsyncGuildServiceApplyJoinGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ApplyJoinGuildResponse&)>;
AsyncGuildServiceApplyJoinGuildHandlerFunctionType AsyncGuildServiceApplyJoinGuildHandler;
AsyncGuildServiceApplyJoinGuildFailedHandlerFunctionType AsyncGuildServiceApplyJoinGuildFailedHandler;

void CompleteGuildServiceApplyJoinGuild(AsyncGuildServiceApplyJoinGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceApplyJoinGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceApplyJoinGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceApplyJoinGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceApplyJoinGuildHandler) {
            AsyncGuildServiceApplyJoinGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.ApplyJoinGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceApplyJoinGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.ApplyJoinGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceApplyJoinGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceApplyJoinGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceApplyJoinGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceApplyJoinGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ApplyJoinGuildResponse> done) {
    auto call(GuildServiceApplyJoinGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceApplyJoinGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncApplyJoinGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceApplyJoinGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request,
                                            GrpcDone<::guildpb::ApplyJoinGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceApplyJoinGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceApplyJoinGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ApplyJoinGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceApplyJoinGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceApplyJoinGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::ApplyJoinGuildRequest& derived = static_cast<const ::guildpb::ApplyJoinGuildRequest&>(message);
    SendGuildServiceApplyJoinGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceCancelGuildApplication
boost::object_pool<AsyncGuildServiceCancelGuildApplicationGrpcClient> GuildServiceCancelGuildApplicationPool;
using AsyncGuildServiceCancelGuildApplicationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::CancelGuildApplicationResponse&)>;
AsyncGuildServiceCancelGuildApplicationHandlerFunctionType AsyncGuildServiceCancelGuildApplicationHandler;
AsyncGuildServiceCancelGuildApplicationFailedHandlerFunctionType AsyncGuildServiceCancelGuildApplicationFailedHandler;

void CompleteGuildServiceCancelGuildApplication(AsyncGuildServiceCancelGuildApplicationGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceCancelGuildApplicationGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceCancelGuildApplicationPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceCancelGuildApplicationGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceCancelGuildApplicationHandler) {
            AsyncGuildServiceCancelGuildApplicationHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.CancelGuildApplication", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceCancelGuildApplicationFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.CancelGuildApplication failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceCancelGuildApplication(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceCancelGuildApplicationGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceCancelGuildApplication(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceCancelGuildApplicationCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::CancelGuildApplicationResponse> done) {
    auto call(GuildServiceCancelGuildApplicationPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceCancelGuildApplication(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCancelGuildApplication(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceCancelGuildApplicationMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request,
                                            GrpcDone<::guildpb::CancelGuildApplicationResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceCancelGuildApplicationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceCancelGuildApplication(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CancelGuildApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceCancelGuildApplicationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceCancelGuildApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::CancelGuildApplicationRequest& derived = static_cast<const ::guildpb::CancelGuildApplicationRequest&>(message);
    SendGuildServiceCancelGuildApplication(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceListMyGuildApplications
boost::object_pool<AsyncGuildServiceListMyGuildApplicationsGrpcClient> GuildServiceListMyGuildApplicationsPool;
using AsyncGuildServiceListMyGuildApplicationsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ListMyGuildApplicationsResponse&)>;
AsyncGuildServiceListMyGuildApplicationsHandlerFunctionType AsyncGuildServiceListMyGuildApplicationsHandler;
AsyncGuildServiceListMyGuildApplicationsFailedHandlerFunctionType AsyncGuildServiceListMyGuildApplicationsFailedHandler;

void CompleteGuildServiceListMyGuildApplications(AsyncGuildServiceListMyGuildApplicationsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceListMyGuildApplicationsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceListMyGuildApplicationsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceListMyGuildApplicationsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceListMyGuildApplicationsHandler) {
            AsyncGuildServiceListMyGuildApplicationsHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.ListMyGuildApplications", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceListMyGuildApplicationsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.ListMyGuildApplications failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceListMyGuildApplications(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceListMyGuildApplicationsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceListMyGuildApplications(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceListMyGuildApplicationsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ListMyGuildApplicationsResponse> done) {
    auto call(GuildServiceListMyGuildApplicationsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceListMyGuildApplications(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListMyGuildApplications(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceListMyGuildApplicationsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request,
                                            GrpcDone<::guildpb::ListMyGuildApplicationsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceListMyGuildApplicationsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceListMyGuildApplications(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListMyGuildApplicationsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceListMyGuildApplicationsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceListMyGuildApplications(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::ListMyGuildApplicationsRequest& derived = static_cast<const ::guildpb::ListMyGuildApplicationsRequest&>(message);
    SendGuildServiceListMyGuildApplications(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceListGuildApplications
boost::object_pool<AsyncGuildServiceListGuildApplicationsGrpcClient> GuildServiceListGuildApplicationsPool;
using AsyncGuildServiceListGuildApplicationsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ListGuildApplicationsResponse&)>;
AsyncGuildServiceListGuildApplicationsHandlerFunctionType AsyncGuildServiceListGuildApplicationsHandler;
AsyncGuildServiceListGuildApplicationsFailedHandlerFunctionType AsyncGuildServiceListGuildApplicationsFailedHandler;

void CompleteGuildServiceListGuildApplications(AsyncGuildServiceListGuildApplicationsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceListGuildApplicationsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceListGuildApplicationsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceListGuildApplicationsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceListGuildApplicationsHandler) {
            AsyncGuildServiceListGuildApplicationsHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.ListGuildApplications", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceListGuildApplicationsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.ListGuildApplications failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceListGuildApplications(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceListGuildApplicationsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceListGuildApplications(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceListGuildApplicationsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ListGuildApplicationsResponse> done) {
    auto call(GuildServiceListGuildApplicationsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceListGuildApplications(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListGuildApplications(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceListGuildApplicationsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request,
                                            GrpcDone<::guildpb::ListGuildApplicationsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceListGuildApplicationsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceListGuildApplications(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ListGuildApplicationsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceListGuildApplicationsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceListGuildApplications(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::ListGuildApplicationsRequest& derived = static_cast<const ::guildpb::ListGuildApplicationsRequest&>(message);
    SendGuildServiceListGuildApplications(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceReviewGuildApplication
boost::object_pool<AsyncGuildServiceReviewGuildApplicationGrpcClient> GuildServiceReviewGuildApplicationPool;
using AsyncGuildServiceReviewGuildApplicationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ReviewGuildApplicationResponse&)>;
AsyncGuildServiceReviewGuildApplicationHandlerFunctionType AsyncGuildServiceReviewGuildApplicationHandler;
AsyncGuildServiceReviewGuildApplicationFailedHandlerFunctionType AsyncGuildServiceReviewGuildApplicationFailedHandler;

void CompleteGuildServiceReviewGuildApplication(AsyncGuildServiceReviewGuildApplicationGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceReviewGuildApplicationGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceReviewGuildApplicationPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceReviewGuildApplicationGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceReviewGuildApplicationHandler) {
            AsyncGuildServiceReviewGuildApplicationHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.ReviewGuildApplication", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceReviewGuildApplicationFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.ReviewGuildApplication failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceReviewGuildApplication(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceReviewGuildApplicationGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceReviewGuildApplication(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceReviewGuildApplicationCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ReviewGuildApplicationResponse> done) {
    auto call(GuildServiceReviewGuildApplicationPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceReviewGuildApplication(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncReviewGuildApplication(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceReviewGuildApplicationMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request,
                                            GrpcDone<::guildpb::ReviewGuildApplicationResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceReviewGuildApplicationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceReviewGuildApplication(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ReviewGuildApplicationRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceReviewGuildApplicationCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceReviewGuildApplication(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::ReviewGuildApplicationRequest& derived = static_cast<const ::guildpb::ReviewGuildApplicationRequest&>(message);
    SendGuildServiceReviewGuildApplication(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceNotifyGuildChanged
boost::object_pool<AsyncGuildServiceNotifyGuildChangedGrpcClient> GuildServiceNotifyGuildChangedPool;
using AsyncGuildServiceNotifyGuildChangedHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncGuildServiceNotifyGuildChangedHandlerFunctionType AsyncGuildServiceNotifyGuildChangedHandler;
AsyncGuildServiceNotifyGuildChangedFailedHandlerFunctionType AsyncGuildServiceNotifyGuildChangedFailedHandler;

void CompleteGuildServiceNotifyGuildChanged(AsyncGuildServiceNotifyGuildChangedGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceNotifyGuildChangedGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceNotifyGuildChangedPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceNotifyGuildChangedGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceNotifyGuildChangedHandler) {
            AsyncGuildServiceNotifyGuildChangedHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.NotifyGuildChanged", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceNotifyGuildChangedFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.NotifyGuildChanged failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceNotifyGuildChanged(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceNotifyGuildChangedGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceNotifyGuildChanged(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceNotifyGuildChangedCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(GuildServiceNotifyGuildChangedPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceNotifyGuildChanged(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyGuildChanged(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceNotifyGuildChangedMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartGuildServiceNotifyGuildChangedCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceNotifyGuildChanged(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GuildChangedS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceNotifyGuildChangedCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceNotifyGuildChanged(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GuildChangedS2C& derived = static_cast<const ::guildpb::GuildChangedS2C&>(message);
    SendGuildServiceNotifyGuildChanged(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceUpdateGuildScore
boost::object_pool<AsyncGuildServiceUpdateGuildScoreGrpcClient> GuildServiceUpdateGuildScorePool;
using AsyncGuildServiceUpdateGuildScoreHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::UpdateGuildScoreResponse&)>;
AsyncGuildServiceUpdateGuildScoreHandlerFunctionType AsyncGuildServiceUpdateGuildScoreHandler;
AsyncGuildServiceUpdateGuildScoreFailedHandlerFunctionType AsyncGuildServiceUpdateGuildScoreFailedHandler;

void CompleteGuildServiceUpdateGuildScore(AsyncGuildServiceUpdateGuildScoreGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceUpdateGuildScoreGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceUpdateGuildScorePool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceUpdateGuildScoreGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceUpdateGuildScoreHandler) {
            AsyncGuildServiceUpdateGuildScoreHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.UpdateGuildScore", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceUpdateGuildScoreFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.UpdateGuildScore failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceUpdateGuildScore(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceUpdateGuildScoreGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceUpdateGuildScore(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceUpdateGuildScoreCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::UpdateGuildScoreResponse> done) {
    auto call(GuildServiceUpdateGuildScorePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceUpdateGuildScore(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncUpdateGuildScore(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceUpdateGuildScoreMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request,
                                            GrpcDone<::guildpb::UpdateGuildScoreResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceUpdateGuildScoreCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceUpdateGuildScore(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpdateGuildScoreRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceUpdateGuildScoreCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceUpdateGuildScore(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::UpdateGuildScoreRequest& derived = static_cast<const ::guildpb::UpdateGuildScoreRequest&>(message);
    SendGuildServiceUpdateGuildScore(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildRank
boost::object_pool<AsyncGuildServiceGetGuildRankGrpcClient> GuildServiceGetGuildRankPool;
using AsyncGuildServiceGetGuildRankHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildRankResponse&)>;
AsyncGuildServiceGetGuildRankHandlerFunctionType AsyncGuildServiceGetGuildRankHandler;
AsyncGuildServiceGetGuildRankFailedHandlerFunctionType AsyncGuildServiceGetGuildRankFailedHandler;

void CompleteGuildServiceGetGuildRank(AsyncGuildServiceGetGuildRankGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceGetGuildRankGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceGetGuildRankPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceGetGuildRankGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceGetGuildRankHandler) {
            AsyncGuildServiceGetGuildRankHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.GetGuildRank", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceGetGuildRankFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildRank failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceGetGuildRank(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceGetGuildRankGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceGetGuildRank(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildRankCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildRankResponse> done) {
    auto call(GuildServiceGetGuildRankPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceGetGuildRank(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetGuildRank(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceGetGuildRankMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request,
                                            GrpcDone<::guildpb::GetGuildRankResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildRankCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildRank(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildRankCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceGetGuildRank(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GetGuildRankRequest& derived = static_cast<const ::guildpb::GetGuildRankRequest&>(message);
    SendGuildServiceGetGuildRank(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildRankByGuild
boost::object_pool<AsyncGuildServiceGetGuildRankByGuildGrpcClient> GuildServiceGetGuildRankByGuildPool;
using AsyncGuildServiceGetGuildRankByGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildRankByGuildResponse&)>;
AsyncGuildServiceGetGuildRankByGuildHandlerFunctionType AsyncGuildServiceGetGuildRankByGuildHandler;
AsyncGuildServiceGetGuildRankByGuildFailedHandlerFunctionType AsyncGuildServiceGetGuildRankByGuildFailedHandler;

void CompleteGuildServiceGetGuildRankByGuild(AsyncGuildServiceGetGuildRankByGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceGetGuildRankByGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceGetGuildRankByGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceGetGuildRankByGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceGetGuildRankByGuildHandler) {
            AsyncGuildServiceGetGuildRankByGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.GetGuildRankByGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceGetGuildRankByGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildRankByGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceGetGuildRankByGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceGetGuildRankByGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceGetGuildRankByGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildRankByGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildRankByGuildResponse> done) {
    auto call(GuildServiceGetGuildRankByGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceGetGuildRankByGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetGuildRankByGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceGetGuildRankByGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request,
                                            GrpcDone<::guildpb::GetGuildRankByGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildRankByGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildRankByGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRankByGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildRankByGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceGetGuildRankByGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GetGuildRankByGuildRequest& derived = static_cast<const ::guildpb::GetGuildRankByGuildRequest&>(message);
    SendGuildServiceGetGuildRankByGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildDonateOptions
boost::object_pool<AsyncGuildServiceGetGuildDonateOptionsGrpcClient> GuildServiceGetGuildDonateOptionsPool;
using AsyncGuildServiceGetGuildDonateOptionsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildDonateOptionsResponse&)>;
AsyncGuildServiceGetGuildDonateOptionsHandlerFunctionType AsyncGuildServiceGetGuildDonateOptionsHandler;
AsyncGuildServiceGetGuildDonateOptionsFailedHandlerFunctionType AsyncGuildServiceGetGuildDonateOptionsFailedHandler;

void CompleteGuildServiceGetGuildDonateOptions(AsyncGuildServiceGetGuildDonateOptionsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceGetGuildDonateOptionsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceGetGuildDonateOptionsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceGetGuildDonateOptionsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceGetGuildDonateOptionsHandler) {
            AsyncGuildServiceGetGuildDonateOptionsHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.GetGuildDonateOptions", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceGetGuildDonateOptionsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildDonateOptions failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceGetGuildDonateOptions(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceGetGuildDonateOptionsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceGetGuildDonateOptions(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildDonateOptionsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildDonateOptionsResponse> done) {
    auto call(GuildServiceGetGuildDonateOptionsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceGetGuildDonateOptions(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetGuildDonateOptions(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceGetGuildDonateOptionsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request,
                                            GrpcDone<::guildpb::GetGuildDonateOptionsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildDonateOptionsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildDonateOptions(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildDonateOptionsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildDonateOptionsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceGetGuildDonateOptions(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GetGuildDonateOptionsRequest& derived = static_cast<const ::guildpb::GetGuildDonateOptionsRequest&>(message);
    SendGuildServiceGetGuildDonateOptions(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceDonateToGuild
boost::object_pool<AsyncGuildServiceDonateToGuildGrpcClient> GuildServiceDonateToGuildPool;
using AsyncGuildServiceDonateToGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::DonateToGuildResponse&)>;
AsyncGuildServiceDonateToGuildHandlerFunctionType AsyncGuildServiceDonateToGuildHandler;
AsyncGuildServiceDonateToGuildFailedHandlerFunctionType AsyncGuildServiceDonateToGuildFailedHandler;

void CompleteGuildServiceDonateToGuild(AsyncGuildServiceDonateToGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceDonateToGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceDonateToGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceDonateToGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceDonateToGuildHandler) {
            AsyncGuildServiceDonateToGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.DonateToGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceDonateToGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.DonateToGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceDonateToGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceDonateToGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceDonateToGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceDonateToGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::DonateToGuildResponse> done) {
    auto call(GuildServiceDonateToGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceDonateToGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDonateToGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceDonateToGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request,
                                            GrpcDone<::guildpb::DonateToGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceDonateToGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceDonateToGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DonateToGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceDonateToGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceDonateToGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::DonateToGuildRequest& derived = static_cast<const ::guildpb::DonateToGuildRequest&>(message);
    SendGuildServiceDonateToGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceUpgradeGuild
boost::object_pool<AsyncGuildServiceUpgradeGuildGrpcClient> GuildServiceUpgradeGuildPool;
using AsyncGuildServiceUpgradeGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::UpgradeGuildResponse&)>;
AsyncGuildServiceUpgradeGuildHandlerFunctionType AsyncGuildServiceUpgradeGuildHandler;
AsyncGuildServiceUpgradeGuildFailedHandlerFunctionType AsyncGuildServiceUpgradeGuildFailedHandler;

void CompleteGuildServiceUpgradeGuild(AsyncGuildServiceUpgradeGuildGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceUpgradeGuildGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceUpgradeGuildPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceUpgradeGuildGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceUpgradeGuildHandler) {
            AsyncGuildServiceUpgradeGuildHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.UpgradeGuild", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceUpgradeGuildFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.UpgradeGuild failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceUpgradeGuild(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceUpgradeGuildGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceUpgradeGuild(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceUpgradeGuildCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::UpgradeGuildResponse> done) {
    auto call(GuildServiceUpgradeGuildPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceUpgradeGuild(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncUpgradeGuild(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceUpgradeGuildMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request,
                                            GrpcDone<::guildpb::UpgradeGuildResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceUpgradeGuildCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceUpgradeGuild(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::UpgradeGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceUpgradeGuildCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceUpgradeGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::UpgradeGuildRequest& derived = static_cast<const ::guildpb::UpgradeGuildRequest&>(message);
    SendGuildServiceUpgradeGuild(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildShop
boost::object_pool<AsyncGuildServiceGetGuildShopGrpcClient> GuildServiceGetGuildShopPool;
using AsyncGuildServiceGetGuildShopHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildShopResponse&)>;
AsyncGuildServiceGetGuildShopHandlerFunctionType AsyncGuildServiceGetGuildShopHandler;
AsyncGuildServiceGetGuildShopFailedHandlerFunctionType AsyncGuildServiceGetGuildShopFailedHandler;

void CompleteGuildServiceGetGuildShop(AsyncGuildServiceGetGuildShopGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceGetGuildShopGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceGetGuildShopPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceGetGuildShopGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceGetGuildShopHandler) {
            AsyncGuildServiceGetGuildShopHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.GetGuildShop", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceGetGuildShopFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildShop failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceGetGuildShop(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceGetGuildShopGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceGetGuildShop(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildShopCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildShopResponse> done) {
    auto call(GuildServiceGetGuildShopPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceGetGuildShop(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetGuildShop(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceGetGuildShopMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request,
                                            GrpcDone<::guildpb::GetGuildShopResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildShopCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildShop(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildShopRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildShopCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceGetGuildShop(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GetGuildShopRequest& derived = static_cast<const ::guildpb::GetGuildShopRequest&>(message);
    SendGuildServiceGetGuildShop(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceBuyGuildShopGoods
boost::object_pool<AsyncGuildServiceBuyGuildShopGoodsGrpcClient> GuildServiceBuyGuildShopGoodsPool;
using AsyncGuildServiceBuyGuildShopGoodsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::BuyGuildShopGoodsResponse&)>;
AsyncGuildServiceBuyGuildShopGoodsHandlerFunctionType AsyncGuildServiceBuyGuildShopGoodsHandler;
AsyncGuildServiceBuyGuildShopGoodsFailedHandlerFunctionType AsyncGuildServiceBuyGuildShopGoodsFailedHandler;

void CompleteGuildServiceBuyGuildShopGoods(AsyncGuildServiceBuyGuildShopGoodsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceBuyGuildShopGoodsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceBuyGuildShopGoodsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceBuyGuildShopGoodsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceBuyGuildShopGoodsHandler) {
            AsyncGuildServiceBuyGuildShopGoodsHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.BuyGuildShopGoods", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceBuyGuildShopGoodsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.BuyGuildShopGoods failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceBuyGuildShopGoods(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceBuyGuildShopGoodsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceBuyGuildShopGoods(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceBuyGuildShopGoodsCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::BuyGuildShopGoodsResponse> done) {
    auto call(GuildServiceBuyGuildShopGoodsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceBuyGuildShopGoods(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncBuyGuildShopGoods(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceBuyGuildShopGoodsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request,
                                            GrpcDone<::guildpb::BuyGuildShopGoodsResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceBuyGuildShopGoodsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceBuyGuildShopGoods(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::BuyGuildShopGoodsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceBuyGuildShopGoodsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceBuyGuildShopGoods(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::BuyGuildShopGoodsRequest& derived = static_cast<const ::guildpb::BuyGuildShopGoodsRequest&>(message);
    SendGuildServiceBuyGuildShopGoods(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceGetGuildActivities
boost::object_pool<AsyncGuildServiceGetGuildActivitiesGrpcClient> GuildServiceGetGuildActivitiesPool;
using AsyncGuildServiceGetGuildActivitiesHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildActivitiesResponse&)>;
AsyncGuildServiceGetGuildActivitiesHandlerFunctionType AsyncGuildServiceGetGuildActivitiesHandler;
AsyncGuildServiceGetGuildActivitiesFailedHandlerFunctionType AsyncGuildServiceGetGuildActivitiesFailedHandler;

void CompleteGuildServiceGetGuildActivities(AsyncGuildServiceGetGuildActivitiesGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceGetGuildActivitiesGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceGetGuildActivitiesPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceGetGuildActivitiesGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceGetGuildActivitiesHandler) {
            AsyncGuildServiceGetGuildActivitiesHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.GetGuildActivities", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceGetGuildActivitiesFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.GetGuildActivities failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceGetGuildActivities(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceGetGuildActivitiesGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceGetGuildActivities(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceGetGuildActivitiesCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::GetGuildActivitiesResponse> done) {
    auto call(GuildServiceGetGuildActivitiesPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceGetGuildActivities(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetGuildActivities(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceGetGuildActivitiesMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request,
                                            GrpcDone<::guildpb::GetGuildActivitiesResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceGetGuildActivitiesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceGetGuildActivities(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildActivitiesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceGetGuildActivitiesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceGetGuildActivities(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::GetGuildActivitiesRequest& derived = static_cast<const ::guildpb::GetGuildActivitiesRequest&>(message);
    SendGuildServiceGetGuildActivities(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceLightGuildLantern
boost::object_pool<AsyncGuildServiceLightGuildLanternGrpcClient> GuildServiceLightGuildLanternPool;
using AsyncGuildServiceLightGuildLanternHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::LightGuildLanternResponse&)>;
AsyncGuildServiceLightGuildLanternHandlerFunctionType AsyncGuildServiceLightGuildLanternHandler;
AsyncGuildServiceLightGuildLanternFailedHandlerFunctionType AsyncGuildServiceLightGuildLanternFailedHandler;

void CompleteGuildServiceLightGuildLantern(AsyncGuildServiceLightGuildLanternGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceLightGuildLanternGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceLightGuildLanternPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceLightGuildLanternGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceLightGuildLanternHandler) {
            AsyncGuildServiceLightGuildLanternHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.LightGuildLantern", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceLightGuildLanternFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.LightGuildLantern failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceLightGuildLantern(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceLightGuildLanternGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceLightGuildLantern(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceLightGuildLanternCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::LightGuildLanternResponse> done) {
    auto call(GuildServiceLightGuildLanternPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceLightGuildLantern(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLightGuildLantern(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceLightGuildLanternMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request,
                                            GrpcDone<::guildpb::LightGuildLanternResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceLightGuildLanternCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceLightGuildLantern(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LightGuildLanternRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceLightGuildLanternCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceLightGuildLantern(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::LightGuildLanternRequest& derived = static_cast<const ::guildpb::LightGuildLanternRequest&>(message);
    SendGuildServiceLightGuildLantern(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceClaimGuildReunion
boost::object_pool<AsyncGuildServiceClaimGuildReunionGrpcClient> GuildServiceClaimGuildReunionPool;
using AsyncGuildServiceClaimGuildReunionHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::ClaimGuildReunionResponse&)>;
AsyncGuildServiceClaimGuildReunionHandlerFunctionType AsyncGuildServiceClaimGuildReunionHandler;
AsyncGuildServiceClaimGuildReunionFailedHandlerFunctionType AsyncGuildServiceClaimGuildReunionFailedHandler;

void CompleteGuildServiceClaimGuildReunion(AsyncGuildServiceClaimGuildReunionGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceClaimGuildReunionGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceClaimGuildReunionPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceClaimGuildReunionGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceClaimGuildReunionHandler) {
            AsyncGuildServiceClaimGuildReunionHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.ClaimGuildReunion", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceClaimGuildReunionFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.ClaimGuildReunion failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceClaimGuildReunion(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceClaimGuildReunionGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceClaimGuildReunion(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceClaimGuildReunionCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::ClaimGuildReunionResponse> done) {
    auto call(GuildServiceClaimGuildReunionPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceClaimGuildReunion(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncClaimGuildReunion(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceClaimGuildReunionMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request,
                                            GrpcDone<::guildpb::ClaimGuildReunionResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceClaimGuildReunionCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceClaimGuildReunion(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::ClaimGuildReunionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceClaimGuildReunionCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceClaimGuildReunion(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::ClaimGuildReunionRequest& derived = static_cast<const ::guildpb::ClaimGuildReunionRequest&>(message);
    SendGuildServiceClaimGuildReunion(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceStartGuildTrial
boost::object_pool<AsyncGuildServiceStartGuildTrialGrpcClient> GuildServiceStartGuildTrialPool;
using AsyncGuildServiceStartGuildTrialHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::StartGuildTrialResponse&)>;
AsyncGuildServiceStartGuildTrialHandlerFunctionType AsyncGuildServiceStartGuildTrialHandler;
AsyncGuildServiceStartGuildTrialFailedHandlerFunctionType AsyncGuildServiceStartGuildTrialFailedHandler;

void CompleteGuildServiceStartGuildTrial(AsyncGuildServiceStartGuildTrialGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceStartGuildTrialGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceStartGuildTrialPool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceStartGuildTrialGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceStartGuildTrialHandler) {
            AsyncGuildServiceStartGuildTrialHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.StartGuildTrial", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceStartGuildTrialFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.StartGuildTrial failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceStartGuildTrial(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceStartGuildTrialGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceStartGuildTrial(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceStartGuildTrialCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::StartGuildTrialResponse> done) {
    auto call(GuildServiceStartGuildTrialPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceStartGuildTrial(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncStartGuildTrial(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceStartGuildTrialMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request,
                                            GrpcDone<::guildpb::StartGuildTrialResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceStartGuildTrialCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceStartGuildTrial(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::StartGuildTrialRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceStartGuildTrialCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceStartGuildTrial(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::StartGuildTrialRequest& derived = static_cast<const ::guildpb::StartGuildTrialRequest&>(message);
    SendGuildServiceStartGuildTrial(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region GuildServiceRespondGuildTrialInvite
boost::object_pool<AsyncGuildServiceRespondGuildTrialInviteGrpcClient> GuildServiceRespondGuildTrialInvitePool;
using AsyncGuildServiceRespondGuildTrialInviteHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::RespondGuildTrialInviteResponse&)>;
AsyncGuildServiceRespondGuildTrialInviteHandlerFunctionType AsyncGuildServiceRespondGuildTrialInviteHandler;
AsyncGuildServiceRespondGuildTrialInviteFailedHandlerFunctionType AsyncGuildServiceRespondGuildTrialInviteFailedHandler;

void CompleteGuildServiceRespondGuildTrialInvite(AsyncGuildServiceRespondGuildTrialInviteGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncGuildServiceRespondGuildTrialInviteGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        GuildServiceRespondGuildTrialInvitePool.destroy(completed);
    };
    const std::unique_ptr<AsyncGuildServiceRespondGuildTrialInviteGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncGuildServiceRespondGuildTrialInviteHandler) {
            AsyncGuildServiceRespondGuildTrialInviteHandler(call->context, call->reply);
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
        const GrpcCallFailure failure{call->messageId, "GuildService.RespondGuildTrialInvite", call->context, call->status, call->sentMetadata};
        AsyncGuildServiceRespondGuildTrialInviteFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC GuildService.RespondGuildTrialInvite failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcGuildServiceRespondGuildTrialInvite(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncGuildServiceRespondGuildTrialInviteGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteGuildServiceRespondGuildTrialInvite(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartGuildServiceRespondGuildTrialInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::guildpb::RespondGuildTrialInviteResponse> done) {
    auto call(GuildServiceRespondGuildTrialInvitePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<GuildServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteGuildServiceRespondGuildTrialInvite(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRespondGuildTrialInvite(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(GuildServiceRespondGuildTrialInviteMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request,
                                            GrpcDone<::guildpb::RespondGuildTrialInviteResponse> done, const GrpcCallOptions& options) {
    StartGuildServiceRespondGuildTrialInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendGuildServiceRespondGuildTrialInvite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::RespondGuildTrialInviteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartGuildServiceRespondGuildTrialInviteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendGuildServiceRespondGuildTrialInvite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::guildpb::RespondGuildTrialInviteRequest& derived = static_cast<const ::guildpb::RespondGuildTrialInviteRequest&>(message);
    SendGuildServiceRespondGuildTrialInvite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleGuildCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case GuildServiceCreateGuildMessageId:
            AsyncCompleteGrpcGuildServiceCreateGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceGetGuildMessageId:
            AsyncCompleteGrpcGuildServiceGetGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceGetPlayerGuildMessageId:
            AsyncCompleteGrpcGuildServiceGetPlayerGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceLeaveGuildMessageId:
            AsyncCompleteGrpcGuildServiceLeaveGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceDisbandGuildMessageId:
            AsyncCompleteGrpcGuildServiceDisbandGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceSetAnnouncementMessageId:
            AsyncCompleteGrpcGuildServiceSetAnnouncement(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceSetGuildMemberRoleMessageId:
            AsyncCompleteGrpcGuildServiceSetGuildMemberRole(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceKickGuildMemberMessageId:
            AsyncCompleteGrpcGuildServiceKickGuildMember(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceTransferGuildLeaderMessageId:
            AsyncCompleteGrpcGuildServiceTransferGuildLeader(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceApplyJoinGuildMessageId:
            AsyncCompleteGrpcGuildServiceApplyJoinGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceCancelGuildApplicationMessageId:
            AsyncCompleteGrpcGuildServiceCancelGuildApplication(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceListMyGuildApplicationsMessageId:
            AsyncCompleteGrpcGuildServiceListMyGuildApplications(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceListGuildApplicationsMessageId:
            AsyncCompleteGrpcGuildServiceListGuildApplications(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceReviewGuildApplicationMessageId:
            AsyncCompleteGrpcGuildServiceReviewGuildApplication(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceNotifyGuildChangedMessageId:
            AsyncCompleteGrpcGuildServiceNotifyGuildChanged(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceUpdateGuildScoreMessageId:
            AsyncCompleteGrpcGuildServiceUpdateGuildScore(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceGetGuildRankMessageId:
            AsyncCompleteGrpcGuildServiceGetGuildRank(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceGetGuildRankByGuildMessageId:
            AsyncCompleteGrpcGuildServiceGetGuildRankByGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceGetGuildDonateOptionsMessageId:
            AsyncCompleteGrpcGuildServiceGetGuildDonateOptions(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceDonateToGuildMessageId:
            AsyncCompleteGrpcGuildServiceDonateToGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceUpgradeGuildMessageId:
            AsyncCompleteGrpcGuildServiceUpgradeGuild(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceGetGuildShopMessageId:
            AsyncCompleteGrpcGuildServiceGetGuildShop(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceBuyGuildShopGoodsMessageId:
            AsyncCompleteGrpcGuildServiceBuyGuildShopGoods(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceGetGuildActivitiesMessageId:
            AsyncCompleteGrpcGuildServiceGetGuildActivities(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceLightGuildLanternMessageId:
            AsyncCompleteGrpcGuildServiceLightGuildLantern(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceClaimGuildReunionMessageId:
            AsyncCompleteGrpcGuildServiceClaimGuildReunion(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceStartGuildTrialMessageId:
            AsyncCompleteGrpcGuildServiceStartGuildTrial(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case GuildServiceRespondGuildTrialInviteMessageId:
            AsyncCompleteGrpcGuildServiceRespondGuildTrialInvite(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetGuildHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

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
}

void SetGuildIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

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
}

void SetGuildCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitGuildGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<GuildServiceStubPtr>(nodeEntity, GuildService::NewStub(channel));

}

}// namespace guildpb
