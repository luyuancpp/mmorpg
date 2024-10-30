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

tls_lua_state.new_usertype<PlayerUint32PbComponent>("PlayerUint32PbComponent",
"class",
sol::property(&PlayerUint32PbComponent::class, &PlayerUint32PbComponent::set_class),
"DebugString",
&PlayerUint32PbComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
