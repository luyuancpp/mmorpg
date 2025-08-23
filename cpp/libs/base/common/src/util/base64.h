#pragma once
#include <string>
#include <vector>

std::string Base64Encode(const std::string& input);
std::vector<uint8_t> Base64Decode(const std::string& input);