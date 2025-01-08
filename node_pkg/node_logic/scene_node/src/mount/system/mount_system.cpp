#include "mount_system.h"

#include "common_error_tip.pb.h"
#include "mount_error_tip.pb.h"
#include "actor/action_state/constants/actor_state_constants.h"
#include "actor/action_state/system/actor_action_state_system.h"
#include "macros/return_define.h"

inline uint32_t MountSystem::MountActor(const entt::entity actorEntity)
{
    RETURN_FALSE_ON_ERROR(ActorActionStateSystem::TryPerformAction(actorEntity, kActorActionMountActor, kActorStateMounted));
    // 执行上坐骑逻辑
    
    return kSuccess;
}

uint32_t MountSystem::UnmountActor(const entt::entity actorEntity)
{
    if (!ActorActionStateSystem::HasState(actorEntity, kActorStateMounted)) {
        return kMountNotMounted;
    }
    // 执行下坐骑逻辑
    return kSuccess;
}

uint32_t MountSystem::ForceUnmountActor(entt::entity actorEntity)
{
    return kSuccess;
}

uint32_t MountSystem::InterruptAndUnmountActor(const entt::entity actorEntity)
{
    RETURN_FALSE_ON_ERROR(ActorActionStateSystem::RemoveState(actorEntity, kActorStateMounted));
    
    return kSuccess;
}
