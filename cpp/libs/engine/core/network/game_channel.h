#pragma once

#include "muduo/base/Atomic.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/protobuf/ProtobufCodecLite.h"
#include "network/codec/dispatcher.h"

#include <google/protobuf/service.h>
#include <map>
#include <memory>

#include "proto/common/base/game_rpc.pb.h"

// RPC 消息类型定义
using RpcMessagePtr = std::shared_ptr<GameRpcMessage>;

// 固定协议标识
constexpr char RPC_TAG[] = "RPC0";

// 消息的网络传输协议结构
// size      : 4-byte  (N+8) 表示消息总长度
// RPC_TAG   : 4-byte  固定标识 "RPC0"
// payload   : N-byte  消息内容（序列化后的 Protobuf 数据）
// checksum  : 4-byte  Adler-32 校验和（基于 "RPC0" + payload）
using RpcCodec = muduo::net::ProtobufCodecLiteT<GameRpcMessage, RPC_TAG>;

// 使用别名简化代码
using muduo::net::TcpConnectionPtr;
using ProtobufService = ::google::protobuf::Service;
using ProtobufMessage = ::google::protobuf::Message;

// GameChannel 类实现 RPC 调用逻辑
class GameChannel {
public:
    GameChannel();
    explicit GameChannel(const TcpConnectionPtr& connection);

    ~GameChannel();

    // 设置连接
    void SetConnection(const TcpConnectionPtr& connection) { connection_ = connection; }

    // 设置服务
    void SetServiceMap(const std::map<std::string, ProtobufService*>* services) { services_ = services; }

    inline bool IsValidMessageId(uint32_t messageId) const;

    // 获取 Protobuf 分发器
    ProtobufDispatcher& GetDispatcher() { return dispatcher_; }

    // RPC 调用并接收响应
    void CallRemoteMethod(uint32_t messageId, const ProtobufMessage& request);

    // 仅发送请求（无响应）
    void SendRequest(uint32_t messageId, const ProtobufMessage& message);

    // 发送路由消息到其他节点
    void RouteMessageToNode(uint32_t messageId, const ProtobufMessage& request);

    // 发送路由消息的响应
    void SendRouteResponse(uint32_t messageId, uint64_t routeId, const std::string& responseData);

    // 处理接收到的消息
    void HandleIncomingMessage(const TcpConnectionPtr& connection,
        muduo::net::Buffer* buffer,
        muduo::Timestamp receiveTime);

private:
    // 错误消息发送
    void SendErrorResponse(const GameRpcMessage& message, GameErrorCode errorCode);

    // 内部消息处理逻辑
    void HandleRpcMessage(const TcpConnectionPtr& connection, const RpcMessagePtr& message, muduo::Timestamp receiveTime);
    void HandleResponseMessage(const TcpConnectionPtr& connection, const GameRpcMessage& message, muduo::Timestamp receiveTime);
    void HandleRequestMessage(const TcpConnectionPtr& connection, const GameRpcMessage& message, muduo::Timestamp receiveTime);
    void HandleNodeRouteMessage(const TcpConnectionPtr& connection, const GameRpcMessage& message, muduo::Timestamp receiveTime);
    void HandleClientRequestMessage(const TcpConnectionPtr& connection, const GameRpcMessage& message, muduo::Timestamp receiveTime);

    void SendRpcRequestMessage(GameMessageType type, uint32_t messageId, const ProtobufMessage* content);
    void SendRpcResponseMessage(GameMessageType type, uint32_t messageId, const ProtobufMessage* content);

    void ProcessMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime);

    bool SerializeMessage(const ProtobufMessage& message, std::string* output) const;

    // 统计消息处理次数
    void LogMessageStatistics(const GameRpcMessage& message) const;

    // 开启消息统计
    void StartMessageStatistics();

    // 关闭消息统计
    void StopMessageStatistics();

    // 发送 Protobuf 消息
    void SendGameRpcMessage(const GameRpcMessage& message);

    // 成员变量
    RpcCodec codec_;  // 消息编解码器
    TcpConnectionPtr connection_;  // TCP 连接
    const std::map<std::string, ProtobufService*>* services_ = nullptr;  // 服务列表
    ProtobufDispatcher dispatcher_;  // Protobuf 消息分发器
};

// 智能指针类型定义
using GameChannelPtr = std::shared_ptr<GameChannel>;
