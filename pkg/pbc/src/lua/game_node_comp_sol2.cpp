#include "logic/component/game_node_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2game_node_comp()
{
tls_lua_state.new_usertype<GameNodePlayerInfoPBComponent>("GameNodePlayerInfoPBComponent",
"player_size",
sol::property(&GameNodePlayerInfoPBComponent::player_size, &GameNodePlayerInfoPBComponent::set_player_size),
"DebugString",
&GameNodePlayerInfoPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
