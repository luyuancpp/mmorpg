#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/centre/centre_service.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnCentreGatePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnCentreGateSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnCentreLoginNodeAccountLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::CentreLoginResponse>& replied, Timestamp timestamp);
void OnCentreLoginNodeEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnCentreLoginNodeLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnCentreLoginNodeSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnCentrePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp);
void OnCentreEnterGsSucceedRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnCentreRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp);
void OnCentreRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp);
void OnCentreInitSceneNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnCentreRegisterNodeSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterNodeSessionResponse>& replied, Timestamp timestamp);
void OnCentreHandshakeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::HandshakeResponse>& replied, Timestamp timestamp);
