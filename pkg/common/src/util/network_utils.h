﻿#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "type_define/type_define.h"
#include "util/node_id_generator.h"

constexpr char kSessionBinMetaKey[] = "x-session-detail-bin";
using SessionIdGenerator = NodeIdGenerator<uint64_t, 32>;

uint16_t get_available_port(uint16_t start_port = 10001, uint16_t max_port = 65535);

void ParseIpPort(const std::string& input, std::string& ip, uint16_t& port);

std::string FormatIpAndPort(const std::string& ip, uint32_t port);

bool IsZoneSingletonNodeType(uint32_t nodeType);

class SessionDetails;
std::vector<std::string> SerializeSessionDetails(const SessionDetails& sessionDetails) ;

NodeId GetGateNodeId(Guid session_id);
