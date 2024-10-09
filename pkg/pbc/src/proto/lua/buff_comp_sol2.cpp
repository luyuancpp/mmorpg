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

tls_lua_state.new_usertype<BuffPBComponent>("BuffPBComponent",
"buff_id",
sol::property(&BuffPBComponent::buff_id, &BuffPBComponent::set_buff_id),
"buff_table_id",
sol::property(&BuffPBComponent::buff_table_id, &BuffPBComponent::set_buff_table_id),
"ability_id",
sol::property(&BuffPBComponent::ability_id, &BuffPBComponent::set_ability_id),
"parent_entity",
sol::property(&BuffPBComponent::parent_entity, &BuffPBComponent::set_parent_entity),
"layer",
sol::property(&BuffPBComponent::layer, &BuffPBComponent::set_layer),
"caster",
sol::property(&BuffPBComponent::caster, &BuffPBComponent::set_caster),
"triggerdamage",
sol::property(&BuffPBComponent::triggerdamage, &BuffPBComponent::set_triggerdamage),
"DebugString",
&BuffPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<DamageEventComponent>("DamageEventComponent",
"skill_id",
sol::property(&DamageEventComponent::skill_id, &DamageEventComponent::set_skill_id),
"attacker_id",
sol::property(&DamageEventComponent::attacker_id, &DamageEventComponent::set_attacker_id),
"target",
sol::property(&DamageEventComponent::target, &DamageEventComponent::set_target),
"damage",
sol::property(&DamageEventComponent::damage, &DamageEventComponent::set_damage),
"damage_type",
sol::property(&DamageEventComponent::damage_type, &DamageEventComponent::set_damage_type),
"DebugString",
&DamageEventComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
