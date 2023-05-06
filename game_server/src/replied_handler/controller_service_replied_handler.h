#include "common_proto/controller_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnControllerServiceStartGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlStartGsResponse>& replied, Timestamp timestamp);

void OnControllerServiceStartLsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartLsResponse>& replied, Timestamp timestamp);

void OnControllerServiceLsLoginAccountRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlLoginAccountResponse>& replied, Timestamp timestamp);

void OnControllerServiceLsEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlEnterGameResponese>& replied, Timestamp timestamp);

void OnControllerServiceGsPlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp);

void OnControllerServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnControllerServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

