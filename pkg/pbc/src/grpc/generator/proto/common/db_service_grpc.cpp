#include "muduo/base/Logging.h"
#include "db_service_grpc.h"
#include "thread_local/storage.h"
#include "proto/logic/constants/etcd_grpc.pb.h"

namespace {

struct AccountDBServiceLoad2RedisCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncAccountDBServiceLoad2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall>&)>;
AsyncAccountDBServiceLoad2RedisHandlerFunctionType AsyncAccountDBServiceLoad2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncAccountDBServiceLoad2RedisGrpcClientCall> call(
        static_cast<AsyncAccountDBServiceLoad2RedisGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncAccountDBServiceLoad2RedisHandler) {
            AsyncAccountDBServiceLoad2RedisHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request) {

    AsyncAccountDBServiceLoad2RedisGrpcClientCall* call = new AsyncAccountDBServiceLoad2RedisGrpcClientCall;
    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                  &registry.get<AccountDBServiceLoad2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendAccountDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncAccountDBServiceLoad2RedisGrpcClientCall* call = new AsyncAccountDBServiceLoad2RedisGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                  &registry.get<AccountDBServiceLoad2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct AccountDBServiceSave2RedisCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncAccountDBServiceSave2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall>&)>;
AsyncAccountDBServiceSave2RedisHandlerFunctionType AsyncAccountDBServiceSave2RedisHandler;

void AsyncCompleteGrpcAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncAccountDBServiceSave2RedisGrpcClientCall> call(
        static_cast<AsyncAccountDBServiceSave2RedisGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncAccountDBServiceSave2RedisHandler) {
            AsyncAccountDBServiceSave2RedisHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request) {

    AsyncAccountDBServiceSave2RedisGrpcClientCall* call = new AsyncAccountDBServiceSave2RedisGrpcClientCall;
    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                  &registry.get<AccountDBServiceSave2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendAccountDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SaveAccountRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncAccountDBServiceSave2RedisGrpcClientCall* call = new AsyncAccountDBServiceSave2RedisGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<AccountDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                  &registry.get<AccountDBServiceSave2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct PlayerDBServiceLoad2RedisCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncPlayerDBServiceLoad2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncPlayerDBServiceLoad2RedisGrpcClientCall>&)>;
AsyncPlayerDBServiceLoad2RedisHandlerFunctionType AsyncPlayerDBServiceLoad2RedisHandler;

void AsyncCompleteGrpcPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncPlayerDBServiceLoad2RedisGrpcClientCall> call(
        static_cast<AsyncPlayerDBServiceLoad2RedisGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncPlayerDBServiceLoad2RedisHandler) {
            AsyncPlayerDBServiceLoad2RedisHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request) {

    AsyncPlayerDBServiceLoad2RedisGrpcClientCall* call = new AsyncPlayerDBServiceLoad2RedisGrpcClientCall;
    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                  &registry.get<PlayerDBServiceLoad2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendPlayerDBServiceLoad2Redis(entt::registry& registry, entt::entity nodeEntity, const ::LoadPlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncPlayerDBServiceLoad2RedisGrpcClientCall* call = new AsyncPlayerDBServiceLoad2RedisGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncLoad2Redis(&call->context, request,
                                  &registry.get<PlayerDBServiceLoad2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

struct PlayerDBServiceSave2RedisCompleteQueue {
    grpc::CompletionQueue cq;
};



using AsyncPlayerDBServiceSave2RedisHandlerFunctionType = std::function<void(const std::unique_ptr<AsyncPlayerDBServiceSave2RedisGrpcClientCall>&)>;
AsyncPlayerDBServiceSave2RedisHandlerFunctionType AsyncPlayerDBServiceSave2RedisHandler;

void AsyncCompleteGrpcPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, grpc::CompletionQueue& cq) {
    void* got_tag = nullptr;
    bool ok = false;
    gpr_timespec tm = {0, 0, GPR_CLOCK_MONOTONIC};
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm)) {
        return;
    }
    if (!ok) {
        LOG_ERROR << "RPC failed";
        return;
    }

    std::unique_ptr<AsyncPlayerDBServiceSave2RedisGrpcClientCall> call(
        static_cast<AsyncPlayerDBServiceSave2RedisGrpcClientCall*>(got_tag));
    if (call->status.ok()) {
        if (AsyncPlayerDBServiceSave2RedisHandler) {
            AsyncPlayerDBServiceSave2RedisHandler(call);
        }
    } else {
        LOG_ERROR << call->status.error_message();
    }
}



void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request) {

    AsyncPlayerDBServiceSave2RedisGrpcClientCall* call = new AsyncPlayerDBServiceSave2RedisGrpcClientCall;
    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                  &registry.get<PlayerDBServiceSave2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}

void SendPlayerDBServiceSave2Redis(entt::registry& registry, entt::entity nodeEntity, const ::SavePlayerRequest& request, const std::vector<std::string>& metaKeys, const std::vector<std::string>& metaValues){

    AsyncPlayerDBServiceSave2RedisGrpcClientCall* call = new AsyncPlayerDBServiceSave2RedisGrpcClientCall;

	for (uint32_t i = 0; i < metaKeys.size() && i < metaValues.size(); ++i)
	{
		call->context.AddMetadata(metaKeys[i], metaValues[i]);
	}

    call->response_reader = registry
        .get<PlayerDBServiceStubPtr>(nodeEntity)
        ->PrepareAsyncSave2Redis(&call->context, request,
                                  &registry.get<PlayerDBServiceSave2RedisCompleteQueue>(nodeEntity).cq);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);

}
void InitAccountDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<AccountDBServiceLoad2RedisCompleteQueue>(nodeEntity);

    registry.emplace<AccountDBServiceSave2RedisCompleteQueue>(nodeEntity);

}
void InitPlayerDBServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
    registry.emplace<PlayerDBServiceLoad2RedisCompleteQueue>(nodeEntity);

    registry.emplace<PlayerDBServiceSave2RedisCompleteQueue>(nodeEntity);

}
void HandleAccountDBServiceCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<AccountDBServiceLoad2RedisCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcAccountDBServiceLoad2Redis(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<AccountDBServiceSave2RedisCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcAccountDBServiceSave2Redis(registry, e, completeQueueComp.cq);
        }
    }
}
void HandlePlayerDBServiceCompletedQueueMessage(entt::registry& registry) {
    {
        auto&& view = registry.view<PlayerDBServiceLoad2RedisCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcPlayerDBServiceLoad2Redis(registry, e, completeQueueComp.cq);
        }
    }
    {
        auto&& view = registry.view<PlayerDBServiceSave2RedisCompleteQueue>();
        for (auto&& [e, completeQueueComp] : view.each()) {
            AsyncCompleteGrpcPlayerDBServiceSave2Redis(registry, e, completeQueueComp.cq);
        }
    }
}

}// namespace 
