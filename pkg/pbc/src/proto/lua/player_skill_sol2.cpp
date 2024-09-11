#include "logic/client_player/player_skill.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_skill()
{
tls_lua_state.new_usertype<ReleaseSkillSkillRequest>("ReleaseSkillSkillRequest",
"skill_table_id",
sol::property(&ReleaseSkillSkillRequest::skill_table_id, &ReleaseSkillSkillRequest::set_skill_table_id),
"target_id",
sol::property(&ReleaseSkillSkillRequest::target_id, &ReleaseSkillSkillRequest::set_target_id),
"position",
[](ReleaseSkillSkillRequest& pb) ->decltype(auto){ return pb.position();},
"mutable_position",
[](ReleaseSkillSkillRequest& pb) ->decltype(auto){ return pb.mutable_position();},
"rotation",
[](ReleaseSkillSkillRequest& pb) ->decltype(auto){ return pb.rotation();},
"mutable_rotation",
[](ReleaseSkillSkillRequest& pb) ->decltype(auto){ return pb.mutable_rotation();},
"DebugString",
&ReleaseSkillSkillRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ReleaseSkillSkillResponse>("ReleaseSkillSkillResponse",
tls_lua_state.new_usertype<>("",
"DebugString",
&::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SkillInterruptedS2C>("SkillInterruptedS2C",
"entity",
sol::property(&SkillInterruptedS2C::entity, &SkillInterruptedS2C::set_entity),
"target_entity",
sol::property(&SkillInterruptedS2C::target_entity, &SkillInterruptedS2C::set_target_entity),
"skill_table_id",
sol::property(&SkillInterruptedS2C::skill_table_id, &SkillInterruptedS2C::set_skill_table_id),
"reason_code",
sol::property(&SkillInterruptedS2C::reason_code, &SkillInterruptedS2C::set_reason_code),
"DebugString",
&SkillInterruptedS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SkillUsedS2C>("SkillUsedS2C",
"entity",
sol::property(&SkillUsedS2C::entity, &SkillUsedS2C::set_entity),
"add_target_entity",
&SkillUsedS2C::add_target_entity,
"target_entity",
[](const SkillUsedS2C& pb, int index) ->decltype(auto){ return pb.target_entity(index);},
"set_target_entity",
[](SkillUsedS2C& pb, int index, uint64_t value) ->decltype(auto){ return pb.set_target_entity(index, value);},
"target_entity_size",
&SkillUsedS2C::target_entity_size,
"clear_target_entity",
&SkillUsedS2C::clear_target_entity,
"skill_table_id",
sol::property(&SkillUsedS2C::skill_table_id, &SkillUsedS2C::set_skill_table_id),
"position",
[](SkillUsedS2C& pb) ->decltype(auto){ return pb.position();},
"mutable_position",
[](SkillUsedS2C& pb) ->decltype(auto){ return pb.mutable_position();},
"time_stamp",
sol::property(&SkillUsedS2C::time_stamp, &SkillUsedS2C::set_time_stamp),
"DebugString",
&SkillUsedS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<GetSkillListRequest>("GetSkillListRequest",
"DebugString",
&GetSkillListRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<GetSkillListResponse>("GetSkillListResponse",
tls_lua_state.new_usertype<>("",
"DebugString",
&::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
