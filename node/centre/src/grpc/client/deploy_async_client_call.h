#pragma once

#include "proto/common/deploy_service.grpc.pb.h"
#include "proto/common/deploy_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

class DeployAsyncGetNodeInfoClientCall
{
public:
    ClientContext context;
    Status status;

    NodeInfoResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<NodeInfoResponse>> response_reader;
};

class DeployAsyncGetIDClientCall
{
public:
    ClientContext context;
    Status status;

    GetIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<GetIDResponse>> response_reader;
};

class DeployAsyncReleaseIDClientCall
{
public:
    ClientContext context;
    Status status;

    ReleaseIDResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<ReleaseIDResponse>> response_reader;
};