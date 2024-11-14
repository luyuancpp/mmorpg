#include "logic/event/actor_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2actor_event()
{
tls_lua_state.new_usertype<InterruptCurrentStatePbEvent>("InterruptCurrentStatePbEvent",
"actor_entity",
sol::property(&InterruptCurrentStatePbEvent::actor_entity, &InterruptCurrentStatePbEvent::set_actor_entity),
"actor_action",
sol::property(&InterruptCurrentStatePbEvent::actor_action, &InterruptCurrentStatePbEvent::set_actor_action),
"actor_state",
sol::property(&InterruptCurrentStatePbEvent::actor_state, &InterruptCurrentStatePbEvent::set_actor_state),
"DebugString",
&InterruptCurrentStatePbEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
