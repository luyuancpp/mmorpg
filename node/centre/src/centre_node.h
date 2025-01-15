#pragma once

#include "proto/common/deploy_service.pb.h"
#include "entt/src/entt/entity/registry.hpp"
#include "handler/service/centre_service_handler.h"
#include "muduo/base/AsyncLogging.h"
#include "network/rpc_connection_event.h"
#include "network/rpc_server.h"
#include "redis_client/redis_client.h"
#include "time/comp/timer_task_comp.h"

class CentreNode : muduo::noncopyable
{
public:
	
	using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;

	explicit CentreNode(muduo::net::EventLoop* loop);
	~CentreNode();

	inline PbSyncRedisClientPtr& GetRedis() { return redis_; }
	uint32_t GetNodeId()const;
	const NodeInfo& GetNodeInfo()const;

	inline [[nodiscard]] muduo::AsyncLogging& Log ( ) { return muduoLog; }
	
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
	static void InitTimeZone();

	void InitNodeByReqInfo();

	static void InitSystemBeforeConnect();
	void InitSystemAfterConnect() const;

	NodeId GetNodeConfIndex() const { return GetNodeId() - 1; }

    void ReleaseNodeId();

	muduo::net::EventLoop* loop_{ nullptr };
	muduo::AsyncLogging muduoLog;
	PbSyncRedisClientPtr redis_;
	RpcServerPtr server_;
	CentreServiceHandler centreService;
	nodes_info_data serversInfo;
    TimerTaskComp deployRpcTimer;
};

extern CentreNode* gCentreNode ;


