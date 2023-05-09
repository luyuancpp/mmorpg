#include "common_proto/game_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnGameServiceEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnGameServiceSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp);

void OnGameServiceClientSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GameNodeRpcClientResponse>& replied, Timestamp timestamp);

void OnGameServiceDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnGameServiceGateConnectGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnGameServiceControllerSend2PlayerViaGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnGameServiceCallPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp);

void OnGameServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnGameServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

