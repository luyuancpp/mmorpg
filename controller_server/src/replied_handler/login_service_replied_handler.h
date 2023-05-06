#include "common_proto/login_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginResponse>& replied, Timestamp timestamp);

void OnCreatPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginNodeCreatePlayerResponse>& replied, Timestamp timestamp);

void OnEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginNodeEnterGameResponse>& replied, Timestamp timestamp);

void OnRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

