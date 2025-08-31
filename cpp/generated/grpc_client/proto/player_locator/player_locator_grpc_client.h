#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_client/grpc_call_tag.h"
#include "proto/player_locator/player_locator.grpc.pb.h"


#include "rpc/service_info/player_locator_service_info.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace playerlocator {
using PlayerLocatorStubPtr = std::unique_ptr<PlayerLocator::Stub>;
#pragma region PlayerLocatorSetLocation


struct AsyncPlayerLocatorSetLocationGrpcClient {
    uint32_t messageId{ PlayerLocatorSetLocationMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};



class ::playerlocator::PlayerLocation;
using AsyncPlayerLocatorSetLocationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncPlayerLocatorSetLocationHandlerFunctionType AsyncPlayerLocatorSetLocationHandler;

void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request);
void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerLocation& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorSetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region PlayerLocatorGetLocation


struct AsyncPlayerLocatorGetLocationGrpcClient {
    uint32_t messageId{ PlayerLocatorGetLocationMessageId };
    ClientContext context;
    Status status;
    ::playerlocator::PlayerLocation reply;
    std::unique_ptr<ClientAsyncResponseReader<::playerlocator::PlayerLocation>> response_reader;
};



class ::playerlocator::PlayerId;
using AsyncPlayerLocatorGetLocationHandlerFunctionType =
    std::function<void(const ClientContext&, const ::playerlocator::PlayerLocation&)>;
extern AsyncPlayerLocatorGetLocationHandlerFunctionType AsyncPlayerLocatorGetLocationHandler;

void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request);
void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorGetLocation(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region PlayerLocatorMarkOffline


struct AsyncPlayerLocatorMarkOfflineGrpcClient {
    uint32_t messageId{ PlayerLocatorMarkOfflineMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};



class ::playerlocator::PlayerId;
using AsyncPlayerLocatorMarkOfflineHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncPlayerLocatorMarkOfflineHandlerFunctionType AsyncPlayerLocatorMarkOfflineHandler;

void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request);
void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const ::playerlocator::PlayerId& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendPlayerLocatorMarkOffline(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetPlayerLocatorHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetPlayerLocatorIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandlePlayerLocatorCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitPlayerLocatorGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace playerlocator

