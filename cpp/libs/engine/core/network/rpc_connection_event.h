#pragma once

#include "muduo/net/TcpConnection.h"

struct OnConnected2TcpServerEvent
{
    OnConnected2TcpServerEvent(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn)
    {   
    }

    muduo::net::TcpConnectionPtr conn_;
};



