#include "game_channel.h"
#include <boost/get_pointer.hpp>
#include <google/protobuf/descriptor.h>
#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "network/message_statistics.h"
#include "proto/common/empty.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "core/utils/stat/stat.h"
#include "network/codec/message_response_dispatcher.h"
#include "threading/rpc_manager.h"

using namespace std::placeholders;

// ====================== 全局变量 ======================
constexpr  size_t kMaxMessageByteSize = 2048;

// 处理未知的 Protobuf 消息
void HandleUnknownProtobufMessage(const TcpConnectionPtr&, const MessagePtr& message, muduo::Timestamp) {
    LOG_ERROR << "Unknown Protobuf message received: " << message->GetTypeName().data();
}

void LogIfMessageTooLarge(const GameRpcMessage& rpcMessage) {
	if (const size_t messageSize = rpcMessage.ByteSizeLong(); messageSize > kMaxMessageByteSize) {
		LOG_ERROR << "RPC message size exceeds 2KB, message ID: "
			<< rpcMessage.message_id()  // 假设所有消息都有这个字段
			<< ", size: " << messageSize
			<< ", message content: " << rpcMessage.DebugString();
	}
}

// 全局响应分发器
MessageResponseDispatcher gResponseDispatcher(std::bind(&HandleUnknownProtobufMessage, _1, _2, _3));

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

// 验证消息 ID 是否有效
bool GameChannel::IsValidMessageId(uint32_t messageId) const {
    if (messageId >= gRpcServiceRegistry.size()) {
        LOG_ERROR << "Invalid message ID: " << messageId
            << " (valid range: 0 to " << gRpcServiceRegistry.size() - 1 << ")";
        return false;
    }
    return true;
}

// 序列化消息
bool GameChannel::SerializeMessage(const ProtobufMessage& message, std::string* output) const {
    output->resize(message.ByteSizeLong());
    return message.SerializePartialToArray(output->data(), static_cast<int32_t>(output->size()));
}

// 构造并发送请求消息
void GameChannel::SendRpcRequestMessage(GameMessageType type, uint32_t messageId, const ProtobufMessage* content) {
    if (!IsValidMessageId(messageId)) return;

    GameRpcMessage rpcMessage;
    rpcMessage.set_type(type);
    rpcMessage.set_message_id(messageId);

    if (content && !SerializeMessage(*content, rpcMessage.mutable_request())) {
        LOG_ERROR << "Failed to serialize message for ID: " << messageId;
        return;
    }

    LogIfMessageTooLarge(rpcMessage);

    codec_.send(connection_, rpcMessage);

    LogMessageStatistics(rpcMessage);
}

// 构造并发送响应消息
void GameChannel::SendRpcResponseMessage(GameMessageType type, uint32_t messageId, const ProtobufMessage* content) {
    if (!IsValidMessageId(messageId)) return;
    
    GameRpcMessage rpcMessage;
    rpcMessage.set_type(type);
    rpcMessage.set_message_id(messageId);

    if (content && !SerializeMessage(*content, rpcMessage.mutable_response())) {
        LOG_ERROR << "Failed to serialize message for ID: " << messageId;
        return;
    }

    LogIfMessageTooLarge(rpcMessage);

    codec_.send(connection_, rpcMessage);

    LogMessageStatistics(rpcMessage);
}

// 记录消息统计信息
void GameChannel::LogMessageStatistics(const GameRpcMessage& message) const {
    if (!gFeatureSwitches[kTestMessageStatistics]) return;

    // 获取消息 ID 和消息大小
    uint32_t messageId = message.message_id();
    uint64_t messageSize = message.ByteSizeLong();

    auto now = std::chrono::steady_clock::now();
    auto& statistic = gMessageStatistics[messageId];

    // 如果是第一次记录，初始化时间
    if (gStartTimes[messageId].time_since_epoch().count() <= 0) {
        gStartTimes[messageId] = now;
    }

    // 更新统计信息
    statistic.set_count(statistic.count() + 1);
    statistic.set_flow_rate_total(statistic.flow_rate_total() + messageSize);
    gTotalFlow += messageSize;

    // 计算流量速率
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - gStartTimes[messageId]).count();
    if (duration > 0) {
        uint64_t flowRatePerSecond = statistic.flow_rate_total() / duration;
        statistic.set_flow_rate_second(flowRatePerSecond);
    }

    // 打印统计信息
    LOG_INFO << "Message ID: " << messageId
        << ", Count: " << statistic.count()
        << ", Total Flow: " << statistic.flow_rate_total()
        << " bytes, Flow Rate: " << statistic.flow_rate_second()
        << " bytes/sec, Duration: " << duration << " sec";
}

// 启动消息统计
void GameChannel::StartMessageStatistics() {
    gFeatureSwitches[kTestMessageStatistics] = true;
    gMessageStatistics.fill(MessageStatistics{});
    gStartTimes.fill(std::chrono::steady_clock::time_point{});
    gTotalFlow = 0;

    LOG_INFO << "Message statistics started.";
}

// 停止消息统计
void GameChannel::StopMessageStatistics() {
    gFeatureSwitches[kTestMessageStatistics] = false;

    // 打印最终统计数据
    std::cout << "Final statistics:" << std::endl;
    for (uint32_t messageId = 0; messageId < gMessageStatistics.size(); ++messageId) {
        const auto& stats = gMessageStatistics[messageId];
        LOG_INFO << "Message ID: " << messageId
            << ", Count: " << stats.count()
            << ", Total Flow: " << stats.flow_rate_total()
            << " bytes, Flow Rate: " << stats.flow_rate_second()
            << " bytes/sec";
    }

    LOG_INFO << "Total Flow of All Messages: " << gTotalFlow << " bytes";
    gMessageStatistics.fill(MessageStatistics{});
    gStartTimes.fill(std::chrono::steady_clock::time_point{});

    LOG_INFO << "Message statistics stopped.";
}

// ====================== 公共方法 ======================

// 调用远程方法
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

    LOG_TRACE << "Sending remote method call, message ID: " << messageId;
    SendGameRpcMessage(rpcMessage);
}

// 发送请求消息
void GameChannel::SendRequest(uint32_t messageId, const ProtobufMessage& request) {
    SendRpcRequestMessage(GameMessageType::REQUEST, messageId, &request);
}

// 路由消息到指定节点
void GameChannel::RouteMessageToNode(uint32_t messageId, const ProtobufMessage& request) {
    SendRpcRequestMessage(GameMessageType::NODE_ROUTE, messageId, &request);
}

// 处理接收到的消息
void GameChannel::HandleIncomingMessage(const TcpConnectionPtr& connection, muduo::net::Buffer* buffer, muduo::Timestamp receiveTime) {
    codec_.onMessage(connection, buffer, receiveTime);
}

// 定义 2KB 的常量

void GameChannel::HandleRpcMessage(const TcpConnectionPtr& conn, const RpcMessagePtr& messagePtr, muduo::Timestamp receiveTime) {
    assert(conn == connection_);
    const auto& rpcMessage = *messagePtr;
    
    // 如果消息大小超过 2KB，记录错误日志
    if (const size_t messageSize = rpcMessage.ByteSizeLong(); messageSize > kMaxMessageByteSize) {
        LOG_ERROR << "RPC message size exceeds 2KB, message ID: " << rpcMessage.message_id() << ", size: " << messageSize
                  << ", message content: " << rpcMessage.DebugString();  // 输出 Protobuf 消息内容
    }

    LOG_TRACE << "RPC message received, type: " << rpcMessage.type() << ", message ID: " << rpcMessage.message_id();

    RpcThreadContext::tls_current_conn = conn;

    switch (rpcMessage.type()) {
    case GameMessageType::RESPONSE:
        HandleResponseMessage(conn, rpcMessage, receiveTime);
        break;
    case GameMessageType::REQUEST:
    case GameMessageType::RPC_CLIENT_REQUEST:
        HandleRequestMessage(conn, rpcMessage, receiveTime);
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

    const auto& messageInfo = gRpcServiceRegistry[rpcMessage.message_id()];
	if (!messageInfo.handlerInstance)
	{
		LOG_ERROR << "Message service implementation not found for message ID: " << rpcMessage.message_id();
		return;
	}
    MessagePtr response(messageInfo.handlerInstance->GetResponsePrototype(
        messageInfo.handlerInstance->GetDescriptor()->FindMethodByName(messageInfo.methodName)).New());

    if (!response->ParsePartialFromArray(rpcMessage.response().data(), static_cast<int32_t>(rpcMessage.response().size()))) {
        LOG_ERROR << "Failed to parse response for message ID: " << rpcMessage.message_id();
        return;
    }

    LOG_TRACE << "Dispatching response for message ID: " << rpcMessage.message_id();
    gResponseDispatcher.onProtobufMessage(rpcMessage.message_id(), conn, response, receiveTime);
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

    const auto& messageInfo = gRpcServiceRegistry[rpcMessage.message_id()];
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
    if (messageId >= gRpcServiceRegistry.size()) {
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
    LogIfMessageTooLarge(message);

    // 使用 codec_ 发送消息
    codec_.send(connection_, message);

    // 记录消息统计信息
    LogMessageStatistics(message);
}

