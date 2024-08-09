#pragma once
#include "common/centre_service.pb.h"
#include "muduo/net/TcpConnection.h"

using namespace muduo;
using namespace muduo::net;

void OnCentreServiceRegisterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServiceRegisterGateNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServiceGatePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServiceGateSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServiceLoginNodeAccountLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginResponse>& replied, Timestamp timestamp);

void OnCentreServiceLoginNodeEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServiceLoginNodeLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServiceLoginNodeSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServicePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp);

void OnCentreServiceEnterGsSucceedRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnCentreServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnCentreServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

void OnCentreServiceUnRegisterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

