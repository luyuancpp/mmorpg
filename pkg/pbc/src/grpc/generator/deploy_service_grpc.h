#pragma once

#include "proto/common/deploy_service.grpc.pb.h"
#include "proto/common/deploy_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
class AsyncDeployServiceGetNodeInfoGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    NodeInfoResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< NodeInfoResponse>> response_reader;
};

class NodeInfoRequest;
void DeployServiceGetNodeInfo(const NodeInfoRequest& request);
class AsyncDeployServiceGetIDGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    GetIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< GetIDResponse>> response_reader;
};

class GetIDRequest;
void DeployServiceGetID(const GetIDRequest& request);
class AsyncDeployServiceReleaseIDGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    ReleaseIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< ReleaseIDResponse>> response_reader;
};

class ReleaseIDRequest;
void DeployServiceReleaseID(const ReleaseIDRequest& request);
class AsyncDeployServiceRenewLeaseGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    RenewLeaseIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< RenewLeaseIDResponse>> response_reader;
};

class RenewLeaseIDRequest;
void DeployServiceRenewLease(const RenewLeaseIDRequest& request);

void HandleDeployServiceCompletedQueueMessage(); 
