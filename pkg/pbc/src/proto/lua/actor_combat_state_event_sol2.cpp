#include "logic/event/actor_combat_state_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_combat_state_event()
{
tls_lua_state.new_usertype<SilenceAddedPbEvent>("SilenceAddedPbEvent",
"actor_entity",
sol::property(&SilenceAddedPbEvent::actor_entity, &SilenceAddedPbEvent::set_actor_entity),
"source_buff_id",
sol::property(&SilenceAddedPbEvent::source_buff_id, &SilenceAddedPbEvent::set_source_buff_id),
"DebugString",
&SilenceAddedPbEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SilenceRemovedEvent>("SilenceRemovedEvent",
"actor_entity",
sol::property(&SilenceRemovedEvent::actor_entity, &SilenceRemovedEvent::set_actor_entity),
"source_buff_id",
sol::property(&SilenceRemovedEvent::source_buff_id, &SilenceRemovedEvent::set_source_buff_id),
"DebugString",
&SilenceRemovedEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
