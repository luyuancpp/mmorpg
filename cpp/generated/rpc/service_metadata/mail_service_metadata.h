#pragma once
#include <cstdint>

#include "proto/mail/mail.pb.h"

constexpr uint32_t ClientPlayerMailListMailsMessageId = 267;
constexpr uint32_t ClientPlayerMailListMailsIndex = 0;
#define ClientPlayerMailListMailsMethod  ::ClientPlayerMail_Stub::descriptor()->method(0)

constexpr uint32_t ClientPlayerMailReadMailMessageId = 285;
constexpr uint32_t ClientPlayerMailReadMailIndex = 1;
#define ClientPlayerMailReadMailMethod  ::ClientPlayerMail_Stub::descriptor()->method(1)

constexpr uint32_t ClientPlayerMailClaimMailAttachmentsMessageId = 296;
constexpr uint32_t ClientPlayerMailClaimMailAttachmentsIndex = 2;
#define ClientPlayerMailClaimMailAttachmentsMethod  ::ClientPlayerMail_Stub::descriptor()->method(2)

constexpr uint32_t ClientPlayerMailDeleteMailsMessageId = 289;
constexpr uint32_t ClientPlayerMailDeleteMailsIndex = 3;
#define ClientPlayerMailDeleteMailsMethod  ::ClientPlayerMail_Stub::descriptor()->method(3)

constexpr uint32_t ClientPlayerMailClearReadMailsMessageId = 273;
constexpr uint32_t ClientPlayerMailClearReadMailsIndex = 4;
#define ClientPlayerMailClearReadMailsMethod  ::ClientPlayerMail_Stub::descriptor()->method(4)

constexpr uint32_t ClientPlayerMailNotifyMailChangedMessageId = 264;
constexpr uint32_t ClientPlayerMailNotifyMailChangedIndex = 5;
#define ClientPlayerMailNotifyMailChangedMethod  ::ClientPlayerMail_Stub::descriptor()->method(5)
