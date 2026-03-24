#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_client/grpc_call_tag.h"
#include "proto/guild/guild.grpc.pb.h"

#include "rpc/service_metadata/guild_service_metadata.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace guildpb {
using GuildServiceStubPtr = std::unique_ptr<GuildService::Stub>;
#pragma region GuildServiceCreateGuild

struct AsyncGuildServiceCreateGuildGrpcClient {
    uint32_t messageId{ GuildServiceCreateGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::CreateGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::CreateGuildResponse>> response_reader;
};

class ::guildpb::CreateGuildRequest;
using AsyncGuildServiceCreateGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::CreateGuildResponse&)>;
extern AsyncGuildServiceCreateGuildHandlerFunctionType AsyncGuildServiceCreateGuildHandler;

void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request);
void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::CreateGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceCreateGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region GuildServiceGetGuild

struct AsyncGuildServiceGetGuildGrpcClient {
    uint32_t messageId{ GuildServiceGetGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetGuildResponse>> response_reader;
};

class ::guildpb::GetGuildRequest;
using AsyncGuildServiceGetGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetGuildResponse&)>;
extern AsyncGuildServiceGetGuildHandlerFunctionType AsyncGuildServiceGetGuildHandler;

void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request);
void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region GuildServiceGetPlayerGuild

struct AsyncGuildServiceGetPlayerGuildGrpcClient {
    uint32_t messageId{ GuildServiceGetPlayerGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::GetPlayerGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::GetPlayerGuildResponse>> response_reader;
};

class ::guildpb::GetPlayerGuildRequest;
using AsyncGuildServiceGetPlayerGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::GetPlayerGuildResponse&)>;
extern AsyncGuildServiceGetPlayerGuildHandlerFunctionType AsyncGuildServiceGetPlayerGuildHandler;

void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request);
void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::GetPlayerGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceGetPlayerGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region GuildServiceJoinGuild

struct AsyncGuildServiceJoinGuildGrpcClient {
    uint32_t messageId{ GuildServiceJoinGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::JoinGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::JoinGuildResponse>> response_reader;
};

class ::guildpb::JoinGuildRequest;
using AsyncGuildServiceJoinGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::JoinGuildResponse&)>;
extern AsyncGuildServiceJoinGuildHandlerFunctionType AsyncGuildServiceJoinGuildHandler;

void SendGuildServiceJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::JoinGuildRequest& request);
void SendGuildServiceJoinGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::JoinGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceJoinGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region GuildServiceLeaveGuild

struct AsyncGuildServiceLeaveGuildGrpcClient {
    uint32_t messageId{ GuildServiceLeaveGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::LeaveGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::LeaveGuildResponse>> response_reader;
};

class ::guildpb::LeaveGuildRequest;
using AsyncGuildServiceLeaveGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::LeaveGuildResponse&)>;
extern AsyncGuildServiceLeaveGuildHandlerFunctionType AsyncGuildServiceLeaveGuildHandler;

void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request);
void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::LeaveGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceLeaveGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region GuildServiceDisbandGuild

struct AsyncGuildServiceDisbandGuildGrpcClient {
    uint32_t messageId{ GuildServiceDisbandGuildMessageId };
    ClientContext context;
    Status status;
    ::guildpb::DisbandGuildResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::DisbandGuildResponse>> response_reader;
};

class ::guildpb::DisbandGuildRequest;
using AsyncGuildServiceDisbandGuildHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::DisbandGuildResponse&)>;
extern AsyncGuildServiceDisbandGuildHandlerFunctionType AsyncGuildServiceDisbandGuildHandler;

void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request);
void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::DisbandGuildRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceDisbandGuild(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region GuildServiceSetAnnouncement

struct AsyncGuildServiceSetAnnouncementGrpcClient {
    uint32_t messageId{ GuildServiceSetAnnouncementMessageId };
    ClientContext context;
    Status status;
    ::guildpb::SetAnnouncementResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::guildpb::SetAnnouncementResponse>> response_reader;
};

class ::guildpb::SetAnnouncementRequest;
using AsyncGuildServiceSetAnnouncementHandlerFunctionType =
    std::function<void(const ClientContext&, const ::guildpb::SetAnnouncementResponse&)>;
extern AsyncGuildServiceSetAnnouncementHandlerFunctionType AsyncGuildServiceSetAnnouncementHandler;

void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request);
void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const ::guildpb::SetAnnouncementRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendGuildServiceSetAnnouncement(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetGuildHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetGuildIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandleGuildCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitGuildGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace guildpb
