#include "common/c2gate.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2c2gate()
{
tls_lua_state.new_usertype<AccountSimplePlayerWrapper>("AccountSimplePlayerWrapper",
"player",
[](AccountSimplePlayerWrapper& pb) ->decltype(auto){ return pb.player();},
"mutable_player",
[](AccountSimplePlayerWrapper& pb) ->decltype(auto){ return pb.mutable_player();},
"DebugString",
&AccountSimplePlayerWrapper::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LoginRequest>("LoginRequest",
"account",
sol::property(&LoginRequest::account, &LoginRequest::set_account<const std::string&>),
"password",
sol::property(&LoginRequest::password, &LoginRequest::set_password<const std::string&>),
"DebugString",
&LoginRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LoginResponse>("LoginResponse",
tls_lua_state.new_usertype<>("",
"add_players",
&::add_players,
"players",
[](const & pb, int index) ->decltype(auto){ return pb.players(index);},
"mutable_players",
[](& pb, int index) ->decltype(auto){ return pb.mutable_players(index);},
"players_size",
&::players_size,
"clear_players",
&::clear_players,
"DebugString",
&::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<TestResponse>("TestResponse",
tls_lua_state.new_usertype<>("",
"add_players",
&::add_players,
"players",
[](const & pb, int index) ->decltype(auto){ return pb.players(index);},
"mutable_players",
[](& pb, int index) ->decltype(auto){ return pb.mutable_players(index);},
"players_size",
&::players_size,
"clear_players",
&::clear_players,
"add_teststring",
[](& pb, const std::string& value) ->decltype(auto){ return pb.add_teststring(value);},
"teststring",
[](& pb, int index) ->decltype(auto){ return pb.teststring(index);},
"set_teststring",
[](& pb, int index, const std::string& value) ->decltype(auto){ return pb.set_teststring(index, value);},
"teststring_size",
&::teststring_size,
"clear_teststring",
&::clear_teststring,
"add_testint",
&::add_testint,
"testint",
[](const & pb, int index) ->decltype(auto){ return pb.testint(index);},
"set_testint",
[](& pb, int index, int32_t value) ->decltype(auto){ return pb.set_testint(index, value);},
"testint_size",
&::testint_size,
"clear_testint",
&::clear_testint,
"DebugString",
&::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CreatePlayerRequest>("CreatePlayerRequest",
"DebugString",
&CreatePlayerRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CreatePlayerResponse>("CreatePlayerResponse",
tls_lua_state.new_usertype<error_message>("error_message",
"add_players",
&error_message::add_players,
"players",
[](const error_message& pb, int index) ->decltype(auto){ return pb.players(index);},
"mutable_players",
[](error_message& pb, int index) ->decltype(auto){ return pb.mutable_players(index);},
"players_size",
&error_message::players_size,
"clear_players",
&error_message::clear_players,
"DebugString",
&error_message::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterGameRequest>("EnterGameRequest",
"player_id",
sol::property(&EnterGameRequest::player_id, &EnterGameRequest::set_player_id),
"DebugString",
&EnterGameRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterGameResponse>("EnterGameResponse",
tls_lua_state.new_usertype<error_message>("error_message",
"DebugString",
&error_message::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<LeaveGameRequest>("LeaveGameRequest",
"DebugString",
&LeaveGameRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<ClientRequest>("ClientRequest",
"id",
sol::property(&ClientRequest::id, &ClientRequest::set_id),
"service",
sol::property(&ClientRequest::service, &ClientRequest::set_service<const std::string&>),
"method",
sol::property(&ClientRequest::method, &ClientRequest::set_method<const std::string&>),
"body",
[](ClientRequest& pb) ->decltype(auto){ return pb.body();},
"mutable_body",
[](ClientRequest& pb) ->decltype(auto){ return pb.mutable_body();},
"message_id",
sol::property(&ClientRequest::message_id, &ClientRequest::set_message_id),
"DebugString",
&ClientRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
