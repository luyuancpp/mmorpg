#include "muduo/base/Logging.h"

#include "data_service_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetDataServiceCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace data_service {
struct DataServiceCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region DataServiceLoadPlayerData
boost::object_pool<AsyncDataServiceLoadPlayerDataGrpcClient> DataServiceLoadPlayerDataPool;
using AsyncDataServiceLoadPlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::LoadPlayerDataResponse&)>;
AsyncDataServiceLoadPlayerDataHandlerFunctionType AsyncDataServiceLoadPlayerDataHandler;
AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType AsyncDataServiceLoadPlayerDataFailedHandler;

void CompleteDataServiceLoadPlayerData(AsyncDataServiceLoadPlayerDataGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceLoadPlayerDataGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceLoadPlayerDataPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceLoadPlayerDataGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceLoadPlayerDataHandler) {
            AsyncDataServiceLoadPlayerDataHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.LoadPlayerData reply dropped: AsyncDataServiceLoadPlayerDataHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceLoadPlayerDataFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.LoadPlayerData", call->context, call->status, call->sentMetadata};
        AsyncDataServiceLoadPlayerDataFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.LoadPlayerData failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceLoadPlayerData(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceLoadPlayerDataGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceLoadPlayerData(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceLoadPlayerDataCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::LoadPlayerDataResponse> done) {
    auto call(DataServiceLoadPlayerDataPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceLoadPlayerData(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLoadPlayerData(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceLoadPlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request,
                                            GrpcDone<::data_service::LoadPlayerDataResponse> done, const GrpcCallOptions& options) {
    StartDataServiceLoadPlayerDataCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceLoadPlayerData(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceLoadPlayerDataCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::LoadPlayerDataRequest& derived = static_cast<const ::data_service::LoadPlayerDataRequest&>(message);
    SendDataServiceLoadPlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceSavePlayerData
boost::object_pool<AsyncDataServiceSavePlayerDataGrpcClient> DataServiceSavePlayerDataPool;
using AsyncDataServiceSavePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SavePlayerDataResponse&)>;
AsyncDataServiceSavePlayerDataHandlerFunctionType AsyncDataServiceSavePlayerDataHandler;
AsyncDataServiceSavePlayerDataFailedHandlerFunctionType AsyncDataServiceSavePlayerDataFailedHandler;

void CompleteDataServiceSavePlayerData(AsyncDataServiceSavePlayerDataGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceSavePlayerDataGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceSavePlayerDataPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceSavePlayerDataGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceSavePlayerDataHandler) {
            AsyncDataServiceSavePlayerDataHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.SavePlayerData reply dropped: AsyncDataServiceSavePlayerDataHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceSavePlayerDataFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.SavePlayerData", call->context, call->status, call->sentMetadata};
        AsyncDataServiceSavePlayerDataFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.SavePlayerData failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceSavePlayerData(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceSavePlayerDataGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceSavePlayerData(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceSavePlayerDataCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::SavePlayerDataResponse> done) {
    auto call(DataServiceSavePlayerDataPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceSavePlayerData(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSavePlayerData(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceSavePlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request,
                                            GrpcDone<::data_service::SavePlayerDataResponse> done, const GrpcCallOptions& options) {
    StartDataServiceSavePlayerDataCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceSavePlayerData(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceSavePlayerDataCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::SavePlayerDataRequest& derived = static_cast<const ::data_service::SavePlayerDataRequest&>(message);
    SendDataServiceSavePlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerField
boost::object_pool<AsyncDataServiceGetPlayerFieldGrpcClient> DataServiceGetPlayerFieldPool;
using AsyncDataServiceGetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerFieldResponse&)>;
AsyncDataServiceGetPlayerFieldHandlerFunctionType AsyncDataServiceGetPlayerFieldHandler;
AsyncDataServiceGetPlayerFieldFailedHandlerFunctionType AsyncDataServiceGetPlayerFieldFailedHandler;

void CompleteDataServiceGetPlayerField(AsyncDataServiceGetPlayerFieldGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceGetPlayerFieldGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceGetPlayerFieldPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceGetPlayerFieldGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceGetPlayerFieldHandler) {
            AsyncDataServiceGetPlayerFieldHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.GetPlayerField reply dropped: AsyncDataServiceGetPlayerFieldHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceGetPlayerFieldFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.GetPlayerField", call->context, call->status, call->sentMetadata};
        AsyncDataServiceGetPlayerFieldFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerField failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceGetPlayerField(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceGetPlayerFieldGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceGetPlayerField(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerFieldCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerFieldResponse> done) {
    auto call(DataServiceGetPlayerFieldPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceGetPlayerField(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetPlayerField(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerFieldMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request,
                                            GrpcDone<::data_service::GetPlayerFieldResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerFieldCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerField(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerFieldCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::GetPlayerFieldRequest& derived = static_cast<const ::data_service::GetPlayerFieldRequest&>(message);
    SendDataServiceGetPlayerField(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceSetPlayerField
boost::object_pool<AsyncDataServiceSetPlayerFieldGrpcClient> DataServiceSetPlayerFieldPool;
using AsyncDataServiceSetPlayerFieldHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::SetPlayerFieldResponse&)>;
AsyncDataServiceSetPlayerFieldHandlerFunctionType AsyncDataServiceSetPlayerFieldHandler;
AsyncDataServiceSetPlayerFieldFailedHandlerFunctionType AsyncDataServiceSetPlayerFieldFailedHandler;

void CompleteDataServiceSetPlayerField(AsyncDataServiceSetPlayerFieldGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceSetPlayerFieldGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceSetPlayerFieldPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceSetPlayerFieldGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceSetPlayerFieldHandler) {
            AsyncDataServiceSetPlayerFieldHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.SetPlayerField reply dropped: AsyncDataServiceSetPlayerFieldHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceSetPlayerFieldFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.SetPlayerField", call->context, call->status, call->sentMetadata};
        AsyncDataServiceSetPlayerFieldFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.SetPlayerField failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceSetPlayerField(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceSetPlayerFieldGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceSetPlayerField(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceSetPlayerFieldCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::SetPlayerFieldResponse> done) {
    auto call(DataServiceSetPlayerFieldPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceSetPlayerField(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncSetPlayerField(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceSetPlayerFieldMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request,
                                            GrpcDone<::data_service::SetPlayerFieldResponse> done, const GrpcCallOptions& options) {
    StartDataServiceSetPlayerFieldCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceSetPlayerField(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceSetPlayerFieldCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::SetPlayerFieldRequest& derived = static_cast<const ::data_service::SetPlayerFieldRequest&>(message);
    SendDataServiceSetPlayerField(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRegisterPlayerZone
boost::object_pool<AsyncDataServiceRegisterPlayerZoneGrpcClient> DataServiceRegisterPlayerZonePool;
using AsyncDataServiceRegisterPlayerZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::google::protobuf::Empty&)>;
AsyncDataServiceRegisterPlayerZoneHandlerFunctionType AsyncDataServiceRegisterPlayerZoneHandler;
AsyncDataServiceRegisterPlayerZoneFailedHandlerFunctionType AsyncDataServiceRegisterPlayerZoneFailedHandler;

void CompleteDataServiceRegisterPlayerZone(AsyncDataServiceRegisterPlayerZoneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceRegisterPlayerZoneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceRegisterPlayerZonePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceRegisterPlayerZoneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceRegisterPlayerZoneHandler) {
            AsyncDataServiceRegisterPlayerZoneHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.RegisterPlayerZone reply dropped: AsyncDataServiceRegisterPlayerZoneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceRegisterPlayerZoneFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.RegisterPlayerZone", call->context, call->status, call->sentMetadata};
        AsyncDataServiceRegisterPlayerZoneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.RegisterPlayerZone failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceRegisterPlayerZone(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceRegisterPlayerZoneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceRegisterPlayerZone(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRegisterPlayerZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::google::protobuf::Empty> done) {
    auto call(DataServiceRegisterPlayerZonePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceRegisterPlayerZone(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRegisterPlayerZone(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceRegisterPlayerZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request,
                                            GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options) {
    StartDataServiceRegisterPlayerZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRegisterPlayerZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRegisterPlayerZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::RegisterPlayerZoneRequest& derived = static_cast<const ::data_service::RegisterPlayerZoneRequest&>(message);
    SendDataServiceRegisterPlayerZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerHomeZone
boost::object_pool<AsyncDataServiceGetPlayerHomeZoneGrpcClient> DataServiceGetPlayerHomeZonePool;
using AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerHomeZoneResponse&)>;
AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneHandler;
AsyncDataServiceGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneFailedHandler;

void CompleteDataServiceGetPlayerHomeZone(AsyncDataServiceGetPlayerHomeZoneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceGetPlayerHomeZoneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceGetPlayerHomeZonePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceGetPlayerHomeZoneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceGetPlayerHomeZoneHandler) {
            AsyncDataServiceGetPlayerHomeZoneHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.GetPlayerHomeZone reply dropped: AsyncDataServiceGetPlayerHomeZoneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceGetPlayerHomeZoneFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.GetPlayerHomeZone", call->context, call->status, call->sentMetadata};
        AsyncDataServiceGetPlayerHomeZoneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerHomeZone failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceGetPlayerHomeZone(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceGetPlayerHomeZoneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceGetPlayerHomeZone(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerHomeZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerHomeZoneResponse> done) {
    auto call(DataServiceGetPlayerHomeZonePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceGetPlayerHomeZone(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetPlayerHomeZone(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerHomeZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request,
                                            GrpcDone<::data_service::GetPlayerHomeZoneResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerHomeZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerHomeZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerHomeZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::GetPlayerHomeZoneRequest& derived = static_cast<const ::data_service::GetPlayerHomeZoneRequest&>(message);
    SendDataServiceGetPlayerHomeZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceBatchGetPlayerHomeZone
boost::object_pool<AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient> DataServiceBatchGetPlayerHomeZonePool;
using AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchGetPlayerHomeZoneResponse&)>;
AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneHandler;
AsyncDataServiceBatchGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler;

void CompleteDataServiceBatchGetPlayerHomeZone(AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceBatchGetPlayerHomeZonePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceBatchGetPlayerHomeZoneHandler) {
            AsyncDataServiceBatchGetPlayerHomeZoneHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.BatchGetPlayerHomeZone reply dropped: AsyncDataServiceBatchGetPlayerHomeZoneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.BatchGetPlayerHomeZone", call->context, call->status, call->sentMetadata};
        AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.BatchGetPlayerHomeZone failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceBatchGetPlayerHomeZone(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceBatchGetPlayerHomeZoneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceBatchGetPlayerHomeZone(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceBatchGetPlayerHomeZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done) {
    auto call(DataServiceBatchGetPlayerHomeZonePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceBatchGetPlayerHomeZone(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncBatchGetPlayerHomeZone(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceBatchGetPlayerHomeZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request,
                                            GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done, const GrpcCallOptions& options) {
    StartDataServiceBatchGetPlayerHomeZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceBatchGetPlayerHomeZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceBatchGetPlayerHomeZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::BatchGetPlayerHomeZoneRequest& derived = static_cast<const ::data_service::BatchGetPlayerHomeZoneRequest&>(message);
    SendDataServiceBatchGetPlayerHomeZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRemapHomeZoneForMerge
boost::object_pool<AsyncDataServiceRemapHomeZoneForMergeGrpcClient> DataServiceRemapHomeZoneForMergePool;
using AsyncDataServiceRemapHomeZoneForMergeHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RemapHomeZoneForMergeResponse&)>;
AsyncDataServiceRemapHomeZoneForMergeHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeHandler;
AsyncDataServiceRemapHomeZoneForMergeFailedHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeFailedHandler;

void CompleteDataServiceRemapHomeZoneForMerge(AsyncDataServiceRemapHomeZoneForMergeGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceRemapHomeZoneForMergeGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceRemapHomeZoneForMergePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceRemapHomeZoneForMergeGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceRemapHomeZoneForMergeHandler) {
            AsyncDataServiceRemapHomeZoneForMergeHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.RemapHomeZoneForMerge reply dropped: AsyncDataServiceRemapHomeZoneForMergeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceRemapHomeZoneForMergeFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.RemapHomeZoneForMerge", call->context, call->status, call->sentMetadata};
        AsyncDataServiceRemapHomeZoneForMergeFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.RemapHomeZoneForMerge failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceRemapHomeZoneForMerge(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceRemapHomeZoneForMergeGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceRemapHomeZoneForMerge(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRemapHomeZoneForMergeCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done) {
    auto call(DataServiceRemapHomeZoneForMergePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceRemapHomeZoneForMerge(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRemapHomeZoneForMerge(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceRemapHomeZoneForMergeMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request,
                                            GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRemapHomeZoneForMergeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRemapHomeZoneForMerge(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRemapHomeZoneForMergeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::RemapHomeZoneForMergeRequest& derived = static_cast<const ::data_service::RemapHomeZoneForMergeRequest&>(message);
    SendDataServiceRemapHomeZoneForMerge(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceDeletePlayerData
boost::object_pool<AsyncDataServiceDeletePlayerDataGrpcClient> DataServiceDeletePlayerDataPool;
using AsyncDataServiceDeletePlayerDataHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::DeletePlayerDataResponse&)>;
AsyncDataServiceDeletePlayerDataHandlerFunctionType AsyncDataServiceDeletePlayerDataHandler;
AsyncDataServiceDeletePlayerDataFailedHandlerFunctionType AsyncDataServiceDeletePlayerDataFailedHandler;

void CompleteDataServiceDeletePlayerData(AsyncDataServiceDeletePlayerDataGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceDeletePlayerDataGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceDeletePlayerDataPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceDeletePlayerDataGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceDeletePlayerDataHandler) {
            AsyncDataServiceDeletePlayerDataHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.DeletePlayerData reply dropped: AsyncDataServiceDeletePlayerDataHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceDeletePlayerDataFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.DeletePlayerData", call->context, call->status, call->sentMetadata};
        AsyncDataServiceDeletePlayerDataFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.DeletePlayerData failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceDeletePlayerData(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceDeletePlayerDataGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceDeletePlayerData(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceDeletePlayerDataCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::DeletePlayerDataResponse> done) {
    auto call(DataServiceDeletePlayerDataPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceDeletePlayerData(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDeletePlayerData(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceDeletePlayerDataMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request,
                                            GrpcDone<::data_service::DeletePlayerDataResponse> done, const GrpcCallOptions& options) {
    StartDataServiceDeletePlayerDataCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceDeletePlayerData(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceDeletePlayerDataCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::DeletePlayerDataRequest& derived = static_cast<const ::data_service::DeletePlayerDataRequest&>(message);
    SendDataServiceDeletePlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceCreatePlayerSnapshot
boost::object_pool<AsyncDataServiceCreatePlayerSnapshotGrpcClient> DataServiceCreatePlayerSnapshotPool;
using AsyncDataServiceCreatePlayerSnapshotHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::CreatePlayerSnapshotResponse&)>;
AsyncDataServiceCreatePlayerSnapshotHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotHandler;
AsyncDataServiceCreatePlayerSnapshotFailedHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotFailedHandler;

void CompleteDataServiceCreatePlayerSnapshot(AsyncDataServiceCreatePlayerSnapshotGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceCreatePlayerSnapshotGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceCreatePlayerSnapshotPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceCreatePlayerSnapshotGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceCreatePlayerSnapshotHandler) {
            AsyncDataServiceCreatePlayerSnapshotHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.CreatePlayerSnapshot reply dropped: AsyncDataServiceCreatePlayerSnapshotHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceCreatePlayerSnapshotFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.CreatePlayerSnapshot", call->context, call->status, call->sentMetadata};
        AsyncDataServiceCreatePlayerSnapshotFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.CreatePlayerSnapshot failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceCreatePlayerSnapshot(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceCreatePlayerSnapshotGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceCreatePlayerSnapshot(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceCreatePlayerSnapshotCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::CreatePlayerSnapshotResponse> done) {
    auto call(DataServiceCreatePlayerSnapshotPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceCreatePlayerSnapshot(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCreatePlayerSnapshot(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceCreatePlayerSnapshotMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request,
                                            GrpcDone<::data_service::CreatePlayerSnapshotResponse> done, const GrpcCallOptions& options) {
    StartDataServiceCreatePlayerSnapshotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceCreatePlayerSnapshot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceCreatePlayerSnapshotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::CreatePlayerSnapshotRequest& derived = static_cast<const ::data_service::CreatePlayerSnapshotRequest&>(message);
    SendDataServiceCreatePlayerSnapshot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceListPlayerSnapshots
boost::object_pool<AsyncDataServiceListPlayerSnapshotsGrpcClient> DataServiceListPlayerSnapshotsPool;
using AsyncDataServiceListPlayerSnapshotsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::ListPlayerSnapshotsResponse&)>;
AsyncDataServiceListPlayerSnapshotsHandlerFunctionType AsyncDataServiceListPlayerSnapshotsHandler;
AsyncDataServiceListPlayerSnapshotsFailedHandlerFunctionType AsyncDataServiceListPlayerSnapshotsFailedHandler;

void CompleteDataServiceListPlayerSnapshots(AsyncDataServiceListPlayerSnapshotsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceListPlayerSnapshotsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceListPlayerSnapshotsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceListPlayerSnapshotsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceListPlayerSnapshotsHandler) {
            AsyncDataServiceListPlayerSnapshotsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.ListPlayerSnapshots reply dropped: AsyncDataServiceListPlayerSnapshotsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceListPlayerSnapshotsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.ListPlayerSnapshots", call->context, call->status, call->sentMetadata};
        AsyncDataServiceListPlayerSnapshotsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.ListPlayerSnapshots failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceListPlayerSnapshots(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceListPlayerSnapshotsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceListPlayerSnapshots(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceListPlayerSnapshotsCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::ListPlayerSnapshotsResponse> done) {
    auto call(DataServiceListPlayerSnapshotsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceListPlayerSnapshots(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncListPlayerSnapshots(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceListPlayerSnapshotsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request,
                                            GrpcDone<::data_service::ListPlayerSnapshotsResponse> done, const GrpcCallOptions& options) {
    StartDataServiceListPlayerSnapshotsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceListPlayerSnapshots(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceListPlayerSnapshotsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::ListPlayerSnapshotsRequest& derived = static_cast<const ::data_service::ListPlayerSnapshotsRequest&>(message);
    SendDataServiceListPlayerSnapshots(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerSnapshotDiff
boost::object_pool<AsyncDataServiceGetPlayerSnapshotDiffGrpcClient> DataServiceGetPlayerSnapshotDiffPool;
using AsyncDataServiceGetPlayerSnapshotDiffHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerSnapshotDiffResponse&)>;
AsyncDataServiceGetPlayerSnapshotDiffHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffHandler;
AsyncDataServiceGetPlayerSnapshotDiffFailedHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffFailedHandler;

void CompleteDataServiceGetPlayerSnapshotDiff(AsyncDataServiceGetPlayerSnapshotDiffGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceGetPlayerSnapshotDiffGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceGetPlayerSnapshotDiffPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceGetPlayerSnapshotDiffGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceGetPlayerSnapshotDiffHandler) {
            AsyncDataServiceGetPlayerSnapshotDiffHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.GetPlayerSnapshotDiff reply dropped: AsyncDataServiceGetPlayerSnapshotDiffHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceGetPlayerSnapshotDiffFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.GetPlayerSnapshotDiff", call->context, call->status, call->sentMetadata};
        AsyncDataServiceGetPlayerSnapshotDiffFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerSnapshotDiff failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceGetPlayerSnapshotDiff(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceGetPlayerSnapshotDiffGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceGetPlayerSnapshotDiff(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerSnapshotDiffCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done) {
    auto call(DataServiceGetPlayerSnapshotDiffPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceGetPlayerSnapshotDiff(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetPlayerSnapshotDiff(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerSnapshotDiffMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request,
                                            GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerSnapshotDiffCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerSnapshotDiff(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerSnapshotDiffCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::GetPlayerSnapshotDiffRequest& derived = static_cast<const ::data_service::GetPlayerSnapshotDiffRequest&>(message);
    SendDataServiceGetPlayerSnapshotDiff(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRollbackPlayer
boost::object_pool<AsyncDataServiceRollbackPlayerGrpcClient> DataServiceRollbackPlayerPool;
using AsyncDataServiceRollbackPlayerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RollbackPlayerResponse&)>;
AsyncDataServiceRollbackPlayerHandlerFunctionType AsyncDataServiceRollbackPlayerHandler;
AsyncDataServiceRollbackPlayerFailedHandlerFunctionType AsyncDataServiceRollbackPlayerFailedHandler;

void CompleteDataServiceRollbackPlayer(AsyncDataServiceRollbackPlayerGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceRollbackPlayerGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceRollbackPlayerPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceRollbackPlayerGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceRollbackPlayerHandler) {
            AsyncDataServiceRollbackPlayerHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.RollbackPlayer reply dropped: AsyncDataServiceRollbackPlayerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceRollbackPlayerFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.RollbackPlayer", call->context, call->status, call->sentMetadata};
        AsyncDataServiceRollbackPlayerFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.RollbackPlayer failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceRollbackPlayer(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceRollbackPlayerGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceRollbackPlayer(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRollbackPlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RollbackPlayerResponse> done) {
    auto call(DataServiceRollbackPlayerPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceRollbackPlayer(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRollbackPlayer(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceRollbackPlayerMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request,
                                            GrpcDone<::data_service::RollbackPlayerResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRollbackPlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRollbackPlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRollbackPlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::RollbackPlayerRequest& derived = static_cast<const ::data_service::RollbackPlayerRequest&>(message);
    SendDataServiceRollbackPlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRollbackZone
boost::object_pool<AsyncDataServiceRollbackZoneGrpcClient> DataServiceRollbackZonePool;
using AsyncDataServiceRollbackZoneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RollbackZoneResponse&)>;
AsyncDataServiceRollbackZoneHandlerFunctionType AsyncDataServiceRollbackZoneHandler;
AsyncDataServiceRollbackZoneFailedHandlerFunctionType AsyncDataServiceRollbackZoneFailedHandler;

void CompleteDataServiceRollbackZone(AsyncDataServiceRollbackZoneGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceRollbackZoneGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceRollbackZonePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceRollbackZoneGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceRollbackZoneHandler) {
            AsyncDataServiceRollbackZoneHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.RollbackZone reply dropped: AsyncDataServiceRollbackZoneHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceRollbackZoneFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.RollbackZone", call->context, call->status, call->sentMetadata};
        AsyncDataServiceRollbackZoneFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.RollbackZone failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceRollbackZone(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceRollbackZoneGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceRollbackZone(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRollbackZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RollbackZoneResponse> done) {
    auto call(DataServiceRollbackZonePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceRollbackZone(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRollbackZone(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceRollbackZoneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request,
                                            GrpcDone<::data_service::RollbackZoneResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRollbackZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRollbackZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRollbackZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::RollbackZoneRequest& derived = static_cast<const ::data_service::RollbackZoneRequest&>(message);
    SendDataServiceRollbackZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRollbackAll
boost::object_pool<AsyncDataServiceRollbackAllGrpcClient> DataServiceRollbackAllPool;
using AsyncDataServiceRollbackAllHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::RollbackAllResponse&)>;
AsyncDataServiceRollbackAllHandlerFunctionType AsyncDataServiceRollbackAllHandler;
AsyncDataServiceRollbackAllFailedHandlerFunctionType AsyncDataServiceRollbackAllFailedHandler;

void CompleteDataServiceRollbackAll(AsyncDataServiceRollbackAllGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceRollbackAllGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceRollbackAllPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceRollbackAllGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceRollbackAllHandler) {
            AsyncDataServiceRollbackAllHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.RollbackAll reply dropped: AsyncDataServiceRollbackAllHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceRollbackAllFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.RollbackAll", call->context, call->status, call->sentMetadata};
        AsyncDataServiceRollbackAllFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.RollbackAll failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceRollbackAll(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceRollbackAllGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceRollbackAll(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRollbackAllCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RollbackAllResponse> done) {
    auto call(DataServiceRollbackAllPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceRollbackAll(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRollbackAll(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceRollbackAllMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request,
                                            GrpcDone<::data_service::RollbackAllResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRollbackAllCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRollbackAll(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRollbackAllCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::RollbackAllRequest& derived = static_cast<const ::data_service::RollbackAllRequest&>(message);
    SendDataServiceRollbackAll(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceBatchRecallItems
boost::object_pool<AsyncDataServiceBatchRecallItemsGrpcClient> DataServiceBatchRecallItemsPool;
using AsyncDataServiceBatchRecallItemsHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchRecallItemsResponse&)>;
AsyncDataServiceBatchRecallItemsHandlerFunctionType AsyncDataServiceBatchRecallItemsHandler;
AsyncDataServiceBatchRecallItemsFailedHandlerFunctionType AsyncDataServiceBatchRecallItemsFailedHandler;

void CompleteDataServiceBatchRecallItems(AsyncDataServiceBatchRecallItemsGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceBatchRecallItemsGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceBatchRecallItemsPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceBatchRecallItemsGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceBatchRecallItemsHandler) {
            AsyncDataServiceBatchRecallItemsHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.BatchRecallItems reply dropped: AsyncDataServiceBatchRecallItemsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceBatchRecallItemsFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.BatchRecallItems", call->context, call->status, call->sentMetadata};
        AsyncDataServiceBatchRecallItemsFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.BatchRecallItems failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceBatchRecallItems(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceBatchRecallItemsGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceBatchRecallItems(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceBatchRecallItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::BatchRecallItemsResponse> done) {
    auto call(DataServiceBatchRecallItemsPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceBatchRecallItems(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncBatchRecallItems(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceBatchRecallItemsMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request,
                                            GrpcDone<::data_service::BatchRecallItemsResponse> done, const GrpcCallOptions& options) {
    StartDataServiceBatchRecallItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceBatchRecallItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceBatchRecallItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::BatchRecallItemsRequest& derived = static_cast<const ::data_service::BatchRecallItemsRequest&>(message);
    SendDataServiceBatchRecallItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceQueryTransactionLog
boost::object_pool<AsyncDataServiceQueryTransactionLogGrpcClient> DataServiceQueryTransactionLogPool;
using AsyncDataServiceQueryTransactionLogHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::QueryTransactionLogResponse&)>;
AsyncDataServiceQueryTransactionLogHandlerFunctionType AsyncDataServiceQueryTransactionLogHandler;
AsyncDataServiceQueryTransactionLogFailedHandlerFunctionType AsyncDataServiceQueryTransactionLogFailedHandler;

void CompleteDataServiceQueryTransactionLog(AsyncDataServiceQueryTransactionLogGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceQueryTransactionLogGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceQueryTransactionLogPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceQueryTransactionLogGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceQueryTransactionLogHandler) {
            AsyncDataServiceQueryTransactionLogHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.QueryTransactionLog reply dropped: AsyncDataServiceQueryTransactionLogHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceQueryTransactionLogFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.QueryTransactionLog", call->context, call->status, call->sentMetadata};
        AsyncDataServiceQueryTransactionLogFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.QueryTransactionLog failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceQueryTransactionLog(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceQueryTransactionLogGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceQueryTransactionLog(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceQueryTransactionLogCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::QueryTransactionLogResponse> done) {
    auto call(DataServiceQueryTransactionLogPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceQueryTransactionLog(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncQueryTransactionLog(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceQueryTransactionLogMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request,
                                            GrpcDone<::data_service::QueryTransactionLogResponse> done, const GrpcCallOptions& options) {
    StartDataServiceQueryTransactionLogCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceQueryTransactionLog(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceQueryTransactionLogCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::QueryTransactionLogRequest& derived = static_cast<const ::data_service::QueryTransactionLogRequest&>(message);
    SendDataServiceQueryTransactionLog(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceCreateEventSnapshot
boost::object_pool<AsyncDataServiceCreateEventSnapshotGrpcClient> DataServiceCreateEventSnapshotPool;
using AsyncDataServiceCreateEventSnapshotHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::CreateEventSnapshotResponse&)>;
AsyncDataServiceCreateEventSnapshotHandlerFunctionType AsyncDataServiceCreateEventSnapshotHandler;
AsyncDataServiceCreateEventSnapshotFailedHandlerFunctionType AsyncDataServiceCreateEventSnapshotFailedHandler;

void CompleteDataServiceCreateEventSnapshot(AsyncDataServiceCreateEventSnapshotGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceCreateEventSnapshotGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceCreateEventSnapshotPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceCreateEventSnapshotGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceCreateEventSnapshotHandler) {
            AsyncDataServiceCreateEventSnapshotHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.CreateEventSnapshot reply dropped: AsyncDataServiceCreateEventSnapshotHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceCreateEventSnapshotFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.CreateEventSnapshot", call->context, call->status, call->sentMetadata};
        AsyncDataServiceCreateEventSnapshotFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.CreateEventSnapshot failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceCreateEventSnapshot(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceCreateEventSnapshotGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceCreateEventSnapshot(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceCreateEventSnapshotCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::CreateEventSnapshotResponse> done) {
    auto call(DataServiceCreateEventSnapshotPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceCreateEventSnapshot(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCreateEventSnapshot(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceCreateEventSnapshotMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request,
                                            GrpcDone<::data_service::CreateEventSnapshotResponse> done, const GrpcCallOptions& options) {
    StartDataServiceCreateEventSnapshotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceCreateEventSnapshot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceCreateEventSnapshotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::CreateEventSnapshotRequest& derived = static_cast<const ::data_service::CreateEventSnapshotRequest&>(message);
    SendDataServiceCreateEventSnapshot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceAllocateIdSegment
boost::object_pool<AsyncDataServiceAllocateIdSegmentGrpcClient> DataServiceAllocateIdSegmentPool;
using AsyncDataServiceAllocateIdSegmentHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::AllocateIdSegmentResponse&)>;
AsyncDataServiceAllocateIdSegmentHandlerFunctionType AsyncDataServiceAllocateIdSegmentHandler;
AsyncDataServiceAllocateIdSegmentFailedHandlerFunctionType AsyncDataServiceAllocateIdSegmentFailedHandler;

void CompleteDataServiceAllocateIdSegment(AsyncDataServiceAllocateIdSegmentGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceAllocateIdSegmentGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceAllocateIdSegmentPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceAllocateIdSegmentGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceAllocateIdSegmentHandler) {
            AsyncDataServiceAllocateIdSegmentHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.AllocateIdSegment reply dropped: AsyncDataServiceAllocateIdSegmentHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceAllocateIdSegmentFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.AllocateIdSegment", call->context, call->status, call->sentMetadata};
        AsyncDataServiceAllocateIdSegmentFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.AllocateIdSegment failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceAllocateIdSegment(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceAllocateIdSegmentGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceAllocateIdSegment(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceAllocateIdSegmentCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::AllocateIdSegmentResponse> done) {
    auto call(DataServiceAllocateIdSegmentPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceAllocateIdSegment(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncAllocateIdSegment(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceAllocateIdSegmentMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request,
                                            GrpcDone<::data_service::AllocateIdSegmentResponse> done, const GrpcCallOptions& options) {
    StartDataServiceAllocateIdSegmentCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceAllocateIdSegment(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceAllocateIdSegmentCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::AllocateIdSegmentRequest& derived = static_cast<const ::data_service::AllocateIdSegmentRequest&>(message);
    SendDataServiceAllocateIdSegment(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceReservePlayerName
boost::object_pool<AsyncDataServiceReservePlayerNameGrpcClient> DataServiceReservePlayerNamePool;
using AsyncDataServiceReservePlayerNameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::ReservePlayerNameResponse&)>;
AsyncDataServiceReservePlayerNameHandlerFunctionType AsyncDataServiceReservePlayerNameHandler;
AsyncDataServiceReservePlayerNameFailedHandlerFunctionType AsyncDataServiceReservePlayerNameFailedHandler;

void CompleteDataServiceReservePlayerName(AsyncDataServiceReservePlayerNameGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceReservePlayerNameGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceReservePlayerNamePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceReservePlayerNameGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceReservePlayerNameHandler) {
            AsyncDataServiceReservePlayerNameHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.ReservePlayerName reply dropped: AsyncDataServiceReservePlayerNameHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceReservePlayerNameFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.ReservePlayerName", call->context, call->status, call->sentMetadata};
        AsyncDataServiceReservePlayerNameFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.ReservePlayerName failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceReservePlayerName(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceReservePlayerNameGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceReservePlayerName(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceReservePlayerNameCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::ReservePlayerNameResponse> done) {
    auto call(DataServiceReservePlayerNamePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceReservePlayerName(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncReservePlayerName(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceReservePlayerNameMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request,
                                            GrpcDone<::data_service::ReservePlayerNameResponse> done, const GrpcCallOptions& options) {
    StartDataServiceReservePlayerNameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceReservePlayerName(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceReservePlayerNameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::ReservePlayerNameRequest& derived = static_cast<const ::data_service::ReservePlayerNameRequest&>(message);
    SendDataServiceReservePlayerName(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceReleasePlayerName
boost::object_pool<AsyncDataServiceReleasePlayerNameGrpcClient> DataServiceReleasePlayerNamePool;
using AsyncDataServiceReleasePlayerNameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::google::protobuf::Empty&)>;
AsyncDataServiceReleasePlayerNameHandlerFunctionType AsyncDataServiceReleasePlayerNameHandler;
AsyncDataServiceReleasePlayerNameFailedHandlerFunctionType AsyncDataServiceReleasePlayerNameFailedHandler;

void CompleteDataServiceReleasePlayerName(AsyncDataServiceReleasePlayerNameGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceReleasePlayerNameGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceReleasePlayerNamePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceReleasePlayerNameGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceReleasePlayerNameHandler) {
            AsyncDataServiceReleasePlayerNameHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.ReleasePlayerName reply dropped: AsyncDataServiceReleasePlayerNameHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceReleasePlayerNameFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.ReleasePlayerName", call->context, call->status, call->sentMetadata};
        AsyncDataServiceReleasePlayerNameFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.ReleasePlayerName failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceReleasePlayerName(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceReleasePlayerNameGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceReleasePlayerName(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceReleasePlayerNameCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::google::protobuf::Empty> done) {
    auto call(DataServiceReleasePlayerNamePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceReleasePlayerName(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncReleasePlayerName(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceReleasePlayerNameMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request,
                                            GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options) {
    StartDataServiceReleasePlayerNameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceReleasePlayerName(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceReleasePlayerNameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::ReleasePlayerNameRequest& derived = static_cast<const ::data_service::ReleasePlayerNameRequest&>(message);
    SendDataServiceReleasePlayerName(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceBatchGetPlayerName
boost::object_pool<AsyncDataServiceBatchGetPlayerNameGrpcClient> DataServiceBatchGetPlayerNamePool;
using AsyncDataServiceBatchGetPlayerNameHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::BatchGetPlayerNameResponse&)>;
AsyncDataServiceBatchGetPlayerNameHandlerFunctionType AsyncDataServiceBatchGetPlayerNameHandler;
AsyncDataServiceBatchGetPlayerNameFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerNameFailedHandler;

void CompleteDataServiceBatchGetPlayerName(AsyncDataServiceBatchGetPlayerNameGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceBatchGetPlayerNameGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceBatchGetPlayerNamePool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceBatchGetPlayerNameGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceBatchGetPlayerNameHandler) {
            AsyncDataServiceBatchGetPlayerNameHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.BatchGetPlayerName reply dropped: AsyncDataServiceBatchGetPlayerNameHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceBatchGetPlayerNameFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.BatchGetPlayerName", call->context, call->status, call->sentMetadata};
        AsyncDataServiceBatchGetPlayerNameFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.BatchGetPlayerName failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceBatchGetPlayerName(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceBatchGetPlayerNameGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceBatchGetPlayerName(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceBatchGetPlayerNameCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::BatchGetPlayerNameResponse> done) {
    auto call(DataServiceBatchGetPlayerNamePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceBatchGetPlayerName(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncBatchGetPlayerName(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceBatchGetPlayerNameMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request,
                                            GrpcDone<::data_service::BatchGetPlayerNameResponse> done, const GrpcCallOptions& options) {
    StartDataServiceBatchGetPlayerNameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceBatchGetPlayerName(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceBatchGetPlayerNameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::BatchGetPlayerNameRequest& derived = static_cast<const ::data_service::BatchGetPlayerNameRequest&>(message);
    SendDataServiceBatchGetPlayerName(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerAssetOpLedger
boost::object_pool<AsyncDataServiceGetPlayerAssetOpLedgerGrpcClient> DataServiceGetPlayerAssetOpLedgerPool;
using AsyncDataServiceGetPlayerAssetOpLedgerHandlerFunctionType =
    std::function<void(const ClientContext&, const ::data_service::GetPlayerAssetOpLedgerResponse&)>;
AsyncDataServiceGetPlayerAssetOpLedgerHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerHandler;
AsyncDataServiceGetPlayerAssetOpLedgerFailedHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler;

void CompleteDataServiceGetPlayerAssetOpLedger(AsyncDataServiceGetPlayerAssetOpLedgerGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncDataServiceGetPlayerAssetOpLedgerGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        DataServiceGetPlayerAssetOpLedgerPool.destroy(completed);
    };
    const std::unique_ptr<AsyncDataServiceGetPlayerAssetOpLedgerGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncDataServiceGetPlayerAssetOpLedgerHandler) {
            AsyncDataServiceGetPlayerAssetOpLedgerHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.GetPlayerAssetOpLedger reply dropped: AsyncDataServiceGetPlayerAssetOpLedgerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "DataService.GetPlayerAssetOpLedger", call->context, call->status, call->sentMetadata};
        AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerAssetOpLedger failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcDataServiceGetPlayerAssetOpLedger(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncDataServiceGetPlayerAssetOpLedgerGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteDataServiceGetPlayerAssetOpLedger(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerAssetOpLedgerCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done) {
    auto call(DataServiceGetPlayerAssetOpLedgerPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<DataServiceStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteDataServiceGetPlayerAssetOpLedger(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncGetPlayerAssetOpLedger(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(DataServiceGetPlayerAssetOpLedgerMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request,
                                            GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerAssetOpLedgerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerAssetOpLedger(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerAssetOpLedgerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::data_service::GetPlayerAssetOpLedgerRequest& derived = static_cast<const ::data_service::GetPlayerAssetOpLedgerRequest&>(message);
    SendDataServiceGetPlayerAssetOpLedger(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleDataServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case DataServiceLoadPlayerDataMessageId:
            AsyncCompleteGrpcDataServiceLoadPlayerData(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceSavePlayerDataMessageId:
            AsyncCompleteGrpcDataServiceSavePlayerData(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceGetPlayerFieldMessageId:
            AsyncCompleteGrpcDataServiceGetPlayerField(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceSetPlayerFieldMessageId:
            AsyncCompleteGrpcDataServiceSetPlayerField(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceRegisterPlayerZoneMessageId:
            AsyncCompleteGrpcDataServiceRegisterPlayerZone(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceGetPlayerHomeZoneMessageId:
            AsyncCompleteGrpcDataServiceGetPlayerHomeZone(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceBatchGetPlayerHomeZoneMessageId:
            AsyncCompleteGrpcDataServiceBatchGetPlayerHomeZone(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceRemapHomeZoneForMergeMessageId:
            AsyncCompleteGrpcDataServiceRemapHomeZoneForMerge(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceDeletePlayerDataMessageId:
            AsyncCompleteGrpcDataServiceDeletePlayerData(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceCreatePlayerSnapshotMessageId:
            AsyncCompleteGrpcDataServiceCreatePlayerSnapshot(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceListPlayerSnapshotsMessageId:
            AsyncCompleteGrpcDataServiceListPlayerSnapshots(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceGetPlayerSnapshotDiffMessageId:
            AsyncCompleteGrpcDataServiceGetPlayerSnapshotDiff(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceRollbackPlayerMessageId:
            AsyncCompleteGrpcDataServiceRollbackPlayer(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceRollbackZoneMessageId:
            AsyncCompleteGrpcDataServiceRollbackZone(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceRollbackAllMessageId:
            AsyncCompleteGrpcDataServiceRollbackAll(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceBatchRecallItemsMessageId:
            AsyncCompleteGrpcDataServiceBatchRecallItems(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceQueryTransactionLogMessageId:
            AsyncCompleteGrpcDataServiceQueryTransactionLog(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceCreateEventSnapshotMessageId:
            AsyncCompleteGrpcDataServiceCreateEventSnapshot(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceAllocateIdSegmentMessageId:
            AsyncCompleteGrpcDataServiceAllocateIdSegment(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceReservePlayerNameMessageId:
            AsyncCompleteGrpcDataServiceReservePlayerName(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceReleasePlayerNameMessageId:
            AsyncCompleteGrpcDataServiceReleasePlayerName(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceBatchGetPlayerNameMessageId:
            AsyncCompleteGrpcDataServiceBatchGetPlayerName(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case DataServiceGetPlayerAssetOpLedgerMessageId:
            AsyncCompleteGrpcDataServiceGetPlayerAssetOpLedger(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetDataServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncDataServiceLoadPlayerDataHandler = handler;
    AsyncDataServiceSavePlayerDataHandler = handler;
    AsyncDataServiceGetPlayerFieldHandler = handler;
    AsyncDataServiceSetPlayerFieldHandler = handler;
    AsyncDataServiceRegisterPlayerZoneHandler = handler;
    AsyncDataServiceGetPlayerHomeZoneHandler = handler;
    AsyncDataServiceBatchGetPlayerHomeZoneHandler = handler;
    AsyncDataServiceRemapHomeZoneForMergeHandler = handler;
    AsyncDataServiceDeletePlayerDataHandler = handler;
    AsyncDataServiceCreatePlayerSnapshotHandler = handler;
    AsyncDataServiceListPlayerSnapshotsHandler = handler;
    AsyncDataServiceGetPlayerSnapshotDiffHandler = handler;
    AsyncDataServiceRollbackPlayerHandler = handler;
    AsyncDataServiceRollbackZoneHandler = handler;
    AsyncDataServiceRollbackAllHandler = handler;
    AsyncDataServiceBatchRecallItemsHandler = handler;
    AsyncDataServiceQueryTransactionLogHandler = handler;
    AsyncDataServiceCreateEventSnapshotHandler = handler;
    AsyncDataServiceAllocateIdSegmentHandler = handler;
    AsyncDataServiceReservePlayerNameHandler = handler;
    AsyncDataServiceReleasePlayerNameHandler = handler;
    AsyncDataServiceBatchGetPlayerNameHandler = handler;
    AsyncDataServiceGetPlayerAssetOpLedgerHandler = handler;
}

void SetDataServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncDataServiceLoadPlayerDataHandler) {
        AsyncDataServiceLoadPlayerDataHandler = handler;
    }
    if (!AsyncDataServiceSavePlayerDataHandler) {
        AsyncDataServiceSavePlayerDataHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerFieldHandler) {
        AsyncDataServiceGetPlayerFieldHandler = handler;
    }
    if (!AsyncDataServiceSetPlayerFieldHandler) {
        AsyncDataServiceSetPlayerFieldHandler = handler;
    }
    if (!AsyncDataServiceRegisterPlayerZoneHandler) {
        AsyncDataServiceRegisterPlayerZoneHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerHomeZoneHandler) {
        AsyncDataServiceGetPlayerHomeZoneHandler = handler;
    }
    if (!AsyncDataServiceBatchGetPlayerHomeZoneHandler) {
        AsyncDataServiceBatchGetPlayerHomeZoneHandler = handler;
    }
    if (!AsyncDataServiceRemapHomeZoneForMergeHandler) {
        AsyncDataServiceRemapHomeZoneForMergeHandler = handler;
    }
    if (!AsyncDataServiceDeletePlayerDataHandler) {
        AsyncDataServiceDeletePlayerDataHandler = handler;
    }
    if (!AsyncDataServiceCreatePlayerSnapshotHandler) {
        AsyncDataServiceCreatePlayerSnapshotHandler = handler;
    }
    if (!AsyncDataServiceListPlayerSnapshotsHandler) {
        AsyncDataServiceListPlayerSnapshotsHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerSnapshotDiffHandler) {
        AsyncDataServiceGetPlayerSnapshotDiffHandler = handler;
    }
    if (!AsyncDataServiceRollbackPlayerHandler) {
        AsyncDataServiceRollbackPlayerHandler = handler;
    }
    if (!AsyncDataServiceRollbackZoneHandler) {
        AsyncDataServiceRollbackZoneHandler = handler;
    }
    if (!AsyncDataServiceRollbackAllHandler) {
        AsyncDataServiceRollbackAllHandler = handler;
    }
    if (!AsyncDataServiceBatchRecallItemsHandler) {
        AsyncDataServiceBatchRecallItemsHandler = handler;
    }
    if (!AsyncDataServiceQueryTransactionLogHandler) {
        AsyncDataServiceQueryTransactionLogHandler = handler;
    }
    if (!AsyncDataServiceCreateEventSnapshotHandler) {
        AsyncDataServiceCreateEventSnapshotHandler = handler;
    }
    if (!AsyncDataServiceAllocateIdSegmentHandler) {
        AsyncDataServiceAllocateIdSegmentHandler = handler;
    }
    if (!AsyncDataServiceReservePlayerNameHandler) {
        AsyncDataServiceReservePlayerNameHandler = handler;
    }
    if (!AsyncDataServiceReleasePlayerNameHandler) {
        AsyncDataServiceReleasePlayerNameHandler = handler;
    }
    if (!AsyncDataServiceBatchGetPlayerNameHandler) {
        AsyncDataServiceBatchGetPlayerNameHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerAssetOpLedgerHandler) {
        AsyncDataServiceGetPlayerAssetOpLedgerHandler = handler;
    }
}

void SetDataServiceFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncDataServiceLoadPlayerDataFailedHandler = handler;
    AsyncDataServiceSavePlayerDataFailedHandler = handler;
    AsyncDataServiceGetPlayerFieldFailedHandler = handler;
    AsyncDataServiceSetPlayerFieldFailedHandler = handler;
    AsyncDataServiceRegisterPlayerZoneFailedHandler = handler;
    AsyncDataServiceGetPlayerHomeZoneFailedHandler = handler;
    AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler = handler;
    AsyncDataServiceRemapHomeZoneForMergeFailedHandler = handler;
    AsyncDataServiceDeletePlayerDataFailedHandler = handler;
    AsyncDataServiceCreatePlayerSnapshotFailedHandler = handler;
    AsyncDataServiceListPlayerSnapshotsFailedHandler = handler;
    AsyncDataServiceGetPlayerSnapshotDiffFailedHandler = handler;
    AsyncDataServiceRollbackPlayerFailedHandler = handler;
    AsyncDataServiceRollbackZoneFailedHandler = handler;
    AsyncDataServiceRollbackAllFailedHandler = handler;
    AsyncDataServiceBatchRecallItemsFailedHandler = handler;
    AsyncDataServiceQueryTransactionLogFailedHandler = handler;
    AsyncDataServiceCreateEventSnapshotFailedHandler = handler;
    AsyncDataServiceAllocateIdSegmentFailedHandler = handler;
    AsyncDataServiceReservePlayerNameFailedHandler = handler;
    AsyncDataServiceReleasePlayerNameFailedHandler = handler;
    AsyncDataServiceBatchGetPlayerNameFailedHandler = handler;
    AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler = handler;
}

void SetDataServiceIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncDataServiceLoadPlayerDataFailedHandler) {
        AsyncDataServiceLoadPlayerDataFailedHandler = handler;
    }
    if (!AsyncDataServiceSavePlayerDataFailedHandler) {
        AsyncDataServiceSavePlayerDataFailedHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerFieldFailedHandler) {
        AsyncDataServiceGetPlayerFieldFailedHandler = handler;
    }
    if (!AsyncDataServiceSetPlayerFieldFailedHandler) {
        AsyncDataServiceSetPlayerFieldFailedHandler = handler;
    }
    if (!AsyncDataServiceRegisterPlayerZoneFailedHandler) {
        AsyncDataServiceRegisterPlayerZoneFailedHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerHomeZoneFailedHandler) {
        AsyncDataServiceGetPlayerHomeZoneFailedHandler = handler;
    }
    if (!AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler) {
        AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler = handler;
    }
    if (!AsyncDataServiceRemapHomeZoneForMergeFailedHandler) {
        AsyncDataServiceRemapHomeZoneForMergeFailedHandler = handler;
    }
    if (!AsyncDataServiceDeletePlayerDataFailedHandler) {
        AsyncDataServiceDeletePlayerDataFailedHandler = handler;
    }
    if (!AsyncDataServiceCreatePlayerSnapshotFailedHandler) {
        AsyncDataServiceCreatePlayerSnapshotFailedHandler = handler;
    }
    if (!AsyncDataServiceListPlayerSnapshotsFailedHandler) {
        AsyncDataServiceListPlayerSnapshotsFailedHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerSnapshotDiffFailedHandler) {
        AsyncDataServiceGetPlayerSnapshotDiffFailedHandler = handler;
    }
    if (!AsyncDataServiceRollbackPlayerFailedHandler) {
        AsyncDataServiceRollbackPlayerFailedHandler = handler;
    }
    if (!AsyncDataServiceRollbackZoneFailedHandler) {
        AsyncDataServiceRollbackZoneFailedHandler = handler;
    }
    if (!AsyncDataServiceRollbackAllFailedHandler) {
        AsyncDataServiceRollbackAllFailedHandler = handler;
    }
    if (!AsyncDataServiceBatchRecallItemsFailedHandler) {
        AsyncDataServiceBatchRecallItemsFailedHandler = handler;
    }
    if (!AsyncDataServiceQueryTransactionLogFailedHandler) {
        AsyncDataServiceQueryTransactionLogFailedHandler = handler;
    }
    if (!AsyncDataServiceCreateEventSnapshotFailedHandler) {
        AsyncDataServiceCreateEventSnapshotFailedHandler = handler;
    }
    if (!AsyncDataServiceAllocateIdSegmentFailedHandler) {
        AsyncDataServiceAllocateIdSegmentFailedHandler = handler;
    }
    if (!AsyncDataServiceReservePlayerNameFailedHandler) {
        AsyncDataServiceReservePlayerNameFailedHandler = handler;
    }
    if (!AsyncDataServiceReleasePlayerNameFailedHandler) {
        AsyncDataServiceReleasePlayerNameFailedHandler = handler;
    }
    if (!AsyncDataServiceBatchGetPlayerNameFailedHandler) {
        AsyncDataServiceBatchGetPlayerNameFailedHandler = handler;
    }
    if (!AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler) {
        AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler = handler;
    }
}

void SetDataServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitDataServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<DataServiceStubPtr>(nodeEntity, DataService::NewStub(channel));

}

}// namespace data_service
