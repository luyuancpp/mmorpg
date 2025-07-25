#pragma once

#include "muduo/net/EventLoop.h"
#include "handler/service/game_service_handler.h"
#include "network/rpc_client.h"
#include "network/rpc_connection_event.h"
#include "node/system/node.h"
#include "redis_client/redis_client.h"

class OnConnect2CentrePbEvent;

class SceneNode : public Node
{
public:
    explicit
        SceneNode(muduo::net::EventLoop* loop);
	~SceneNode() override = default;
    
    ::google::protobuf::Service* GetNodeReplyService() override { return &nodeReplyService; }

    void StartRpcServer() override;
    
private:
    SceneHandler nodeReplyService;
    TimerTaskComp worldTimer;
};



