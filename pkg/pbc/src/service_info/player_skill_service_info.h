#pragma once
#include <cstdint>

#include "proto/game/player_skill.pb.h"


constexpr uint32_t ClientPlayerSkillServiceReleaseSkillMessageId = 17;
constexpr uint32_t ClientPlayerSkillServiceReleaseSkillIndex = 0;
#define ClientPlayerSkillServiceReleaseSkillMethod  ::ClientPlayerSkillService_Stub::descriptor()->method(0)

constexpr uint32_t ClientPlayerSkillServiceNotifySkillUsedMessageId = 46;
constexpr uint32_t ClientPlayerSkillServiceNotifySkillUsedIndex = 1;
#define ClientPlayerSkillServiceNotifySkillUsedMethod  ::ClientPlayerSkillService_Stub::descriptor()->method(1)

constexpr uint32_t ClientPlayerSkillServiceNotifySkillInterruptedMessageId = 53;
constexpr uint32_t ClientPlayerSkillServiceNotifySkillInterruptedIndex = 2;
#define ClientPlayerSkillServiceNotifySkillInterruptedMethod  ::ClientPlayerSkillService_Stub::descriptor()->method(2)

constexpr uint32_t ClientPlayerSkillServiceGetSkillListMessageId = 70;
constexpr uint32_t ClientPlayerSkillServiceGetSkillListIndex = 3;
#define ClientPlayerSkillServiceGetSkillListMethod  ::ClientPlayerSkillService_Stub::descriptor()->method(3)


