#include "logic/component/npc_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2npc_comp()
{
tls_lua_state.new_usertype<Npc>("Npc",
"DebugString",
&Npc::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
