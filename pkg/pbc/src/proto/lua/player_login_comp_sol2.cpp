#include "logic/component/player_login_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_login_comp()
{
tls_lua_state.new_usertype<EnterGameNodeInfoPBComponent>("EnterGameNodeInfoPBComponent",
"enter_gs_type",
sol::property(&EnterGameNodeInfoPBComponent::enter_gs_type, &EnterGameNodeInfoPBComponent::set_enter_gs_type),
"DebugString",
&EnterGameNodeInfoPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
