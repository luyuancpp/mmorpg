#include "logic/event/npc_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2npc_event()
{
tls_lua_state.new_usertype<InitializeNpcComponentsEvent>("InitializeNpcComponentsEvent",
"actor_entity",
sol::property(&InitializeNpcComponentsEvent::actor_entity, &InitializeNpcComponentsEvent::set_actor_entity),
"DebugString",
&InitializeNpcComponentsEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
