#include "session.h"

#include "type_alias/session_id_gen.h"

NodeId get_gate_node_id(Guid session_id)
{
    return static_cast<NodeId>(session_id >> SessionIdGen::node_bit());
}
