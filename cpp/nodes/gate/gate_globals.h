#pragma once
#include "core/network/codec/codec.h"

// Gate-specific client TCP channel, initialized in startGateNode().
// Replaces gGateNode->SendMessageToClient / gGateNode->Codec().
extern ProtobufCodec* gGateCodec;
