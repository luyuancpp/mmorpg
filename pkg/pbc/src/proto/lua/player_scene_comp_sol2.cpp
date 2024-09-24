#include "logic/component/player_scene_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2player_scene_comp()
{
tls_lua_state.new_usertype<PlayerSceneInfoPBComponent>("PlayerSceneInfoPBComponent",
"scene_confid",
sol::property(&PlayerSceneInfoPBComponent::scene_confid, &PlayerSceneInfoPBComponent::set_scene_confid),
"guid",
sol::property(&PlayerSceneInfoPBComponent::guid, &PlayerSceneInfoPBComponent::set_guid),
"DebugString",
&PlayerSceneInfoPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<PlayerSceneContextPBComponent>("PlayerSceneContextPBComponent",
"scene_info",
[](PlayerSceneContextPBComponent& pb) ->decltype(auto){ return pb.scene_info();},
"mutable_scene_info",
[](PlayerSceneContextPBComponent& pb) ->decltype(auto){ return pb.mutable_scene_info();},
"scene_info_last_time",
[](PlayerSceneContextPBComponent& pb) ->decltype(auto){ return pb.scene_info_last_time();},
"mutable_scene_info_last_time",
[](PlayerSceneContextPBComponent& pb) ->decltype(auto){ return pb.mutable_scene_info_last_time();},
"DebugString",
&PlayerSceneContextPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
