#pragma once
#include <cstdint>

#include "proto/guild/guild_internal.pb.h"

constexpr uint32_t GuildInternalListAppliedAssetOpsSinceMessageId = 244;
constexpr uint32_t GuildInternalListAppliedAssetOpsSinceIndex = 0;
#define GuildInternalListAppliedAssetOpsSinceMethod  ::GuildInternal_Stub::descriptor()->method(0)
