#include "logic/component/skill_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2skill_comp()
{
tls_lua_state.new_usertype<SkillPBComponent>("SkillPBComponent",
"skill_table_id",
sol::property(&SkillPBComponent::skill_table_id, &SkillPBComponent::set_skill_table_id),
"DebugString",
&SkillPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SkillContextPBComponent>("SkillContextPBComponent",
"caster",
sol::property(&SkillContextPBComponent::caster, &SkillContextPBComponent::set_caster),
"target",
sol::property(&SkillContextPBComponent::target, &SkillContextPBComponent::set_target),
"SkillId",
sol::property(&SkillContextPBComponent::SkillId, &SkillContextPBComponent::set_SkillId),
"skillTableId",
sol::property(&SkillContextPBComponent::skillTableId, &SkillContextPBComponent::set_skillTableId),
"castPosition",
[](SkillContextPBComponent& pb) ->decltype(auto){ return pb.castPosition();},
"mutable_castPosition",
[](SkillContextPBComponent& pb) ->decltype(auto){ return pb.mutable_castPosition();},
"castTime",
sol::property(&SkillContextPBComponent::castTime, &SkillContextPBComponent::set_castTime),
"state",
sol::property(&SkillContextPBComponent::state, &SkillContextPBComponent::set_state<const std::string&>),
"count_additionalData",
[](SkillContextPBComponent& pb, string& key) ->decltype(auto){ return pb.additionalData().count(key);},
"insert_additionalData",
[](SkillContextPBComponent& pb, string& key, uint32_t value) ->decltype(auto){ return pb.mutable_additionalData()->emplace(key, value).second;},
"additionalData",
[](SkillContextPBComponent& pb, string& key) ->decltype(auto){
 auto it =  pb.mutable_additionalData()->find(key);
 if (it == pb.mutable_additionalData()->end()){ return uint32_t(); }
 return it->second;},
"additionalData_size",
&SkillContextPBComponent::additionalData_size,
"clear_additionalData",
&SkillContextPBComponent::clear_additionalData,
"DebugString",
&SkillContextPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
