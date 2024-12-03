// Copyright 2010, Shuo Chen. All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "game_channel.h"

#include <boost/get_pointer.hpp>
#include <google/protobuf/descriptor.h>

#include "muduo/base/Logging.h"
#include "network/message_statistics.h"
#include "service_info/service_info.h"
#include "test/test.h"
#include "proto/common/empty.pb.h"

using namespace std::placeholders;

void HandleUnknownProtobufMessage(const TcpConnectionPtr&,
    const MessagePtr& message,
    muduo::Timestamp){
    LOG_ERROR << "onUnknownMessage: " << message->GetTypeName();
}

// Global dispatcher for unknown protobuf messages
ProtobufDispatcher gResponseDispatcher(
    std::bind(&HandleUnknownProtobufMessage, _1, _2, _3));

GameChannel::GameChannel()
    : codec_(std::bind(&GameChannel::HandleRpcMessage, this, _1, _2, _3)),
    services_(nullptr),
    dispatcher_(std::bind(&HandleUnknownProtobufMessage, _1, _2, _3)) {
    LOG_DEBUG << "GameChannel created: " << this;
}

GameChannel::GameChannel(const TcpConnectionPtr& conn)
    : codec_(std::bind(&GameChannel::HandleRpcMessage, this, _1, _2, _3)),
    connection_(conn),
    services_(nullptr),
    dispatcher_(std::bind(&HandleUnknownProtobufMessage, _1, _2, _3)) {
    LOG_DEBUG << "GameChannel created with connection: " << this;
}

GameChannel::~GameChannel() {
    LOG_DEBUG << "GameChannel destroyed: " << this;
}


bool IsValidMessageId(uint32_t messageId) {
    if (messageId >= gMessageInfo.size())
    {
        return false;
    }

    auto& message_info = gMessageInfo[messageId];
    if (nullptr == message_info.serviceImplInstance)
    {
        return false;
    }

    auto& service = message_info.serviceImplInstance;
    const auto& desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(message_info.methodName);
    if (nullptr == method)
    {
        return false;
    }

    return true;
}

void GameChannel::CallRemoteMethod(uint32_t messageId, const ProtobufMessage& request) {
    GameRpcMessage rpcMessage;
    rpcMessage.set_type(GameMessageType::REQUEST);
    rpcMessage.set_message_id(messageId);

    if (!SerializeMessage(request, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize request.";
        return;
    }

    SendProtobufMessage(rpcMessage);
}

void GameChannel::SendRequest(uint32_t messageId, const ProtobufMessage& request) {
    if (!IsValidMessageId(messageId)) {
        LOG_ERROR << "Invalid message ID: " << messageId;
        return;
    }

    GameRpcMessage rpcMessage;
    rpcMessage.set_type(GameMessageType::REQUEST);
    rpcMessage.set_message_id(messageId);

    if (!SerializeMessage(request, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize request.";
        return;
    }

    SendProtobufMessage(rpcMessage);
}

void GameChannel::RouteMessageToNode(uint32_t messageId, const ProtobufMessage& request) {
    GameRpcMessage rpcMessage;
    rpcMessage.set_type(GameMessageType::NODE_ROUTE);
    rpcMessage.set_message_id(messageId);

    if (!SerializeMessage(request, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize request.";
        return;
    }

    SendProtobufMessage(rpcMessage);
}

void GameChannel::HandleIncomingMessage(const TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp receiveTime) {
    codec_.onMessage(conn, buf, receiveTime);
}

void GameChannel::HandleRpcMessage(const TcpConnectionPtr& conn, const RpcMessagePtr& messagePtr, muduo::Timestamp receiveTime) {
    assert(conn == connection_);

    const auto& rpcMessage = *messagePtr;

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
        LOG_WARN << "Received RPC error.";
        break;

    default:
        LOG_ERROR << "Unknown RPC message type.";
        break;
    }
}

void GameChannel::HandleResponseMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    if (!IsValidMessageId(rpcMessage.message_id())) {
        LOG_ERROR << "Invalid response message ID: " << rpcMessage.message_id();
        return;
    }

    const auto& messageInfo = gMessageInfo[rpcMessage.message_id()];
    const auto& service = messageInfo.serviceImplInstance;
    if (!service) {
        LOG_ERROR << "Service instance not found.";
        return;
    }

    const auto* descriptor = service->GetDescriptor();
    const auto* method = descriptor->FindMethodByName(messageInfo.methodName);

    if (!method) {
        LOG_ERROR << "Method not found: " << messageInfo.methodName;
        return;
    }

    const MessagePtr response(service->GetResponsePrototype(method).New());
    if (!response->ParsePartialFromArray(rpcMessage.response().data(), rpcMessage.response().size())) {
        LOG_ERROR << "Failed to parse response.";
        return;
    }

    gResponseDispatcher.onProtobufMessage(conn, response, receiveTime);
}

void GameChannel::HandleRequestMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    ProcessMessage(conn, rpcMessage, receiveTime);
}

void GameChannel::HandleClientRequestMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    ProcessMessage(conn, rpcMessage, receiveTime);
}

void GameChannel::HandleNodeRouteMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    // Add routing logic here
}

void GameChannel::ProcessMessage(const TcpConnectionPtr& conn, const GameRpcMessage& rpcMessage, muduo::Timestamp receiveTime) {
    if (!IsValidMessageId(rpcMessage.message_id())) {
        LOG_ERROR << "Invalid message ID: " << rpcMessage.message_id();
        return;
    }

    const auto& messageInfo = gMessageInfo[rpcMessage.message_id()];
    auto it = services_->find(messageInfo.serviceName);

    if (it == services_->end()) {
        SendErrorResponse(rpcMessage, GameErrorCode::NO_SERVICE);
        return;
    }

    auto* service = it->second;
    const auto* descriptor = service->GetDescriptor();
    const auto* method = descriptor->FindMethodByName(messageInfo.methodName);

    if (!method) {
        SendErrorResponse(rpcMessage, GameErrorCode::NO_METHOD);
        return;
    }

    MessagePtr request(service->GetRequestPrototype(method).New());
    if (!request->ParsePartialFromArray(rpcMessage.request().data(), rpcMessage.request().size())) {
        LOG_ERROR << "Failed to parse request.";
        SendErrorResponse(rpcMessage, GameErrorCode::INVALID_REQUEST);
        return;
    }

    MessagePtr response(service->GetResponsePrototype(method).New());
    service->CallMethod(method, nullptr, boost::get_pointer(request), boost::get_pointer(response), nullptr);

    if (Empty::GetDescriptor() == response->GetDescriptor()) {
        return;
    }

    if (response->ByteSizeLong() > 0) {
        GameRpcMessage rpcResponse;
        rpcResponse.set_type(GameMessageType::RESPONSE);
        rpcResponse.set_message_id(rpcMessage.message_id());

        if (SerializeMessage(*response, rpcResponse.mutable_response())) {
            SendProtobufMessage(rpcResponse);
        }
        else {
            LOG_ERROR << "Failed to serialize response.";
        }
    }

}

bool GameChannel::SerializeMessage(const ProtobufMessage& message, std::string* output) {
    output->resize(message.ByteSizeLong());
    return message.SerializePartialToArray(output->data(), output->size());
}

void GameChannel::SendErrorResponse(const GameRpcMessage& rpcMessage, GameErrorCode errorCode) {
    GameRpcMessage errorResponse;
    errorResponse.set_type(GameMessageType::RPC_ERROR);
    errorResponse.set_error(errorCode);
    errorResponse.set_message_id(rpcMessage.message_id());

    SendProtobufMessage(errorResponse);
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
    SendProtobufMessage(response);
}

void GameChannel::LogMessageStatistics(const GameRpcMessage& message) {
    // 检查消息统计功能是否启用
    if (!gFeatureSwitches[kTestMessageStatistics]) {
        return;
    }

    // 更新统计计数
    auto& statistic = g_message_statistics[message.message_id()];
    statistic.set_count(statistic.count() + 1);
}

void GameChannel::SendProtobufMessage(const GameRpcMessage& message) {
    // 使用 codec_ 发送消息
    codec_.send(connection_, message);

    // 记录消息统计信息
    LogMessageStatistics(message);
}
