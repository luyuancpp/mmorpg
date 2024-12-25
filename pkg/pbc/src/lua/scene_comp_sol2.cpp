#include "logic/component/scene_comp.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2scene_comp()
{
tls_lua_state.new_usertype<ChangeSceneInfoPBComponent>("ChangeSceneInfoPBComponent",
"scene_confid",
sol::property(&ChangeSceneInfoPBComponent::scene_confid, &ChangeSceneInfoPBComponent::set_scene_confid),
"guid",
sol::property(&ChangeSceneInfoPBComponent::guid, &ChangeSceneInfoPBComponent::set_guid),
"dungen_confid",
sol::property(&ChangeSceneInfoPBComponent::dungen_confid, &ChangeSceneInfoPBComponent::set_dungen_confid),
"mirror_confid",
sol::property(&ChangeSceneInfoPBComponent::mirror_confid, &ChangeSceneInfoPBComponent::set_mirror_confid),
"change_sequence",
sol::property(&ChangeSceneInfoPBComponent::change_sequence, &ChangeSceneInfoPBComponent::set_change_sequence),
"change_gs_type",
sol::property(&ChangeSceneInfoPBComponent::change_gs_type, &ChangeSceneInfoPBComponent::set_change_gs_type),
"change_gs_status",
sol::property(&ChangeSceneInfoPBComponent::change_gs_status, &ChangeSceneInfoPBComponent::set_change_gs_status),
"change_time",
sol::property(&ChangeSceneInfoPBComponent::change_time, &ChangeSceneInfoPBComponent::set_change_time),
"ignore_full",
sol::property(&ChangeSceneInfoPBComponent::ignore_full, &ChangeSceneInfoPBComponent::set_ignore_full),
"processing",
sol::property(&ChangeSceneInfoPBComponent::processing, &ChangeSceneInfoPBComponent::set_processing),
"DebugString",
&ChangeSceneInfoPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
