#include "route_system.h"

#include "src/network/node_info.h"
#include "src/network/rpc_msg_route.h"

#include "src/login_server.h"

void Route2Db(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method)
{
	route2db.set_service(method->service()->full_name());
	route2db.set_method(method->name());
	route_msg_body = msg.SerializeAsString();
}

void Route2Controller(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method)
{
	route2controller.set_service(method->service()->full_name());
	route2controller.set_method(method->name());
	route_msg_body = msg.SerializeAsString();
}

void Route2Gate(const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method)
{
	route2gate.set_service(method->service()->full_name());
	route2gate.set_method(method->name());
	route_msg_body = msg.SerializeAsString();
}
