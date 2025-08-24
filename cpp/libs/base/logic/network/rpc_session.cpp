#include "rpc_session.h"

bool IsSameAddress(const muduo::net::InetAddress& connAddr, const muduo::net::InetAddress& serverAddr)
{
	return serverAddr.toIp() == connAddr.toIp() && serverAddr.port() == connAddr.port();
}
