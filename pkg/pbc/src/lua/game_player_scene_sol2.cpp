#include "logic/server_player/game_player_scene.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2game_player_scene()
{
tls_lua_state.new_usertype<GsEnterSceneRequest>("GsEnterSceneRequest",
"session_id",
sol::property(&GsEnterSceneRequest::session_id, &GsEnterSceneRequest::set_session_id),
"scene_id",
sol::property(&GsEnterSceneRequest::scene_id, &GsEnterSceneRequest::set_scene_id),
"DebugString",
&GsEnterSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<GsLeaveSceneRequest>("GsLeaveSceneRequest",
"change_gs",
sol::property(&GsLeaveSceneRequest::change_gs, &GsLeaveSceneRequest::set_change_gs),
"DebugString",
&GsLeaveSceneRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterSceneS2CRequest>("EnterSceneS2CRequest",
"DebugString",
&EnterSceneS2CRequest::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<EnterScenerS2CResponse>("EnterScenerS2CResponse",
tls_lua_state.new_usertype<>("",
"DebugString",
&::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
