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
	~CentreNode() override = default;

	inline PbSyncRedisClientPtr& GetRedis() { return redis_; }

	void        Initialize() override;

	NodeInfo& GetNodeInfo()override;

	uint32_t GetNodeType() const override;

    void PrepareForBeforeConnection()override;

	std::string GetServiceName() const override;


	
	static void BroadCastRegisterGameToGate(entt::entity gs, entt::entity gate);

	void StartRpcServer() override;

	void Receive2(const OnBeConnectedEvent& es);

private:
	void InitEventCallback();
	
	void InitSystemAfterConnect() const;

private:
	PbSyncRedisClientPtr redis_;
	CentreServiceHandler centreService;
};

extern CentreNode* gCentreNode ;


