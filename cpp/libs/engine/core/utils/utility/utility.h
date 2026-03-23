#pragma once

#include <cmath>
#include <limits>  // for std::numeric_limits

inline bool IsZero(const double value) {
    return std::fabs(value) < std::numeric_limits<double>::epsilon();
}

inline bool IsZero(const float value) {
    return std::fabs(value) < std::numeric_limits<float>::epsilon();
}