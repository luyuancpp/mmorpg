#pragma once
#include <cstdint>

#include "proto/logic/client_player/player_skill.pb.h"

constexpr uint32_t PlayerSkillServiceReleaseSkillMessageId = 18;
constexpr uint32_t PlayerSkillServiceReleaseSkillIndex = 0;
#define PlayerSkillServiceReleaseSkillMethod  ::PlayerSkillService_Stub::descriptor()->method(0)

constexpr uint32_t PlayerSkillServiceNotifySkillUsedMessageId = 49;
constexpr uint32_t PlayerSkillServiceNotifySkillUsedIndex = 1;
#define PlayerSkillServiceNotifySkillUsedMethod  ::PlayerSkillService_Stub::descriptor()->method(1)

constexpr uint32_t PlayerSkillServiceNotifySkillInterruptedMessageId = 58;
constexpr uint32_t PlayerSkillServiceNotifySkillInterruptedIndex = 2;
#define PlayerSkillServiceNotifySkillInterruptedMethod  ::PlayerSkillService_Stub::descriptor()->method(2)

constexpr uint32_t PlayerSkillServiceGetSkillListMessageId = 57;
constexpr uint32_t PlayerSkillServiceGetSkillListIndex = 3;
#define PlayerSkillServiceGetSkillListMethod  ::PlayerSkillService_Stub::descriptor()->method(3)

