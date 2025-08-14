#pragma once

#include "config_loader/config.h"
#include "proto/common/message.pb.h"
#include "type_define/type_define.h"
#include <muduo/contrib/hiredis/hiredis.h>

class ThreadLocalStorageCommonLogic
{
public:
	using HiredisPtr = std::unique_ptr<hiredis::Hiredis>;

    RoutingNodeInfo& GetRoutingNodeInfo() { return route_data_; }
    std::string& RouteMsgBody() { return route_msg_body_; }
    void SetNextRouteNodeType(const uint32_t node_type) { next_route_node_type_ = node_type; }
    uint32_t GetNextRouteNodeType() const { return next_route_node_type_; }
    void SeNextRouteNodeId(const uint32_t next_node_id) { next_route_node_id_ = next_node_id; }
    uint32_t GetNextRouteNodeId() const { return next_route_node_id_; }
    void SetCurrentSessionId(const uint64_t current_session_id) { current_session_id_ = current_session_id; }
    uint64_t GetSessionId() const { return current_session_id_; }

    [[nodiscard]] ::BaseDeployConfig& GetBaseDeployConfig()
    {
        return BaseDeployConfig;
    }

    void SetBaseDeployConfig(const ::BaseDeployConfig& baseDeployConfig)
    {
        BaseDeployConfig = baseDeployConfig;
    }

    [[nodiscard]] GameConfig& GetGameConfig()
    {
        return GameConfig;
    }

    void SetGameConfig(const GameConfig& gameConfig)
    {
        GameConfig = gameConfig;
    }

	HiredisPtr& GetZoneRedis()
	{
		return zoneRedis;
	}
private:
    RoutingNodeInfo route_data_;
    std::string route_msg_body_;
    uint32_t next_route_node_type_{UINT32_MAX};
    uint32_t next_route_node_id_{UINT32_MAX};
    uint64_t current_session_id_{kInvalidSessionId};
    std::string prev_node_replied_;
    BaseDeployConfig BaseDeployConfig;
    GameConfig GameConfig;
	HiredisPtr zoneRedis;
};

extern thread_local ThreadLocalStorageCommonLogic tlsCommonLogic;


