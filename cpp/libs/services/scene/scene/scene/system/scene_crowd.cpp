#include "scene_crowd.h"

#include "scene/scene/comp/dt_crowd.h"

#include "modules/scene/comp/scene_comp.h"
#include "scene/scene/constants/dt_crowd.h"

#include "proto/logic/event/scene_event.pb.h"
#include <proto/logic/component/actor_comp.pb.h>
#include <threading/registry_manager.h>
#include <muduo/base/Logging.h>

void SceneCrowdSystem::AfterEnterSceneHandler(const AfterEnterRoom& message)
{
	const auto playerEntity = entt::to_entity(message.entity());

	if (!tlsRegistryManager.actorRegistry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity not found";
		return;
	}

	auto dtCrowd = tlsRegistryManager.roomRegistry.try_get<DtCrowdPtr>(playerEntity);
	if (dtCrowd == nullptr)
	{
		return;
	}

	auto transform = tlsRegistryManager.actorRegistry.try_get<Transform>(playerEntity);
	if (transform == nullptr)
	{
		LOG_ERROR << "Transform component not found for player with GUID: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity);
		return;
	}

	// Setup dtCrowd agent parameters
	dtCrowdAgentParams agentParams;
	memset(&agentParams, 0, sizeof(agentParams));
	agentParams.radius = kAgentRadius;
	agentParams.height = kAgentHeight;
	agentParams.maxAcceleration = 8.0f;
	agentParams.maxSpeed = 3.5f;
	agentParams.collisionQueryRange = agentParams.radius * 12.0f;
	agentParams.pathOptimizationRange = agentParams.radius * 30.0f;
	agentParams.updateFlags = DT_CROWD_ANTICIPATE_TURNS |
		DT_CROWD_OPTIMIZE_VIS |
		DT_CROWD_OPTIMIZE_TOPO |
		DT_CROWD_OBSTACLE_AVOIDANCE |
		DT_CROWD_SEPARATION;
	agentParams.obstacleAvoidanceType = kObstacleAvoidanceType;
	agentParams.separationWeight = kSeparationWeight;

	// Initialize agent position
	dtReal position[] = { transform->mutable_location()->x(),
						  transform->mutable_location()->y(),
						  transform->mutable_location()->z() };

	// Add agent to dtCrowd
	int agentIndex = (*dtCrowd)->addAgent(position, agentParams, nullptr);
}

void SceneCrowdSystem::BeforeLeaveSceneHandler(const BeforeLeaveRoom& message)
{
	// Placeholder for any necessary cleanup or handling before leaving the scene
	// No implementation needed currently
}
