#include "rpc_session.h"

bool RpcSession::Connected() const
{ return connection->connected(); }

bool IsSameAddr(const muduo::net::InetAddress& conn_addr, const muduo::net::InetAddress& server_info)
{
	return server_info.toIp() == conn_addr.toIp() && server_info.port() == conn_addr.port();
}
