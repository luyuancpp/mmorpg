#pragma once

#include <cstdint>
#include <string>

std::string get_local_ip();
int get_available_port(uint32_t start_port = 10001, uint32_t max_port = 65535);

