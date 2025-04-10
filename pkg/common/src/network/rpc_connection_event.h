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

struct OnBeConnectedEvent//服务器被连接
{
    OnBeConnectedEvent(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn)
    {
    }

    muduo::net::TcpConnectionPtr conn_;
};

