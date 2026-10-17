#pragma once
#include <cstdint>

#include "proto/mail/mail_admin.pb.h"

constexpr uint32_t MailAdminSendPersonalMailMessageId = 268;
constexpr uint32_t MailAdminSendPersonalMailIndex = 0;
#define MailAdminSendPersonalMailMethod  ::MailAdmin_Stub::descriptor()->method(0)

constexpr uint32_t MailAdminSendSystemMailMessageId = 286;
constexpr uint32_t MailAdminSendSystemMailIndex = 1;
#define MailAdminSendSystemMailMethod  ::MailAdmin_Stub::descriptor()->method(1)

constexpr uint32_t MailAdminSendGuildMailMessageId = 290;
constexpr uint32_t MailAdminSendGuildMailIndex = 2;
#define MailAdminSendGuildMailMethod  ::MailAdmin_Stub::descriptor()->method(2)

constexpr uint32_t MailAdminImportMailRecipientsMessageId = 301;
constexpr uint32_t MailAdminImportMailRecipientsIndex = 3;
#define MailAdminImportMailRecipientsMethod  ::MailAdmin_Stub::descriptor()->method(3)

constexpr uint32_t MailAdminSealMailCampaignMessageId = 302;
constexpr uint32_t MailAdminSealMailCampaignIndex = 4;
#define MailAdminSealMailCampaignMethod  ::MailAdmin_Stub::descriptor()->method(4)
