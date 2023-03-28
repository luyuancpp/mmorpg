#pragma once

#include "common_proto/node_common.pb.h"

class CommonLogicThreadLocalStorage
{
public:
    RouteData& route_data() { return route_data_; };
    std::string& route_msg_body() { return route_msg_body_; }
    void set_next_route_node_type(uint32_t note_type) { next_route_node_type_ = note_type; }
	uint32_t next_route_node_type()const { return next_route_node_type_; }
	void set_next_route_node_id(uint32_t next_node_id) { next_route_node_id_ = next_node_id; }
	uint32_t next_route_node_id()const { return next_route_node_id_; }
private:
    RouteData route_data_;
    std::string route_msg_body_;
    uint32_t next_route_node_type_{ UINT32_MAX };
    uint32_t next_route_node_id_{ UINT32_MAX };
};

 extern thread_local CommonLogicThreadLocalStorage cl_tls;