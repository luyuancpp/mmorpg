#include "logic/component/buff_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2buff_comp()
{
tls_lua_state.new_usertype<BuffContextPBComponent>("BuffContextPBComponent",
"damage_value",
sol::property(&BuffContextPBComponent::damage_value, &BuffContextPBComponent::set_damage_value),
"DebugString",
&BuffContextPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BuffPbComponent>("BuffPbComponent",
"buff_id",
sol::property(&BuffPbComponent::buff_id, &BuffPbComponent::set_buff_id),
"buff_table_id",
sol::property(&BuffPbComponent::buff_table_id, &BuffPbComponent::set_buff_table_id),
"ability_id",
sol::property(&BuffPbComponent::ability_id, &BuffPbComponent::set_ability_id),
"parent_entity",
sol::property(&BuffPbComponent::parent_entity, &BuffPbComponent::set_parent_entity),
"layer",
sol::property(&BuffPbComponent::layer, &BuffPbComponent::set_layer),
"caster",
sol::property(&BuffPbComponent::caster, &BuffPbComponent::set_caster),
"triggerdamage",
sol::property(&BuffPbComponent::triggerdamage, &BuffPbComponent::set_triggerdamage),
"processed_caster",
sol::property(&BuffPbComponent::processed_caster, &BuffPbComponent::set_processed_caster),
"DebugString",
&BuffPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<DamageEventPbComponent>("DamageEventPbComponent",
"skill_id",
sol::property(&DamageEventPbComponent::skill_id, &DamageEventPbComponent::set_skill_id),
"attacker_id",
sol::property(&DamageEventPbComponent::attacker_id, &DamageEventPbComponent::set_attacker_id),
"target",
sol::property(&DamageEventPbComponent::target, &DamageEventPbComponent::set_target),
"damage",
sol::property(&DamageEventPbComponent::damage, &DamageEventPbComponent::set_damage),
"damage_type",
sol::property(&DamageEventPbComponent::damage_type, &DamageEventPbComponent::set_damage_type),
"DebugString",
&DamageEventPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PeriodicBuffPbComponent>("PeriodicBuffPbComponent",
"periodic_timer",
sol::property(&PeriodicBuffPbComponent::periodic_timer, &PeriodicBuffPbComponent::set_periodic_timer),
"ticks_done",
sol::property(&PeriodicBuffPbComponent::ticks_done, &PeriodicBuffPbComponent::set_ticks_done),
"DebugString",
&PeriodicBuffPbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
