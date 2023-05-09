#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/common_proto/deploy_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using LobbyServerResponsePtr = std::shared_ptr<LobbyServerResponse>;
void OnLobbyServerReplied(const TcpConnectionPtr& conn, const LobbyServerResponsePtr& replied, Timestamp timestamp);

using SceneSqueueResponesePtr = std::shared_ptr<SceneSqueueResponese>;
void SceneSequenceNodeIdReplied(const TcpConnectionPtr& conn, const SceneSqueueResponesePtr& replied, Timestamp timestamp);