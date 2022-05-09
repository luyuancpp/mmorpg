#include "./logic_proto/scene_client_player.pb.h"
#include <sol/sol.hpp>
using namespace ./logic_proto/scene_client_player;
extern thread_local sol::state g_lua;
void Pb2sol2./logic_proto/scene_client_player()
{
g_lua.new_usertype<EnterSeceneC2SRequest>("EnterSeceneC2SRequest",
"scene_info",
[](EnterSeceneC2SRequest& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](EnterSeceneC2SRequest& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"DebugString",
&EnterSeceneC2SRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

g_lua.new_usertype<EnterSeceneC2SResponse>("EnterSeceneC2SResponse",
"error",
[](EnterSeceneC2SResponse& pb) ->decltype(auto){ return pb.error();},
"mutable_error",
[](EnterSeceneC2SResponse& pb) ->decltype(auto){ return pb.mutable_error();},
"DebugString",
&EnterSeceneC2SResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

g_lua.new_usertype<EnterSeceneS2C>("EnterSeceneS2C",
"scene_info",
[](EnterSeceneS2C& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](EnterSeceneS2C& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"DebugString",
&EnterSeceneS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

g_lua.new_usertype<SceneInfoS2C>("SceneInfoS2C",
"add_scene_info",
&SceneInfoS2C::add_scene_info,
"scene_info",
[](SceneInfoS2C& pb, int index) ->decltype(auto){ return pb.scene_info(index);},
"mutable_scene_info",
[](SceneInfoS2C& pb, int index) ->decltype(auto){ return pb.mutable_scene_info(index);},
"scene_info_size",
&SceneInfoS2C::scene_info_size,
"clear_scene_info",
&SceneInfoS2C::clear_scene_info,
"DebugString",
&SceneInfoS2C::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
