#include "logic/component/player_scene_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_scene_comp()
{
tls_lua_state.new_usertype<PlayerSceneInfoPBComp>("PlayerSceneInfoPBComp",
"scene_confid",
sol::property(&PlayerSceneInfoPBComp::scene_confid, &PlayerSceneInfoPBComp::set_scene_confid),
"guid",
sol::property(&PlayerSceneInfoPBComp::guid, &PlayerSceneInfoPBComp::set_guid),
"DebugString",
&PlayerSceneInfoPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerSceneContextPBComp>("PlayerSceneContextPBComp",
"scene_info",
[](PlayerSceneContextPBComp& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](PlayerSceneContextPBComp& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"scene_info_last_time",
[](PlayerSceneContextPBComp& pb) ->decltype(auto){ return pb.scene_info_last_time();},
"mutable_scene_info_last_time",
[](PlayerSceneContextPBComp& pb) ->decltype(auto){ return pb.mutable_scene_info_last_time();},
"DebugString",
&PlayerSceneContextPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
