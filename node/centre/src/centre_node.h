#pragma once

#include "handler/service/centre_service_handler.h"
#include "node/system/node.h"

class CentreNode : public Node
{
public:
	explicit CentreNode(muduo::net::EventLoop* loop);
	~CentreNode() override = default;

	::google::protobuf::Service* GetNodeReplyService() override { return &nodeReplyService; }


private:
	CentreHandler nodeReplyService;
};



