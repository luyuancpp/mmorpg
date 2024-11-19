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

tls_lua_state.new_usertype<SilenceRemovedPbEvent>("SilenceRemovedPbEvent",
"actor_entity",
sol::property(&SilenceRemovedPbEvent::actor_entity, &SilenceRemovedPbEvent::set_actor_entity),
"source_buff_id",
sol::property(&SilenceRemovedPbEvent::source_buff_id, &SilenceRemovedPbEvent::set_source_buff_id),
"DebugString",
&SilenceRemovedPbEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
