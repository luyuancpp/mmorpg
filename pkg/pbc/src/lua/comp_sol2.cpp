#include "common/comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2comp()
{
tls_lua_state.new_usertype<Vector3>("Vector3",
"x",
sol::property(&Vector3::x, &Vector3::set_x),
"y",
sol::property(&Vector3::y, &Vector3::set_y),
"z",
sol::property(&Vector3::z, &Vector3::set_z),
"DebugString",
&Vector3::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
