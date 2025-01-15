#pragma once
#include "player/system/player_node_system.h"

class SceneNodeInfo {
public:
    [[nodiscard]] NodeId GetNodeId() const
    {
        return nodeInfo.node_id();
    }

    void SetNodeId(const NodeId nodeId)
    {
        nodeInfo.set_node_id(nodeId);
    }
    
    [[nodiscard]] NodeInfo& GetNodeInfo() 
    {
        return nodeInfo;
    }
private:
    NodeInfo nodeInfo;
};

extern  SceneNodeInfo gSceneNodeInfo; 