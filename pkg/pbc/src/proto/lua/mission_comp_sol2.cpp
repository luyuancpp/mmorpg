#include "logic/component/mission_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2mission_comp()
{
tls_lua_state.new_usertype<MissionPBComp>("MissionPBComp",
"id",
sol::property(&MissionPBComp::id, &MissionPBComp::set_id),
"status",
sol::property(&MissionPBComp::status, &MissionPBComp::set_status),
"add_progress",
&MissionPBComp::add_progress,
"progress",
[](const MissionPBComp& pb, int index) ->decltype(auto){ return pb.progress(index);},
"set_progress",
[](MissionPBComp& pb, int index, uint32_t value) ->decltype(auto){ return pb.set_progress(index, value);},
"progress_size",
&MissionPBComp::progress_size,
"clear_progress",
&MissionPBComp::clear_progress,
"DebugString",
&MissionPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<MissionListPBComp>("MissionListPBComp",
"count_missions",
[](MissionListPBComp& pb, uint32_t key) ->decltype(auto){ return pb.missions().count(key);},
"insert_missions",
[](MissionListPBComp& pb, uint32_t key, MissionPBComp& value) ->decltype(auto){ return pb.mutable_missions()->emplace(key, value).second;},
"missions",
[](MissionListPBComp& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_missions()->find(key);
 if (it == pb.mutable_missions()->end()){ static MissionPBComp instance; return instance; }
 return it->second;},
"missions_size",
&MissionListPBComp::missions_size,
"clear_missions",
&MissionListPBComp::clear_missions,
"count_complete_missions",
[](MissionListPBComp& pb, uint32_t key) ->decltype(auto){ return pb.complete_missions().count(key);},
"insert_complete_missions",
[](MissionListPBComp& pb, uint32_t key, bool value) ->decltype(auto){ return pb.mutable_complete_missions()->emplace(key, value).second;},
"complete_missions",
[](MissionListPBComp& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_complete_missions()->find(key);
 if (it == pb.mutable_complete_missions()->end()){ return bool(); }
 return it->second;},
"complete_missions_size",
&MissionListPBComp::complete_missions_size,
"clear_complete_missions",
&MissionListPBComp::clear_complete_missions,
"count_mission_begin_time",
[](MissionListPBComp& pb, uint32_t key) ->decltype(auto){ return pb.mission_begin_time().count(key);},
"insert_mission_begin_time",
[](MissionListPBComp& pb, uint32_t key, uint64_t value) ->decltype(auto){ return pb.mutable_mission_begin_time()->emplace(key, value).second;},
"mission_begin_time",
[](MissionListPBComp& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_mission_begin_time()->find(key);
 if (it == pb.mutable_mission_begin_time()->end()){ return uint64_t(); }
 return it->second;},
"mission_begin_time_size",
&MissionListPBComp::mission_begin_time_size,
"clear_mission_begin_time",
&MissionListPBComp::clear_mission_begin_time,
"type",
sol::property(&MissionListPBComp::type, &MissionListPBComp::set_type),
"DebugString",
&MissionListPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RewardListPBComp>("RewardListPBComp",
"count_can_reward_mission_id",
[](RewardListPBComp& pb, uint32_t key) ->decltype(auto){ return pb.can_reward_mission_id().count(key);},
"insert_can_reward_mission_id",
[](RewardListPBComp& pb, uint32_t key, bool value) ->decltype(auto){ return pb.mutable_can_reward_mission_id()->emplace(key, value).second;},
"can_reward_mission_id",
[](RewardListPBComp& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_can_reward_mission_id()->find(key);
 if (it == pb.mutable_can_reward_mission_id()->end()){ return bool(); }
 return it->second;},
"can_reward_mission_id_size",
&RewardListPBComp::can_reward_mission_id_size,
"clear_can_reward_mission_id",
&RewardListPBComp::clear_can_reward_mission_id,
"DebugString",
&RewardListPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
