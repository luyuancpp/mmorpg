#include "./logic_proto/team_client_player.pb.h"
#include <sol/sol.hpp>
using namespace ./logic_proto/team_client_player;
extern thread_local sol::state g_lua;
void Pb2sol2./logic_proto/team_client_player()
{
g_lua.new_usertype<TeamInfoS2CRequest>("TeamInfoS2CRequest",
"DebugString",
&TeamInfoS2CRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

g_lua.new_usertype<TeamInfoS2CResponse>("TeamInfoS2CResponse",
"error",
[](TeamInfoS2CResponse& pb) ->decltype(auto){ return pb.error();},
"mutable_error",
[](TeamInfoS2CResponse& pb) ->decltype(auto){ return pb.mutable_error();},
"DebugString",
&TeamInfoS2CResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
