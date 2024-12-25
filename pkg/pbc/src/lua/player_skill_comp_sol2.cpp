#include "logic/component/player_skill_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_skill_comp()
{
tls_lua_state.new_usertype<PlayerSkillPBComponent>("PlayerSkillPBComponent",
"id",
sol::property(&PlayerSkillPBComponent::id, &PlayerSkillPBComponent::set_id),
"skill_table_id",
sol::property(&PlayerSkillPBComponent::skill_table_id, &PlayerSkillPBComponent::set_skill_table_id),
"DebugString",
&PlayerSkillPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerSkillListPBComponent>("PlayerSkillListPBComponent",
"add_skill_list",
&PlayerSkillListPBComponent::add_skill_list,
"skill_list",
[](const PlayerSkillListPBComponent& pb, int index) ->decltype(auto){ return pb.skill_list(index);},
"mutable_skill_list",
[](PlayerSkillListPBComponent& pb, int index) ->decltype(auto){ return pb.mutable_skill_list(index);},
"skill_list_size",
&PlayerSkillListPBComponent::skill_list_size,
"clear_skill_list",
&PlayerSkillListPBComponent::clear_skill_list,
"DebugString",
&PlayerSkillListPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
