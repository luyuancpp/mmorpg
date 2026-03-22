#include "time.h"

#include <chrono>
#include <cstdint>

#include <boost/date_time/posix_time/posix_time.hpp>

uint64_t PTimeToSeconds(const boost::posix_time::ptime& pt) {
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (pt - epoch).total_seconds();
}

uint64_t PTimeToMilliseconds(const boost::posix_time::ptime& pt) {
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (pt - epoch).total_milliseconds();
}

uint64_t PTimeToMicroseconds(const boost::posix_time::ptime& pt) {
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1), boost::posix_time::time_duration(0, 0, 0));
	return (pt - epoch).total_microseconds();
}

uint64_t TimeSystem::NowMicroseconds(){
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();
	return PTimeToMicroseconds(utc_now);
}

uint64_t TimeSystem::NowMilliseconds() {
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();
	return PTimeToMilliseconds(utc_now);
}

uint64_t TimeSystem::NowSeconds() {
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();
	return PTimeToSeconds(utc_now);
}

uint64_t TimeSystem::NowMillisecondsUTC() {
	boost::posix_time::ptime utc_now = boost::posix_time::microsec_clock::universal_time();
	return PTimeToMilliseconds(utc_now);
}

uint64_t TimeSystem::NowMicrosecondsUTC(){
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();
	return PTimeToMicroseconds(utc_now);
}

uint64_t TimeSystem::NowSecondsUTC() {
	boost::posix_time::ptime utc_now = boost::posix_time::second_clock::universal_time();
	return PTimeToSeconds(utc_now);
}
