#include "logic/client_player/player_state_attribute_sync.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_state_attribute_sync()
{
tls_lua_state.new_usertype<SyncBaseStateAttributeDeltaS2C>("SyncBaseStateAttributeDeltaS2C",
"entity_id",
sol::property(&SyncBaseStateAttributeDeltaS2C::entity_id, &SyncBaseStateAttributeDeltaS2C::set_entity_id),
"velocity",
[](SyncBaseStateAttributeDeltaS2C& pb) ->decltype(auto){ return pb.velocity();},
"mutable_velocity",
[](SyncBaseStateAttributeDeltaS2C& pb) ->decltype(auto){ return pb.mutable_velocity();},
"DebugString",
&SyncBaseStateAttributeDeltaS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
