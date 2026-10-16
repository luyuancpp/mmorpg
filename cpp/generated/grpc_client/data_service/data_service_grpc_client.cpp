#include "muduo/base/Logging.h"

#include "data_service_grpc_client.h"
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
// 本文件所有 unary 调用的 deadline(毫秒)。启动时 SetDataServiceCallDeadline 按目标节点类型写入
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

namespace data_service {
#pragma region DataServiceLoadPlayerData
AsyncDataServiceLoadPlayerDataHandlerFunctionType AsyncDataServiceLoadPlayerDataHandler;
AsyncDataServiceLoadPlayerDataFailedHandlerFunctionType AsyncDataServiceLoadPlayerDataFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceLoadPlayerDataCall {
    ::data_service::LoadPlayerDataRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceLoadPlayerDataHandler / FailedHandler。
    GrpcDone<::data_service::LoadPlayerDataResponse> done;
};

void CompleteDataServiceLoadPlayerData(const DataServiceLoadPlayerDataCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::LoadPlayerDataResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceLoadPlayerDataHandler) {
            AsyncDataServiceLoadPlayerDataHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceLoadPlayerDataMessageId, "DataService.LoadPlayerData", outcome, call.sentMetadata};
        AsyncDataServiceLoadPlayerDataFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.LoadPlayerData failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceLoadPlayerDataFailure(std::shared_ptr<DataServiceLoadPlayerDataCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceLoadPlayerData(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceLoadPlayerDataCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::LoadPlayerDataResponse> done) {
    auto call = std::make_shared<DataServiceLoadPlayerDataCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceLoadPlayerDataFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceLoadPlayerDataGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceLoadPlayerDataFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceLoadPlayerData(*call, outcome, nullptr);
            return;
        }
        ::data_service::LoadPlayerDataResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.LoadPlayerData response";
            CompleteDataServiceLoadPlayerData(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceLoadPlayerData(*call, outcome, &reply);
    });
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request,
                                            GrpcDone<::data_service::LoadPlayerDataResponse> done, const GrpcCallOptions& options) {
    StartDataServiceLoadPlayerDataCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceLoadPlayerData(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::LoadPlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceLoadPlayerDataCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceLoadPlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::LoadPlayerDataRequest& derived = static_cast<const ::data_service::LoadPlayerDataRequest&>(message);
    SendDataServiceLoadPlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceSavePlayerData
AsyncDataServiceSavePlayerDataHandlerFunctionType AsyncDataServiceSavePlayerDataHandler;
AsyncDataServiceSavePlayerDataFailedHandlerFunctionType AsyncDataServiceSavePlayerDataFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceSavePlayerDataCall {
    ::data_service::SavePlayerDataRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceSavePlayerDataHandler / FailedHandler。
    GrpcDone<::data_service::SavePlayerDataResponse> done;
};

void CompleteDataServiceSavePlayerData(const DataServiceSavePlayerDataCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::SavePlayerDataResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceSavePlayerDataHandler) {
            AsyncDataServiceSavePlayerDataHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceSavePlayerDataMessageId, "DataService.SavePlayerData", outcome, call.sentMetadata};
        AsyncDataServiceSavePlayerDataFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.SavePlayerData failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceSavePlayerDataFailure(std::shared_ptr<DataServiceSavePlayerDataCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceSavePlayerData(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceSavePlayerDataCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::SavePlayerDataResponse> done) {
    auto call = std::make_shared<DataServiceSavePlayerDataCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceSavePlayerDataFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceSavePlayerDataGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceSavePlayerDataFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceSavePlayerData(*call, outcome, nullptr);
            return;
        }
        ::data_service::SavePlayerDataResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.SavePlayerData response";
            CompleteDataServiceSavePlayerData(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceSavePlayerData(*call, outcome, &reply);
    });
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request,
                                            GrpcDone<::data_service::SavePlayerDataResponse> done, const GrpcCallOptions& options) {
    StartDataServiceSavePlayerDataCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceSavePlayerData(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SavePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceSavePlayerDataCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceSavePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::SavePlayerDataRequest& derived = static_cast<const ::data_service::SavePlayerDataRequest&>(message);
    SendDataServiceSavePlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerField
AsyncDataServiceGetPlayerFieldHandlerFunctionType AsyncDataServiceGetPlayerFieldHandler;
AsyncDataServiceGetPlayerFieldFailedHandlerFunctionType AsyncDataServiceGetPlayerFieldFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceGetPlayerFieldCall {
    ::data_service::GetPlayerFieldRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerFieldHandler / FailedHandler。
    GrpcDone<::data_service::GetPlayerFieldResponse> done;
};

void CompleteDataServiceGetPlayerField(const DataServiceGetPlayerFieldCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::GetPlayerFieldResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceGetPlayerFieldHandler) {
            AsyncDataServiceGetPlayerFieldHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceGetPlayerFieldMessageId, "DataService.GetPlayerField", outcome, call.sentMetadata};
        AsyncDataServiceGetPlayerFieldFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerField failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceGetPlayerFieldFailure(std::shared_ptr<DataServiceGetPlayerFieldCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceGetPlayerField(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerFieldCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerFieldResponse> done) {
    auto call = std::make_shared<DataServiceGetPlayerFieldCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceGetPlayerFieldFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceGetPlayerFieldGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceGetPlayerFieldFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceGetPlayerField(*call, outcome, nullptr);
            return;
        }
        ::data_service::GetPlayerFieldResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.GetPlayerField response";
            CompleteDataServiceGetPlayerField(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceGetPlayerField(*call, outcome, &reply);
    });
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request,
                                            GrpcDone<::data_service::GetPlayerFieldResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerFieldCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerField(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerFieldCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceGetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::GetPlayerFieldRequest& derived = static_cast<const ::data_service::GetPlayerFieldRequest&>(message);
    SendDataServiceGetPlayerField(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceSetPlayerField
AsyncDataServiceSetPlayerFieldHandlerFunctionType AsyncDataServiceSetPlayerFieldHandler;
AsyncDataServiceSetPlayerFieldFailedHandlerFunctionType AsyncDataServiceSetPlayerFieldFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceSetPlayerFieldCall {
    ::data_service::SetPlayerFieldRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceSetPlayerFieldHandler / FailedHandler。
    GrpcDone<::data_service::SetPlayerFieldResponse> done;
};

void CompleteDataServiceSetPlayerField(const DataServiceSetPlayerFieldCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::SetPlayerFieldResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceSetPlayerFieldHandler) {
            AsyncDataServiceSetPlayerFieldHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceSetPlayerFieldMessageId, "DataService.SetPlayerField", outcome, call.sentMetadata};
        AsyncDataServiceSetPlayerFieldFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.SetPlayerField failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceSetPlayerFieldFailure(std::shared_ptr<DataServiceSetPlayerFieldCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceSetPlayerField(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceSetPlayerFieldCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::SetPlayerFieldResponse> done) {
    auto call = std::make_shared<DataServiceSetPlayerFieldCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceSetPlayerFieldFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceSetPlayerFieldGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceSetPlayerFieldFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceSetPlayerField(*call, outcome, nullptr);
            return;
        }
        ::data_service::SetPlayerFieldResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.SetPlayerField response";
            CompleteDataServiceSetPlayerField(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceSetPlayerField(*call, outcome, &reply);
    });
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request,
                                            GrpcDone<::data_service::SetPlayerFieldResponse> done, const GrpcCallOptions& options) {
    StartDataServiceSetPlayerFieldCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceSetPlayerField(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const ::data_service::SetPlayerFieldRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceSetPlayerFieldCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceSetPlayerField(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::SetPlayerFieldRequest& derived = static_cast<const ::data_service::SetPlayerFieldRequest&>(message);
    SendDataServiceSetPlayerField(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRegisterPlayerZone
AsyncDataServiceRegisterPlayerZoneHandlerFunctionType AsyncDataServiceRegisterPlayerZoneHandler;
AsyncDataServiceRegisterPlayerZoneFailedHandlerFunctionType AsyncDataServiceRegisterPlayerZoneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceRegisterPlayerZoneCall {
    ::data_service::RegisterPlayerZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRegisterPlayerZoneHandler / FailedHandler。
    GrpcDone<::google::protobuf::Empty> done;
};

void CompleteDataServiceRegisterPlayerZone(const DataServiceRegisterPlayerZoneCall& call, const GrpcCallOutcome& outcome,
                                                const ::google::protobuf::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceRegisterPlayerZoneHandler) {
            AsyncDataServiceRegisterPlayerZoneHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceRegisterPlayerZoneMessageId, "DataService.RegisterPlayerZone", outcome, call.sentMetadata};
        AsyncDataServiceRegisterPlayerZoneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.RegisterPlayerZone failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceRegisterPlayerZoneFailure(std::shared_ptr<DataServiceRegisterPlayerZoneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceRegisterPlayerZone(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRegisterPlayerZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::google::protobuf::Empty> done) {
    auto call = std::make_shared<DataServiceRegisterPlayerZoneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceRegisterPlayerZoneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceRegisterPlayerZoneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceRegisterPlayerZoneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceRegisterPlayerZone(*call, outcome, nullptr);
            return;
        }
        ::google::protobuf::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.RegisterPlayerZone response";
            CompleteDataServiceRegisterPlayerZone(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceRegisterPlayerZone(*call, outcome, &reply);
    });
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request,
                                            GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options) {
    StartDataServiceRegisterPlayerZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRegisterPlayerZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RegisterPlayerZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRegisterPlayerZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceRegisterPlayerZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::RegisterPlayerZoneRequest& derived = static_cast<const ::data_service::RegisterPlayerZoneRequest&>(message);
    SendDataServiceRegisterPlayerZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerHomeZone
AsyncDataServiceGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneHandler;
AsyncDataServiceGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceGetPlayerHomeZoneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceGetPlayerHomeZoneCall {
    ::data_service::GetPlayerHomeZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerHomeZoneHandler / FailedHandler。
    GrpcDone<::data_service::GetPlayerHomeZoneResponse> done;
};

void CompleteDataServiceGetPlayerHomeZone(const DataServiceGetPlayerHomeZoneCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::GetPlayerHomeZoneResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceGetPlayerHomeZoneHandler) {
            AsyncDataServiceGetPlayerHomeZoneHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceGetPlayerHomeZoneMessageId, "DataService.GetPlayerHomeZone", outcome, call.sentMetadata};
        AsyncDataServiceGetPlayerHomeZoneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerHomeZone failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceGetPlayerHomeZoneFailure(std::shared_ptr<DataServiceGetPlayerHomeZoneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceGetPlayerHomeZone(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerHomeZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerHomeZoneResponse> done) {
    auto call = std::make_shared<DataServiceGetPlayerHomeZoneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceGetPlayerHomeZoneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceGetPlayerHomeZoneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceGetPlayerHomeZoneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceGetPlayerHomeZone(*call, outcome, nullptr);
            return;
        }
        ::data_service::GetPlayerHomeZoneResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.GetPlayerHomeZone response";
            CompleteDataServiceGetPlayerHomeZone(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceGetPlayerHomeZone(*call, outcome, &reply);
    });
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request,
                                            GrpcDone<::data_service::GetPlayerHomeZoneResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerHomeZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerHomeZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerHomeZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::GetPlayerHomeZoneRequest& derived = static_cast<const ::data_service::GetPlayerHomeZoneRequest&>(message);
    SendDataServiceGetPlayerHomeZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceBatchGetPlayerHomeZone
AsyncDataServiceBatchGetPlayerHomeZoneHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneHandler;
AsyncDataServiceBatchGetPlayerHomeZoneFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceBatchGetPlayerHomeZoneCall {
    ::data_service::BatchGetPlayerHomeZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceBatchGetPlayerHomeZoneHandler / FailedHandler。
    GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done;
};

void CompleteDataServiceBatchGetPlayerHomeZone(const DataServiceBatchGetPlayerHomeZoneCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::BatchGetPlayerHomeZoneResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceBatchGetPlayerHomeZoneHandler) {
            AsyncDataServiceBatchGetPlayerHomeZoneHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceBatchGetPlayerHomeZoneMessageId, "DataService.BatchGetPlayerHomeZone", outcome, call.sentMetadata};
        AsyncDataServiceBatchGetPlayerHomeZoneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.BatchGetPlayerHomeZone failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceBatchGetPlayerHomeZoneFailure(std::shared_ptr<DataServiceBatchGetPlayerHomeZoneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceBatchGetPlayerHomeZone(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceBatchGetPlayerHomeZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done) {
    auto call = std::make_shared<DataServiceBatchGetPlayerHomeZoneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceBatchGetPlayerHomeZoneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceBatchGetPlayerHomeZoneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceBatchGetPlayerHomeZoneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceBatchGetPlayerHomeZone(*call, outcome, nullptr);
            return;
        }
        ::data_service::BatchGetPlayerHomeZoneResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.BatchGetPlayerHomeZone response";
            CompleteDataServiceBatchGetPlayerHomeZone(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceBatchGetPlayerHomeZone(*call, outcome, &reply);
    });
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request,
                                            GrpcDone<::data_service::BatchGetPlayerHomeZoneResponse> done, const GrpcCallOptions& options) {
    StartDataServiceBatchGetPlayerHomeZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceBatchGetPlayerHomeZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerHomeZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceBatchGetPlayerHomeZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceBatchGetPlayerHomeZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::BatchGetPlayerHomeZoneRequest& derived = static_cast<const ::data_service::BatchGetPlayerHomeZoneRequest&>(message);
    SendDataServiceBatchGetPlayerHomeZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRemapHomeZoneForMerge
AsyncDataServiceRemapHomeZoneForMergeHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeHandler;
AsyncDataServiceRemapHomeZoneForMergeFailedHandlerFunctionType AsyncDataServiceRemapHomeZoneForMergeFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceRemapHomeZoneForMergeCall {
    ::data_service::RemapHomeZoneForMergeRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRemapHomeZoneForMergeHandler / FailedHandler。
    GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done;
};

void CompleteDataServiceRemapHomeZoneForMerge(const DataServiceRemapHomeZoneForMergeCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::RemapHomeZoneForMergeResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceRemapHomeZoneForMergeHandler) {
            AsyncDataServiceRemapHomeZoneForMergeHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceRemapHomeZoneForMergeMessageId, "DataService.RemapHomeZoneForMerge", outcome, call.sentMetadata};
        AsyncDataServiceRemapHomeZoneForMergeFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.RemapHomeZoneForMerge failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceRemapHomeZoneForMergeFailure(std::shared_ptr<DataServiceRemapHomeZoneForMergeCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceRemapHomeZoneForMerge(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRemapHomeZoneForMergeCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done) {
    auto call = std::make_shared<DataServiceRemapHomeZoneForMergeCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceRemapHomeZoneForMergeFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceRemapHomeZoneForMergeGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceRemapHomeZoneForMergeFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceRemapHomeZoneForMerge(*call, outcome, nullptr);
            return;
        }
        ::data_service::RemapHomeZoneForMergeResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.RemapHomeZoneForMerge response";
            CompleteDataServiceRemapHomeZoneForMerge(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceRemapHomeZoneForMerge(*call, outcome, &reply);
    });
}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request,
                                            GrpcDone<::data_service::RemapHomeZoneForMergeResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRemapHomeZoneForMergeCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRemapHomeZoneForMerge(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RemapHomeZoneForMergeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRemapHomeZoneForMergeCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceRemapHomeZoneForMerge(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::RemapHomeZoneForMergeRequest& derived = static_cast<const ::data_service::RemapHomeZoneForMergeRequest&>(message);
    SendDataServiceRemapHomeZoneForMerge(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceDeletePlayerData
AsyncDataServiceDeletePlayerDataHandlerFunctionType AsyncDataServiceDeletePlayerDataHandler;
AsyncDataServiceDeletePlayerDataFailedHandlerFunctionType AsyncDataServiceDeletePlayerDataFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceDeletePlayerDataCall {
    ::data_service::DeletePlayerDataRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceDeletePlayerDataHandler / FailedHandler。
    GrpcDone<::data_service::DeletePlayerDataResponse> done;
};

void CompleteDataServiceDeletePlayerData(const DataServiceDeletePlayerDataCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::DeletePlayerDataResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceDeletePlayerDataHandler) {
            AsyncDataServiceDeletePlayerDataHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceDeletePlayerDataMessageId, "DataService.DeletePlayerData", outcome, call.sentMetadata};
        AsyncDataServiceDeletePlayerDataFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.DeletePlayerData failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceDeletePlayerDataFailure(std::shared_ptr<DataServiceDeletePlayerDataCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceDeletePlayerData(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceDeletePlayerDataCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::DeletePlayerDataResponse> done) {
    auto call = std::make_shared<DataServiceDeletePlayerDataCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceDeletePlayerDataFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceDeletePlayerDataGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceDeletePlayerDataFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceDeletePlayerData(*call, outcome, nullptr);
            return;
        }
        ::data_service::DeletePlayerDataResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.DeletePlayerData response";
            CompleteDataServiceDeletePlayerData(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceDeletePlayerData(*call, outcome, &reply);
    });
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request,
                                            GrpcDone<::data_service::DeletePlayerDataResponse> done, const GrpcCallOptions& options) {
    StartDataServiceDeletePlayerDataCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceDeletePlayerData(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const ::data_service::DeletePlayerDataRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceDeletePlayerDataCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceDeletePlayerData(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::DeletePlayerDataRequest& derived = static_cast<const ::data_service::DeletePlayerDataRequest&>(message);
    SendDataServiceDeletePlayerData(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceCreatePlayerSnapshot
AsyncDataServiceCreatePlayerSnapshotHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotHandler;
AsyncDataServiceCreatePlayerSnapshotFailedHandlerFunctionType AsyncDataServiceCreatePlayerSnapshotFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceCreatePlayerSnapshotCall {
    ::data_service::CreatePlayerSnapshotRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceCreatePlayerSnapshotHandler / FailedHandler。
    GrpcDone<::data_service::CreatePlayerSnapshotResponse> done;
};

void CompleteDataServiceCreatePlayerSnapshot(const DataServiceCreatePlayerSnapshotCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::CreatePlayerSnapshotResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceCreatePlayerSnapshotHandler) {
            AsyncDataServiceCreatePlayerSnapshotHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceCreatePlayerSnapshotMessageId, "DataService.CreatePlayerSnapshot", outcome, call.sentMetadata};
        AsyncDataServiceCreatePlayerSnapshotFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.CreatePlayerSnapshot failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceCreatePlayerSnapshotFailure(std::shared_ptr<DataServiceCreatePlayerSnapshotCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceCreatePlayerSnapshot(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceCreatePlayerSnapshotCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::CreatePlayerSnapshotResponse> done) {
    auto call = std::make_shared<DataServiceCreatePlayerSnapshotCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceCreatePlayerSnapshotFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceCreatePlayerSnapshotGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceCreatePlayerSnapshotFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceCreatePlayerSnapshot(*call, outcome, nullptr);
            return;
        }
        ::data_service::CreatePlayerSnapshotResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.CreatePlayerSnapshot response";
            CompleteDataServiceCreatePlayerSnapshot(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceCreatePlayerSnapshot(*call, outcome, &reply);
    });
}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request,
                                            GrpcDone<::data_service::CreatePlayerSnapshotResponse> done, const GrpcCallOptions& options) {
    StartDataServiceCreatePlayerSnapshotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceCreatePlayerSnapshot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreatePlayerSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceCreatePlayerSnapshotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceCreatePlayerSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::CreatePlayerSnapshotRequest& derived = static_cast<const ::data_service::CreatePlayerSnapshotRequest&>(message);
    SendDataServiceCreatePlayerSnapshot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceListPlayerSnapshots
AsyncDataServiceListPlayerSnapshotsHandlerFunctionType AsyncDataServiceListPlayerSnapshotsHandler;
AsyncDataServiceListPlayerSnapshotsFailedHandlerFunctionType AsyncDataServiceListPlayerSnapshotsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceListPlayerSnapshotsCall {
    ::data_service::ListPlayerSnapshotsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceListPlayerSnapshotsHandler / FailedHandler。
    GrpcDone<::data_service::ListPlayerSnapshotsResponse> done;
};

void CompleteDataServiceListPlayerSnapshots(const DataServiceListPlayerSnapshotsCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::ListPlayerSnapshotsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceListPlayerSnapshotsHandler) {
            AsyncDataServiceListPlayerSnapshotsHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceListPlayerSnapshotsMessageId, "DataService.ListPlayerSnapshots", outcome, call.sentMetadata};
        AsyncDataServiceListPlayerSnapshotsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.ListPlayerSnapshots failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceListPlayerSnapshotsFailure(std::shared_ptr<DataServiceListPlayerSnapshotsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceListPlayerSnapshots(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceListPlayerSnapshotsCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::ListPlayerSnapshotsResponse> done) {
    auto call = std::make_shared<DataServiceListPlayerSnapshotsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceListPlayerSnapshotsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceListPlayerSnapshotsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceListPlayerSnapshotsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceListPlayerSnapshots(*call, outcome, nullptr);
            return;
        }
        ::data_service::ListPlayerSnapshotsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.ListPlayerSnapshots response";
            CompleteDataServiceListPlayerSnapshots(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceListPlayerSnapshots(*call, outcome, &reply);
    });
}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request,
                                            GrpcDone<::data_service::ListPlayerSnapshotsResponse> done, const GrpcCallOptions& options) {
    StartDataServiceListPlayerSnapshotsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceListPlayerSnapshots(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ListPlayerSnapshotsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceListPlayerSnapshotsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceListPlayerSnapshots(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::ListPlayerSnapshotsRequest& derived = static_cast<const ::data_service::ListPlayerSnapshotsRequest&>(message);
    SendDataServiceListPlayerSnapshots(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerSnapshotDiff
AsyncDataServiceGetPlayerSnapshotDiffHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffHandler;
AsyncDataServiceGetPlayerSnapshotDiffFailedHandlerFunctionType AsyncDataServiceGetPlayerSnapshotDiffFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceGetPlayerSnapshotDiffCall {
    ::data_service::GetPlayerSnapshotDiffRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerSnapshotDiffHandler / FailedHandler。
    GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done;
};

void CompleteDataServiceGetPlayerSnapshotDiff(const DataServiceGetPlayerSnapshotDiffCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::GetPlayerSnapshotDiffResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceGetPlayerSnapshotDiffHandler) {
            AsyncDataServiceGetPlayerSnapshotDiffHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceGetPlayerSnapshotDiffMessageId, "DataService.GetPlayerSnapshotDiff", outcome, call.sentMetadata};
        AsyncDataServiceGetPlayerSnapshotDiffFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerSnapshotDiff failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceGetPlayerSnapshotDiffFailure(std::shared_ptr<DataServiceGetPlayerSnapshotDiffCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceGetPlayerSnapshotDiff(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerSnapshotDiffCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done) {
    auto call = std::make_shared<DataServiceGetPlayerSnapshotDiffCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceGetPlayerSnapshotDiffFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceGetPlayerSnapshotDiffGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceGetPlayerSnapshotDiffFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceGetPlayerSnapshotDiff(*call, outcome, nullptr);
            return;
        }
        ::data_service::GetPlayerSnapshotDiffResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.GetPlayerSnapshotDiff response";
            CompleteDataServiceGetPlayerSnapshotDiff(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceGetPlayerSnapshotDiff(*call, outcome, &reply);
    });
}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request,
                                            GrpcDone<::data_service::GetPlayerSnapshotDiffResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerSnapshotDiffCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerSnapshotDiff(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerSnapshotDiffRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerSnapshotDiffCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceGetPlayerSnapshotDiff(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::GetPlayerSnapshotDiffRequest& derived = static_cast<const ::data_service::GetPlayerSnapshotDiffRequest&>(message);
    SendDataServiceGetPlayerSnapshotDiff(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRollbackPlayer
AsyncDataServiceRollbackPlayerHandlerFunctionType AsyncDataServiceRollbackPlayerHandler;
AsyncDataServiceRollbackPlayerFailedHandlerFunctionType AsyncDataServiceRollbackPlayerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceRollbackPlayerCall {
    ::data_service::RollbackPlayerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRollbackPlayerHandler / FailedHandler。
    GrpcDone<::data_service::RollbackPlayerResponse> done;
};

void CompleteDataServiceRollbackPlayer(const DataServiceRollbackPlayerCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::RollbackPlayerResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceRollbackPlayerHandler) {
            AsyncDataServiceRollbackPlayerHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceRollbackPlayerMessageId, "DataService.RollbackPlayer", outcome, call.sentMetadata};
        AsyncDataServiceRollbackPlayerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.RollbackPlayer failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceRollbackPlayerFailure(std::shared_ptr<DataServiceRollbackPlayerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceRollbackPlayer(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRollbackPlayerCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RollbackPlayerResponse> done) {
    auto call = std::make_shared<DataServiceRollbackPlayerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceRollbackPlayerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceRollbackPlayerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceRollbackPlayerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceRollbackPlayer(*call, outcome, nullptr);
            return;
        }
        ::data_service::RollbackPlayerResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.RollbackPlayer response";
            CompleteDataServiceRollbackPlayer(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceRollbackPlayer(*call, outcome, &reply);
    });
}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request,
                                            GrpcDone<::data_service::RollbackPlayerResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRollbackPlayerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRollbackPlayer(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRollbackPlayerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceRollbackPlayer(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::RollbackPlayerRequest& derived = static_cast<const ::data_service::RollbackPlayerRequest&>(message);
    SendDataServiceRollbackPlayer(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRollbackZone
AsyncDataServiceRollbackZoneHandlerFunctionType AsyncDataServiceRollbackZoneHandler;
AsyncDataServiceRollbackZoneFailedHandlerFunctionType AsyncDataServiceRollbackZoneFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceRollbackZoneCall {
    ::data_service::RollbackZoneRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRollbackZoneHandler / FailedHandler。
    GrpcDone<::data_service::RollbackZoneResponse> done;
};

void CompleteDataServiceRollbackZone(const DataServiceRollbackZoneCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::RollbackZoneResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceRollbackZoneHandler) {
            AsyncDataServiceRollbackZoneHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceRollbackZoneMessageId, "DataService.RollbackZone", outcome, call.sentMetadata};
        AsyncDataServiceRollbackZoneFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.RollbackZone failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceRollbackZoneFailure(std::shared_ptr<DataServiceRollbackZoneCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceRollbackZone(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRollbackZoneCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RollbackZoneResponse> done) {
    auto call = std::make_shared<DataServiceRollbackZoneCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceRollbackZoneFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceRollbackZoneGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceRollbackZoneFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceRollbackZone(*call, outcome, nullptr);
            return;
        }
        ::data_service::RollbackZoneResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.RollbackZone response";
            CompleteDataServiceRollbackZone(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceRollbackZone(*call, outcome, &reply);
    });
}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request,
                                            GrpcDone<::data_service::RollbackZoneResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRollbackZoneCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRollbackZone(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackZoneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRollbackZoneCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceRollbackZone(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::RollbackZoneRequest& derived = static_cast<const ::data_service::RollbackZoneRequest&>(message);
    SendDataServiceRollbackZone(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceRollbackAll
AsyncDataServiceRollbackAllHandlerFunctionType AsyncDataServiceRollbackAllHandler;
AsyncDataServiceRollbackAllFailedHandlerFunctionType AsyncDataServiceRollbackAllFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceRollbackAllCall {
    ::data_service::RollbackAllRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceRollbackAllHandler / FailedHandler。
    GrpcDone<::data_service::RollbackAllResponse> done;
};

void CompleteDataServiceRollbackAll(const DataServiceRollbackAllCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::RollbackAllResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceRollbackAllHandler) {
            AsyncDataServiceRollbackAllHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceRollbackAllMessageId, "DataService.RollbackAll", outcome, call.sentMetadata};
        AsyncDataServiceRollbackAllFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.RollbackAll failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceRollbackAllFailure(std::shared_ptr<DataServiceRollbackAllCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceRollbackAll(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceRollbackAllCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::RollbackAllResponse> done) {
    auto call = std::make_shared<DataServiceRollbackAllCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceRollbackAllFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceRollbackAllGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceRollbackAllFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceRollbackAll(*call, outcome, nullptr);
            return;
        }
        ::data_service::RollbackAllResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.RollbackAll response";
            CompleteDataServiceRollbackAll(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceRollbackAll(*call, outcome, &reply);
    });
}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request,
                                            GrpcDone<::data_service::RollbackAllResponse> done, const GrpcCallOptions& options) {
    StartDataServiceRollbackAllCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceRollbackAll(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const ::data_service::RollbackAllRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceRollbackAllCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceRollbackAll(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::RollbackAllRequest& derived = static_cast<const ::data_service::RollbackAllRequest&>(message);
    SendDataServiceRollbackAll(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceBatchRecallItems
AsyncDataServiceBatchRecallItemsHandlerFunctionType AsyncDataServiceBatchRecallItemsHandler;
AsyncDataServiceBatchRecallItemsFailedHandlerFunctionType AsyncDataServiceBatchRecallItemsFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceBatchRecallItemsCall {
    ::data_service::BatchRecallItemsRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceBatchRecallItemsHandler / FailedHandler。
    GrpcDone<::data_service::BatchRecallItemsResponse> done;
};

void CompleteDataServiceBatchRecallItems(const DataServiceBatchRecallItemsCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::BatchRecallItemsResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceBatchRecallItemsHandler) {
            AsyncDataServiceBatchRecallItemsHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceBatchRecallItemsMessageId, "DataService.BatchRecallItems", outcome, call.sentMetadata};
        AsyncDataServiceBatchRecallItemsFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.BatchRecallItems failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceBatchRecallItemsFailure(std::shared_ptr<DataServiceBatchRecallItemsCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceBatchRecallItems(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceBatchRecallItemsCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::BatchRecallItemsResponse> done) {
    auto call = std::make_shared<DataServiceBatchRecallItemsCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceBatchRecallItemsFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceBatchRecallItemsGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceBatchRecallItemsFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceBatchRecallItems(*call, outcome, nullptr);
            return;
        }
        ::data_service::BatchRecallItemsResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.BatchRecallItems response";
            CompleteDataServiceBatchRecallItems(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceBatchRecallItems(*call, outcome, &reply);
    });
}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request,
                                            GrpcDone<::data_service::BatchRecallItemsResponse> done, const GrpcCallOptions& options) {
    StartDataServiceBatchRecallItemsCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceBatchRecallItems(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchRecallItemsRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceBatchRecallItemsCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceBatchRecallItems(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::BatchRecallItemsRequest& derived = static_cast<const ::data_service::BatchRecallItemsRequest&>(message);
    SendDataServiceBatchRecallItems(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceQueryTransactionLog
AsyncDataServiceQueryTransactionLogHandlerFunctionType AsyncDataServiceQueryTransactionLogHandler;
AsyncDataServiceQueryTransactionLogFailedHandlerFunctionType AsyncDataServiceQueryTransactionLogFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceQueryTransactionLogCall {
    ::data_service::QueryTransactionLogRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceQueryTransactionLogHandler / FailedHandler。
    GrpcDone<::data_service::QueryTransactionLogResponse> done;
};

void CompleteDataServiceQueryTransactionLog(const DataServiceQueryTransactionLogCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::QueryTransactionLogResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceQueryTransactionLogHandler) {
            AsyncDataServiceQueryTransactionLogHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceQueryTransactionLogMessageId, "DataService.QueryTransactionLog", outcome, call.sentMetadata};
        AsyncDataServiceQueryTransactionLogFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.QueryTransactionLog failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceQueryTransactionLogFailure(std::shared_ptr<DataServiceQueryTransactionLogCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceQueryTransactionLog(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceQueryTransactionLogCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::QueryTransactionLogResponse> done) {
    auto call = std::make_shared<DataServiceQueryTransactionLogCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceQueryTransactionLogFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceQueryTransactionLogGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceQueryTransactionLogFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceQueryTransactionLog(*call, outcome, nullptr);
            return;
        }
        ::data_service::QueryTransactionLogResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.QueryTransactionLog response";
            CompleteDataServiceQueryTransactionLog(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceQueryTransactionLog(*call, outcome, &reply);
    });
}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request,
                                            GrpcDone<::data_service::QueryTransactionLogResponse> done, const GrpcCallOptions& options) {
    StartDataServiceQueryTransactionLogCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceQueryTransactionLog(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const ::data_service::QueryTransactionLogRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceQueryTransactionLogCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceQueryTransactionLog(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::QueryTransactionLogRequest& derived = static_cast<const ::data_service::QueryTransactionLogRequest&>(message);
    SendDataServiceQueryTransactionLog(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceCreateEventSnapshot
AsyncDataServiceCreateEventSnapshotHandlerFunctionType AsyncDataServiceCreateEventSnapshotHandler;
AsyncDataServiceCreateEventSnapshotFailedHandlerFunctionType AsyncDataServiceCreateEventSnapshotFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceCreateEventSnapshotCall {
    ::data_service::CreateEventSnapshotRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceCreateEventSnapshotHandler / FailedHandler。
    GrpcDone<::data_service::CreateEventSnapshotResponse> done;
};

void CompleteDataServiceCreateEventSnapshot(const DataServiceCreateEventSnapshotCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::CreateEventSnapshotResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceCreateEventSnapshotHandler) {
            AsyncDataServiceCreateEventSnapshotHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceCreateEventSnapshotMessageId, "DataService.CreateEventSnapshot", outcome, call.sentMetadata};
        AsyncDataServiceCreateEventSnapshotFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.CreateEventSnapshot failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceCreateEventSnapshotFailure(std::shared_ptr<DataServiceCreateEventSnapshotCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceCreateEventSnapshot(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceCreateEventSnapshotCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::CreateEventSnapshotResponse> done) {
    auto call = std::make_shared<DataServiceCreateEventSnapshotCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceCreateEventSnapshotFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceCreateEventSnapshotGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceCreateEventSnapshotFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceCreateEventSnapshot(*call, outcome, nullptr);
            return;
        }
        ::data_service::CreateEventSnapshotResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.CreateEventSnapshot response";
            CompleteDataServiceCreateEventSnapshot(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceCreateEventSnapshot(*call, outcome, &reply);
    });
}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request,
                                            GrpcDone<::data_service::CreateEventSnapshotResponse> done, const GrpcCallOptions& options) {
    StartDataServiceCreateEventSnapshotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceCreateEventSnapshot(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::data_service::CreateEventSnapshotRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceCreateEventSnapshotCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceCreateEventSnapshot(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::CreateEventSnapshotRequest& derived = static_cast<const ::data_service::CreateEventSnapshotRequest&>(message);
    SendDataServiceCreateEventSnapshot(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceAllocateIdSegment
AsyncDataServiceAllocateIdSegmentHandlerFunctionType AsyncDataServiceAllocateIdSegmentHandler;
AsyncDataServiceAllocateIdSegmentFailedHandlerFunctionType AsyncDataServiceAllocateIdSegmentFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceAllocateIdSegmentCall {
    ::data_service::AllocateIdSegmentRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceAllocateIdSegmentHandler / FailedHandler。
    GrpcDone<::data_service::AllocateIdSegmentResponse> done;
};

void CompleteDataServiceAllocateIdSegment(const DataServiceAllocateIdSegmentCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::AllocateIdSegmentResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceAllocateIdSegmentHandler) {
            AsyncDataServiceAllocateIdSegmentHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceAllocateIdSegmentMessageId, "DataService.AllocateIdSegment", outcome, call.sentMetadata};
        AsyncDataServiceAllocateIdSegmentFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.AllocateIdSegment failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceAllocateIdSegmentFailure(std::shared_ptr<DataServiceAllocateIdSegmentCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceAllocateIdSegment(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceAllocateIdSegmentCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::AllocateIdSegmentResponse> done) {
    auto call = std::make_shared<DataServiceAllocateIdSegmentCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceAllocateIdSegmentFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceAllocateIdSegmentGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceAllocateIdSegmentFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceAllocateIdSegment(*call, outcome, nullptr);
            return;
        }
        ::data_service::AllocateIdSegmentResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.AllocateIdSegment response";
            CompleteDataServiceAllocateIdSegment(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceAllocateIdSegment(*call, outcome, &reply);
    });
}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request,
                                            GrpcDone<::data_service::AllocateIdSegmentResponse> done, const GrpcCallOptions& options) {
    StartDataServiceAllocateIdSegmentCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceAllocateIdSegment(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const ::data_service::AllocateIdSegmentRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceAllocateIdSegmentCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceAllocateIdSegment(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::AllocateIdSegmentRequest& derived = static_cast<const ::data_service::AllocateIdSegmentRequest&>(message);
    SendDataServiceAllocateIdSegment(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceReservePlayerName
AsyncDataServiceReservePlayerNameHandlerFunctionType AsyncDataServiceReservePlayerNameHandler;
AsyncDataServiceReservePlayerNameFailedHandlerFunctionType AsyncDataServiceReservePlayerNameFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceReservePlayerNameCall {
    ::data_service::ReservePlayerNameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceReservePlayerNameHandler / FailedHandler。
    GrpcDone<::data_service::ReservePlayerNameResponse> done;
};

void CompleteDataServiceReservePlayerName(const DataServiceReservePlayerNameCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::ReservePlayerNameResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceReservePlayerNameHandler) {
            AsyncDataServiceReservePlayerNameHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceReservePlayerNameMessageId, "DataService.ReservePlayerName", outcome, call.sentMetadata};
        AsyncDataServiceReservePlayerNameFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.ReservePlayerName failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceReservePlayerNameFailure(std::shared_ptr<DataServiceReservePlayerNameCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceReservePlayerName(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceReservePlayerNameCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::ReservePlayerNameResponse> done) {
    auto call = std::make_shared<DataServiceReservePlayerNameCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceReservePlayerNameFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceReservePlayerNameGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceReservePlayerNameFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceReservePlayerName(*call, outcome, nullptr);
            return;
        }
        ::data_service::ReservePlayerNameResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.ReservePlayerName response";
            CompleteDataServiceReservePlayerName(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceReservePlayerName(*call, outcome, &reply);
    });
}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request,
                                            GrpcDone<::data_service::ReservePlayerNameResponse> done, const GrpcCallOptions& options) {
    StartDataServiceReservePlayerNameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceReservePlayerName(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReservePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceReservePlayerNameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceReservePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::ReservePlayerNameRequest& derived = static_cast<const ::data_service::ReservePlayerNameRequest&>(message);
    SendDataServiceReservePlayerName(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceReleasePlayerName
AsyncDataServiceReleasePlayerNameHandlerFunctionType AsyncDataServiceReleasePlayerNameHandler;
AsyncDataServiceReleasePlayerNameFailedHandlerFunctionType AsyncDataServiceReleasePlayerNameFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceReleasePlayerNameCall {
    ::data_service::ReleasePlayerNameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceReleasePlayerNameHandler / FailedHandler。
    GrpcDone<::google::protobuf::Empty> done;
};

void CompleteDataServiceReleasePlayerName(const DataServiceReleasePlayerNameCall& call, const GrpcCallOutcome& outcome,
                                                const ::google::protobuf::Empty* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceReleasePlayerNameHandler) {
            AsyncDataServiceReleasePlayerNameHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceReleasePlayerNameMessageId, "DataService.ReleasePlayerName", outcome, call.sentMetadata};
        AsyncDataServiceReleasePlayerNameFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.ReleasePlayerName failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceReleasePlayerNameFailure(std::shared_ptr<DataServiceReleasePlayerNameCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceReleasePlayerName(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceReleasePlayerNameCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::google::protobuf::Empty> done) {
    auto call = std::make_shared<DataServiceReleasePlayerNameCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceReleasePlayerNameFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceReleasePlayerNameGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceReleasePlayerNameFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceReleasePlayerName(*call, outcome, nullptr);
            return;
        }
        ::google::protobuf::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.ReleasePlayerName response";
            CompleteDataServiceReleasePlayerName(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceReleasePlayerName(*call, outcome, &reply);
    });
}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request,
                                            GrpcDone<::google::protobuf::Empty> done, const GrpcCallOptions& options) {
    StartDataServiceReleasePlayerNameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceReleasePlayerName(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::ReleasePlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceReleasePlayerNameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceReleasePlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::ReleasePlayerNameRequest& derived = static_cast<const ::data_service::ReleasePlayerNameRequest&>(message);
    SendDataServiceReleasePlayerName(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceBatchGetPlayerName
AsyncDataServiceBatchGetPlayerNameHandlerFunctionType AsyncDataServiceBatchGetPlayerNameHandler;
AsyncDataServiceBatchGetPlayerNameFailedHandlerFunctionType AsyncDataServiceBatchGetPlayerNameFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceBatchGetPlayerNameCall {
    ::data_service::BatchGetPlayerNameRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceBatchGetPlayerNameHandler / FailedHandler。
    GrpcDone<::data_service::BatchGetPlayerNameResponse> done;
};

void CompleteDataServiceBatchGetPlayerName(const DataServiceBatchGetPlayerNameCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::BatchGetPlayerNameResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceBatchGetPlayerNameHandler) {
            AsyncDataServiceBatchGetPlayerNameHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceBatchGetPlayerNameMessageId, "DataService.BatchGetPlayerName", outcome, call.sentMetadata};
        AsyncDataServiceBatchGetPlayerNameFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.BatchGetPlayerName failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceBatchGetPlayerNameFailure(std::shared_ptr<DataServiceBatchGetPlayerNameCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceBatchGetPlayerName(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceBatchGetPlayerNameCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::BatchGetPlayerNameResponse> done) {
    auto call = std::make_shared<DataServiceBatchGetPlayerNameCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceBatchGetPlayerNameFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceBatchGetPlayerNameGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceBatchGetPlayerNameFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceBatchGetPlayerName(*call, outcome, nullptr);
            return;
        }
        ::data_service::BatchGetPlayerNameResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.BatchGetPlayerName response";
            CompleteDataServiceBatchGetPlayerName(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceBatchGetPlayerName(*call, outcome, &reply);
    });
}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request,
                                            GrpcDone<::data_service::BatchGetPlayerNameResponse> done, const GrpcCallOptions& options) {
    StartDataServiceBatchGetPlayerNameCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceBatchGetPlayerName(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const ::data_service::BatchGetPlayerNameRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceBatchGetPlayerNameCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceBatchGetPlayerName(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::BatchGetPlayerNameRequest& derived = static_cast<const ::data_service::BatchGetPlayerNameRequest&>(message);
    SendDataServiceBatchGetPlayerName(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerAssetOpLedger
AsyncDataServiceGetPlayerAssetOpLedgerHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerHandler;
AsyncDataServiceGetPlayerAssetOpLedgerFailedHandlerFunctionType AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceGetPlayerAssetOpLedgerCall {
    ::data_service::GetPlayerAssetOpLedgerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerAssetOpLedgerHandler / FailedHandler。
    GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done;
};

void CompleteDataServiceGetPlayerAssetOpLedger(const DataServiceGetPlayerAssetOpLedgerCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::GetPlayerAssetOpLedgerResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceGetPlayerAssetOpLedgerHandler) {
            AsyncDataServiceGetPlayerAssetOpLedgerHandler(outcome, *reply);
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
        const GrpcCallFailure failure{ DataServiceGetPlayerAssetOpLedgerMessageId, "DataService.GetPlayerAssetOpLedger", outcome, call.sentMetadata};
        AsyncDataServiceGetPlayerAssetOpLedgerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerAssetOpLedger failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceGetPlayerAssetOpLedgerFailure(std::shared_ptr<DataServiceGetPlayerAssetOpLedgerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceGetPlayerAssetOpLedger(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerAssetOpLedgerCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done) {
    auto call = std::make_shared<DataServiceGetPlayerAssetOpLedgerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceGetPlayerAssetOpLedgerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceGetPlayerAssetOpLedgerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceGetPlayerAssetOpLedgerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceGetPlayerAssetOpLedger(*call, outcome, nullptr);
            return;
        }
        ::data_service::GetPlayerAssetOpLedgerResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.GetPlayerAssetOpLedger response";
            CompleteDataServiceGetPlayerAssetOpLedger(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceGetPlayerAssetOpLedger(*call, outcome, &reply);
    });
}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request,
                                            GrpcDone<::data_service::GetPlayerAssetOpLedgerResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerAssetOpLedgerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerAssetOpLedger(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerAssetOpLedgerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerAssetOpLedgerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceGetPlayerAssetOpLedger(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::GetPlayerAssetOpLedgerRequest& derived = static_cast<const ::data_service::GetPlayerAssetOpLedgerRequest&>(message);
    SendDataServiceGetPlayerAssetOpLedger(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region DataServiceGetPlayerInboxLedger
AsyncDataServiceGetPlayerInboxLedgerHandlerFunctionType AsyncDataServiceGetPlayerInboxLedgerHandler;
AsyncDataServiceGetPlayerInboxLedgerFailedHandlerFunctionType AsyncDataServiceGetPlayerInboxLedgerFailedHandler;

namespace {
// 一次调用的状态:随完成回调走(shared_ptr),不进 ECS、不放对象池。
// request 必须是副本:gate 通用路径发出的是 gRpcMethodRegistry 里的共享原型,下一条客户端消息就会覆盖它。
struct DataServiceGetPlayerInboxLedgerCall {
    ::data_service::GetPlayerInboxLedgerRequest request;
    GrpcSentMetadata sentMetadata;
    // 每次调用回调;为空时走全局 AsyncDataServiceGetPlayerInboxLedgerHandler / FailedHandler。
    GrpcDone<::data_service::GetPlayerInboxLedgerResponse> done;
};

void CompleteDataServiceGetPlayerInboxLedger(const DataServiceGetPlayerInboxLedgerCall& call, const GrpcCallOutcome& outcome,
                                                const ::data_service::GetPlayerInboxLedgerResponse* reply) {
    if (call.done) {
        // 带每次调用回调的调用只交给它,不再走全局应答 / 失败处理器。reply 只在回调期间有效。
        call.done(outcome, outcome.ok() ? reply : nullptr);
        return;
    }
    if (outcome.ok()) {
        if (AsyncDataServiceGetPlayerInboxLedgerHandler) {
            AsyncDataServiceGetPlayerInboxLedgerHandler(outcome, *reply);
        } else {
            // 应答到了却没人收:2026-04 起换图应答就是这样静默丢了约 5 个月。每个方法每线程报一次;
            // 确实不需要应答的调用方显式装一个空处理器。
            thread_local bool reportedMissingHandler = false;
            if (!reportedMissingHandler) {
                reportedMissingHandler = true;
                LOG_ERROR << "gRPC DataService.GetPlayerInboxLedger reply dropped: AsyncDataServiceGetPlayerInboxLedgerHandler is not installed"
                          << " (install one, or an empty one if the reply is intentionally ignored)";
            }
        }
    } else if (AsyncDataServiceGetPlayerInboxLedgerFailedHandler) {
        const GrpcCallFailure failure{ DataServiceGetPlayerInboxLedgerMessageId, "DataService.GetPlayerInboxLedger", outcome, call.sentMetadata};
        AsyncDataServiceGetPlayerInboxLedgerFailedHandler(failure, call.request);
    } else {
        LOG_ERROR << "gRPC DataService.GetPlayerInboxLedger failed: code=" << outcome.code << " msg=" << outcome.message;
    }
}

void QueueLocalDataServiceGetPlayerInboxLedgerFailure(std::shared_ptr<DataServiceGetPlayerInboxLedgerCall> call, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([call = std::move(call), outcome = LocalFailure(code, std::move(message))] {
        CompleteDataServiceGetPlayerInboxLedger(*call, outcome, nullptr);
    });
}
}

// 所有 unary Send 重载的唯一发送入口。deadline <= 0 用本文件默认 deadline;done 为空走全局处理器。
void StartDataServiceGetPlayerInboxLedgerCall(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::data_service::GetPlayerInboxLedgerResponse> done) {
    auto call = std::make_shared<DataServiceGetPlayerInboxLedgerCall>();
    call->request = request;
    call->sentMetadata = std::move(metadata);
    call->done = std::move(done);

    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalDataServiceGetPlayerInboxLedgerFailure(std::move(call), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kDataServiceGetPlayerInboxLedgerGrpcPath);
    if (!call->request.SerializeToString(&rpc.message)) {
        QueueLocalDataServiceGetPlayerInboxLedgerFailure(std::move(call), kGrpcStatusInternal, "failed to serialize the request");
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
            CompleteDataServiceGetPlayerInboxLedger(*call, outcome, nullptr);
            return;
        }
        ::data_service::GetPlayerInboxLedgerResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the DataService.GetPlayerInboxLedger response";
            CompleteDataServiceGetPlayerInboxLedger(*call, outcome, nullptr);
            return;
        }
        CompleteDataServiceGetPlayerInboxLedger(*call, outcome, &reply);
    });
}

void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request,
                                            GrpcDone<::data_service::GetPlayerInboxLedgerResponse> done, const GrpcCallOptions& options) {
    StartDataServiceGetPlayerInboxLedgerCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request) {
    // 显式写出空 vector:另有 (request, GrpcDone, GrpcCallOptions) 重载,两个 {} 会有二义性。
    SendDataServiceGetPlayerInboxLedger(registry, nodeEntity, request, std::vector<std::string>{}, std::vector<std::string>{});
}

void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const ::data_service::GetPlayerInboxLedgerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const size_t count = std::min(metaKeys.size(), metaValues.size());
    GrpcSentMetadata metadata;
    metadata.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        metadata.emplace_back(metaKeys[i], metaValues[i]);
    }
    StartDataServiceGetPlayerInboxLedgerCall(registry, nodeEntity, request, std::move(metadata), std::chrono::milliseconds{0}, nullptr);
}

void SendDataServiceGetPlayerInboxLedger(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues) {
    const ::data_service::GetPlayerInboxLedgerRequest& derived = static_cast<const ::data_service::GetPlayerInboxLedgerRequest&>(message);
    SendDataServiceGetPlayerInboxLedger(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion

void SetDataServiceHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
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
    AsyncDataServiceGetPlayerInboxLedgerHandler = handler;
    (void)handler;
}

void SetDataServiceIfEmptyHandler(const std::function<void(const GrpcReplyContext&, const ::google::protobuf::Message& reply)>& handler) {
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
    if (!AsyncDataServiceGetPlayerInboxLedgerHandler) {
        AsyncDataServiceGetPlayerInboxLedgerHandler = handler;
    }
    (void)handler;
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
    AsyncDataServiceGetPlayerInboxLedgerFailedHandler = handler;
    (void)handler;
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
    if (!AsyncDataServiceGetPlayerInboxLedgerFailedHandler) {
        AsyncDataServiceGetPlayerInboxLedgerFailedHandler = handler;
    }
    (void)handler;
}

void SetDataServiceCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace data_service
