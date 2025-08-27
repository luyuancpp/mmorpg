#pragma once

#include "handler/rpc/centre_service_handler.h"
#include "base/core/node/system/node/node.h"

class CentreNode : public Node
{
public:
	explicit CentreNode(muduo::net::EventLoop* loop);
	~CentreNode() override = default;

	::google::protobuf::Service* GetNodeReplyService() override { return &nodeReplyService; }

private:
	CentreHandler nodeReplyService;
};



