#include "logic/component/mission_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2mission_comp()
{
tls_lua_state.new_usertype<MissionPBComponent>("MissionPBComponent",
"id",
sol::property(&MissionPBComponent::id, &MissionPBComponent::set_id),
"status",
sol::property(&MissionPBComponent::status, &MissionPBComponent::set_status),
"add_progress",
&MissionPBComponent::add_progress,
"progress",
[](const MissionPBComponent& pb, int index) ->decltype(auto){ return pb.progress(index);},
"set_progress",
[](MissionPBComponent& pb, int index, uint32_t value) ->decltype(auto){ return pb.set_progress(index, value);},
"progress_size",
&MissionPBComponent::progress_size,
"clear_progress",
&MissionPBComponent::clear_progress,
"DebugString",
&MissionPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<MissionListPBComponent>("MissionListPBComponent",
"count_missions",
[](MissionListPBComponent& pb, uint32_t key) ->decltype(auto){ return pb.missions().count(key);},
"insert_missions",
[](MissionListPBComponent& pb, uint32_t key, MissionPBComponent& value) ->decltype(auto){ return pb.mutable_missions()->emplace(key, value).second;},
"missions",
[](MissionListPBComponent& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_missions()->find(key);
 if (it == pb.mutable_missions()->end()){ static MissionPBComponent instance; return instance; }
 return it->second;},
"missions_size",
&MissionListPBComponent::missions_size,
"clear_missions",
&MissionListPBComponent::clear_missions,
"count_complete_missions",
[](MissionListPBComponent& pb, uint32_t key) ->decltype(auto){ return pb.complete_missions().count(key);},
"insert_complete_missions",
[](MissionListPBComponent& pb, uint32_t key, bool value) ->decltype(auto){ return pb.mutable_complete_missions()->emplace(key, value).second;},
"complete_missions",
[](MissionListPBComponent& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_complete_missions()->find(key);
 if (it == pb.mutable_complete_missions()->end()){ return bool(); }
 return it->second;},
"complete_missions_size",
&MissionListPBComponent::complete_missions_size,
"clear_complete_missions",
&MissionListPBComponent::clear_complete_missions,
"count_mission_begin_time",
[](MissionListPBComponent& pb, uint32_t key) ->decltype(auto){ return pb.mission_begin_time().count(key);},
"insert_mission_begin_time",
[](MissionListPBComponent& pb, uint32_t key, uint64_t value) ->decltype(auto){ return pb.mutable_mission_begin_time()->emplace(key, value).second;},
"mission_begin_time",
[](MissionListPBComponent& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_mission_begin_time()->find(key);
 if (it == pb.mutable_mission_begin_time()->end()){ return uint64_t(); }
 return it->second;},
"mission_begin_time_size",
&MissionListPBComponent::mission_begin_time_size,
"clear_mission_begin_time",
&MissionListPBComponent::clear_mission_begin_time,
"type",
sol::property(&MissionListPBComponent::type, &MissionListPBComponent::set_type),
"DebugString",
&MissionListPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RewardListPBComponent>("RewardListPBComponent",
"count_can_reward_mission_id",
[](RewardListPBComponent& pb, uint32_t key) ->decltype(auto){ return pb.can_reward_mission_id().count(key);},
"insert_can_reward_mission_id",
[](RewardListPBComponent& pb, uint32_t key, bool value) ->decltype(auto){ return pb.mutable_can_reward_mission_id()->emplace(key, value).second;},
"can_reward_mission_id",
[](RewardListPBComponent& pb, uint32_t key) ->decltype(auto){
 auto it =  pb.mutable_can_reward_mission_id()->find(key);
 if (it == pb.mutable_can_reward_mission_id()->end()){ return bool(); }
 return it->second;},
"can_reward_mission_id_size",
&RewardListPBComponent::can_reward_mission_id_size,
"clear_can_reward_mission_id",
&RewardListPBComponent::clear_can_reward_mission_id,
"DebugString",
&RewardListPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
