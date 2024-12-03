#include "game_channel.h"

#include <boost/get_pointer.hpp>
#include <google/protobuf/descriptor.h>

#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "network/message_statistics.h"
#include "proto/common/empty.pb.h"
#include "service_info/service_info.h"
#include "test/test.h"

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

bool GameChannel::IsValidMessageId(const uint32_t messageId) {
    if (messageId >= gMessageInfo.size()) {
        LOG_ERROR << "Invalid message ID: " << messageId
            << " (out of range, valid range: 0 to " << gMessageInfo.size() - 1 << ")";
        return false;
    }

    const auto& [serviceName, methodName, request, response, serviceImplInstance] = gMessageInfo[messageId];
    if (nullptr == serviceImplInstance) {
        LOG_ERROR << "Service implementation instance is null for message ID: " << messageId;
        return false;
    }

    auto& service = serviceImplInstance;
    const auto desc = service->GetDescriptor();
    if (const auto method = desc->FindMethodByName(methodName); nullptr == method) {
        LOG_ERROR << "Method '" << methodName << "' not found for message ID: " << messageId;
        return false;
    }

    return true;
}

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
    SendProtobufMessage(rpcMessage);
}

void GameChannel::SendRequest(const uint32_t messageId, const ProtobufMessage& request) {
    if (!IsValidMessageId(messageId)) {
        LOG_ERROR << "Invalid message ID for request: " << messageId;
        return;
    }

    GameRpcMessage rpcMessage;
    rpcMessage.set_type(GameMessageType::REQUEST);
    rpcMessage.set_message_id(messageId);

    if (!SerializeMessage(request, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize request for message ID: " << messageId;
        return;
    }

    LOG_DEBUG << "Sending request, message ID: " << messageId;
    SendProtobufMessage(rpcMessage);
}

void GameChannel::RouteMessageToNode(uint32_t messageId, const ProtobufMessage& request) {
    if (!IsValidMessageId(messageId)) {
        LOG_ERROR << "Invalid message ID for node routing: " << messageId;
        return;
    }

    GameRpcMessage rpcMessage;
    rpcMessage.set_type(GameMessageType::NODE_ROUTE);
    rpcMessage.set_message_id(messageId);

    if (!SerializeMessage(request, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize request for node routing, message ID: " << messageId;
        return;
    }

    LOG_DEBUG << "Routing message to node, message ID: " << messageId;
    SendProtobufMessage(rpcMessage);
}

void GameChannel::HandleIncomingMessage(const TcpConnectionPtr& connection, muduo::net::Buffer* buffer, muduo::Timestamp receiveTime) {
    LOG_DEBUG << "Handling incoming message from connection: " << connection->getTcpInfoString();
    codec_.onMessage(connection, buffer, receiveTime);
}

void GameChannel::HandleRpcMessage(const TcpConnectionPtr& conn, const RpcMessagePtr& messagePtr, muduo::Timestamp receiveTime) {
    assert(conn == connection_);

    const auto& rpcMessage = *messagePtr;

    LOG_DEBUG << "Handling RPC message, type: " << rpcMessage.type() << ", message ID: " << rpcMessage.message_id();

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
        LOG_WARN << "Received RPC error, message ID: " << rpcMessage.message_id();
        break;

    default:
        LOG_ERROR << "Unknown RPC message type received, message ID: " << rpcMessage.message_id();
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
        LOG_ERROR << "Service instance not found for message ID: " << rpcMessage.message_id();
        return;
    }

    const auto* descriptor = service->GetDescriptor();
    const auto* method = descriptor->FindMethodByName(messageInfo.methodName);

    if (!method) {
        LOG_ERROR << "Method '" << messageInfo.methodName << "' not found for message ID: " << rpcMessage.message_id();
        return;
    }

    const MessagePtr response(service->GetResponsePrototype(method).New());
    if (!response->ParsePartialFromArray(rpcMessage.response().data(), static_cast<int32_t>(rpcMessage.response().size()))) {
        LOG_ERROR << "Failed to parse response for message ID: " << rpcMessage.message_id();
        return;
    }

    LOG_DEBUG << "Dispatched response for message ID: " << rpcMessage.message_id();
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
        LOG_ERROR << "Invalid message ID for processing: " << rpcMessage.message_id();
        return;
    }

    const auto& messageInfo = gMessageInfo[rpcMessage.message_id()];
    auto it = services_->find(messageInfo.serviceName);

    if (it == services_->end()) {
        LOG_ERROR << "Service not found for message ID: " << rpcMessage.message_id();
        SendErrorResponse(rpcMessage, GameErrorCode::NO_SERVICE);
        return;
    }

    auto* service = it->second;
    const auto* descriptor = service->GetDescriptor();
    const auto* method = descriptor->FindMethodByName(messageInfo.methodName);

    if (!method) {
        LOG_ERROR << "Method '" << messageInfo.methodName << "' not found for message ID: " << rpcMessage.message_id();
        SendErrorResponse(rpcMessage, GameErrorCode::NO_METHOD);
        return;
    }

    MessagePtr request(service->GetRequestPrototype(method).New());
    if (!request->ParsePartialFromArray(rpcMessage.request().data(), static_cast<int32_t>(rpcMessage.response().size()))) {
        LOG_ERROR << "Failed to parse request for message ID: " << rpcMessage.message_id();
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
            LOG_DEBUG << "Sending response for message ID: " << rpcMessage.message_id();
            SendProtobufMessage(rpcResponse);
        }
        else {
            LOG_ERROR << "Failed to serialize response for message ID: " << rpcMessage.message_id();
        }
    }
}

bool GameChannel::SerializeMessage(const ProtobufMessage& message, std::string* output) {
    output->resize(message.ByteSizeLong());
    return message.SerializePartialToArray(output->data(), static_cast<int32_t>(output->size()));
}

void GameChannel::SendErrorResponse(const GameRpcMessage& message, const GameErrorCode errorCode) {
    GameRpcMessage errorResponse;
    errorResponse.set_type(GameMessageType::RPC_ERROR);
    errorResponse.set_error(errorCode);
    errorResponse.set_message_id(message.message_id());

    LOG_ERROR << "Sending error response for message ID: " << message.message_id()
        << ", error code: " << errorCode;
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
void GameChannel::SendProtobufMessage(const GameRpcMessage& message) {
    // 使用 codec_ 发送消息
    codec_.send(connection_, message);

    // 记录消息统计信息
    LogMessageStatistics(message);
}

void GameChannel::LogMessageStatistics(const GameRpcMessage& message) {
    if (gFeatureSwitches[kTestMessageStatistics]) {
        auto& statistic = g_message_statistics[message.message_id()];
        statistic.set_count(statistic.count() + 1);
    }
}
