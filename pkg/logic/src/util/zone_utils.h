#pragma once
#include "type_define/type_define.h"

uint32_t GetZoneIdFromNodeId(NodeId nodeId);
bool IsCrossZone(NodeId fromNodeId, NodeId toNodeId);