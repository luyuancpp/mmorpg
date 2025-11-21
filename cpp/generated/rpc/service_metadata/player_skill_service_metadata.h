#pragma once
#include <cstdint>

#include "proto/service/cpp/rpc/scene/player_skill.pb.h"


constexpr uint32_t SceneSkillClientPlayerReleaseSkillMessageId = 55;
constexpr uint32_t SceneSkillClientPlayerReleaseSkillIndex = 0;
#define SceneSkillClientPlayerReleaseSkillMethod  ::SceneSkillClientPlayer_Stub::descriptor()->method(0)

constexpr uint32_t SceneSkillClientPlayerNotifySkillUsedMessageId = 78;
constexpr uint32_t SceneSkillClientPlayerNotifySkillUsedIndex = 1;
#define SceneSkillClientPlayerNotifySkillUsedMethod  ::SceneSkillClientPlayer_Stub::descriptor()->method(1)

constexpr uint32_t SceneSkillClientPlayerNotifySkillInterruptedMessageId = 73;
constexpr uint32_t SceneSkillClientPlayerNotifySkillInterruptedIndex = 2;
#define SceneSkillClientPlayerNotifySkillInterruptedMethod  ::SceneSkillClientPlayer_Stub::descriptor()->method(2)

constexpr uint32_t SceneSkillClientPlayerGetSkillListMessageId = 46;
constexpr uint32_t SceneSkillClientPlayerGetSkillListIndex = 3;
#define SceneSkillClientPlayerGetSkillListMethod  ::SceneSkillClientPlayer_Stub::descriptor()->method(3)


