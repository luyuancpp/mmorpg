#include "common_proto/gate_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnPlayerEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GateNodePlayerEnterGsResponese>& replied, Timestamp timestamp);

void OnRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

