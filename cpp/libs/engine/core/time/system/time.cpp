#include "time.h"

#include <boost/date_time/posix_time/posix_time.hpp>

namespace {
const boost::posix_time::ptime kUnixEpoch(
	boost::gregorian::date(1970, 1, 1),
	boost::posix_time::time_duration(0, 0, 0));
}

uint64_t PTimeToSeconds(const boost::posix_time::ptime& pt) {
	return (pt - kUnixEpoch).total_seconds();
}

uint64_t PTimeToMilliseconds(const boost::posix_time::ptime& pt) {
	return (pt - kUnixEpoch).total_milliseconds();
}

uint64_t PTimeToMicroseconds(const boost::posix_time::ptime& pt) {
	return (pt - kUnixEpoch).total_microseconds();
}

uint64_t TimeSystem::NowMicroseconds(){
	return PTimeToMicroseconds(boost::posix_time::microsec_clock::universal_time());
}

uint64_t TimeSystem::NowMilliseconds() {
	return PTimeToMilliseconds(boost::posix_time::microsec_clock::universal_time());
}

uint64_t TimeSystem::NowSeconds() {
	return PTimeToSeconds(boost::posix_time::microsec_clock::universal_time());
}

uint64_t TimeSystem::NowMillisecondsUTC() {
	return PTimeToMilliseconds(boost::posix_time::microsec_clock::universal_time());
}

uint64_t TimeSystem::NowMicrosecondsUTC(){
	return PTimeToMicroseconds(boost::posix_time::microsec_clock::universal_time());
}

uint64_t TimeSystem::NowSecondsUTC() {
	return PTimeToSeconds(boost::posix_time::second_clock::universal_time());
}
