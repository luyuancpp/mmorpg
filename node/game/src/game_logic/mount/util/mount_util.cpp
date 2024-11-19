#include "mount_util.h"

#include "common_error_tip.pb.h"
#include "mount_error_tip.pb.h"
#include "game_logic/actor/action_state/constants/actor_state_constants.h"
#include "game_logic/actor/action_state/util/actor_action_state_util.h"
#include "macros/return_define.h"

inline uint32_t MountUtil::MountActor(const entt::entity actorEntity)
{
    RETURN_FALSE_ON_ERROR(ActorActionStateUtil::TryPerformAction(actorEntity, kActorActionMountActor));
    // 执行上坐骑逻辑
    
    return kSuccess;
}

uint32_t MountUtil::UnmountActor(const entt::entity actorEntity)
{
    if (!ActorActionStateUtil::HasState(actorEntity, kActorStateMounted)) {
        return kMountNotMounted;
    }
    // 执行下坐骑逻辑
    return kSuccess;
}

uint32_t MountUtil::ForceUnmountActor(entt::entity actorEntity)
{
    return kSuccess;
}

uint32_t MountUtil::InterruptAndUnmountActor(const entt::entity actorEntity)
{
    RETURN_FALSE_ON_ERROR(ActorActionStateUtil::RemoveState(actorEntity, kActorStateMounted));
    
    return kSuccess;
}
