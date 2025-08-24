#pragma once

#include <memory>
#include <string>
#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "network/game_channel.h"

// RpcSession：封装了与 TcpConnection 相关的 RPC 操作
class RpcSession
{
public:
    // 构造函数：初始化 TcpConnection 和 GameChannel
    explicit RpcSession(const muduo::net::TcpConnectionPtr& conn)
        : connection(conn),
          channel_(boost::any_cast<GameChannelPtr>(conn->getContext())) {}

    // 检查连接是否有效
    [[nodiscard]] bool IsConnected() const
    {
        return connection && connection->connected();
    }

    // 调用远程方法
    void CallRemoteMethod(uint32_t messageId, const ::google::protobuf::Message& request) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot call remote method.";
            return;
        }
        channel_->CallRemoteMethod(messageId, request);
    }

    // 发送请求
    void SendRequest(uint32_t messageId, const ::google::protobuf::Message& message) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot send request.";
            return;
        }
        channel_->SendRequest(messageId, message);
    }

    // 路由消息到节点
    void RouteMessageToNode(uint32_t messageId, const ::google::protobuf::Message& message) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot route message.";
            return;
        }
        channel_->RouteMessageToNode(messageId, message);
    }

    // 发送路由响应
    void SendRouteResponse(uint32_t messageId, uint64_t id, const std::string& messageBytes) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot send route response.";
            return;
        }
        channel_->SendRouteResponse(messageId, id, messageBytes);
    }

    muduo::net::TcpConnectionPtr connection; // 连接对象

private:
    GameChannelPtr channel_; // 游戏通道对象
};

// 判断连接地址是否与服务器信息匹配
template<typename ServerInfo>
bool IsSameAddress(const muduo::net::InetAddress& connAddr, const ServerInfo& serverInfo)
{
    return serverInfo.ip() == connAddr.toIp() && serverInfo.port() == connAddr.port();
}

// 重载函数：判断两个 InetAddress 是否匹配
bool IsSameAddress(const muduo::net::InetAddress& connAddr, const muduo::net::InetAddress& serverAddr);

