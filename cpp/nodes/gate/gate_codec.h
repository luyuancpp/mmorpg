#pragma once

#include "network/codec/codec.h"
#include <cassert>

namespace detail {
    inline ProtobufCodec* g_gate_codec = nullptr;
}

// Call once during gate startup to wire the codec reference.
inline void InitGateCodec(ProtobufCodec& codec)
{
    detail::g_gate_codec = &codec;
}

// Returns the codec owned by GateRuntimeContext.
// Must only be called after gate startup completes (after InitGateCodec).
inline ProtobufCodec& GetGateCodec()
{
    assert(detail::g_gate_codec);
    return *detail::g_gate_codec;
}
