#pragma once

#include <cstdint>
#include <string>

std::string get_local_ip();

uint16_t get_available_port(uint16_t start_port = 10001, uint16_t max_port = 65535);

void ParseIpPort(const std::string& input, std::string& ip, uint16_t& port);


std::string FormatIpAndPort(const std::string& ip, uint32_t port);
