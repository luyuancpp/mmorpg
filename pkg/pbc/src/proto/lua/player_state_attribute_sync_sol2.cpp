#include "logic/client_player/player_state_attribute_sync.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_state_attribute_sync()
{
tls_lua_state.new_usertype<SyncEntityVelocityS2C>("SyncEntityVelocityS2C",
"entity_id",
sol::property(&SyncEntityVelocityS2C::entity_id, &SyncEntityVelocityS2C::set_entity_id),
"velocity",
[](SyncEntityVelocityS2C& pb) ->decltype(auto){ return pb.velocity();},
"mutable_velocity",
[](SyncEntityVelocityS2C& pb) ->decltype(auto){ return pb.mutable_velocity();},
"DebugString",
&SyncEntityVelocityS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
