#pragma once
#include <memory>
#include <muduo/net/TcpConnection.h>

namespace RpcThreadContext {
	inline thread_local std::shared_ptr<muduo::net::TcpConnection> tls_current_conn = nullptr;
}
