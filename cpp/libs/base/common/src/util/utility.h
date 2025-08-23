#pragma once

#include <cmath>
#include <limits>  // for std::numeric_limits
#include <sstream>

template <typename T>
T StringToNumber(const std::string& str)
{
    std::istringstream iss(str);
    T num;
    iss >> num;
    return num;
}

//todo std::is_zero
bool IsZero(const double value) {
    return std::fabs(value) < std::numeric_limits<double>::epsilon();
}

bool IsZero(const float value) {
    return std::fabs(value) < std::numeric_limits<float>::epsilon();
}