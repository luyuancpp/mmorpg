#include "logic/event/player_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_event()
{
tls_lua_state.new_usertype<RegisterPlayerEvent>("RegisterPlayerEvent",
"entity",
sol::property(&RegisterPlayerEvent::entity, &RegisterPlayerEvent::set_entity),
"DebugString",
&RegisterPlayerEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUpgradeEvent>("PlayerUpgradeEvent",
"entity",
sol::property(&PlayerUpgradeEvent::entity, &PlayerUpgradeEvent::set_entity),
"new_level",
sol::property(&PlayerUpgradeEvent::new_level, &PlayerUpgradeEvent::set_new_level),
"DebugString",
&PlayerUpgradeEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<InitializePlayerComponentsEvent>("InitializePlayerComponentsEvent",
"entity",
sol::property(&InitializePlayerComponentsEvent::entity, &InitializePlayerComponentsEvent::set_entity),
"DebugString",
&InitializePlayerComponentsEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
