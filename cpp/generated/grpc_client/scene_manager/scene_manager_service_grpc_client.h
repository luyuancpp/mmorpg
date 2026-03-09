#pragma once
#include <memory>
#include "entt/src/entt/entity/registry.hpp"
#include <boost/circular_buffer.hpp>
#include "grpc_client/grpc_call_tag.h"
#include "proto/scene_manager/scene_manager_service.grpc.pb.h"


#include "rpc/service_metadata/scene_manager_service_service_metadata.h"


using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

namespace scene_manager {
using SceneManagerStubPtr = std::unique_ptr<SceneManager::Stub>;
#pragma region SceneManagerCreateScene


struct AsyncSceneManagerCreateSceneGrpcClient {
    uint32_t messageId{ SceneManagerCreateSceneMessageId };
    ClientContext context;
    Status status;
    ::scene_manager::CreateSceneResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::scene_manager::CreateSceneResponse>> response_reader;
};



class ::scene_manager::CreateSceneRequest;
using AsyncSceneManagerCreateSceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::CreateSceneResponse&)>;
extern AsyncSceneManagerCreateSceneHandlerFunctionType AsyncSceneManagerCreateSceneHandler;

void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request);
void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::CreateSceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerCreateScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region SceneManagerDestroyScene


struct AsyncSceneManagerDestroySceneGrpcClient {
    uint32_t messageId{ SceneManagerDestroySceneMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};



class ::scene_manager::DestroySceneRequest;
using AsyncSceneManagerDestroySceneHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncSceneManagerDestroySceneHandlerFunctionType AsyncSceneManagerDestroySceneHandler;

void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request);
void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::DestroySceneRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerDestroyScene(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region SceneManagerEnterSceneByCentre


struct AsyncSceneManagerEnterSceneByCentreGrpcClient {
    uint32_t messageId{ SceneManagerEnterSceneByCentreMessageId };
    ClientContext context;
    Status status;
    ::scene_manager::EnterSceneByCentreResponse reply;
    std::unique_ptr<ClientAsyncResponseReader<::scene_manager::EnterSceneByCentreResponse>> response_reader;
};



class ::scene_manager::EnterSceneByCentreRequest;
using AsyncSceneManagerEnterSceneByCentreHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::EnterSceneByCentreResponse&)>;
extern AsyncSceneManagerEnterSceneByCentreHandlerFunctionType AsyncSceneManagerEnterSceneByCentreHandler;

void SendSceneManagerEnterSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneByCentreRequest& request);
void SendSceneManagerEnterSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::EnterSceneByCentreRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerEnterSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region SceneManagerLeaveSceneByCentre


struct AsyncSceneManagerLeaveSceneByCentreGrpcClient {
    uint32_t messageId{ SceneManagerLeaveSceneByCentreMessageId };
    ClientContext context;
    Status status;
    ::Empty reply;
    std::unique_ptr<ClientAsyncResponseReader<::Empty>> response_reader;
};



class ::scene_manager::LeaveSceneByCentreRequest;
using AsyncSceneManagerLeaveSceneByCentreHandlerFunctionType =
    std::function<void(const ClientContext&, const ::Empty&)>;
extern AsyncSceneManagerLeaveSceneByCentreHandlerFunctionType AsyncSceneManagerLeaveSceneByCentreHandler;

void SendSceneManagerLeaveSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneByCentreRequest& request);
void SendSceneManagerLeaveSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::LeaveSceneByCentreRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerLeaveSceneByCentre(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
#pragma region SceneManagerGateConnect


struct AsyncSceneManagerGateConnectGrpcClient {
    uint32_t messageId{ SceneManagerGateConnectMessageId };
    ClientContext context;
    Status status;
    ::scene_manager::GateCommand reply;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<::scene_manager::GateHeartbeat, ::scene_manager::GateCommand>> stream;
};

struct GateHeartbeatBuffer {
    boost::circular_buffer<::scene_manager::GateHeartbeat> pendingWritesBuffer{200};
};

struct GateHeartbeatWriteInProgress {
    bool isInProgress{false};
};



class ::scene_manager::GateHeartbeat;
using AsyncSceneManagerGateConnectHandlerFunctionType =
    std::function<void(const ClientContext&, const ::scene_manager::GateCommand&)>;
extern AsyncSceneManagerGateConnectHandlerFunctionType AsyncSceneManagerGateConnectHandler;

void SendSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::GateHeartbeat& request);
void SendSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, const ::scene_manager::GateHeartbeat& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
void SendSceneManagerGateConnect(entt::registry& registry, entt::entity nodeEntity, const google::protobuf::Message& message, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues);
#pragma endregion
void SetSceneManagerServiceHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void SetSceneManagerServiceIfEmptyHandler(const std::function<void(const ClientContext&, const ::google::protobuf::Message& reply)>& handler);
void HandleSceneManagerServiceCompletedQueueMessage(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& completeQueueComp, GrpcTag* grpcTag);
void InitSceneManagerServiceGrpcNode(const std::shared_ptr< ::grpc::ChannelInterface>& channel, entt::registry& registry, entt::entity nodeEntity);

}// namespace scene_manager

