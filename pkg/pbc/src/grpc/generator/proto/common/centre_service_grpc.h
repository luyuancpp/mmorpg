#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/common/centre_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcCentreServiceStubPtr = std::unique_ptr<::CentreService::Stub>;



class AsyncCentreServiceGatePlayerServiceGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncCentreServiceGatePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceGatePlayerServiceGrpcClientCall>&)>;
extern AsyncCentreServiceGatePlayerServiceHandlerFunctionType AsyncCentreServiceGatePlayerServiceHandler;



class ::GateClientMessageRequest;

void SendCentreServiceGatePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::GateClientMessageRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceGateSessionDisconnectGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncCentreServiceGateSessionDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceGateSessionDisconnectGrpcClientCall>&)>;
extern AsyncCentreServiceGateSessionDisconnectHandlerFunctionType AsyncCentreServiceGateSessionDisconnectHandler;



class ::GateSessionDisconnectRequest;

void SendCentreServiceGateSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::GateSessionDisconnectRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::LoginResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::LoginResponse>> response_reader;
};

using AsyncCentreServiceLoginNodeAccountLoginHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeAccountLoginGrpcClientCall>&)>;
extern AsyncCentreServiceLoginNodeAccountLoginHandlerFunctionType AsyncCentreServiceLoginNodeAccountLoginHandler;



class ::LoginRequest;

void SendCentreServiceLoginNodeAccountLogin(entt::registry& registry, entt::entity nodeEntity, const ::LoginRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceLoginNodeEnterGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncCentreServiceLoginNodeEnterGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeEnterGameGrpcClientCall>&)>;
extern AsyncCentreServiceLoginNodeEnterGameHandlerFunctionType AsyncCentreServiceLoginNodeEnterGameHandler;



class ::CentrePlayerGameNodeEntryRequest;

void SendCentreServiceLoginNodeEnterGame(entt::registry& registry, entt::entity nodeEntity, const ::CentrePlayerGameNodeEntryRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncCentreServiceLoginNodeLeaveGameHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeLeaveGameGrpcClientCall>&)>;
extern AsyncCentreServiceLoginNodeLeaveGameHandlerFunctionType AsyncCentreServiceLoginNodeLeaveGameHandler;



class ::LoginNodeLeaveGameRequest;

void SendCentreServiceLoginNodeLeaveGame(entt::registry& registry, entt::entity nodeEntity, const ::LoginNodeLeaveGameRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncCentreServiceLoginNodeSessionDisconnectHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceLoginNodeSessionDisconnectGrpcClientCall>&)>;
extern AsyncCentreServiceLoginNodeSessionDisconnectHandlerFunctionType AsyncCentreServiceLoginNodeSessionDisconnectHandler;



class ::GateSessionDisconnectRequest;

void SendCentreServiceLoginNodeSessionDisconnect(entt::registry& registry, entt::entity nodeEntity, const ::GateSessionDisconnectRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServicePlayerServiceGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::NodeRouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::NodeRouteMessageResponse>> response_reader;
};

using AsyncCentreServicePlayerServiceHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServicePlayerServiceGrpcClientCall>&)>;
extern AsyncCentreServicePlayerServiceHandlerFunctionType AsyncCentreServicePlayerServiceHandler;



class ::NodeRouteMessageRequest;

void SendCentreServicePlayerService(entt::registry& registry, entt::entity nodeEntity, const ::NodeRouteMessageRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceEnterGsSucceedGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncCentreServiceEnterGsSucceedHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceEnterGsSucceedGrpcClientCall>&)>;
extern AsyncCentreServiceEnterGsSucceedHandlerFunctionType AsyncCentreServiceEnterGsSucceedHandler;



class ::EnterGameNodeSuccessRequest;

void SendCentreServiceEnterGsSucceed(entt::registry& registry, entt::entity nodeEntity, const ::EnterGameNodeSuccessRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceRouteNodeStringMsgGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RouteMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RouteMessageResponse>> response_reader;
};

using AsyncCentreServiceRouteNodeStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRouteNodeStringMsgGrpcClientCall>&)>;
extern AsyncCentreServiceRouteNodeStringMsgHandlerFunctionType AsyncCentreServiceRouteNodeStringMsgHandler;



class ::RouteMessageRequest;

void SendCentreServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RouteMessageRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RoutePlayerMessageResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RoutePlayerMessageResponse>> response_reader;
};

using AsyncCentreServiceRoutePlayerStringMsgHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRoutePlayerStringMsgGrpcClientCall>&)>;
extern AsyncCentreServiceRoutePlayerStringMsgHandlerFunctionType AsyncCentreServiceRoutePlayerStringMsgHandler;



class ::RoutePlayerMessageRequest;

void SendCentreServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const ::RoutePlayerMessageRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceInitSceneNodeGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};

using AsyncCentreServiceInitSceneNodeHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceInitSceneNodeGrpcClientCall>&)>;
extern AsyncCentreServiceInitSceneNodeHandlerFunctionType AsyncCentreServiceInitSceneNodeHandler;



class ::InitSceneNodeRequest;

void SendCentreServiceInitSceneNode(entt::registry& registry, entt::entity nodeEntity, const ::InitSceneNodeRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncCentreServiceRegisterNodeSessionGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RegisterNodeSessionResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RegisterNodeSessionResponse>> response_reader;
};

using AsyncCentreServiceRegisterNodeSessionHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncCentreServiceRegisterNodeSessionGrpcClientCall>&)>;
extern AsyncCentreServiceRegisterNodeSessionHandlerFunctionType AsyncCentreServiceRegisterNodeSessionHandler;



class ::RegisterNodeSessionRequest;

void SendCentreServiceRegisterNodeSession(entt::registry& registry, entt::entity nodeEntity, const ::RegisterNodeSessionRequest& request);
void HandleCentreServiceCompletedQueueMessage(entt::registry& registry);
void InitCentreServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

