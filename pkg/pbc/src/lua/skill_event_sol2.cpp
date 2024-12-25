#include "logic/event/skill_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2skill_event()
{
tls_lua_state.new_usertype<SkillExecutedEvent>("SkillExecutedEvent",
"skill_id",
sol::property(&SkillExecutedEvent::skill_id, &SkillExecutedEvent::set_skill_id),
"skill_table_id",
sol::property(&SkillExecutedEvent::skill_table_id, &SkillExecutedEvent::set_skill_table_id),
"caster",
sol::property(&SkillExecutedEvent::caster, &SkillExecutedEvent::set_caster),
"target",
sol::property(&SkillExecutedEvent::target, &SkillExecutedEvent::set_target),
"DebugString",
&SkillExecutedEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
