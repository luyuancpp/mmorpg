#include "node_allocator.h"
#include <muduo/base/Logging.h>
#include "node.h"
#include "node/system/etcd/etcd_manager.h"
#include "node/system/etcd/etcd_helper.h"
#include <network/node_utils.h>
#include <thread_context/redis_manager.h>
#include <core/utils/id/snow_flake.h>
#include <thread_context/entity_manager.h>

#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

uint32_t tryPortId{0};

void NodeAllocator::AcquireNode()
{
	const uint32_t nodeType = gNode->GetNodeType();
	LOG_INFO << "Acquiring node ID for node type: " << nodeType;

	// All services pick next free ID in local snapshot,
	// then rely on etcd PutIfAbsent CAS to enforce same-type uniqueness.
	auto &nodeList = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity())[nodeType];
	auto &existingNodes = *nodeList.mutable_node_list();

	std::unordered_set<uint32_t> usedIds;
	uint32_t maxUsedId = 0;

	for (const auto &node : existingNodes)
	{
		usedIds.insert(node.node_id());
		if (node.node_id() > maxUsedId)
		{
			maxUsedId = node.node_id();
		}
	}

	static constexpr uint32_t kMaxNodeId = static_cast<uint32_t>(kNodeMask);
	uint32_t nextNodeId;

	if (maxUsedId < kMaxNodeId)
	{
		nextNodeId = maxUsedId + 1;
	}
	else
	{
		bool found = false;
		for (uint32_t id = 0; id <= kMaxNodeId; ++id)
		{
			if (usedIds.find(id) == usedIds.end())
			{
				nextNodeId = id;
				found = true;
				break;
			}
		}
		if (!found)
		{
			LOG_FATAL << "No available node ID (max " << kMaxNodeId << ")";
			throw std::runtime_error("Node ID space exhausted");
		}
	}

	GetNodeInfo().set_node_id(nextNodeId);

	gNode->GetEtcdManager().RegisterNodeService();
}

void NodeAllocator::ReRegisterExistingNode()
{
	// Re-registration must keep existing node_id to preserve SnowFlake worker bits
	// and avoid mid-flight identity change after temporary lease loss.
	LOG_INFO << "Re-registering existing node with node_id=" << GetNodeInfo().node_id()
			 << " port=" << GetNodeInfo().endpoint().port();
	gNode->GetEtcdManager().RegisterNodePort();
}

bool IsPortReservedType(uint32_t type)
{
	return type == eNodeType::GateNodeService;
}

bool IsLocalPortAvailable(uint16_t port)
{
#ifdef _WIN32
	SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		return false;
	}
#else
	int sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
	{
		return false;
	}
#endif

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	int optval = 1;
	::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				 reinterpret_cast<const char *>(&optval), sizeof(optval));

	bool available = (::bind(sock, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0);
#ifdef _WIN32
	::closesocket(sock);
#else
	::close(sock);
#endif
	return available;
}

uint32_t AllocatePortInRange(const std::unordered_set<uint32_t> &usedPorts,
							 uint32_t minPort, uint32_t maxPort, uint32_t tryPortId)
{
	// Scan from tryPortId to maxPort first
	for (uint32_t port = tryPortId; port <= maxPort; ++port)
	{
		if (usedPorts.find(port) == usedPorts.end() && IsLocalPortAvailable(static_cast<uint16_t>(port)))
		{
			return port;
		}
	}

	// Wrap around from minPort to tryPortId - 1
	for (uint32_t port = minPort; port < tryPortId; ++port)
	{
		if (usedPorts.find(port) == usedPorts.end() && IsLocalPortAvailable(static_cast<uint16_t>(port)))
		{
			return port;
		}
	}

	return 0; // No available port
}

void NodeAllocator::AcquireNodePort()
{
	auto &nodeList = tlsEcs.nodeGlobalRegistry.get_or_emplace<ServiceNodeList>(tlsEcs.GrpcNodeEntity())[gNode->GetNodeType()];
	auto &existingNodes = *nodeList.mutable_node_list();

	std::unordered_set<uint32_t> usedPorts;
	for (const auto &node : existingNodes)
	{
		usedPorts.insert(node.endpoint().port());
	}

	uint32_t assignedPort = 0;

	if (IsPortReservedType(GetNodeInfo().node_type()))
	{
		constexpr uint32_t GATE_BASE_PORT = 10000;
		constexpr uint32_t GATE_PORT_LIMIT = 19999;

		// Reset to base if out of range
		if (tryPortId < GATE_BASE_PORT || tryPortId > GATE_PORT_LIMIT)
		{
			tryPortId = GATE_BASE_PORT;
		}

		assignedPort = AllocatePortInRange(usedPorts, GATE_BASE_PORT, GATE_PORT_LIMIT, tryPortId);
		LOG_INFO << "Assigned Gate RPC port: " << assignedPort;
	}
	else
	{
		constexpr uint32_t MIN_PORT = 20000;
		constexpr uint32_t MAX_PORT = 65535;

		if (tryPortId < MIN_PORT || tryPortId > MAX_PORT)
		{
			tryPortId = MIN_PORT;
		}

		assignedPort = AllocatePortInRange(usedPorts, MIN_PORT, MAX_PORT, tryPortId);
		LOG_INFO << "Assigned dynamic RPC port: " << assignedPort;
	}

	if (assignedPort != 0)
	{
		tryPortId = assignedPort + 1;
	}
	else
	{
		LOG_WARN << "No available RPC port found. TryPortId was: " << tryPortId;
		tryPortId = 0; // fallback or signal failure
	}

	GetNodeInfo().mutable_endpoint()->set_port(assignedPort);

	// Allocate gRPC port if node has gRPC services registered.
	if (!gNode->GetGrpcServices().empty() && assignedPort != 0)
	{
		uint32_t grpcPort = assignedPort + 1;
		if (!IsLocalPortAvailable(static_cast<uint16_t>(grpcPort)))
		{
			LOG_WARN << "gRPC port " << grpcPort << " (TCP+1) not available, scanning...";
			constexpr uint32_t MIN_PORT = 20000;
			constexpr uint32_t MAX_PORT = 65535;
			std::unordered_set<uint32_t> usedGrpcPorts = usedPorts;
			usedGrpcPorts.insert(assignedPort);
			grpcPort = AllocatePortInRange(usedGrpcPorts, MIN_PORT, MAX_PORT, assignedPort + 2);
		}

		if (grpcPort != 0)
		{
			GetNodeInfo().mutable_grpc_endpoint()->set_ip(GetNodeInfo().endpoint().ip());
			GetNodeInfo().mutable_grpc_endpoint()->set_port(grpcPort);
			LOG_INFO << "Assigned gRPC port: " << grpcPort;
		}
		else
		{
			LOG_ERROR << "No available gRPC port found.";
		}
	}

	LOG_INFO << "NodeType: " << gNode->GetNodeType()
			 << " IP: " << GetNodeInfo().endpoint().ip()
			 << " Port: " << assignedPort;

	gNode->GetEtcdManager().RegisterNodePort();
}
