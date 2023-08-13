#pragma once

#include "muduo/net/EventLoop.h"

#include "src/network/rpc_client.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/rpc_server.h"

#include "src/service/lobby_scene.h"

#include "deploy_service.pb.h"

class LobbyServer : muduo::noncopyable
{
public:
    using RpcServerPtr = std::shared_ptr<muduo::net::RpcServer>;
    LobbyServer(muduo::net::EventLoop* loop);
    ~LobbyServer();

    void Init();

    void ConnectDeploy();

    void StartServer(const ::lobby_server_db& info);

    void Receive1(const OnConnected2ServerEvent& es) const;
    void Receive2(const OnBeConnectedEvent& es)const;
private:
    muduo::net::EventLoop* loop_{ nullptr };
    RpcServerPtr server_;
    RpcClientPtr deploy_session_;
    LobbyServiceImpl impl_;
};
extern LobbyServer* g_lobby_server;



