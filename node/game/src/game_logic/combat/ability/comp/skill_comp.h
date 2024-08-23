#pragma once
#include "time/comp/timer_task_comp.h"
#include "logic/component/actor_comp.pb.h"


//前摇技能timer
struct CastingTimerComp
{
    TimerTaskComp timer;
};

//后摇技能timer
struct RecoveryTimerComp
{
    TimerTaskComp timer;
};

//引导技能timer
struct ChannelFinishTimerComp
{
    TimerTaskComp timer;
};
//引导技能timer
struct ChannelIntervalTimerComp
{
    TimerTaskComp timer;
};


struct SkillContextComp
{
	uint64_t caster = { entt::null }; // 施法者
	entt::entity target = { entt::null }; // 目标（如果有）
	uint32_t abilityId = { 0 }; // 技能唯一id
	uint32_t abilityTableId = { 0 };//技能表id
	Transform transform;//施法位置
	uint64_t castTime = 0; // 施法时间
	std::string state;              // 当前状态
	std::unordered_map<std::string, uint32_t> additionalData; // 附加数据
};

using AbilityContextPtrComp = std::shared_ptr<SkillContextComp>;

// 存储技能上下文信息的容器
using  SkillContextMap = std::unordered_map<uint32_t, SkillContextComp>;