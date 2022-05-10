#include "./logic_proto/common_client_player.pb.h"
#include <sol/sol.hpp>
extern thread_local sol::state g_lua;
void Pb2sol2__logic_proto_common_client_player()
{
g_lua.new_usertype<TipsS2CRequest>("TipsS2CRequest",
"DebugString",
&TipsS2CRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

g_lua.new_usertype<TipsS2CResponse>("TipsS2CResponse",
"tips",
[](TipsS2CResponse& pb) ->decltype(auto){ return pb.tips();},
"mutable_tips",
[](TipsS2CResponse& pb) ->decltype(auto){ return pb.mutable_tips();},
"DebugString",
&TipsS2CResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
