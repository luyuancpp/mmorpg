#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/slg/slg_map.pb.h"

void OnSlgMapStartMarchReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::StartMarchResponse>& replied, muduo::Timestamp timestamp);
void OnSlgMapCancelMarchReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp);
void OnSlgMapQueryViewportReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::ViewportResponse>& replied, muduo::Timestamp timestamp);
void OnSlgMapBuildReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::BuildResponse>& replied, muduo::Timestamp timestamp);
void OnSlgMapUpgradeBuildingReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::UpgradeBuildingResponse>& replied, muduo::Timestamp timestamp);
void OnSlgMapNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp);
