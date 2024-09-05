#include "logic/server_player/game_player.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2game_player()
{
tls_lua_state.new_usertype<Centre2GsLoginRequest>("Centre2GsLoginRequest",
"enter_gs_type",
sol::property(&Centre2GsLoginRequest::enter_gs_type, &Centre2GsLoginRequest::set_enter_gs_type),
"DebugString",
&Centre2GsLoginRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<GameNodeExitGameRequest>("GameNodeExitGameRequest",
"DebugString",
&GameNodeExitGameRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
