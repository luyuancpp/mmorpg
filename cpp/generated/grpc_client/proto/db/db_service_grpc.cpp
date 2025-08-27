#include "muduo/base/Logging.h"


#include "db_service_grpc.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "common/util/encode/base64.h"
#include <boost/pool/object_pool.hpp>

namespace  {
struct DbServiceCompleteQueue {
    grpc::CompletionQueue cq;
};

boost::object_pool<GrpcTag> tagPool;
#pragma region dbserviceTest
boost::object_pool<AsyncdbserviceTestGrpcClient> dbserviceTestPool;
using AsyncdbserviceTestHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncdbserviceTestHandlerFunctionType AsyncdbserviceTestHandler;

void AsyncCompleteGrpcdbserviceTest(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncdbserviceTestGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncdbserviceTestHandler) {
            AsyncdbserviceTestHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	dbserviceTestPool.destroy(call);
}



void SenddbserviceTest(entt::registry& registry, entt::entity nodeEntity, const ::Empty& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(dbserviceTestPool.construct());
    call->response_reader = registry
        .get<dbserviceStubPtr>(nodeEntity)
        ->PrepareAsyncTest(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(dbserviceTestMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SenddbserviceTest(entt::registry& registry, entt::entity nodeEntity, const ::Empty& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(dbserviceTestPool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<dbserviceStubPtr>(nodeEntity)
        ->PrepareAsyncTest(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(dbserviceTestMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SenddbserviceTest(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::Empty& derived = static_cast<const ::Empty&>(message);
    SenddbserviceTest(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion


void HandleDbServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag) {
        switch (grpcTag->messageId) {
        case dbserviceTestMessageId:
            AsyncCompleteGrpcdbserviceTest(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        default:
            break;
        }
}



void SetDbServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncdbserviceTestHandler = handler;
}


void SetDbServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncdbserviceTestHandler) {
        AsyncdbserviceTestHandler = handler;
    }
}


void InitDbServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<dbserviceStubPtr>(nodeEntity, dbservice::NewStub(channel));

}


}// namespace 
