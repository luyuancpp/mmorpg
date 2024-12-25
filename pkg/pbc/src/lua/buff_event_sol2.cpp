#include "logic/event/buff_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2buff_event()
{
tls_lua_state.new_usertype<BuffTestEvet>("BuffTestEvet",
"period",
sol::property(&BuffTestEvet::period, &BuffTestEvet::set_period),
"periodic_timer",
sol::property(&BuffTestEvet::periodic_timer, &BuffTestEvet::set_periodic_timer),
"period_done",
sol::property(&BuffTestEvet::period_done, &BuffTestEvet::set_period_done),
"DebugString",
&BuffTestEvet::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
