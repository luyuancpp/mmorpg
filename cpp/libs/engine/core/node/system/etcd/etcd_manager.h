#pragma once
#include <string>
#include <deque>
#include "time/comp/timer_task_comp.h"

class NodeInfo;

class EtcdManager
{
public:
	std::deque<std::string> &GetPendingKeys() { return pendingKeys; }

	void Shutdown();

	std::string GetServiceName(uint32_t type);

	std::string MakeNodeEtcdPrefix(const NodeInfo &info);

	std::string MakeNodeEtcdKey(const NodeInfo &info);

	// Global-uniqueness allocation key for (node_type, node_id). Intentionally
	// zone-independent: two zones must not both claim the same (node_type,
	// node_id) or their Snowflake PlayerId/GuidId streams collide. See
	// RegisterNodeService for the CAS protocol that honours this key.
	std::string MakeNodeAllocationKey(const NodeInfo &info);

	std::string MakeNodePortEtcdPrefix(const NodeInfo &nodeInfo);

	std::string MakeNodePortEtcdKey(const NodeInfo &nodeInfo);

	void RegisterNodeService();

	// Second phase of node registration. Must only be called after the
	// allocation key CAS (phase 1 of RegisterNodeService) succeeds —
	// EtcdService::OnTxnSucceeded is responsible for that sequencing.
	void PublishNodeInfoAfterAllocation();

	void UpdateNodeInfo();

	void RegisterNodePort();

	void RequestNodeLease();

	void StartLeaseKeepAlive();

	static std::string MakeSnowFlakeGuardKey(const NodeInfo &info);

	void WriteSnowFlakeGuard();

private:
	TimerTaskComp leaseKeepAliveTimer;
	std::deque<std::string> pendingKeys;
};