#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"
#include "util/consistent_hash_node.h"
#include "proto/common/node.pb.h"


class ThreadLocalStorage
{
public:
	using NodeRgistries = std::array<entt::registry, eNodeType_ARRAYSIZE>;
	using NodeGloabalEntity = std::array<entt::entity, eNodeType_ARRAYSIZE>;
	using ConsistentNode = ConsistentHashNode<uint64_t, entt::entity>;
	using ConsistentNodes = std::array<ConsistentNode, eNodeType_ARRAYSIZE>;

	ThreadLocalStorage();

	entt::registry globalRegistry;
	entt::registry actorRegistry;
	entt::registry sceneRegistry;
	entt::registry itemRegistry;
	entt::registry sessionRegistry;
	entt::registry nodeGlobalRegistry;

	entt::dispatcher dispatcher;

	entt::entity operatorEntity{ entt::null };
	entt::entity errorEntity{ entt::null };
	entt::entity globalEntity{ entt::null };
	entt::entity globalGrpcNodeEntity{ entt::null };

	SnowFlake itemIdGenerator;
	Guid lastGeneratorItemGuid{ kInvalidGuid };

	void Clear();

	entt::registry& GetNodeRegistry(uint32_t nodeType) {
		return nodeRegistries[nodeType];
	}

	NodeRgistries& GetNodeRegistry() {
		return nodeRegistries;
	}

	entt::entity GetNodeGlobalEntity(uint32_t nodeType);

	ConsistentNode& GetConsistentNode(uint32_t nodeType) {
		return consistentNodes[nodeType];
	}

	void OnNodeStart(uint32_t nodeId);
private:
	NodeRgistries nodeRegistries;
	NodeGloabalEntity nodeGlobalEntities;
	ConsistentNodes consistentNodes;
};

extern thread_local ThreadLocalStorage tls;