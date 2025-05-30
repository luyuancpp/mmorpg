#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/common/game_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace  {
using GameServiceStubPtr = std::unique_ptr<GameService::Stub>;
#pragma region GameServicePlayerEnterGameNode

class AsyncGameServicePlayerEnterGameNodeGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGameServicePlayerEnterGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall>&)>;
extern AsyncGameServicePlayerEnterGameNodeHandlerFunctionType AsyncGameServicePlayerEnterGameNodeHandler;


class ::PlayerEnterGameNodeRequest;

void SendGameServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::PlayerEnterGameNodeRequest& request);
void SendGameServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const ::PlayerEnterGameNodeRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceSendMessageToPlayer

class AsyncGameServiceSendMessageToPlayerGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::NodeRouteMessageResponse>> response_reader;
};

using AsyncGameServiceSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall>&)>;
extern AsyncGameServiceSendMessageToPlayerHandlerFunctionType AsyncGameServiceSendMessageToPlayerHandler;


class ::NodeRouteMessageRequest;

void SendGameServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request);
void SendGameServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceClientSendMessageToPlayer

class AsyncGameServiceClientSendMessageToPlayerGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::ClientSendMessageToPlayerResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::ClientSendMessageToPlayerResponse>> response_reader;
};

using AsyncGameServiceClientSendMessageToPlayerHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall>&)>;
extern AsyncGameServiceClientSendMessageToPlayerHandlerFunctionType AsyncGameServiceClientSendMessageToPlayerHandler;


class ::ClientSendMessageToPlayerRequest;

void SendGameServiceClientSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::ClientSendMessageToPlayerRequest& request);
void SendGameServiceClientSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ::ClientSendMessageToPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceCentreSendToPlayerViaGameNode

class AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGameServiceCentreSendToPlayerViaGameNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall>&)>;
extern AsyncGameServiceCentreSendToPlayerViaGameNodeHandlerFunctionType AsyncGameServiceCentreSendToPlayerViaGameNodeHandler;


class ::NodeRouteMessageRequest;

void SendGameServiceCentreSendToPlayerViaGameNode(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request);
void SendGameServiceCentreSendToPlayerViaGameNode(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceInvokePlayerService

class AsyncGameServiceInvokePlayerServiceGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::NodeRouteMessageResponse>> response_reader;
};

using AsyncGameServiceInvokePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall>&)>;
extern AsyncGameServiceInvokePlayerServiceHandlerFunctionType AsyncGameServiceInvokePlayerServiceHandler;


class ::NodeRouteMessageRequest;

void SendGameServiceInvokePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request);
void SendGameServiceInvokePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceRouteNodeStringMsg

class AsyncGameServiceRouteNodeStringMsgGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RouteMessageResponse>> response_reader;
};

using AsyncGameServiceRouteNodeStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall>&)>;
extern AsyncGameServiceRouteNodeStringMsgHandlerFunctionType AsyncGameServiceRouteNodeStringMsgHandler;


class ::RouteMessageRequest;

void SendGameServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request);
void SendGameServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceRoutePlayerStringMsg

class AsyncGameServiceRoutePlayerStringMsgGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RoutePlayerMessageResponse>> response_reader;
};

using AsyncGameServiceRoutePlayerStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall>&)>;
extern AsyncGameServiceRoutePlayerStringMsgHandlerFunctionType AsyncGameServiceRoutePlayerStringMsgHandler;


class ::RoutePlayerMessageRequest;

void SendGameServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request);
void SendGameServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceUpdateSessionDetail

class AsyncGameServiceUpdateSessionDetailGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGameServiceUpdateSessionDetailHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall>&)>;
extern AsyncGameServiceUpdateSessionDetailHandlerFunctionType AsyncGameServiceUpdateSessionDetailHandler;


class ::RegisterPlayerSessionRequest;

void SendGameServiceUpdateSessionDetail(entt::registry& registry, entt::entity nodeEntity, const ::RegisterPlayerSessionRequest& request);
void SendGameServiceUpdateSessionDetail(entt::registry& registry, entt::entity nodeEntity, const ::RegisterPlayerSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceEnterScene

class AsyncGameServiceEnterSceneGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncGameServiceEnterSceneHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall>&)>;
extern AsyncGameServiceEnterSceneHandlerFunctionType AsyncGameServiceEnterSceneHandler;


class ::Centre2GsEnterSceneRequest;

void SendGameServiceEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::Centre2GsEnterSceneRequest& request);
void SendGameServiceEnterScene(entt::registry& registry, entt::entity nodeEntity, const ::Centre2GsEnterSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceCreateScene

class AsyncGameServiceCreateSceneGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::CreateSceneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::CreateSceneResponse>> response_reader;
};

using AsyncGameServiceCreateSceneHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall>&)>;
extern AsyncGameServiceCreateSceneHandlerFunctionType AsyncGameServiceCreateSceneHandler;


class ::CreateSceneRequest;

void SendGameServiceCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request);
void SendGameServiceCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion


#pragma region GameServiceRegisterNodeSession

class AsyncGameServiceRegisterNodeSessionGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RegisterNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RegisterNodeSessionResponse>> response_reader;
};

using AsyncGameServiceRegisterNodeSessionHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncGameServiceRegisterNodeSessionGrpcClientCall>&)>;
extern AsyncGameServiceRegisterNodeSessionHandlerFunctionType AsyncGameServiceRegisterNodeSessionHandler;


class ::RegisterNodeSessionRequest;

void SendGameServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request);
void SendGameServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void HandleGameServiceCompletedQueueMessage(entt::registry& registry);
void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

#pragma endregion



}// namespace 
