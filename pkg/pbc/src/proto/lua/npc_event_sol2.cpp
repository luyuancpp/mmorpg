#include "logic/event/npc_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2npc_event()
{
tls_lua_state.new_usertype<InitializeNpcComponents>("InitializeNpcComponents",
"entity",
sol::property(&InitializeNpcComponents::entity, &InitializeNpcComponents::set_entity),
"DebugString",
&InitializeNpcComponents::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
