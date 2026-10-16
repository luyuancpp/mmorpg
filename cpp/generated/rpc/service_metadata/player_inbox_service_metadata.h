#pragma once
#include <cstdint>

#include "proto/player_inbox/player_inbox.pb.h"

constexpr uint32_t PlayerInboxFetchItemsMessageId = 257;
constexpr uint32_t PlayerInboxFetchItemsIndex = 0;
#define PlayerInboxFetchItemsMethod  ::PlayerInbox_Stub::descriptor()->method(0)

constexpr uint32_t PlayerInboxAckItemsMessageId = 255;
constexpr uint32_t PlayerInboxAckItemsIndex = 1;
#define PlayerInboxAckItemsMethod  ::PlayerInbox_Stub::descriptor()->method(1)
