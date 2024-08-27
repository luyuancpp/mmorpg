#include "logic/event/mission_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2mission_event()
{
tls_lua_state.new_usertype<AcceptMissionEvent>("AcceptMissionEvent",
"entity",
sol::property(&AcceptMissionEvent::entity, &AcceptMissionEvent::set_entity),
"mission_id",
sol::property(&AcceptMissionEvent::mission_id, &AcceptMissionEvent::set_mission_id),
"DebugString",
&AcceptMissionEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<MissionConditionEvent>("MissionConditionEvent",
"entity",
sol::property(&MissionConditionEvent::entity, &MissionConditionEvent::set_entity),
"condition_type",
sol::property(&MissionConditionEvent::condition_type, &MissionConditionEvent::set_condition_type),
"add_condtion_ids",
&MissionConditionEvent::add_condtion_ids,
"condtion_ids",
[](const MissionConditionEvent& pb, int index) ->decltype(auto){ return pb.condtion_ids(index);},
"set_condtion_ids",
[](MissionConditionEvent& pb, int index, uint32_t value) ->decltype(auto){ return pb.set_condtion_ids(index, value);},
"condtion_ids_size",
&MissionConditionEvent::condtion_ids_size,
"clear_condtion_ids",
&MissionConditionEvent::clear_condtion_ids,
"amount",
sol::property(&MissionConditionEvent::amount, &MissionConditionEvent::set_amount),
"DebugString",
&MissionConditionEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<OnAcceptedMissionEvent>("OnAcceptedMissionEvent",
"entity",
sol::property(&OnAcceptedMissionEvent::entity, &OnAcceptedMissionEvent::set_entity),
"mission_id",
sol::property(&OnAcceptedMissionEvent::mission_id, &OnAcceptedMissionEvent::set_mission_id),
"DebugString",
&OnAcceptedMissionEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<OnMissionAwardEvent>("OnMissionAwardEvent",
"entity",
sol::property(&OnMissionAwardEvent::entity, &OnMissionAwardEvent::set_entity),
"mission_id",
sol::property(&OnMissionAwardEvent::mission_id, &OnMissionAwardEvent::set_mission_id),
"DebugString",
&OnMissionAwardEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
