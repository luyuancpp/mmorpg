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

}
