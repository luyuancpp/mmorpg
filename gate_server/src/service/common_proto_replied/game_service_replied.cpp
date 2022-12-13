#include "game_service_replied.h"

#include "src/gate_server.h"

void OnGsPlayerServiceReplied(const TcpConnectionPtr& conn, const RpcClientResponsePtr& replied, Timestamp timestamp)
{
    MessageBody msg;
    msg.set_body(replied->response());
    msg.set_id(replied->id());
    msg.set_msg_id(replied->msg_id());
    g_gate_node->codec().send(conn, msg);
}
