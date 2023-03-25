#include "game_service_replied.h"

#include "src/gate_server.h"

void OnGsPlayerServiceReplied(const TcpConnectionPtr& conn, const RpcClientResponsePtr& replied, Timestamp timestamp)
{
    MessageBody message;
    message.set_body(replied->response());
    message.set_id(replied->id());
    message.set_service(replied->service());
    message.set_method(replied->method());
    g_gate_node->codec().send(conn, message);
}
