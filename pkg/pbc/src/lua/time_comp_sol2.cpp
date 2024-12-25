#include "logic/component/time_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2time_comp()
{
tls_lua_state.new_usertype<TimeMeterComp>("TimeMeterComp",
"start",
sol::property(&TimeMeterComp::start, &TimeMeterComp::set_start),
"duration",
sol::property(&TimeMeterComp::duration, &TimeMeterComp::set_duration),
"DebugString",
&TimeMeterComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CooldownTimeComp>("CooldownTimeComp",
"start",
sol::property(&CooldownTimeComp::start, &CooldownTimeComp::set_start),
"cooldown_table_id",
sol::property(&CooldownTimeComp::cooldown_table_id, &CooldownTimeComp::set_cooldown_table_id),
"DebugString",
&CooldownTimeComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CooldownTimeListComp>("CooldownTimeListComp",
"count_cooldown_list",
[](CooldownTimeListComp& pb, uint32_t key) ->decltype(auto){ return pb.cooldown_list().count(key);},
"insert_cooldown_list",
[](CooldownTimeListComp& pb, uint32_t key, CooldownTimeComp& value) ->decltype(auto){ return pb.mutable_cooldown_list()->emplace(key, value).second;},
"cooldown_list",
[](CooldownTimeListComp& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_cooldown_list()->find(key);
 if (it == pb.mutable_cooldown_list()->end()){ static CooldownTimeComp instance; return instance; }
 return it->second;},
"cooldown_list_size",
&CooldownTimeListComp::cooldown_list_size,
"clear_cooldown_list",
&CooldownTimeListComp::clear_cooldown_list,
"DebugString",
&CooldownTimeListComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
