#pragma once
#include "player/system/player_node_system.h"
#include "proto/common/common.pb.h"

class CentreNodeInfo {
public:
    [[nodiscard]] NodeId GetNodeId() const
    {
        return nodeInfo.node_id();
    }

    void SetNodeId(const NodeId nodeId)
    {
        nodeInfo.set_node_id(nodeId);
    }
    
    [[nodiscard]] uint32_t GetNodeType() const
    {
        return nodeInfo.node_type();
    }

    void SetNodeType(const uint32_t nodeType)
    {
        nodeInfo.set_node_type(nodeType);
    }

    [[nodiscard]] NodeInfo& GetNodeInfo() 
    {
        return nodeInfo;
    }
private:
    NodeInfo nodeInfo;
};

extern  CentreNodeInfo gCentreNodeInfo; 