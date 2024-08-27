#include "logic/component/team_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2team_comp()
{
tls_lua_state.new_usertype<TeamId>("TeamId",
"team_id",
sol::property(&TeamId::team_id, &TeamId::set_team_id),
"DebugString",
&TeamId::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
