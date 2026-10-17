#include "muduo/base/Logging.h"

#include "jubaozhai_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetJubaozhaiCallDeadline 按目标节点类型写入
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

namespace trade {
#pragma region ClientPlayerJubaozhaiBrowseListings
AsyncClientPlayerJubaozhaiBrowseListingsHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseListingsHandler;
AsyncClientPlayerJubaozhaiBrowseListingsFailedHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiBrowseListingsCall {
    ::trade::BrowseListingsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiBrowseListingsHandler / FailedHandler。
    GrpcDone<::trade::BrowseListingsResponse> done;
};

void CompleteClientPlayerJubaozhaiBrowseListings(const ClientPlayerJubaozhaiBrowseListingsCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::BrowseListingsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiBrowseListingsHandler) {
            AsyncClientPlayerJubaozhaiBrowseListingsHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiBrowseListingsMessageId, "ClientPlayerJubaozhai.BrowseListings", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.BrowseListings failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiBrowseListingsFailure(std::shared_ptr<ClientPlayerJubaozhaiBrowseListingsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiBrowseListings(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiBrowseListingsCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::BrowseListingsResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiBrowseListingsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiBrowseListingsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiBrowseListingsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiBrowseListingsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiBrowseListings(*call, outcome, nullptr);
            return;
        }
        ::trade::BrowseListingsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.BrowseListings response";
            CompleteClientPlayerJubaozhaiBrowseListings(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiBrowseListings(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request,
                                            GrpcDone<::trade::BrowseListingsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiBrowseListingsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiBrowseListings(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseListingsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiBrowseListingsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiBrowseListings(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::BrowseListingsRequest& derived = static_cast<const ::trade::BrowseListingsRequest&>(message);
    SendClientPlayerJubaozhaiBrowseListings(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetListingDetail
AsyncClientPlayerJubaozhaiGetListingDetailHandlerFunctionType AsyncClientPlayerJubaozhaiGetListingDetailHandler;
AsyncClientPlayerJubaozhaiGetListingDetailFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiGetListingDetailCall {
    ::trade::GetListingDetailRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiGetListingDetailHandler / FailedHandler。
    GrpcDone<::trade::GetListingDetailResponse> done;
};

void CompleteClientPlayerJubaozhaiGetListingDetail(const ClientPlayerJubaozhaiGetListingDetailCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::GetListingDetailResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiGetListingDetailHandler) {
            AsyncClientPlayerJubaozhaiGetListingDetailHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiGetListingDetailMessageId, "ClientPlayerJubaozhai.GetListingDetail", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetListingDetail failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiGetListingDetailFailure(std::shared_ptr<ClientPlayerJubaozhaiGetListingDetailCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiGetListingDetail(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiGetListingDetailCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::GetListingDetailResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiGetListingDetailCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiGetListingDetailFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiGetListingDetailGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiGetListingDetailFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiGetListingDetail(*call, outcome, nullptr);
            return;
        }
        ::trade::GetListingDetailResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.GetListingDetail response";
            CompleteClientPlayerJubaozhaiGetListingDetail(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiGetListingDetail(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request,
                                            GrpcDone<::trade::GetListingDetailResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiGetListingDetailCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiGetListingDetail(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetListingDetailRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiGetListingDetailCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiGetListingDetail(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::GetListingDetailRequest& derived = static_cast<const ::trade::GetListingDetailRequest&>(message);
    SendClientPlayerJubaozhaiGetListingDetail(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiSetFavorite
AsyncClientPlayerJubaozhaiSetFavoriteHandlerFunctionType AsyncClientPlayerJubaozhaiSetFavoriteHandler;
AsyncClientPlayerJubaozhaiSetFavoriteFailedHandlerFunctionType AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiSetFavoriteCall {
    ::trade::SetFavoriteRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiSetFavoriteHandler / FailedHandler。
    GrpcDone<::trade::SetFavoriteResponse> done;
};

void CompleteClientPlayerJubaozhaiSetFavorite(const ClientPlayerJubaozhaiSetFavoriteCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::SetFavoriteResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiSetFavoriteHandler) {
            AsyncClientPlayerJubaozhaiSetFavoriteHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiSetFavoriteMessageId, "ClientPlayerJubaozhai.SetFavorite", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.SetFavorite failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiSetFavoriteFailure(std::shared_ptr<ClientPlayerJubaozhaiSetFavoriteCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiSetFavorite(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiSetFavoriteCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::SetFavoriteResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiSetFavoriteCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiSetFavoriteFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiSetFavoriteGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiSetFavoriteFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiSetFavorite(*call, outcome, nullptr);
            return;
        }
        ::trade::SetFavoriteResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.SetFavorite response";
            CompleteClientPlayerJubaozhaiSetFavorite(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiSetFavorite(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request,
                                            GrpcDone<::trade::SetFavoriteResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiSetFavoriteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiSetFavorite(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const ::trade::SetFavoriteRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiSetFavoriteCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiSetFavorite(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::SetFavoriteRequest& derived = static_cast<const ::trade::SetFavoriteRequest&>(message);
    SendClientPlayerJubaozhaiSetFavorite(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetMyShelf
AsyncClientPlayerJubaozhaiGetMyShelfHandlerFunctionType AsyncClientPlayerJubaozhaiGetMyShelfHandler;
AsyncClientPlayerJubaozhaiGetMyShelfFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiGetMyShelfCall {
    ::trade::GetMyShelfRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiGetMyShelfHandler / FailedHandler。
    GrpcDone<::trade::GetMyShelfResponse> done;
};

void CompleteClientPlayerJubaozhaiGetMyShelf(const ClientPlayerJubaozhaiGetMyShelfCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::GetMyShelfResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiGetMyShelfHandler) {
            AsyncClientPlayerJubaozhaiGetMyShelfHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiGetMyShelfMessageId, "ClientPlayerJubaozhai.GetMyShelf", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetMyShelf failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiGetMyShelfFailure(std::shared_ptr<ClientPlayerJubaozhaiGetMyShelfCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiGetMyShelf(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiGetMyShelfCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::GetMyShelfResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiGetMyShelfCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiGetMyShelfFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiGetMyShelfGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiGetMyShelfFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiGetMyShelf(*call, outcome, nullptr);
            return;
        }
        ::trade::GetMyShelfResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.GetMyShelf response";
            CompleteClientPlayerJubaozhaiGetMyShelf(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiGetMyShelf(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request,
                                            GrpcDone<::trade::GetMyShelfResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiGetMyShelfCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiGetMyShelf(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMyShelfRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiGetMyShelfCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiGetMyShelf(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::GetMyShelfRequest& derived = static_cast<const ::trade::GetMyShelfRequest&>(message);
    SendClientPlayerJubaozhaiGetMyShelf(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiListOfficialSkus
AsyncClientPlayerJubaozhaiListOfficialSkusHandlerFunctionType AsyncClientPlayerJubaozhaiListOfficialSkusHandler;
AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiListOfficialSkusCall {
    ::trade::ListOfficialSkusRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiListOfficialSkusHandler / FailedHandler。
    GrpcDone<::trade::ListOfficialSkusResponse> done;
};

void CompleteClientPlayerJubaozhaiListOfficialSkus(const ClientPlayerJubaozhaiListOfficialSkusCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::ListOfficialSkusResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiListOfficialSkusHandler) {
            AsyncClientPlayerJubaozhaiListOfficialSkusHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListOfficialSkus reply dropped: AsyncClientPlayerJubaozhaiListOfficialSkusHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiListOfficialSkusMessageId, "ClientPlayerJubaozhai.ListOfficialSkus", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListOfficialSkus failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiListOfficialSkusFailure(std::shared_ptr<ClientPlayerJubaozhaiListOfficialSkusCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiListOfficialSkus(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiListOfficialSkusCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::ListOfficialSkusResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiListOfficialSkusCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiListOfficialSkusFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiListOfficialSkusGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiListOfficialSkusFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiListOfficialSkus(*call, outcome, nullptr);
            return;
        }
        ::trade::ListOfficialSkusResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.ListOfficialSkus response";
            CompleteClientPlayerJubaozhaiListOfficialSkus(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiListOfficialSkus(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request,
                                            GrpcDone<::trade::ListOfficialSkusResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiListOfficialSkusCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiListOfficialSkus(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListOfficialSkusRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiListOfficialSkusCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiListOfficialSkus(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::ListOfficialSkusRequest& derived = static_cast<const ::trade::ListOfficialSkusRequest&>(message);
    SendClientPlayerJubaozhaiListOfficialSkus(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateOfficialOrder
AsyncClientPlayerJubaozhaiCreateOfficialOrderHandlerFunctionType AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler;
AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCreateOfficialOrderCall {
    ::trade::CreateOfficialOrderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiCreateOfficialOrder(const ClientPlayerJubaozhaiCreateOfficialOrderCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler) {
            AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreateOfficialOrder reply dropped: AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCreateOfficialOrderMessageId, "ClientPlayerJubaozhai.CreateOfficialOrder", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreateOfficialOrder failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCreateOfficialOrderFailure(std::shared_ptr<ClientPlayerJubaozhaiCreateOfficialOrderCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCreateOfficialOrder(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCreateOfficialOrderCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCreateOfficialOrderCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCreateOfficialOrderFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCreateOfficialOrderGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCreateOfficialOrderFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCreateOfficialOrder(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CreateOfficialOrder response";
            CompleteClientPlayerJubaozhaiCreateOfficialOrder(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCreateOfficialOrder(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCreateOfficialOrderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCreateOfficialOrder(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateOfficialOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCreateOfficialOrderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCreateOfficialOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CreateOfficialOrderRequest& derived = static_cast<const ::trade::CreateOfficialOrderRequest&>(message);
    SendClientPlayerJubaozhaiCreateOfficialOrder(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetCommerceOrder
AsyncClientPlayerJubaozhaiGetCommerceOrderHandlerFunctionType AsyncClientPlayerJubaozhaiGetCommerceOrderHandler;
AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiGetCommerceOrderCall {
    ::trade::GetCommerceOrderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiGetCommerceOrderHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiGetCommerceOrder(const ClientPlayerJubaozhaiGetCommerceOrderCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiGetCommerceOrderHandler) {
            AsyncClientPlayerJubaozhaiGetCommerceOrderHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetCommerceOrder reply dropped: AsyncClientPlayerJubaozhaiGetCommerceOrderHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiGetCommerceOrderMessageId, "ClientPlayerJubaozhai.GetCommerceOrder", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetCommerceOrder failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiGetCommerceOrderFailure(std::shared_ptr<ClientPlayerJubaozhaiGetCommerceOrderCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiGetCommerceOrder(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiGetCommerceOrderCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiGetCommerceOrderCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiGetCommerceOrderFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiGetCommerceOrderGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiGetCommerceOrderFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiGetCommerceOrder(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.GetCommerceOrder response";
            CompleteClientPlayerJubaozhaiGetCommerceOrder(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiGetCommerceOrder(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiGetCommerceOrderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiGetCommerceOrder(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetCommerceOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiGetCommerceOrderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiGetCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::GetCommerceOrderRequest& derived = static_cast<const ::trade::GetCommerceOrderRequest&>(message);
    SendClientPlayerJubaozhaiGetCommerceOrder(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelCommerceOrder
AsyncClientPlayerJubaozhaiCancelCommerceOrderHandlerFunctionType AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler;
AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCancelCommerceOrderCall {
    ::trade::CancelCommerceOrderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiCancelCommerceOrder(const ClientPlayerJubaozhaiCancelCommerceOrderCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler) {
            AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelCommerceOrder reply dropped: AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCancelCommerceOrderMessageId, "ClientPlayerJubaozhai.CancelCommerceOrder", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelCommerceOrder failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCancelCommerceOrderFailure(std::shared_ptr<ClientPlayerJubaozhaiCancelCommerceOrderCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCancelCommerceOrder(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCancelCommerceOrderCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCancelCommerceOrderCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCancelCommerceOrderFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCancelCommerceOrderGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCancelCommerceOrderFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCancelCommerceOrder(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CancelCommerceOrder response";
            CompleteClientPlayerJubaozhaiCancelCommerceOrder(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCancelCommerceOrder(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCancelCommerceOrderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCancelCommerceOrder(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelCommerceOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCancelCommerceOrderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCancelCommerceOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CancelCommerceOrderRequest& derived = static_cast<const ::trade::CancelCommerceOrderRequest&>(message);
    SendClientPlayerJubaozhaiCancelCommerceOrder(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiListCommerceOrders
AsyncClientPlayerJubaozhaiListCommerceOrdersHandlerFunctionType AsyncClientPlayerJubaozhaiListCommerceOrdersHandler;
AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiListCommerceOrdersCall {
    ::trade::ListCommerceOrdersRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiListCommerceOrdersHandler / FailedHandler。
    GrpcDone<::trade::ListCommerceOrdersResponse> done;
};

void CompleteClientPlayerJubaozhaiListCommerceOrders(const ClientPlayerJubaozhaiListCommerceOrdersCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::ListCommerceOrdersResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiListCommerceOrdersHandler) {
            AsyncClientPlayerJubaozhaiListCommerceOrdersHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListCommerceOrders reply dropped: AsyncClientPlayerJubaozhaiListCommerceOrdersHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiListCommerceOrdersMessageId, "ClientPlayerJubaozhai.ListCommerceOrders", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListCommerceOrders failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiListCommerceOrdersFailure(std::shared_ptr<ClientPlayerJubaozhaiListCommerceOrdersCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiListCommerceOrders(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiListCommerceOrdersCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::ListCommerceOrdersResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiListCommerceOrdersCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiListCommerceOrdersFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiListCommerceOrdersGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiListCommerceOrdersFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiListCommerceOrders(*call, outcome, nullptr);
            return;
        }
        ::trade::ListCommerceOrdersResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.ListCommerceOrders response";
            CompleteClientPlayerJubaozhaiListCommerceOrders(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiListCommerceOrders(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request,
                                            GrpcDone<::trade::ListCommerceOrdersResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiListCommerceOrdersCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiListCommerceOrders(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListCommerceOrdersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiListCommerceOrdersCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiListCommerceOrders(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::ListCommerceOrdersRequest& derived = static_cast<const ::trade::ListCommerceOrdersRequest&>(message);
    SendClientPlayerJubaozhaiListCommerceOrders(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreatePlayerTrade
AsyncClientPlayerJubaozhaiCreatePlayerTradeHandlerFunctionType AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler;
AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCreatePlayerTradeCall {
    ::trade::CreatePlayerTradeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiCreatePlayerTrade(const ClientPlayerJubaozhaiCreatePlayerTradeCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler) {
            AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreatePlayerTrade reply dropped: AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCreatePlayerTradeMessageId, "ClientPlayerJubaozhai.CreatePlayerTrade", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreatePlayerTrade failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCreatePlayerTradeFailure(std::shared_ptr<ClientPlayerJubaozhaiCreatePlayerTradeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCreatePlayerTrade(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCreatePlayerTradeCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCreatePlayerTradeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCreatePlayerTradeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCreatePlayerTradeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCreatePlayerTradeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCreatePlayerTrade(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CreatePlayerTrade response";
            CompleteClientPlayerJubaozhaiCreatePlayerTrade(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCreatePlayerTrade(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCreatePlayerTradeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCreatePlayerTrade(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreatePlayerTradeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCreatePlayerTradeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCreatePlayerTrade(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CreatePlayerTradeRequest& derived = static_cast<const ::trade::CreatePlayerTradeRequest&>(message);
    SendClientPlayerJubaozhaiCreatePlayerTrade(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiConfirmPlayerTrade
AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandlerFunctionType AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler;
AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandlerFunctionType AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiConfirmPlayerTradeCall {
    ::trade::ConfirmPlayerTradeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiConfirmPlayerTrade(const ClientPlayerJubaozhaiConfirmPlayerTradeCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler) {
            AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.ConfirmPlayerTrade reply dropped: AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiConfirmPlayerTradeMessageId, "ClientPlayerJubaozhai.ConfirmPlayerTrade", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.ConfirmPlayerTrade failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiConfirmPlayerTradeFailure(std::shared_ptr<ClientPlayerJubaozhaiConfirmPlayerTradeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiConfirmPlayerTrade(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiConfirmPlayerTradeCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiConfirmPlayerTradeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiConfirmPlayerTradeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiConfirmPlayerTradeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiConfirmPlayerTradeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiConfirmPlayerTrade(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.ConfirmPlayerTrade response";
            CompleteClientPlayerJubaozhaiConfirmPlayerTrade(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiConfirmPlayerTrade(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiConfirmPlayerTradeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiConfirmPlayerTrade(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const ::trade::ConfirmPlayerTradeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiConfirmPlayerTradeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiConfirmPlayerTrade(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::ConfirmPlayerTradeRequest& derived = static_cast<const ::trade::ConfirmPlayerTradeRequest&>(message);
    SendClientPlayerJubaozhaiConfirmPlayerTrade(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateConsignment
AsyncClientPlayerJubaozhaiCreateConsignmentHandlerFunctionType AsyncClientPlayerJubaozhaiCreateConsignmentHandler;
AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCreateConsignmentCall {
    ::trade::CreateConsignmentRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCreateConsignmentHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiCreateConsignment(const ClientPlayerJubaozhaiCreateConsignmentCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCreateConsignmentHandler) {
            AsyncClientPlayerJubaozhaiCreateConsignmentHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreateConsignment reply dropped: AsyncClientPlayerJubaozhaiCreateConsignmentHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCreateConsignmentMessageId, "ClientPlayerJubaozhai.CreateConsignment", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreateConsignment failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCreateConsignmentFailure(std::shared_ptr<ClientPlayerJubaozhaiCreateConsignmentCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCreateConsignment(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCreateConsignmentCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCreateConsignmentCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCreateConsignmentFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCreateConsignmentGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCreateConsignmentFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCreateConsignment(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CreateConsignment response";
            CompleteClientPlayerJubaozhaiCreateConsignment(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCreateConsignment(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCreateConsignmentCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCreateConsignment(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateConsignmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCreateConsignmentCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCreateConsignment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CreateConsignmentRequest& derived = static_cast<const ::trade::CreateConsignmentRequest&>(message);
    SendClientPlayerJubaozhaiCreateConsignment(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiBuyConsignment
AsyncClientPlayerJubaozhaiBuyConsignmentHandlerFunctionType AsyncClientPlayerJubaozhaiBuyConsignmentHandler;
AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandlerFunctionType AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiBuyConsignmentCall {
    ::trade::BuyConsignmentRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiBuyConsignmentHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiBuyConsignment(const ClientPlayerJubaozhaiBuyConsignmentCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiBuyConsignmentHandler) {
            AsyncClientPlayerJubaozhaiBuyConsignmentHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.BuyConsignment reply dropped: AsyncClientPlayerJubaozhaiBuyConsignmentHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiBuyConsignmentMessageId, "ClientPlayerJubaozhai.BuyConsignment", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.BuyConsignment failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiBuyConsignmentFailure(std::shared_ptr<ClientPlayerJubaozhaiBuyConsignmentCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiBuyConsignment(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiBuyConsignmentCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiBuyConsignmentCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiBuyConsignmentFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiBuyConsignmentGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiBuyConsignmentFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiBuyConsignment(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.BuyConsignment response";
            CompleteClientPlayerJubaozhaiBuyConsignment(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiBuyConsignment(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiBuyConsignmentCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiBuyConsignment(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::BuyConsignmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiBuyConsignmentCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiBuyConsignment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::BuyConsignmentRequest& derived = static_cast<const ::trade::BuyConsignmentRequest&>(message);
    SendClientPlayerJubaozhaiBuyConsignment(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelConsignment
AsyncClientPlayerJubaozhaiCancelConsignmentHandlerFunctionType AsyncClientPlayerJubaozhaiCancelConsignmentHandler;
AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCancelConsignmentCall {
    ::trade::CancelConsignmentRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCancelConsignmentHandler / FailedHandler。
    GrpcDone<::trade::CommerceOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiCancelConsignment(const ClientPlayerJubaozhaiCancelConsignmentCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::CommerceOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCancelConsignmentHandler) {
            AsyncClientPlayerJubaozhaiCancelConsignmentHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelConsignment reply dropped: AsyncClientPlayerJubaozhaiCancelConsignmentHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCancelConsignmentMessageId, "ClientPlayerJubaozhai.CancelConsignment", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelConsignment failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCancelConsignmentFailure(std::shared_ptr<ClientPlayerJubaozhaiCancelConsignmentCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCancelConsignment(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCancelConsignmentCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::CommerceOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCancelConsignmentCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCancelConsignmentFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCancelConsignmentGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCancelConsignmentFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCancelConsignment(*call, outcome, nullptr);
            return;
        }
        ::trade::CommerceOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CancelConsignment response";
            CompleteClientPlayerJubaozhaiCancelConsignment(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCancelConsignment(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request,
                                            GrpcDone<::trade::CommerceOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCancelConsignmentCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCancelConsignment(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelConsignmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCancelConsignmentCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCancelConsignment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CancelConsignmentRequest& derived = static_cast<const ::trade::CancelConsignmentRequest&>(message);
    SendClientPlayerJubaozhaiCancelConsignment(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiPlaceMarketOrder
AsyncClientPlayerJubaozhaiPlaceMarketOrderHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler;
AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiPlaceMarketOrderCall {
    ::trade::PlaceMarketOrderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler / FailedHandler。
    GrpcDone<::trade::MarketOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiPlaceMarketOrder(const ClientPlayerJubaozhaiPlaceMarketOrderCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::MarketOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler) {
            AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.PlaceMarketOrder reply dropped: AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiPlaceMarketOrderMessageId, "ClientPlayerJubaozhai.PlaceMarketOrder", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.PlaceMarketOrder failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiPlaceMarketOrderFailure(std::shared_ptr<ClientPlayerJubaozhaiPlaceMarketOrderCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiPlaceMarketOrder(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiPlaceMarketOrderCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::MarketOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiPlaceMarketOrderCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiPlaceMarketOrderFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiPlaceMarketOrderGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiPlaceMarketOrderFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiPlaceMarketOrder(*call, outcome, nullptr);
            return;
        }
        ::trade::MarketOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.PlaceMarketOrder response";
            CompleteClientPlayerJubaozhaiPlaceMarketOrder(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiPlaceMarketOrder(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request,
                                            GrpcDone<::trade::MarketOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiPlaceMarketOrderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiPlaceMarketOrder(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceMarketOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiPlaceMarketOrderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiPlaceMarketOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::PlaceMarketOrderRequest& derived = static_cast<const ::trade::PlaceMarketOrderRequest&>(message);
    SendClientPlayerJubaozhaiPlaceMarketOrder(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelMarketOrder
AsyncClientPlayerJubaozhaiCancelMarketOrderHandlerFunctionType AsyncClientPlayerJubaozhaiCancelMarketOrderHandler;
AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCancelMarketOrderCall {
    ::trade::CancelMarketOrderRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCancelMarketOrderHandler / FailedHandler。
    GrpcDone<::trade::MarketOrderResponse> done;
};

void CompleteClientPlayerJubaozhaiCancelMarketOrder(const ClientPlayerJubaozhaiCancelMarketOrderCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::MarketOrderResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCancelMarketOrderHandler) {
            AsyncClientPlayerJubaozhaiCancelMarketOrderHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelMarketOrder reply dropped: AsyncClientPlayerJubaozhaiCancelMarketOrderHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCancelMarketOrderMessageId, "ClientPlayerJubaozhai.CancelMarketOrder", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelMarketOrder failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCancelMarketOrderFailure(std::shared_ptr<ClientPlayerJubaozhaiCancelMarketOrderCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCancelMarketOrder(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCancelMarketOrderCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::MarketOrderResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCancelMarketOrderCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCancelMarketOrderFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCancelMarketOrderGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCancelMarketOrderFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCancelMarketOrder(*call, outcome, nullptr);
            return;
        }
        ::trade::MarketOrderResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CancelMarketOrder response";
            CompleteClientPlayerJubaozhaiCancelMarketOrder(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCancelMarketOrder(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request,
                                            GrpcDone<::trade::MarketOrderResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCancelMarketOrderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCancelMarketOrder(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelMarketOrderRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCancelMarketOrderCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCancelMarketOrder(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CancelMarketOrderRequest& derived = static_cast<const ::trade::CancelMarketOrderRequest&>(message);
    SendClientPlayerJubaozhaiCancelMarketOrder(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiListMarketOrders
AsyncClientPlayerJubaozhaiListMarketOrdersHandlerFunctionType AsyncClientPlayerJubaozhaiListMarketOrdersHandler;
AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiListMarketOrdersCall {
    ::trade::ListMarketOrdersRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiListMarketOrdersHandler / FailedHandler。
    GrpcDone<::trade::ListMarketOrdersResponse> done;
};

void CompleteClientPlayerJubaozhaiListMarketOrders(const ClientPlayerJubaozhaiListMarketOrdersCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::ListMarketOrdersResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiListMarketOrdersHandler) {
            AsyncClientPlayerJubaozhaiListMarketOrdersHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListMarketOrders reply dropped: AsyncClientPlayerJubaozhaiListMarketOrdersHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiListMarketOrdersMessageId, "ClientPlayerJubaozhai.ListMarketOrders", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListMarketOrders failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiListMarketOrdersFailure(std::shared_ptr<ClientPlayerJubaozhaiListMarketOrdersCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiListMarketOrders(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiListMarketOrdersCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::ListMarketOrdersResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiListMarketOrdersCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiListMarketOrdersFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiListMarketOrdersGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiListMarketOrdersFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiListMarketOrders(*call, outcome, nullptr);
            return;
        }
        ::trade::ListMarketOrdersResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.ListMarketOrders response";
            CompleteClientPlayerJubaozhaiListMarketOrders(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiListMarketOrders(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request,
                                            GrpcDone<::trade::ListMarketOrdersResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiListMarketOrdersCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiListMarketOrders(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListMarketOrdersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiListMarketOrdersCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiListMarketOrders(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::ListMarketOrdersRequest& derived = static_cast<const ::trade::ListMarketOrdersRequest&>(message);
    SendClientPlayerJubaozhaiListMarketOrders(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetMarketDepth
AsyncClientPlayerJubaozhaiGetMarketDepthHandlerFunctionType AsyncClientPlayerJubaozhaiGetMarketDepthHandler;
AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiGetMarketDepthCall {
    ::trade::GetMarketDepthRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiGetMarketDepthHandler / FailedHandler。
    GrpcDone<::trade::GetMarketDepthResponse> done;
};

void CompleteClientPlayerJubaozhaiGetMarketDepth(const ClientPlayerJubaozhaiGetMarketDepthCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::GetMarketDepthResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiGetMarketDepthHandler) {
            AsyncClientPlayerJubaozhaiGetMarketDepthHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetMarketDepth reply dropped: AsyncClientPlayerJubaozhaiGetMarketDepthHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiGetMarketDepthMessageId, "ClientPlayerJubaozhai.GetMarketDepth", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetMarketDepth failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiGetMarketDepthFailure(std::shared_ptr<ClientPlayerJubaozhaiGetMarketDepthCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiGetMarketDepth(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiGetMarketDepthCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::GetMarketDepthResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiGetMarketDepthCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiGetMarketDepthFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiGetMarketDepthGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiGetMarketDepthFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiGetMarketDepth(*call, outcome, nullptr);
            return;
        }
        ::trade::GetMarketDepthResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.GetMarketDepth response";
            CompleteClientPlayerJubaozhaiGetMarketDepth(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiGetMarketDepth(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request,
                                            GrpcDone<::trade::GetMarketDepthResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiGetMarketDepthCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiGetMarketDepth(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetMarketDepthRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiGetMarketDepthCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiGetMarketDepth(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::GetMarketDepthRequest& derived = static_cast<const ::trade::GetMarketDepthRequest&>(message);
    SendClientPlayerJubaozhaiGetMarketDepth(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCreateAuctionLot
AsyncClientPlayerJubaozhaiCreateAuctionLotHandlerFunctionType AsyncClientPlayerJubaozhaiCreateAuctionLotHandler;
AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCreateAuctionLotCall {
    ::trade::CreateAuctionLotRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCreateAuctionLotHandler / FailedHandler。
    GrpcDone<::trade::AuctionLotResponse> done;
};

void CompleteClientPlayerJubaozhaiCreateAuctionLot(const ClientPlayerJubaozhaiCreateAuctionLotCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::AuctionLotResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCreateAuctionLotHandler) {
            AsyncClientPlayerJubaozhaiCreateAuctionLotHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreateAuctionLot reply dropped: AsyncClientPlayerJubaozhaiCreateAuctionLotHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCreateAuctionLotMessageId, "ClientPlayerJubaozhai.CreateAuctionLot", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CreateAuctionLot failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCreateAuctionLotFailure(std::shared_ptr<ClientPlayerJubaozhaiCreateAuctionLotCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCreateAuctionLot(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCreateAuctionLotCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::AuctionLotResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCreateAuctionLotCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCreateAuctionLotFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCreateAuctionLotGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCreateAuctionLotFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCreateAuctionLot(*call, outcome, nullptr);
            return;
        }
        ::trade::AuctionLotResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CreateAuctionLot response";
            CompleteClientPlayerJubaozhaiCreateAuctionLot(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCreateAuctionLot(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request,
                                            GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCreateAuctionLotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCreateAuctionLot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CreateAuctionLotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCreateAuctionLotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCreateAuctionLot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CreateAuctionLotRequest& derived = static_cast<const ::trade::CreateAuctionLotRequest&>(message);
    SendClientPlayerJubaozhaiCreateAuctionLot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiPlaceAuctionBid
AsyncClientPlayerJubaozhaiPlaceAuctionBidHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler;
AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandlerFunctionType AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiPlaceAuctionBidCall {
    ::trade::PlaceAuctionBidRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler / FailedHandler。
    GrpcDone<::trade::AuctionLotResponse> done;
};

void CompleteClientPlayerJubaozhaiPlaceAuctionBid(const ClientPlayerJubaozhaiPlaceAuctionBidCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::AuctionLotResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler) {
            AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.PlaceAuctionBid reply dropped: AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiPlaceAuctionBidMessageId, "ClientPlayerJubaozhai.PlaceAuctionBid", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.PlaceAuctionBid failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiPlaceAuctionBidFailure(std::shared_ptr<ClientPlayerJubaozhaiPlaceAuctionBidCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiPlaceAuctionBid(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiPlaceAuctionBidCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::AuctionLotResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiPlaceAuctionBidCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiPlaceAuctionBidFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiPlaceAuctionBidGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiPlaceAuctionBidFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiPlaceAuctionBid(*call, outcome, nullptr);
            return;
        }
        ::trade::AuctionLotResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.PlaceAuctionBid response";
            CompleteClientPlayerJubaozhaiPlaceAuctionBid(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiPlaceAuctionBid(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request,
                                            GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiPlaceAuctionBidCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiPlaceAuctionBid(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const ::trade::PlaceAuctionBidRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiPlaceAuctionBidCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiPlaceAuctionBid(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::PlaceAuctionBidRequest& derived = static_cast<const ::trade::PlaceAuctionBidRequest&>(message);
    SendClientPlayerJubaozhaiPlaceAuctionBid(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiGetAuctionLot
AsyncClientPlayerJubaozhaiGetAuctionLotHandlerFunctionType AsyncClientPlayerJubaozhaiGetAuctionLotHandler;
AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandlerFunctionType AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiGetAuctionLotCall {
    ::trade::GetAuctionLotRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiGetAuctionLotHandler / FailedHandler。
    GrpcDone<::trade::AuctionLotResponse> done;
};

void CompleteClientPlayerJubaozhaiGetAuctionLot(const ClientPlayerJubaozhaiGetAuctionLotCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::AuctionLotResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiGetAuctionLotHandler) {
            AsyncClientPlayerJubaozhaiGetAuctionLotHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetAuctionLot reply dropped: AsyncClientPlayerJubaozhaiGetAuctionLotHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiGetAuctionLotMessageId, "ClientPlayerJubaozhai.GetAuctionLot", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.GetAuctionLot failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiGetAuctionLotFailure(std::shared_ptr<ClientPlayerJubaozhaiGetAuctionLotCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiGetAuctionLot(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiGetAuctionLotCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::AuctionLotResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiGetAuctionLotCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiGetAuctionLotFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiGetAuctionLotGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiGetAuctionLotFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiGetAuctionLot(*call, outcome, nullptr);
            return;
        }
        ::trade::AuctionLotResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.GetAuctionLot response";
            CompleteClientPlayerJubaozhaiGetAuctionLot(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiGetAuctionLot(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request,
                                            GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiGetAuctionLotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiGetAuctionLot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::GetAuctionLotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiGetAuctionLotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiGetAuctionLot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::GetAuctionLotRequest& derived = static_cast<const ::trade::GetAuctionLotRequest&>(message);
    SendClientPlayerJubaozhaiGetAuctionLot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiCancelAuctionLot
AsyncClientPlayerJubaozhaiCancelAuctionLotHandlerFunctionType AsyncClientPlayerJubaozhaiCancelAuctionLotHandler;
AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandlerFunctionType AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiCancelAuctionLotCall {
    ::trade::CancelAuctionLotRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiCancelAuctionLotHandler / FailedHandler。
    GrpcDone<::trade::AuctionLotResponse> done;
};

void CompleteClientPlayerJubaozhaiCancelAuctionLot(const ClientPlayerJubaozhaiCancelAuctionLotCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::AuctionLotResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiCancelAuctionLotHandler) {
            AsyncClientPlayerJubaozhaiCancelAuctionLotHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelAuctionLot reply dropped: AsyncClientPlayerJubaozhaiCancelAuctionLotHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiCancelAuctionLotMessageId, "ClientPlayerJubaozhai.CancelAuctionLot", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.CancelAuctionLot failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiCancelAuctionLotFailure(std::shared_ptr<ClientPlayerJubaozhaiCancelAuctionLotCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiCancelAuctionLot(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiCancelAuctionLotCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::AuctionLotResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiCancelAuctionLotCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiCancelAuctionLotFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiCancelAuctionLotGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiCancelAuctionLotFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiCancelAuctionLot(*call, outcome, nullptr);
            return;
        }
        ::trade::AuctionLotResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.CancelAuctionLot response";
            CompleteClientPlayerJubaozhaiCancelAuctionLot(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiCancelAuctionLot(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request,
                                            GrpcDone<::trade::AuctionLotResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiCancelAuctionLotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiCancelAuctionLot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const ::trade::CancelAuctionLotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiCancelAuctionLotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiCancelAuctionLot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::CancelAuctionLotRequest& derived = static_cast<const ::trade::CancelAuctionLotRequest&>(message);
    SendClientPlayerJubaozhaiCancelAuctionLot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiBrowseCommerceOffers
AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler;
AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandlerFunctionType AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiBrowseCommerceOffersCall {
    ::trade::BrowseCommerceOffersRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler / FailedHandler。
    GrpcDone<::trade::BrowseCommerceOffersResponse> done;
};

void CompleteClientPlayerJubaozhaiBrowseCommerceOffers(const ClientPlayerJubaozhaiBrowseCommerceOffersCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::BrowseCommerceOffersResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler) {
            AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.BrowseCommerceOffers reply dropped: AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiBrowseCommerceOffersMessageId, "ClientPlayerJubaozhai.BrowseCommerceOffers", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.BrowseCommerceOffers failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiBrowseCommerceOffersFailure(std::shared_ptr<ClientPlayerJubaozhaiBrowseCommerceOffersCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiBrowseCommerceOffers(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiBrowseCommerceOffersCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::BrowseCommerceOffersResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiBrowseCommerceOffersCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiBrowseCommerceOffersFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiBrowseCommerceOffersGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiBrowseCommerceOffersFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiBrowseCommerceOffers(*call, outcome, nullptr);
            return;
        }
        ::trade::BrowseCommerceOffersResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.BrowseCommerceOffers response";
            CompleteClientPlayerJubaozhaiBrowseCommerceOffers(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiBrowseCommerceOffers(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request,
                                            GrpcDone<::trade::BrowseCommerceOffersResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiBrowseCommerceOffersCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiBrowseCommerceOffers(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const ::trade::BrowseCommerceOffersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiBrowseCommerceOffersCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiBrowseCommerceOffers(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::BrowseCommerceOffersRequest& derived = static_cast<const ::trade::BrowseCommerceOffersRequest&>(message);
    SendClientPlayerJubaozhaiBrowseCommerceOffers(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region ClientPlayerJubaozhaiListTradeMarkets
AsyncClientPlayerJubaozhaiListTradeMarketsHandlerFunctionType AsyncClientPlayerJubaozhaiListTradeMarketsHandler;
AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandlerFunctionType AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct ClientPlayerJubaozhaiListTradeMarketsCall {
    ::trade::ListTradeMarketsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncClientPlayerJubaozhaiListTradeMarketsHandler / FailedHandler。
    GrpcDone<::trade::ListTradeMarketsResponse> done;
};

void CompleteClientPlayerJubaozhaiListTradeMarkets(const ClientPlayerJubaozhaiListTradeMarketsCall& call, const GrpcCallOutcome& outcome,
                                                const ::trade::ListTradeMarketsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncClientPlayerJubaozhaiListTradeMarketsHandler) {
            AsyncClientPlayerJubaozhaiListTradeMarketsHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListTradeMarkets reply dropped: AsyncClientPlayerJubaozhaiListTradeMarketsHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandler) {
        const GrpcCallFailure failure{ ClientPlayerJubaozhaiListTradeMarketsMessageId, "ClientPlayerJubaozhai.ListTradeMarkets", outcome, call.sentMetadata};
        AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC ClientPlayerJubaozhai.ListTradeMarkets failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalClientPlayerJubaozhaiListTradeMarketsFailure(std::shared_ptr<ClientPlayerJubaozhaiListTradeMarketsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteClientPlayerJubaozhaiListTradeMarkets(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartClientPlayerJubaozhaiListTradeMarketsCall(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::trade::ListTradeMarketsResponse> done) {
    auto call = std::make_shared<ClientPlayerJubaozhaiListTradeMarketsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerJubaozhaiListTradeMarketsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerJubaozhaiListTradeMarketsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerJubaozhaiListTradeMarketsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteClientPlayerJubaozhaiListTradeMarkets(*call, outcome, nullptr);
            return;
        }
        ::trade::ListTradeMarketsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerJubaozhai.ListTradeMarkets response";
            CompleteClientPlayerJubaozhaiListTradeMarkets(*call, outcome, nullptr);
            return;
        }
        CompleteClientPlayerJubaozhaiListTradeMarkets(*call, outcome, &reply);
    });
}

void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request,
                                            GrpcDone<::trade::ListTradeMarketsResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerJubaozhaiListTradeMarketsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendClientPlayerJubaozhaiListTradeMarkets(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const ::trade::ListTradeMarketsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartClientPlayerJubaozhaiListTradeMarketsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendClientPlayerJubaozhaiListTradeMarkets(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::trade::ListTradeMarketsRequest& derived = static_cast<const ::trade::ListTradeMarketsRequest&>(message);
    SendClientPlayerJubaozhaiListTradeMarkets(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetJubaozhaiHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerJubaozhaiBrowseListingsHandler = handler;
    AsyncClientPlayerJubaozhaiGetListingDetailHandler = handler;
    AsyncClientPlayerJubaozhaiSetFavoriteHandler = handler;
    AsyncClientPlayerJubaozhaiGetMyShelfHandler = handler;
    AsyncClientPlayerJubaozhaiListOfficialSkusHandler = handler;
    AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler = handler;
    AsyncClientPlayerJubaozhaiGetCommerceOrderHandler = handler;
    AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler = handler;
    AsyncClientPlayerJubaozhaiListCommerceOrdersHandler = handler;
    AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler = handler;
    AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler = handler;
    AsyncClientPlayerJubaozhaiCreateConsignmentHandler = handler;
    AsyncClientPlayerJubaozhaiBuyConsignmentHandler = handler;
    AsyncClientPlayerJubaozhaiCancelConsignmentHandler = handler;
    AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler = handler;
    AsyncClientPlayerJubaozhaiCancelMarketOrderHandler = handler;
    AsyncClientPlayerJubaozhaiListMarketOrdersHandler = handler;
    AsyncClientPlayerJubaozhaiGetMarketDepthHandler = handler;
    AsyncClientPlayerJubaozhaiCreateAuctionLotHandler = handler;
    AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler = handler;
    AsyncClientPlayerJubaozhaiGetAuctionLotHandler = handler;
    AsyncClientPlayerJubaozhaiCancelAuctionLotHandler = handler;
    AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler = handler;
    AsyncClientPlayerJubaozhaiListTradeMarketsHandler = handler;
    (void)handler;
}

void SetJubaozhaiIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
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
    if (!AsyncClientPlayerJubaozhaiListOfficialSkusHandler) {
        AsyncClientPlayerJubaozhaiListOfficialSkusHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler) {
        AsyncClientPlayerJubaozhaiCreateOfficialOrderHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetCommerceOrderHandler) {
        AsyncClientPlayerJubaozhaiGetCommerceOrderHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler) {
        AsyncClientPlayerJubaozhaiCancelCommerceOrderHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiListCommerceOrdersHandler) {
        AsyncClientPlayerJubaozhaiListCommerceOrdersHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler) {
        AsyncClientPlayerJubaozhaiCreatePlayerTradeHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler) {
        AsyncClientPlayerJubaozhaiConfirmPlayerTradeHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreateConsignmentHandler) {
        AsyncClientPlayerJubaozhaiCreateConsignmentHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiBuyConsignmentHandler) {
        AsyncClientPlayerJubaozhaiBuyConsignmentHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelConsignmentHandler) {
        AsyncClientPlayerJubaozhaiCancelConsignmentHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler) {
        AsyncClientPlayerJubaozhaiPlaceMarketOrderHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelMarketOrderHandler) {
        AsyncClientPlayerJubaozhaiCancelMarketOrderHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiListMarketOrdersHandler) {
        AsyncClientPlayerJubaozhaiListMarketOrdersHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetMarketDepthHandler) {
        AsyncClientPlayerJubaozhaiGetMarketDepthHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreateAuctionLotHandler) {
        AsyncClientPlayerJubaozhaiCreateAuctionLotHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler) {
        AsyncClientPlayerJubaozhaiPlaceAuctionBidHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetAuctionLotHandler) {
        AsyncClientPlayerJubaozhaiGetAuctionLotHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelAuctionLotHandler) {
        AsyncClientPlayerJubaozhaiCancelAuctionLotHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler) {
        AsyncClientPlayerJubaozhaiBrowseCommerceOffersHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiListTradeMarketsHandler) {
        AsyncClientPlayerJubaozhaiListTradeMarketsHandler = handler;
    }
    (void)handler;
}

void SetJubaozhaiFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler = handler;
    AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler = handler;
    AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandler = handler;
    AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandler = handler;
    AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandler = handler;
    AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandler = handler;
    AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandler = handler;
    AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandler = handler;
    AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandler = handler;
    AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandler = handler;
    AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandler = handler;
    AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandler = handler;
    (void)handler;
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
    if (!AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandler) {
        AsyncClientPlayerJubaozhaiListOfficialSkusFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandler) {
        AsyncClientPlayerJubaozhaiCreateOfficialOrderFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandler) {
        AsyncClientPlayerJubaozhaiGetCommerceOrderFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandler) {
        AsyncClientPlayerJubaozhaiCancelCommerceOrderFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandler) {
        AsyncClientPlayerJubaozhaiListCommerceOrdersFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandler) {
        AsyncClientPlayerJubaozhaiCreatePlayerTradeFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandler) {
        AsyncClientPlayerJubaozhaiConfirmPlayerTradeFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandler) {
        AsyncClientPlayerJubaozhaiCreateConsignmentFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandler) {
        AsyncClientPlayerJubaozhaiBuyConsignmentFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandler) {
        AsyncClientPlayerJubaozhaiCancelConsignmentFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandler) {
        AsyncClientPlayerJubaozhaiPlaceMarketOrderFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandler) {
        AsyncClientPlayerJubaozhaiCancelMarketOrderFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandler) {
        AsyncClientPlayerJubaozhaiListMarketOrdersFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandler) {
        AsyncClientPlayerJubaozhaiGetMarketDepthFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandler) {
        AsyncClientPlayerJubaozhaiCreateAuctionLotFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandler) {
        AsyncClientPlayerJubaozhaiPlaceAuctionBidFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandler) {
        AsyncClientPlayerJubaozhaiGetAuctionLotFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandler) {
        AsyncClientPlayerJubaozhaiCancelAuctionLotFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandler) {
        AsyncClientPlayerJubaozhaiBrowseCommerceOffersFailedHandler = handler;
    }
    if (!AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandler) {
        AsyncClientPlayerJubaozhaiListTradeMarketsFailedHandler = handler;
    }
    (void)handler;
}

void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace trade
