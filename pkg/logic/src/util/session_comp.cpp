#include "session_comp.h"

#include "type_alias/session_id_gen.h"

NodeId GetGateNodeId(Guid session_id)
{
    return static_cast<NodeId>(session_id >> SessionIdGen::node_bit());
}
