#pragma once

#include <grpcpp/grpcpp.h>

#include "common_proto/login_service.pb.h"
#include "common_proto/login_service.grpc.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

class LoginC2LRequestAsyncClientCall
{
public:
    ClientContext context;
    Status status;

    LoginResponse login_response_reply;
    std::unique_ptr<ClientAsyncResponseReader<LoginResponse>> login_response_reader;
};

