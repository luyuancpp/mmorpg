#pragma once
#include <cstdint>

#include "proto/guild/guild.pb.h"

constexpr uint32_t GuildServiceCreateGuildMessageId = 15;
constexpr uint32_t GuildServiceCreateGuildIndex = 0;
#define GuildServiceCreateGuildMethod  ::GuildService_Stub::descriptor()->method(0)

constexpr uint32_t GuildServiceGetGuildMessageId = 60;
constexpr uint32_t GuildServiceGetGuildIndex = 1;
#define GuildServiceGetGuildMethod  ::GuildService_Stub::descriptor()->method(1)

constexpr uint32_t GuildServiceGetPlayerGuildMessageId = 35;
constexpr uint32_t GuildServiceGetPlayerGuildIndex = 2;
#define GuildServiceGetPlayerGuildMethod  ::GuildService_Stub::descriptor()->method(2)

constexpr uint32_t GuildServiceJoinGuildMessageId = 19;
constexpr uint32_t GuildServiceJoinGuildIndex = 3;
#define GuildServiceJoinGuildMethod  ::GuildService_Stub::descriptor()->method(3)

constexpr uint32_t GuildServiceLeaveGuildMessageId = 29;
constexpr uint32_t GuildServiceLeaveGuildIndex = 4;
#define GuildServiceLeaveGuildMethod  ::GuildService_Stub::descriptor()->method(4)

constexpr uint32_t GuildServiceDisbandGuildMessageId = 38;
constexpr uint32_t GuildServiceDisbandGuildIndex = 5;
#define GuildServiceDisbandGuildMethod  ::GuildService_Stub::descriptor()->method(5)

constexpr uint32_t GuildServiceSetAnnouncementMessageId = 39;
constexpr uint32_t GuildServiceSetAnnouncementIndex = 6;
#define GuildServiceSetAnnouncementMethod  ::GuildService_Stub::descriptor()->method(6)

constexpr uint32_t GuildServiceUpdateGuildScoreMessageId = 8;
constexpr uint32_t GuildServiceUpdateGuildScoreIndex = 7;
#define GuildServiceUpdateGuildScoreMethod  ::GuildService_Stub::descriptor()->method(7)

constexpr uint32_t GuildServiceGetGuildRankMessageId = 27;
constexpr uint32_t GuildServiceGetGuildRankIndex = 8;
#define GuildServiceGetGuildRankMethod  ::GuildService_Stub::descriptor()->method(8)

constexpr uint32_t GuildServiceGetGuildRankByGuildMessageId = 52;
constexpr uint32_t GuildServiceGetGuildRankByGuildIndex = 9;
#define GuildServiceGetGuildRankByGuildMethod  ::GuildService_Stub::descriptor()->method(9)
