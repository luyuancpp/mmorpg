#include "common/statistics.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2statistics()
{
tls_lua_state.new_usertype<MessageStatistics>("MessageStatistics",
"count",
sol::property(&MessageStatistics::count, &MessageStatistics::set_count),
"flow_rate_second",
sol::property(&MessageStatistics::flow_rate_second, &MessageStatistics::set_flow_rate_second),
"flow_rate_total",
sol::property(&MessageStatistics::flow_rate_total, &MessageStatistics::set_flow_rate_total),
"second",
sol::property(&MessageStatistics::second, &MessageStatistics::set_second),
"DebugString",
&MessageStatistics::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
