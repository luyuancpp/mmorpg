#pragma once
#include <cstdint>

#include "proto/mail/mail_admin.pb.h"

constexpr uint32_t MailAdminSendPersonalMailMessageId = 271;
constexpr uint32_t MailAdminSendPersonalMailIndex = 0;
#define MailAdminSendPersonalMailMethod  ::MailAdmin_Stub::descriptor()->method(0)

constexpr uint32_t MailAdminSendSystemMailMessageId = 272;
constexpr uint32_t MailAdminSendSystemMailIndex = 1;
#define MailAdminSendSystemMailMethod  ::MailAdmin_Stub::descriptor()->method(1)
