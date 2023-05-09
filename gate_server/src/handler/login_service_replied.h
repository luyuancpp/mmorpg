#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/common_proto/login_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using CreatePlayerResponsePtr = std::shared_ptr<LoginNodeCreatePlayerResponse>;
void OnServerCreatePlayerReplied(const TcpConnectionPtr& conn, const CreatePlayerResponsePtr& replied, Timestamp timestamp);

using EnterGameResponsePtr = std::shared_ptr<LoginNodeEnterGameResponse>;
void OnEnterGameReplied(const TcpConnectionPtr& conn, const EnterGameResponsePtr& replied, Timestamp timestamp);

using LoginLoginResponsePtr = std::shared_ptr<LoginResponse>;
void OnServerLoginReplied(const TcpConnectionPtr& conn, const LoginLoginResponsePtr& replied, Timestamp timestamp);