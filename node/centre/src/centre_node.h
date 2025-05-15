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

    void PrepareForBeforeConnection()override;

	::google::protobuf::Service* GetNodeRepleyService() override { return &nodeReplyService; }

	void StartRpcServer() override;

private:
	PbSyncRedisClientPtr redis_;
	CentreServiceHandler nodeReplyService;
};

extern CentreNode* gCentreNode ;


