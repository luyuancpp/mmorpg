#include "common_proto/game_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp);

void OnClientSend2PlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GameNodeRpcClientResponse>& replied, Timestamp timestamp);

void OnCallPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp);

void OnRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

