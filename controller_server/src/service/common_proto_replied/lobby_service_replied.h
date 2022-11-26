#pragma once

#include <memory>

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/logic_proto/lobby_scene.pb.h"

using namespace muduo;
using namespace muduo::net;

using EnterCrossMainSceneResponesePtr = std::shared_ptr<lobbyservcie::EnterCrossMainSceneResponese>;
void OnEnterLobbyMainSceneReplied(const TcpConnectionPtr& conn, const EnterCrossMainSceneResponesePtr& replied, Timestamp timestamp);
