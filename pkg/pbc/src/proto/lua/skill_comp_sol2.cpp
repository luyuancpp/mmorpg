#include "logic/component/skill_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2skill_comp()
{
tls_lua_state.new_usertype<SkillPBComp>("SkillPBComp",
"skill_table_id",
sol::property(&SkillPBComp::skill_table_id, &SkillPBComp::set_skill_table_id),
"DebugString",
&SkillPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SkillContextPBComp>("SkillContextPBComp",
"caster",
sol::property(&SkillContextPBComp::caster, &SkillContextPBComp::set_caster),
"target",
sol::property(&SkillContextPBComp::target, &SkillContextPBComp::set_target),
"SkillId",
sol::property(&SkillContextPBComp::SkillId, &SkillContextPBComp::set_SkillId),
"skillTableId",
sol::property(&SkillContextPBComp::skillTableId, &SkillContextPBComp::set_skillTableId),
"castPosition",
[](SkillContextPBComp& pb) ->decltype(auto){ return pb.castPosition();},
"mutable_castPosition",
[](SkillContextPBComp& pb) ->decltype(auto){ return pb.mutable_castPosition();},
"castTime",
sol::property(&SkillContextPBComp::castTime, &SkillContextPBComp::set_castTime),
"state",
sol::property(&SkillContextPBComp::state, &SkillContextPBComp::set_state<const std::string&>),
"count_additionalData",
[](SkillContextPBComp& pb, string& key) ->decltype(auto){ return pb.additionalData().count(key);},
"insert_additionalData",
[](SkillContextPBComp& pb, string& key, uint32_t value) ->decltype(auto){ return pb.mutable_additionalData()->emplace(key, value).second;},
"additionalData",
[](SkillContextPBComp& pb, string& key) ->decltype(auto){
 auto it =  pb.mutable_additionalData()->find(key);
 if (it == pb.mutable_additionalData()->end()){ return uint32_t(); }
 return it->second;},
"additionalData_size",
&SkillContextPBComp::additionalData_size,
"clear_additionalData",
&SkillContextPBComp::clear_additionalData,
"DebugString",
&SkillContextPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
