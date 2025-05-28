#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/common/gate_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace  {

using GateServiceStubPtr = std::unique_ptr<GateService::Stub>;



class AsyncGateServicePlayerEnterGameNodeGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RegisterGameNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RegisterGameNodeSessionResponse>> response_reader;
};

using AsyncGateServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServicePlayerEnterGameNodeGrpcClientCall>&)>;
extern AsyncGateServicePlayerEnterGameNodeHandlerFunctionType AsyncGateServicePlayerEnterGameNodeHandler;



class ::RegisterGameNodeSessionRequest;

void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::RegisterGameNodeSessionRequest& request);
void SendGateServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::RegisterGameNodeSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGateServiceCompletedQueueMessage(entt::registry& registry);
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncGateServiceSendMessageToPlayerGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceSendMessageToPlayerGrpcClientCall>&)>;
extern AsyncGateServiceSendMessageToPlayerHandlerFunctionType AsyncGateServiceSendMessageToPlayerHandler;



class ::NodeRouteMessageRequest;

void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request);
void SendGateServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGateServiceCompletedQueueMessage(entt::registry& registry);
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncGateServiceKickSessionByCentreGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceKickSessionByCentreHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceKickSessionByCentreGrpcClientCall>&)>;
extern AsyncGateServiceKickSessionByCentreHandlerFunctionType AsyncGateServiceKickSessionByCentreHandler;



class ::KickSessionRequest;

void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const ::KickSessionRequest& request);
void SendGateServiceKickSessionByCentre(entt::registry& registry, entt::entity nodeEntity, const ::KickSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGateServiceCompletedQueueMessage(entt::registry& registry);
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncGateServiceRouteNodeMessageGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RouteMessageResponse>> response_reader;
};

using AsyncGateServiceRouteNodeMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRouteNodeMessageGrpcClientCall>&)>;
extern AsyncGateServiceRouteNodeMessageHandlerFunctionType AsyncGateServiceRouteNodeMessageHandler;



class ::RouteMessageRequest;

void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request);
void SendGateServiceRouteNodeMessage(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGateServiceCompletedQueueMessage(entt::registry& registry);
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncGateServiceRoutePlayerMessageGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RoutePlayerMessageResponse>> response_reader;
};

using AsyncGateServiceRoutePlayerMessageHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRoutePlayerMessageGrpcClientCall>&)>;
extern AsyncGateServiceRoutePlayerMessageHandlerFunctionType AsyncGateServiceRoutePlayerMessageHandler;



class ::RoutePlayerMessageRequest;

void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request);
void SendGateServiceRoutePlayerMessage(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGateServiceCompletedQueueMessage(entt::registry& registry);
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncGateServiceBroadcastToPlayersGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGateServiceBroadcastToPlayersHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceBroadcastToPlayersGrpcClientCall>&)>;
extern AsyncGateServiceBroadcastToPlayersHandlerFunctionType AsyncGateServiceBroadcastToPlayersHandler;



class ::BroadcastToPlayersRequest;

void SendGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const ::BroadcastToPlayersRequest& request);
void SendGateServiceBroadcastToPlayers(entt::registry& registry, entt::entity nodeEntity, const ::BroadcastToPlayersRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGateServiceCompletedQueueMessage(entt::registry& registry);
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncGateServiceRegisterNodeSessionGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RegisterNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RegisterNodeSessionResponse>> response_reader;
};

using AsyncGateServiceRegisterNodeSessionHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGateServiceRegisterNodeSessionGrpcClientCall>&)>;
extern AsyncGateServiceRegisterNodeSessionHandlerFunctionType AsyncGateServiceRegisterNodeSessionHandler;



class ::RegisterNodeSessionRequest;

void SendGateServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request);
void SendGateServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGateServiceCompletedQueueMessage(entt::registry& registry);
void InitGateServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

}// namespace 
