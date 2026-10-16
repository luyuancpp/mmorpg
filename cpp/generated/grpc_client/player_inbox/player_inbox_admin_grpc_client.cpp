#include "muduo/base/Logging.h"

#include "player_inbox_admin_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetPlayerInboxAdminCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace player_inbox {
struct PlayerInboxAdminCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region PlayerInboxAdminGmSanctionPlayer
boost::object_pool<AsyncPlayerInboxAdminGmSanctionPlayerGrpcClient> PlayerInboxAdminGmSanctionPlayerPool;
using AsyncPlayerInboxAdminGmSanctionPlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::GmSanctionPlayerResponse&)>;
AsyncPlayerInboxAdminGmSanctionPlayerHandlerFunctionType AsyncPlayerInboxAdminGmSanctionPlayerHandler;
AsyncPlayerInboxAdminGmSanctionPlayerFailedHandlerFunctionType AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler;

void CompletePlayerInboxAdminGmSanctionPlayer(AsyncPlayerInboxAdminGmSanctionPlayerGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxAdminGmSanctionPlayerGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxAdminGmSanctionPlayerPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxAdminGmSanctionPlayerGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxAdminGmSanctionPlayerHandler) {
            AsyncPlayerInboxAdminGmSanctionPlayerHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInboxAdmin.GmSanctionPlayer reply dropped: AsyncPlayerInboxAdminGmSanctionPlayerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInboxAdmin.GmSanctionPlayer", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.GmSanctionPlayer failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxAdminGmSanctionPlayer(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxAdminGmSanctionPlayerGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxAdminGmSanctionPlayer(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminGmSanctionPlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::GmSanctionPlayerResponse> done) {
    auto call(PlayerInboxAdminGmSanctionPlayerPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxAdminStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxAdminGmSanctionPlayer(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGmSanctionPlayer(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxAdminGmSanctionPlayerMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request,
                                            GrpcDone<::player_inbox::GmSanctionPlayerResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminGmSanctionPlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminGmSanctionPlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminGmSanctionPlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::GmSanctionPlayerRequest& derived = static_cast<const ::player_inbox::GmSanctionPlayerRequest&>(message);
    SendPlayerInboxAdminGmSanctionPlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminGetSummary
boost::object_pool<AsyncPlayerInboxAdminGetSummaryGrpcClient> PlayerInboxAdminGetSummaryPool;
using AsyncPlayerInboxAdminGetSummaryHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::GetPlayerInboxSummaryResponse&)>;
AsyncPlayerInboxAdminGetSummaryHandlerFunctionType AsyncPlayerInboxAdminGetSummaryHandler;
AsyncPlayerInboxAdminGetSummaryFailedHandlerFunctionType AsyncPlayerInboxAdminGetSummaryFailedHandler;

void CompletePlayerInboxAdminGetSummary(AsyncPlayerInboxAdminGetSummaryGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxAdminGetSummaryGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxAdminGetSummaryPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxAdminGetSummaryGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxAdminGetSummaryHandler) {
            AsyncPlayerInboxAdminGetSummaryHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInboxAdmin.GetSummary reply dropped: AsyncPlayerInboxAdminGetSummaryHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxAdminGetSummaryFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInboxAdmin.GetSummary", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxAdminGetSummaryFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.GetSummary failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxAdminGetSummary(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxAdminGetSummaryGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxAdminGetSummary(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminGetSummaryCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::GetPlayerInboxSummaryResponse> done) {
    auto call(PlayerInboxAdminGetSummaryPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxAdminStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxAdminGetSummary(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetSummary(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxAdminGetSummaryMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request,
                                            GrpcDone<::player_inbox::GetPlayerInboxSummaryResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminGetSummaryCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminGetSummary(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminGetSummaryCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::GetPlayerInboxSummaryRequest& derived = static_cast<const ::player_inbox::GetPlayerInboxSummaryRequest&>(message);
    SendPlayerInboxAdminGetSummary(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminListItems
boost::object_pool<AsyncPlayerInboxAdminListItemsGrpcClient> PlayerInboxAdminListItemsPool;
using AsyncPlayerInboxAdminListItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::ListPlayerInboxItemsResponse&)>;
AsyncPlayerInboxAdminListItemsHandlerFunctionType AsyncPlayerInboxAdminListItemsHandler;
AsyncPlayerInboxAdminListItemsFailedHandlerFunctionType AsyncPlayerInboxAdminListItemsFailedHandler;

void CompletePlayerInboxAdminListItems(AsyncPlayerInboxAdminListItemsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxAdminListItemsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxAdminListItemsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxAdminListItemsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxAdminListItemsHandler) {
            AsyncPlayerInboxAdminListItemsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInboxAdmin.ListItems reply dropped: AsyncPlayerInboxAdminListItemsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxAdminListItemsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInboxAdmin.ListItems", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxAdminListItemsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.ListItems failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxAdminListItems(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxAdminListItemsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxAdminListItems(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminListItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::ListPlayerInboxItemsResponse> done) {
    auto call(PlayerInboxAdminListItemsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxAdminStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxAdminListItems(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListItems(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxAdminListItemsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request,
                                            GrpcDone<::player_inbox::ListPlayerInboxItemsResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminListItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminListItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminListItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::ListPlayerInboxItemsRequest& derived = static_cast<const ::player_inbox::ListPlayerInboxItemsRequest&>(message);
    SendPlayerInboxAdminListItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminLookupByRequest
boost::object_pool<AsyncPlayerInboxAdminLookupByRequestGrpcClient> PlayerInboxAdminLookupByRequestPool;
using AsyncPlayerInboxAdminLookupByRequestHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::LookupPlayerInboxResponse&)>;
AsyncPlayerInboxAdminLookupByRequestHandlerFunctionType AsyncPlayerInboxAdminLookupByRequestHandler;
AsyncPlayerInboxAdminLookupByRequestFailedHandlerFunctionType AsyncPlayerInboxAdminLookupByRequestFailedHandler;

void CompletePlayerInboxAdminLookupByRequest(AsyncPlayerInboxAdminLookupByRequestGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxAdminLookupByRequestGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxAdminLookupByRequestPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxAdminLookupByRequestGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxAdminLookupByRequestHandler) {
            AsyncPlayerInboxAdminLookupByRequestHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInboxAdmin.LookupByRequest reply dropped: AsyncPlayerInboxAdminLookupByRequestHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxAdminLookupByRequestFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInboxAdmin.LookupByRequest", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxAdminLookupByRequestFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.LookupByRequest failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxAdminLookupByRequest(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxAdminLookupByRequestGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxAdminLookupByRequest(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminLookupByRequestCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::LookupPlayerInboxResponse> done) {
    auto call(PlayerInboxAdminLookupByRequestPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxAdminStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxAdminLookupByRequest(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLookupByRequest(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxAdminLookupByRequestMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::LookupPlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminLookupByRequestCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminLookupByRequest(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminLookupByRequestCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::LookupPlayerInboxRequest& derived = static_cast<const ::player_inbox::LookupPlayerInboxRequest&>(message);
    SendPlayerInboxAdminLookupByRequest(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminVoidItem
boost::object_pool<AsyncPlayerInboxAdminVoidItemGrpcClient> PlayerInboxAdminVoidItemPool;
using AsyncPlayerInboxAdminVoidItemHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::VoidPlayerInboxItemResponse&)>;
AsyncPlayerInboxAdminVoidItemHandlerFunctionType AsyncPlayerInboxAdminVoidItemHandler;
AsyncPlayerInboxAdminVoidItemFailedHandlerFunctionType AsyncPlayerInboxAdminVoidItemFailedHandler;

void CompletePlayerInboxAdminVoidItem(AsyncPlayerInboxAdminVoidItemGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxAdminVoidItemGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxAdminVoidItemPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxAdminVoidItemGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxAdminVoidItemHandler) {
            AsyncPlayerInboxAdminVoidItemHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInboxAdmin.VoidItem reply dropped: AsyncPlayerInboxAdminVoidItemHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxAdminVoidItemFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInboxAdmin.VoidItem", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxAdminVoidItemFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.VoidItem failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxAdminVoidItem(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxAdminVoidItemGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxAdminVoidItem(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminVoidItemCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::VoidPlayerInboxItemResponse> done) {
    auto call(PlayerInboxAdminVoidItemPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxAdminStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxAdminVoidItem(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncVoidItem(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxAdminVoidItemMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request,
                                            GrpcDone<::player_inbox::VoidPlayerInboxItemResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminVoidItemCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminVoidItem(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminVoidItemCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::VoidPlayerInboxItemRequest& derived = static_cast<const ::player_inbox::VoidPlayerInboxItemRequest&>(message);
    SendPlayerInboxAdminVoidItem(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminReconcilePlayer
boost::object_pool<AsyncPlayerInboxAdminReconcilePlayerGrpcClient> PlayerInboxAdminReconcilePlayerPool;
using AsyncPlayerInboxAdminReconcilePlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::player_inbox::ReconcilePlayerInboxResponse&)>;
AsyncPlayerInboxAdminReconcilePlayerHandlerFunctionType AsyncPlayerInboxAdminReconcilePlayerHandler;
AsyncPlayerInboxAdminReconcilePlayerFailedHandlerFunctionType AsyncPlayerInboxAdminReconcilePlayerFailedHandler;

void CompletePlayerInboxAdminReconcilePlayer(AsyncPlayerInboxAdminReconcilePlayerGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncPlayerInboxAdminReconcilePlayerGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        PlayerInboxAdminReconcilePlayerPool.destroy(completed);
    };
    const std::unique_ptr<AsyncPlayerInboxAdminReconcilePlayerGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncPlayerInboxAdminReconcilePlayerHandler) {
            AsyncPlayerInboxAdminReconcilePlayerHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC PlayerInboxAdmin.ReconcilePlayer reply dropped: AsyncPlayerInboxAdminReconcilePlayerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncPlayerInboxAdminReconcilePlayerFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "PlayerInboxAdmin.ReconcilePlayer", call->context, call->status, call->sentMetadata};
        AsyncPlayerInboxAdminReconcilePlayerFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.ReconcilePlayer failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcPlayerInboxAdminReconcilePlayer(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncPlayerInboxAdminReconcilePlayerGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompletePlayerInboxAdminReconcilePlayer(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminReconcilePlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::ReconcilePlayerInboxResponse> done) {
    auto call(PlayerInboxAdminReconcilePlayerPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<PlayerInboxAdminStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompletePlayerInboxAdminReconcilePlayer(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncReconcilePlayer(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(PlayerInboxAdminReconcilePlayerMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::ReconcilePlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminReconcilePlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminReconcilePlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminReconcilePlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::player_inbox::ReconcilePlayerInboxRequest& derived = static_cast<const ::player_inbox::ReconcilePlayerInboxRequest&>(message);
    SendPlayerInboxAdminReconcilePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandlePlayerInboxAdminCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case PlayerInboxAdminGmSanctionPlayerMessageId:
            AsyncCompleteGrpcPlayerInboxAdminGmSanctionPlayer(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case PlayerInboxAdminGetSummaryMessageId:
            AsyncCompleteGrpcPlayerInboxAdminGetSummary(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case PlayerInboxAdminListItemsMessageId:
            AsyncCompleteGrpcPlayerInboxAdminListItems(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case PlayerInboxAdminLookupByRequestMessageId:
            AsyncCompleteGrpcPlayerInboxAdminLookupByRequest(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case PlayerInboxAdminVoidItemMessageId:
            AsyncCompleteGrpcPlayerInboxAdminVoidItem(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case PlayerInboxAdminReconcilePlayerMessageId:
            AsyncCompleteGrpcPlayerInboxAdminReconcilePlayer(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetPlayerInboxAdminHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncPlayerInboxAdminGmSanctionPlayerHandler = handler;
    AsyncPlayerInboxAdminGetSummaryHandler = handler;
    AsyncPlayerInboxAdminListItemsHandler = handler;
    AsyncPlayerInboxAdminLookupByRequestHandler = handler;
    AsyncPlayerInboxAdminVoidItemHandler = handler;
    AsyncPlayerInboxAdminReconcilePlayerHandler = handler;
}

void SetPlayerInboxAdminIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncPlayerInboxAdminGmSanctionPlayerHandler) {
        AsyncPlayerInboxAdminGmSanctionPlayerHandler = handler;
    }
    if (!AsyncPlayerInboxAdminGetSummaryHandler) {
        AsyncPlayerInboxAdminGetSummaryHandler = handler;
    }
    if (!AsyncPlayerInboxAdminListItemsHandler) {
        AsyncPlayerInboxAdminListItemsHandler = handler;
    }
    if (!AsyncPlayerInboxAdminLookupByRequestHandler) {
        AsyncPlayerInboxAdminLookupByRequestHandler = handler;
    }
    if (!AsyncPlayerInboxAdminVoidItemHandler) {
        AsyncPlayerInboxAdminVoidItemHandler = handler;
    }
    if (!AsyncPlayerInboxAdminReconcilePlayerHandler) {
        AsyncPlayerInboxAdminReconcilePlayerHandler = handler;
    }
}

void SetPlayerInboxAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler = handler;
    AsyncPlayerInboxAdminGetSummaryFailedHandler = handler;
    AsyncPlayerInboxAdminListItemsFailedHandler = handler;
    AsyncPlayerInboxAdminLookupByRequestFailedHandler = handler;
    AsyncPlayerInboxAdminVoidItemFailedHandler = handler;
    AsyncPlayerInboxAdminReconcilePlayerFailedHandler = handler;
}

void SetPlayerInboxAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler) {
        AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler = handler;
    }
    if (!AsyncPlayerInboxAdminGetSummaryFailedHandler) {
        AsyncPlayerInboxAdminGetSummaryFailedHandler = handler;
    }
    if (!AsyncPlayerInboxAdminListItemsFailedHandler) {
        AsyncPlayerInboxAdminListItemsFailedHandler = handler;
    }
    if (!AsyncPlayerInboxAdminLookupByRequestFailedHandler) {
        AsyncPlayerInboxAdminLookupByRequestFailedHandler = handler;
    }
    if (!AsyncPlayerInboxAdminVoidItemFailedHandler) {
        AsyncPlayerInboxAdminVoidItemFailedHandler = handler;
    }
    if (!AsyncPlayerInboxAdminReconcilePlayerFailedHandler) {
        AsyncPlayerInboxAdminReconcilePlayerFailedHandler = handler;
    }
}

void SetPlayerInboxAdminCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitPlayerInboxAdminGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<PlayerInboxAdminStubPtr>(nodeEntity, PlayerInboxAdmin::NewStub(channel));

}

}// namespace player_inbox
