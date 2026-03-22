#pragma once
#include "type_define/type_define.h"
#include "node/system/node/node_util.h"

using ServiceNodeList = std::array<NodeInfoListPBComponent, eNodeType_ARRAYSIZE>;

NodeInfo* FindZoneUniqueNodeInfo(uint32_t zoneId, uint32_t nodeType);
NodeInfo& GetNodeInfo();
uint32_t GetZoneId();
