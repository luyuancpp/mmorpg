#include "logic/event/player_event.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_event()
{
tls_lua_state.new_usertype<RegisterPlayerEvent>("RegisterPlayerEvent",
"actor_entity",
sol::property(&RegisterPlayerEvent::actor_entity, &RegisterPlayerEvent::set_actor_entity),
"DebugString",
&RegisterPlayerEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerUpgradeEvent>("PlayerUpgradeEvent",
"actor_entity",
sol::property(&PlayerUpgradeEvent::actor_entity, &PlayerUpgradeEvent::set_actor_entity),
"new_level",
sol::property(&PlayerUpgradeEvent::new_level, &PlayerUpgradeEvent::set_new_level),
"DebugString",
&PlayerUpgradeEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<InitializePlayerComponentsEvent>("InitializePlayerComponentsEvent",
"actor_entity",
sol::property(&InitializePlayerComponentsEvent::actor_entity, &InitializePlayerComponentsEvent::set_actor_entity),
"DebugString",
&InitializePlayerComponentsEvent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
