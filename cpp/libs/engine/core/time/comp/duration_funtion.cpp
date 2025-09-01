#include "duration_funtion.h"
#include <iomanip>
#include <sstream>
#include <ctime>

#include "core/utils/utility/utility.h"
#include "timer_task_comp.h"

void GetLocalTime(const time_t* ptime, struct tm* pTm)
{
#if defined (WIN32)
localtime_s(pTm, ptime);
#else 
localtime_r(ptime, pTm);
#endif
}


time_t StringToTime(const std::string & timeStr, const char* formatStr)
{
#ifdef __linux__
    tm rt = {};
    strptime(timeStr.c_str(), formatStr, &rt);
    return std::mktime(&rt);
#endif//

    std::tm t = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&t, formatStr);
    return std::mktime(&t);
}

time_t YmdHmsStringToTime(const std::string & timeStr)
{
    return StringToTime(timeStr, "%Y-%m-%d %H-%M-%S");
}

time_t GetZeroUnixTime(time_t n)
{
    struct tm  tn, tm_today {};
    memset(&tm_today, 0, sizeof(struct tm));
	GetLocalTime(&n, &tn);
    tm_today.tm_year = tn.tm_year;
    tm_today.tm_mon = tn.tm_mon;
    tm_today.tm_mday = tn.tm_mday;
    return mktime(&tm_today);
}

//本周的周一
muduo::Date GetWeekBeginDay(muduo::Date & dateNow)
{
	int32_t nWeekDay = dateNow.weekDay();
	if (0 == nWeekDay)
	{
		nWeekDay = 7;
	}
	muduo::Date weekBeginDay(dateNow.julianDayNumber() - (nWeekDay - 1));
//     muduo::Date weekBeginDay(dateNow.julianDayNumber() - dateNow.weekDay());
    return weekBeginDay;
}


muduo::Date GetTodayDate(time_t now)
{
	struct tm t;
	GetLocalTime(&now, &t);
    muduo::Date dateToday(t);
    return dateToday;
}

time_t GetTime(muduo::Date & dateTime, const std::string & hms)
{
    return YmdHmsStringToTime(dateTime.toIsoString() + " " + hms);
}

time_t GetWeekTime(muduo::Date & dateWeekBegin, const std::string & whms)
{
    std::size_t pW = whms.find(" ");
    int32_t dayNumber = StringToNumber<int32_t>(whms.substr(0, pW + 1));
    if (dayNumber == 0)
    {
        dayNumber = 7;
    }
    std::string szTime = whms.substr(pW + 1, whms.length());
    int32_t n = dayNumber - dateWeekBegin.weekDay();

    muduo::Date beginDate( dateWeekBegin.julianDayNumber() + n);
    return GetTime(beginDate, szTime);

}







