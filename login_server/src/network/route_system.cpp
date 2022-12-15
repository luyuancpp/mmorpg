#include "route_system.h"

#include "src/network/node_info.h"
#include "src/network/rpc_msg_route.h"

#include "src/login_server.h"

void Route2Db(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method)
{
	g_route2db_msg.set_service(method->service()->full_name());
	g_route2db_msg.set_method(method->name());
}

void Route2Controller(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method)
{
	g_route2controller_msg.set_service(method->service()->full_name());
	g_route2controller_msg.set_method(method->name());
}

void Route2Gate(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method)
{
	g_route2gate_msg.set_service(method->service()->full_name());
	g_route2gate_msg.set_method(method->name());
}
