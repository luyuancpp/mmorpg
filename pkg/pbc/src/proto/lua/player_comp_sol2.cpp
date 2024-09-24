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

tls_lua_state.new_usertype<PlayerUint64PBComponent>("PlayerUint64PBComponent",
"registration_timestamp",
sol::property(&PlayerUint64PBComponent::registration_timestamp, &PlayerUint64PBComponent::set_registration_timestamp),
"DebugString",
&PlayerUint64PBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUint32PBComponent>("PlayerUint32PBComponent",
"class",
sol::property(&PlayerUint32PBComponent::class, &PlayerUint32PBComponent::set_class),
"DebugString",
&PlayerUint32PBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
