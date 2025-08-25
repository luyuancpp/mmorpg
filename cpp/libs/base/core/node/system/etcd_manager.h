#pragma once
#include <string>
#include <deque>
#include "time/comp/timer_task_comp.h"

class NodeInfo;

class EtcdManager
{
public:
	std::deque<std::string>& GetPendingKeys() { return pendingKeys; }

	void Shutdown();

	std::string GetServiceName(uint32_t type);

	std::string MakeNodeEtcdPrefix(const NodeInfo& info);

	std::string MakeNodeEtcdKey(const NodeInfo& info);

	std::string MakeNodePortEtcdPrefix(const NodeInfo& nodeInfo);

	std::string MakeNodePortEtcdKey(const NodeInfo& nodeInfo);

	void RegisterNodeService();

	void RegisterNodePort();

	void RequestEtcdLease();

	void KeepNodeAlive();

private:
	TimerTaskComp renewLeaseTimer;
	std::deque<std::string> pendingKeys;
};