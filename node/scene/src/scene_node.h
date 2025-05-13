#pragma once

#include "muduo/net/EventLoop.h"
#include "muduo/base/AsyncLogging.h"

#include "handler/service/game_service_handler.h"
#include "network/rpc_client.h"
#include "network/rpc_connection_event.h"
#include "node/system/node.h"
#include "redis_client/redis_client.h"

#include "proto/common/deploy_service.pb.h"

class OnConnect2CentrePbEvent;

class SceneNode : public Node
{
public:
    explicit
        SceneNode(muduo::net::EventLoop* loop);
	~SceneNode() override = default;

    NodeInfo& GetNodeInfo()override;

    virtual uint32_t GetNodeType() const override;
    
    void Initialize()override;

    ::google::protobuf::Service* GetNodeRepleyService() override { return &nodeReplyService; }

	CanConnectNodeTypeList GetAllowedTargetNodeTypes() override;
    
    virtual void OnConfigLoadSuccessful()override;
    void StartRpcServer() override;
    virtual void PrepareForBeforeConnection() override;
    
    virtual void ReadyForGame()override;

private:
    PbSyncRedisClientPtr redis;
    GameServiceHandler nodeReplyService;
    TimerTaskComp worldTimer;
};



extern SceneNode* gSceneNode;
