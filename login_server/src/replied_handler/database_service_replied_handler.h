#include "common_proto/database_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnDbServiceLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeLoginResponse>& replied, Timestamp timestamp);

void OnDbServiceCreatePlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeCreatePlayerResponse>& replied, Timestamp timestamp);

void OnDbServiceEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<DatabaseNodeEnterGameResponse>& replied, Timestamp timestamp);

void OnDbServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnDbServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

