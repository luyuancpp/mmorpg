#include "logic/component/buff_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2buff_comp()
{
tls_lua_state.new_usertype<BuffContextPBComp>("BuffContextPBComp",
"damage_value",
sol::property(&BuffContextPBComp::damage_value, &BuffContextPBComp::set_damage_value),
"DebugString",
&BuffContextPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BuffPBComp>("BuffPBComp",
"buff_id",
sol::property(&BuffPBComp::buff_id, &BuffPBComp::set_buff_id),
"buff_table_id",
sol::property(&BuffPBComp::buff_table_id, &BuffPBComp::set_buff_table_id),
"ability_id",
sol::property(&BuffPBComp::ability_id, &BuffPBComp::set_ability_id),
"parent_entity",
sol::property(&BuffPBComp::parent_entity, &BuffPBComp::set_parent_entity),
"layer",
sol::property(&BuffPBComp::layer, &BuffPBComp::set_layer),
"caster",
sol::property(&BuffPBComp::caster, &BuffPBComp::set_caster),
"triggerdamage",
sol::property(&BuffPBComp::triggerdamage, &BuffPBComp::set_triggerdamage),
"DebugString",
&BuffPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
