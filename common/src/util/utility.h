#pragma once

#include <cinttypes>
#include <sstream>

template <typename T>
T StringToNumber(const std::string& str)
{
    std::istringstream iss(str);
    T num;
    iss >> num;
    return num;
}
