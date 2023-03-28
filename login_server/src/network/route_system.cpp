#include "route_system.h"

#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"

#include "src/login_server.h"

void Route2Node(uint32_t note_type, const google::protobuf::Message& msg, const google::protobuf::MethodDescriptor* method)
{
	cl_tls.route_data().set_service(method->service()->full_name());
	cl_tls.route_data().set_method(method->name());
	cl_tls.route_msg_body() = std::move(msg.SerializeAsString());
	cl_tls.set_route_node_type(note_type);
}
