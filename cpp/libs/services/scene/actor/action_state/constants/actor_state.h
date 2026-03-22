#pragma once
#include <cstdint>

enum eActorState : uint32_t {
    kActorStateCombat,            // In combat
    kActorStateTeamFollow,        // Following team
    kActorStateMounted,           // Mounted
    kActorStateActorStateMax      // Max state sentinel
};

enum eActorAction : uint32_t {
    kActorActionUseSkill,        // Use skill
    kActorActionJoinFollow,      // Join team follow
    kActorActionMountActor,      // Mount
    kActorActionUnmountActor,    // Unmount
};

enum eActionStateMode : uint32_t {
    kActionStateMutualExclusion,// Mutually exclusive, blocked
    kActionStatePermitted,// Permitted
    kActionStateInterrupt,// Interrupt current and execute
};
