#include "scene_crowd.h"

#include "DetourCrowd/DetourCrowd.h"
#undef  TEXT
#include "thread_local/storage.h"
#include "thread_local/storage_game.h"
#include "comp/scene.h"
#include "constants/dt_crowd.h"

#include "event_proto/scene_event.pb.h"

void SceneCrowdSystem::AfterEnterSceneHandler(const AfterEnterScene& message)
{
    const auto player = entt::to_entity(message.entity());
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "scene not found";
        return;
    }
    const auto scene_entity = tls.registry.try_get<SceneEntity>(player);
    if (nullptr == scene_entity || !tls.scene_registry.valid(scene_entity->scene_entity_))
    {
        LOG_ERROR << "scene not found";
        return;
    }
    auto scene = scene_entity->scene_entity_;
    auto dt_crowd = tls.scene_registry.try_get<dtCrowd>(scene);
    if (nullptr == dt_crowd)
    {
        return;
    }
    auto transform = tls.registry.try_get<Transform>(player);
    if (nullptr == transform)
    {
        LOG_ERROR << "player location not found" << tls.registry.get<Guid>(player);
        return;
    }
    dtCrowdAgentParams ap;
    memset(&ap, 0, sizeof(ap));
    ap.radius = kAgentRadius;
    ap.height = kAgentHeight;
    ap.maxAcceleration = 8.0f;
    ap.maxSpeed = 3.5f;
    ap.collisionQueryRange = ap.radius * 12.0f;
    ap.pathOptimizationRange = ap.radius * 30.0f;
    ap.updateFlags = 0;
    ap.updateFlags |= DT_CROWD_ANTICIPATE_TURNS;
    ap.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
    ap.updateFlags |= DT_CROWD_OPTIMIZE_TOPO;
    ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
    ap.updateFlags |= DT_CROWD_SEPARATION;
    ap.obstacleAvoidanceType = kObstacleAvoidanceType;
    ap.separationWeight = kSeparationWeight;
    dtReal p[] = { transform->localtion().x(), transform->localtion().y(), transform->localtion().z() };
    int idx = dt_crowd->addAgent(p, ap, nullptr);
} 

void SceneCrowdSystem::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{

}

