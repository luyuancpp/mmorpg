#pragma once
#include <cstdint>

#include "proto/mail/mail.pb.h"

constexpr uint32_t ClientPlayerMailListMailsMessageId = 273;
constexpr uint32_t ClientPlayerMailListMailsIndex = 0;
#define ClientPlayerMailListMailsMethod  ::ClientPlayerMail_Stub::descriptor()->method(0)

constexpr uint32_t ClientPlayerMailReadMailMessageId = 269;
constexpr uint32_t ClientPlayerMailReadMailIndex = 1;
#define ClientPlayerMailReadMailMethod  ::ClientPlayerMail_Stub::descriptor()->method(1)

constexpr uint32_t ClientPlayerMailDeleteMailsMessageId = 274;
constexpr uint32_t ClientPlayerMailDeleteMailsIndex = 2;
#define ClientPlayerMailDeleteMailsMethod  ::ClientPlayerMail_Stub::descriptor()->method(2)

constexpr uint32_t ClientPlayerMailDeleteReadMailsMessageId = 267;
constexpr uint32_t ClientPlayerMailDeleteReadMailsIndex = 3;
#define ClientPlayerMailDeleteReadMailsMethod  ::ClientPlayerMail_Stub::descriptor()->method(3)

constexpr uint32_t ClientPlayerMailClaimMailAttachmentsMessageId = 270;
constexpr uint32_t ClientPlayerMailClaimMailAttachmentsIndex = 4;
#define ClientPlayerMailClaimMailAttachmentsMethod  ::ClientPlayerMail_Stub::descriptor()->method(4)

constexpr uint32_t ClientPlayerMailNotifyMailEventMessageId = 268;
constexpr uint32_t ClientPlayerMailNotifyMailEventIndex = 5;
#define ClientPlayerMailNotifyMailEventMethod  ::ClientPlayerMail_Stub::descriptor()->method(5)
