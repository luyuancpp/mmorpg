#include "common_proto/controller_service.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnStartGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlStartGsResponse>& replied, Timestamp timestamp);

void OnStartLsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartLsResponse>& replied, Timestamp timestamp);

void OnLsLoginAccountRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlLoginAccountResponse>& replied, Timestamp timestamp);

void OnLsEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<CtrlEnterGameResponese>& replied, Timestamp timestamp);

void OnGsPlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeServiceMessageResponse>& replied, Timestamp timestamp);

void OnRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RouteMsgStringResponse>& replied, Timestamp timestamp);

void OnRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RoutePlayerMsgStringResponse>& replied, Timestamp timestamp);

