#pragma once
#include <cstdint>

#include "proto/team/team_authority.pb.h"

constexpr uint32_t TeamAuthorityReadMembershipMessageId = 332;
constexpr uint32_t TeamAuthorityReadMembershipIndex = 0;
#define TeamAuthorityReadMembershipMethod  ::TeamAuthority_Stub::descriptor()->method(0)
