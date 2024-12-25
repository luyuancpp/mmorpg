#include "logic/client_player/player_state_attribute_sync.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_state_attribute_sync()
{
tls_lua_state.new_usertype<BaseAttributeSyncDataS2C>("BaseAttributeSyncDataS2C",
"entity_id",
sol::property(&BaseAttributeSyncDataS2C::entity_id, &BaseAttributeSyncDataS2C::set_entity_id),
"transform",
[](BaseAttributeSyncDataS2C& pb) ->decltype(auto){ return pb.transform();},
"mutable_transform",
[](BaseAttributeSyncDataS2C& pb) ->decltype(auto){ return pb.mutable_transform();},
"velocity",
[](BaseAttributeSyncDataS2C& pb) ->decltype(auto){ return pb.velocity();},
"mutable_velocity",
[](BaseAttributeSyncDataS2C& pb) ->decltype(auto){ return pb.mutable_velocity();},
"combat_state_flags",
[](BaseAttributeSyncDataS2C& pb) ->decltype(auto){ return pb.combat_state_flags();},
"mutable_combat_state_flags",
[](BaseAttributeSyncDataS2C& pb) ->decltype(auto){ return pb.mutable_combat_state_flags();},
"DebugString",
&BaseAttributeSyncDataS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta2FramesS2C>("AttributeDelta2FramesS2C",
"entity_id",
sol::property(&AttributeDelta2FramesS2C::entity_id, &AttributeDelta2FramesS2C::set_entity_id),
"DebugString",
&AttributeDelta2FramesS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta5FramesS2C>("AttributeDelta5FramesS2C",
"entity_id",
sol::property(&AttributeDelta5FramesS2C::entity_id, &AttributeDelta5FramesS2C::set_entity_id),
"DebugString",
&AttributeDelta5FramesS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta10FramesS2C>("AttributeDelta10FramesS2C",
"entity_id",
sol::property(&AttributeDelta10FramesS2C::entity_id, &AttributeDelta10FramesS2C::set_entity_id),
"DebugString",
&AttributeDelta10FramesS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta30FramesS2C>("AttributeDelta30FramesS2C",
"entity_id",
sol::property(&AttributeDelta30FramesS2C::entity_id, &AttributeDelta30FramesS2C::set_entity_id),
"DebugString",
&AttributeDelta30FramesS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AttributeDelta60FramesS2C>("AttributeDelta60FramesS2C",
"entity_id",
sol::property(&AttributeDelta60FramesS2C::entity_id, &AttributeDelta60FramesS2C::set_entity_id),
"DebugString",
&AttributeDelta60FramesS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
