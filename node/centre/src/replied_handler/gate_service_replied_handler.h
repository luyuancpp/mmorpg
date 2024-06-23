#include "common_proto/gate_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnGateServiceRegisterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGateServiceUnRegisterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGateServicePlayerEnterGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RegisterSessionGameNodeResponse>& replied, Timestamp timestamp);

void OnGateServicePlayerMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGateServiceKickConnByCentreRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

void OnGateServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnGateServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

