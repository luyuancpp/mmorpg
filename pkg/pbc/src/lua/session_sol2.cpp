#include "common/session.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2session()
{
tls_lua_state.new_usertype<SessionDetails>("SessionDetails",
"session_id",
sol::property(&SessionDetails::session_id, &SessionDetails::set_session_id),
"DebugString",
&SessionDetails::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
