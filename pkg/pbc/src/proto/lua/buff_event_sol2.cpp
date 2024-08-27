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

tls_lua_state.new_usertype<BeforeGiveDamageEvent>("BeforeGiveDamageEvent",
"skill_idd",
sol::property(&BeforeGiveDamageEvent::skill_idd, &BeforeGiveDamageEvent::set_skill_idd),
"attacker_id",
sol::property(&BeforeGiveDamageEvent::attacker_id, &BeforeGiveDamageEvent::set_attacker_id),
"receiver_id",
sol::property(&BeforeGiveDamageEvent::receiver_id, &BeforeGiveDamageEvent::set_receiver_id),
"damage_amount",
sol::property(&BeforeGiveDamageEvent::damage_amount, &BeforeGiveDamageEvent::set_damage_amount),
"damage_flags",
sol::property(&BeforeGiveDamageEvent::damage_flags, &BeforeGiveDamageEvent::set_damage_flags),
"DebugString",
&BeforeGiveDamageEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
