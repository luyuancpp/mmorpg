#include "logic/server_player/centre_player_scene.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2centre_player_scene()
{
tls_lua_state.new_usertype<CentreEnterSceneRequest>("CentreEnterSceneRequest",
"scene_info",
[](CentreEnterSceneRequest& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](CentreEnterSceneRequest& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"DebugString",
&CentreEnterSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CentreLeaveSceneRequest>("CentreLeaveSceneRequest",
"DebugString",
&CentreLeaveSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CentreLeaveSceneAsyncSavePlayerCompleteRequest>("CentreLeaveSceneAsyncSavePlayerCompleteRequest",
"DebugString",
&CentreLeaveSceneAsyncSavePlayerCompleteRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
