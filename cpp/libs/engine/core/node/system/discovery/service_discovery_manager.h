#pragma once
#include "node/system/etcd/etcd_service.h"
#include "service_node_snapshot.h"
#include "time/comp/timer_task_comp.h"

class ServiceDiscoveryManager {
public:
	// 同步定时器捕获 this,生命周期内禁止复制或移动宿主。
	ServiceDiscoveryManager() = default;
	ServiceDiscoveryManager(const ServiceDiscoveryManager&) = delete;
	ServiceDiscoveryManager& operator=(const ServiceDiscoveryManager&) = delete;
	ServiceDiscoveryManager(ServiceDiscoveryManager&&) = delete;
	ServiceDiscoveryManager& operator=(ServiceDiscoveryManager&&) = delete;

	void Init();

	void Shutdown();

	void FetchServiceNodes();

	void AddServiceNode(const std::string& nodeJson, uint32_t nodeType);

	void HandleServiceNodeStart(const std::string& key, const std::string& value);

	// 返回的快照及其借用列表只在所属 loop 线程访问。
	const ServiceNodeSnapshot &ServiceNodes() const { return serviceNodes_; }
	// 仅供 Node 的 etcd DELETE / 宽限期收尾移除快照,仍在所属 loop 线程执行。
	int RemoveServiceNode(uint32_t nodeType, const std::string &nodeUuid)
	{
		return serviceNodes_.Remove(nodeType, nodeUuid);
	}

	EtcdService etcdService;

private:
	ServiceNodeSnapshot serviceNodes_;
	TimerTaskComp serviceNodeSyncTimer_;
};
