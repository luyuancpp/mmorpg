#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "handler/centre_service_handler.h"
#include "redis_client/redis_client.h"
#include "network/rpc_client.h"
#include "network/rpc_connection_event.h"
#include "network/rpc_server.h"
#include "network/rpc_session.h"

#include "common_proto/deploy_service.pb.h"

class CentreNode : muduo::noncopyable
{
public:
	
	using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
	
	CentreNode(muduo::net::EventLoop* loop);
	~CentreNode();

	inline PbSyncRedisClientPtr& redis_client() { return redis_; }
	inline uint32_t center_node_id()const { return serverinfos_.centre_info().id(); }
	inline const NodeInfo& node_info()const { return node_info_; }

	void Init();
	void BroadCastRegisterGameToGate(entt::entity gs, entt::entity gate);

    void StartServer(const ::servers_info_data& info);

	void Receive2(const OnBeConnectedEvent& es);

private:

	void InitConfig();

	void InitNodeByReqInfo();

	void InitNodeServer();

	muduo::net::EventLoop* loop_{ nullptr };
	PbSyncRedisClientPtr redis_;
	RpcServerPtr server_;

	CentreServiceHandler contoller_service_;

	NodeInfo node_info_;
	servers_info_data serverinfos_;
};

extern CentreNode* g_centre_node;


