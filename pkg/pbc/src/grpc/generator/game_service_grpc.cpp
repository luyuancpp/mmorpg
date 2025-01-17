#include "muduo/base/Logging.h"

#include "grpc/generator/game_service_grpc.h"

using GrpcGameServiceStubPtr = std::unique_ptr<GameService::Stub>;
GrpcGameServiceStubPtr gGameServiceStub;

std::unique_ptr<grpc::CompletionQueue> gGameServicePlayerEnterGameNodeCq;

void GameServicePlayerEnterGameNode(const PlayerEnterGameNodeRequest& request)
{
    AsyncGameServicePlayerEnterGameNodeGrpcClientCall* call = new AsyncGameServicePlayerEnterGameNodeGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncPlayerEnterGameNode(&call->context, request, gGameServicePlayerEnterGameNodeCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServicePlayerEnterGameNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServicePlayerEnterGameNodeGrpcClientCall> call(static_cast<AsyncGameServicePlayerEnterGameNodeGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceSendMessageToPlayerCq;

void GameServiceSendMessageToPlayer(const NodeRouteMessageRequest& request)
{
    AsyncGameServiceSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncSendMessageToPlayer(&call->context, request, gGameServiceSendMessageToPlayerCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceSendMessageToPlayer(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceSendMessageToPlayerGrpcClientCall> call(static_cast<AsyncGameServiceSendMessageToPlayerGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceClientSendMessageToPlayerCq;

void GameServiceClientSendMessageToPlayer(const ClientSendMessageToPlayerRequest& request)
{
    AsyncGameServiceClientSendMessageToPlayerGrpcClientCall* call = new AsyncGameServiceClientSendMessageToPlayerGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncClientSendMessageToPlayer(&call->context, request, gGameServiceClientSendMessageToPlayerCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceClientSendMessageToPlayer(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall> call(static_cast<AsyncGameServiceClientSendMessageToPlayerGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceRegisterGateNodeCq;

void GameServiceRegisterGateNode(const RegisterGateNodeRequest& request)
{
    AsyncGameServiceRegisterGateNodeGrpcClientCall* call = new AsyncGameServiceRegisterGateNodeGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncRegisterGateNode(&call->context, request, gGameServiceRegisterGateNodeCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceRegisterGateNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceRegisterGateNodeGrpcClientCall> call(static_cast<AsyncGameServiceRegisterGateNodeGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceCentreSendToPlayerViaGameNodeCq;

void GameServiceCentreSendToPlayerViaGameNode(const NodeRouteMessageRequest& request)
{
    AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall* call = new AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncCentreSendToPlayerViaGameNode(&call->context, request, gGameServiceCentreSendToPlayerViaGameNodeCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceCentreSendToPlayerViaGameNode(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall> call(static_cast<AsyncGameServiceCentreSendToPlayerViaGameNodeGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceInvokePlayerServiceCq;

void GameServiceInvokePlayerService(const NodeRouteMessageRequest& request)
{
    AsyncGameServiceInvokePlayerServiceGrpcClientCall* call = new AsyncGameServiceInvokePlayerServiceGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncInvokePlayerService(&call->context, request, gGameServiceInvokePlayerServiceCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceInvokePlayerService(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceInvokePlayerServiceGrpcClientCall> call(static_cast<AsyncGameServiceInvokePlayerServiceGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceRouteNodeStringMsgCq;

void GameServiceRouteNodeStringMsg(const RouteMessageRequest& request)
{
    AsyncGameServiceRouteNodeStringMsgGrpcClientCall* call = new AsyncGameServiceRouteNodeStringMsgGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncRouteNodeStringMsg(&call->context, request, gGameServiceRouteNodeStringMsgCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceRouteNodeStringMsg(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceRouteNodeStringMsgGrpcClientCall> call(static_cast<AsyncGameServiceRouteNodeStringMsgGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceRoutePlayerStringMsgCq;

void GameServiceRoutePlayerStringMsg(const RoutePlayerMessageRequest& request)
{
    AsyncGameServiceRoutePlayerStringMsgGrpcClientCall* call = new AsyncGameServiceRoutePlayerStringMsgGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncRoutePlayerStringMsg(&call->context, request, gGameServiceRoutePlayerStringMsgCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceRoutePlayerStringMsg(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall> call(static_cast<AsyncGameServiceRoutePlayerStringMsgGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceUpdateSessionDetailCq;

void GameServiceUpdateSessionDetail(const RegisterPlayerSessionRequest& request)
{
    AsyncGameServiceUpdateSessionDetailGrpcClientCall* call = new AsyncGameServiceUpdateSessionDetailGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncUpdateSessionDetail(&call->context, request, gGameServiceUpdateSessionDetailCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceUpdateSessionDetail(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceUpdateSessionDetailGrpcClientCall> call(static_cast<AsyncGameServiceUpdateSessionDetailGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceEnterSceneCq;

void GameServiceEnterScene(const Centre2GsEnterSceneRequest& request)
{
    AsyncGameServiceEnterSceneGrpcClientCall* call = new AsyncGameServiceEnterSceneGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncEnterScene(&call->context, request, gGameServiceEnterSceneCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceEnterScene(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceEnterSceneGrpcClientCall> call(static_cast<AsyncGameServiceEnterSceneGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}

std::unique_ptr<grpc::CompletionQueue> gGameServiceCreateSceneCq;

void GameServiceCreateScene(const CreateSceneRequest& request)
{
    AsyncGameServiceCreateSceneGrpcClientCall* call = new AsyncGameServiceCreateSceneGrpcClientCall;

    call->response_reader =
        gGameServiceStub->PrepareAsyncCreateScene(&call->context, request, gGameServiceCreateSceneCq.get());

    call->response_reader->StartCall();

    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
}


void AsyncCompleteGrpcGameServiceCreateScene(grpc::CompletionQueue& cq)
{
    void* got_tag;
    bool ok = false;

    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (grpc::CompletionQueue::GOT_EVENT != cq.AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncGameServiceCreateSceneGrpcClientCall> call(static_cast<AsyncGameServiceCreateSceneGrpcClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
    }
    else
    {
        LOG_ERROR << call->status.error_message();
    }
}