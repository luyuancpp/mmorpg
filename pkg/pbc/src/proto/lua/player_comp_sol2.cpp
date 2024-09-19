#include "logic/component/player_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_comp()
{
tls_lua_state.new_usertype<NormalLogin>("NormalLogin",
"DebugString",
&NormalLogin::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CoverLogin>("CoverLogin",
"DebugString",
&CoverLogin::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Player>("Player",
"DebugString",
&Player::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<Account>("Account",
"account",
sol::property(&Account::account, &Account::set_account<const std::string&>),
"DebugString",
&Account::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<UnregisterPlayer>("UnregisterPlayer",
"DebugString",
&UnregisterPlayer::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUint64PBComp>("PlayerUint64PBComp",
"registration_timestamp",
sol::property(&PlayerUint64PBComp::registration_timestamp, &PlayerUint64PBComp::set_registration_timestamp),
"DebugString",
&PlayerUint64PBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUint32PBComp>("PlayerUint32PBComp",
"class",
sol::property(&PlayerUint32PBComp::class, &PlayerUint32PBComp::set_class),
"DebugString",
&PlayerUint32PBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
