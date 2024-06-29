#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "handler/centre_service_handler.h"
#include "network/rpc_connection_event.h"
#include "network/rpc_server.h"
#include "redis_client/redis_client.h"
#include "timer_task/timer_task.h"

#include "common_proto/deploy_service.pb.h"

class CentreNode : muduo::noncopyable
{
public:
	
	using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
	
	CentreNode(muduo::net::EventLoop* loop);
	~CentreNode();

	inline PbSyncRedisClientPtr& redis_client() { return redis_; }
	inline uint32_t center_node_id()const { return node_info_.node_id(); }
	inline const NodeInfo& node_info()const { return node_info_; }

	void        Init();
	static void BroadCastRegisterGameToGate(entt::entity gs, entt::entity gate);

    void SetNodeId(NodeId node_id);
    void StartServer(const ::nodes_info_data& info);

	void Receive2(const OnBeConnectedEvent& es);

private:
	static void InitConfig();

	void InitNodeByReqInfo();

	void InitNodeServer();

	static void InitSystemBeforeConnect();
	void InitSystemAfterConnect() const;

	NodeId centre_node_index() { return center_node_id() - 1; }

	muduo::net::EventLoop* loop_{ nullptr };
	PbSyncRedisClientPtr redis_;
	RpcServerPtr server_;

	CentreServiceHandler centre_service_;

	NodeInfo node_info_;
	nodes_info_data server_infos_;

    TimerTask deploy_rpc_timer_;
};

extern CentreNode* g_centre_node;


