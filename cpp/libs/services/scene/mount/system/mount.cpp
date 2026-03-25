#include "mount.h"

#include "table/proto/tip/common_error_tip.pb.h"
#include "table/proto/tip/mount_error_tip.pb.h"
#include "actor/action_state/constants/actor_state.h"
#include "actor/action_state/system/actor_action_state.h"
#include "macros/return_define.h"

uint32_t MountSystem::MountActor(const entt::entity actorEntity)
{
    RETURN_FALSE_ON_ERROR(ActorActionStateSystem::TryPerformAction(actorEntity, kActorActionMountActor, kActorStateMounted));
    
    return kSuccess;
}

uint32_t MountSystem::UnmountActor(const entt::entity actorEntity)
{
    if (!ActorActionStateSystem::HasState(actorEntity, kActorStateMounted)) {
        return kMountNotMounted;
    }
    // TODO: Clear mount state and detach rider from mount entity
    return kSuccess;
}

uint32_t MountSystem::ForceUnmountActor(entt::entity actorEntity)
{
    // TODO: Force-remove mount state regardless of current action/combat state
    return kSuccess;
}

uint32_t MountSystem::InterruptAndUnmountActor(const entt::entity actorEntity)
{
    RETURN_FALSE_ON_ERROR(ActorActionStateSystem::RemoveState(actorEntity, kActorStateMounted));
    
    return kSuccess;
}
