#include "muduo/base/Logging.h"

#include "player_inbox_admin_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetPlayerInboxAdminCallDeadline 按目标节点类型写入
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

namespace player_inbox {
#pragma region PlayerInboxAdminGmSanctionPlayer
AsyncPlayerInboxAdminGmSanctionPlayerHandlerFunctionType AsyncPlayerInboxAdminGmSanctionPlayerHandler;
AsyncPlayerInboxAdminGmSanctionPlayerFailedHandlerFunctionType AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxAdminGmSanctionPlayerCall {
    ::player_inbox::GmSanctionPlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminGmSanctionPlayerHandler / FailedHandler。
    GrpcDone<::player_inbox::GmSanctionPlayerResponse> done;
};

void CompletePlayerInboxAdminGmSanctionPlayer(const PlayerInboxAdminGmSanctionPlayerCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::GmSanctionPlayerResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxAdminGmSanctionPlayerHandler) {
            AsyncPlayerInboxAdminGmSanctionPlayerHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxAdminGmSanctionPlayerMessageId, "PlayerInboxAdmin.GmSanctionPlayer", outcome, call.sentMetadata};
        AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.GmSanctionPlayer failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxAdminGmSanctionPlayerFailure(std::shared_ptr<PlayerInboxAdminGmSanctionPlayerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxAdminGmSanctionPlayer(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminGmSanctionPlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::GmSanctionPlayerResponse> done) {
    auto call = std::make_shared<PlayerInboxAdminGmSanctionPlayerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxAdminGmSanctionPlayerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxAdminGmSanctionPlayerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxAdminGmSanctionPlayerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerInboxAdminGmSanctionPlayer(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::GmSanctionPlayerResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInboxAdmin.GmSanctionPlayer response";
            CompletePlayerInboxAdminGmSanctionPlayer(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxAdminGmSanctionPlayer(*call, outcome, &reply);
    });
}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request,
                                            GrpcDone<::player_inbox::GmSanctionPlayerResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminGmSanctionPlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminGmSanctionPlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GmSanctionPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminGmSanctionPlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxAdminGmSanctionPlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::GmSanctionPlayerRequest& derived = static_cast<const ::player_inbox::GmSanctionPlayerRequest&>(message);
    SendPlayerInboxAdminGmSanctionPlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminGetSummary
AsyncPlayerInboxAdminGetSummaryHandlerFunctionType AsyncPlayerInboxAdminGetSummaryHandler;
AsyncPlayerInboxAdminGetSummaryFailedHandlerFunctionType AsyncPlayerInboxAdminGetSummaryFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxAdminGetSummaryCall {
    ::player_inbox::GetPlayerInboxSummaryRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminGetSummaryHandler / FailedHandler。
    GrpcDone<::player_inbox::GetPlayerInboxSummaryResponse> done;
};

void CompletePlayerInboxAdminGetSummary(const PlayerInboxAdminGetSummaryCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::GetPlayerInboxSummaryResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxAdminGetSummaryHandler) {
            AsyncPlayerInboxAdminGetSummaryHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxAdminGetSummaryMessageId, "PlayerInboxAdmin.GetSummary", outcome, call.sentMetadata};
        AsyncPlayerInboxAdminGetSummaryFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.GetSummary failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxAdminGetSummaryFailure(std::shared_ptr<PlayerInboxAdminGetSummaryCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxAdminGetSummary(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminGetSummaryCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::GetPlayerInboxSummaryResponse> done) {
    auto call = std::make_shared<PlayerInboxAdminGetSummaryCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxAdminGetSummaryFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxAdminGetSummaryGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxAdminGetSummaryFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerInboxAdminGetSummary(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::GetPlayerInboxSummaryResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInboxAdmin.GetSummary response";
            CompletePlayerInboxAdminGetSummary(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxAdminGetSummary(*call, outcome, &reply);
    });
}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request,
                                            GrpcDone<::player_inbox::GetPlayerInboxSummaryResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminGetSummaryCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminGetSummary(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::GetPlayerInboxSummaryRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminGetSummaryCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxAdminGetSummary(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::GetPlayerInboxSummaryRequest& derived = static_cast<const ::player_inbox::GetPlayerInboxSummaryRequest&>(message);
    SendPlayerInboxAdminGetSummary(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminListItems
AsyncPlayerInboxAdminListItemsHandlerFunctionType AsyncPlayerInboxAdminListItemsHandler;
AsyncPlayerInboxAdminListItemsFailedHandlerFunctionType AsyncPlayerInboxAdminListItemsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxAdminListItemsCall {
    ::player_inbox::ListPlayerInboxItemsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminListItemsHandler / FailedHandler。
    GrpcDone<::player_inbox::ListPlayerInboxItemsResponse> done;
};

void CompletePlayerInboxAdminListItems(const PlayerInboxAdminListItemsCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::ListPlayerInboxItemsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxAdminListItemsHandler) {
            AsyncPlayerInboxAdminListItemsHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxAdminListItemsMessageId, "PlayerInboxAdmin.ListItems", outcome, call.sentMetadata};
        AsyncPlayerInboxAdminListItemsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.ListItems failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxAdminListItemsFailure(std::shared_ptr<PlayerInboxAdminListItemsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxAdminListItems(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminListItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::ListPlayerInboxItemsResponse> done) {
    auto call = std::make_shared<PlayerInboxAdminListItemsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxAdminListItemsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxAdminListItemsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxAdminListItemsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerInboxAdminListItems(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::ListPlayerInboxItemsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInboxAdmin.ListItems response";
            CompletePlayerInboxAdminListItems(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxAdminListItems(*call, outcome, &reply);
    });
}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request,
                                            GrpcDone<::player_inbox::ListPlayerInboxItemsResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminListItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminListItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ListPlayerInboxItemsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminListItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxAdminListItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::ListPlayerInboxItemsRequest& derived = static_cast<const ::player_inbox::ListPlayerInboxItemsRequest&>(message);
    SendPlayerInboxAdminListItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminLookupByRequest
AsyncPlayerInboxAdminLookupByRequestHandlerFunctionType AsyncPlayerInboxAdminLookupByRequestHandler;
AsyncPlayerInboxAdminLookupByRequestFailedHandlerFunctionType AsyncPlayerInboxAdminLookupByRequestFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxAdminLookupByRequestCall {
    ::player_inbox::LookupPlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminLookupByRequestHandler / FailedHandler。
    GrpcDone<::player_inbox::LookupPlayerInboxResponse> done;
};

void CompletePlayerInboxAdminLookupByRequest(const PlayerInboxAdminLookupByRequestCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::LookupPlayerInboxResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxAdminLookupByRequestHandler) {
            AsyncPlayerInboxAdminLookupByRequestHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxAdminLookupByRequestMessageId, "PlayerInboxAdmin.LookupByRequest", outcome, call.sentMetadata};
        AsyncPlayerInboxAdminLookupByRequestFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.LookupByRequest failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxAdminLookupByRequestFailure(std::shared_ptr<PlayerInboxAdminLookupByRequestCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxAdminLookupByRequest(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminLookupByRequestCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::LookupPlayerInboxResponse> done) {
    auto call = std::make_shared<PlayerInboxAdminLookupByRequestCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxAdminLookupByRequestFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxAdminLookupByRequestGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxAdminLookupByRequestFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerInboxAdminLookupByRequest(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::LookupPlayerInboxResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInboxAdmin.LookupByRequest response";
            CompletePlayerInboxAdminLookupByRequest(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxAdminLookupByRequest(*call, outcome, &reply);
    });
}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::LookupPlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminLookupByRequestCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminLookupByRequest(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::LookupPlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminLookupByRequestCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxAdminLookupByRequest(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::LookupPlayerInboxRequest& derived = static_cast<const ::player_inbox::LookupPlayerInboxRequest&>(message);
    SendPlayerInboxAdminLookupByRequest(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminVoidItem
AsyncPlayerInboxAdminVoidItemHandlerFunctionType AsyncPlayerInboxAdminVoidItemHandler;
AsyncPlayerInboxAdminVoidItemFailedHandlerFunctionType AsyncPlayerInboxAdminVoidItemFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxAdminVoidItemCall {
    ::player_inbox::VoidPlayerInboxItemRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminVoidItemHandler / FailedHandler。
    GrpcDone<::player_inbox::VoidPlayerInboxItemResponse> done;
};

void CompletePlayerInboxAdminVoidItem(const PlayerInboxAdminVoidItemCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::VoidPlayerInboxItemResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxAdminVoidItemHandler) {
            AsyncPlayerInboxAdminVoidItemHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxAdminVoidItemMessageId, "PlayerInboxAdmin.VoidItem", outcome, call.sentMetadata};
        AsyncPlayerInboxAdminVoidItemFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.VoidItem failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxAdminVoidItemFailure(std::shared_ptr<PlayerInboxAdminVoidItemCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxAdminVoidItem(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminVoidItemCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::VoidPlayerInboxItemResponse> done) {
    auto call = std::make_shared<PlayerInboxAdminVoidItemCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxAdminVoidItemFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxAdminVoidItemGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxAdminVoidItemFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerInboxAdminVoidItem(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::VoidPlayerInboxItemResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInboxAdmin.VoidItem response";
            CompletePlayerInboxAdminVoidItem(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxAdminVoidItem(*call, outcome, &reply);
    });
}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request,
                                            GrpcDone<::player_inbox::VoidPlayerInboxItemResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminVoidItemCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminVoidItem(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::VoidPlayerInboxItemRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminVoidItemCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxAdminVoidItem(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::VoidPlayerInboxItemRequest& derived = static_cast<const ::player_inbox::VoidPlayerInboxItemRequest&>(message);
    SendPlayerInboxAdminVoidItem(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region PlayerInboxAdminReconcilePlayer
AsyncPlayerInboxAdminReconcilePlayerHandlerFunctionType AsyncPlayerInboxAdminReconcilePlayerHandler;
AsyncPlayerInboxAdminReconcilePlayerFailedHandlerFunctionType AsyncPlayerInboxAdminReconcilePlayerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct PlayerInboxAdminReconcilePlayerCall {
    ::player_inbox::ReconcilePlayerInboxRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncPlayerInboxAdminReconcilePlayerHandler / FailedHandler。
    GrpcDone<::player_inbox::ReconcilePlayerInboxResponse> done;
};

void CompletePlayerInboxAdminReconcilePlayer(const PlayerInboxAdminReconcilePlayerCall& call, const GrpcCallOutcome& outcome,
                                                const ::player_inbox::ReconcilePlayerInboxResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncPlayerInboxAdminReconcilePlayerHandler) {
            AsyncPlayerInboxAdminReconcilePlayerHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ PlayerInboxAdminReconcilePlayerMessageId, "PlayerInboxAdmin.ReconcilePlayer", outcome, call.sentMetadata};
        AsyncPlayerInboxAdminReconcilePlayerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC PlayerInboxAdmin.ReconcilePlayer failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalPlayerInboxAdminReconcilePlayerFailure(std::shared_ptr<PlayerInboxAdminReconcilePlayerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompletePlayerInboxAdminReconcilePlayer(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartPlayerInboxAdminReconcilePlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::player_inbox::ReconcilePlayerInboxResponse> done) {
    auto call = std::make_shared<PlayerInboxAdminReconcilePlayerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalPlayerInboxAdminReconcilePlayerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kPlayerInboxAdminReconcilePlayerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalPlayerInboxAdminReconcilePlayerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompletePlayerInboxAdminReconcilePlayer(*call, outcome, nullptr);
            return;
        }
        ::player_inbox::ReconcilePlayerInboxResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the PlayerInboxAdmin.ReconcilePlayer response";
            CompletePlayerInboxAdminReconcilePlayer(*call, outcome, nullptr);
            return;
        }
        CompletePlayerInboxAdminReconcilePlayer(*call, outcome, &reply);
    });
}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request,
                                            GrpcDone<::player_inbox::ReconcilePlayerInboxResponse> done, const GrpcCallOptions& options) {
    StartPlayerInboxAdminReconcilePlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendPlayerInboxAdminReconcilePlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const ::player_inbox::ReconcilePlayerInboxRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartPlayerInboxAdminReconcilePlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendPlayerInboxAdminReconcilePlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::player_inbox::ReconcilePlayerInboxRequest& derived = static_cast<const ::player_inbox::ReconcilePlayerInboxRequest&>(message);
    SendPlayerInboxAdminReconcilePlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetPlayerInboxAdminHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncPlayerInboxAdminGmSanctionPlayerHandler = handler;
    AsyncPlayerInboxAdminGetSummaryHandler = handler;
    AsyncPlayerInboxAdminListItemsHandler = handler;
    AsyncPlayerInboxAdminLookupByRequestHandler = handler;
    AsyncPlayerInboxAdminVoidItemHandler = handler;
    AsyncPlayerInboxAdminReconcilePlayerHandler = handler;
    (void)handler;
}

void SetPlayerInboxAdminIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
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
    (void)handler;
}

void SetPlayerInboxAdminFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncPlayerInboxAdminGmSanctionPlayerFailedHandler = handler;
    AsyncPlayerInboxAdminGetSummaryFailedHandler = handler;
    AsyncPlayerInboxAdminListItemsFailedHandler = handler;
    AsyncPlayerInboxAdminLookupByRequestFailedHandler = handler;
    AsyncPlayerInboxAdminVoidItemFailedHandler = handler;
    AsyncPlayerInboxAdminReconcilePlayerFailedHandler = handler;
    (void)handler;
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
    (void)handler;
}

void SetPlayerInboxAdminCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace player_inbox
