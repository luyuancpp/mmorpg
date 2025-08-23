#pragma once

#include "muduo/net/TcpConnection.h"

#include "network/game_channel.h"

struct OnConnected2TcpServerEvent
{
    OnConnected2TcpServerEvent(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn)
    {   
    }

    muduo::net::TcpConnectionPtr conn_;
};

struct OnTcpClientConnectedEvent//服务器被连接
{
    OnTcpClientConnectedEvent(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn)
    {
    }

    muduo::net::TcpConnectionPtr conn_;
};

