#include "logic/event/npc_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2npc_event()
{
tls_lua_state.new_usertype<InitializeNpcComponentsEvent>("InitializeNpcComponentsEvent",
"entity",
sol::property(&InitializeNpcComponentsEvent::entity, &InitializeNpcComponentsEvent::set_entity),
"DebugString",
&InitializeNpcComponentsEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
