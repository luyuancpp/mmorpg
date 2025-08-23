#pragma once
#include <string>

#include "muduo/base/Date.h"

time_t StringToTime(const std::string & timeStr, const char* formatStr);
time_t YmdHmsStringToTime(const std::string & timeStr);
time_t GetZeroUnixTime(time_t n);

muduo::Date GetWeekBeginDay(muduo::Date & dateNow);
muduo::Date GetTodayDate(time_t now);

time_t GetTime(muduo::Date & dateTime, const std::string & hms);

time_t GetWeekTime(muduo::Date & dateToday, const std::string & whms);





