#pragma once

#include <string>
#include <vector>
#include "type_define/type_define.h"
#include "core/utils/id/node_id_generator.h"
#include <utils/id/snow_flake.h>

constexpr char kSessionBinMetaKey[] = "x-session-detail-bin";
using SessionIdGenerator = TransientNodeCompositeIdGenerator<uint32_t, kNodeBits>;

void ParseIpPort(const std::string &input, std::string &ip, uint16_t &port);

std::string FormatIpAndPort(const std::string &ip, uint32_t port);

class SessionDetails;
std::vector<std::string> SerializeSessionDetails(const SessionDetails &sessionDetails);

NodeId GetGateNodeId(SessionId session_id);
