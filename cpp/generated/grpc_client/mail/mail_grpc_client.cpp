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
#pragma region ClientPlayerMailClearReadMails
AsyncClientPlayerMailClearReadMailsHandlerFunctionType AsyncClientPlayerMailClearReadMailsHandler;
AsyncClientPlayerMailClearReadMailsFailedHandlerFunctionType AsyncClientPlayerMailClearReadMailsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailClearReadMailsCall {
    ::mailpb::ClearReadMailsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailClearReadMailsHandler / FailedHandler。
    GrpcDone<::mailpb::ClearReadMailsResponse> done;
};

void CompleteClientPlayerMailClearReadMails(const ClientPlayerMailClearReadMailsCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::ClearReadMailsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailClearReadMailsHandler) {
            AsyncClientPlayerMailClearReadMailsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.ClearReadMails reply dropped: AsyncClientPlayerMailClearReadMailsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailClearReadMailsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailClearReadMailsMessageId, "ClientPlayerMail.ClearReadMails", outcome, call.sentMetadata};
        AsyncClientPlayerMailClearReadMailsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.ClearReadMails failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailClearReadMailsFailure(std::shared_ptr<ClientPlayerMailClearReadMailsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailClearReadMails(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailClearReadMailsCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::ClearReadMailsResponse> done) {
    auto call = std::make_shared<ClientPlayerMailClearReadMailsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailClearReadMailsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailClearReadMailsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailClearReadMailsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailClearReadMails(*call, outcome, nullptr);
            return;
        }
        ::mailpb::ClearReadMailsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.ClearReadMails response";
            CompleteClientPlayerMailClearReadMails(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailClearReadMails(*call, outcome, &reply);
    });
}

void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request,
                                            GrpcDone<::mailpb::ClearReadMailsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerMailClearReadMailsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailClearReadMails(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ClearReadMailsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailClearReadMailsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailClearReadMails(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::ClearReadMailsRequest& derived = static_cast<const ::mailpb::ClearReadMailsRequest&>(message);
    SendClientPlayerMailClearReadMails(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerMailNotifyMailChanged
AsyncClientPlayerMailNotifyMailChangedHandlerFunctionType AsyncClientPlayerMailNotifyMailChangedHandler;
AsyncClientPlayerMailNotifyMailChangedFailedHandlerFunctionType AsyncClientPlayerMailNotifyMailChangedFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerMailNotifyMailChangedCall {
    ::mailpb::MailChangedEvent request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerMailNotifyMailChangedHandler / FailedHandler。
    GrpcDone<::mailpb::MailChangedEvent> done;
};

void CompleteClientPlayerMailNotifyMailChanged(const ClientPlayerMailNotifyMailChangedCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::MailChangedEvent* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerMailNotifyMailChangedHandler) {
            AsyncClientPlayerMailNotifyMailChangedHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerMail.NotifyMailChanged reply dropped: AsyncClientPlayerMailNotifyMailChangedHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerMailNotifyMailChangedFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerMailNotifyMailChangedMessageId, "ClientPlayerMail.NotifyMailChanged", outcome, call.sentMetadata};
        AsyncClientPlayerMailNotifyMailChangedFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerMail.NotifyMailChanged failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerMailNotifyMailChangedFailure(std::shared_ptr<ClientPlayerMailNotifyMailChangedCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerMailNotifyMailChanged(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerMailNotifyMailChangedCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::MailChangedEvent> done) {
    auto call = std::make_shared<ClientPlayerMailNotifyMailChangedCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerMailNotifyMailChangedFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerMailNotifyMailChangedGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerMailNotifyMailChangedFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerMailNotifyMailChanged(*call, outcome, nullptr);
            return;
        }
        ::mailpb::MailChangedEvent reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerMail.NotifyMailChanged response";
            CompleteClientPlayerMailNotifyMailChanged(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerMailNotifyMailChanged(*call, outcome, &reply);
    });
}

void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request,
                                            GrpcDone<::mailpb::MailChangedEvent> done, const GrpcCallOptions& options) {
    StartClientPlayerMailNotifyMailChangedCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerMailNotifyMailChanged(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::MailChangedEvent& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerMailNotifyMailChangedCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerMailNotifyMailChanged(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::MailChangedEvent& derived = static_cast<const ::mailpb::MailChangedEvent&>(message);
    SendClientPlayerMailNotifyMailChanged(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetMailHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerMailListMailsHandler = handler;
    AsyncClientPlayerMailReadMailHandler = handler;
    AsyncClientPlayerMailClaimMailAttachmentsHandler = handler;
    AsyncClientPlayerMailDeleteMailsHandler = handler;
    AsyncClientPlayerMailClearReadMailsHandler = handler;
    AsyncClientPlayerMailNotifyMailChangedHandler = handler;
    (void)handler;
}

void SetMailIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncClientPlayerMailListMailsHandler) {
        AsyncClientPlayerMailListMailsHandler = handler;
    }
    if (!AsyncClientPlayerMailReadMailHandler) {
        AsyncClientPlayerMailReadMailHandler = handler;
    }
    if (!AsyncClientPlayerMailClaimMailAttachmentsHandler) {
        AsyncClientPlayerMailClaimMailAttachmentsHandler = handler;
    }
    if (!AsyncClientPlayerMailDeleteMailsHandler) {
        AsyncClientPlayerMailDeleteMailsHandler = handler;
    }
    if (!AsyncClientPlayerMailClearReadMailsHandler) {
        AsyncClientPlayerMailClearReadMailsHandler = handler;
    }
    if (!AsyncClientPlayerMailNotifyMailChangedHandler) {
        AsyncClientPlayerMailNotifyMailChangedHandler = handler;
    }
    (void)handler;
}

void SetMailFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerMailListMailsFailedHandler = handler;
    AsyncClientPlayerMailReadMailFailedHandler = handler;
    AsyncClientPlayerMailClaimMailAttachmentsFailedHandler = handler;
    AsyncClientPlayerMailDeleteMailsFailedHandler = handler;
    AsyncClientPlayerMailClearReadMailsFailedHandler = handler;
    AsyncClientPlayerMailNotifyMailChangedFailedHandler = handler;
    (void)handler;
}

void SetMailIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncClientPlayerMailListMailsFailedHandler) {
        AsyncClientPlayerMailListMailsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailReadMailFailedHandler) {
        AsyncClientPlayerMailReadMailFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailClaimMailAttachmentsFailedHandler) {
        AsyncClientPlayerMailClaimMailAttachmentsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailDeleteMailsFailedHandler) {
        AsyncClientPlayerMailDeleteMailsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailClearReadMailsFailedHandler) {
        AsyncClientPlayerMailClearReadMailsFailedHandler = handler;
    }
    if (!AsyncClientPlayerMailNotifyMailChangedFailedHandler) {
        AsyncClientPlayerMailNotifyMailChangedFailedHandler = handler;
    }
    (void)handler;
}

void SetMailCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace mailpb
