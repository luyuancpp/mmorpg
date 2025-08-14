#pragma once
#include <string>
#include <deque>
#include "time/comp/timer_task_comp.h"

class NodeInfo;

class EtcdManager
{
public:
	static void Shutdown();

	static std::string GetServiceName(uint32_t type);

	static std::string MakeNodeEtcdPrefix(const NodeInfo& info);

	static std::string MakeNodeEtcdKey(const NodeInfo& info);

	static std::string MakeNodePortEtcdPrefix(const NodeInfo& nodeInfo);

	static std::string MakeNodePortEtcdKey(const NodeInfo& nodeInfo);

	static void RegisterNodeService();

	static void RegisterNodePort();

	static void RequestEtcdLease();

	static void KeepNodeAlive();

private:

};