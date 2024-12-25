#include "time_system.h"

#include <chrono>
#include <cstdint>

#include <boost/date_time/posix_time/posix_time.hpp>

uint64_t PTimeToSeconds(const boost::posix_time::ptime& pt) {
	// 1970年1月1日的时间点
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (pt - epoch).total_seconds();  // 返回自1970年以来的秒数
}

uint64_t PTimeToMilliseconds(const boost::posix_time::ptime& pt) {
	// 1970年1月1日的时间点
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (pt - epoch).total_milliseconds();  // 返回自1970年以来的毫秒数
}

uint64_t PTimeToMicroseconds(const boost::posix_time::ptime& pt) {
	// 1970年1月1日的时间点
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (pt - epoch).total_microseconds();  // 返回自1970年以来的微秒数
}

uint64_t TimeUtil::NowMicroseconds(){
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();
	return PTimeToMicroseconds(utc_now);
}

uint64_t TimeUtil::NowMilliseconds() {
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();
	return PTimeToMilliseconds(utc_now);
}

uint64_t TimeUtil::NowSeconds() {
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();
	return PTimeToSeconds(utc_now);
}

uint64_t TimeUtil::NowMillisecondsUTC() {
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();
	return PTimeToMilliseconds(utc_now);
}

uint64_t TimeUtil::NowMicrosecondsUTC(){
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();
	return PTimeToMicroseconds(utc_now);
}

uint64_t TimeUtil::NowSecondsUTC() {
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();
	return PTimeToSeconds(utc_now);
}
