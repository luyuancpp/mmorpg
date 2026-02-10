#pragma once

#include "core/network/codec/codec.h"
#include "muduo/net/EventLoop.h"
#include "network/rpc_client.h"
#include "network/codec/dispatcher.h"
#include "engine/core/type_define/type_define.h"
#include "proto/common/base/message.pb.h"

using RpcClientMessagePtr = std::shared_ptr<ClientRequest>;

struct SessionInfo;
class OnNodeRemovePbEvent;

class RpcClientSessionHandler : muduo::noncopyable
{
public:
    // 构造函数，初始化 codec 和 dispatcher
    RpcClientSessionHandler(ProtobufCodec& codec, ProtobufDispatcher& dispatcher);

    // 返回 codec 引用
    ProtobufCodec& codec() const { return protobufCodec; }

    // 处理连接建立事件
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    // 向客户端发送消息
    void SendMessageToClient(const muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& message) const;

    // 处理来自客户端的 RPC 消息
    void DispatchClientRpcMessage(const muduo::net::TcpConnectionPtr& conn,
        const RpcClientMessagePtr& message,
        muduo::Timestamp);

    // 根据连接获取会话 ID
    static Guid GetSessionId(const muduo::net::TcpConnectionPtr& conn);

    // 向客户端发送提示
    static void SendTipToClient(const muduo::net::TcpConnectionPtr& conn, uint32_t tip_id);

    bool CheckMessageSize(const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn) const;

    bool CheckMessageLimit(SessionInfo& session, const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn)const;

    bool ValidateClientMessage(SessionInfo& session, const RpcClientMessagePtr& request, const muduo::net::TcpConnectionPtr& conn) const;


	void OnNodeRemovePbEventHandler(const OnNodeRemovePbEvent& pb);

private:

    // 处理连接断开事件
    static void HandleConnectionDisconnection(const muduo::net::TcpConnectionPtr& conn);

    static void HandleConnectionEstablished(const muduo::net::TcpConnectionPtr& conn);

    // codec 和 dispatcher 是核心依赖
    ProtobufCodec& protobufCodec;
    ProtobufDispatcher& messageDispatcher;
};
