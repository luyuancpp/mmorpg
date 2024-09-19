#include "time_util.h"

#include <chrono>
#include <cstdint>

#include <boost/date_time/posix_time/posix_time.hpp>

uint64_t TimeUtil::NowMilliseconds() {
	// 获取当前 UTC 时间
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();

	// 将时间转换为自1970年1月1日以来的毫秒数
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (utc_now - epoch).total_milliseconds();
}

uint64_t TimeUtil::NowSeconds() {
	// 获取当前 UTC 时间
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();

	// 将时间转换为自1970年1月1日以来的秒数
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (utc_now - epoch).total_seconds();
}

uint64_t TimeUtil::NowMillisecondsUTC() {
	// 获取当前 UTC 时间
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();

	// 将时间转换为自1970年1月1日以来的毫秒数
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (utc_now - epoch).total_milliseconds();
}

uint64_t TimeUtil::NowMicrosecondsUTC()
{
	// 获取当前 UTC 时间
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();

	// 将时间转换为自1970年1月1日以来的微秒数
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (utc_now - epoch).total_microseconds();
}

uint64_t TimeUtil::NowSecondsUTC() {
	// 获取当前 UTC 时间
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();

	// 将时间转换为自1970年1月1日以来的秒数
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (utc_now - epoch).total_seconds();
}
