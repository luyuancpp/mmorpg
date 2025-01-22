#pragma once

#include "proto/common/deploy_service.pb.h"
#include "entt/src/entt/entity/registry.hpp"
#include "handler/service/centre_service_handler.h"
#include "muduo/base/AsyncLogging.h"
#include "network/rpc_connection_event.h"
#include "node/system/node.h"
#include "redis_client/redis_client.h"

class CentreNode : public Node
{
public:
	explicit CentreNode(muduo::net::EventLoop* loop);
	~CentreNode() override;

	inline PbSyncRedisClientPtr& GetRedis() { return redis_; }


	void InitializeGameConfig() override;
	
	void        Init() override;
	void		ShutdownNode() override;

	NodeInfo& GetNodeInfo()override;
	uint32_t GetNodeType() const override;
    void InitializeSystemBeforeConnection()override;

	
	static void BroadCastRegisterGameToGate(entt::entity gs, entt::entity gate);

	void StartRpcServer(const ::nodes_info_data& info) override;

	void Receive2(const OnBeConnectedEvent& es);

private:
	void InitEventCallback();
	
	void InitSystemAfterConnect() const;

private:
	PbSyncRedisClientPtr redis_;
	CentreServiceHandler centreService;
	nodes_info_data nodesInfo;
};

extern CentreNode* gCentreNode ;


