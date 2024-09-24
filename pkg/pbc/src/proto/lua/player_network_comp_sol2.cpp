#include "logic/component/player_network_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_network_comp()
{
tls_lua_state.new_usertype<PlayerNodeInfoPBComponent>("PlayerNodeInfoPBComponent",
"gate_session_id",
sol::property(&PlayerNodeInfoPBComponent::gate_session_id, &PlayerNodeInfoPBComponent::set_gate_session_id),
"centre_node_id",
sol::property(&PlayerNodeInfoPBComponent::centre_node_id, &PlayerNodeInfoPBComponent::set_centre_node_id),
"game_node_id",
sol::property(&PlayerNodeInfoPBComponent::game_node_id, &PlayerNodeInfoPBComponent::set_game_node_id),
"DebugString",
&PlayerNodeInfoPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerSessionPBComponent>("PlayerSessionPBComponent",
"player_id",
sol::property(&PlayerSessionPBComponent::player_id, &PlayerSessionPBComponent::set_player_id),
"DebugString",
&PlayerSessionPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
