#include "common/config.pb.h"
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
void Pb2sol2config()
{
tls_lua_state.new_usertype<ServiceConfig>("ServiceConfig",
"name",
sol::property(&ServiceConfig::name, &ServiceConfig::set_name<const std::string&>),
"url",
sol::property(&ServiceConfig::url, &ServiceConfig::set_url<const std::string&>),
"DebugString",
&ServiceConfig::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<BaseDeployConfig>("BaseDeployConfig",
"add_etcd_hosts",
[](BaseDeployConfig& pb, const std::string& value) ->decltype(auto){ return pb.add_etcd_hosts(value);},
"etcd_hosts",
[](BaseDeployConfig& pb, int index) ->decltype(auto){ return pb.etcd_hosts(index);},
"set_etcd_hosts",
[](BaseDeployConfig& pb, int index, const std::string& value) ->decltype(auto){ return pb.set_etcd_hosts(index, value);},
"etcd_hosts_size",
&BaseDeployConfig::etcd_hosts_size,
"clear_etcd_hosts",
&BaseDeployConfig::clear_etcd_hosts,
"log_level",
sol::property(&BaseDeployConfig::log_level, &BaseDeployConfig::set_log_level),
"add_services",
&BaseDeployConfig::add_services,
"services",
[](const BaseDeployConfig& pb, int index) ->decltype(auto){ return pb.services(index);},
"mutable_services",
[](BaseDeployConfig& pb, int index) ->decltype(auto){ return pb.mutable_services(index);},
"services_size",
&BaseDeployConfig::services_size,
"clear_services",
&BaseDeployConfig::clear_services,
"add_service_discovery_prefixes",
[](BaseDeployConfig& pb, const std::string& value) ->decltype(auto){ return pb.add_service_discovery_prefixes(value);},
"service_discovery_prefixes",
[](BaseDeployConfig& pb, int index) ->decltype(auto){ return pb.service_discovery_prefixes(index);},
"set_service_discovery_prefixes",
[](BaseDeployConfig& pb, int index, const std::string& value) ->decltype(auto){ return pb.set_service_discovery_prefixes(index, value);},
"service_discovery_prefixes_size",
&BaseDeployConfig::service_discovery_prefixes_size,
"clear_service_discovery_prefixes",
&BaseDeployConfig::clear_service_discovery_prefixes,
"DebugString",
&BaseDeployConfig::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

tls_lua_state.new_usertype<GameConfig>("GameConfig",
"scene_node_type",
sol::property(&GameConfig::scene_node_type, &GameConfig::set_scene_node_type),
"zone_id",
sol::property(&GameConfig::zone_id, &GameConfig::set_zone_id),
"DebugString",
&GameConfig::DebugString,
sol::base_classes, sol::bases<::google::protobuf::Message>());

}
