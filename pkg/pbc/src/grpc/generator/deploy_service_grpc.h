#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"

#include "proto/common/deploy_service.grpc.pb.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

using GrpcDeployServiceStubPtr = std::unique_ptr<::DeployService::Stub>;
class AsyncDeployServiceGetNodeInfoGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::NodeInfoResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::NodeInfoResponse>> response_reader;
};

class ::NodeInfoRequest;
void SendDeployServiceGetNodeInfo(entt::registry& registry, entt::entity nodeEntity, const  ::NodeInfoRequest& request);

using AsyncDeployServiceGetNodeInfoHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceGetNodeInfoGrpcClientCall>&)>;

extern AsyncDeployServiceGetNodeInfoHandlerFunctionType  AsyncDeployServiceGetNodeInfoHandler;;
class AsyncDeployServiceGetIDGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::GetIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::GetIDResponse>> response_reader;
};

class ::GetIDRequest;
void SendDeployServiceGetID(entt::registry& registry, entt::entity nodeEntity, const  ::GetIDRequest& request);

using AsyncDeployServiceGetIDHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceGetIDGrpcClientCall>&)>;

extern AsyncDeployServiceGetIDHandlerFunctionType  AsyncDeployServiceGetIDHandler;;
class AsyncDeployServiceReleaseIDGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::ReleaseIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::ReleaseIDResponse>> response_reader;
};

class ::ReleaseIDRequest;
void SendDeployServiceReleaseID(entt::registry& registry, entt::entity nodeEntity, const  ::ReleaseIDRequest& request);

using AsyncDeployServiceReleaseIDHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceReleaseIDGrpcClientCall>&)>;

extern AsyncDeployServiceReleaseIDHandlerFunctionType  AsyncDeployServiceReleaseIDHandler;;
class AsyncDeployServiceRenewLeaseGrpcClientCall
{
public:
    ClientContext context;
    Status status;
    ::RenewLeaseIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<  ::RenewLeaseIDResponse>> response_reader;
};

class ::RenewLeaseIDRequest;
void SendDeployServiceRenewLease(entt::registry& registry, entt::entity nodeEntity, const  ::RenewLeaseIDRequest& request);

using AsyncDeployServiceRenewLeaseHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncDeployServiceRenewLeaseGrpcClientCall>&)>;

extern AsyncDeployServiceRenewLeaseHandlerFunctionType  AsyncDeployServiceRenewLeaseHandler;;

void HandleDeployServiceCompletedQueueMessage(entt::registry& registry	); 

void InitDeployServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity);


