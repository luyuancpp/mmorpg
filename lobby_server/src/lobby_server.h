#pragma once

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_server.h"
#include "src/network/rpc_stub.h"
#include "src/service/logic/lobby_scene.h"

#include "deploy_service.pb.h"

class LobbyServer : muduo::noncopyable, public Receiver<LobbyServer>
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    using DeployStub = RpcStub<deploy::DeployService_Stub>;
    LobbyServer(muduo::net::EventLoop* loop);

    void Init();

    void ConnectDeploy();

	using LobbyInfoRpc = std::shared_ptr< NormalClosure<deploy::LobbyServerRequest,
		deploy::LobbyServerResponse>>;
    void StartServer(LobbyInfoRpc replied);
    using SceneNodeSequeIdRpc = std::shared_ptr<NormalClosure<deploy::SceneSqueueRequest, deploy::SceneSqueueResponese>>;
    void SceneSqueueNodeId(SceneNodeSequeIdRpc replied);

    void receive(const OnConnected2ServerEvent& es);
    void receive(const OnBeConnectedEvent& es);
private:
    muduo::net::EventLoop* loop_{ nullptr };
    RpcServerPtr server_;

    RpcClientPtr deploy_rpc_client_;
    DeployStub deploy_stub_;

    LobbyServiceImpl impl_;
};
extern LobbyServer* g_lobby_server;



