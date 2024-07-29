#include "muduo/base/Logging.h"

#include "gate_node.h"
#include "grpc/client/deploy_async_client_call.h"
#include "grpc/deploy/deployclient.h"

void AsyncCompleteGrpcDeployService()
{
    void* got_tag;
    bool ok = false;
   
    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 0;
    tm.clock_type = GPR_CLOCK_MONOTONIC;
    if (CompletionQueue::GOT_EVENT != gDeployCq->AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<DeployAsyncClientCall> call (static_cast<DeployAsyncClientCall*>(got_tag));
	if (!ok)
	{
		LOG_ERROR << "RPC failed";
		return;
	}
    if (call->status.ok())
    {
        g_gate_node->SetNodeId(call->reply.node_id());
        g_gate_node->StartServer(call->reply.info());
    }
    else
    {
        LOG_ERROR << "RPC failed";
    }
}

