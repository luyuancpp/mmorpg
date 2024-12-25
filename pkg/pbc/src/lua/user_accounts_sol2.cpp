#include "common/user_accounts.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2user_accounts()
{
tls_lua_state.new_usertype<AccountSimplePlayer>("AccountSimplePlayer",
"player_id",
sol::property(&AccountSimplePlayer::player_id, &AccountSimplePlayer::set_player_id),
"DebugString",
&AccountSimplePlayer::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<AccountSimplePlayerList>("AccountSimplePlayerList",
"add_players",
&AccountSimplePlayerList::add_players,
"players",
[](const AccountSimplePlayerList& pb, int index) ->decltype(auto){ return pb.players(index);},
"mutable_players",
[](AccountSimplePlayerList& pb, int index) ->decltype(auto){ return pb.mutable_players(index);},
"players_size",
&AccountSimplePlayerList::players_size,
"clear_players",
&AccountSimplePlayerList::clear_players,
"DebugString",
&AccountSimplePlayerList::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
