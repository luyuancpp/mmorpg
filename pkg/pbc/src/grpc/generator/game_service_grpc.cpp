#include "muduo/base/Logging.h"

#include "grpc/generator/game_service_grpc.h"
#include "thread_local/storage.h"

using GrpcGameServiceStubPtr = std::unique_ptr<GameService::Stub>;
GrpcGameServiceStubPtr gGameServiceStub;

entt::entity GlobalGrpcNodeEntity();
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

void GameServicePlayerEnterGameNode(const PlayerEnterGameNodeRequest& request)
{
    AsyncGameServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGameServicePlayerEnterGameNodeGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncPlayerEnterGameNode(&call->context, request,
		&tls.grpc_node_registry.get<GameServicePlayerEnterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall>&)> AsyncGameServicePlayerEnterGameNodeHandler;

void AsyncCompleteGrpcGameServicePlayerEnterGameNode()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServicePlayerEnterGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceSendMessageToPlayer(const NodeRouteMessageRequest& request)
{
    AsyncGameServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncSendMessageToPlayer(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall>&)> AsyncGameServiceSendMessageToPlayerHandler;

void AsyncCompleteGrpcGameServiceSendMessageToPlayer()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceClientSendMessageToPlayer(const ClientSendMessageToPlayerRequest& request)
{
    AsyncGameServiceClientSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceClientSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncClientSendMessageToPlayer(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceClientSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall>&)> AsyncGameServiceClientSendMessageToPlayerHandler;

void AsyncCompleteGrpcGameServiceClientSendMessageToPlayer()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceClientSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceRegisterGateNode(const RegisterGateNodeRequest& request)
{
    AsyncGameServiceRegisterGateNodeGrpcClientCall* call = new AsyncGameServiceRegisterGateNodeGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncRegisterGateNode(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceRegisterGateNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceRegisterGateNodeGrpcClientCall>&)> AsyncGameServiceRegisterGateNodeHandler;

void AsyncCompleteGrpcGameServiceRegisterGateNode()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceRegisterGateNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceCentreSendToPlayerViaGameNode(const NodeRouteMessageRequest& request)
{
    AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall* call = new AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncCentreSendToPlayerViaGameNode(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall>&)> AsyncGameServiceCentreSendToPlayerViaGameNodeHandler;

void AsyncCompleteGrpcGameServiceCentreSendToPlayerViaGameNode()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceInvokePlayerService(const NodeRouteMessageRequest& request)
{
    AsyncGameServiceInvokePlayerServiceGrpcClientCall* call = new AsyncGameServiceInvokePlayerServiceGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncInvokePlayerService(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceInvokePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall>&)> AsyncGameServiceInvokePlayerServiceHandler;

void AsyncCompleteGrpcGameServiceInvokePlayerService()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceInvokePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceRouteNodeStringMsg(const RouteMessageRequest& request)
{
    AsyncGameServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncGameServiceRouteNodeStringMsgGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncRouteNodeStringMsg(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceRouteNodeStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall>&)> AsyncGameServiceRouteNodeStringMsgHandler;

void AsyncCompleteGrpcGameServiceRouteNodeStringMsg()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceRouteNodeStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceRoutePlayerStringMsg(const RoutePlayerMessageRequest& request)
{
    AsyncGameServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncGameServiceRoutePlayerStringMsgGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncRoutePlayerStringMsg(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceRoutePlayerStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall>&)> AsyncGameServiceRoutePlayerStringMsgHandler;

void AsyncCompleteGrpcGameServiceRoutePlayerStringMsg()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceRoutePlayerStringMsgCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceUpdateSessionDetail(const RegisterPlayerSessionRequest& request)
{
    AsyncGameServiceUpdateSessionDetailGrpcClientCall* call = new AsyncGameServiceUpdateSessionDetailGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncUpdateSessionDetail(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceUpdateSessionDetailCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall>&)> AsyncGameServiceUpdateSessionDetailHandler;

void AsyncCompleteGrpcGameServiceUpdateSessionDetail()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceUpdateSessionDetailCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceEnterScene(const Centre2GsEnterSceneRequest& request)
{
    AsyncGameServiceEnterSceneGrpcClientCall* call = new AsyncGameServiceEnterSceneGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncEnterScene(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceEnterSceneCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall>&)> AsyncGameServiceEnterSceneHandler;

void AsyncCompleteGrpcGameServiceEnterScene()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceEnterSceneCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void GameServiceCreateScene(const CreateSceneRequest& request)
{
    AsyncGameServiceCreateSceneGrpcClientCall* call = new AsyncGameServiceCreateSceneGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncCreateScene(&call->context, request,
		&tls.grpc_node_registry.get<GameServiceCreateSceneCompleteQueue>(GlobalGrpcNodeEntity()).cq);

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}

std::function<void(const std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall>&)> AsyncGameServiceCreateSceneHandler;

void AsyncCompleteGrpcGameServiceCreateScene()
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != 
		tls.grpc_node_registry.get<GameServiceCreateSceneCompleteQueue>(GlobalGrpcNodeEntity()).cq.AsyncNext(&got_tag, &ok, tm)){
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

void InitGameServiceCompletedQueue() {
	tls.grpc_node_registry.emplace<GameServicePlayerEnterGameNodeCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceClientSendMessageToPlayerCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceRegisterGateNodeCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceCentreSendToPlayerViaGameNodeCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceInvokePlayerServiceCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceRouteNodeStringMsgCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceRoutePlayerStringMsgCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceUpdateSessionDetailCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceEnterSceneCompleteQueue>(GlobalGrpcNodeEntity());
	tls.grpc_node_registry.emplace<GameServiceCreateSceneCompleteQueue>(GlobalGrpcNodeEntity());
}

void HandleGameServiceCompletedQueueMessage() {
    AsyncCompleteGrpcGameServicePlayerEnterGameNode();
    AsyncCompleteGrpcGameServiceSendMessageToPlayer();
    AsyncCompleteGrpcGameServiceClientSendMessageToPlayer();
    AsyncCompleteGrpcGameServiceRegisterGateNode();
    AsyncCompleteGrpcGameServiceCentreSendToPlayerViaGameNode();
    AsyncCompleteGrpcGameServiceInvokePlayerService();
    AsyncCompleteGrpcGameServiceRouteNodeStringMsg();
    AsyncCompleteGrpcGameServiceRoutePlayerStringMsg();
    AsyncCompleteGrpcGameServiceUpdateSessionDetail();
    AsyncCompleteGrpcGameServiceEnterScene();
    AsyncCompleteGrpcGameServiceCreateScene();
}

