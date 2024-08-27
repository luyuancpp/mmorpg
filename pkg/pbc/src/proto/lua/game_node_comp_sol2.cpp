#include "logic/component/game_node_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2game_node_comp()
{
tls_lua_state.new_usertype<GameNodePlayerInfoPBComp>("GameNodePlayerInfoPBComp",
"player_size",
sol::property(&GameNodePlayerInfoPBComp::player_size, &GameNodePlayerInfoPBComp::set_player_size),
"DebugString",
&GameNodePlayerInfoPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
