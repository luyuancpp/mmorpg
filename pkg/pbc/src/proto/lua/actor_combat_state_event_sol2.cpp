#include "logic/event/actor_combat_state_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_combat_state_event()
{
tls_lua_state.new_usertype<CombatStateAddedPbEvent>("CombatStateAddedPbEvent",
"actor_entity",
sol::property(&CombatStateAddedPbEvent::actor_entity, &CombatStateAddedPbEvent::set_actor_entity),
"source_buff_id",
sol::property(&CombatStateAddedPbEvent::source_buff_id, &CombatStateAddedPbEvent::set_source_buff_id),
"state_type",
sol::property(&CombatStateAddedPbEvent::state_type, &CombatStateAddedPbEvent::set_state_type),
"DebugString",
&CombatStateAddedPbEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CombatStateRemovedPbEvent>("CombatStateRemovedPbEvent",
"actor_entity",
sol::property(&CombatStateRemovedPbEvent::actor_entity, &CombatStateRemovedPbEvent::set_actor_entity),
"source_buff_id",
sol::property(&CombatStateRemovedPbEvent::source_buff_id, &CombatStateRemovedPbEvent::set_source_buff_id),
"state_type",
sol::property(&CombatStateRemovedPbEvent::state_type, &CombatStateRemovedPbEvent::set_state_type),
"DebugString",
&CombatStateRemovedPbEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
