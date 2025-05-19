#pragma once

#include "handler/service/centre_service_handler.h"
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

	::google::protobuf::Service* GetNodeReplyService() override { return &nodeReplyService; }

	void StartRpcServer() override;

private:
	PbSyncRedisClientPtr redis_;
	CentreServiceHandler nodeReplyService;
};

extern CentreNode* gCentreNode ;


