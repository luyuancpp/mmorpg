#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/scene/player_state_attribute_sync.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnEntitySyncServiceSyncBaseAttributeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnEntitySyncServiceSyncAttribute2FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnEntitySyncServiceSyncAttribute5FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnEntitySyncServiceSyncAttribute10FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnEntitySyncServiceSyncAttribute30FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
void OnEntitySyncServiceSyncAttribute60FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
