#pragma once

#include "common_proto/node_common.pb.h"

class CommonLogicThreadLocalStorage
{
public:
    RouteInfo& route_info() { return route_info_; };
    std::string& route_msg_body() { return route_msg_body_; }
    void set_route_node_type(uint32_t note_type) { route_node_type_ = note_type; }
    uint32_t route_node()const { return route_node_type_; }

private:
    RouteInfo route_info_;
    std::string route_msg_body_;
    uint32_t route_node_type_;
};

 extern thread_local CommonLogicThreadLocalStorage cl_tls;