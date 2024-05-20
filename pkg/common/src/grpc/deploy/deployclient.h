#pragma once

#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

class DeployClient
{
public:
    CompletionQueue& cq() { return cq_; }
private:
    CompletionQueue cq_;
};

extern std::unique_ptr<DeployClient> g_deploy_client;

