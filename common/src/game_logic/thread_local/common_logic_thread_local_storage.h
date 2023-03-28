#pragma once

#include "common_proto/node_common.pb.h"

class CommonLogicThreadLocalStorage
{
public:
    RouteData& route_data() { return route_data_; };
    std::string& route_msg_body() { return route_msg_body_; }
    void set_route_node_type(uint32_t note_type) { route_node_type_ = note_type; }
    uint32_t route_node_type()const { return route_node_type_; }
    void set_need_to_response(bool b) { need_to_response_ = b; }
    bool need_to_response() { return need_to_response_; }
private:
    RouteData route_data_;
    std::string route_msg_body_;
    uint32_t route_node_type_{ UINT32_MAX };
    bool need_to_response_{ false };
};

 extern thread_local CommonLogicThreadLocalStorage cl_tls;