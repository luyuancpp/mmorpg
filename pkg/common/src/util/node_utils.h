#pragma once
#include "type_define/type_define.h"
#include "proto/common/common.pb.h"
#include "proto/common/node.pb.h"

using ServiceNodeList = std::array<NodeInfoListPBComponent, eNodeType_ARRAYSIZE>;

NodeInfo* FindZoneUniqueNodeInfo(uint32_t zoneId, uint32_t nodeType);
NodeInfo& GetNodeInfo();
