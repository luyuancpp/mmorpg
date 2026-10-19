#include "muduo/base/Logging.h"

#include "team_grpc_client.h"
#include <atomic>
#include <chrono>
#include <string>
#include <utility>
#include "core/network/grpc_client_runtime.h"
#include "core/utils/encode/base64.h"
#include "muduo_grpc/channel.h"

namespace {
// 每个 proto 文件独立的进程级 deadline;启动时按目标节点类型写入。
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

namespace teampb {
#pragma region ClientPlayerTeamCreateTeam
namespace {
void QueueLocalClientPlayerTeamCreateTeamFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamCreateTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.CreateTeam requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamCreateTeamGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamCreateTeamFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamCreateTeamFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.CreateTeam response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamCreateTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::CreateTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamCreateTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamCreateTeamErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.CreateTeam requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::CreateTeamRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.CreateTeam: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::CreateTeamRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.CreateTeam: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamCreateTeamCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamGetMyTeam
namespace {
void QueueLocalClientPlayerTeamGetMyTeamFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamGetMyTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.GetMyTeam requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamGetMyTeamGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamGetMyTeamFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamGetMyTeamFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.GetMyTeam response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamGetMyTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::GetMyTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamGetMyTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamGetMyTeamErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.GetMyTeam requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::GetMyTeamRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.GetMyTeam: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::GetMyTeamRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.GetMyTeam: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamGetMyTeamCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamApplyJoinTeam
namespace {
void QueueLocalClientPlayerTeamApplyJoinTeamFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamApplyJoinTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.ApplyJoinTeam requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamApplyJoinTeamGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamApplyJoinTeamFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamApplyJoinTeamFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.ApplyJoinTeam response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamApplyJoinTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ApplyJoinTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamApplyJoinTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamApplyJoinTeamErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.ApplyJoinTeam requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::ApplyJoinTeamRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.ApplyJoinTeam: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::ApplyJoinTeamRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.ApplyJoinTeam: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamApplyJoinTeamCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamHandleApplication
namespace {
void QueueLocalClientPlayerTeamHandleApplicationFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamHandleApplicationCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.HandleApplication requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamHandleApplicationGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamHandleApplicationFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamHandleApplicationFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.HandleApplication response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamHandleApplication(entt::registry& registry, entt::entity nodeEntity, const ::teampb::HandleApplicationRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamHandleApplicationCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamHandleApplicationErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.HandleApplication requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::HandleApplicationRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.HandleApplication: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::HandleApplicationRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.HandleApplication: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamHandleApplicationCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamInviteToTeam
namespace {
void QueueLocalClientPlayerTeamInviteToTeamFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamInviteToTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.InviteToTeam requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamInviteToTeamGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamInviteToTeamFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamInviteToTeamFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.InviteToTeam response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamInviteToTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::InviteToTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamInviteToTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamInviteToTeamErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.InviteToTeam requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::InviteToTeamRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.InviteToTeam: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::InviteToTeamRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.InviteToTeam: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamInviteToTeamCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamRespondInvite
namespace {
void QueueLocalClientPlayerTeamRespondInviteFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamRespondInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.RespondInvite requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamRespondInviteGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamRespondInviteFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamRespondInviteFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.RespondInvite response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamRespondInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::RespondInviteRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamRespondInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamRespondInviteErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.RespondInvite requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::RespondInviteRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.RespondInvite: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::RespondInviteRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.RespondInvite: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamRespondInviteCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamListMyInvites
namespace {
void QueueLocalClientPlayerTeamListMyInvitesFailure(GrpcDone<::teampb::ListMyInvitesResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamListMyInvitesCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::ListMyInvitesResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.ListMyInvites requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamListMyInvitesGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamListMyInvitesFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamListMyInvitesFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::ListMyInvitesResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.ListMyInvites response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamListMyInvites(entt::registry& registry, entt::entity nodeEntity, const ::teampb::ListMyInvitesRequest& request,
                                            GrpcDone<::teampb::ListMyInvitesResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamListMyInvitesCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamListMyInvitesErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.ListMyInvites requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::ListMyInvitesRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.ListMyInvites: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::ListMyInvitesRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.ListMyInvites: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamListMyInvitesCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::ListMyInvitesResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamLeaveTeam
namespace {
void QueueLocalClientPlayerTeamLeaveTeamFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamLeaveTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.LeaveTeam requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamLeaveTeamGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamLeaveTeamFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamLeaveTeamFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.LeaveTeam response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamLeaveTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::LeaveTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamLeaveTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamLeaveTeamErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.LeaveTeam requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::LeaveTeamRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.LeaveTeam: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::LeaveTeamRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.LeaveTeam: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamLeaveTeamCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamKickMember
namespace {
void QueueLocalClientPlayerTeamKickMemberFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamKickMemberCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.KickMember requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamKickMemberGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamKickMemberFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamKickMemberFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.KickMember response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamKickMember(entt::registry& registry, entt::entity nodeEntity, const ::teampb::KickMemberRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamKickMemberCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamKickMemberErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.KickMember requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::KickMemberRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.KickMember: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::KickMemberRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.KickMember: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamKickMemberCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamTransferLeader
namespace {
void QueueLocalClientPlayerTeamTransferLeaderFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamTransferLeaderCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.TransferLeader requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamTransferLeaderGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamTransferLeaderFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamTransferLeaderFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.TransferLeader response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamTransferLeader(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TransferLeaderRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamTransferLeaderCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamTransferLeaderErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.TransferLeader requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::TransferLeaderRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.TransferLeader: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::TransferLeaderRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.TransferLeader: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamTransferLeaderCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamDisbandTeam
namespace {
void QueueLocalClientPlayerTeamDisbandTeamFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamDisbandTeamCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.DisbandTeam requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamDisbandTeamGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamDisbandTeamFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamDisbandTeamFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.DisbandTeam response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamDisbandTeam(entt::registry& registry, entt::entity nodeEntity, const ::teampb::DisbandTeamRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamDisbandTeamCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamDisbandTeamErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.DisbandTeam requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::DisbandTeamRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.DisbandTeam: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::DisbandTeamRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.DisbandTeam: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamDisbandTeamCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamStartTeamMatch
namespace {
void QueueLocalClientPlayerTeamStartTeamMatchFailure(GrpcDone<::teampb::TeamResponse> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamStartTeamMatchCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::teampb::TeamResponse> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.StartTeamMatch requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamStartTeamMatchGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamStartTeamMatchFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamStartTeamMatchFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::teampb::TeamResponse reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.StartTeamMatch response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamStartTeamMatch(entt::registry& registry, entt::entity nodeEntity, const ::teampb::StartTeamMatchRequest& request,
                                            GrpcDone<::teampb::TeamResponse> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamStartTeamMatchCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamStartTeamMatchErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.StartTeamMatch requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::StartTeamMatchRequest::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.StartTeamMatch: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::StartTeamMatchRequest*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.StartTeamMatch: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamStartTeamMatchCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::teampb::TeamResponse* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamSnapshot
namespace {
void QueueLocalClientPlayerTeamNotifyTeamSnapshotFailure(GrpcDone<::Empty> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamNotifyTeamSnapshotCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.NotifyTeamSnapshot requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamNotifyTeamSnapshotGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamNotifyTeamSnapshotFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamNotifyTeamSnapshotFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.NotifyTeamSnapshot response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamNotifyTeamSnapshot(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamSnapshotS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamSnapshotCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamSnapshotErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.NotifyTeamSnapshot requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::TeamSnapshotS2C::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamSnapshot: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::TeamSnapshotS2C*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamSnapshot: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamNotifyTeamSnapshotCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::Empty* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamInvite
namespace {
void QueueLocalClientPlayerTeamNotifyTeamInviteFailure(GrpcDone<::Empty> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamNotifyTeamInviteCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.NotifyTeamInvite requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamNotifyTeamInviteGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamNotifyTeamInviteFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamNotifyTeamInviteFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.NotifyTeamInvite response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamNotifyTeamInvite(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamInviteS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamInviteCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamInviteErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.NotifyTeamInvite requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::TeamInviteS2C::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamInvite: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::TeamInviteS2C*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamInvite: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamNotifyTeamInviteCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::Empty* reply) { done(outcome, reply); });
}
#pragma endregion
#pragma region ClientPlayerTeamNotifyTeamEvent
namespace {
void QueueLocalClientPlayerTeamNotifyTeamEventFailure(GrpcDone<::Empty> done, uint32_t code, std::string message) {
    QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(code, std::move(message))] {
        done(outcome, nullptr);
    });
}
}

// 请求必须在这里同步序列化:gate 的共享原型在下次调用会被覆盖。
// 若将来改成异步序列化,必须先恢复请求的独立副本。
void StartClientPlayerTeamNotifyTeamEventCall(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request,
                                             GrpcSentMetadata metadata, std::chrono::milliseconds deadline,
                                             GrpcDone<::Empty> done) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.NotifyTeamEvent requires a done callback";
    }
    muduo_grpc::UnaryCallRequest rpc;
    rpc.method = std::string(kClientPlayerTeamNotifyTeamEventGrpcPath);
    if (!request.SerializeToString(&rpc.message)) {
        QueueLocalClientPlayerTeamNotifyTeamEventFailure(std::move(done), kGrpcStatusInternal, "failed to serialize the request");
        return;
    }
    const auto channel = FindGrpcChannel(registry, nodeEntity);
    if (!channel) {
        QueueLocalClientPlayerTeamNotifyTeamEventFailure(std::move(done), kGrpcStatusUnavailable, "node transport missing or closing");
        return;
    }
    rpc.metadata.reserve(metadata.size());
    for (const auto& [key, value] : metadata) {
        // Go 侧依赖这层应用编码;-bin 的线上编码另由传输层负责。
        rpc.metadata.emplace_back(key, Base64Encode(value));
    }
    rpc.deadline = std::chrono::steady_clock::now() + (deadline.count() > 0 ? deadline : DefaultCallDeadline());
    channel->StartUnaryCall(std::move(rpc), [done = std::move(done)](muduo_grpc::UnaryCallResult&& result) {
        GrpcCallOutcome outcome;
        outcome.code = result.status.code_value();
        outcome.message = result.status.message();
        outcome.initialMetadata = std::move(result.initialMetadata);
        outcome.trailingMetadata = std::move(result.trailingMetadata);
        if (!outcome.ok()) {
            done(outcome, nullptr);
            return;
        }
        ::Empty reply;
        if (!reply.ParseFromString(result.response)) {
            outcome.code = kGrpcStatusInternal;
            outcome.message = "failed to parse the ClientPlayerTeam.NotifyTeamEvent response";
            done(outcome, nullptr);
            return;
        }
        done(outcome, &reply);
    });
}

void SendClientPlayerTeamNotifyTeamEvent(entt::registry& registry, entt::entity nodeEntity, const ::teampb::TeamEventS2C& request,
                                            GrpcDone<::Empty> done, const GrpcCallOptions& options) {
    StartClientPlayerTeamNotifyTeamEventCall(registry, nodeEntity, request, options.metadata, options.deadline, std::move(done));
}

void SendClientPlayerTeamNotifyTeamEventErased(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message,
                                                  GrpcMessageDone done, const GrpcCallOptions& options) {
    if (!done) {
        LOG_FATAL << "gRPC ClientPlayerTeam.NotifyTeamEvent requires a done callback";
    }
    if (message.GetDescriptor() != ::teampb::TeamEventS2C::descriptor()) {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamEvent: request type " << message.GetDescriptor()->full_name()
                  << " does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    // descriptor 相同仍可能是 DynamicMessage;进入 typed 路径前必须核对真实 C++ 类型。
    const auto* typedRequest = dynamic_cast<const ::teampb::TeamEventS2C*>(&message);
    if (typedRequest == nullptr) {
        LOG_ERROR << "gRPC ClientPlayerTeam.NotifyTeamEvent: request C++ type does not match; not sent";
        QueueGrpcLocalCompletion([done = std::move(done), outcome = LocalFailure(kGrpcStatusInternal, "request type mismatch")] {
            done(outcome, nullptr);
        });
        return;
    }
    StartClientPlayerTeamNotifyTeamEventCall(registry, nodeEntity, *typedRequest, options.metadata, options.deadline,
        [done = std::move(done)](const GrpcCallOutcome& outcome, const ::Empty* reply) { done(outcome, reply); });
}
#pragma endregion

void SetTeamCallDeadline(std::chrono::milliseconds deadline) {
    callDeadlineMs.store(static_cast<uint32_t>(deadline.count()), std::memory_order_relaxed);
}

}// namespace teampb
