#include "route_system.h"

#include "src/thread_local/thread_local_storage_common_logic.h"

#include "src/login_server.h"

void Route2Node(uint32_t node_type, uint32_t message_id, const google::protobuf::Message& message)
{
	cl_tls.route_data().set_message_id(message_id);
	cl_tls.route_msg_body() = message.SerializeAsString();
	cl_tls.set_next_route_node_type(node_type);
}

void SendPrevNodeResponse(const google::protobuf::Message& message)
{
	cl_tls.route_msg_body() = message.SerializeAsString();
}