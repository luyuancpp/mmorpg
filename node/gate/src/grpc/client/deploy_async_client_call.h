#pragma once

#include <grpcpp/grpcpp.h>

#include "common_proto/deploy_service.pb.h"
#include "common_proto/deploy_service.grpc.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

class AsyncClientCall
{
public:
    ClientContext context;
    Status status;

    NodeInfoResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<NodeInfoResponse>> response_reader;
};

