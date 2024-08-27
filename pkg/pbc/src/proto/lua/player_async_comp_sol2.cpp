#include "logic/component/player_async_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_async_comp()
{
tls_lua_state.new_usertype<PlayerGameNodeEnteryInfoPBComp>("PlayerGameNodeEnteryInfoPBComp",
"centre_node_id",
sol::property(&PlayerGameNodeEnteryInfoPBComp::centre_node_id, &PlayerGameNodeEnteryInfoPBComp::set_centre_node_id),
"DebugString",
&PlayerGameNodeEnteryInfoPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
