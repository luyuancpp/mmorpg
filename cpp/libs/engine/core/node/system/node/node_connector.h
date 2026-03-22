#pragma once

#include "node/system/node/node_util.h"

class NodeConnector {
public:
	static void ConnectToNode(const NodeInfo& nodeInfo);
	static void ConnectToGrpcNode(const NodeInfo& nodeInfo);
	static void ConnectToTcpNode(const NodeInfo& nodeInfo);
	static void ConnectToHttpNode(const NodeInfo& nodeInfo);
	static void ConnectAllNodes();
};