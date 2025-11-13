#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/service/cpp/rpc/gate/gate_service.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnGatePlayerEnterGameNodeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterGameNodeSessionResponse>& replied, Timestamp timestamp);
void OnGateSendMessageToPlayerReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnGateKickSessionByCentreReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnGateRouteNodeMessageReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp);
void OnGateRoutePlayerMessageReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp);
void OnGateBroadcastToPlayersReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnGateNodeHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp);
void OnGateBindSessionToGateReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
