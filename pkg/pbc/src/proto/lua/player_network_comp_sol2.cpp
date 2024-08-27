#include "logic/component/player_network_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_network_comp()
{
tls_lua_state.new_usertype<PlayerNodeInfoPBComp>("PlayerNodeInfoPBComp",
"gate_session_id",
sol::property(&PlayerNodeInfoPBComp::gate_session_id, &PlayerNodeInfoPBComp::set_gate_session_id),
"centre_node_id",
sol::property(&PlayerNodeInfoPBComp::centre_node_id, &PlayerNodeInfoPBComp::set_centre_node_id),
"game_node_id",
sol::property(&PlayerNodeInfoPBComp::game_node_id, &PlayerNodeInfoPBComp::set_game_node_id),
"DebugString",
&PlayerNodeInfoPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerSessionPBComp>("PlayerSessionPBComp",
"player_id",
sol::property(&PlayerSessionPBComp::player_id, &PlayerSessionPBComp::set_player_id),
"DebugString",
&PlayerSessionPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
