#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/gate/gate_service.pb.h"

void OnGatePlayerEnterGameNodeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::RegisterGameNodeSessionResponse>& replied, muduo::Timestamp timestamp);
void OnGateSendMessageToPlayerReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp);
void OnGateRouteNodeMessageReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, muduo::Timestamp timestamp);
void OnGateRoutePlayerMessageReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, muduo::Timestamp timestamp);
void OnGateBroadcastToPlayersReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp);
void OnGateBroadcastToSceneReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp);
void OnGateBroadcastToAllReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp);
void OnGateNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp);
void OnGateBindSessionToGateReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::BindSessionToGateResponse>& replied, muduo::Timestamp timestamp);
void OnGateGmGracefulShutdownReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::GmGracefulShutdownResponse>& replied, muduo::Timestamp timestamp);
