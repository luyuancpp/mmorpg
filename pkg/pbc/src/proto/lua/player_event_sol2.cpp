#include "logic/event/player_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_event()
{
tls_lua_state.new_usertype<RegisterPlayer>("RegisterPlayer",
"entity",
sol::property(&RegisterPlayer::entity, &RegisterPlayer::set_entity),
"DebugString",
&RegisterPlayer::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUpgrade>("PlayerUpgrade",
"entity",
sol::property(&PlayerUpgrade::entity, &PlayerUpgrade::set_entity),
"new_level",
sol::property(&PlayerUpgrade::new_level, &PlayerUpgrade::set_new_level),
"DebugString",
&PlayerUpgrade::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
