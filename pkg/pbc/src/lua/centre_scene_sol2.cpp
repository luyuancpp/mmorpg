#include "logic/server/centre_scene.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2centre_scene()
{
tls_lua_state.new_usertype<RegisterSceneRequest>("RegisterSceneRequest",
"game_node_id",
sol::property(&RegisterSceneRequest::game_node_id, &RegisterSceneRequest::set_game_node_id),
"add_scenes_info",
&RegisterSceneRequest::add_scenes_info,
"scenes_info",
[](const RegisterSceneRequest& pb, int index) ->decltype(auto){ return pb.scenes_info(index);},
"mutable_scenes_info",
[](RegisterSceneRequest& pb, int index) ->decltype(auto){ return pb.mutable_scenes_info(index);},
"scenes_info_size",
&RegisterSceneRequest::scenes_info_size,
"clear_scenes_info",
&RegisterSceneRequest::clear_scenes_info,
"DebugString",
&RegisterSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<RegisterSceneResponse>("RegisterSceneResponse",
"DebugString",
&RegisterSceneResponse::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<UnRegisterSceneRequest>("UnRegisterSceneRequest",
"game_node_id",
sol::property(&UnRegisterSceneRequest::game_node_id, &UnRegisterSceneRequest::set_game_node_id),
"scene",
sol::property(&UnRegisterSceneRequest::scene, &UnRegisterSceneRequest::set_scene),
"reason",
sol::property(&UnRegisterSceneRequest::reason, &UnRegisterSceneRequest::set_reason),
"DebugString",
&UnRegisterSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
