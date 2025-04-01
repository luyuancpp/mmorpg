#include "muduo/base/Logging.h"

#include "grpc/generator/game_service_grpc.h"
#include "thread_local/storage.h"
struct GameServicePlayerEnterGameNodeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceSendMessageToPlayerCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceClientSendMessageToPlayerCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceRegisterGateNodeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceCentreSendToPlayerViaGameNodeCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceInvokePlayerServiceCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceRouteNodeStringMsgCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceRoutePlayerStringMsgCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceUpdateSessionDetailCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceEnterSceneCompleteQueue{
	grpc::CompletionQueue cq;
};
struct GameServiceCreateSceneCompleteQueue{
	grpc::CompletionQueue cq;
};

void GameServicePlayerEnterGameNode(entt::registry& registry, entt::entity nodeEntity, const PlayerEnterGameNodeRequest& request)
{
    AsyncGameServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGameServicePlayerEnterGameNodeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncPlayerEnterGameNode(&call->context, request,
		&registry.get<GameServicePlayerEnterGameNodeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall>&)> AsyncGameServicePlayerEnterGameNodeHandler;

void AsyncCompleteGrpcGameServicePlayerEnterGameNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall> call(static_cast<AsyncGameServicePlayerEnterGameNodeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServicePlayerEnterGameNodeHandler){
			AsyncGameServicePlayerEnterGameNodeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const NodeRouteMessageRequest& request)
{
    AsyncGameServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncSendMessageToPlayer(&call->context, request,
		&registry.get<GameServiceSendMessageToPlayerCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall>&)> AsyncGameServiceSendMessageToPlayerHandler;

void AsyncCompleteGrpcGameServiceSendMessageToPlayer(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall> call(static_cast<AsyncGameServiceSendMessageToPlayerGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceSendMessageToPlayerHandler){
			AsyncGameServiceSendMessageToPlayerHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceClientSendMessageToPlayer(entt::registry& registry, entt::entity nodeEntity, const ClientSendMessageToPlayerRequest& request)
{
    AsyncGameServiceClientSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceClientSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncClientSendMessageToPlayer(&call->context, request,
		&registry.get<GameServiceClientSendMessageToPlayerCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall>&)> AsyncGameServiceClientSendMessageToPlayerHandler;

void AsyncCompleteGrpcGameServiceClientSendMessageToPlayer(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall> call(static_cast<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceClientSendMessageToPlayerHandler){
			AsyncGameServiceClientSendMessageToPlayerHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceRegisterGateNode(entt::registry& registry, entt::entity nodeEntity, const RegisterGateNodeRequest& request)
{
    AsyncGameServiceRegisterGateNodeGrpcClientCall* call = new AsyncGameServiceRegisterGateNodeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncRegisterGateNode(&call->context, request,
		&registry.get<GameServiceRegisterGateNodeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceRegisterGateNodeGrpcClientCall>&)> AsyncGameServiceRegisterGateNodeHandler;

void AsyncCompleteGrpcGameServiceRegisterGateNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceRegisterGateNodeGrpcClientCall> call(static_cast<AsyncGameServiceRegisterGateNodeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceRegisterGateNodeHandler){
			AsyncGameServiceRegisterGateNodeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceCentreSendToPlayerViaGameNode(entt::registry& registry, entt::entity nodeEntity, const NodeRouteMessageRequest& request)
{
    AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall* call = new AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncCentreSendToPlayerViaGameNode(&call->context, request,
		&registry.get<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall>&)> AsyncGameServiceCentreSendToPlayerViaGameNodeHandler;

void AsyncCompleteGrpcGameServiceCentreSendToPlayerViaGameNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall> call(static_cast<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceCentreSendToPlayerViaGameNodeHandler){
			AsyncGameServiceCentreSendToPlayerViaGameNodeHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceInvokePlayerService(entt::registry& registry, entt::entity nodeEntity, const NodeRouteMessageRequest& request)
{
    AsyncGameServiceInvokePlayerServiceGrpcClientCall* call = new AsyncGameServiceInvokePlayerServiceGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncInvokePlayerService(&call->context, request,
		&registry.get<GameServiceInvokePlayerServiceCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall>&)> AsyncGameServiceInvokePlayerServiceHandler;

void AsyncCompleteGrpcGameServiceInvokePlayerService(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall> call(static_cast<AsyncGameServiceInvokePlayerServiceGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceInvokePlayerServiceHandler){
			AsyncGameServiceInvokePlayerServiceHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceRouteNodeStringMsg(entt::registry& registry, entt::entity nodeEntity, const RouteMessageRequest& request)
{
    AsyncGameServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncGameServiceRouteNodeStringMsgGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncRouteNodeStringMsg(&call->context, request,
		&registry.get<GameServiceRouteNodeStringMsgCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall>&)> AsyncGameServiceRouteNodeStringMsgHandler;

void AsyncCompleteGrpcGameServiceRouteNodeStringMsg(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall> call(static_cast<AsyncGameServiceRouteNodeStringMsgGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceRouteNodeStringMsgHandler){
			AsyncGameServiceRouteNodeStringMsgHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceRoutePlayerStringMsg(entt::registry& registry, entt::entity nodeEntity, const RoutePlayerMessageRequest& request)
{
    AsyncGameServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncGameServiceRoutePlayerStringMsgGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
		&registry.get<GameServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall>&)> AsyncGameServiceRoutePlayerStringMsgHandler;

void AsyncCompleteGrpcGameServiceRoutePlayerStringMsg(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall> call(static_cast<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceRoutePlayerStringMsgHandler){
			AsyncGameServiceRoutePlayerStringMsgHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceUpdateSessionDetail(entt::registry& registry, entt::entity nodeEntity, const RegisterPlayerSessionRequest& request)
{
    AsyncGameServiceUpdateSessionDetailGrpcClientCall* call = new AsyncGameServiceUpdateSessionDetailGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncUpdateSessionDetail(&call->context, request,
		&registry.get<GameServiceUpdateSessionDetailCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall>&)> AsyncGameServiceUpdateSessionDetailHandler;

void AsyncCompleteGrpcGameServiceUpdateSessionDetail(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall> call(static_cast<AsyncGameServiceUpdateSessionDetailGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceUpdateSessionDetailHandler){
			AsyncGameServiceUpdateSessionDetailHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceEnterScene(entt::registry& registry, entt::entity nodeEntity, const Centre2GsEnterSceneRequest& request)
{
    AsyncGameServiceEnterSceneGrpcClientCall* call = new AsyncGameServiceEnterSceneGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncEnterScene(&call->context, request,
		&registry.get<GameServiceEnterSceneCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall>&)> AsyncGameServiceEnterSceneHandler;

void AsyncCompleteGrpcGameServiceEnterScene(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall> call(static_cast<AsyncGameServiceEnterSceneGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceEnterSceneHandler){
			AsyncGameServiceEnterSceneHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void GameServiceCreateScene(entt::registry& registry, entt::entity nodeEntity, const CreateSceneRequest& request)
{
    AsyncGameServiceCreateSceneGrpcClientCall* call = new AsyncGameServiceCreateSceneGrpcClientCall;

    call->response_reader =
        registry.get<GrpcGameServiceStubPtr>(nodeEntity)->PrepareAsyncCreateScene(&call->context, request,
		&registry.get<GameServiceCreateSceneCompleteQueue>(nodeEntity).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall>&)> AsyncGameServiceCreateSceneHandler;

void AsyncCompleteGrpcGameServiceCreateScene(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		cq.AsyncNext(&got_tag, &ok, tm)){
        return;
    }

    std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall> call(static_cast<AsyncGameServiceCreateSceneGrpcClientCall*>(got_tag));
	if (!ok){
		LOG_ERROR << "RPC failed";
		return;
	}

    if (call->status.ok()){
		if(AsyncGameServiceCreateSceneHandler){
			AsyncGameServiceCreateSceneHandler(call);
		}
    }else{
        LOG_ERROR << call->status.error_message();
    }
}

void InitGameServiceCompletedQueue(entt::registry& registry, entt::entity nodeEntity) {
	registry.emplace<GameServicePlayerEnterGameNodeCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceSendMessageToPlayerCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceClientSendMessageToPlayerCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceRegisterGateNodeCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceInvokePlayerServiceCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceRouteNodeStringMsgCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceRoutePlayerStringMsgCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceUpdateSessionDetailCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceEnterSceneCompleteQueue>(nodeEntity);
	registry.emplace<GameServiceCreateSceneCompleteQueue>(nodeEntity);
}

void HandleGameServiceCompletedQueueMessage(entt::registry& registry) {
	{
		auto&& view = registry.view<GameServicePlayerEnterGameNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServicePlayerEnterGameNode(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceSendMessageToPlayerCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceSendMessageToPlayer(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceClientSendMessageToPlayerCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceClientSendMessageToPlayer(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceRegisterGateNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceRegisterGateNode(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceCentreSendToPlayerViaGameNode(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceInvokePlayerServiceCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceInvokePlayerService(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceRouteNodeStringMsgCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceRouteNodeStringMsg(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceRoutePlayerStringMsgCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceRoutePlayerStringMsg(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceUpdateSessionDetailCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceUpdateSessionDetail(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceEnterSceneCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceEnterScene(completeQueueComp.cq);
		}
	}
	{
		auto&& view = registry.view<GameServiceCreateSceneCompleteQueue>();
		for(auto&& [e, completeQueueComp] : view.each()){
			AsyncCompleteGrpcGameServiceCreateScene(completeQueueComp.cq);
		}
	}
}

