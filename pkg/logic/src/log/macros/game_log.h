#pragma once

//https://www.cnblogs.com/xutopia/p/15820428.html

#ifdef WIN32

#define LOG_NONE         "\033[m"
#define LOG_RED          "\033[0;32;31m"
#define LOG_LIGHT_RED    "\033[1;31m"
#define LOG_GREEN        "\033[0;32;32m"
#define LOG_LIGHT_GREEN  "\033[1;32m"
#define LOG_BLUE         "\033[0;32;34m"
#define LOG_LIGHT_BLUE   "\033[1;34m"
#define LOG_DARY_GRAY    "\033[1;30m"
#define LOG_CYAN         "\033[0;36m"
#define LOG_LIGHT_CYAN   "\033[1;36m"
#define LOG_PURPLE       "\033[0;35m"
#define LOG_LIGHT_PURPLE "\033[1;35m"
#define LOG_BROWN        "\033[0;33m"
#define LOG_YELLOW       "\033[1;33m"
#define LOG_LIGHT_GRAY   "\033[0;37m"
#define LOG_WHITE        "\033[1;37m"

#endif