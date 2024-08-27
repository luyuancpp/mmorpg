#include "logic/component/scene_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2scene_comp()
{
tls_lua_state.new_usertype<SceneInfoPBComp>("SceneInfoPBComp",
"scene_confid",
sol::property(&SceneInfoPBComp::scene_confid, &SceneInfoPBComp::set_scene_confid),
"guid",
sol::property(&SceneInfoPBComp::guid, &SceneInfoPBComp::set_guid),
"mirror_confid",
sol::property(&SceneInfoPBComp::mirror_confid, &SceneInfoPBComp::set_mirror_confid),
"dungen_confid",
sol::property(&SceneInfoPBComp::dungen_confid, &SceneInfoPBComp::set_dungen_confid),
"count_creators",
[](SceneInfoPBComp& pb, uint64_t key) ->decltype(auto){ return pb.creators().count(key);},
"insert_creators",
[](SceneInfoPBComp& pb, uint64_t key, bool value) ->decltype(auto){ return pb.mutable_creators()->emplace(key, value).second;},
"creators",
[](SceneInfoPBComp& pb, uint64_t key) ->decltype(auto){
 auto it =  pb.mutable_creators()->find(key);
 if (it == pb.mutable_creators()->end()){ return bool(); }
 return it->second;},
"creators_size",
&SceneInfoPBComp::creators_size,
"clear_creators",
&SceneInfoPBComp::clear_creators,
"DebugString",
&SceneInfoPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<CentreChangeSceneInfoPBComp>("CentreChangeSceneInfoPBComp",
"scene_confid",
sol::property(&CentreChangeSceneInfoPBComp::scene_confid, &CentreChangeSceneInfoPBComp::set_scene_confid),
"guid",
sol::property(&CentreChangeSceneInfoPBComp::guid, &CentreChangeSceneInfoPBComp::set_guid),
"dungen_confid",
sol::property(&CentreChangeSceneInfoPBComp::dungen_confid, &CentreChangeSceneInfoPBComp::set_dungen_confid),
"mirror_confid",
sol::property(&CentreChangeSceneInfoPBComp::mirror_confid, &CentreChangeSceneInfoPBComp::set_mirror_confid),
"change_sequence",
sol::property(&CentreChangeSceneInfoPBComp::change_sequence, &CentreChangeSceneInfoPBComp::set_change_sequence),
"change_gs_type",
sol::property(&CentreChangeSceneInfoPBComp::change_gs_type, &CentreChangeSceneInfoPBComp::set_change_gs_type),
"change_gs_status",
sol::property(&CentreChangeSceneInfoPBComp::change_gs_status, &CentreChangeSceneInfoPBComp::set_change_gs_status),
"change_time",
sol::property(&CentreChangeSceneInfoPBComp::change_time, &CentreChangeSceneInfoPBComp::set_change_time),
"ignore_full",
sol::property(&CentreChangeSceneInfoPBComp::ignore_full, &CentreChangeSceneInfoPBComp::set_ignore_full),
"processing",
sol::property(&CentreChangeSceneInfoPBComp::processing, &CentreChangeSceneInfoPBComp::set_processing),
"DebugString",
&CentreChangeSceneInfoPBComp::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
