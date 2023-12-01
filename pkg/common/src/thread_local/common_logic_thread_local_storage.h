#pragma once

#include "common_proto/node_common.pb.h"
#include "src/type_define/type_define.h"

using SceneList = std::unordered_map<Guid, entt::entity>;
using PlayerList = std::unordered_map<Guid, entt::entity>;

class CommonLogicThreadLocalStorage
{
public:
    RouteNodeInfo& route_data() { return route_data_; }
    std::string& route_msg_body() { return route_msg_body_; }
    void set_next_route_node_type(const uint32_t node_type) { next_route_node_type_ = node_type; }
    uint32_t next_route_node_type() const { return next_route_node_type_; }
    void set_next_route_node_id(const uint32_t next_node_id) { next_route_node_id_ = next_node_id; }
    uint32_t next_route_node_id() const { return next_route_node_id_; }
    void set_current_session_id(const uint64_t current_session_id) { current_session_id_ = current_session_id; }
    uint64_t session_id() const { return current_session_id_; }
    SceneList& scene_list() { return scene_list_; }
    const SceneList& scene_list() const { return scene_list_; }
    PlayerList& player_list() { return players_; }
    const PlayerList& player_list() const { return players_; }
private:
    RouteNodeInfo route_data_;
    std::string route_msg_body_;
    uint32_t next_route_node_type_{UINT32_MAX};
    uint32_t next_route_node_id_{UINT32_MAX};
    uint64_t current_session_id_{kInvalidSessionId};
    std::string prev_node_replied_;
    SceneList scene_list_;
    PlayerList players_;
};

extern thread_local CommonLogicThreadLocalStorage cl_tls;