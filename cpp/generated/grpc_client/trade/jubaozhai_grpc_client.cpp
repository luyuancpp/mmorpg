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

void SetJubaozhaiHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
    AsyncClientPlayerJubaozhaiBrowseListingsHandler = handler;
    AsyncClientPlayerJubaozhaiGetListingDetailHandler = handler;
    AsyncClientPlayerJubaozhaiSetFavoriteHandler = handler;
    AsyncClientPlayerJubaozhaiGetMyShelfHandler = handler;
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
    (void)handler;
}

void SetJubaozhaiFailedHandler(const std::function<void(const GrpcCallFailure&, const ::google::protobuf::Message& request)>& handler) {
    AsyncClientPlayerJubaozhaiBrowseListingsFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetListingDetailFailedHandler = handler;
    AsyncClientPlayerJubaozhaiSetFavoriteFailedHandler = handler;
    AsyncClientPlayerJubaozhaiGetMyShelfFailedHandler = handler;
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
    (void)handler;
}

void SetJubaozhaiCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace trade
