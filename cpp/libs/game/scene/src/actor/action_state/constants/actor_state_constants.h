#pragma once
#include <cstdint>

enum eActorState : uint32_t {
    kActorStateCombat,            // 正在战斗
    kActorStateTeamFollow,        // 正在组队跟随
    kActorStateMounted,           // 正在坐骑上
    kActorStateActorStateMax      // 状态的边界值
};

enum eActorAction : uint32_t {
    kActorActionUseSkill,        // 尝试使用技能
    kActorActionJoinFollow,      // 尝试进入组队跟随
    kActorActionMountActor,      // 尝试上坐骑
    kActorActionUnmountActor,    //尝试下坐骑
    // 可以继续添加其他动作
};

enum eActionStateMode : uint32_t {
    kActionStateMutualExclusion,// 互斥，禁止执行
    kActionStatePermitted,// 不互斥，允许执行
    kActionStateInterrupt,// 打断并执行
};
