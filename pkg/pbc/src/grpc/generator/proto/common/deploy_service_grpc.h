#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "proto/common/deploy_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcDeployServiceStubPtr = std::unique_ptr<::DeployService::Stub>;



class AsyncDeployServiceGetNodeInfoGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::NodeInfoResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::NodeInfoResponse>> response_reader;
};

using AsyncDeployServiceGetNodeInfoHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)>;
extern AsyncDeployServiceGetNodeInfoHandlerFunctionType AsyncDeployServiceGetNodeInfoHandler;



class ::NodeInfoRequest;

void SendDeployServiceGetNodeInfo(entt::registry& registry, entt::entity nodeEntity, const ::NodeInfoRequest& request);
void HandleDeployServiceCompletedQueueMessage(entt::registry& registry);
void InitDeployServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncDeployServiceGetIDGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::GetIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::GetIDResponse>> response_reader;
};

using AsyncDeployServiceGetIDHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceGetIDGrpcClientCall>&)>;
extern AsyncDeployServiceGetIDHandlerFunctionType AsyncDeployServiceGetIDHandler;



class ::GetIDRequest;

void SendDeployServiceGetID(entt::registry& registry, entt::entity nodeEntity, const ::GetIDRequest& request);
void HandleDeployServiceCompletedQueueMessage(entt::registry& registry);
void InitDeployServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncDeployServiceReleaseIDGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::ReleaseIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::ReleaseIDResponse>> response_reader;
};

using AsyncDeployServiceReleaseIDHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceReleaseIDGrpcClientCall>&)>;
extern AsyncDeployServiceReleaseIDHandlerFunctionType AsyncDeployServiceReleaseIDHandler;



class ::ReleaseIDRequest;

void SendDeployServiceReleaseID(entt::registry& registry, entt::entity nodeEntity, const ::ReleaseIDRequest& request);
void HandleDeployServiceCompletedQueueMessage(entt::registry& registry);
void InitDeployServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);



class AsyncDeployServiceRenewLeaseGrpcClientCall {
public:
    ClientContext context;
    Status status;
    ::RenewLeaseIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::RenewLeaseIDResponse>> response_reader;
};

using AsyncDeployServiceRenewLeaseHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceRenewLeaseGrpcClientCall>&)>;
extern AsyncDeployServiceRenewLeaseHandlerFunctionType AsyncDeployServiceRenewLeaseHandler;



class ::RenewLeaseIDRequest;

void SendDeployServiceRenewLease(entt::registry& registry, entt::entity nodeEntity, const ::RenewLeaseIDRequest& request);
void HandleDeployServiceCompletedQueueMessage(entt::registry& registry);
void InitDeployServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

