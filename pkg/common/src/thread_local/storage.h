#pragma once

#include "util/game_registry.h"
#include "util/snow_flake.h"

class ThreadLocalStorage
{
public:
    entt::registry globalRegistry;
	entt::registry registry;
    entt::registry sceneRegistry;
    entt::registry sceneNodeRegistry;
    entt::registry gateNodeRegistry;
    entt::registry centreNodeRegistry;
    entt::registry itemRegistry;
    entt::registry networkRegistry;
    entt::registry grpc_node_registry;

	entt::dispatcher dispatcher;

	entt::entity operatorEntity{entt::null};
	entt::entity errorEntity{entt::null};
	entt::entity globalEntity{entt::null};
    entt::entity globalGrpcNodeEntity{ entt::null };

    SnowFlake itemIdGenerator;
    Guid lastGeneratorItemGuid{ kInvalidGuid };

	void ClearForTest();
};

extern thread_local ThreadLocalStorage tls;