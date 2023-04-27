#pragma once

#include "muduo/net/EventLoop.h"

#include "src/event/event.h"
#include "src/network/rpc_client.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/rpc_server.h"

#include "src/service/lobby_scene.h"

#include "deploy_service.pb.h"

class LobbyServer : muduo::noncopyable, public Receiver<LobbyServer>
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    LobbyServer(muduo::net::EventLoop* loop);

    void Init();

    void ConnectDeploy();

    void StartServer(const ::lobby_server_db& info);

    void receive(const OnConnected2ServerEvent& es);
    void receive(const OnBeConnectedEvent& es);
private:
    muduo::net::EventLoop* loop_{ nullptr };
    RpcServerPtr server_;

    RpcClientPtr deploy_session_;

    LobbyServiceImpl impl_;
};
extern LobbyServer* g_lobby_server;



