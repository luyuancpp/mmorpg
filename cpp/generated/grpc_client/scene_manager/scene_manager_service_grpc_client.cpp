#include "muduo/base/Logging.h"


#include "scene_manager_service_grpc_client.h"
#include "proto/logic/constants/etcd_grpc.pb.h"
#include "core/utils/encode/base64.h"
#include <boost/pool/object_pool.hpp>
#include "grpc_call_tag.h"

namespace scene_manager {
struct SceneManagerServiceCompleteQueue {
    grpc::CompletionQueue cq;
};

boost::object_pool<GrpcTag> tagPool;
#pragma region SceneManagerCreateScene
boost::object_pool<AsyncSceneManagerCreateSceneGrpcClient> SceneManagerCreateScenePool;
using AsyncSceneManagerCreateSceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::CreateSceneResponse&)>;
AsyncSceneManagerCreateSceneHandlerFunctionType AsyncSceneManagerCreateSceneHandler;

void AsyncCompleteGrpcSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncSceneManagerCreateSceneGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncSceneManagerCreateSceneHandler) {
            AsyncSceneManagerCreateSceneHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	SceneManagerCreateScenePool.destroy(call);
}



void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(SceneManagerCreateScenePool.construct());
    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncCreateScene(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerCreateSceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(SceneManagerCreateScenePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncCreateScene(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerCreateSceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::CreateSceneRequest& derived = static_cast<const ::scene_manager::CreateSceneRequest&>(message);
    SendSceneManagerCreateScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerDestroyScene
boost::object_pool<AsyncSceneManagerDestroySceneGrpcClient> SceneManagerDestroyScenePool;
using AsyncSceneManagerDestroySceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncSceneManagerDestroySceneHandlerFunctionType AsyncSceneManagerDestroySceneHandler;

void AsyncCompleteGrpcSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncSceneManagerDestroySceneGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncSceneManagerDestroySceneHandler) {
            AsyncSceneManagerDestroySceneHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	SceneManagerDestroyScenePool.destroy(call);
}



void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(SceneManagerDestroyScenePool.construct());
    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncDestroyScene(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerDestroySceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(SceneManagerDestroyScenePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncDestroyScene(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerDestroySceneMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::DestroySceneRequest& derived = static_cast<const ::scene_manager::DestroySceneRequest&>(message);
    SendSceneManagerDestroyScene(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerEnterSceneByCentre
boost::object_pool<AsyncSceneManagerEnterSceneByCentreGrpcClient> SceneManagerEnterSceneByCentrePool;
using AsyncSceneManagerEnterSceneByCentreHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::EnterSceneByCentreResponse&)>;
AsyncSceneManagerEnterSceneByCentreHandlerFunctionType AsyncSceneManagerEnterSceneByCentreHandler;

void AsyncCompleteGrpcSceneManagerEnterSceneByCentre(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncSceneManagerEnterSceneByCentreGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncSceneManagerEnterSceneByCentreHandler) {
            AsyncSceneManagerEnterSceneByCentreHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	SceneManagerEnterSceneByCentrePool.destroy(call);
}



void SendSceneManagerEnterSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneByCentreRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(SceneManagerEnterSceneByCentrePool.construct());
    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncEnterSceneByCentre(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerEnterSceneByCentreMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendSceneManagerEnterSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneByCentreRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(SceneManagerEnterSceneByCentrePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncEnterSceneByCentre(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerEnterSceneByCentreMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendSceneManagerEnterSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::EnterSceneByCentreRequest& derived = static_cast<const ::scene_manager::EnterSceneByCentreRequest&>(message);
    SendSceneManagerEnterSceneByCentre(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerLeaveSceneByCentre
boost::object_pool<AsyncSceneManagerLeaveSceneByCentreGrpcClient> SceneManagerLeaveSceneByCentrePool;
using AsyncSceneManagerLeaveSceneByCentreHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
AsyncSceneManagerLeaveSceneByCentreHandlerFunctionType AsyncSceneManagerLeaveSceneByCentreHandler;

void AsyncCompleteGrpcSceneManagerLeaveSceneByCentre(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto call(
        static_cast<AsyncSceneManagerLeaveSceneByCentreGrpcClient*>(got_tag));
    if (call->status.ok()) {
        if (AsyncSceneManagerLeaveSceneByCentreHandler) {
            AsyncSceneManagerLeaveSceneByCentreHandler(call->context, call->reply);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }

	SceneManagerLeaveSceneByCentrePool.destroy(call);
}



void SendSceneManagerLeaveSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneByCentreRequest& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto call(SceneManagerLeaveSceneByCentrePool.construct());
    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveSceneByCentre(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerLeaveSceneByCentreMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}


void SendSceneManagerLeaveSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneByCentreRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto call(SceneManagerLeaveSceneByCentrePool.construct());
    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);

    const size_t count = std::min(metaKeys.size(), metaValues.size());
    for (size_t i = 0; i < count; ++i) {
        call->context.AddMetadata(metaKeys[i], Base64Encode(metaValues[i]));
    }

    call->response_reader = registry
        .get<SceneManagerStubPtr>(nodeEntity)
        ->PrepareAsyncLeaveSceneByCentre(&call->context, request,
                                           &cq);
    call->response_reader->StartCall();
    GrpcTag* got_tag(tagPool.construct(SceneManagerLeaveSceneByCentreMessageId, (void*)call));
    call->response_reader->Finish(&call->reply, &call->status, (void*)got_tag);

}

void SendSceneManagerLeaveSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::LeaveSceneByCentreRequest& derived = static_cast<const ::scene_manager::LeaveSceneByCentreRequest&>(message);
    SendSceneManagerLeaveSceneByCentre(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion
#pragma region SceneManagerGateConnect
boost::object_pool<AsyncSceneManagerGateConnectGrpcClient> SceneManagerGateConnectPool;
using AsyncSceneManagerGateConnectHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::GateCommand&)>;
AsyncSceneManagerGateConnectHandlerFunctionType AsyncSceneManagerGateConnectHandler;


void TryWriteNextNextSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    auto& writeInProgress = registry.get<GateHeartbeatWriteInProgress>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<GateHeartbeatBuffer>(nodeEntity).pendingWritesBuffer;

    if (writeInProgress.isInProgress || pendingWritesBuffer.empty()) {
        return;
    }

    auto& client = registry.get<AsyncSceneManagerGateConnectGrpcClient>(nodeEntity);
    auto& request = pendingWritesBuffer.front();

    writeInProgress.isInProgress = true;
    GrpcTag* got_tag(tagPool.construct(SceneManagerGateConnectMessageId,  (void*)GrpcOperation::WRITE));
    client.stream->Write(request, (void*)(got_tag));
}
void AsyncCompleteGrpcSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq, void* got_tag) {
    auto& client = registry.get<AsyncSceneManagerGateConnectGrpcClient>(nodeEntity);
    auto& writeInProgress = registry.get<GateHeartbeatWriteInProgress>(nodeEntity);

    switch (static_cast<GrpcOperation>(reinterpret_cast<intptr_t>(got_tag))) {
        case GrpcOperation::WRITE: {
            auto& pendingWritesBuffer = registry.get<GateHeartbeatBuffer>(nodeEntity).pendingWritesBuffer;
            if (!pendingWritesBuffer.empty()) {
                pendingWritesBuffer.pop_front();
            }
            writeInProgress.isInProgress = false;
            TryWriteNextNextSceneManagerGateConnect(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::WRITES_DONE: {
            GrpcTag* got_tag(tagPool.construct(SceneManagerGateConnectMessageId,  (void*)GrpcOperation::READ));
            client.stream->Finish(&client.status, (void*)(got_tag));
            break;
        }
        case GrpcOperation::FINISH:
            cq.Shutdown();
            break;
        case GrpcOperation::READ: {
            auto& response = registry.get<::scene_manager::GateCommand>(nodeEntity);
            if (AsyncSceneManagerGateConnectHandler) {
                AsyncSceneManagerGateConnectHandler(client.context, response);
            }
            GrpcTag* got_tag(tagPool.construct(SceneManagerGateConnectMessageId, (void*)GrpcOperation::READ));
            client.stream->Read(&response, (void*)got_tag);
            TryWriteNextNextSceneManagerGateConnect(registry, nodeEntity, cq);
            break;
        }
        case GrpcOperation::INIT: {
            GrpcTag* got_tag(tagPool.construct(SceneManagerGateConnectMessageId, (void*)GrpcOperation::READ));
            auto& response = registry.get<::scene_manager::GateCommand>(nodeEntity);
            client.stream->Read(&response, (void*)got_tag);
            TryWriteNextNextSceneManagerGateConnect(registry, nodeEntity, cq);
            break;
        }
        default:
            break;
    }
}


void SendSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::GateHeartbeat& request) {

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<GateHeartbeatBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextSceneManagerGateConnect(registry, nodeEntity, cq);

}


void SendSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::GateHeartbeat& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    auto& cq = registry.get<grpc::CompletionQueue>(nodeEntity);
    auto& pendingWritesBuffer = registry.get<GateHeartbeatBuffer>(nodeEntity).pendingWritesBuffer;
    pendingWritesBuffer.push_back(request);
    TryWriteNextNextSceneManagerGateConnect(registry, nodeEntity, cq);

}

void SendSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){
    const ::scene_manager::GateHeartbeat& derived = static_cast<const ::scene_manager::GateHeartbeat&>(message);
    SendSceneManagerGateConnect(registry, nodeEntity, derived, metaKeys, metaValues);
}
#pragma endregion


void HandleSceneManagerServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag) {
        switch (grpcTag->messageId) {
        case SceneManagerCreateSceneMessageId:
            AsyncCompleteGrpcSceneManagerCreateScene(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case SceneManagerDestroySceneMessageId:
            AsyncCompleteGrpcSceneManagerDestroyScene(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case SceneManagerEnterSceneByCentreMessageId:
            AsyncCompleteGrpcSceneManagerEnterSceneByCentre(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case SceneManagerLeaveSceneByCentreMessageId:
            AsyncCompleteGrpcSceneManagerLeaveSceneByCentre(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        case SceneManagerGateConnectMessageId:
            AsyncCompleteGrpcSceneManagerGateConnect(registry, nodeEntity, completeQueueComp, grpcTag->valuePtr);
			tagPool.destroy(grpcTag);
            break;
        default:
            break;
        }
}



void SetSceneManagerServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    AsyncSceneManagerCreateSceneHandler = handler;
    AsyncSceneManagerDestroySceneHandler = handler;
    AsyncSceneManagerEnterSceneByCentreHandler = handler;
    AsyncSceneManagerLeaveSceneByCentreHandler = handler;
    AsyncSceneManagerGateConnectHandler = handler;
}


void SetSceneManagerServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler) {

    if (!AsyncSceneManagerCreateSceneHandler) {
        AsyncSceneManagerCreateSceneHandler = handler;
    }
    if (!AsyncSceneManagerDestroySceneHandler) {
        AsyncSceneManagerDestroySceneHandler = handler;
    }
    if (!AsyncSceneManagerEnterSceneByCentreHandler) {
        AsyncSceneManagerEnterSceneByCentreHandler = handler;
    }
    if (!AsyncSceneManagerLeaveSceneByCentreHandler) {
        AsyncSceneManagerLeaveSceneByCentreHandler = handler;
    }
    if (!AsyncSceneManagerGateConnectHandler) {
        AsyncSceneManagerGateConnectHandler = handler;
    }
}


void InitSceneManagerServiceGrpcNode(const std::shared_ptr<::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity) {

    registry.emplace<SceneManagerStubPtr>(nodeEntity, SceneManager::NewStub(channel));
    {
        GrpcTag* got_tag(tagPool.construct(SceneManagerGateConnectMessageId, (void*)GrpcOperation::INIT));

        auto& client = registry.emplace<AsyncSceneManagerGateConnectGrpcClient>(nodeEntity);
        registry.emplace<GateHeartbeatBuffer>(nodeEntity);
        registry.emplace<GateHeartbeatWriteInProgress>(nodeEntity);
        registry.emplace<::scene_manager::GateCommand>(nodeEntity);
        registry.emplace<::scene_manager::GateHeartbeat>(nodeEntity);

        client.stream = registry
            .get<SceneManagerStubPtr>(nodeEntity)
            ->AsyncGateConnect(&client.context,
                                        &registry.get<grpc::CompletionQueue>(nodeEntity),
                                        (void*)(got_tag));
    }

}


}// namespace scene_manager
