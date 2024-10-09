#include "logic/event/buff_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2buff_event()
{
tls_lua_state.new_usertype<SkillExecutedEvent>("SkillExecutedEvent",
"skill_id",
sol::property(&SkillExecutedEvent::skill_id, &SkillExecutedEvent::set_skill_id),
"caster_id",
sol::property(&SkillExecutedEvent::caster_id, &SkillExecutedEvent::set_caster_id),
"DebugString",
&SkillExecutedEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
