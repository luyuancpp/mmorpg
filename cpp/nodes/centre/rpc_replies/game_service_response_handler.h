#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/scene/game_service.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnScenePlayerEnterGameNodeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnSceneSendMessageToPlayerReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp);
void OnSceneClientSendMessageToPlayerReply(const TcpConnectionPtr& conn, const std::shared_ptr<::ClientSendMessageToPlayerResponse>& replied, Timestamp timestamp);
void OnSceneCentreSendToPlayerViaGameNodeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnSceneInvokePlayerServiceReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp);
void OnSceneRouteNodeStringMsgReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp);
void OnSceneRoutePlayerStringMsgReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp);
void OnSceneUpdateSessionDetailReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnSceneEnterSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnSceneCreateSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::CreateSceneResponse>& replied, Timestamp timestamp);
void OnSceneNodeHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp);
