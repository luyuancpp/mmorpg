#pragma once
#include <cstdint>

enum eActorState : uint32_t {
    kActorStateCombat,            // 正在战斗
    kActorStateTeamFollow,        // 正在组队跟随
    kActorStateMounted,           // 正在坐骑上
    kActorStateActorStateMax      // 状态的边界值
};

enum eActorAction {
    kActorActionUseSkill,        // 尝试使用技能
    kActorActionJoinFollow,      // 尝试进入组队跟随
    // 可以继续添加其他动作
};

enum eActionStateFlag
{
    kActionStateMutualExclusion,// 互斥，禁止执行
    kActionStateAllow,// 不互斥，允许执行
    kActionStateInterruptAndExecute,// 打断并执行
};
