#pragma once
#include "type_define/type_define.h"
#include "node/system/node/node_util.h"

using ServiceNodeList = std::array<NodeInfoListComp, eNodeType_ARRAYSIZE>;

NodeInfo &GetNodeInfo();
uint32_t GetZoneId();
