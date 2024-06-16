#pragma once
#include <grpcpp/grpcpp.h>

#include "type_define/type_define.h"

using grpc::CompletionQueue;


class LoginC2LRequest;
struct LoginC2LCompletionQueue
{
    CompletionQueue cq;
};

void SendLoginC2LRequest(entt::entity login_node, LoginC2LRequest& request);

class CreatePlayerC2LRequest;
struct CreatePlayerC2LCompletionQueue
{
    CompletionQueue cq;
};

void SendCreatePlayerC2LRequest(entt::entity login_node, CreatePlayerC2LRequest& request);
