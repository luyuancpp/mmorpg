#include "console_log.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "log/macros/game_log.h"

#include "log/constants/log_color_constants.h"

// https://www.cnblogs.com/xutopia/p/15820428.html

thread_local std::string console_msg;

void LogToConsole(const char *msg, int len)
{
    if (len <= static_cast<int>(kLoginInfoInex))
    {
        fwrite(msg, 1, len, stdout);
        return;
    }

    const char *levelStart = msg + kLoginInfoInex;
    const char *color = log_color::kLightRed;

    if (std::strncmp(levelStart, "TRACE", 5) == 0)
        color = log_color::kLightGray;
    else if (std::strncmp(levelStart, "DEBUG", 5) == 0)
        color = log_color::kWhite;
    else if (std::strncmp(levelStart, "INFO", 4) == 0)
        color = log_color::kGreen;
    else if (std::strncmp(levelStart, "WARN", 4) == 0)
        color = log_color::kYellow;
    else if (std::strncmp(levelStart, "ERROR", 5) == 0)
        color = log_color::kRed;
    else if (std::strncmp(levelStart, "FATAL", 5) == 0)
        color = log_color::kLightRed;

    console_msg.clear();
    console_msg.append(color);
    console_msg.append(msg, len);
    console_msg.append(log_color::kNone);
    fwrite(console_msg.data(), 1, console_msg.length(), stdout);
}
