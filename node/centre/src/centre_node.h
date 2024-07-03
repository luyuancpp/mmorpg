#pragma once

#include "common_proto/deploy_service.pb.h"
#include "entt/src/entt/entity/registry.hpp"
#include "handler/centre_service_handler.h"
#include "muduo/base/AsyncLogging.h"
#include "network/rpc_connection_event.h"
#include "network/rpc_server.h"
#include "redis_client/redis_client.h"
#include "timer_task/timer_task.h"

class CentreNode : muduo::noncopyable
{
public:
	
	using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

	explicit CentreNode(muduo::net::EventLoop* loop);
	~CentreNode();

	inline PbSyncRedisClientPtr& GetRedis() { return redis_; }
	inline uint32_t GetNodeId()const { return node_info_.node_id(); }
	inline const NodeInfo& GetNodeInfo()const { return node_info_; }

	inline [[nodiscard]] muduo::AsyncLogging& Log ( ) { return log_; }
	
	void        Init();
	void		Exit();
	
	static void BroadCastRegisterGameToGate(entt::entity gs, entt::entity gate);

    void SetNodeId(NodeId node_id);
    void StartServer(const ::nodes_info_data& info);

	void Receive2(const OnBeConnectedEvent& es);

private:
	void InitEventCallback();
	void InitLog();
	static void InitConfig();
	static void InitNodeConfig();
	static void InitGameConfig();

	void InitNodeByReqInfo();

	static void InitSystemBeforeConnect();
	void InitSystemAfterConnect() const;

	NodeId GetNodeConfIndex() const { return GetNodeId() - 1; }

	muduo::net::EventLoop* loop_{ nullptr };
	muduo::AsyncLogging log_;
private:
	PbSyncRedisClientPtr redis_;
	RpcServerPtr server_;
	CentreServiceHandler centre_service_;
	NodeInfo node_info_;
	nodes_info_data server_infos_;
    TimerTask deploy_rpc_timer_;
};

extern CentreNode* g_centre_node;


