#pragma once

// https://www.cnblogs.com/xutopia/p/15820428.html

namespace log_color
{
    constexpr const char *kNone = "\033[m";
    constexpr const char *kRed = "\033[0;32;31m";
    constexpr const char *kLightRed = "\033[1;31m";
    constexpr const char *kGreen = "\033[0;32;32m";
    constexpr const char *kLightGreen = "\033[1;32m";
    constexpr const char *kBlue = "\033[0;32;34m";
    constexpr const char *kLightBlue = "\033[1;34m";
    constexpr const char *kDarkGray = "\033[1;30m";
    constexpr const char *kCyan = "\033[0;36m";
    constexpr const char *kLightCyan = "\033[1;36m";
    constexpr const char *kPurple = "\033[0;35m";
    constexpr const char *kLightPurple = "\033[1;35m";
    constexpr const char *kBrown = "\033[0;33m";
    constexpr const char *kYellow = "\033[1;33m";
    constexpr const char *kLightGray = "\033[0;37m";
    constexpr const char *kWhite = "\033[1;37m";
}