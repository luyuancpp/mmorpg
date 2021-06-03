#ifndef COMMON_SRC_UTIL_UTILITY_H_
#define COMMON_SRC_UTIL_UTILITY_H_

#include <cinttypes>
#include <sstream>

namespace common
{
template <typename T>
T StringToNumber(const std::string& str)
{
    std::istringstream iss(str);
    T num;
    iss >> num;
    return num;
}

}//namespace common

#endif//COMMON_SRC_UTIL_UTILITY_H_