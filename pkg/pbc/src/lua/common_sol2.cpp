#include "common/common.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2common()
{
tls_lua_state.new_usertype<NodeInfo>("NodeInfo",
"node_id",
sol::property(&NodeInfo::node_id, &NodeInfo::set_node_id),
"node_type",
sol::property(&NodeInfo::node_type, &NodeInfo::set_node_type),
"launch_time",
sol::property(&NodeInfo::launch_time, &NodeInfo::set_launch_time),
"game_node_type",
sol::property(&NodeInfo::game_node_type, &NodeInfo::set_game_node_type),
"DebugString",
&NodeInfo::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<NetworkAddress>("NetworkAddress",
"ip",
sol::property(&NetworkAddress::ip, &NetworkAddress::set_ip<const std::string&>),
"port",
sol::property(&NetworkAddress::port, &NetworkAddress::set_port),
"DebugString",
&NetworkAddress::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<SceneInfoPBComponent>("SceneInfoPBComponent",
"scene_confid",
sol::property(&SceneInfoPBComponent::scene_confid, &SceneInfoPBComponent::set_scene_confid),
"guid",
sol::property(&SceneInfoPBComponent::guid, &SceneInfoPBComponent::set_guid),
"mirror_confid",
sol::property(&SceneInfoPBComponent::mirror_confid, &SceneInfoPBComponent::set_mirror_confid),
"dungen_confid",
sol::property(&SceneInfoPBComponent::dungen_confid, &SceneInfoPBComponent::set_dungen_confid),
"count_creators",
[](SceneInfoPBComponent& pb, uint64_t key) ->decltype(auto){ return pb.creators().count(key);},
"insert_creators",
[](SceneInfoPBComponent& pb, uint64_t key, bool value) ->decltype(auto){ return pb.mutable_creators()->emplace(key, value).second;},
"creators",
[](SceneInfoPBComponent& pb, uint64_t key) ->decltype(auto){
 auto it =  pb.mutable_creators()->find(key);
 if (it == pb.mutable_creators()->end()){ return bool(); }
 return it->second;},
"creators_size",
&SceneInfoPBComponent::creators_size,
"clear_creators",
&SceneInfoPBComponent::clear_creators,
"DebugString",
&SceneInfoPBComponent::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
