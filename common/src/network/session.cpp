#include "session.h"

#include "src/util/snow_flake.h"

NodeId node_id(Guid sesion_id)
{
    return sesion_id >> ServerSequence32::kNodeBit;
}

uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn)
{
    return boost::any_cast<uint64_t>(conn->getContext());
}
