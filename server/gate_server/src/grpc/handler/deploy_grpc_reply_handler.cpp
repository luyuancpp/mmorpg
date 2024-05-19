#include "deploy_grpc_reply_handler.h"

#include "src/util/defer.h"

#include "src/grpc/async_client_call.h"
#include "src/grpc/deployclient.h"

void AsyncCompleteGrpc()
{
    void* got_tag;
    bool ok = false;
    gpr_timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 1;
    if (CompletionQueue::GOT_EVENT != g_deploy_client->cq().AsyncNext(&got_tag, &ok, tm))
    {
        return;
    }

    std::unique_ptr<AsyncClientCall> call (static_cast<AsyncClientCall*>(got_tag));

    CHECK(ok);

    if (call->status.ok())
        std::cout << "Greeter received: " << call->reply.DebugString() << std::endl;
    else
        std::cout << "RPC failed" << std::endl;

}

void DeployGrpcReplyHandler()
{

}
