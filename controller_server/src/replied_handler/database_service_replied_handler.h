#include "common_proto/database_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeLoginResponse>& replied, Timestamp timestamp);

void OnCreatePlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeCreatePlayerResponse>& replied, Timestamp timestamp);

void OnEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeEnterGameResponse>& replied, Timestamp timestamp);

void OnRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

