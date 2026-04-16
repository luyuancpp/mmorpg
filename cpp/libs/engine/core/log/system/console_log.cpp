#include "console_log.h"

#include <cstdio>
#include <cstring>

#include "log/macros/game_log.h"

#include "log/constants/log_color_constants.h"

// https://www.cnblogs.com/xutopia/p/15820428.html

void LogToConsole(const char *msg, int len)
{
    if (len <= static_cast<int>(kLoginInfoInex))
    {
        fwrite(msg, 1, len, stdout);
        return;
    }

    const char *color = log_color::kLightRed;

    switch (msg[kLoginInfoInex])
    {
    case 'T':
        color = log_color::kLightGray;
        break; // TRACE
    case 'D':
        color = log_color::kWhite;
        break; // DEBUG
    case 'I':
        color = log_color::kGreen;
        break; // INFO
    case 'W':
        color = log_color::kYellow;
        break; // WARN
    case 'E':
        color = log_color::kRed;
        break; // ERROR
    case 'F':
        break; // FATAL (already kLightRed)
    default:
        break;
    }

    const auto colorLen = std::strlen(color);
    constexpr auto resetLen = sizeof("\033[m") - 1;

#ifdef _WIN32
    _lock_file(stdout);
    _fwrite_nolock(color, 1, colorLen, stdout);
    _fwrite_nolock(msg, 1, len, stdout);
    _fwrite_nolock(log_color::kNone, 1, resetLen, stdout);
    _unlock_file(stdout);
#else
    flockfile(stdout);
    fwrite_unlocked(color, 1, colorLen, stdout);
    fwrite_unlocked(msg, 1, len, stdout);
    fwrite_unlocked(log_color::kNone, 1, resetLen, stdout);
    funlockfile(stdout);
#endif
}
