#pragma once
#include "core/network/codec/codec.h"

// Gate 专属的客户端 TCP 通道，在 startGateNode() 中初始化。
// 用于替代原来的 gGateNode->SendMessageToClient / gGateNode->Codec()。
extern ProtobufCodec* gGateCodec;
