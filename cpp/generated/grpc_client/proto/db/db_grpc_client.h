#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_call_tag.h"

#include "proto/db/db_service.grpc.pb.h"


#include "service_info/db_service_service_info.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace  {
using dbserviceStubPtr = std::unique_ptr<dbservice::Stub>;
#pragma region dbserviceTest


struct AsyncdbserviceTestGrpcClient {
    uint32_t messageId{ dbserviceTestMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};



class ::Empty;
using AsyncdbserviceTestHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncdbserviceTestHandlerFunctionType AsyncdbserviceTestHandler;

void SenddbserviceTest(entt::registry& registry, entt::entity nodeEntity, const ::Empty& request);
void SenddbserviceTest(entt::registry& registry, entt::entity nodeEntity, const ::Empty& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SenddbserviceTest(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetDbServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetDbServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandleDbServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitDbServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace 

