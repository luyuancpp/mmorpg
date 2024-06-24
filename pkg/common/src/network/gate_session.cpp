#include "gate_session.h"

#include "util/snow_flake.h"

uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn)
{
    return boost::any_cast<uint64_t>(conn->getContext());
}
