#pragma once
#include <string>
#include <deque>

class NodeInfo;

class EtcdManager
{
public:
	static std::string GetServiceName(uint32_t type);

	static std::string MakeNodeEtcdPrefix(const NodeInfo& info);

	static std::string MakeNodeEtcdKey(const NodeInfo& info);

	static std::string MakeNodePortEtcdPrefix(const NodeInfo& nodeInfo);

	static std::string MakeNodePortEtcdKey(const NodeInfo& nodeInfo);

	static void RegisterNodeService();

	static void RegisterNodePort();

	static void RequestEtcdLease();

	static std::deque<std::string> pendingKeys;

};