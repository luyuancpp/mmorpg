#pragma once

#include "common_proto/node_common.pb.h"
#include "src/common_type/common_type.h"

class CommonLogicThreadLocalStorage
{
public:
    RouteData& route_data() { return route_data_; };
    std::string& route_msg_body() { return route_msg_body_; }
    void set_next_route_node_type(uint32_t node_type) { next_route_node_type_ = node_type; }
	uint32_t next_route_node_type()const { return next_route_node_type_; }
	void set_next_route_node_id(uint32_t next_node_id) { next_route_node_id_ = next_node_id; }
	uint32_t next_route_node_id()const { return next_route_node_id_; }
    void set_current_session_id(uint64_t current_session_id) { current_session_id_ = current_session_id; }
    uint64_t session_id()const { return current_session_id_; }
private:
    RouteData route_data_;
    std::string route_msg_body_;
    uint32_t next_route_node_type_{ UINT32_MAX };
    uint32_t next_route_node_id_{ UINT32_MAX };
    uint64_t current_session_id_{ kInvalidSessionId };
};

 extern thread_local CommonLogicThreadLocalStorage cl_tls;