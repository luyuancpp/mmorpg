#include "common_proto/login_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnLoginServiceLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginResponse>& replied, Timestamp timestamp);

void OnLoginServiceCreatPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginNodeCreatePlayerResponse>& replied, Timestamp timestamp);

void OnLoginServiceEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoginNodeEnterGameResponse>& replied, Timestamp timestamp);

void OnLoginServiceLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnLoginServiceDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnLoginServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnLoginServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

void OnLoginServiceGateConnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

