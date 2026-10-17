#include "muduo/base/Logging.h"

#include "mail_admin_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetMailAdminCallDeadline 按目标节点类型写入
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
#pragma region MailAdminSendPersonalMail
AsyncMailAdminSendPersonalMailHandlerFunctionType AsyncMailAdminSendPersonalMailHandler;
AsyncMailAdminSendPersonalMailFailedHandlerFunctionType AsyncMailAdminSendPersonalMailFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MailAdminSendPersonalMailCall {
    ::mailpb::SendMailRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMailAdminSendPersonalMailHandler / FailedHandler。
    GrpcDone<::mailpb::SendMailResponse> done;
};

void CompleteMailAdminSendPersonalMail(const MailAdminSendPersonalMailCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::SendMailResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMailAdminSendPersonalMailHandler) {
            AsyncMailAdminSendPersonalMailHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MailAdmin.SendPersonalMail reply dropped: AsyncMailAdminSendPersonalMailHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMailAdminSendPersonalMailFailedHandler) {
        const GrpcCallFailure failure{ MailAdminSendPersonalMailMessageId, "MailAdmin.SendPersonalMail", outcome, call.sentMetadata};
        AsyncMailAdminSendPersonalMailFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MailAdmin.SendPersonalMail failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMailAdminSendPersonalMailFailure(std::shared_ptr<MailAdminSendPersonalMailCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMailAdminSendPersonalMail(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMailAdminSendPersonalMailCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::SendMailResponse> done) {
    auto call = std::make_shared<MailAdminSendPersonalMailCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMailAdminSendPersonalMailFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMailAdminSendPersonalMailGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMailAdminSendPersonalMailFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMailAdminSendPersonalMail(*call, outcome, nullptr);
            return;
        }
        ::mailpb::SendMailResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MailAdmin.SendPersonalMail response";
            CompleteMailAdminSendPersonalMail(*call, outcome, nullptr);
            return;
        }
        CompleteMailAdminSendPersonalMail(*call, outcome, &reply);
    });
}

void SendMailAdminSendPersonalMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request,
                                            GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options) {
    StartMailAdminSendPersonalMailCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMailAdminSendPersonalMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMailAdminSendPersonalMail(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMailAdminSendPersonalMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMailAdminSendPersonalMailCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMailAdminSendPersonalMail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::SendMailRequest& derived = static_cast<const ::mailpb::SendMailRequest&>(message);
    SendMailAdminSendPersonalMail(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MailAdminSendSystemMail
AsyncMailAdminSendSystemMailHandlerFunctionType AsyncMailAdminSendSystemMailHandler;
AsyncMailAdminSendSystemMailFailedHandlerFunctionType AsyncMailAdminSendSystemMailFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MailAdminSendSystemMailCall {
    ::mailpb::SendMailRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMailAdminSendSystemMailHandler / FailedHandler。
    GrpcDone<::mailpb::SendMailResponse> done;
};

void CompleteMailAdminSendSystemMail(const MailAdminSendSystemMailCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::SendMailResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMailAdminSendSystemMailHandler) {
            AsyncMailAdminSendSystemMailHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MailAdmin.SendSystemMail reply dropped: AsyncMailAdminSendSystemMailHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMailAdminSendSystemMailFailedHandler) {
        const GrpcCallFailure failure{ MailAdminSendSystemMailMessageId, "MailAdmin.SendSystemMail", outcome, call.sentMetadata};
        AsyncMailAdminSendSystemMailFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MailAdmin.SendSystemMail failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMailAdminSendSystemMailFailure(std::shared_ptr<MailAdminSendSystemMailCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMailAdminSendSystemMail(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMailAdminSendSystemMailCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::SendMailResponse> done) {
    auto call = std::make_shared<MailAdminSendSystemMailCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMailAdminSendSystemMailFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMailAdminSendSystemMailGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMailAdminSendSystemMailFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMailAdminSendSystemMail(*call, outcome, nullptr);
            return;
        }
        ::mailpb::SendMailResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MailAdmin.SendSystemMail response";
            CompleteMailAdminSendSystemMail(*call, outcome, nullptr);
            return;
        }
        CompleteMailAdminSendSystemMail(*call, outcome, &reply);
    });
}

void SendMailAdminSendSystemMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request,
                                            GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options) {
    StartMailAdminSendSystemMailCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMailAdminSendSystemMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMailAdminSendSystemMail(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMailAdminSendSystemMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMailAdminSendSystemMailCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMailAdminSendSystemMail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::SendMailRequest& derived = static_cast<const ::mailpb::SendMailRequest&>(message);
    SendMailAdminSendSystemMail(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MailAdminSendGuildMail
AsyncMailAdminSendGuildMailHandlerFunctionType AsyncMailAdminSendGuildMailHandler;
AsyncMailAdminSendGuildMailFailedHandlerFunctionType AsyncMailAdminSendGuildMailFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MailAdminSendGuildMailCall {
    ::mailpb::SendMailRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMailAdminSendGuildMailHandler / FailedHandler。
    GrpcDone<::mailpb::SendMailResponse> done;
};

void CompleteMailAdminSendGuildMail(const MailAdminSendGuildMailCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::SendMailResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMailAdminSendGuildMailHandler) {
            AsyncMailAdminSendGuildMailHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MailAdmin.SendGuildMail reply dropped: AsyncMailAdminSendGuildMailHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMailAdminSendGuildMailFailedHandler) {
        const GrpcCallFailure failure{ MailAdminSendGuildMailMessageId, "MailAdmin.SendGuildMail", outcome, call.sentMetadata};
        AsyncMailAdminSendGuildMailFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MailAdmin.SendGuildMail failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMailAdminSendGuildMailFailure(std::shared_ptr<MailAdminSendGuildMailCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMailAdminSendGuildMail(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMailAdminSendGuildMailCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::SendMailResponse> done) {
    auto call = std::make_shared<MailAdminSendGuildMailCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMailAdminSendGuildMailFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMailAdminSendGuildMailGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMailAdminSendGuildMailFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMailAdminSendGuildMail(*call, outcome, nullptr);
            return;
        }
        ::mailpb::SendMailResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MailAdmin.SendGuildMail response";
            CompleteMailAdminSendGuildMail(*call, outcome, nullptr);
            return;
        }
        CompleteMailAdminSendGuildMail(*call, outcome, &reply);
    });
}

void SendMailAdminSendGuildMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request,
                                            GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options) {
    StartMailAdminSendGuildMailCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMailAdminSendGuildMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMailAdminSendGuildMail(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMailAdminSendGuildMail(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SendMailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMailAdminSendGuildMailCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMailAdminSendGuildMail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::SendMailRequest& derived = static_cast<const ::mailpb::SendMailRequest&>(message);
    SendMailAdminSendGuildMail(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MailAdminImportMailRecipients
AsyncMailAdminImportMailRecipientsHandlerFunctionType AsyncMailAdminImportMailRecipientsHandler;
AsyncMailAdminImportMailRecipientsFailedHandlerFunctionType AsyncMailAdminImportMailRecipientsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MailAdminImportMailRecipientsCall {
    ::mailpb::ImportMailRecipientsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMailAdminImportMailRecipientsHandler / FailedHandler。
    GrpcDone<::mailpb::ImportMailRecipientsResponse> done;
};

void CompleteMailAdminImportMailRecipients(const MailAdminImportMailRecipientsCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::ImportMailRecipientsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMailAdminImportMailRecipientsHandler) {
            AsyncMailAdminImportMailRecipientsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MailAdmin.ImportMailRecipients reply dropped: AsyncMailAdminImportMailRecipientsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMailAdminImportMailRecipientsFailedHandler) {
        const GrpcCallFailure failure{ MailAdminImportMailRecipientsMessageId, "MailAdmin.ImportMailRecipients", outcome, call.sentMetadata};
        AsyncMailAdminImportMailRecipientsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MailAdmin.ImportMailRecipients failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMailAdminImportMailRecipientsFailure(std::shared_ptr<MailAdminImportMailRecipientsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMailAdminImportMailRecipients(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMailAdminImportMailRecipientsCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ImportMailRecipientsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::ImportMailRecipientsResponse> done) {
    auto call = std::make_shared<MailAdminImportMailRecipientsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMailAdminImportMailRecipientsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMailAdminImportMailRecipientsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMailAdminImportMailRecipientsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMailAdminImportMailRecipients(*call, outcome, nullptr);
            return;
        }
        ::mailpb::ImportMailRecipientsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MailAdmin.ImportMailRecipients response";
            CompleteMailAdminImportMailRecipients(*call, outcome, nullptr);
            return;
        }
        CompleteMailAdminImportMailRecipients(*call, outcome, &reply);
    });
}

void SendMailAdminImportMailRecipients(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ImportMailRecipientsRequest& request,
                                            GrpcDone<::mailpb::ImportMailRecipientsResponse> done, const GrpcCallOptions& options) {
    StartMailAdminImportMailRecipientsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMailAdminImportMailRecipients(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ImportMailRecipientsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMailAdminImportMailRecipients(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMailAdminImportMailRecipients(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::ImportMailRecipientsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMailAdminImportMailRecipientsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMailAdminImportMailRecipients(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::ImportMailRecipientsRequest& derived = static_cast<const ::mailpb::ImportMailRecipientsRequest&>(message);
    SendMailAdminImportMailRecipients(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region MailAdminSealMailCampaign
AsyncMailAdminSealMailCampaignHandlerFunctionType AsyncMailAdminSealMailCampaignHandler;
AsyncMailAdminSealMailCampaignFailedHandlerFunctionType AsyncMailAdminSealMailCampaignFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct MailAdminSealMailCampaignCall {
    ::mailpb::SealMailCampaignRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncMailAdminSealMailCampaignHandler / FailedHandler。
    GrpcDone<::mailpb::SendMailResponse> done;
};

void CompleteMailAdminSealMailCampaign(const MailAdminSealMailCampaignCall& call, const GrpcCallOutcome& outcome,
                                                const ::mailpb::SendMailResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncMailAdminSealMailCampaignHandler) {
            AsyncMailAdminSealMailCampaignHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC MailAdmin.SealMailCampaign reply dropped: AsyncMailAdminSealMailCampaignHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncMailAdminSealMailCampaignFailedHandler) {
        const GrpcCallFailure failure{ MailAdminSealMailCampaignMessageId, "MailAdmin.SealMailCampaign", outcome, call.sentMetadata};
        AsyncMailAdminSealMailCampaignFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC MailAdmin.SealMailCampaign failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalMailAdminSealMailCampaignFailure(std::shared_ptr<MailAdminSealMailCampaignCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteMailAdminSealMailCampaign(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartMailAdminSealMailCampaignCall(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SealMailCampaignRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::mailpb::SendMailResponse> done) {
    auto call = std::make_shared<MailAdminSealMailCampaignCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalMailAdminSealMailCampaignFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kMailAdminSealMailCampaignGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalMailAdminSealMailCampaignFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteMailAdminSealMailCampaign(*call, outcome, nullptr);
            return;
        }
        ::mailpb::SendMailResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the MailAdmin.SealMailCampaign response";
            CompleteMailAdminSealMailCampaign(*call, outcome, nullptr);
            return;
        }
        CompleteMailAdminSealMailCampaign(*call, outcome, &reply);
    });
}

void SendMailAdminSealMailCampaign(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SealMailCampaignRequest& request,
                                            GrpcDone<::mailpb::SendMailResponse> done, const GrpcCallOptions& options) {
    StartMailAdminSealMailCampaignCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendMailAdminSealMailCampaign(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SealMailCampaignRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendMailAdminSealMailCampaign(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendMailAdminSealMailCampaign(entt::registry& registry, entt::entity nodeEntity, const ::mailpb::SealMailCampaignRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartMailAdminSealMailCampaignCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendMailAdminSealMailCampaign(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::mailpb::SealMailCampaignRequest& derived = static_cast<const ::mailpb::SealMailCampaignRequest&>(message);
    SendMailAdminSealMailCampaign(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetMailAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncMailAdminSendPersonalMailHandler = handler;
    AsyncMailAdminSendSystemMailHandler = handler;
    AsyncMailAdminSendGuildMailHandler = handler;
    AsyncMailAdminImportMailRecipientsHandler = handler;
    AsyncMailAdminSealMailCampaignHandler = handler;
    (void)handler;
}

void SetMailAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    if (!AsyncMailAdminSendPersonalMailHandler) {
        AsyncMailAdminSendPersonalMailHandler = handler;
    }
    if (!AsyncMailAdminSendSystemMailHandler) {
        AsyncMailAdminSendSystemMailHandler = handler;
    }
    if (!AsyncMailAdminSendGuildMailHandler) {
        AsyncMailAdminSendGuildMailHandler = handler;
    }
    if (!AsyncMailAdminImportMailRecipientsHandler) {
        AsyncMailAdminImportMailRecipientsHandler = handler;
    }
    if (!AsyncMailAdminSealMailCampaignHandler) {
        AsyncMailAdminSealMailCampaignHandler = handler;
    }
    (void)handler;
}

void SetMailAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncMailAdminSendPersonalMailFailedHandler = handler;
    AsyncMailAdminSendSystemMailFailedHandler = handler;
    AsyncMailAdminSendGuildMailFailedHandler = handler;
    AsyncMailAdminImportMailRecipientsFailedHandler = handler;
    AsyncMailAdminSealMailCampaignFailedHandler = handler;
    (void)handler;
}

void SetMailAdminIfEmptyFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    if (!AsyncMailAdminSendPersonalMailFailedHandler) {
        AsyncMailAdminSendPersonalMailFailedHandler = handler;
    }
    if (!AsyncMailAdminSendSystemMailFailedHandler) {
        AsyncMailAdminSendSystemMailFailedHandler = handler;
    }
    if (!AsyncMailAdminSendGuildMailFailedHandler) {
        AsyncMailAdminSendGuildMailFailedHandler = handler;
    }
    if (!AsyncMailAdminImportMailRecipientsFailedHandler) {
        AsyncMailAdminImportMailRecipientsFailedHandler = handler;
    }
    if (!AsyncMailAdminSealMailCampaignFailedHandler) {
        AsyncMailAdminSealMailCampaignFailedHandler = handler;
    }
    (void)handler;
}

void SetMailAdminCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace mailpb
