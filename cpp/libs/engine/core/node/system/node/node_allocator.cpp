#include "node_allocator.h"
#include <muduo/base/Logging.h>
#include <network/network_utils.h>
#include "node.h"
#include "node/system/etcd/etcd_manager.h"
#include "node/system/etcd/etcd_helper.h"
#include <network/node_utils.h>
#include <threading/redis_manager.h>
#include <node_config_manager.h>
#include <threading/registry_manager.h>
#include <core/utils/id/snow_flake.h>
#include <threading/entity_manager.h>

uint32_t tryPortId{ 0 };

void NodeAllocator::AcquireNode() {
	LOG_INFO << "Acquiring node ID for node type: " << gNode->GetNodeType();

	// 1. 如果是全局唯一类型，执行清理逻辑 + 直接使用 zone_id
	if (IsZoneSingletonNodeType(gNode->GetNodeType())) {
		const uint32_t zoneId = tlsNodeConfigManager.GetGameConfig().zone_id();
		gNode->GetNodeInfo().set_node_id(zoneId);
		LOG_INFO << "Assigned node_id by zone_id: " << zoneId;

		std::string prefix = gNode->GetEtcdManager().MakeNodeEtcdKey(gNode->GetNodeInfo());
		EtcdHelper::DeleteRange(prefix, false);
		gNode->GetEtcdManager().RegisterNodeService();
		return;
	}

	// ...（以下为非 singleton 的原有分配逻辑）
	auto& nodeList = tlsRegistryManager.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity())[gNode->GetNodeType()];
	auto& existingNodes = *nodeList.mutable_node_list();

	std::unordered_set<uint32_t> usedIds;
	uint32_t maxUsedId = 0;

	for (const auto& node : existingNodes) {
		usedIds.insert(node.node_id());
		if (node.node_id() > maxUsedId) {
			maxUsedId = node.node_id();
		}
	}

	static constexpr uint32_t kMaxNodeId = (1U << kNodeBits) - 1;
	uint32_t nextNodeId;

	if (maxUsedId < kMaxNodeId) {
		// 还有空间，优先用 maxUsedId + 1
		nextNodeId = maxUsedId + 1;
	}
	else {
		// 空间满了，从头找未使用的（包括 0）
		bool found = false;
		for (uint32_t id = 0; id <= kMaxNodeId; ++id) {
			if (usedIds.find(id) == usedIds.end()) {
				nextNodeId = id;
				found = true;
				break;
			}
		}
		if (!found) {
			LOG_FATAL << "No available node ID (max " << kMaxNodeId << ")";
			throw std::runtime_error("Node ID space exhausted");
		}
	}

	GetNodeInfo().set_node_id(nextNodeId);

	gNode->GetEtcdManager().RegisterNodeService();
}

bool IsPortReservedType(uint32_t type)
{
	return type == eNodeType::GateNodeService;
}

uint32_t AllocatePortInRange(const std::unordered_set<uint32_t>& usedPorts,
	uint32_t minPort, uint32_t maxPort, uint32_t tryPortId)
{
	// 优先从 tryPortId 到 maxPort
	for (uint32_t port = tryPortId; port <= maxPort; ++port) {
		if (usedPorts.find(port) == usedPorts.end()) {
			return port;
		}
	}

	// 再从 minPort 到 tryPortId - 1
	for (uint32_t port = minPort; port < tryPortId; ++port) {
		if (usedPorts.find(port) == usedPorts.end()) {
			return port;
		}
	}

	return 0; // 没有可用端口
}

void NodeAllocator::AcquireNodePort() {
	auto& nodeList = tlsRegistryManager.nodeGlobalRegistry.get<ServiceNodeList>(GetGlobalGrpcNodeEntity())[gNode->GetNodeType()];
	auto& existingNodes = *nodeList.mutable_node_list();

	std::unordered_set<uint32_t> usedPorts;
	for (const auto& node : existingNodes) {
		usedPorts.insert(node.endpoint().port());
	}

	uint32_t assignedPort = 0;

	if (IsPortReservedType(GetNodeInfo().node_type())) {
		constexpr uint32_t GATE_BASE_PORT = 10000;
		constexpr uint32_t GATE_PORT_LIMIT = 19999;

		// 默认初始为 BASE_PORT
		if (tryPortId < GATE_BASE_PORT || tryPortId > GATE_PORT_LIMIT) {
			tryPortId = GATE_BASE_PORT;
		}

		assignedPort = AllocatePortInRange(usedPorts, GATE_BASE_PORT, GATE_PORT_LIMIT, tryPortId);
		LOG_INFO << "Assigned Gate RPC port: " << assignedPort;
	}
	else {
		constexpr uint32_t MIN_PORT = 20000;
		constexpr uint32_t MAX_PORT = 65535;

		if (tryPortId < MIN_PORT || tryPortId > MAX_PORT) {
			tryPortId = MIN_PORT;
		}

		assignedPort = AllocatePortInRange(usedPorts, MIN_PORT, MAX_PORT, tryPortId);
		LOG_INFO << "Assigned dynamic RPC port: " << assignedPort;
	}

	if (assignedPort != 0) {
		tryPortId = assignedPort + 1;
	}
	else {
		LOG_WARN << "No available RPC port found. TryPortId was: " << tryPortId;
		tryPortId = 0; // fallback or signal failure
	}

	GetNodeInfo().mutable_endpoint()->set_port(assignedPort);

	LOG_INFO << "NodeType: " << gNode->GetNodeType()
		<< " IP: " << GetNodeInfo().endpoint().ip()
		<< " Port: " << assignedPort;

	gNode->GetEtcdManager().RegisterNodePort();
}
