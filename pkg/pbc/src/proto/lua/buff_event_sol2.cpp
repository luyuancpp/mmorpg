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
"DebugString",
&BeforeGiveDamageEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AfterGiveDamageEvent>("AfterGiveDamageEvent",
"DebugString",
&AfterGiveDamageEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BeforeTakeDamageEvent>("BeforeTakeDamageEvent",
"DebugString",
&BeforeTakeDamageEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AfterTakeDamageEvent>("AfterTakeDamageEvent",
"DebugString",
&AfterTakeDamageEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BeforeDeadEvent>("BeforeDeadEvent",
"skill_id",
sol::property(&BeforeDeadEvent::skill_id, &BeforeDeadEvent::set_skill_id),
"attacker_id",
sol::property(&BeforeDeadEvent::attacker_id, &BeforeDeadEvent::set_attacker_id),
"receiver_id",
sol::property(&BeforeDeadEvent::receiver_id, &BeforeDeadEvent::set_receiver_id),
"DebugString",
&BeforeDeadEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AfterDeadEvent>("AfterDeadEvent",
"skill_id",
sol::property(&AfterDeadEvent::skill_id, &AfterDeadEvent::set_skill_id),
"attacker_id",
sol::property(&AfterDeadEvent::attacker_id, &AfterDeadEvent::set_attacker_id),
"receiver_id",
sol::property(&AfterDeadEvent::receiver_id, &AfterDeadEvent::set_receiver_id),
"DebugString",
&AfterDeadEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<KillEvent>("KillEvent",
"skill_id",
sol::property(&KillEvent::skill_id, &KillEvent::set_skill_id),
"attacker_id",
sol::property(&KillEvent::attacker_id, &KillEvent::set_attacker_id),
"receiver_id",
sol::property(&KillEvent::receiver_id, &KillEvent::set_receiver_id),
"DebugString",
&KillEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
