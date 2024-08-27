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

tls_lua_state.new_usertype<PlayerInt32Array>("PlayerInt32Array",
"add_int32_valuse",
&PlayerInt32Array::add_int32_valuse,
"int32_valuse",
[](const PlayerInt32Array& pb, int index) ->decltype(auto){ return pb.int32_valuse(index);},
"set_int32_valuse",
[](PlayerInt32Array& pb, int index, int32_t value) ->decltype(auto){ return pb.set_int32_valuse(index, value);},
"int32_valuse_size",
&PlayerInt32Array::int32_valuse_size,
"clear_int32_valuse",
&PlayerInt32Array::clear_int32_valuse,
"DebugString",
&PlayerInt32Array::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUInt32Array>("PlayerUInt32Array",
"add_uint32_valuse",
&PlayerUInt32Array::add_uint32_valuse,
"uint32_valuse",
[](const PlayerUInt32Array& pb, int index) ->decltype(auto){ return pb.uint32_valuse(index);},
"set_uint32_valuse",
[](PlayerUInt32Array& pb, int index, uint32_t value) ->decltype(auto){ return pb.set_uint32_valuse(index, value);},
"uint32_valuse_size",
&PlayerUInt32Array::uint32_valuse_size,
"clear_uint32_valuse",
&PlayerUInt32Array::clear_uint32_valuse,
"DebugString",
&PlayerUInt32Array::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerInt64Array>("PlayerInt64Array",
"DebugString",
&PlayerInt64Array::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUInt64Array>("PlayerUInt64Array",
"DebugString",
&PlayerUInt64Array::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
