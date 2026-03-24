#pragma once
#include <cstdint>

#include "proto/friend/friend.pb.h"

constexpr uint32_t FriendServiceAddFriendMessageId = 11;
constexpr uint32_t FriendServiceAddFriendIndex = 0;
#define FriendServiceAddFriendMethod  ::FriendService_Stub::descriptor()->method(0)

constexpr uint32_t FriendServiceAcceptFriendMessageId = 7;
constexpr uint32_t FriendServiceAcceptFriendIndex = 1;
#define FriendServiceAcceptFriendMethod  ::FriendService_Stub::descriptor()->method(1)

constexpr uint32_t FriendServiceRejectFriendMessageId = 2;
constexpr uint32_t FriendServiceRejectFriendIndex = 2;
#define FriendServiceRejectFriendMethod  ::FriendService_Stub::descriptor()->method(2)

constexpr uint32_t FriendServiceRemoveFriendMessageId = 53;
constexpr uint32_t FriendServiceRemoveFriendIndex = 3;
#define FriendServiceRemoveFriendMethod  ::FriendService_Stub::descriptor()->method(3)

constexpr uint32_t FriendServiceGetFriendListMessageId = 76;
constexpr uint32_t FriendServiceGetFriendListIndex = 4;
#define FriendServiceGetFriendListMethod  ::FriendService_Stub::descriptor()->method(4)

constexpr uint32_t FriendServiceGetPendingRequestsMessageId = 12;
constexpr uint32_t FriendServiceGetPendingRequestsIndex = 5;
#define FriendServiceGetPendingRequestsMethod  ::FriendService_Stub::descriptor()->method(5)
