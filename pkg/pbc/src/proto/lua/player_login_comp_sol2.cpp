#include "logic/component/player_login_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_login_comp()
{
tls_lua_state.new_usertype<EnterGameNodeInfoPBComp>("EnterGameNodeInfoPBComp",
"enter_gs_type",
sol::property(&EnterGameNodeInfoPBComp::enter_gs_type, &EnterGameNodeInfoPBComp::set_enter_gs_type),
"DebugString",
&EnterGameNodeInfoPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
