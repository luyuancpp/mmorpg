#include "session.h"

#include "src/util/snow_flake.h"

NodeId get_gate_node_id(Guid session_id)
{
    return static_cast<NodeId>(session_id >> NodeBit15Sequence::node_bit());
}

uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn)
{
    return boost::any_cast<uint64_t>(conn->getContext());
}
