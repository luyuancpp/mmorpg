#pragma once

#include "handler/service/centre_service_handler.h"
#include "node/system/node.h"
#include "redis_client/redis_client.h"

class CentreNode : public Node
{
public:
	explicit CentreNode(muduo::net::EventLoop* loop);
	~CentreNode() override = default;

	void        Initialize() override;

    void InitGlobalData()override;

	::google::protobuf::Service* GetNodeReplyService() override { return &nodeReplyService; }

	void StartRpcServer() override;

private:
	CentreServiceHandler nodeReplyService;
};

extern CentreNode* gCentreNode ;


