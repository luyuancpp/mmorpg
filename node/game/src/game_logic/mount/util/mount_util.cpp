#include "mount_util.h"

#include "common_error_tip.pb.h"
#include "mount_error_tip.pb.h"
#include "game_logic/actor/actionstate/constants/actor_state_constants.h"
#include "game_logic/actor/actionstate/util/actor_action_state_util.h"

inline uint32_t MountUtil::MountActor(const entt::entity actorEntity)
{
    if (const auto result = ActorActionStateUtil::TryToPerformAction(actorEntity, kActorActionMountActor);
        result != kSuccess) {
        return result;
    }
    // 执行上坐骑逻辑
    
    return kSuccess;
}

uint32_t MountUtil::UnmountActor(entt::entity actorEntity)
{
    if (const auto result = ActorActionStateUtil::GetStatusCode(actorEntity, kActorActionUnmountActor, kActorStateMounted);
        result != kSuccess) {
        return result;
    }
    // 执行下坐骑逻辑
    return kSuccess;
}

uint32_t MountUtil::ForceUnmountActor(entt::entity actorEntity)
{
    return kSuccess;
}

uint32_t MountUtil::InterruptAndUnmountActor(entt::entity actorEntity)
{
    return kSuccess;
}
