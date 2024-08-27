#include "logic/event/server_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2server_event()
{
tls_lua_state.new_usertype<OnConnect2Centre>("OnConnect2Centre",
"entity",
sol::property(&OnConnect2Centre::entity, &OnConnect2Centre::set_entity),
"DebugString",
&OnConnect2Centre::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<OnConnect2Game>("OnConnect2Game",
"entity",
sol::property(&OnConnect2Game::entity, &OnConnect2Game::set_entity),
"DebugString",
&OnConnect2Game::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<OnConnect2Gate>("OnConnect2Gate",
"entity",
sol::property(&OnConnect2Gate::entity, &OnConnect2Gate::set_entity),
"DebugString",
&OnConnect2Gate::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<OnConnect2Login>("OnConnect2Login",
"entity",
sol::property(&OnConnect2Login::entity, &OnConnect2Login::set_entity),
"DebugString",
&OnConnect2Login::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<OnServerStart>("OnServerStart",
"DebugString",
&OnServerStart::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
