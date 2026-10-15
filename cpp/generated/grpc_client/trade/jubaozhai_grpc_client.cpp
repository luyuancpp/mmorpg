#include "muduo/base/Logging.h"

#include "jubaozhai_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetJubaozhaiCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace trade {
struct JubaozhaiCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region ClientPlayerJubaozhaiBrowseListings
boost::object_pool<AsyncClientPlayerJubaozhaiBrowseListingsGrpcClient> ClientPlayerJubaozhaiBrowseListingsPool;
using AsyncClientPlayerJubaozhaiBrowseListingsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::trade::BrowseListingsResponse&)>;
AsyncClientPlayerJubaozhaiBrowseListingsHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseListingsHandler;
AsyncClientPlayerJubaozhaiBrowseListingsFailedHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler;

void CompleteClientPlayerJubaozhaiBrowseListings(AsyncClientPlayerJubaozhaiBrowseListingsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerJubaozhaiBrowseListingsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerJubaozhaiBrowseListingsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerJubaozhaiBrowseListingsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerJubaozhaiBrowseListingsHandler) {
            AsyncClientPlayerJubaozhaiBrowseListingsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.BrowseListings reply dropped: AsyncClientPlayerJubaozhaiBrowseListingsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerJubaozhai.BrowseListings", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.BrowseListings failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerJubaozhaiBrowseListings(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerJubaozhaiBrowseListingsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerJubaozhaiBrowseListings(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiBrowseListingsCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::BrowseListingsResponse> done) {
    auto call(ClientPlayerJubaozhaiBrowseListingsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerJubaozhaiStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerJubaozhaiBrowseListings(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncBrowseListings(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerJubaozhaiBrowseListingsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request,
                                            GrpcDone<::trade::BrowseListingsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiBrowseListingsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiBrowseListings(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiBrowseListingsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::trade::BrowseListingsRequest& derived = static_cast<const ::trade::BrowseListingsRequest&>(message);
    SendClientPlayerJubaozhaiBrowseListings(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetListingDetail
boost::object_pool<AsyncClientPlayerJubaozhaiGetListingDetailGrpcClient> ClientPlayerJubaozhaiGetListingDetailPool;
using AsyncClientPlayerJubaozhaiGetListingDetailHandlerFunctionType =
    std::function<void(const ClientContext&, const ::trade::GetListingDetailResponse&)>;
AsyncClientPlayerJubaozhaiGetListingDetailHandlerFunctionType AsyncClientPlayerJubaozhaiGetListingDetailHandler;
AsyncClientPlayerJubaozhaiGetListingDetailFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler;

void CompleteClientPlayerJubaozhaiGetListingDetail(AsyncClientPlayerJubaozhaiGetListingDetailGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerJubaozhaiGetListingDetailGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerJubaozhaiGetListingDetailPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerJubaozhaiGetListingDetailGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerJubaozhaiGetListingDetailHandler) {
            AsyncClientPlayerJubaozhaiGetListingDetailHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetListingDetail reply dropped: AsyncClientPlayerJubaozhaiGetListingDetailHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerJubaozhai.GetListingDetail", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetListingDetail failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerJubaozhaiGetListingDetail(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerJubaozhaiGetListingDetailGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerJubaozhaiGetListingDetail(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiGetListingDetailCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::GetListingDetailResponse> done) {
    auto call(ClientPlayerJubaozhaiGetListingDetailPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerJubaozhaiStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerJubaozhaiGetListingDetail(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetListingDetail(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerJubaozhaiGetListingDetailMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request,
                                            GrpcDone<::trade::GetListingDetailResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiGetListingDetailCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiGetListingDetail(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiGetListingDetailCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::trade::GetListingDetailRequest& derived = static_cast<const ::trade::GetListingDetailRequest&>(message);
    SendClientPlayerJubaozhaiGetListingDetail(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiSetFavorite
boost::object_pool<AsyncClientPlayerJubaozhaiSetFavoriteGrpcClient> ClientPlayerJubaozhaiSetFavoritePool;
using AsyncClientPlayerJubaozhaiSetFavoriteHandlerFunctionType =
    std::function<void(const ClientContext&, const ::trade::SetFavoriteResponse&)>;
AsyncClientPlayerJubaozhaiSetFavoriteHandlerFunctionType AsyncClientPlayerJubaozhaiSetFavoriteHandler;
AsyncClientPlayerJubaozhaiSetFavoriteFailedHandlerFunctionType AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler;

void CompleteClientPlayerJubaozhaiSetFavorite(AsyncClientPlayerJubaozhaiSetFavoriteGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerJubaozhaiSetFavoriteGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerJubaozhaiSetFavoritePool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerJubaozhaiSetFavoriteGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerJubaozhaiSetFavoriteHandler) {
            AsyncClientPlayerJubaozhaiSetFavoriteHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.SetFavorite reply dropped: AsyncClientPlayerJubaozhaiSetFavoriteHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerJubaozhai.SetFavorite", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.SetFavorite failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerJubaozhaiSetFavorite(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerJubaozhaiSetFavoriteGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerJubaozhaiSetFavorite(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiSetFavoriteCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::SetFavoriteResponse> done) {
    auto call(ClientPlayerJubaozhaiSetFavoritePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerJubaozhaiStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerJubaozhaiSetFavorite(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSetFavorite(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerJubaozhaiSetFavoriteMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request,
                                            GrpcDone<::trade::SetFavoriteResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiSetFavoriteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiSetFavorite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiSetFavoriteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::trade::SetFavoriteRequest& derived = static_cast<const ::trade::SetFavoriteRequest&>(message);
    SendClientPlayerJubaozhaiSetFavorite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetMyShelf
boost::object_pool<AsyncClientPlayerJubaozhaiGetMyShelfGrpcClient> ClientPlayerJubaozhaiGetMyShelfPool;
using AsyncClientPlayerJubaozhaiGetMyShelfHandlerFunctionType =
    std::function<void(const ClientContext&, const ::trade::GetMyShelfResponse&)>;
AsyncClientPlayerJubaozhaiGetMyShelfHandlerFunctionType AsyncClientPlayerJubaozhaiGetMyShelfHandler;
AsyncClientPlayerJubaozhaiGetMyShelfFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler;

void CompleteClientPlayerJubaozhaiGetMyShelf(AsyncClientPlayerJubaozhaiGetMyShelfGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncClientPlayerJubaozhaiGetMyShelfGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        ClientPlayerJubaozhaiGetMyShelfPool.destroy(completed);
    };
    const std::unique_ptr<AsyncClientPlayerJubaozhaiGetMyShelfGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncClientPlayerJubaozhaiGetMyShelfHandler) {
            AsyncClientPlayerJubaozhaiGetMyShelfHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetMyShelf reply dropped: AsyncClientPlayerJubaozhaiGetMyShelfHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "ClientPlayerJubaozhai.GetMyShelf", call->context, call->status, call->sentMetadata};
        AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetMyShelf failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcClientPlayerJubaozhaiGetMyShelf(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncClientPlayerJubaozhaiGetMyShelfGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteClientPlayerJubaozhaiGetMyShelf(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiGetMyShelfCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::GetMyShelfResponse> done) {
    auto call(ClientPlayerJubaozhaiGetMyShelfPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<ClientPlayerJubaozhaiStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteClientPlayerJubaozhaiGetMyShelf(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetMyShelf(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(ClientPlayerJubaozhaiGetMyShelfMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request,
                                            GrpcDone<::trade::GetMyShelfResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiGetMyShelfCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiGetMyShelf(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiGetMyShelfCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::trade::GetMyShelfRequest& derived = static_cast<const ::trade::GetMyShelfRequest&>(message);
    SendClientPlayerJubaozhaiGetMyShelf(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleJubaozhaiCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case ClientPlayerJubaozhaiBrowseListingsMessageId:
            AsyncCompleteGrpcClientPlayerJubaozhaiBrowseListings(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerJubaozhaiGetListingDetailMessageId:
            AsyncCompleteGrpcClientPlayerJubaozhaiGetListingDetail(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerJubaozhaiSetFavoriteMessageId:
            AsyncCompleteGrpcClientPlayerJubaozhaiSetFavorite(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case ClientPlayerJubaozhaiGetMyShelfMessageId:
            AsyncCompleteGrpcClientPlayerJubaozhaiGetMyShelf(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetJubaozhaiHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncClientPlayerJubaozhaiBrowseListingsHandler = handler;
    AsyncClientPlayerJubaozhaiGetListingDetailHandler = handler;
    AsyncClientPlayerJubaozhaiSetFavoriteHandler = handler;
    AsyncClientPlayerJubaozhaiGetMyShelfHandler = handler;
}

void SetJubaozhaiIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncClientPlayerJubaozhaiBrowseListingsHandler) {
        AsyncClientPlayerJubaozhaiBrowseListingsHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetListingDetailHandler) {
        AsyncClientPlayerJubaozhaiGetListingDetailHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiSetFavoriteHandler) {
        AsyncClientPlayerJubaozhaiSetFavoriteHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetMyShelfHandler) {
        AsyncClientPlayerJubaozhaiGetMyShelfHandler = handler;
    }
}

void SetJubaozhaiFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler = handler;
    AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler = handler;
}

void SetJubaozhaiIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler) {
        AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler) {
        AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler) {
        AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler) {
        AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler = handler;
    }
}

void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitJubaozhaiGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<ClientPlayerJubaozhaiStubPtr>(nodeEntity, ClientPlayerJubaozhai::NewStub(channel));

}

}// namespace trade
