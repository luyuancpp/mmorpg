#pragma once

#include <grpcpp/grpcpp.h>

using grpc::CompletionQueue;

extern std::unique_ptr<CompletionQueue> g_deploy_cq;

