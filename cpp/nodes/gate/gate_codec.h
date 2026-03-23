#pragma once

#include "handler/rpc/gate_service_handler.h"
#include "node/system/node/simple_node.h"

// Returns the codec owned by the GateHandler, via the global node pointer.
// Must only be called after gate startup completes.
inline ProtobufCodec& GetGateCodec()
{
    return static_cast<SimpleNode<GateHandler>*>(gNode)->GetHandler().Codec();
}
