#pragma once
#include "common/game_service.pb.h"
#include "muduo/net/TcpConnection.h"

using namespace muduo;
using namespace muduo::net;

void OnGameServicePlayerEnterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGameServiceSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp);

void OnGameServiceClientSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<PlayerClientResponse>& replied, Timestamp timestamp);

void OnGameServiceDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGameServiceRegisterGateNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGameServiceCentreSendToPlayerViaGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGameServiceInvokePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeRouteMessageResponse>& replied, Timestamp timestamp);

void OnGameServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnGameServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

void OnGameServiceUpdateSessionDetailRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGameServiceEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGameServiceCreateSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CreateSceneResponse>& replied, Timestamp timestamp);

