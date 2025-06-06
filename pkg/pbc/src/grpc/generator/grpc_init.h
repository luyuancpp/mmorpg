#pragma once

#include <functional>
#include "entt/src/entt/entity/registry.hpp"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/message.h>

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

void SetIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);

void SetHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);

void InitCompletedQueue(entt::registry& registry, entt::entity nodeEntity);

void HandleCompletedQueueMessage(entt::registry& registry);

void InitStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);