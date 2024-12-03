﻿#include "game_channel.h"
#include <boost/get_pointer.hpp>
#include <google/protobuf/descriptor.h>
#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "network/message_statistics.h"
#include "proto/common/empty.pb.h"
#include "service_info/service_info.h"
#include "test/test.h"

using namespace std::placeholders;

// ====================== 全局变量 ======================

// 处理未知的 Protobuf 消息
void HandleUnknownProtobufMessage(const TcpConnectionPtr&, const MessagePtr& message, muduo::Timestamp) {
    LOG_ERROR << "Unknown Protobuf message received: " << message->GetTypeName();
}

// 全局响应分发器
ProtobufDispatcher gResponseDispatcher(std::bind(&HandleUnknownProtobufMessage, _1, _2, _3));

// ====================== GameChannel 类实现 ======================

GameChannel::GameChannel()
    : codec_(std::bind(&GameChannel::HandleRpcMessage, this, _1, _2, _3)),
    services_(nullptr),
    dispatcher_(std::bind(&HandleUnknownProtobufMessage, _1, _2, _3)) {
    LOG_DEBUG << "GameChannel created: " << this;
}

GameChannel::GameChannel(const TcpConnectionPtr& connection)
    : codec_(std::bind(&GameChannel::HandleRpcMessage, this, _1, _2, _3)),
    connection_(connection),
    services_(nullptr),
    dispatcher_(std::bind(&HandleUnknownProtobufMessage, _1, _2, _3)) {
    LOG_DEBUG << "GameChannel created with connection: " << this;
}

GameChannel::~GameChannel() {
    LOG_DEBUG << "GameChannel destroyed: " << this;
}

// ====================== 私有方法 ======================

// 验证消息 ID 的合法性
bool GameChannel::IsValidMessageId(uint32_t messageId) const {
    if (messageId >= gMessageInfo.size()) {
        LOG_ERROR << "Invalid message ID: " << messageId << " (valid range: 0 to " << gMessageInfo.size() - 1 << ")";
        return false;
    }
    const auto& [serviceName, methodName, request, response, serviceImplInstance] = gMessageInfo[messageId];
    if (!serviceImplInstance) {
        LOG_ERROR << "No service instance for message ID: " << messageId;
        return false;
    }
    if (!serviceImplInstance->GetDescriptor()->FindMethodByName(methodName)) {
        LOG_ERROR << "Method '" << methodName << "' not found for message ID: " << messageId;
        return false;
    }
    return true;
}

// 序列化消息
bool GameChannel::SerializeMessage(const ProtobufMessage& message, std::string* output) const {
    output->resize(message.ByteSizeLong());
    return message.SerializePartialToArray(output->data(), static_cast<int32_t>(output->size()));
}

// 构造并发送消息
void GameChannel::SendRpcRequestMessage(GameMessageType type, uint32_t messageId, const ProtobufMessage* content) {
    if (!IsValidMessageId(messageId)) return;

    GameRpcMessage rpcMessage;
    rpcMessage.set_type(type);
    rpcMessage.set_message_id(messageId);

    if (content && !SerializeMessage(*content, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize message for ID: " << messageId;
        return;
    }

    codec_.send(connection_, rpcMessage);
    LogMessageStatistics(rpcMessage);
}

void GameChannel::SendRpcResponseMessage(GameMessageType type, uint32_t messageId, const ProtobufMessage* content)
{
    if (!IsValidMessageId(messageId)) return;

    GameRpcMessage rpcMessage;
    rpcMessage.set_type(type);
    rpcMessage.set_message_id(messageId);

    if (content && !SerializeMessage(*content, rpcMessage.mutable_response())) {
        LOG_ERROR << "Failed to serialize message for ID: " << messageId;
        return;
    }

    codec_.send(connection_, rpcMessage);
    LogMessageStatistics(rpcMessage);
}

// 记录消息统计信息
void GameChannel::LogMessageStatistics(const GameRpcMessage& message) const {
    if (gFeatureSwitches[kTestMessageStatistics]) {
        auto& statistic = g_message_statistics[message.message_id()];
        statistic.set_count(statistic.count() + 1);
    }
}

// ====================== 公共方法 ======================

void GameChannel::CallRemoteMethod(const uint32_t messageId, const ProtobufMessage& request) {
    if (!IsValidMessageId(messageId)) {
        LOG_ERROR << "Failed to validate message ID for remote method call: " << messageId;
        return;
    }

    GameRpcMessage rpcMessage;
    rpcMessage.set_type(GameMessageType::REQUEST);
    rpcMessage.set_message_id(messageId);

    if (!SerializeMessage(request, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize request for message ID: " << messageId;
        return;
    }

    LOG_DEBUG << "Sending remote method call, message ID: " << messageId;
    SendGameRpcMessage(rpcMessage);
}

// 发送请求消息
void GameChannel::SendRequest(uint32_t messageId, const ProtobufMessage& request) {
    SendRpcRequestMessage(GameMessageType::REQUEST, messageId, &request);
}

// 路由消息到节点
void GameChannel::RouteMessageToNode(uint32_t messageId, const ProtobufMessage& request) {
    SendRpcRequestMessage(GameMessageType::NODE_ROUTE, messageId, &request);
}

// 处理接收的消息
void GameChannel::HandleIncomingMessage(const TcpConnectionPtr& connection, muduo::net::Buffer* buffer, muduo::Timestamp receiveTime) {
    LOG_DEBUG << "Incoming message from connection: " << connection->getTcpInfoString();
    codec_.onMessage(connection, buffer, receiveTime);
}

// 处理 RPC 消息
void GameChannel::HandleRpcMessage(const TcpConnectionPtr& conn, const RpcMessagePtr& messagePtr, muduo::Timestamp receiveTime) {
    assert(conn == connection_);
    const auto& rpcMessage = *messagePtr;

    LOG_DEBUG << "RPC message received, type: " << rpcMessage.type() << ", message ID: " << rpcMessage.message_id();

    switch (rpcMessage.type()) {
    case GameMessageType::RESPONSE:
        HandleResponseMessage(conn, rpcMessage, receiveTime);
        break;
    case GameMessageType::REQUEST:
        HandleRequestMessage(conn, rpcMessage, receiveTime);
        break;

    case GameMessageType::RPC_CLIENT_REQUEST:
        HandleClientRequestMessage(conn, rpcMessage, receiveTime);
        break;
    case GameMessageType::NODE_ROUTE:
        HandleNodeRouteMessage(conn, rpcMessage, receiveTime);
        break;
    case GameMessageType::RPC_ERROR:
        LOG_WARN << "RPC error received, message ID: " << rpcMessage.message_id();
        break;
    default:
        LOG_ERROR << "Unknown RPC message type, message ID: " << rpcMessage.message_id();
        break;
    }
}

// 处理响应消息
void GameChannel::HandleResponseMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    if (!IsValidMessageId(rpcMessage.message_id())) return;

    const auto& messageInfo = gMessageInfo[rpcMessage.message_id()];
    MessagePtr response(messageInfo.serviceImplInstance->GetResponsePrototype(
        messageInfo.serviceImplInstance->GetDescriptor()->FindMethodByName(messageInfo.methodName)).New());

    if (!response->ParsePartialFromArray(rpcMessage.response().data(), static_cast<int32_t>(rpcMessage.response().size()))) {
        LOG_ERROR << "Failed to parse response for message ID: " << rpcMessage.message_id();
        return;
    }

    LOG_DEBUG << "Dispatching response for message ID: " << rpcMessage.message_id();
    gResponseDispatcher.onProtobufMessage(conn, response, receiveTime);
}

void GameChannel::HandleRequestMessage(const TcpConnectionPtr& connection, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    ProcessMessage(connection, rpcMessage, receiveTime);
}

void GameChannel::HandleClientRequestMessage(const TcpConnectionPtr& connection, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    ProcessMessage(connection, rpcMessage, receiveTime);
}

void GameChannel::HandleNodeRouteMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    // Add routing logic here
}

void GameChannel::ProcessMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    if (!IsValidMessageId(rpcMessage.message_id())) {
        SendErrorResponse(rpcMessage, GameErrorCode::INVALID_REQUEST);
        return;
    }

    const auto& messageInfo = gMessageInfo[rpcMessage.message_id()];
    auto it = services_->find(messageInfo.serviceName);
    if (it == services_->end()) {
        SendErrorResponse(rpcMessage, GameErrorCode::NO_SERVICE);
        return;
    }

    auto* service = it->second;
    const auto* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
    if (!method) {
        SendErrorResponse(rpcMessage, GameErrorCode::NO_METHOD);
        return;
    }

    MessagePtr request(service->GetRequestPrototype(method).New());
    if (!request->ParsePartialFromArray(rpcMessage.request().data(), static_cast<int32_t>(rpcMessage.request().size()))) {
        SendErrorResponse(rpcMessage, GameErrorCode::INVALID_REQUEST);
        return;
    }

    MessagePtr response(service->GetResponsePrototype(method).New());
    service->CallMethod(method, nullptr, boost::get_pointer(request), boost::get_pointer(response), nullptr);

    if (Empty::GetDescriptor() == response->GetDescriptor()) {
        return;
    }
    if (response->ByteSizeLong() > 0) {
        SendRpcResponseMessage(GameMessageType::RESPONSE, rpcMessage.message_id(), response.get());
    }
}

// 发送错误响应
void GameChannel::SendErrorResponse(const GameRpcMessage& message, GameErrorCode errorCode) {
    GameRpcMessage errorResponse;
    errorResponse.set_type(GameMessageType::RPC_ERROR);
    errorResponse.set_error(errorCode);
    errorResponse.set_message_id(message.message_id());
    SendGameRpcMessage(errorResponse);
}

void GameChannel::SendRouteResponse(uint32_t messageId, uint64_t id, const std::string& body) {
    // 检查 messageId 的合法性
    if (messageId >= gMessageInfo.size()) {
        LOG_ERROR << "Invalid message_id: " << messageId;
        return;
    }

    // 构造 GameRpcMessage 响应
    GameRpcMessage response;
    response.set_type(GameMessageType::RESPONSE); // 设置消息类型为响应
    response.set_message_id(messageId);         // 设置消息 ID
    response.set_response(body);               // 设置响应内容

    // 发送响应消息
    SendGameRpcMessage(response);
}

void GameChannel::SendGameRpcMessage(const GameRpcMessage& message) {
    // 使用 codec_ 发送消息
    codec_.send(connection_, message);

    // 记录消息统计信息
    LogMessageStatistics(message);
}
