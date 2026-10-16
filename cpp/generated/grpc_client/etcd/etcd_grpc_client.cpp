#include "muduo/base/Logging.h"

#include "etcd_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetEtcdCallDeadline 按目标节点类型写入
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

namespace etcdserverpb {
#pragma region KVRange
AsyncKVRangeHandlerFunctionType AsyncKVRangeHandler;
AsyncKVRangeFailedHandlerFunctionType AsyncKVRangeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct KVRangeCall {
    ::etcdserverpb::RangeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncKVRangeHandler / FailedHandler。
    GrpcDone<::etcdserverpb::RangeResponse> done;
};

void CompleteKVRange(const KVRangeCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::RangeResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncKVRangeHandler) {
            AsyncKVRangeHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ KVRangeMessageId, "KV.Range", outcome, call.sentMetadata};
        AsyncKVRangeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC KV.Range failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalKVRangeFailure(std::shared_ptr<KVRangeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteKVRange(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVRangeCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::RangeResponse> done) {
    auto call = std::make_shared<KVRangeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalKVRangeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kKVRangeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalKVRangeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteKVRange(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::RangeResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the KV.Range response";
            CompleteKVRange(*call, outcome, nullptr);
            return;
        }
        CompleteKVRange(*call, outcome, &reply);
    });
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request,
                                            GrpcDone<::etcdserverpb::RangeResponse> done, const GrpcCallOptions& options) {
    StartKVRangeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVRange(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::RangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVRangeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendKVRange(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::RangeRequest& derived = static_cast<const ::etcdserverpb::RangeRequest&>(message);
    SendKVRange(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVPut
AsyncKVPutHandlerFunctionType AsyncKVPutHandler;
AsyncKVPutFailedHandlerFunctionType AsyncKVPutFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct KVPutCall {
    ::etcdserverpb::PutRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncKVPutHandler / FailedHandler。
    GrpcDone<::etcdserverpb::PutResponse> done;
};

void CompleteKVPut(const KVPutCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::PutResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncKVPutHandler) {
            AsyncKVPutHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ KVPutMessageId, "KV.Put", outcome, call.sentMetadata};
        AsyncKVPutFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC KV.Put failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalKVPutFailure(std::shared_ptr<KVPutCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteKVPut(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVPutCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::PutResponse> done) {
    auto call = std::make_shared<KVPutCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalKVPutFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kKVPutGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalKVPutFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteKVPut(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::PutResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the KV.Put response";
            CompleteKVPut(*call, outcome, nullptr);
            return;
        }
        CompleteKVPut(*call, outcome, &reply);
    });
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request,
                                            GrpcDone<::etcdserverpb::PutResponse> done, const GrpcCallOptions& options) {
    StartKVPutCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVPut(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::PutRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVPutCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendKVPut(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::PutRequest& derived = static_cast<const ::etcdserverpb::PutRequest&>(message);
    SendKVPut(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVDeleteRange
AsyncKVDeleteRangeHandlerFunctionType AsyncKVDeleteRangeHandler;
AsyncKVDeleteRangeFailedHandlerFunctionType AsyncKVDeleteRangeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct KVDeleteRangeCall {
    ::etcdserverpb::DeleteRangeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncKVDeleteRangeHandler / FailedHandler。
    GrpcDone<::etcdserverpb::DeleteRangeResponse> done;
};

void CompleteKVDeleteRange(const KVDeleteRangeCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::DeleteRangeResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncKVDeleteRangeHandler) {
            AsyncKVDeleteRangeHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ KVDeleteRangeMessageId, "KV.DeleteRange", outcome, call.sentMetadata};
        AsyncKVDeleteRangeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC KV.DeleteRange failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalKVDeleteRangeFailure(std::shared_ptr<KVDeleteRangeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteKVDeleteRange(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVDeleteRangeCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::DeleteRangeResponse> done) {
    auto call = std::make_shared<KVDeleteRangeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalKVDeleteRangeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kKVDeleteRangeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalKVDeleteRangeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteKVDeleteRange(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::DeleteRangeResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the KV.DeleteRange response";
            CompleteKVDeleteRange(*call, outcome, nullptr);
            return;
        }
        CompleteKVDeleteRange(*call, outcome, &reply);
    });
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request,
                                            GrpcDone<::etcdserverpb::DeleteRangeResponse> done, const GrpcCallOptions& options) {
    StartKVDeleteRangeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVDeleteRange(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::DeleteRangeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVDeleteRangeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendKVDeleteRange(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::DeleteRangeRequest& derived = static_cast<const ::etcdserverpb::DeleteRangeRequest&>(message);
    SendKVDeleteRange(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVTxn
AsyncKVTxnHandlerFunctionType AsyncKVTxnHandler;
AsyncKVTxnFailedHandlerFunctionType AsyncKVTxnFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct KVTxnCall {
    ::etcdserverpb::TxnRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncKVTxnHandler / FailedHandler。
    GrpcDone<::etcdserverpb::TxnResponse> done;
};

void CompleteKVTxn(const KVTxnCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::TxnResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncKVTxnHandler) {
            AsyncKVTxnHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ KVTxnMessageId, "KV.Txn", outcome, call.sentMetadata};
        AsyncKVTxnFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC KV.Txn failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalKVTxnFailure(std::shared_ptr<KVTxnCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteKVTxn(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVTxnCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::TxnResponse> done) {
    auto call = std::make_shared<KVTxnCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalKVTxnFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kKVTxnGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalKVTxnFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteKVTxn(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::TxnResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the KV.Txn response";
            CompleteKVTxn(*call, outcome, nullptr);
            return;
        }
        CompleteKVTxn(*call, outcome, &reply);
    });
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request,
                                            GrpcDone<::etcdserverpb::TxnResponse> done, const GrpcCallOptions& options) {
    StartKVTxnCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVTxn(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::TxnRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVTxnCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendKVTxn(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::TxnRequest& derived = static_cast<const ::etcdserverpb::TxnRequest&>(message);
    SendKVTxn(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region KVCompact
AsyncKVCompactHandlerFunctionType AsyncKVCompactHandler;
AsyncKVCompactFailedHandlerFunctionType AsyncKVCompactFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct KVCompactCall {
    ::etcdserverpb::CompactionRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncKVCompactHandler / FailedHandler。
    GrpcDone<::etcdserverpb::CompactionResponse> done;
};

void CompleteKVCompact(const KVCompactCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::CompactionResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncKVCompactHandler) {
            AsyncKVCompactHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ KVCompactMessageId, "KV.Compact", outcome, call.sentMetadata};
        AsyncKVCompactFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC KV.Compact failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalKVCompactFailure(std::shared_ptr<KVCompactCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteKVCompact(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartKVCompactCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::CompactionResponse> done) {
    auto call = std::make_shared<KVCompactCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalKVCompactFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kKVCompactGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalKVCompactFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteKVCompact(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::CompactionResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the KV.Compact response";
            CompleteKVCompact(*call, outcome, nullptr);
            return;
        }
        CompleteKVCompact(*call, outcome, &reply);
    });
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request,
                                            GrpcDone<::etcdserverpb::CompactionResponse> done, const GrpcCallOptions& options) {
    StartKVCompactCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendKVCompact(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::CompactionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartKVCompactCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendKVCompact(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::CompactionRequest& derived = static_cast<const ::etcdserverpb::CompactionRequest&>(message);
    SendKVCompact(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region WatchWatch

void SendWatchWatch(entt::registry&, entt::entity, const ::etcdserverpb::WatchRequest&) {
    LOG_ERROR << "gRPC Watch.Watch is a streaming method; the generated client does not support it";
}

void SendWatchWatch(entt::registry&, entt::entity, const ::etcdserverpb::WatchRequest&, const std::vector<std::string>&, const std::vector<std::string>&) {
    LOG_ERROR << "gRPC Watch.Watch is a streaming method; the generated client does not support it";
}

void SendWatchWatch(entt::registry&, entt::entity, const google::protobuf::Message&, const std::vector<std::string>&, const std::vector<std::string>&) {
    LOG_ERROR << "gRPC Watch.Watch is a streaming method; the generated client does not support it";
}
#pragma endregion
#pragma region LeaseLeaseGrant
AsyncLeaseLeaseGrantHandlerFunctionType AsyncLeaseLeaseGrantHandler;
AsyncLeaseLeaseGrantFailedHandlerFunctionType AsyncLeaseLeaseGrantFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct LeaseLeaseGrantCall {
    ::etcdserverpb::LeaseGrantRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncLeaseLeaseGrantHandler / FailedHandler。
    GrpcDone<::etcdserverpb::LeaseGrantResponse> done;
};

void CompleteLeaseLeaseGrant(const LeaseLeaseGrantCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::LeaseGrantResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncLeaseLeaseGrantHandler) {
            AsyncLeaseLeaseGrantHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ LeaseLeaseGrantMessageId, "Lease.LeaseGrant", outcome, call.sentMetadata};
        AsyncLeaseLeaseGrantFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseGrant failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalLeaseLeaseGrantFailure(std::shared_ptr<LeaseLeaseGrantCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteLeaseLeaseGrant(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseGrantCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseGrantResponse> done) {
    auto call = std::make_shared<LeaseLeaseGrantCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalLeaseLeaseGrantFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kLeaseLeaseGrantGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalLeaseLeaseGrantFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteLeaseLeaseGrant(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::LeaseGrantResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the Lease.LeaseGrant response";
            CompleteLeaseLeaseGrant(*call, outcome, nullptr);
            return;
        }
        CompleteLeaseLeaseGrant(*call, outcome, &reply);
    });
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseGrantResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseGrantCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseGrant(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseGrantRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseGrantCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendLeaseLeaseGrant(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::LeaseGrantRequest& derived = static_cast<const ::etcdserverpb::LeaseGrantRequest&>(message);
    SendLeaseLeaseGrant(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseRevoke
AsyncLeaseLeaseRevokeHandlerFunctionType AsyncLeaseLeaseRevokeHandler;
AsyncLeaseLeaseRevokeFailedHandlerFunctionType AsyncLeaseLeaseRevokeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct LeaseLeaseRevokeCall {
    ::etcdserverpb::LeaseRevokeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncLeaseLeaseRevokeHandler / FailedHandler。
    GrpcDone<::etcdserverpb::LeaseRevokeResponse> done;
};

void CompleteLeaseLeaseRevoke(const LeaseLeaseRevokeCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::LeaseRevokeResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncLeaseLeaseRevokeHandler) {
            AsyncLeaseLeaseRevokeHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ LeaseLeaseRevokeMessageId, "Lease.LeaseRevoke", outcome, call.sentMetadata};
        AsyncLeaseLeaseRevokeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseRevoke failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalLeaseLeaseRevokeFailure(std::shared_ptr<LeaseLeaseRevokeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteLeaseLeaseRevoke(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseRevokeCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseRevokeResponse> done) {
    auto call = std::make_shared<LeaseLeaseRevokeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalLeaseLeaseRevokeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kLeaseLeaseRevokeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalLeaseLeaseRevokeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteLeaseLeaseRevoke(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::LeaseRevokeResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the Lease.LeaseRevoke response";
            CompleteLeaseLeaseRevoke(*call, outcome, nullptr);
            return;
        }
        CompleteLeaseLeaseRevoke(*call, outcome, &reply);
    });
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseRevokeResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseRevokeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseRevoke(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseRevokeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseRevokeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendLeaseLeaseRevoke(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::LeaseRevokeRequest& derived = static_cast<const ::etcdserverpb::LeaseRevokeRequest&>(message);
    SendLeaseLeaseRevoke(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseKeepAlive

void SendLeaseLeaseKeepAlive(entt::registry&, entt::entity, const ::etcdserverpb::LeaseKeepAliveRequest&) {
    LOG_ERROR << "gRPC Lease.LeaseKeepAlive is a streaming method; the generated client does not support it";
}

void SendLeaseLeaseKeepAlive(entt::registry&, entt::entity, const ::etcdserverpb::LeaseKeepAliveRequest&, const std::vector<std::string>&, const std::vector<std::string>&) {
    LOG_ERROR << "gRPC Lease.LeaseKeepAlive is a streaming method; the generated client does not support it";
}

void SendLeaseLeaseKeepAlive(entt::registry&, entt::entity, const google::protobuf::Message&, const std::vector<std::string>&, const std::vector<std::string>&) {
    LOG_ERROR << "gRPC Lease.LeaseKeepAlive is a streaming method; the generated client does not support it";
}
#pragma endregion
#pragma region LeaseLeaseTimeToLive
AsyncLeaseLeaseTimeToLiveHandlerFunctionType AsyncLeaseLeaseTimeToLiveHandler;
AsyncLeaseLeaseTimeToLiveFailedHandlerFunctionType AsyncLeaseLeaseTimeToLiveFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct LeaseLeaseTimeToLiveCall {
    ::etcdserverpb::LeaseTimeToLiveRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncLeaseLeaseTimeToLiveHandler / FailedHandler。
    GrpcDone<::etcdserverpb::LeaseTimeToLiveResponse> done;
};

void CompleteLeaseLeaseTimeToLive(const LeaseLeaseTimeToLiveCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::LeaseTimeToLiveResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncLeaseLeaseTimeToLiveHandler) {
            AsyncLeaseLeaseTimeToLiveHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ LeaseLeaseTimeToLiveMessageId, "Lease.LeaseTimeToLive", outcome, call.sentMetadata};
        AsyncLeaseLeaseTimeToLiveFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseTimeToLive failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalLeaseLeaseTimeToLiveFailure(std::shared_ptr<LeaseLeaseTimeToLiveCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteLeaseLeaseTimeToLive(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseTimeToLiveCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseTimeToLiveResponse> done) {
    auto call = std::make_shared<LeaseLeaseTimeToLiveCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalLeaseLeaseTimeToLiveFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kLeaseLeaseTimeToLiveGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalLeaseLeaseTimeToLiveFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteLeaseLeaseTimeToLive(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::LeaseTimeToLiveResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the Lease.LeaseTimeToLive response";
            CompleteLeaseLeaseTimeToLive(*call, outcome, nullptr);
            return;
        }
        CompleteLeaseLeaseTimeToLive(*call, outcome, &reply);
    });
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseTimeToLiveResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseTimeToLiveCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseTimeToLive(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseTimeToLiveRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseTimeToLiveCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendLeaseLeaseTimeToLive(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::LeaseTimeToLiveRequest& derived = static_cast<const ::etcdserverpb::LeaseTimeToLiveRequest&>(message);
    SendLeaseLeaseTimeToLive(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region LeaseLeaseLeases
AsyncLeaseLeaseLeasesHandlerFunctionType AsyncLeaseLeaseLeasesHandler;
AsyncLeaseLeaseLeasesFailedHandlerFunctionType AsyncLeaseLeaseLeasesFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct LeaseLeaseLeasesCall {
    ::etcdserverpb::LeaseLeasesRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncLeaseLeaseLeasesHandler / FailedHandler。
    GrpcDone<::etcdserverpb::LeaseLeasesResponse> done;
};

void CompleteLeaseLeaseLeases(const LeaseLeaseLeasesCall& call, const GrpcCallOutcome& outcome,
                                                const ::etcdserverpb::LeaseLeasesResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncLeaseLeaseLeasesHandler) {
            AsyncLeaseLeaseLeasesHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ LeaseLeaseLeasesMessageId, "Lease.LeaseLeases", outcome, call.sentMetadata};
        AsyncLeaseLeaseLeasesFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC Lease.LeaseLeases failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalLeaseLeaseLeasesFailure(std::shared_ptr<LeaseLeaseLeasesCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteLeaseLeaseLeases(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartLeaseLeaseLeasesCall(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::etcdserverpb::LeaseLeasesResponse> done) {
    auto call = std::make_shared<LeaseLeaseLeasesCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalLeaseLeaseLeasesFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kLeaseLeaseLeasesGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalLeaseLeaseLeasesFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteLeaseLeaseLeases(*call, outcome, nullptr);
            return;
        }
        ::etcdserverpb::LeaseLeasesResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the Lease.LeaseLeases response";
            CompleteLeaseLeaseLeases(*call, outcome, nullptr);
            return;
        }
        CompleteLeaseLeaseLeases(*call, outcome, &reply);
    });
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request,
                                            GrpcDone<::etcdserverpb::LeaseLeasesResponse> done, const GrpcCallOptions& options) {
    StartLeaseLeaseLeasesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendLeaseLeaseLeases(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const ::etcdserverpb::LeaseLeasesRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartLeaseLeaseLeasesCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendLeaseLeaseLeases(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::etcdserverpb::LeaseLeasesRequest& derived = static_cast<const ::etcdserverpb::LeaseLeasesRequest&>(message);
    SendLeaseLeaseLeases(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetEtcdHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncKVRangeHandler = handler;
    AsyncKVPutHandler = handler;
    AsyncKVDeleteRangeHandler = handler;
    AsyncKVTxnHandler = handler;
    AsyncKVCompactHandler = handler;
    AsyncLeaseLeaseGrantHandler = handler;
    AsyncLeaseLeaseRevokeHandler = handler;
    AsyncLeaseLeaseTimeToLiveHandler = handler;
    AsyncLeaseLeaseLeasesHandler = handler;
    (void)handler;
}

void SetEtcdIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
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
    if (!AsyncLeaseLeaseGrantHandler) {
        AsyncLeaseLeaseGrantHandler = handler;
    }
    if (!AsyncLeaseLeaseRevokeHandler) {
        AsyncLeaseLeaseRevokeHandler = handler;
    }
    if (!AsyncLeaseLeaseTimeToLiveHandler) {
        AsyncLeaseLeaseTimeToLiveHandler = handler;
    }
    if (!AsyncLeaseLeaseLeasesHandler) {
        AsyncLeaseLeaseLeasesHandler = handler;
    }
    (void)handler;
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
    (void)handler;
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
    (void)handler;
}

void SetEtcdCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace etcdserverpb
