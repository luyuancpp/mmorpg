#include "logic/component/player_skill_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_skill_comp()
{
tls_lua_state.new_usertype<PlayerSkillPBComp>("PlayerSkillPBComp",
"id",
sol::property(&PlayerSkillPBComp::id, &PlayerSkillPBComp::set_id),
"skill_table_id",
sol::property(&PlayerSkillPBComp::skill_table_id, &PlayerSkillPBComp::set_skill_table_id),
"DebugString",
&PlayerSkillPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerSkillListPBComp>("PlayerSkillListPBComp",
"add_skill_list",
&PlayerSkillListPBComp::add_skill_list,
"skill_list",
[](const PlayerSkillListPBComp& pb, int index) ->decltype(auto){ return pb.skill_list(index);},
"mutable_skill_list",
[](PlayerSkillListPBComp& pb, int index) ->decltype(auto){ return pb.mutable_skill_list(index);},
"skill_list_size",
&PlayerSkillListPBComp::skill_list_size,
"clear_skill_list",
&PlayerSkillListPBComp::clear_skill_list,
"DebugString",
&PlayerSkillListPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
