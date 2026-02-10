#pragma once
#pragma once

#include "proto/common/base/message.pb.h"
#include "engine/core/type_define/type_define.h"

class MessageContext 
{
public:

	MessageContext () = default;

	MessageContext (const MessageContext &) = delete;
	MessageContext & operator=(const MessageContext &) = delete;

	RoutingNodeInfo& GetRoutingNodeInfo() { return route_data_; }
	std::string& RouteMsgBody() { return route_msg_body_; }
	void SetNextRouteNodeType(const uint32_t node_type) { next_route_node_type_ = node_type; }
	uint32_t GetNextRouteNodeType() const { return next_route_node_type_; }
	void SetNextRouteNodeId(const uint32_t next_node_id) { next_route_node_id_ = next_node_id; }
	uint32_t GetNextRouteNodeId() const { return next_route_node_id_; }
	void SetCurrentSessionId(const uint64_t current_session_id) { current_session_id_ = current_session_id; }
	uint64_t GetSessionId() const { return current_session_id_; }

private:
	RoutingNodeInfo route_data_;
	std::string route_msg_body_;
	uint32_t next_route_node_type_{ UINT32_MAX };
	uint32_t next_route_node_id_{ UINT32_MAX };
	uint64_t current_session_id_{ kInvalidSessionId };
	std::string prev_node_replied_;
};


extern thread_local MessageContext tlsMessageContext;
