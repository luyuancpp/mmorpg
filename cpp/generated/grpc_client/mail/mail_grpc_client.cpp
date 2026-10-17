#include "muduo/base/Logging.h"

#include "mail_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetMailCallDeadline 按目标节点类型写入
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

namespace mailpb {
#pragma region ClientPlayerMailListMails
AsyncClientPlayerMailListMailsHandlerFunctionType AsyncClientPlayerMailListMailsHandler;
AsyncClientPlayerMailListMailsFailedHandlerFunctionType AsyncClientPlayerMailListMailsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailListMailsCall {
    ::mailpb::ListMailsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailListMailsHandler / FailedHandler。
    GrpcDone<::mailpb::ListMailsResponse> done;
};

void CompleteClientPlayerMailListMails(const ClientPlayerMailListMailsCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::ListMailsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailListMailsHandler) {
            AsyncClientPlayerMailListMailsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.ListMails reply dropped: AsyncClientPlayerMailListMailsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailListMailsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailListMailsMessageId, "ClientPlayerMail.ListMails", outcome, call.sentMetadata};
        AsyncClientPlayerMailListMailsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.ListMails failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailListMailsFailure(std::shared_ptr<ClientPlayerMailListMailsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailListMails(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailListMailsCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::ListMailsResponse> done) {
    auto call = std::make_shared<ClientPlayerMailListMailsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailListMailsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailListMailsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailListMailsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailListMails(*call, outcome, nullptr);
            return;
        }
        ::mailpb::ListMailsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.ListMails response";
            CompleteClientPlayerMailListMails(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailListMails(*call, outcome, &reply);
    });
}

void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request,
                                            GrpcDone<::mailpb::ListMailsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerMailListMailsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailListMails(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ListMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailListMailsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailListMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::ListMailsRequest& derived = static_cast<const ::mailpb::ListMailsRequest&>(message);
    SendClientPlayerMailListMails(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerMailReadMail
AsyncClientPlayerMailReadMailHandlerFunctionType AsyncClientPlayerMailReadMailHandler;
AsyncClientPlayerMailReadMailFailedHandlerFunctionType AsyncClientPlayerMailReadMailFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailReadMailCall {
    ::mailpb::ReadMailRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailReadMailHandler / FailedHandler。
    GrpcDone<::mailpb::ReadMailResponse> done;
};

void CompleteClientPlayerMailReadMail(const ClientPlayerMailReadMailCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::ReadMailResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailReadMailHandler) {
            AsyncClientPlayerMailReadMailHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.ReadMail reply dropped: AsyncClientPlayerMailReadMailHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailReadMailFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailReadMailMessageId, "ClientPlayerMail.ReadMail", outcome, call.sentMetadata};
        AsyncClientPlayerMailReadMailFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.ReadMail failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailReadMailFailure(std::shared_ptr<ClientPlayerMailReadMailCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailReadMail(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailReadMailCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::ReadMailResponse> done) {
    auto call = std::make_shared<ClientPlayerMailReadMailCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailReadMailFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailReadMailGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailReadMailFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailReadMail(*call, outcome, nullptr);
            return;
        }
        ::mailpb::ReadMailResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.ReadMail response";
            CompleteClientPlayerMailReadMail(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailReadMail(*call, outcome, &reply);
    });
}

void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request,
                                            GrpcDone<::mailpb::ReadMailResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerMailReadMailCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailReadMail(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ReadMailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailReadMailCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailReadMail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::ReadMailRequest& derived = static_cast<const ::mailpb::ReadMailRequest&>(message);
    SendClientPlayerMailReadMail(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerMailDeleteMails
AsyncClientPlayerMailDeleteMailsHandlerFunctionType AsyncClientPlayerMailDeleteMailsHandler;
AsyncClientPlayerMailDeleteMailsFailedHandlerFunctionType AsyncClientPlayerMailDeleteMailsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailDeleteMailsCall {
    ::mailpb::DeleteMailsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailDeleteMailsHandler / FailedHandler。
    GrpcDone<::mailpb::DeleteMailsResponse> done;
};

void CompleteClientPlayerMailDeleteMails(const ClientPlayerMailDeleteMailsCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::DeleteMailsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailDeleteMailsHandler) {
            AsyncClientPlayerMailDeleteMailsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.DeleteMails reply dropped: AsyncClientPlayerMailDeleteMailsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailDeleteMailsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailDeleteMailsMessageId, "ClientPlayerMail.DeleteMails", outcome, call.sentMetadata};
        AsyncClientPlayerMailDeleteMailsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.DeleteMails failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailDeleteMailsFailure(std::shared_ptr<ClientPlayerMailDeleteMailsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailDeleteMails(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailDeleteMailsCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::DeleteMailsResponse> done) {
    auto call = std::make_shared<ClientPlayerMailDeleteMailsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailDeleteMailsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailDeleteMailsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailDeleteMailsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailDeleteMails(*call, outcome, nullptr);
            return;
        }
        ::mailpb::DeleteMailsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.DeleteMails response";
            CompleteClientPlayerMailDeleteMails(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailDeleteMails(*call, outcome, &reply);
    });
}

void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request,
                                            GrpcDone<::mailpb::DeleteMailsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerMailDeleteMailsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailDeleteMails(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailDeleteMailsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailDeleteMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::DeleteMailsRequest& derived = static_cast<const ::mailpb::DeleteMailsRequest&>(message);
    SendClientPlayerMailDeleteMails(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerMailDeleteReadMails
AsyncClientPlayerMailDeleteReadMailsHandlerFunctionType AsyncClientPlayerMailDeleteReadMailsHandler;
AsyncClientPlayerMailDeleteReadMailsFailedHandlerFunctionType AsyncClientPlayerMailDeleteReadMailsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailDeleteReadMailsCall {
    ::mailpb::DeleteReadMailsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailDeleteReadMailsHandler / FailedHandler。
    GrpcDone<::mailpb::DeleteReadMailsResponse> done;
};

void CompleteClientPlayerMailDeleteReadMails(const ClientPlayerMailDeleteReadMailsCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::DeleteReadMailsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailDeleteReadMailsHandler) {
            AsyncClientPlayerMailDeleteReadMailsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.DeleteReadMails reply dropped: AsyncClientPlayerMailDeleteReadMailsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailDeleteReadMailsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailDeleteReadMailsMessageId, "ClientPlayerMail.DeleteReadMails", outcome, call.sentMetadata};
        AsyncClientPlayerMailDeleteReadMailsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.DeleteReadMails failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailDeleteReadMailsFailure(std::shared_ptr<ClientPlayerMailDeleteReadMailsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailDeleteReadMails(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailDeleteReadMailsCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteReadMailsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::DeleteReadMailsResponse> done) {
    auto call = std::make_shared<ClientPlayerMailDeleteReadMailsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailDeleteReadMailsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailDeleteReadMailsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailDeleteReadMailsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailDeleteReadMails(*call, outcome, nullptr);
            return;
        }
        ::mailpb::DeleteReadMailsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.DeleteReadMails response";
            CompleteClientPlayerMailDeleteReadMails(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailDeleteReadMails(*call, outcome, &reply);
    });
}

void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteReadMailsRequest& request,
                                            GrpcDone<::mailpb::DeleteReadMailsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerMailDeleteReadMailsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteReadMailsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailDeleteReadMails(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::DeleteReadMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailDeleteReadMailsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailDeleteReadMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::DeleteReadMailsRequest& derived = static_cast<const ::mailpb::DeleteReadMailsRequest&>(message);
    SendClientPlayerMailDeleteReadMails(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerMailClaimMailAttachments
AsyncClientPlayerMailClaimMailAttachmentsHandlerFunctionType AsyncClientPlayerMailClaimMailAttachmentsHandler;
AsyncClientPlayerMailClaimMailAttachmentsFailedHandlerFunctionType AsyncClientPlayerMailClaimMailAttachmentsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailClaimMailAttachmentsCall {
    ::mailpb::ClaimMailAttachmentsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailClaimMailAttachmentsHandler / FailedHandler。
    GrpcDone<::mailpb::ClaimMailAttachmentsResponse> done;
};

void CompleteClientPlayerMailClaimMailAttachments(const ClientPlayerMailClaimMailAttachmentsCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::ClaimMailAttachmentsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailClaimMailAttachmentsHandler) {
            AsyncClientPlayerMailClaimMailAttachmentsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.ClaimMailAttachments reply dropped: AsyncClientPlayerMailClaimMailAttachmentsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailClaimMailAttachmentsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailClaimMailAttachmentsMessageId, "ClientPlayerMail.ClaimMailAttachments", outcome, call.sentMetadata};
        AsyncClientPlayerMailClaimMailAttachmentsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.ClaimMailAttachments failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailClaimMailAttachmentsFailure(std::shared_ptr<ClientPlayerMailClaimMailAttachmentsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailClaimMailAttachments(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailClaimMailAttachmentsCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::ClaimMailAttachmentsResponse> done) {
    auto call = std::make_shared<ClientPlayerMailClaimMailAttachmentsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailClaimMailAttachmentsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailClaimMailAttachmentsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailClaimMailAttachmentsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailClaimMailAttachments(*call, outcome, nullptr);
            return;
        }
        ::mailpb::ClaimMailAttachmentsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.ClaimMailAttachments response";
            CompleteClientPlayerMailClaimMailAttachments(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailClaimMailAttachments(*call, outcome, &reply);
    });
}

void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request,
                                            GrpcDone<::mailpb::ClaimMailAttachmentsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerMailClaimMailAttachmentsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailClaimMailAttachments(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClaimMailAttachmentsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailClaimMailAttachmentsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailClaimMailAttachments(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::ClaimMailAttachmentsRequest& derived = static_cast<const ::mailpb::ClaimMailAttachmentsRequest&>(message);
    SendClientPlayerMailClaimMailAttachments(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerMailNotifyMailEvent
AsyncClientPlayerMailNotifyMailEventHandlerFunctionType AsyncClientPlayerMailNotifyMailEventHandler;
AsyncClientPlayerMailNotifyMailEventFailedHandlerFunctionType AsyncClientPlayerMailNotifyMailEventFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailNotifyMailEventCall {
    ::mailpb::MailEventS2C request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailNotifyMailEventHandler / FailedHandler。
    GrpcDone<::Empty> done;
};

void CompleteClientPlayerMailNotifyMailEvent(const ClientPlayerMailNotifyMailEventCall& call, const GrpcCallOutcome& outcome,
                                                const ::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailNotifyMailEventHandler) {
            AsyncClientPlayerMailNotifyMailEventHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.NotifyMailEvent reply dropped: AsyncClientPlayerMailNotifyMailEventHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailNotifyMailEventFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailNotifyMailEventMessageId, "ClientPlayerMail.NotifyMailEvent", outcome, call.sentMetadata};
        AsyncClientPlayerMailNotifyMailEventFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.NotifyMailEvent failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailNotifyMailEventFailure(std::shared_ptr<ClientPlayerMailNotifyMailEventCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailNotifyMailEvent(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailNotifyMailEventCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailEventS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    auto call = std::make_shared<ClientPlayerMailNotifyMailEventCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailNotifyMailEventFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailNotifyMailEventGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailNotifyMailEventFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailNotifyMailEvent(*call, outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.NotifyMailEvent response";
            CompleteClientPlayerMailNotifyMailEvent(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailNotifyMailEvent(*call, outcome, &reply);
    });
}

void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailEventS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerMailNotifyMailEventCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailEventS2C& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailNotifyMailEvent(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailEventS2C& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailNotifyMailEventCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailNotifyMailEvent(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::MailEventS2C& derived = static_cast<const ::mailpb::MailEventS2C&>(message);
    SendClientPlayerMailNotifyMailEvent(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetMailHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerMailListMailsHandler = handler;
    AsyncClientPlayerMailReadMailHandler = handler;
    AsyncClientPlayerMailDeleteMailsHandler = handler;
    AsyncClientPlayerMailDeleteReadMailsHandler = handler;
    AsyncClientPlayerMailClaimMailAttachmentsHandler = handler;
    AsyncClientPlayerMailNotifyMailEventHandler = handler;
    (void)handler;
}

void SetMailIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncClientPlayerMailListMailsHandler) {
        AsyncClientPlayerMailListMailsHandler = handler;
    }
    if (!AsyncClientPlayerMailReadMailHandler) {
        AsyncClientPlayerMailReadMailHandler = handler;
    }
    if (!AsyncClientPlayerMailDeleteMailsHandler) {
        AsyncClientPlayerMailDeleteMailsHandler = handler;
    }
    if (!AsyncClientPlayerMailDeleteReadMailsHandler) {
        AsyncClientPlayerMailDeleteReadMailsHandler = handler;
    }
    if (!AsyncClientPlayerMailClaimMailAttachmentsHandler) {
        AsyncClientPlayerMailClaimMailAttachmentsHandler = handler;
    }
    if (!AsyncClientPlayerMailNotifyMailEventHandler) {
        AsyncClientPlayerMailNotifyMailEventHandler = handler;
    }
    (void)handler;
}

void SetMailFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerMailListMailsFailedHandler = handler;
    AsyncClientPlayerMailReadMailFailedHandler = handler;
    AsyncClientPlayerMailDeleteMailsFailedHandler = handler;
    AsyncClientPlayerMailDeleteReadMailsFailedHandler = handler;
    AsyncClientPlayerMailClaimMailAttachmentsFailedHandler = handler;
    AsyncClientPlayerMailNotifyMailEventFailedHandler = handler;
    (void)handler;
}

void SetMailIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerMailListMailsFailedHandler) {
        AsyncClientPlayerMailListMailsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailReadMailFailedHandler) {
        AsyncClientPlayerMailReadMailFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailDeleteMailsFailedHandler) {
        AsyncClientPlayerMailDeleteMailsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailDeleteReadMailsFailedHandler) {
        AsyncClientPlayerMailDeleteReadMailsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailClaimMailAttachmentsFailedHandler) {
        AsyncClientPlayerMailClaimMailAttachmentsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailNotifyMailEventFailedHandler) {
        AsyncClientPlayerMailNotifyMailEventFailedHandler = handler;
    }
    (void)handler;
}

void SetMailCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace mailpb
