#pragma once
#include <cstdint>

#include "logic/client_player/player_skill.pb.h"

constexpr uint32_t PlayerSkillServiceUseSkillMessageId = 18;
constexpr uint32_t PlayerSkillServiceUseSkillIndex = 0;
#define PlayerSkillServiceUseSkillMethod  ::PlayerSkillService_Stub::descriptor()->method(0)

constexpr uint32_t PlayerSkillServiceNotifySkillUsedMessageId = 49;
constexpr uint32_t PlayerSkillServiceNotifySkillUsedIndex = 1;
#define PlayerSkillServiceNotifySkillUsedMethod  ::PlayerSkillService_Stub::descriptor()->method(1)

constexpr uint32_t PlayerSkillServiceNotifySkillInterruptedMessageId = 58;
constexpr uint32_t PlayerSkillServiceNotifySkillInterruptedIndex = 2;
#define PlayerSkillServiceNotifySkillInterruptedMethod  ::PlayerSkillService_Stub::descriptor()->method(2)

