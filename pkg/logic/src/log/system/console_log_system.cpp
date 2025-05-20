#include "console_log_system.h"

#include <stdio.h>
#include <string>

#include "log/macros/game_log.h"

#include "log/constants/log_color_constants.h"

//https://www.cnblogs.com/xutopia/p/15820428.html

thread_local std::string console_msg;

void LogToConsole(const char* msg, int len)
{
    console_msg.assign(msg, len );
    if (std::string{ msg, kLoginInfoInex, 5 }.compare("TRACE") == 0)
    {
        console_msg = LOG_LIGHT_GRAY + console_msg;
    }
    else if (std::string{ msg, kLoginInfoInex, 5 }.compare("DEBUG") == 0)
    {
        console_msg = LOG_WHITE + console_msg;
    }
    if (std::string{ msg, kLoginInfoInex, 4 }.compare("INFO") == 0)
    {
        console_msg = LOG_GREEN + console_msg;
    }
    else if (std::string{ msg, kLoginInfoInex, 4 }.compare("WARN") == 0)
    {
        console_msg = LOG_YELLOW + console_msg;
    }
    else if (std::string{ msg, kLoginInfoInex, 5 }.compare("ERROR") == 0)
    {
        console_msg = LOG_RED + console_msg;
    }
    else 
    {
        console_msg = LOG_LIGHT_RED + console_msg;
    }
    fwrite(console_msg.data(), 1, console_msg.length(), stdout);
}
