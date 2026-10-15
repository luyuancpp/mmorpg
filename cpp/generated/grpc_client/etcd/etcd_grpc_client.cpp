#include "muduo/base/Logging.h"

#include "etcd_grpc_client.h"
#include "proto/common/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <atomic>
#include <chrono>
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace {
boost::object_pool<GrpcTag> tagPool;
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetEtcdCallDeadline 按目标节点类型写入
// (Node::Initialize → grpc_call_deadline::Apply);原子量:与应答处理器一样是进程级全局。
std::atomic<uint32_t> callDeadlineMs{kDefaultGrpcCallDeadlineMs};

std::chrono::system_clock::time_point NextCallDeadline() {
    return std::chrono::system_clock::now() +
        std::chrono::milliseconds(callDeadlineMs.load(std::memory_order_relaxed));
}
}

namespace etcdserverpb {
struct EtcdCompleteQueue {
    grpc::CompletionQueue cq;
};
#pragma region KVRange
boost::object_pool<AsyncKVRangeGrpcClient> KVRangePool;
using AsyncKVRangeHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::RangeResponse&)>;
AsyncKVRangeHandlerFunctionType AsyncKVRangeHandler;
AsyncKVRangeFailedHandlerFunctionType AsyncKVRangeFailedHandler;

void CompleteKVRange(AsyncKVRangeGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncKVRangeGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        KVRangePool.destroy(completed);
    };
    const std::unique_ptr<AsyncKVRangeGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncKVRangeHandler) {
            AsyncKVRangeHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC KV.Range reply dropped: AsyncKVRangeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncKVRangeFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "KV.Range", call->context, call->status, call->sentMetadata};
        AsyncKVRangeFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC KV.Range failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcKVRange(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncKVRangeGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteKVRange(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVRangeCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::RangeResponse> done) {
    auto call(KVRangePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<KVStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteKVRange(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncRange(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(KVRangeMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request,
                                            GrpcDone<::etcdserverpb::RangeResponse> done, const GrpcCallOptions& options) {
    StartKVRangeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVRange(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVRangeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::RangeRequest& derived = static_cast<const ::etcdserverpb::RangeRequest&>(message);
    SendKVRange(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVPut
boost::object_pool<AsyncKVPutGrpcClient> KVPutPool;
using AsyncKVPutHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::PutResponse&)>;
AsyncKVPutHandlerFunctionType AsyncKVPutHandler;
AsyncKVPutFailedHandlerFunctionType AsyncKVPutFailedHandler;

void CompleteKVPut(AsyncKVPutGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncKVPutGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        KVPutPool.destroy(completed);
    };
    const std::unique_ptr<AsyncKVPutGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncKVPutHandler) {
            AsyncKVPutHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC KV.Put reply dropped: AsyncKVPutHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncKVPutFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "KV.Put", call->context, call->status, call->sentMetadata};
        AsyncKVPutFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC KV.Put failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcKVPut(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncKVPutGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteKVPut(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVPutCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::PutResponse> done) {
    auto call(KVPutPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<KVStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteKVPut(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncPut(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(KVPutMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request,
                                            GrpcDone<::etcdserverpb::PutResponse> done, const GrpcCallOptions& options) {
    StartKVPutCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVPut(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVPutCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::PutRequest& derived = static_cast<const ::etcdserverpb::PutRequest&>(message);
    SendKVPut(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVDeleteRange
boost::object_pool<AsyncKVDeleteRangeGrpcClient> KVDeleteRangePool;
using AsyncKVDeleteRangeHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::DeleteRangeResponse&)>;
AsyncKVDeleteRangeHandlerFunctionType AsyncKVDeleteRangeHandler;
AsyncKVDeleteRangeFailedHandlerFunctionType AsyncKVDeleteRangeFailedHandler;

void CompleteKVDeleteRange(AsyncKVDeleteRangeGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncKVDeleteRangeGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        KVDeleteRangePool.destroy(completed);
    };
    const std::unique_ptr<AsyncKVDeleteRangeGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncKVDeleteRangeHandler) {
            AsyncKVDeleteRangeHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC KV.DeleteRange reply dropped: AsyncKVDeleteRangeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncKVDeleteRangeFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "KV.DeleteRange", call->context, call->status, call->sentMetadata};
        AsyncKVDeleteRangeFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC KV.DeleteRange failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcKVDeleteRange(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncKVDeleteRangeGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteKVDeleteRange(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVDeleteRangeCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::DeleteRangeResponse> done) {
    auto call(KVDeleteRangePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<KVStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteKVDeleteRange(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncDeleteRange(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(KVDeleteRangeMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request,
                                            GrpcDone<::etcdserverpb::DeleteRangeResponse> done, const GrpcCallOptions& options) {
    StartKVDeleteRangeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVDeleteRange(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVDeleteRangeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::DeleteRangeRequest& derived = static_cast<const ::etcdserverpb::DeleteRangeRequest&>(message);
    SendKVDeleteRange(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVTxn
boost::object_pool<AsyncKVTxnGrpcClient> KVTxnPool;
using AsyncKVTxnHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::TxnResponse&)>;
AsyncKVTxnHandlerFunctionType AsyncKVTxnHandler;
AsyncKVTxnFailedHandlerFunctionType AsyncKVTxnFailedHandler;

void CompleteKVTxn(AsyncKVTxnGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncKVTxnGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        KVTxnPool.destroy(completed);
    };
    const std::unique_ptr<AsyncKVTxnGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncKVTxnHandler) {
            AsyncKVTxnHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC KV.Txn reply dropped: AsyncKVTxnHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncKVTxnFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "KV.Txn", call->context, call->status, call->sentMetadata};
        AsyncKVTxnFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC KV.Txn failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcKVTxn(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncKVTxnGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteKVTxn(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVTxnCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::TxnResponse> done) {
    auto call(KVTxnPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<KVStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteKVTxn(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncTxn(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(KVTxnMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request,
                                            GrpcDone<::etcdserverpb::TxnResponse> done, const GrpcCallOptions& options) {
    StartKVTxnCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVTxn(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVTxnCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::TxnRequest& derived = static_cast<const ::etcdserverpb::TxnRequest&>(message);
    SendKVTxn(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVCompact
boost::object_pool<AsyncKVCompactGrpcClient> KVCompactPool;
using AsyncKVCompactHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::CompactionResponse&)>;
AsyncKVCompactHandlerFunctionType AsyncKVCompactHandler;
AsyncKVCompactFailedHandlerFunctionType AsyncKVCompactFailedHandler;

void CompleteKVCompact(AsyncKVCompactGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncKVCompactGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        KVCompactPool.destroy(completed);
    };
    const std::unique_ptr<AsyncKVCompactGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncKVCompactHandler) {
            AsyncKVCompactHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC KV.Compact reply dropped: AsyncKVCompactHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncKVCompactFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "KV.Compact", call->context, call->status, call->sentMetadata};
        AsyncKVCompactFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC KV.Compact failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcKVCompact(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncKVCompactGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteKVCompact(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVCompactCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::CompactionResponse> done) {
    auto call(KVCompactPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<KVStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteKVCompact(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncCompact(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(KVCompactMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request,
                                            GrpcDone<::etcdserverpb::CompactionResponse> done, const GrpcCallOptions& options) {
    StartKVCompactCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVCompact(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVCompactCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::CompactionRequest& derived = static_cast<const ::etcdserverpb::CompactionRequest&>(message);
    SendKVCompact(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region WatchWatch
boost::object_pool<AsyncWatchWatchGrpcClient> WatchWatchPool;
using AsyncWatchWatchHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::WatchResponse&)>;
AsyncWatchWatchHandlerFunctionType AsyncWatchWatchHandler;

void TryFinishWatchWatch(AsyncWatchWatchGrpcClient& client) {
    if (!(client.closing || client.runtime->closing) || client.initializing || client.reading ||
        client.writing || client.finishing || client.finished) {
        return;
    }
    client.finishing = true;
    GrpcTag* tag = tagPool.construct(WatchWatchMessageId, (void*)GrpcOperation::FINISH);
    client.stream->Finish(&client.status, tag);
}

void TryWriteNextNextWatchWatch(AsyncWatchWatchGrpcClient& client) {
    if (client.initializing || client.writing || client.closing || client.runtime->closing ||
        client.finishing || client.finished || client.pendingWrites.empty()) {
        return;
    }
    // 不把可淘汰的队列元素地址交给异步 Write。
    client.writingRequest.Swap(&client.pendingWrites.front());
    client.pendingWrites.pop_front();
    client.writing = true;
    GrpcTag* tag = tagPool.construct(WatchWatchMessageId, (void*)GrpcOperation::WRITE);
    client.stream->Write(client.writingRequest, tag);
}

void AsyncCompleteGrpcWatchWatch(entt::registry& registry, entt::entity nodeEntity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* client = registry.try_get<AsyncWatchWatchGrpcClient>(nodeEntity);
    if (client == nullptr) {
        LOG_ERROR << "gRPC stream owner disappeared before completion";
        return;
    }
    const auto operation = static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag));
    if (!ok && operation != GrpcOperation::FINISH) {
        client->closing = true;
        client->context.TryCancel();
    }
    switch (operation) {
        case GrpcOperation::WRITE:
            client->writing = false;
            client->writingRequest.Clear();
            break;
        case GrpcOperation::READ: {
            // Read 已完成，但业务 handler 仍借用 reply；保持 reading，禁止重入 WRITE 再排 Read/Finish。
            struct ReadBorrow {
                entt::registry& registry;
                entt::entity entity;
                ~ReadBorrow() {
                    if (!registry.valid(entity)) return;
                    if (auto* owner = registry.try_get<AsyncWatchWatchGrpcClient>(entity)) {
                        owner->reading = false;
                    }
                }
            };
            try {
                const ReadBorrow borrow{registry, nodeEntity};
                if (ok && !client->closing && !client->runtime->closing && AsyncWatchWatchHandler) {
                    AsyncWatchWatchHandler(client->context, client->reply);
                }
            } catch (...) {
                if (registry.valid(nodeEntity)) {
                    if (auto* owner = registry.try_get<AsyncWatchWatchGrpcClient>(nodeEntity)) {
                        owner->closing = true;
                        owner->context.TryCancel();
                        TryFinishWatchWatch(*owner);
                    }
                }
                throw;
            }
            // 应答处理器可能重入关闭节点，借用结束后重取句柄再继续。
            if (!registry.valid(nodeEntity)) return;
            client = registry.try_get<AsyncWatchWatchGrpcClient>(nodeEntity);
            if (client == nullptr) return;
            break;
        }
        case GrpcOperation::INIT:
            client->initializing = false;
            break;
        case GrpcOperation::FINISH:
            client->finished = true;
            client->runtime->calls.erase(&client->context);
            client->pendingWrites.clear();
            if (!client->status.ok() && !client->runtime->closing) {
                LOG_ERROR << "gRPC Watch.Watch stream ended: " << client->status.error_message();
            }
            return;
        default:
            break;
    }
    if (!client->initializing && !client->reading && !client->closing && !client->runtime->closing) {
        client->reply.Clear();
        client->reading = true;
        GrpcTag* tag = tagPool.construct(WatchWatchMessageId, (void*)GrpcOperation::READ);
        client->stream->Read(&client->reply, tag);
    }
    TryWriteNextNextWatchWatch(*client);
    TryFinishWatchWatch(*client);
}

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendWatchWatch(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::WatchRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    if (!HasUsableGrpcClient(registry, nodeEntity)) {
        LOG_ERROR << "gRPC Watch.Watch rejected: node missing or closing";
        return;
    }
    auto* client = registry.try_get<AsyncWatchWatchGrpcClient>(nodeEntity);
    if (client == nullptr || !client->stream || client->closing || client->finished || client->finishing ||
        client->pendingWrites.size() >= 200) {
        LOG_ERROR << "gRPC Watch.Watch rejected: stream unavailable or write queue full";
        return;
    }
    client->pendingWrites.push_back(request);
    TryWriteNextNextWatchWatch(*client);

}

void SendWatchWatch(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::WatchRequest& derived = static_cast<const ::etcdserverpb::WatchRequest&>(message);
    SendWatchWatch(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseGrant
boost::object_pool<AsyncLeaseLeaseGrantGrpcClient> LeaseLeaseGrantPool;
using AsyncLeaseLeaseGrantHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::LeaseGrantResponse&)>;
AsyncLeaseLeaseGrantHandlerFunctionType AsyncLeaseLeaseGrantHandler;
AsyncLeaseLeaseGrantFailedHandlerFunctionType AsyncLeaseLeaseGrantFailedHandler;

void CompleteLeaseLeaseGrant(AsyncLeaseLeaseGrantGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncLeaseLeaseGrantGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        LeaseLeaseGrantPool.destroy(completed);
    };
    const std::unique_ptr<AsyncLeaseLeaseGrantGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncLeaseLeaseGrantHandler) {
            AsyncLeaseLeaseGrantHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC Lease.LeaseGrant reply dropped: AsyncLeaseLeaseGrantHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncLeaseLeaseGrantFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "Lease.LeaseGrant", call->context, call->status, call->sentMetadata};
        AsyncLeaseLeaseGrantFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseGrant failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcLeaseLeaseGrant(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncLeaseLeaseGrantGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteLeaseLeaseGrant(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseGrantCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseGrantResponse> done) {
    auto call(LeaseLeaseGrantPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<LeaseStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteLeaseLeaseGrant(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLeaseGrant(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(LeaseLeaseGrantMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseGrantResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseGrantCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseGrant(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseGrantCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::LeaseGrantRequest& derived = static_cast<const ::etcdserverpb::LeaseGrantRequest&>(message);
    SendLeaseLeaseGrant(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseRevoke
boost::object_pool<AsyncLeaseLeaseRevokeGrpcClient> LeaseLeaseRevokePool;
using AsyncLeaseLeaseRevokeHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::LeaseRevokeResponse&)>;
AsyncLeaseLeaseRevokeHandlerFunctionType AsyncLeaseLeaseRevokeHandler;
AsyncLeaseLeaseRevokeFailedHandlerFunctionType AsyncLeaseLeaseRevokeFailedHandler;

void CompleteLeaseLeaseRevoke(AsyncLeaseLeaseRevokeGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncLeaseLeaseRevokeGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        LeaseLeaseRevokePool.destroy(completed);
    };
    const std::unique_ptr<AsyncLeaseLeaseRevokeGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncLeaseLeaseRevokeHandler) {
            AsyncLeaseLeaseRevokeHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC Lease.LeaseRevoke reply dropped: AsyncLeaseLeaseRevokeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncLeaseLeaseRevokeFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "Lease.LeaseRevoke", call->context, call->status, call->sentMetadata};
        AsyncLeaseLeaseRevokeFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseRevoke failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcLeaseLeaseRevoke(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncLeaseLeaseRevokeGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteLeaseLeaseRevoke(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseRevokeCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseRevokeResponse> done) {
    auto call(LeaseLeaseRevokePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<LeaseStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteLeaseLeaseRevoke(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLeaseRevoke(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(LeaseLeaseRevokeMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseRevokeResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseRevokeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseRevoke(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseRevokeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::LeaseRevokeRequest& derived = static_cast<const ::etcdserverpb::LeaseRevokeRequest&>(message);
    SendLeaseLeaseRevoke(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseKeepAlive
boost::object_pool<AsyncLeaseLeaseKeepAliveGrpcClient> LeaseLeaseKeepAlivePool;
using AsyncLeaseLeaseKeepAliveHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::LeaseKeepAliveResponse&)>;
AsyncLeaseLeaseKeepAliveHandlerFunctionType AsyncLeaseLeaseKeepAliveHandler;

void TryFinishLeaseLeaseKeepAlive(AsyncLeaseLeaseKeepAliveGrpcClient& client) {
    if (!(client.closing || client.runtime->closing) || client.initializing || client.reading ||
        client.writing || client.finishing || client.finished) {
        return;
    }
    client.finishing = true;
    GrpcTag* tag = tagPool.construct(LeaseLeaseKeepAliveMessageId, (void*)GrpcOperation::FINISH);
    client.stream->Finish(&client.status, tag);
}

void TryWriteNextNextLeaseLeaseKeepAlive(AsyncLeaseLeaseKeepAliveGrpcClient& client) {
    if (client.initializing || client.writing || client.closing || client.runtime->closing ||
        client.finishing || client.finished || client.pendingWrites.empty()) {
        return;
    }
    // 不把可淘汰的队列元素地址交给异步 Write。
    client.writingRequest.Swap(&client.pendingWrites.front());
    client.pendingWrites.pop_front();
    client.writing = true;
    GrpcTag* tag = tagPool.construct(LeaseLeaseKeepAliveMessageId, (void*)GrpcOperation::WRITE);
    client.stream->Write(client.writingRequest, tag);
}

void AsyncCompleteGrpcLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* client = registry.try_get<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
    if (client == nullptr) {
        LOG_ERROR << "gRPC stream owner disappeared before completion";
        return;
    }
    const auto operation = static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag));
    if (!ok && operation != GrpcOperation::FINISH) {
        client->closing = true;
        client->context.TryCancel();
    }
    switch (operation) {
        case GrpcOperation::WRITE:
            client->writing = false;
            client->writingRequest.Clear();
            break;
        case GrpcOperation::READ: {
            // Read 已完成，但业务 handler 仍借用 reply；保持 reading，禁止重入 WRITE 再排 Read/Finish。
            struct ReadBorrow {
                entt::registry& registry;
                entt::entity entity;
                ~ReadBorrow() {
                    if (!registry.valid(entity)) return;
                    if (auto* owner = registry.try_get<AsyncLeaseLeaseKeepAliveGrpcClient>(entity)) {
                        owner->reading = false;
                    }
                }
            };
            try {
                const ReadBorrow borrow{registry, nodeEntity};
                if (ok && !client->closing && !client->runtime->closing && AsyncLeaseLeaseKeepAliveHandler) {
                    AsyncLeaseLeaseKeepAliveHandler(client->context, client->reply);
                }
            } catch (...) {
                if (registry.valid(nodeEntity)) {
                    if (auto* owner = registry.try_get<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity)) {
                        owner->closing = true;
                        owner->context.TryCancel();
                        TryFinishLeaseLeaseKeepAlive(*owner);
                    }
                }
                throw;
            }
            // 应答处理器可能重入关闭节点，借用结束后重取句柄再继续。
            if (!registry.valid(nodeEntity)) return;
            client = registry.try_get<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
            if (client == nullptr) return;
            break;
        }
        case GrpcOperation::INIT:
            client->initializing = false;
            break;
        case GrpcOperation::FINISH:
            client->finished = true;
            client->runtime->calls.erase(&client->context);
            client->pendingWrites.clear();
            if (!client->status.ok() && !client->runtime->closing) {
                LOG_ERROR << "gRPC Lease.LeaseKeepAlive stream ended: " << client->status.error_message();
            }
            return;
        default:
            break;
    }
    if (!client->initializing && !client->reading && !client->closing && !client->runtime->closing) {
        client->reply.Clear();
        client->reading = true;
        GrpcTag* tag = tagPool.construct(LeaseLeaseKeepAliveMessageId, (void*)GrpcOperation::READ);
        client->stream->Read(&client->reply, tag);
    }
    TryWriteNextNextLeaseLeaseKeepAlive(*client);
    TryFinishLeaseLeaseKeepAlive(*client);
}

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseKeepAlive(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseKeepAliveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    if (!HasUsableGrpcClient(registry, nodeEntity)) {
        LOG_ERROR << "gRPC Lease.LeaseKeepAlive rejected: node missing or closing";
        return;
    }
    auto* client = registry.try_get<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
    if (client == nullptr || !client->stream || client->closing || client->finished || client->finishing ||
        client->pendingWrites.size() >= 200) {
        LOG_ERROR << "gRPC Lease.LeaseKeepAlive rejected: stream unavailable or write queue full";
        return;
    }
    client->pendingWrites.push_back(request);
    TryWriteNextNextLeaseLeaseKeepAlive(*client);

}

void SendLeaseLeaseKeepAlive(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::LeaseKeepAliveRequest& derived = static_cast<const ::etcdserverpb::LeaseKeepAliveRequest&>(message);
    SendLeaseLeaseKeepAlive(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseTimeToLive
boost::object_pool<AsyncLeaseLeaseTimeToLiveGrpcClient> LeaseLeaseTimeToLivePool;
using AsyncLeaseLeaseTimeToLiveHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::LeaseTimeToLiveResponse&)>;
AsyncLeaseLeaseTimeToLiveHandlerFunctionType AsyncLeaseLeaseTimeToLiveHandler;
AsyncLeaseLeaseTimeToLiveFailedHandlerFunctionType AsyncLeaseLeaseTimeToLiveFailedHandler;

void CompleteLeaseLeaseTimeToLive(AsyncLeaseLeaseTimeToLiveGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncLeaseLeaseTimeToLiveGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        LeaseLeaseTimeToLivePool.destroy(completed);
    };
    const std::unique_ptr<AsyncLeaseLeaseTimeToLiveGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncLeaseLeaseTimeToLiveHandler) {
            AsyncLeaseLeaseTimeToLiveHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC Lease.LeaseTimeToLive reply dropped: AsyncLeaseLeaseTimeToLiveHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncLeaseLeaseTimeToLiveFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "Lease.LeaseTimeToLive", call->context, call->status, call->sentMetadata};
        AsyncLeaseLeaseTimeToLiveFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseTimeToLive failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcLeaseLeaseTimeToLive(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncLeaseLeaseTimeToLiveGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteLeaseLeaseTimeToLive(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseTimeToLiveCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseTimeToLiveResponse> done) {
    auto call(LeaseLeaseTimeToLivePool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<LeaseStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteLeaseLeaseTimeToLive(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLeaseTimeToLive(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(LeaseLeaseTimeToLiveMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseTimeToLiveResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseTimeToLiveCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseTimeToLive(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseTimeToLiveCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::LeaseTimeToLiveRequest& derived = static_cast<const ::etcdserverpb::LeaseTimeToLiveRequest&>(message);
    SendLeaseLeaseTimeToLive(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseLeases
boost::object_pool<AsyncLeaseLeaseLeasesGrpcClient> LeaseLeaseLeasesPool;
using AsyncLeaseLeaseLeasesHandlerFunctionType =
    std::function<void(const ClientContext&, const ::etcdserverpb::LeaseLeasesResponse&)>;
AsyncLeaseLeaseLeasesHandlerFunctionType AsyncLeaseLeaseLeasesHandler;
AsyncLeaseLeaseLeasesFailedHandlerFunctionType AsyncLeaseLeaseLeasesFailedHandler;

void CompleteLeaseLeaseLeases(AsyncLeaseLeaseLeasesGrpcClient* call) {
    // 回调可能重入关闭/排空；context 登记与调用对象必须一起保留到业务回调退栈。
    const auto release = [](AsyncLeaseLeaseLeasesGrpcClient* completed) {
        if (completed->runtime) completed->runtime->calls.erase(&completed->context);
        LeaseLeaseLeasesPool.destroy(completed);
    };
    const std::unique_ptr<AsyncLeaseLeaseLeasesGrpcClient, decltype(release)> owner(call, release);
    if (call->done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 借自调用对象,回调退栈后才释放。
        const GrpcCallOutcome outcome{static_cast<uint32_t>(call->status.error_code()), call->status.error_message()};
        call->done(outcome, call->status.ok() ? &call->reply : nullptr);
        return;
    }
    if (call->status.ok()) {
        if (AsyncLeaseLeaseLeasesHandler) {
            AsyncLeaseLeaseLeasesHandler(call->context, call->reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC Lease.LeaseLeases reply dropped: AsyncLeaseLeaseLeasesHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncLeaseLeaseLeasesFailedHandler) {
        const GrpcCallFailure failure{call->messageId, "Lease.LeaseLeases", call->context, call->status, call->sentMetadata};
        AsyncLeaseLeaseLeasesFailedHandler(failure, call->request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseLeases failed: code=" << static_cast<int>(call->status.error_code())
                  << " msg=" << call->status.error_message();
    }

}

void AsyncCompleteGrpcLeaseLeaseLeases(entt::registry&, entt::entity,
                                                         grpc::CompletionQueue&, bool ok, void* got_tag) {
    auto* call = static_cast<AsyncLeaseLeaseLeasesGrpcClient*>(got_tag);
    if (!ok && call->status.ok()) {
        call->status = Status(grpc::StatusCode::CANCELLED, "completion cancelled");
    }
    CompleteLeaseLeaseLeases(call);
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseLeasesCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseLeasesResponse> done) {
    auto call(LeaseLeaseLeasesPool.construct());
    for (const auto& [key, value] : metadata) {
        call->context.AddMetadata(key, Base64Encode(value));
    }
    call->sentMetadata = std::move(metadata);
    call->request = request;
    call->done = std::move(done);
    call->context.set_deadline(deadline.count() > 0 ? std::chrono::system_clock::now() + deadline : NextCallDeadline());
    auto* cq = registry.valid(nodeEntity) ? registry.try_get<grpc::CompletionQueue>(nodeEntity) : nullptr;
    auto* stub = registry.valid(nodeEntity) ? registry.try_get<LeaseStubPtr>(nodeEntity) : nullptr;
    auto* runtime = registry.valid(nodeEntity) ? registry.try_get<GrpcClientRuntimePtr>(nodeEntity) : nullptr;
    if (runtime != nullptr) call->runtime = *runtime;
    if (cq == nullptr || stub == nullptr || !*stub || !call->runtime || call->runtime->closing) {
        call->status = Status(grpc::StatusCode::UNAVAILABLE, "node transport missing or closing");
        QueueGrpcClientCompletion(call->runtime, [call] { CompleteLeaseLeaseLeases(call); });
        return;
    }
    call->runtime->calls.insert(&call->context);
    call->response_reader = (*stub)->PrepareAsyncLeaseLeases(&call->context, call->request, cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(LeaseLeaseLeasesMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseLeasesResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseLeasesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request) {
    // 显式写出空 vector:unary 方法另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseLeases(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseLeasesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);

}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::etcdserverpb::LeaseLeasesRequest& derived = static_cast<const ::etcdserverpb::LeaseLeasesRequest&>(message);
    SendLeaseLeaseLeases(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void HandleEtcdCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, bool ok, GrpcTag* grpcTag) {
        const auto releaseTag = [](GrpcTag* tag) { tagPool.destroy(tag); };
        const std::unique_ptr<GrpcTag, decltype(releaseTag)> owner(grpcTag, releaseTag);
        switch (grpcTag->messageId) {
        case KVRangeMessageId:
            AsyncCompleteGrpcKVRange(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case KVPutMessageId:
            AsyncCompleteGrpcKVPut(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case KVDeleteRangeMessageId:
            AsyncCompleteGrpcKVDeleteRange(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case KVTxnMessageId:
            AsyncCompleteGrpcKVTxn(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case KVCompactMessageId:
            AsyncCompleteGrpcKVCompact(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case WatchWatchMessageId:
            AsyncCompleteGrpcWatchWatch(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case LeaseLeaseGrantMessageId:
            AsyncCompleteGrpcLeaseLeaseGrant(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case LeaseLeaseRevokeMessageId:
            AsyncCompleteGrpcLeaseLeaseRevoke(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case LeaseLeaseKeepAliveMessageId:
            AsyncCompleteGrpcLeaseLeaseKeepAlive(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case LeaseLeaseTimeToLiveMessageId:
            AsyncCompleteGrpcLeaseLeaseTimeToLive(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        case LeaseLeaseLeasesMessageId:
            AsyncCompleteGrpcLeaseLeaseLeases(registry, nodeEntity, completeQueueComp, ok, grpcTag->valuePtr);
            break;
        default:
            break;
        }
}

void SetEtcdHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncKVRangeHandler = handler;
    AsyncKVPutHandler = handler;
    AsyncKVDeleteRangeHandler = handler;
    AsyncKVTxnHandler = handler;
    AsyncKVCompactHandler = handler;
    AsyncWatchWatchHandler = handler;
    AsyncLeaseLeaseGrantHandler = handler;
    AsyncLeaseLeaseRevokeHandler = handler;
    AsyncLeaseLeaseKeepAliveHandler = handler;
    AsyncLeaseLeaseTimeToLiveHandler = handler;
    AsyncLeaseLeaseLeasesHandler = handler;
}

void SetEtcdIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncKVRangeHandler) {
        AsyncKVRangeHandler = handler;
    }
    if (!AsyncKVPutHandler) {
        AsyncKVPutHandler = handler;
    }
    if (!AsyncKVDeleteRangeHandler) {
        AsyncKVDeleteRangeHandler = handler;
    }
    if (!AsyncKVTxnHandler) {
        AsyncKVTxnHandler = handler;
    }
    if (!AsyncKVCompactHandler) {
        AsyncKVCompactHandler = handler;
    }
    if (!AsyncWatchWatchHandler) {
        AsyncWatchWatchHandler = handler;
    }
    if (!AsyncLeaseLeaseGrantHandler) {
        AsyncLeaseLeaseGrantHandler = handler;
    }
    if (!AsyncLeaseLeaseRevokeHandler) {
        AsyncLeaseLeaseRevokeHandler = handler;
    }
    if (!AsyncLeaseLeaseKeepAliveHandler) {
        AsyncLeaseLeaseKeepAliveHandler = handler;
    }
    if (!AsyncLeaseLeaseTimeToLiveHandler) {
        AsyncLeaseLeaseTimeToLiveHandler = handler;
    }
    if (!AsyncLeaseLeaseLeasesHandler) {
        AsyncLeaseLeaseLeasesHandler = handler;
    }
}

void SetEtcdFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncKVRangeFailedHandler = handler;
    AsyncKVPutFailedHandler = handler;
    AsyncKVDeleteRangeFailedHandler = handler;
    AsyncKVTxnFailedHandler = handler;
    AsyncKVCompactFailedHandler = handler;
    AsyncLeaseLeaseGrantFailedHandler = handler;
    AsyncLeaseLeaseRevokeFailedHandler = handler;
    AsyncLeaseLeaseTimeToLiveFailedHandler = handler;
    AsyncLeaseLeaseLeasesFailedHandler = handler;
}

void SetEtcdIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncKVRangeFailedHandler) {
        AsyncKVRangeFailedHandler = handler;
    }
    if (!AsyncKVPutFailedHandler) {
        AsyncKVPutFailedHandler = handler;
    }
    if (!AsyncKVDeleteRangeFailedHandler) {
        AsyncKVDeleteRangeFailedHandler = handler;
    }
    if (!AsyncKVTxnFailedHandler) {
        AsyncKVTxnFailedHandler = handler;
    }
    if (!AsyncKVCompactFailedHandler) {
        AsyncKVCompactFailedHandler = handler;
    }
    if (!AsyncLeaseLeaseGrantFailedHandler) {
        AsyncLeaseLeaseGrantFailedHandler = handler;
    }
    if (!AsyncLeaseLeaseRevokeFailedHandler) {
        AsyncLeaseLeaseRevokeFailedHandler = handler;
    }
    if (!AsyncLeaseLeaseTimeToLiveFailedHandler) {
        AsyncLeaseLeaseTimeToLiveFailedHandler = handler;
    }
    if (!AsyncLeaseLeaseLeasesFailedHandler) {
        AsyncLeaseLeaseLeasesFailedHandler = handler;
    }
}

void SetEtcdCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

void InitEtcdGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<KVStubPtr>(nodeEntity, KV::NewStub(channel));
    registry.emplace<WatchStubPtr>(nodeEntity, Watch::NewStub(channel));
    registry.emplace<LeaseStubPtr>(nodeEntity, Lease::NewStub(channel));
    {
        GrpcTag* got_tag(tagPool.construct(WatchWatchMessageId, (void*)GrpcOperation::INIT));

        auto& client = registry.emplace<AsyncWatchWatchGrpcClient>(nodeEntity);
        client.runtime = registry.get<GrpcClientRuntimePtr>(nodeEntity);
        client.runtime->calls.insert(&client.context);

        client.stream = registry
            .get<WatchStubPtr>(nodeEntity)
            ->AsyncWatch(&client.context,
                                        &registry.get<grpc::CompletionQueue>(nodeEntity),
                                        (void*)(got_tag));
    }
    {
        GrpcTag* got_tag(tagPool.construct(LeaseLeaseKeepAliveMessageId, (void*)GrpcOperation::INIT));

        auto& client = registry.emplace<AsyncLeaseLeaseKeepAliveGrpcClient>(nodeEntity);
        client.runtime = registry.get<GrpcClientRuntimePtr>(nodeEntity);
        client.runtime->calls.insert(&client.context);

        client.stream = registry
            .get<LeaseStubPtr>(nodeEntity)
            ->AsyncLeaseKeepAlive(&client.context,
                                        &registry.get<grpc::CompletionQueue>(nodeEntity),
                                        (void*)(got_tag));
    }

}

}// namespace etcdserverpb
