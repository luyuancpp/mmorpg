#include "logic/client_player/player_state_attribute_sync.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_state_attribute_sync()
{
tls_lua_state.new_usertype<BaseAttributeDeltaS2C>("BaseAttributeDeltaS2C",
"entity_id",
sol::property(&BaseAttributeDeltaS2C::entity_id, &BaseAttributeDeltaS2C::set_entity_id),
"velocity",
[](BaseAttributeDeltaS2C& pb) ->decltype(auto){ return pb.velocity();},
"mutable_velocity",
[](BaseAttributeDeltaS2C& pb) ->decltype(auto){ return pb.mutable_velocity();},
"DebugString",
&BaseAttributeDeltaS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta>("AttributeDelta",
"entity_id",
sol::property(&AttributeDelta::entity_id, &AttributeDelta::set_entity_id),
"delta_2_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.delta_2_frames();},
"mutable_delta_2_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.mutable_delta_2_frames();},
"delta_5_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.delta_5_frames();},
"mutable_delta_5_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.mutable_delta_5_frames();},
"delta_10_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.delta_10_frames();},
"mutable_delta_10_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.mutable_delta_10_frames();},
"delta_30_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.delta_30_frames();},
"mutable_delta_30_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.mutable_delta_30_frames();},
"delta_60_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.delta_60_frames();},
"mutable_delta_60_frames",
[](AttributeDelta& pb) ->decltype(auto){ return pb.mutable_delta_60_frames();},
"DebugString",
&AttributeDelta::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta2Frames>("AttributeDelta2Frames",
"entity_id",
sol::property(&AttributeDelta2Frames::entity_id, &AttributeDelta2Frames::set_entity_id),
"DebugString",
&AttributeDelta2Frames::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta5Frames>("AttributeDelta5Frames",
"entity_id",
sol::property(&AttributeDelta5Frames::entity_id, &AttributeDelta5Frames::set_entity_id),
"DebugString",
&AttributeDelta5Frames::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta10Frames>("AttributeDelta10Frames",
"entity_id",
sol::property(&AttributeDelta10Frames::entity_id, &AttributeDelta10Frames::set_entity_id),
"DebugString",
&AttributeDelta10Frames::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta30Frames>("AttributeDelta30Frames",
"entity_id",
sol::property(&AttributeDelta30Frames::entity_id, &AttributeDelta30Frames::set_entity_id),
"DebugString",
&AttributeDelta30Frames::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta60Frames>("AttributeDelta60Frames",
"entity_id",
sol::property(&AttributeDelta60Frames::entity_id, &AttributeDelta60Frames::set_entity_id),
"DebugString",
&AttributeDelta60Frames::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
