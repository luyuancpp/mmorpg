#include "logic/event/combat_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2combat_event()
{
tls_lua_state.new_usertype<BeKillEvent>("BeKillEvent",
"caster",
sol::property(&BeKillEvent::caster, &BeKillEvent::set_caster),
"target",
sol::property(&BeKillEvent::target, &BeKillEvent::set_target),
"DebugString",
&BeKillEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
