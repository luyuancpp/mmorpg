#pragma once

#include "proto/common/deploy_service.grpc.pb.h"
#include "proto/common/deploy_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

class DeployAsyncClientCall
{
public:
    ClientContext context;
    Status status;

    NodeInfoResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<NodeInfoResponse>> response_reader;
};

