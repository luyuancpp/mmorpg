#include "scene_crowd.h"

#include "comp/scene/dtcrowd.h"
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "comp/scene.h"
#include "constants/dt_crowd.h"

#include "proto/logic/event/scene_event.pb.h"

void SceneCrowdSystem::AfterEnterSceneHandler(const AfterEnterScene& message)
{
	const auto playerEntity = entt::to_entity(message.entity());

	if (!tls.registry.valid(playerEntity))
	{
		LOG_ERROR << "Player entity not found";
		return;
	}

	auto dtCrowd = tls.sceneRegistry.try_get<DtCrowdPtr>(playerEntity);
	if (dtCrowd == nullptr)
	{
		LOG_ERROR << "dtCrowd component not found for scene";
		return;
	}

	auto transform = tls.registry.try_get<Transform>(playerEntity);
	if (transform == nullptr)
	{
		LOG_ERROR << "Transform component not found for player with GUID: " << tls.registry.get<Guid>(playerEntity);
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

void SceneCrowdSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
	// Placeholder for any necessary cleanup or handling before leaving the scene
	// No implementation needed currently
}
