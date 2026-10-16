#include "muduo/base/Logging.h"

#include "friend_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetFriendCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace friendpb {
struct FriendCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region ClientPlayerFriendAddFriend
boost::object_pool<AsyncClientPlayerFriendAddFriendGrpcClient> ClientPlayerFriendAddFriendPool;
using AsyncClientPlayerFriendAddFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::AddFriendResponse&)>;
AsyncClientPlayerFriendAddFriendHandlerFunctionType AsyncClientPlayerFriendAddFriendHandler;
AsyncClientPlayerFriendAddFriendFailedHandlerFunctionType AsyncClientPlayerFriendAddFriendFailedHandler;

void CompleteClientPlayerFriendAddFriend(AsyncClientPlayerFriendAddFriendGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendAddFriendGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendAddFriendPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendAddFriendGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendAddFriendHandler) {
            AsyncClientPlayerFriendAddFriendHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.AddFriend reply dropped: AsyncClientPlayerFriendAddFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendAddFriendFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.AddFriend", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendAddFriendFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.AddFriend failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendAddFriend(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendAddFriendGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendAddFriend(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendAddFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::AddFriendResponse> done) {
    auto call(ClientPlayerFriendAddFriendPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendAddFriend(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAddFriend(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendAddFriendMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request,
                                            GrpcDone<::friendpb::AddFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendAddFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendAddFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AddFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendAddFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendAddFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::AddFriendRequest& derived = static_cast<const ::friendpb::AddFriendRequest&>(message);
    SendClientPlayerFriendAddFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendAcceptFriend
boost::object_pool<AsyncClientPlayerFriendAcceptFriendGrpcClient> ClientPlayerFriendAcceptFriendPool;
using AsyncClientPlayerFriendAcceptFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::AcceptFriendResponse&)>;
AsyncClientPlayerFriendAcceptFriendHandlerFunctionType AsyncClientPlayerFriendAcceptFriendHandler;
AsyncClientPlayerFriendAcceptFriendFailedHandlerFunctionType AsyncClientPlayerFriendAcceptFriendFailedHandler;

void CompleteClientPlayerFriendAcceptFriend(AsyncClientPlayerFriendAcceptFriendGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendAcceptFriendGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendAcceptFriendPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendAcceptFriendGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendAcceptFriendHandler) {
            AsyncClientPlayerFriendAcceptFriendHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.AcceptFriend reply dropped: AsyncClientPlayerFriendAcceptFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendAcceptFriendFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.AcceptFriend", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendAcceptFriendFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.AcceptFriend failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendAcceptFriend(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendAcceptFriendGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendAcceptFriend(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendAcceptFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::AcceptFriendResponse> done) {
    auto call(ClientPlayerFriendAcceptFriendPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendAcceptFriend(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAcceptFriend(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendAcceptFriendMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request,
                                            GrpcDone<::friendpb::AcceptFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendAcceptFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendAcceptFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::AcceptFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendAcceptFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendAcceptFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::AcceptFriendRequest& derived = static_cast<const ::friendpb::AcceptFriendRequest&>(message);
    SendClientPlayerFriendAcceptFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendRejectFriend
boost::object_pool<AsyncClientPlayerFriendRejectFriendGrpcClient> ClientPlayerFriendRejectFriendPool;
using AsyncClientPlayerFriendRejectFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::RejectFriendResponse&)>;
AsyncClientPlayerFriendRejectFriendHandlerFunctionType AsyncClientPlayerFriendRejectFriendHandler;
AsyncClientPlayerFriendRejectFriendFailedHandlerFunctionType AsyncClientPlayerFriendRejectFriendFailedHandler;

void CompleteClientPlayerFriendRejectFriend(AsyncClientPlayerFriendRejectFriendGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendRejectFriendGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendRejectFriendPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendRejectFriendGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendRejectFriendHandler) {
            AsyncClientPlayerFriendRejectFriendHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.RejectFriend reply dropped: AsyncClientPlayerFriendRejectFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendRejectFriendFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.RejectFriend", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendRejectFriendFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.RejectFriend failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendRejectFriend(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendRejectFriendGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendRejectFriend(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendRejectFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::RejectFriendResponse> done) {
    auto call(ClientPlayerFriendRejectFriendPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendRejectFriend(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRejectFriend(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendRejectFriendMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request,
                                            GrpcDone<::friendpb::RejectFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendRejectFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendRejectFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RejectFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendRejectFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendRejectFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::RejectFriendRequest& derived = static_cast<const ::friendpb::RejectFriendRequest&>(message);
    SendClientPlayerFriendRejectFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendRemoveFriend
boost::object_pool<AsyncClientPlayerFriendRemoveFriendGrpcClient> ClientPlayerFriendRemoveFriendPool;
using AsyncClientPlayerFriendRemoveFriendHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::RemoveFriendResponse&)>;
AsyncClientPlayerFriendRemoveFriendHandlerFunctionType AsyncClientPlayerFriendRemoveFriendHandler;
AsyncClientPlayerFriendRemoveFriendFailedHandlerFunctionType AsyncClientPlayerFriendRemoveFriendFailedHandler;

void CompleteClientPlayerFriendRemoveFriend(AsyncClientPlayerFriendRemoveFriendGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendRemoveFriendGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendRemoveFriendPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendRemoveFriendGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendRemoveFriendHandler) {
            AsyncClientPlayerFriendRemoveFriendHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.RemoveFriend reply dropped: AsyncClientPlayerFriendRemoveFriendHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendRemoveFriendFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.RemoveFriend", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendRemoveFriendFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.RemoveFriend failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendRemoveFriend(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendRemoveFriendGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendRemoveFriend(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendRemoveFriendCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::RemoveFriendResponse> done) {
    auto call(ClientPlayerFriendRemoveFriendPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendRemoveFriend(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRemoveFriend(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendRemoveFriendMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request,
                                            GrpcDone<::friendpb::RemoveFriendResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendRemoveFriendCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendRemoveFriend(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RemoveFriendRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendRemoveFriendCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendRemoveFriend(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::RemoveFriendRequest& derived = static_cast<const ::friendpb::RemoveFriendRequest&>(message);
    SendClientPlayerFriendRemoveFriend(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendGetFriendList
boost::object_pool<AsyncClientPlayerFriendGetFriendListGrpcClient> ClientPlayerFriendGetFriendListPool;
using AsyncClientPlayerFriendGetFriendListHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::GetFriendListResponse&)>;
AsyncClientPlayerFriendGetFriendListHandlerFunctionType AsyncClientPlayerFriendGetFriendListHandler;
AsyncClientPlayerFriendGetFriendListFailedHandlerFunctionType AsyncClientPlayerFriendGetFriendListFailedHandler;

void CompleteClientPlayerFriendGetFriendList(AsyncClientPlayerFriendGetFriendListGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendGetFriendListGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendGetFriendListPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendGetFriendListGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendGetFriendListHandler) {
            AsyncClientPlayerFriendGetFriendListHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.GetFriendList reply dropped: AsyncClientPlayerFriendGetFriendListHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendGetFriendListFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.GetFriendList", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendGetFriendListFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.GetFriendList failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendGetFriendList(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendGetFriendListGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendGetFriendList(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendGetFriendListCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::GetFriendListResponse> done) {
    auto call(ClientPlayerFriendGetFriendListPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendGetFriendList(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetFriendList(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendGetFriendListMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request,
                                            GrpcDone<::friendpb::GetFriendListResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendGetFriendListCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendGetFriendList(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetFriendListRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendGetFriendListCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendGetFriendList(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::GetFriendListRequest& derived = static_cast<const ::friendpb::GetFriendListRequest&>(message);
    SendClientPlayerFriendGetFriendList(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendGetPendingRequests
boost::object_pool<AsyncClientPlayerFriendGetPendingRequestsGrpcClient> ClientPlayerFriendGetPendingRequestsPool;
using AsyncClientPlayerFriendGetPendingRequestsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::GetPendingRequestsResponse&)>;
AsyncClientPlayerFriendGetPendingRequestsHandlerFunctionType AsyncClientPlayerFriendGetPendingRequestsHandler;
AsyncClientPlayerFriendGetPendingRequestsFailedHandlerFunctionType AsyncClientPlayerFriendGetPendingRequestsFailedHandler;

void CompleteClientPlayerFriendGetPendingRequests(AsyncClientPlayerFriendGetPendingRequestsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendGetPendingRequestsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendGetPendingRequestsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendGetPendingRequestsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendGetPendingRequestsHandler) {
            AsyncClientPlayerFriendGetPendingRequestsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.GetPendingRequests reply dropped: AsyncClientPlayerFriendGetPendingRequestsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendGetPendingRequestsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.GetPendingRequests", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendGetPendingRequestsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.GetPendingRequests failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendGetPendingRequests(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendGetPendingRequestsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendGetPendingRequests(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendGetPendingRequestsCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::GetPendingRequestsResponse> done) {
    auto call(ClientPlayerFriendGetPendingRequestsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendGetPendingRequests(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetPendingRequests(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendGetPendingRequestsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request,
                                            GrpcDone<::friendpb::GetPendingRequestsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendGetPendingRequestsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendGetPendingRequests(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::GetPendingRequestsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendGetPendingRequestsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendGetPendingRequests(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::GetPendingRequestsRequest& derived = static_cast<const ::friendpb::GetPendingRequestsRequest&>(message);
    SendClientPlayerFriendGetPendingRequests(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendBlock
boost::object_pool<AsyncClientPlayerFriendBlockGrpcClient> ClientPlayerFriendBlockPool;
using AsyncClientPlayerFriendBlockHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::BlockResponse&)>;
AsyncClientPlayerFriendBlockHandlerFunctionType AsyncClientPlayerFriendBlockHandler;
AsyncClientPlayerFriendBlockFailedHandlerFunctionType AsyncClientPlayerFriendBlockFailedHandler;

void CompleteClientPlayerFriendBlock(AsyncClientPlayerFriendBlockGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendBlockGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendBlockPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendBlockGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendBlockHandler) {
            AsyncClientPlayerFriendBlockHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.Block reply dropped: AsyncClientPlayerFriendBlockHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendBlockFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.Block", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendBlockFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.Block failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendBlock(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendBlockGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendBlock(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendBlockCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::BlockResponse> done) {
    auto call(ClientPlayerFriendBlockPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendBlock(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncBlock(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendBlockMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request,
                                            GrpcDone<::friendpb::BlockResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendBlockCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendBlock(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::BlockRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendBlockCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendBlock(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::BlockRequest& derived = static_cast<const ::friendpb::BlockRequest&>(message);
    SendClientPlayerFriendBlock(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendUnblock
boost::object_pool<AsyncClientPlayerFriendUnblockGrpcClient> ClientPlayerFriendUnblockPool;
using AsyncClientPlayerFriendUnblockHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::UnblockResponse&)>;
AsyncClientPlayerFriendUnblockHandlerFunctionType AsyncClientPlayerFriendUnblockHandler;
AsyncClientPlayerFriendUnblockFailedHandlerFunctionType AsyncClientPlayerFriendUnblockFailedHandler;

void CompleteClientPlayerFriendUnblock(AsyncClientPlayerFriendUnblockGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendUnblockGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendUnblockPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendUnblockGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendUnblockHandler) {
            AsyncClientPlayerFriendUnblockHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.Unblock reply dropped: AsyncClientPlayerFriendUnblockHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendUnblockFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.Unblock", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendUnblockFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.Unblock failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendUnblock(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendUnblockGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendUnblock(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendUnblockCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::UnblockResponse> done) {
    auto call(ClientPlayerFriendUnblockPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendUnblock(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncUnblock(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendUnblockMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request,
                                            GrpcDone<::friendpb::UnblockResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendUnblockCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendUnblock(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::UnblockRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendUnblockCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendUnblock(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::UnblockRequest& derived = static_cast<const ::friendpb::UnblockRequest&>(message);
    SendClientPlayerFriendUnblock(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendListBlocks
boost::object_pool<AsyncClientPlayerFriendListBlocksGrpcClient> ClientPlayerFriendListBlocksPool;
using AsyncClientPlayerFriendListBlocksHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::ListBlocksResponse&)>;
AsyncClientPlayerFriendListBlocksHandlerFunctionType AsyncClientPlayerFriendListBlocksHandler;
AsyncClientPlayerFriendListBlocksFailedHandlerFunctionType AsyncClientPlayerFriendListBlocksFailedHandler;

void CompleteClientPlayerFriendListBlocks(AsyncClientPlayerFriendListBlocksGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendListBlocksGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendListBlocksPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendListBlocksGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendListBlocksHandler) {
            AsyncClientPlayerFriendListBlocksHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.ListBlocks reply dropped: AsyncClientPlayerFriendListBlocksHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendListBlocksFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.ListBlocks", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendListBlocksFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.ListBlocks failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendListBlocks(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendListBlocksGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendListBlocks(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendListBlocksCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::ListBlocksResponse> done) {
    auto call(ClientPlayerFriendListBlocksPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendListBlocks(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListBlocks(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendListBlocksMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request,
                                            GrpcDone<::friendpb::ListBlocksResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendListBlocksCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendListBlocks(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::ListBlocksRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendListBlocksCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendListBlocks(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::ListBlocksRequest& derived = static_cast<const ::friendpb::ListBlocksRequest&>(message);
    SendClientPlayerFriendListBlocks(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendRecommendFriends
boost::object_pool<AsyncClientPlayerFriendRecommendFriendsGrpcClient> ClientPlayerFriendRecommendFriendsPool;
using AsyncClientPlayerFriendRecommendFriendsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::friendpb::RecommendFriendsResponse&)>;
AsyncClientPlayerFriendRecommendFriendsHandlerFunctionType AsyncClientPlayerFriendRecommendFriendsHandler;
AsyncClientPlayerFriendRecommendFriendsFailedHandlerFunctionType AsyncClientPlayerFriendRecommendFriendsFailedHandler;

void CompleteClientPlayerFriendRecommendFriends(AsyncClientPlayerFriendRecommendFriendsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendRecommendFriendsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendRecommendFriendsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendRecommendFriendsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendRecommendFriendsHandler) {
            AsyncClientPlayerFriendRecommendFriendsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.RecommendFriends reply dropped: AsyncClientPlayerFriendRecommendFriendsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendRecommendFriendsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.RecommendFriends", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendRecommendFriendsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.RecommendFriends failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendRecommendFriends(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendRecommendFriendsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendRecommendFriends(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendRecommendFriendsCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::friendpb::RecommendFriendsResponse> done) {
    auto call(ClientPlayerFriendRecommendFriendsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendRecommendFriends(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRecommendFriends(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendRecommendFriendsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request,
                                            GrpcDone<::friendpb::RecommendFriendsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendRecommendFriendsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendRecommendFriends(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::RecommendFriendsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendRecommendFriendsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendRecommendFriends(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::RecommendFriendsRequest& derived = static_cast<const ::friendpb::RecommendFriendsRequest&>(message);
    SendClientPlayerFriendRecommendFriends(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerFriendNotifyFriendEvent
boost::object_pool<AsyncClientPlayerFriendNotifyFriendEventGrpcClient> ClientPlayerFriendNotifyFriendEventPool;
using AsyncClientPlayerFriendNotifyFriendEventHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncClientPlayerFriendNotifyFriendEventHandlerFunctionType AsyncClientPlayerFriendNotifyFriendEventHandler;
AsyncClientPlayerFriendNotifyFriendEventFailedHandlerFunctionType AsyncClientPlayerFriendNotifyFriendEventFailedHandler;

void CompleteClientPlayerFriendNotifyFriendEvent(AsyncClientPlayerFriendNotifyFriendEventGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerFriendNotifyFriendEventGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerFriendNotifyFriendEventPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerFriendNotifyFriendEventGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerFriendNotifyFriendEventHandler) {
            AsyncClientPlayerFriendNotifyFriendEventHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerFriend.NotifyFriendEvent reply dropped: AsyncClientPlayerFriendNotifyFriendEventHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerFriendNotifyFriendEventFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerFriend.NotifyFriendEvent", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerFriendNotifyFriendEventFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerFriend.NotifyFriendEvent failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerFriendNotifyFriendEvent(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerFriendNotifyFriendEventGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerFriendNotifyFriendEvent(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerFriendNotifyFriendEventCall(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call(ClientPlayerFriendNotifyFriendEventPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerFriendStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerFriendNotifyFriendEvent(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncNotifyFriendEvent(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerFriendNotifyFriendEventMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerFriendNotifyFriendEventCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerFriendNotifyFriendEvent(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const ::friendpb::FriendEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerFriendNotifyFriendEventCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerFriendNotifyFriendEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::friendpb::FriendEventS2C& derived = static_cast<const ::friendpb::FriendEventS2C&>(message);
    SendClientPlayerFriendNotifyFriendEvent(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleFriendCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case ClientPlayerFriendAddFriendMessageId:
            AsyncCompleteGrpcClientPlayerFriendAddFriend(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendAcceptFriendMessageId:
            AsyncCompleteGrpcClientPlayerFriendAcceptFriend(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendRejectFriendMessageId:
            AsyncCompleteGrpcClientPlayerFriendRejectFriend(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendRemoveFriendMessageId:
            AsyncCompleteGrpcClientPlayerFriendRemoveFriend(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendGetFriendListMessageId:
            AsyncCompleteGrpcClientPlayerFriendGetFriendList(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendGetPendingRequestsMessageId:
            AsyncCompleteGrpcClientPlayerFriendGetPendingRequests(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendBlockMessageId:
            AsyncCompleteGrpcClientPlayerFriendBlock(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendUnblockMessageId:
            AsyncCompleteGrpcClientPlayerFriendUnblock(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendListBlocksMessageId:
            AsyncCompleteGrpcClientPlayerFriendListBlocks(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendRecommendFriendsMessageId:
            AsyncCompleteGrpcClientPlayerFriendRecommendFriends(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerFriendNotifyFriendEventMessageId:
            AsyncCompleteGrpcClientPlayerFriendNotifyFriendEvent(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetFriendHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncClientPlayerFriendAddFriendHandler = handler;
    AsyncClientPlayerFriendAcceptFriendHandler = handler;
    AsyncClientPlayerFriendRejectFriendHandler = handler;
    AsyncClientPlayerFriendRemoveFriendHandler = handler;
    AsyncClientPlayerFriendGetFriendListHandler = handler;
    AsyncClientPlayerFriendGetPendingRequestsHandler = handler;
    AsyncClientPlayerFriendBlockHandler = handler;
    AsyncClientPlayerFriendUnblockHandler = handler;
    AsyncClientPlayerFriendListBlocksHandler = handler;
    AsyncClientPlayerFriendRecommendFriendsHandler = handler;
    AsyncClientPlayerFriendNotifyFriendEventHandler = handler;
}

void SetFriendIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncClientPlayerFriendAddFriendHandler) {
        AsyncClientPlayerFriendAddFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendAcceptFriendHandler) {
        AsyncClientPlayerFriendAcceptFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendRejectFriendHandler) {
        AsyncClientPlayerFriendRejectFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendRemoveFriendHandler) {
        AsyncClientPlayerFriendRemoveFriendHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetFriendListHandler) {
        AsyncClientPlayerFriendGetFriendListHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetPendingRequestsHandler) {
        AsyncClientPlayerFriendGetPendingRequestsHandler = handler;
    }
    if (!AsyncClientPlayerFriendBlockHandler) {
        AsyncClientPlayerFriendBlockHandler = handler;
    }
    if (!AsyncClientPlayerFriendUnblockHandler) {
        AsyncClientPlayerFriendUnblockHandler = handler;
    }
    if (!AsyncClientPlayerFriendListBlocksHandler) {
        AsyncClientPlayerFriendListBlocksHandler = handler;
    }
    if (!AsyncClientPlayerFriendRecommendFriendsHandler) {
        AsyncClientPlayerFriendRecommendFriendsHandler = handler;
    }
    if (!AsyncClientPlayerFriendNotifyFriendEventHandler) {
        AsyncClientPlayerFriendNotifyFriendEventHandler = handler;
    }
}

void SetFriendFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerFriendAddFriendFailedHandler = handler;
    AsyncClientPlayerFriendAcceptFriendFailedHandler = handler;
    AsyncClientPlayerFriendRejectFriendFailedHandler = handler;
    AsyncClientPlayerFriendRemoveFriendFailedHandler = handler;
    AsyncClientPlayerFriendGetFriendListFailedHandler = handler;
    AsyncClientPlayerFriendGetPendingRequestsFailedHandler = handler;
    AsyncClientPlayerFriendBlockFailedHandler = handler;
    AsyncClientPlayerFriendUnblockFailedHandler = handler;
    AsyncClientPlayerFriendListBlocksFailedHandler = handler;
    AsyncClientPlayerFriendRecommendFriendsFailedHandler = handler;
    AsyncClientPlayerFriendNotifyFriendEventFailedHandler = handler;
}

void SetFriendIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerFriendAddFriendFailedHandler) {
        AsyncClientPlayerFriendAddFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendAcceptFriendFailedHandler) {
        AsyncClientPlayerFriendAcceptFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendRejectFriendFailedHandler) {
        AsyncClientPlayerFriendRejectFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendRemoveFriendFailedHandler) {
        AsyncClientPlayerFriendRemoveFriendFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetFriendListFailedHandler) {
        AsyncClientPlayerFriendGetFriendListFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendGetPendingRequestsFailedHandler) {
        AsyncClientPlayerFriendGetPendingRequestsFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendBlockFailedHandler) {
        AsyncClientPlayerFriendBlockFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendUnblockFailedHandler) {
        AsyncClientPlayerFriendUnblockFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendListBlocksFailedHandler) {
        AsyncClientPlayerFriendListBlocksFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendRecommendFriendsFailedHandler) {
        AsyncClientPlayerFriendRecommendFriendsFailedHandler = handler;
    }
    if (!AsyncClientPlayerFriendNotifyFriendEventFailedHandler) {
        AsyncClientPlayerFriendNotifyFriendEventFailedHandler = handler;
    }
}

void SetFriendCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitFriendGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<ClientPlayerFriendStubPtr>(nodeEntity, ClientPlayerFriend::NewStub(channel));

}

}// namespace friendpb
